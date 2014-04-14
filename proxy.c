#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "parser.h"
#include "proxy.h"
#include "buffer.h"

#define BAT_MAPPER "BAT_MAPPER;;"
#define UNKNOWN_TAG "[unknown tag %d]%s[/]\n"
#define PROTS "[prots]"
#define XTERM_FG_DEFAULT "\033[39m"
#define XTERM_BG_DEFAULT "\033[49m"
#define XTERM_256_FMT "\033[%d;5;%dm"

void on_open(struct bc_parser *parser) {
    parser = parser;
}

void on_close(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;

    /* NULL-terminate tmpbuf to allow string operations on it */
    buffer_append(st->tmpbuf, "", 1);
    switch (parser->code) {
        case 5: /* connection success */
        case 6: /* connection failure */
            break;
        case 10: /* Message with type */
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
            buffer_append_buf(st->obuf, st->tmpbuf);
            break;
        case 11: /* Clear screen */
            break;
        case 20: /* Set fg color */
        case 21: /* Set bg color */
            if (st->argstr) {
                /* TODO: Recent xterm supports closest match for ISO-8613-3
                 * color controls (24-bit), but tf doesn't, so we need to to
                 * approximate the closest match ourselves here and output the
                 * corresponding 256-color escapes */
#if 0
                unsigned r, g, b, color, fg_or_bg;
                char colorbuf[sizeof(XTERM_256_FMT) + 1]; /* 1 for extra digit */
                fg_or_bg = 38 + (parser->code - 20) * 10;
                sscanf(st->argstr, "%2x%2x%2x", &r, &g, &b);
                /* FIXME: actually create the color */
                sprintf(colorbuf, XTERM_256_FMT, fg_or_bg, color);
                buffer_append(st->obuf, colorbuf, sizeof(XTERM_256_FMT));
#endif
            }
            buffer_append_buf(st->obuf, st->tmpbuf);
            buffer_append(st->obuf, XTERM_FG_DEFAULT, sizeof(XTERM_FG_DEFAULT));
            break;
        case 22: /* Bold */
        case 23: /* Italic */
        case 24: /* Underlined */
        case 25: /* Blink */
        case 31: /* "in-game link" */
            buffer_append_buf(st->obuf, st->tmpbuf);
            break;
        case 50: /* full hp/sp/ep status */
        case 51: /* partial hp/sp/ep status */
        case 52: /* player name, race, level etc. and exp */
        case 53: /* exp */
        case 54: /* player status */
            break;
        case 64: /* prot status */
            buffer_append(st->obuf, PROTS, sizeof(PROTS));
            /* NOTE: this includes a NUL but that's ok */
            buffer_append_buf(st->obuf, st->tmpbuf);
            buffer_append(st->obuf, "\n", 1);
            break;
        case 99:
            if (strncmp(st->tmpbuf->data,
                        BAT_MAPPER,
                        sizeof(BAT_MAPPER) - 1) == 0) {
            }
            break;
        default: {
            char *str;
            int len = snprintf(NULL, 0, UNKNOWN_TAG, parser->code,
                               st->tmpbuf->data);
            str = malloc(len + 1);
            if (str) {
                sprintf(str, UNKNOWN_TAG, parser->code, st->tmpbuf->data);
                buffer_append(st->obuf, str, len);
            }
            free(str);
            break;
        }
    }

    free(st->argstr);
    st->argstr = NULL;
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
