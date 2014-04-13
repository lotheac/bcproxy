#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "bc_parser.h"

void on_open(struct bc_parser *parser) {
    printf("<%d>", parser->code);
    fflush(stdout);
}

void on_close(struct bc_parser *parser) {
    printf("</%d>", parser->code);
    fflush(stdout);
}

void on_arg_end(struct bc_parser *parser) {
    parser = parser;
    write(STDOUT_FILENO, "|", 1);
}

void on_text(struct bc_parser *parser, const char *loc, size_t len) {
    parser = parser;
    write(STDOUT_FILENO, loc, len);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        return 1;
    }
    char *buf;
    ssize_t n;
    struct bc_parser parser;
    size_t bufsz;

    sscanf(argv[1], "%zu", &bufsz);
    buf = malloc(bufsz);

    bc_parser_init(&parser);
    parser.on_open = on_open;
    parser.on_close = on_close;
    parser.on_text = on_text;
    parser.on_arg_end = on_arg_end;
    while ((n = read(STDIN_FILENO, buf, bufsz)) > 0) {
        bc_parse(&parser, buf, n);
    }
}
