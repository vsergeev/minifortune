/*
 * minifortune v2.4 - https://github.com/vsergeev/minifortune
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

/* Default fortune directory */
#define DEF_FORTUNE_DIR     "/usr/share/fortune"

/* Fortune path environment variable */
#define ENV_FORTUNE_PATH    "FORTUNE_PATH"

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

#define DAT_STR_FLAG_RANDOM    0x1      /* randomized pointers */
#define DAT_STR_FLAG_ORDERED   0x2      /* ordered pointers */
#define DAT_STR_FLAG_ROTATED   0x4      /* rot-13'd text */

void dat_header_dump(const struct fortune_dat_header *dat_header) {
    printf("str_version: %d\n", dat_header->str_version);
    printf("str_numstr: %d\n", dat_header->str_numstr);
    printf("str_longlen: %d\n", dat_header->str_longlen);
    printf("str_shortlen: %d\n", dat_header->str_shortlen);
    printf("str_flags: %d\n", dat_header->str_flags);
    printf("str_delim: %c\n", dat_header->str_delim);
}

/******************************************************************************/
/* Utility functions to choose a random path, dat file, and fortune position. */
/******************************************************************************/

/* Choose a random path from the colon-separated list of paths.
 * e.g. "/foo/bar:/usr/share/foo:/usr/local/bar" -> "/usr/local/bar" */
int choose_random_path(char *path, size_t maxlen, const char *paths) {
    /* By POSIX rules, ":abc:d\:ef:" is 4 tokens: "", "abc", "d\:ef", ""
     * and empty tokens mean current directory. */

    /* Count number of tokens */
    unsigned int token_count = 1;
    for (int i = 0; i < strlen(paths); i++)
        token_count += ((paths[i] == ':' && i == 0) || (paths[i] == ':' && paths[i-1] != '\\'));

    /* Pick a random token from [1, token_count] */
    unsigned int token_choice = (rand() % token_count) + 1;

    /* Find the token offset and length */
    token_count = 1;
    unsigned int token_offset = 0, token_length = 0;
    for (int i = 0; i < strlen(paths); i++) {
        if ((paths[i] == ':' && i == 0) || (paths[i] == ':' && paths[i-1] != '\\')) {
            token_count++;

            if (token_count == token_choice)
                token_offset = i+1;
            else if (token_count > token_choice)
                break;
        } else {
            if (token_count == token_choice)
                token_length++;
        }
    }

    /* Copy token to path */
    if (token_length == 0) {
        strncpy(path, ".", maxlen-1);
        path[maxlen-1] = '\0';
    } else {
        unsigned int length = (token_length < (maxlen-1)) ? token_length : (maxlen-1);
        strncpy(path, paths+token_offset, length);
        path[length] = '\0';
    }

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

    /* Form a list of .dat files in directory dir_path */
    n = scandir(dir_path, &fnamelist, filter_extension_dat, alphasort);
    if (n < 0) {
        fprintf(stderr, "Error reading fortune directory '%s': scandir(): %s\n", dir_path, strerror(errno));
        return -1;
    }

    /* No .dat files found */
    if (n == 0)
        return -1;

    if (n > 0) {
        /* Pick a random .dat file */
        unsigned int i = rand() % n;
        /* Assemble the .dat file path */
        snprintf(dat_path, maxlen, "%s/%s", dir_path, fnamelist[i]->d_name);
    }

    /* Free our filename list structure */
    for (int i = 0; i < n; i++)
        free(fnamelist[i]);
    free(fnamelist);

    return 0;
}

