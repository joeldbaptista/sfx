CC        = cc
PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man
CFLAGS    = -std=c99 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-format-truncation -O2

# macOS hides BSD extensions such as SIGWINCH under a strict _XOPEN_SOURCE.
ifeq ($(shell uname -s),Darwin)
CFLAGS += -D_DARWIN_C_SOURCE
endif

# Opt-in: compile with GNU readline for tab-completion and history in ":"
# Usage: make USE_READLINE=1
ifdef USE_READLINE
CFLAGS += -DUSE_READLINE
LDFLAGS += -lreadline
endif

sfx: sfx.c config.h
	$(CC) $(CFLAGS) -o sfx sfx.c $(LDFLAGS)

# Generated once from the tracked template; never clobbered afterwards,
# so local edits survive. Not removed by `clean`.
config.h:
	cp config.def.h $@

install: sfx
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 sfx $(DESTDIR)$(PREFIX)/bin/sfx
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	install -m 644 sfx.1 $(DESTDIR)$(MANPREFIX)/man1/sfx.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/sfx
	rm -f $(DESTDIR)$(MANPREFIX)/man1/sfx.1

frmt:
	clang-format -i *.c *.h

clean:
	rm -f sfx

.PHONY: install uninstall frmt clean
