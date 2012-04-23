CC = gcc
STRIP = strip
CFLAGS = -Wall -O3 -D_GNU_SOURCE
LDFLAGS =
PROGNAME = minifortune
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

all: $(PROGNAME)

install: $(PROGNAME)
	install -D -s -m 0755 $(PROGNAME) $(DESTDIR)$(BINDIR)/$(PROGNAME)

$(PROGNAME): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(PROGNAME).c -o $@
	$(STRIP) $@

clean:
	rm -rf $(PROGNAME)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROGNAME)

