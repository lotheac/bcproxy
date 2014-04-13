CC = gcc
CFLAGS = -std=c99 -Werror -Wall -Wextra -pedantic -O2 -g
CPPFLAGS = -D_XOPEN_SOURCE=600

all: bcproxy

tags: *.c
	ctags -R --c++-kinds=+pl --c-kinds=+cdefmgpstux --fields=+iaS --extra=+q
cscope.out: *.c
	cscope -bR

test_parser: test_parser.c bc_parser.c

bcproxy: bcproxy.c
