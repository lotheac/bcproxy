#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "proxy.h"
#include "buffer.h"

int
main(int argc, char **argv)
{
	if (argc != 2) {
		return 1;
	}
	char *buf;
	ssize_t n;
	struct bc_parser parser = { 0 };
	struct proxy_state *st = proxy_state_new(4096);
	size_t bufsz;

	sscanf(argv[1], "%zu", &bufsz);
	buf = malloc(bufsz);

	parser.data = st;

	parser.on_open = on_open;
	parser.on_close = on_close;
	parser.on_text = on_text;
	parser.on_tag_text = on_tag_text;
	parser.on_arg_end = on_arg_end;
	parser.on_prompt = on_prompt;
	while ((n = read(STDIN_FILENO, buf, bufsz)) > 0) {
		bc_parse(&parser, buf, n);
		write(STDOUT_FILENO, st->obuf->data, st->obuf->len);
		buffer_clear(st->obuf);
	}
	free(buf);
	proxy_state_free(st);
}
