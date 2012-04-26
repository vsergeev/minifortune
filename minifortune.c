/*
 * minifortune - Vanya Sergeev <vsergeev at gmail.com>
 *
 * A minimal fortune-mod clone, dependent on libc only, that provides random
 * fortune look up from directories containing fortune files with their
 * corresponding .dat files.
 *
 * Usage: minifortune <path to fortunes> [filename]
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
#include <arpa/inet.h>

/* Random device for random seeding */
#define RANDOM_DEVICE_PATH  "/dev/urandom"
/* Undefine RANDOM_DEVICE_PATH to use time(NULL)+getpid() as a random seed
 * (more platform independent) */

/* .dat file header from STRFILE(1) man page */
struct fortune_dat_header {
	uint32_t str_version;
	uint32_t str_numstr;
	uint32_t str_longlen;
	uint32_t str_shortlen;
	uint32_t str_flags;
	uint32_t str_delim;
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

int dat_file_filter(const struct dirent *d) {
	int len = strlen(d->d_name);

	/* File name length must be at least: "*.dat" */
	if (len < 5)
		return 0;
	/* File name must end with ".dat" */
	return (strncmp(d->d_name + (len - 4), ".dat", 4) == 0);
}

int random_fortune_filepath(char *dir_path, char **fortune_dat_path) {
	struct dirent **fnamelist;
	int n, i, ret;

	ret = 0;

	/* Form a list of .dat fortune files */
	n = scandir(dir_path, &fnamelist, dat_file_filter, alphasort);
	if (n < 0) {
		fprintf(stderr, "Error reading fortune directory %s! scandir: %s\n", dir_path, strerror(errno));
		return -1;
	} else {
		/* Pick a random .dat file */
		i = rand() % n;

		/* Allocate memory for the fortune path */
		*fortune_dat_path = malloc(strlen(dir_path) + strlen(fnamelist[i]->d_name) + 2);
		if (*fortune_dat_path == NULL) {
			perror("Error allocating memory for fortune path! malloc");
			ret = -1;
		} else {
			/* Assemble the fortune path */
			snprintf(*fortune_dat_path, strlen(dir_path) + strlen(fnamelist[i]->d_name) + 2, "%s/%s", dir_path, fnamelist[i]->d_name);
			ret = 1;
		}

		/* Free our filename list structure */
		for (i = 0; i < n; i++)
			free(fnamelist[i]);
		free(fnamelist);

	}
	return ret;
}

int main(int argc, char *argv[]) {
	char *fortune_dat_path;
	FILE *fp;
	struct fortune_dat_header dat_header;
	uint32_t fortune_id, fortune_pos, fortune_pos_next, fortune_len;
	char *fortune, *sep;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <path to fortunes> [filename]\n", argv[0]);
		fprintf(stderr, "minifortune version 1.0\n");
		exit(EXIT_FAILURE);
	}

