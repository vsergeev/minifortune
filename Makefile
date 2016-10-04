PROGNAME = minifortune

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin

CFLAGS += -Wall -D_GNU_SOURCE -std=c99
LDFLAGS +=

.PHONY: all
all: $(PROGNAME)

.PHONY: clean
clean:
	rm -f $(PROGNAME)

.PHONY: install
install: $(PROGNAME)
	install -d $(DESTDIR)$(BINDIR)
	install -s -m755 $(PROGNAME) $(DESTDIR)$(BINDIR)/$(PROGNAME)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROGNAME)

$(PROGNAME): $(PROGNAME).c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@
