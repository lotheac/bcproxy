CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2 -g
CPPFLAGS = -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -I/opt/pgsql/include
LDFLAGS = -lsocket -lnsl -lpq -L/opt/pgsql/lib -R/opt/pgsql/lib

all: bcproxy test_parser

.PHONY: clean

tags: *.c
	ctags -R --c++-kinds=+pl --c-kinds=+cdefmgpstux --fields=+iaS --extra=+q
cscope.out: *.c
	cscope -bR

test_parser: test_parser.c parser.c proxy.c buffer.c room.c color.c db.c

bcproxy: bcproxy.c parser.c proxy.c buffer.c room.c color.c db.c

clean:
	rm -f test_parser bcproxy
