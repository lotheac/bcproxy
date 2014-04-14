#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "parser.h"
#include "proxy.h"
#include "buffer.h"

void on_open(struct bc_parser *parser) {
    parser = parser;
}

void on_close(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;

    switch (parser->code) {
        case 10:
            if (st->argstr) {
                if (strcmp(st->argstr, "spec_prompt") == 0) {
                    buffer_append_buf(st->obuf, st->tmpbuf);
                    /* Parser removes GOAHEAD; see discussion in parser.c */
                    buffer_append(st->obuf, "\377\371", 2);
                    break;
                } else if (strcmp(st->argstr, "spec_map") == 0 &&
                           strcmp(st->tmpbuf->data, "NoMapSupport") == 0) {
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
            buffer_append_buf(st->obuf, st->tmpbuf);
    }

    free(st->argstr),
    buffer_clear(st->tmpbuf);
}

void on_tag_text(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    buffer_append(st->tmpbuf, buf, len);
}

void on_arg_end(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;
    st->argstr = malloc(st->tmpbuf->len + 1);
    if (st->argstr) {
        memcpy(st->argstr, st->tmpbuf->data, st->tmpbuf->len);
        st->argstr[st->tmpbuf->len] = '\0';
    } else
        perror("argstr: malloc");

    buffer_clear(st->tmpbuf);
}

void on_text(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    buffer_append(st->obuf, buf, len);
}
