minifortune
===========

About
-----

minifortune is a minimal fortune-mod clone, dependent on libc only, that
provides random fortune look up from directories containing fortune files with
their corresponding .dat files, or from a specific fortune file and its
corresponding .dat file.

The official fortune-mod (version 1.99.1) depends on the recode library, which
additionally has its own dependencies. This basic clone only requires the C
library.

minifortune currently does not generate the random-access .dat files associated
with fortune files; strfile can be used for this. The folder containing
fortunes and their .dat files must be provided.

Author: Vanya Sergeev, vsergeev at gmail.com.

Where do I get fortune files?
-----------------------------

https://github.com/bmc/fortunes/

ftp://ftp.ibiblio.org/pub/linux/games/amusements/fortune/

http://bradthemad.org/tech/notes/fortune_makefile.php

http://www.linusakesson.net/cookies/

Building
--------

Run make to build minifortune.
Run make install to install minifortune to /usr/local/bin.

Usage
-----

    Usage: minifortune <path to fortune file or folder>
    minifortune version 1.2

Example usage:

    Display a random fortune chosen from a random fortune file chosen from the
    specified fortune directory:
    $ minifortune /usr/share/fortune/

    Display a random fortune chosen from the specified fortune file:
    $ minifortune /usr/share/fortune/linux

How Fortune Files Work
----------------------

Fortune files (e.g. /usr/share/fortune/wisdom) are simply plaintext ASCII files
with paragraphs typically delimited by a "\n%\n" separator. In order to allow
for fast random access to a particular paragraph in a fortune file, fortune
files are usually paired with a corresponding binary .dat file (e.g.
/usr/share/fortune/wisdom.dat), which contains metadata describing the number
of fortunes, longest paragraph length, shortest paragraph length, and delimiter
used in the fortune file. The packed .dat file header looks like this:

    /* Version 2 */
    struct fortune_dat_header {
        uint32_t str_version;
        uint32_t str_numstr;
        uint32_t str_longlen;
        uint32_t str_shortlen;
        uint32_t str_flags;
        uint32_t str_delim;
        uint8_t str_delim;
        uint8_t padding[3];
    } __attribute__ ((packed));

The header is stored in big endian byte order in the .dat file. Following the
header are str_numstr number of 4-byte integers, which represent the fortune
seek positions in the fortune file.

Bit 0 of str_flags set indicates fortune seek positions are randomized in the
dat file, bit 1 of str_flags set indicates the fortune seek positions are
ordered in the dat file, and bit 2 of str_flags set indicates the fortune
contents are rot13'd in the fortune file.

License
-------

MIT Licensed. See the included LICENSE file.

Alternatives
------------

A more sophisticated Python-based clone of fortune-mod: http://software.clapper.org/fortune/