	/* Set our random number generator seed */
	#ifdef RANDOM_DEVICE_PATH
	{
		int fd;
		uint32_t seed;
		if ((fd = open(RANDOM_DEVICE_PATH, O_RDONLY)) < 0) {
			fprintf(stderr, "Error opening %s for random seed! open: %s\n", RANDOM_DEVICE_PATH, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (read(fd, &seed, 4) < 4) {
			fprintf(stderr, "Error reading from %s for random seed! read: %s\n", RANDOM_DEVICE_PATH, strerror(errno));
			exit(EXIT_FAILURE);
		}
		close(fd);
		srand(seed);
	}
	#else
	/* time + getpid seeding borrowed from original fortune-mod */
	srand(time(NULL) + getpid());
	#endif

	if (argc < 3) {
		/* Look up a random fortune dat file in the fortunes directory */
		if (random_fortune_filepath(argv[1], &fortune_dat_path) <= 0) {
			fprintf(stderr, "Error finding fortune file!\n");
			exit(EXIT_FAILURE);
		}
	} else {
		/* Allocate memory for the fortune path */
		if ( (fortune_dat_path = malloc(strlen(argv[1]) + strlen(argv[1]) + 6)) == NULL) {
			perror("Error allocating memory for fortune path! malloc");
			exit(EXIT_FAILURE);
		}
		/* Assemble the fortune path */
		snprintf(fortune_dat_path, strlen(argv[1]) + strlen(argv[2]) + 6, "%s/%s.dat", argv[1], argv[2]);
	}

	/* Open the dat file */
	if ( (fp = fopen(fortune_dat_path, "r")) == NULL) {
		fprintf(stderr, "Error opening fortune dat file %s! fopen: %s\n", fortune_dat_path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Read the dat file header */
	if (fread(&dat_header, sizeof(struct fortune_dat_header), 1, fp) < 1) {
		fprintf(stderr, "Error reading fortune dat header from %s! fread: %s\n", fortune_dat_path, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	/* Convert fields from network byte order to host byte order */
	dat_header.str_version = ntohl(dat_header.str_version);
	dat_header.str_numstr = ntohl(dat_header.str_numstr);
	dat_header.str_longlen = ntohl(dat_header.str_longlen);
	dat_header.str_shortlen = ntohl(dat_header.str_shortlen);
	dat_header.str_flags = ntohl(dat_header.str_flags);

	/* Check the .dat file header */
	if (dat_header.str_version != 2) {
		fprintf(stderr, "Error, unsupported .dat header version %d in %s!\n", dat_header.str_version, fortune_dat_path);
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	/* Pick a random fortune */
	fortune_id = rand() % dat_header.str_numstr;

	/* Seek to the pointer in the dat file table */
	if (fseek(fp, fortune_id*4, SEEK_CUR) < 0) {
		fprintf(stderr, "Error seeking to fortune pointer in %s! fseek: %s\n", fortune_dat_path, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	/* Read the fortune position */
	if (fread(&fortune_pos, 4, 1, fp) < 1) {
		fprintf(stderr, "Error reading fortune pointer in %s! fread: %s\n", fortune_dat_path, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	fortune_pos = ntohl(fortune_pos);

	/* Read the next fortune position (conveniently, the final entry in the
	 * dat file table is also the position after the last fortune, i.e. end
	 * of file) */
	if (fread(&fortune_pos_next, 4, 1, fp) < 1) {
		fprintf(stderr, "Error reading fortune pointer from %s! fread: %s", fortune_dat_path, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	fortune_pos_next = ntohl(fortune_pos_next);

	fclose(fp);

	fortune_len = fortune_pos_next - fortune_pos;

	/* Sanity check the fortune length (including the potential subsequent "%\n") */
	if (fortune_len > dat_header.str_longlen+2) {
		fprintf(stderr, "Invalid fortune length! Fortune dat file: %s\n", fortune_dat_path);
		exit(EXIT_FAILURE);
	}

	/* Chop off the .dat suffix of the fortune dat path */
	fortune_dat_path[strlen(fortune_dat_path)-4] = '\0';

	/* Open the fortune file */
	if ( (fp = fopen(fortune_dat_path, "r")) == NULL) {
		fprintf(stderr, "Error opening fortune file %s! fopen: %s\n", fortune_dat_path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Allocate memory for the fortune + '\0' */
	if ((fortune = malloc(fortune_len+1)) == NULL) {
		perror("Error allocating memory for fortune! malloc");
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	/* Seek to the fortune position */
	if (fseek(fp, fortune_pos, SEEK_SET) < 0) {
		fprintf(stderr, "Error seeking in fortune file %s! fseek: %s\n", fortune_dat_path, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	/* Read the fortune */
	if (fread(fortune, 1, fortune_len, fp) < fortune_len) {
		fprintf(stderr, "Error reading fortune from %s! fread: %s\n", fortune_dat_path, strerror(errno));
		fclose(fp);
		exit(EXIT_FAILURE);
	}

	/* Null terminate the fortune */
	/* Depending on how the fortune file was terminated (single % or not),
	 * the fortune may or may not end with the separator sequence "\n%\n" */
	if ( (sep = strstr(fortune, "\n%\n")) != NULL)
		*(++sep) = '\0';
	else
		fortune[fortune_len] = '\0';

	/* Print the fortune */
	fputs(fortune, stdout);

	free(fortune);
	free(fortune_dat_path);
	fclose(fp);

	return 0;
}

