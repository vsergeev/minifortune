/*
 * minifortune - Vanya Sergeev <vsergeev at gmail.com>
 *
 * A minimal fortune-mod clone, dependent on libc only, that provides random
 * fortune look up from directories containing fortune files with their
 * corresponding .dat files, or fortune look up directly from fortune files.
 *
 * Usage: minifortune <path to fortune file or folder>
 *
 * LICENSE: MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <arpa/inet.h>

/* Random device for random seeding
 * Undefine me to use time(NULL)+getpid() as a random seed
 * (more platform independent) */
#define RANDOM_DEVICE_PATH  "/dev/urandom"

#define PATH_MAX            4096

/* .dat file header from STRFILE(1) man page */
struct fortune_dat_header {
    uint32_t str_version;
    uint32_t str_numstr;
    uint32_t str_longlen;
    uint32_t str_shortlen;
    uint32_t str_flags;
    uint8_t str_delim;
    uint8_t padding[3];
} __attribute__ ((aligned (8), packed));

#if 0
void dat_header_dump(struct fortune_dat_header *dat_header) {
    printf("str_version: %d\n", dat_header->str_version);
    printf("str_numstr: %d\n", dat_header->str_numstr);
    printf("str_longlen: %d\n", dat_header->str_longlen);
    printf("str_shortlen: %d\n", dat_header->str_shortlen);
    printf("str_flags: %d\n", dat_header->str_flags);
    printf("str_delim: %c\n", dat_header->str_delim);
}
#endif

/******************************************************************************/
/* Random Seed Initialization */
/******************************************************************************/

