#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "proxy.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        return 1;
    }
    char *buf;
    ssize_t n;
    struct bc_parser parser = { 0 };
    struct proxy_state st = { 0 };
    size_t bufsz;

    sscanf(argv[1], "%zu", &bufsz);
    buf = malloc(bufsz);

    st.argbuf = malloc(4096);
    st.obuf = malloc(4096);
    parser.data = &st;

    parser.on_open = on_open;
    parser.on_close = on_close;
    parser.on_text = on_text;
    parser.on_tag_text = on_tag_text;
    parser.on_arg_end = on_arg_end;
    while ((n = read(STDIN_FILENO, buf, bufsz)) > 0) {
        bc_parse(&parser, buf, n);
        write(STDOUT_FILENO, st.obuf, st.olen);
        st.olen = 0;
    }
}
