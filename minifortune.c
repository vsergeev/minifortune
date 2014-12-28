/*
 * minifortune - https://github.com/vsergeev/minifortune
 * A minimal fortune-mod clone, dependent on libc only.
 * LICENSE: MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <arpa/inet.h>

/* Random device to use for random seeding
 * Undefine me to use time(NULL)+getpid() as a random seed (more platform independent) */
#define RANDOM_DEVICE_PATH  "/dev/urandom"

/* Default fortune directory */
#define DEF_FORTUNE_DIR     "/usr/share/fortune"

/* Fortune directory environment variable */
#define ENV_FORTUNE_DIR     "FORTUNE_DIR"

/* Maximum path length */
#define PATH_MAX            4096

/******************************************************************************/
/* Fortune DAT Header Structure */
/******************************************************************************/

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

void dat_header_dump(const struct fortune_dat_header *dat_header) {
    printf("str_version: %d\n", dat_header->str_version);
    printf("str_numstr: %d\n", dat_header->str_numstr);
    printf("str_longlen: %d\n", dat_header->str_longlen);
    printf("str_shortlen: %d\n", dat_header->str_shortlen);
    printf("str_flags: %d\n", dat_header->str_flags);
    printf("str_delim: %c\n", dat_header->str_delim);
}

/******************************************************************************/
/* Utility functions to choose a random directory, dat file, fortune position. */
/******************************************************************************/

/* Choose random directory from colon-separated list directories. */
int choose_random_directory(char *dir_path, size_t maxlen, const char *directories) {
    char *directories_tokenized;
    unsigned int i, token_count, token_index;

    if (strlen(directories) == 0)
        return -1;

    /* Make a local copy of the directory list to tokenize */
    directories_tokenized = strdup(directories);

    /* By POSIX rules, ":abc:d\:ef:" is 4 tokens: "", "abc", "d\:ef", ""
     * and empty tokens mean current directory. */

    /* Tokenize at the path separator, but interpret escape sequence \: as a colon belonging to a directory path */
    for (i = 0, token_count = 1; i < strlen(directories_tokenized); i++) {
        if ((directories_tokenized[i] == ':' && i == 0) ||
              (directories_tokenized[i] == ':' && directories_tokenized[i-1] != '\\')) {
            directories_tokenized[i] = '\0';
            token_count++;
        }
    }

    /* Pick a random token */
    token_index = rand() % token_count;

    /* Find the random token */
    for (i = 0, token_count = 0; i < strlen(directories_tokenized); i++) {
        if (token_index == token_count)
            break;
        if (directories_tokenized[i] == '\0')
            token_count++;
    }

    /* Empty token means current directory */
    if (strlen(directories_tokenized+i) == 0)
        strncpy(dir_path, ".", maxlen);
    else
        strncpy(dir_path, directories_tokenized+i, maxlen);

    free(directories_tokenized);

    return 0;
}

/* Boolean filter for .dat extension for use with scandir() */
int filter_extension_dat(const struct dirent *d) {
    /* File name length must be at least: "*.dat" */
    /* File name must end with ".dat" */
    size_t len = strlen(d->d_name);
    return (len >= 5) && (strncmp(d->d_name + (len - 4), ".dat", 4) == 0);
}

/* Choose random .dat file from directory dir_path. */
int choose_random_datfile(char *dat_path, int maxlen, const char *dir_path) {
    struct dirent **fnamelist;
    int n;

    /* Form a list of .dat fortune files in directory dir_path */
    n = scandir(dir_path, &fnamelist, filter_extension_dat, alphasort);
    if (n < 0) {
        fprintf(stderr, "Error reading fortune directory '%s': scandir(): %s\n", dir_path, strerror(errno));
        return -1;
    }

    if (n > 0) {
        /* Pick a random .dat file */
        unsigned int i = rand() % n;
        /* Assemble the fortune path */
        snprintf(dat_path, maxlen, "%s/%s", dir_path, fnamelist[i]->d_name);
    }

    /* Free our filename list structure */
    for (int i = 0; i < n; i++)
        free(fnamelist[i]);
    free(fnamelist);

    return 0;
}

