# minifortune [![GitHub release](https://img.shields.io/github/release/vsergeev/minifortune.svg?maxAge=7200)](https://github.com/vsergeev/minifortune) [![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/vsergeev/minifortune/blob/master/LICENSE)

minifortune is a minimal fortune-mod clone.

The official fortune-mod (version 1.99.1) depends on the recode library, which
in turn has its own dependencies. This basic clone requires only the C library.

The fortunes files and their corresponding .dat files must be provided.
minifortune does not generate .dat files, but strfile can be used for this.

## Where do I get fortune files?

https://github.com/bmc/fortunes/

ftp://ftp.ibiblio.org/pub/linux/games/amusements/fortune/

http://www.linusakesson.net/cookies/

http://bradthemad.org/tech/notes/fortune_makefile.php

## Installation

AUR package: https://aur.archlinux.org/packages/minifortune

Use `make`, `make install`, `make uninstall` to build, install, or uninstall
minifortune.

## Usage

```
$ minifortune --help
Usage: minifortune [path to fortune file or directory]

Version 2.4 - https://github.com/vsergeev/minifortune

If no fortune file or directory is specified, minifortune defaults to:

    FORTUNE_PATH         environment variable containing one or more
                         colon-separated files or directories

    /usr/share/fortune   directory

$
```

### Examples

Display a random fortune from the default `/usr/share/fortune` directory:

```
$ minifortune
```

Display a random fortune from paths in the `FORTUNE_PATH` environment variable:

```
$ export FORTUNE_PATH=/usr/share/fortune/off:/usr/share/fortune/foo:/usr/share/fortune
$ minifortune
```

Display a random fortune from a specific directory:

```
$ minifortune /usr/share/fortune/off/
```

Display a random fortune from a specific fortune file:

```
$ minifortune /usr/share/fortune/linux
```

## How Fortune Files Work

Fortune files, e.g. `/usr/share/fortune/wisdom`, are plaintext files containing
fortunes separated by a delimiter, usually `"%\n"`. To allow for fast random
access to a fortune, fortune files are usually paired with a corresponding
binary .dat file, e.g. `/usr/share/fortune/wisdom.dat`, which stores metadata
about the fortunes, including the number of fortunes and the delimiter used in
the fortune file, as well as the file offsets to every fortune in the fortune
file.

The packed .dat file header looks like this:

``` c
/* Version 2 */
struct fortune_dat_header {
    uint32_t str_version;
    uint32_t str_numstr;
    uint32_t str_longlen;
    uint32_t str_shortlen;
    uint32_t str_flags;
    uint8_t str_delim;
    uint8_t padding[3];
} __attribute__ ((packed));
```

The header fields are stored in big endian byte order in the .dat file.  The
header is followed by `str_numstr` number of 32-bit unsigned integers, which
are the file offsets to every fortune in the fortune file.

`str_version` is 1 or 2. `str_flags` contains three feature bits: bit 0
indicates the fortune offsets are randomized, bit 1 indicates the fortune
offsets are ordered, and bit 2 indicates the fortune text is rot13'd in the
fortune file (typically used for offensive fortunes). `str_longlen` is the
longest fortune length, `str_shortlen` is the shortest fortune length, and
`str_delim` is the delimiting character used to separate fortunes (e.g. `%`).

## License

minifortune is MIT licensed. See the included [LICENSE](LICENSE) file.

## Alternatives

A more sophisticated Python-based clone of fortune-mod: http://software.clapper.org/fortune/