/* Choose a random fortune position from a .dat file dat_path. */
int choose_random_fortune_pos(uint32_t *pos, uint8_t *delim, bool *rot13, const char *dat_path) {
    FILE *fp;
    struct fortune_dat_header dat_header;
    uint32_t fortune_id, fortune_pos;

    /* Open the .dat file */
    if ((fp = fopen(dat_path, "r")) == NULL) {
        fprintf(stderr, "Error opening fortune dat file '%s': fopen(): %s\n", dat_path, strerror(errno));
        goto cleanup_failure;
    }

    /* Read the .dat file header */
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

    /* Seek to the fortune position in the .dat file positions table */
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
    *rot13 = ((dat_header.str_flags & DAT_STR_FLAG_ROTATED) != 0);

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
int read_fortune(char **fortune, const char *fortune_path, uint32_t pos, uint8_t delim, bool rot13) {
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

    /* If the fortune is rot13'd, de-rot13 it */
    if (rot13) {
        for (unsigned int i = 0; i < len; i++) {
            if ((*fortune)[i] >= 'a' && (*fortune)[i] <= 'z')
                (*fortune)[i] = ((((*fortune)[i] - 'a') + 13) % 26) + 'a';
            else if ((*fortune)[i] >= 'A' && (*fortune)[i] <= 'Z')
                (*fortune)[i] = ((((*fortune)[i] - 'A') + 13) % 26) + 'A';
        }
    }

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

int main(int argc, char *argv[]) {
    char dat_path[PATH_MAX];

    uint32_t fortune_pos;
    uint8_t fortune_delim;
    bool fortune_rot13;
    char *fortune = NULL;

    /* Help/Usage */
    if (argc == 2 && ((strcmp("-h", argv[1]) == 0) || (strcmp("--help", argv[1]) == 0))) {
        printf("Usage: %s [path to fortune file or directory]\n"
               "Version 2.4 - https://github.com/vsergeev/minifortune\n\n"
               "If no fortune file or directory is specified, minifortune defaults to:\n\n"
               "    %s         environment variable containing one or\n"
               "                         more colon-separated files or directories\n\n"
               "    %s   directory\n\n", argv[0], ENV_FORTUNE_PATH, DEF_FORTUNE_DIR);
        exit(EXIT_SUCCESS);
    }

    /* Initialize our random seed */
    /* time() + getpid() seeding borrowed from original fortune-mod */
    srand(time(NULL) + getpid());

    if (argc == 1) {
        /* No explicit fortune directory/file specified */

        if (getenv(ENV_FORTUNE_PATH) != NULL && strlen(getenv(ENV_FORTUNE_PATH)) > 0) {
            /* Default to the fortune path environment variable first */

            char path[PATH_MAX];

            /* Look up a random path from the colon separated paths in the environment variable */
            if (choose_random_path(path, sizeof(path), getenv(ENV_FORTUNE_PATH)) < 0) {
                fprintf(stderr, "Error, no path found in paths '%s'.\n", getenv(ENV_FORTUNE_PATH));
                exit(EXIT_FAILURE);
            }

            if (isdir(path)) {
                /* Look up a random .dat file in the directory */
                if (choose_random_datfile(dat_path, sizeof(dat_path), path) < 0) {
                    fprintf(stderr, "Error, no fortune file found in directory '%s'.\n", path);
                    exit(EXIT_FAILURE);
                }
            } else {
                /* Assemble the .dat file path from the fortune file path */
                snprintf(dat_path, sizeof(dat_path), "%s.dat", path);
            }
        } else if (isdir(DEF_FORTUNE_DIR)) {
            /* Default to fortune directory second */

            /* Look up a random .dat file in the directory */
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

            /* Look up a random .dat file in the directory */
            if (choose_random_datfile(dat_path, sizeof(dat_path), argv[1]) < 0) {
                fprintf(stderr, "Error, no fortune file found in directory '%s'.\n", argv[1]);
                exit(EXIT_FAILURE);
            }
        } else {
            /* If the supplied path is a fortune file */

            /* Assemble the .dat file path from the fortune file path */
            snprintf(dat_path, sizeof(dat_path), "%s.dat", argv[1]);
        }
    }

    /* Choose a random fortune position from the .dat file */
    if (choose_random_fortune_pos(&fortune_pos, &fortune_delim, &fortune_rot13, dat_path) < 0)
        exit(EXIT_FAILURE);

    /* Chop off the .dat suffix of the .dat file path to get the fortune file path */
    dat_path[strlen(dat_path)-4] = '\0';

    /* Read the fortune */
    if (read_fortune(&fortune, dat_path, fortune_pos, fortune_delim, fortune_rot13) < 0)
        exit(EXIT_FAILURE);

    /* Print the fortune */
    fputs(fortune, stdout);

    free(fortune);

    exit(EXIT_SUCCESS);
}

