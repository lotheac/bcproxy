#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "parser.h"
#include "proxy.h"
#include "buffer.h"

#define XTERM_FG_DEFAULT "\033[39m"
#define XTERM_BG_DEFAULT "\033[49m"
#define XTERM_256_FMT "\033[%d;5;%dm"

void on_open(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;
    /* If we are already inside a tag and there is something pending output, we
     * need to either clear it (output incomplete tag) or have the next tag's
     * processed output appended to it so that we get the entire processed
     * contents contents of the outer tag in on_close. For now, let's just do
     * the first option. */
    if (st->tmpbuf->len || st->argstr)
        on_close(parser);
}

void on_close(struct bc_parser *parser) {
    assert(parser->tag);
    struct proxy_state *st = parser->data;
    /* We only need a short prefix of tmpbuf for string operations */
    char tmpstr[16];
    unsigned len = 16 < st->tmpbuf->len ? 16 : st->tmpbuf->len;
    memcpy(tmpstr, st->tmpbuf->data, len);
    tmpstr[len-1] = '\0';

    switch (parser->tag->code) {
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
                } else if (strcmp(st->argstr, "spec_map") == 0) {
                        if (strcmp(tmpstr, "NoMapSupport") == 0)
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
            buffer_append(st->obuf, "[prots]", strlen("[prots]"));
            buffer_append_buf(st->obuf, st->tmpbuf);
            buffer_append(st->obuf, "\n", 1);
            break;
        case 99:
            if (strcmp(tmpstr, "BAT_MAPPER;;") == 0) {
                /* TODO store mapper data */
            }
            break;
        default: {
            char *str = NULL;
            int len = asprintf(&str, "[unknown tag %d(truncated)]%s[/]\n",
                               parser->tag->code,
                               tmpstr);
            if (str)
                buffer_append(st->obuf, str, len);
            else
                perror("asprintf");
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
    /* XXX We assume that control code arguments don't contain NUL bytes, but
     * if they do, the failure is graceful: string comparisons fail earlier
     * instead of at the end of the allocated buffer */
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
