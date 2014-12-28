PROGNAME = minifortune

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin

CFLAGS += -Wall -D_GNU_SOURCE -std=c99
LDFLAGS +=

.PHONY: all
all: $(PROGNAME)

.PHONY: clean
clean:
	rm -rf $(PROGNAME)

.PHONY: install
install: $(PROGNAME)
	install -D -s -m 0755 $(PROGNAME) $(DESTDIR)$(BINDIR)/$(PROGNAME)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROGNAME)

$(PROGNAME): $(PROGNAME).c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

