CC = gcc
STRIP = strip
CFLAGS = -Wall -O3 -D_GNU_SOURCE
LDFLAGS =
OBJECTS = minifortune.o
PROGNAME = minifortune
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

all: $(PROGNAME)

install: $(PROGNAME)
	$(STRIP) $(PROGNAME)
	install -D -s -m 0755 $(PROGNAME) $(DESTDIR)$(BINDIR)/$(PROGNAME)

$(PROGNAME): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

clean:
	rm -rf $(PROGNAME) $(OBJECTS)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROGNAME)

