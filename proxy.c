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

    switch (parser->code) {
        case 10:
            if (st->argstr) {
                if (strcmp(st->argstr, "spec_prompt") == 0) {
                    append_output(st, st->argbuf, st->arglen);
                    /* Parser removes GOAHEAD; see discussion in parser.c */
                    append_output(st, "\377\371", 2);
                    break;
                } else if (strcmp(st->argstr, "spec_map") == 0 &&
                           strcmp(st->argbuf, "NoMapSupport") == 0) {
                    break;
                }
            }
            /* Other msg 10 types fallthrough */
        case 20:
        case 21:
        case 22: /* Message attributes */
        case 23:
        case 24:
        case 25:
        case 31: /* "in-game link" */
            append_output(st, st->argbuf, st->arglen);
    }

    free(st->argstr);
    st->arglen = 0;
}

void on_tag_text(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    append_arg(st, buf, len);
}

void on_arg_end(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;
    st->argstr = malloc(st->arglen + 1);
    if (st->argstr) {
        memcpy(st->argstr, st->argbuf, st->arglen);
        st->argstr[st->arglen] = '\0';
    } else
        perror("argstr: malloc");

    st->arglen = 0;
}

void on_text(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    append_output(st, buf, len);
}
