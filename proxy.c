#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "parser.h"
#include "proxy.h"


/* FIXME really need to create a data type for these buffers */
static void append_arg(struct proxy_state *st, const char *buf, size_t len) {
    if (st->arglen + len > st->argsz) {
        size_t newsz = len > st->argsz ? len * 2 : st->argsz * 2;
        char *newp = realloc(st->argbuf, newsz);
        if (!newp) {
            perror("realloc arg buffer");
        }
        st->argbuf = newp;
        st->argsz = newsz;
    }
    assert(st->arglen + len <= st->argsz);
    memcpy(st->argbuf + st->arglen, buf, len);
    st->arglen += len;
}

static void append_output(struct proxy_state *st, const char *buf, size_t len) {
    if (st->olen + len > st->osz) {
        size_t newsz = len > st->osz ? len * 2 : st->osz * 2;
        char *newp = realloc(st->obuf, newsz);
        if (!newp) {
            perror("realloc o buffer");
        }
        st->obuf = newp;
        st->osz = newsz;
    }
    assert(st->olen + len <= st->osz);
    memcpy(st->obuf + st->olen, buf, len);
    st->olen += len;
}

void on_open(struct bc_parser *parser) {
    parser = parser;
}

void on_close(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;
    st->ignore = 0;
}

void on_arg(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    append_arg(st, buf, len);
}

void on_arg_end(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;
    char *argstr = malloc(st->arglen + 1);
    memcpy(argstr, st->argbuf, st->arglen);
    argstr[st->arglen] = '\0';
    /* FIXME magic numbers */
    switch (parser->code) {
        case 10: /* Message with type */
            if (strcmp(argstr, "spec_prompt") == 0) {
                /* These are unwanted since they show up every second in
                 * addition to the normal prompt line */
                st->ignore = 1;
            }
            break;
        case 22: /* Message attributes */
        case 23:
        case 24:
        case 25:
            append_output(st, st->argbuf, st->arglen);
    }

    st->arglen = 0;
    free(argstr);
}

void on_text(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    if (!st->ignore)
        append_output(st, buf, len);
}
