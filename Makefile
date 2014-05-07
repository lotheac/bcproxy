CC = gcc
CFLAGS = -std=c99 -Werror -Wall -Wextra -pedantic -O2 -g
CPPFLAGS = -D_XOPEN_SOURCE=600

ifeq ($(shell uname),SunOS)
    CPPFLAGS += -D__EXTENSIONS__
    LDFLAGS += -lsocket -lnsl
endif

all: bcproxy test_parser

.PHONY: clean

tags: *.c
	ctags -R --c++-kinds=+pl --c-kinds=+cdefmgpstux --fields=+iaS --extra=+q
cscope.out: *.c
	cscope -bR

test_parser: test_parser.c parser.c proxy.c buffer.c

bcproxy: bcproxy.c parser.c proxy.c buffer.c

clean:
	rm -f test_parser bcproxy
