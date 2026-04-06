CC     = cc
CFLAGS = -std=c99 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-format-truncation -O2

sfx: sfx.c config.h
	$(CC) $(CFLAGS) -o sfx sfx.c

install: sfx
	cp sfx /usr/local/bin/sfx

frmt:
	clang-format -i *.c *.h

clean:
	rm -f sfx
