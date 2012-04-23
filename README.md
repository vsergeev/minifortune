minifortune
===========

About
-----

minifortune is a minimal fortune-mod clone, dependent on libc only, that
provides random fortune look up from directories containing fortune files with
their corresponding .dat table files.

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

Building
--------

Run make to build minifortune.
Run make install to install minifortune to /usr/local/bin.

Usage
-----

	Usage: minifortune <path to fortunes> [filename]
	minifortune version 1.0

Example usage:

	$ minifortune /usr/share/fortune/

	$ minifortune /usr/share/fortune/ linux

License
-------

MIT Licensed. See the included LICENSE file.

Alternatives
------------

A more sophisticated Python-based clone of fortune-mod: http://software.clapper.org/fortune/