void random_seed_init(void) {
    /* Set our random number generator seed */
    #ifdef RANDOM_DEVICE_PATH
    {
        int fd;
        uint32_t seed;
        if ((fd = open(RANDOM_DEVICE_PATH, O_RDONLY)) < 0) {
            fprintf(stderr, "Error opening %s for random seed. open: %s\n", RANDOM_DEVICE_PATH, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (read(fd, &seed, 4) < 4) {
            fprintf(stderr, "Error reading from %s for random seed. read: %s\n", RANDOM_DEVICE_PATH, strerror(errno));
            exit(EXIT_FAILURE);
        }
        close(fd);
        srand(seed);
    }
    #else
    /* time + getpid seeding borrowed from original fortune-mod */
    srand(time(NULL) + getpid());
    #endif
}

/******************************************************************************/
/* Utility functions to look up a random fortune file in a directory */
/******************************************************************************/

int isdir(const char *path) {
    struct stat st_buf;

    if (stat(path, &st_buf) < 0)
        return -1;

    if (S_ISDIR(st_buf.st_mode))
        return 1;

    return 0;
}

/* Boolean filter for .dat extension for use with scandir() */
int filter_extension_dat(const struct dirent *d) {
    int len = strlen(d->d_name);

    /* File name length must be at least: "*.dat" */
    if (len < 5)
        return 0;
    /* File name must end with ".dat" */
    return (strncmp(d->d_name + (len - 4), ".dat", 4) == 0);
}

int random_datfile(char *dat_path, int maxlen, const char *dir_path) {
    struct dirent **fnamelist;
    int n, i;

    /* Form a list of .dat fortune files */
    n = scandir(dir_path, &fnamelist, filter_extension_dat, alphasort);
    if (n < 0) {
        fprintf(stderr, "Error reading fortune directory %s. scandir: %s\n", dir_path, strerror(errno));
        return -1;
    }

    /* Pick a random .dat file */
    i = rand() % n;

    /* Assemble the fortune path */
    snprintf(dat_path, maxlen, "%s/%s", dir_path, fnamelist[i]->d_name);

    /* Free our filename list structure */
    for (i = 0; i < n; i++)
        free(fnamelist[i]);
    free(fnamelist);

    return 0;
}

/******************************************************************************/
/* Read a random fortune position and delimiter from a dat file */
/******************************************************************************/

int read_fortune_pos(uint32_t *pos, uint8_t *delim, const char *dat_path) {
    FILE *fp;
    struct fortune_dat_header dat_header;
    uint32_t fortune_id, fortune_pos;

    /* Open the dat file */
    if ( (fp = fopen(dat_path, "r")) == NULL) {
        fprintf(stderr, "Error opening fortune dat file %s. fopen: %s\n", dat_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Read the dat file header */
    if (fread(&dat_header, sizeof(struct fortune_dat_header), 1, fp) < 1) {
        fprintf(stderr, "Error reading fortune dat header from %s. fread: %s\n", dat_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Convert fields from network byte order to host byte order */
    dat_header.str_version = ntohl(dat_header.str_version);
    dat_header.str_numstr = ntohl(dat_header.str_numstr);
    dat_header.str_longlen = ntohl(dat_header.str_longlen);
    dat_header.str_shortlen = ntohl(dat_header.str_shortlen);
    dat_header.str_flags = ntohl(dat_header.str_flags);

    /* Check the .dat file header */
    if (dat_header.str_version != 1 && dat_header.str_version != 2) {
        fprintf(stderr, "Error, unsupported .dat header version %d in %s.\n", dat_header.str_version, dat_path);
        goto cleanup_failure;
    }

    /* Pick a random fortune */
    fortune_id = rand() % dat_header.str_numstr;

    /* Seek to the fortune position in the dat file table */
    if (fseek(fp, fortune_id*4, SEEK_CUR) < 0) {
        fprintf(stderr, "Error seeking to fortune id %d in %s. fseek: %s\n", fortune_id, dat_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Read the fortune position */
    if (fread(&fortune_pos, 4, 1, fp) < 1) {
        fprintf(stderr, "Error reading fortune id %d in %s. fread: %s\n", fortune_id, dat_path, strerror(errno));
        goto cleanup_failure;
    }
    fortune_pos = ntohl(fortune_pos);

    *pos = fortune_pos;
    *delim = dat_header.str_delim;

    fclose(fp);
    return 0;

    cleanup_failure:
    if (fp != NULL) fclose(fp);
    return -1;
}

/******************************************************************************/
/* Read a fortune from a fortune file at position pos and delimiter delim */
/******************************************************************************/

int read_fortune(char **fortune, const char *fortune_path, uint32_t pos, uint8_t delim) {
    FILE *fp;
    int len, c;
    char search[2] = {0};

    /* Open the fortune file */
    if ( (fp = fopen(fortune_path, "r")) == NULL) {
        fprintf(stderr, "Error opening fortune file %s. fopen: %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Seek to the fortune position */
    if (fseek(fp, pos, SEEK_SET) < 0) {
        fprintf(stderr, "Error seeking in fortune file %s. fseek: %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Count the fortune length up to the delimiter */
    for (len = 0; ; len++) {
        if ( (c = fgetc(fp)) == EOF) {
            if (feof(fp))
                fprintf(stderr, "Error reading fortune from %s. Delimiter not found.\n", fortune_path);
            else if (ferror(fp))
                fprintf(stderr, "Error reading fortune from %s. fgetc: %s\n", fortune_path, strerror(errno));
            goto cleanup_failure;
        }
        search[0] = search[1];
        search[1] = (char)c;
        if (search[0] == delim && search[1] == '\n') {
            len--;
            break;
        }
    }

    /* Allocate memory for the fortune */
    if ((*fortune = malloc(len+1)) == NULL) {
        perror("Error allocating memory for fortune. malloc");
        goto cleanup_failure;
    }

    /* Seek back to the fortune position */
    if (fseek(fp, pos, SEEK_SET) < 0) {
        fprintf(stderr, "Error seeking in fortune file %s. fseek: %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Read the fortune */
    if (fread(*fortune, 1, len, fp) < len) {
        fprintf(stderr, "Error reading fortune from %s. fread: %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Null terminate the fortune */
    (*fortune)[len] = '\0';

    fclose(fp);
    return 0;

    cleanup_failure:
    if (fp != NULL) fclose(fp);
    return -1;
}


int main(int argc, char *argv[]) {
    char dat_path[PATH_MAX];
    uint32_t pos; uint8_t delim;
    char *fortune = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path to fortune file or folder>\n", argv[0]);
        fprintf(stderr, "minifortune version 1.2\n");
        exit(EXIT_FAILURE);
    }

    /* Initialize our random seed */
    random_seed_init();

    /* If the supplied path is a folder */
    if (isdir(argv[1])) {
        /* Look up a random dat file in the fortunes directory */
        if (random_datfile(dat_path, sizeof(dat_path), argv[1]) < 0) {
            fprintf(stderr, "Error finding a fortune file.\n");
            goto cleanup_failure;
        }
    /* If the supplied path is a file */
    } else {
        /* Assemble the dat file path */
        snprintf(dat_path, sizeof(dat_path), "%s.dat", argv[1]);
    }

    /* Read a random fortune position and length */
    if (read_fortune_pos(&pos, &delim, dat_path) < 0)
        goto cleanup_failure;

    /* Chop off the .dat suffix of the fortune dat path */
    dat_path[strlen(dat_path)-4] = '\0';

    /* Read the actual fortune */
    if (read_fortune(&fortune, dat_path, pos, delim) < 0)
        goto cleanup_failure;

    /* Print the fortune */
    fputs(fortune, stdout);

    free(fortune);
    return 0;

    cleanup_failure:
    if (fortune != NULL) free(fortune);
    return -1;
}

