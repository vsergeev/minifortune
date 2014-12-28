# minifortune

minifortune is a minimal fortune-mod clone, dependent on libc only.

The official fortune-mod (version 1.99.1) depends on the recode library, which in turn has its own dependencies. This basic clone only requires the C library.

minifortune currently does not generate the random-access .dat files associated with fortune files -- strfile can be used for this. The folder containing fortunes and their .dat files must be provided.

## Where do I get fortune files?

<https://github.com/bmc/fortunes/>

<ftp://ftp.ibiblio.org/pub/linux/games/amusements/fortune/>

<http://bradthemad.org/tech/notes/fortune_makefile.php>

<http://www.linusakesson.net/cookies/>

## Building

Run `make` to build minifortune.

Run `make install` to install minifortune to /usr/bin.

Run `make uninstall` to uninstall minifortune from /usr/bin.

## Usage

    $ minifortune -h
    Usage: minifortune [path to fortune file or directory]
    Version 2.2
    
    If no fortune file or directory is specified, minifortune defaults to:
    
        FORTUNE_DIR          environment variable containing one
                             or more colon-separated directories
    
        /usr/share/fortune   directory
    
    $


## Example usage:

    Display a random fortune from the /usr/share/fortune directory:
    $ minifortune

    Display a random fortune from directories in the FORTUNE_DIR environment variable.
    $ export FORTUNE_DIR=/usr/share/fortune/off:/usr/share/fortune/foo:/usr/share/fortune
    $ minifortune

    Display a random fortune from a specific directory:
    $ minifortune /usr/share/fortune/

    Display a random fortune from a specific fortune file:
    $ minifortune /usr/share/fortune/linux

The `FORTUNE_DIR` environment variable may contain several colon-separated directories.  minifortune will choose one randomly from them.

## How Fortune Files Work

Fortune files (e.g. `/usr/share/fortune/wisdom`) are plaintext ASCII files with fortunes typically delimited by a "%\n" separator. In order to allow for fast random access to a particular fortune in a fortune file, fortune files are usually paired with a corresponding binary .dat file (e.g.  `/usr/share/fortune/wisdom.dat`), which contains metadata describing the number of fortunes, longest fortune length, shortest fortune length, and delimiter used in the fortune file. The packed .dat file header looks like this:

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

The header is stored in big endian byte order in the .dat file. Following the header are str_numstr number of 32-bit integers, which contain the fortune seek positions in the fortune file.

Bit 0 of str_flags set indicates fortune seek positions are randomized in the dat file, bit 1 of str_flags set indicates the fortune seek positions are ordered in the dat file, and bit 2 of str_flags set indicates the fortune contents are rot13'd in the fortune file (typically used for offensive fortunes). str_longlen is the longest fortune length, str_shortlen is the shortest fortune length, and str_delim is the delimiting character used between fortunes.

## License

MIT Licensed. See the included LICENSE file.

## Alternatives

A more sophisticated Python-based clone of fortune-mod: http://software.clapper.org/fortune/