/* Choose a random fortune position from a .dat file dat_path. */
int choose_random_fortune_pos(uint32_t *pos, uint8_t *delim, const char *dat_path) {
    FILE *fp;
    struct fortune_dat_header dat_header;
    uint32_t fortune_id, fortune_pos;

    /* Open the dat file */
    if ((fp = fopen(dat_path, "r")) == NULL) {
        fprintf(stderr, "Error opening fortune dat file '%s': fopen(): %s\n", dat_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Read the dat file header */
    if (fread(&dat_header, sizeof(struct fortune_dat_header), 1, fp) < 1) {
        fprintf(stderr, "Error reading fortune dat header from '%s': fread(): %s\n", dat_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Convert fields from network byte order to host byte order */
    dat_header.str_version = ntohl(dat_header.str_version);
    dat_header.str_numstr = ntohl(dat_header.str_numstr);
    dat_header.str_longlen = ntohl(dat_header.str_longlen);
    dat_header.str_shortlen = ntohl(dat_header.str_shortlen);
    dat_header.str_flags = ntohl(dat_header.str_flags);

    #ifdef DEBUG
    dat_header_dump(&dat_header);
    #endif

    /* Check the .dat file header */
    if (dat_header.str_version != 1 && dat_header.str_version != 2) {
        fprintf(stderr, "Error, unsupported .dat header version %d in '%s'.\n", dat_header.str_version, dat_path);
        goto cleanup_failure;
    }

    /* Pick a random fortune */
    fortune_id = rand() % dat_header.str_numstr;

    /* Seek to the fortune position in the dat file table */
    if (fseek(fp, fortune_id*4, SEEK_CUR) < 0) {
        fprintf(stderr, "Error seeking to fortune id %d in '%s': fseek(): %s\n", fortune_id, dat_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Read the fortune position */
    if (fread(&fortune_pos, 4, 1, fp) < 1) {
        fprintf(stderr, "Error reading fortune id %d in '%s': fread: %s\n", fortune_id, dat_path, strerror(errno));
        goto cleanup_failure;
    }
    fortune_pos = ntohl(fortune_pos);

    *pos = fortune_pos;
    *delim = dat_header.str_delim;

    fclose(fp);
    return 0;

    cleanup_failure:
    if (fp != NULL)
        fclose(fp);
    return -1;
}

/******************************************************************************/
/* Read fortune function. */
/******************************************************************************/

/* Read a fortune from fortune file fortune_path at position pos and delimiter delim. */
int read_fortune(char **fortune, const char *fortune_path, uint32_t pos, uint8_t delim) {
    FILE *fp;
    int len, c;
    char search[2] = {0};

    *fortune = NULL;

    /* Open the fortune file */
    if ((fp = fopen(fortune_path, "r")) == NULL) {
        fprintf(stderr, "Error opening fortune file '%s': fopen(): %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Seek to the fortune position */
    if (fseek(fp, pos, SEEK_SET) < 0) {
        fprintf(stderr, "Error seeking in fortune file '%s': fseek(): %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Count the fortune length up to the delimiter */
    for (len = 0; ; len++) {
        if ((c = fgetc(fp)) == EOF) {
            if (feof(fp))
                fprintf(stderr, "Error reading fortune from '%s': Delimiter not found.\n", fortune_path);
            else if (ferror(fp))
                fprintf(stderr, "Error reading fortune from '%s': fgetc(): %s\n", fortune_path, strerror(errno));
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
        perror("Error allocating memory for fortune. malloc()");
        goto cleanup_failure;
    }

    /* Seek back to the fortune position */
    if (fseek(fp, pos, SEEK_SET) < 0) {
        fprintf(stderr, "Error seeking in fortune file '%s': fseek(): %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Read the fortune */
    if (fread(*fortune, 1, len, fp) < len) {
        fprintf(stderr, "Error reading fortune from '%s': fread(): %s\n", fortune_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Null terminate the fortune */
    (*fortune)[len] = '\0';

    fclose(fp);
    return 0;

    cleanup_failure:
    if (*fortune != NULL)
        free(*fortune);
    if (fp != NULL)
        fclose(fp);
    return -1;
}

bool isdir(const char *path) {
    struct stat st_buf;

    if (stat(path, &st_buf) < 0)
        return false;

    if (S_ISDIR(st_buf.st_mode))
        return true;

    return false;
}

int main(int argc, char *argv[], char *envp[]) {
    char dir_path[PATH_MAX];
    char dat_path[PATH_MAX];

    uint32_t fortune_pos;
    uint8_t fortune_delim;
    char *fortune = NULL;

    /* Help/Usage */
    if (argc == 2 && ((strcmp("-h", argv[1]) == 0) || (strcmp("--help", argv[1]) == 0))) {
        printf("Usage: %s [path to fortune file or directory]\n", argv[0]);
        printf("Version 2.2 - https://github.com/vsergeev/minifortune\n\n\
If no fortune file or directory is specified, minifortune defaults to:\n\n\
    %s          environment variable containing one\n\
                         or more colon-separated directories\n\n\
    %s   directory\n\n", ENV_FORTUNE_DIR, DEF_FORTUNE_DIR);
        exit(EXIT_SUCCESS);
    }

    /* Initialize our random seed */
    #ifdef RANDOM_DEVICE_PATH
    /* Read from random device path */
    {
        int fd;
        uint32_t seed;

        if ((fd = open(RANDOM_DEVICE_PATH, O_RDONLY)) < 0) {
            fprintf(stderr, "Error opening random device '%s'. open(): %s\n", RANDOM_DEVICE_PATH, strerror(errno));
            exit(EXIT_FAILURE);
        }

        if (read(fd, &seed, 4) < 4) {
            fprintf(stderr, "Error reading random device '%s'. read(): %s\n", RANDOM_DEVICE_PATH, strerror(errno));
            exit(EXIT_FAILURE);
        }

        close(fd);
        srand(seed);
    }
    #else
    /* time + getpid seeding borrowed from original fortune-mod */
    srand(time(NULL) + getpid());
    #endif

    if (argc == 1) {
        /* No explicit fortune directory/file specified */

        if (getenv(ENV_FORTUNE_DIR) != NULL && strlen(getenv(ENV_FORTUNE_DIR)) > 0) {
            /* Default to the fortune directory environment variable first */

            /* Look up a random directory from the colon separated path in the environment variable */
            if (choose_random_directory(dir_path, sizeof(dir_path), getenv(ENV_FORTUNE_DIR)) < 0) {
                fprintf(stderr, "Error, no directory found in list '%s'.\n", getenv(ENV_FORTUNE_DIR));
                exit(EXIT_FAILURE);
            }
            /* Look up a random dat file in the directory */
            if (choose_random_datfile(dat_path, sizeof(dat_path), dir_path) < 0) {
                fprintf(stderr, "Error, no fortune file found in directory '%s'.\n", dir_path);
                exit(EXIT_FAILURE);
            }

        } else if (isdir(DEF_FORTUNE_DIR)) {
            /* Default to /usr/share/fortune directory second */

            /* Look up a random dat file in the directory */
            if (choose_random_datfile(dat_path, sizeof(dat_path), DEF_FORTUNE_DIR) < 0) {
                fprintf(stderr, "Error, no fortune file found in directory '%s'.\n", DEF_FORTUNE_DIR);
                exit(EXIT_FAILURE);
            }
        } else {
            /* Give up */
            printf("A wise man once said:\n\tPopulate %s with fortunes,\n\tor run 'minifortune -h' for more options.\n", DEF_FORTUNE_DIR);
            exit(EXIT_SUCCESS);
        }
    } else if (argc == 2) {
        /* Explicit fortune directory/file specified */

        if (isdir(argv[1])) {
            /* If the supplied path is a folder */

            /* Look up a random dat file in the directory */
            if (choose_random_datfile(dat_path, sizeof(dat_path), argv[1]) < 0) {
                fprintf(stderr, "Error, no fortune file found in directory '%s'.\n", argv[1]);
                exit(EXIT_FAILURE);
            }
        } else {
            /* If the supplied path is a fortune */

            /* Assemble the DAT path from the fortune path */
            snprintf(dat_path, sizeof(dat_path), "%s.dat", argv[1]);
        }
    }

    /* Choose a random fortune position from the DAT file */
    if (choose_random_fortune_pos(&fortune_pos, &fortune_delim, dat_path) < 0)
        exit(EXIT_FAILURE);

    /* Chop off the .dat suffix of the DAT path to get the fortune path */
    dat_path[strlen(dat_path)-4] = '\0';

    /* Read the fortune */
    if (read_fortune(&fortune, dat_path, fortune_pos, fortune_delim) < 0)
        exit(EXIT_FAILURE);

    /* Print the fortune */
    fputs(fortune, stdout);

    free(fortune);

    exit(EXIT_SUCCESS);
}

