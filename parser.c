#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "parser.h"

#define ESC '\033'
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

enum state {
    s_text = 0, /* Regular text, or text inside BC tags (including args for
                   opening tags) */
    s_esc,
    s_open,     /* ESC< */
    s_open_n,   /* ESC<[0-9] */
    s_close,    /* ESC> */
    s_close_n,  /* ESC>[0-9] */
    s_iac,      /* TELNET IAC \377 (\xFF) */
};

/* Calls text or tag_text callback depending on parser state. Does nothing if
 * either buf or the callback function is NULL, but asserts that parser is in
 * the correct state. */
static void callback_data(struct bc_parser *parser, const char *buf, size_t len) {
    assert(parser->state == s_text);
    if (!buf)
        return;
    if (parser->tag && parser->on_tag_text)
        parser->on_tag_text(parser, buf, len);
    else if (parser->on_text)
        parser->on_text(parser, buf, len);
}

static void tag_push(struct bc_parser *parser, int code) {
    struct tag *new = malloc(sizeof(struct tag));
    if (!new) {
        fprintf(stderr, "parser: ignoring tag %d due to malloc: %s\n",
                code, strerror(errno));
        return;
    }
    new->prev = parser->tag;
    new->code = code;
    parser->tag = new;
    if (parser->on_open)
        parser->on_open(parser);
}

static void tag_pop(struct bc_parser *parser) {
    struct tag *tag = parser->tag;
    if (!tag || parser->partial_code != tag->code) {
        fprintf(stderr, "parser warning: unexpected end tag %d.%s\n",
                parser->partial_code,
                tag ? " tag stack:" : "");
        for (struct tag *cur = tag; cur != NULL; cur = cur->prev) {
            fprintf(stderr, "\t%d\n", cur->code);
        }
        return;
    }
    if (parser->on_close)
        parser->on_close(parser);
    parser->tag = tag->prev;
    free(tag);
}

void bc_parse(struct bc_parser *parser, const char *buf, size_t len) {
    const char *text_start = NULL;
    const char *p;

    for (p = buf; p < buf + len; p++) {
        char ch = *p;
reread:
        switch (parser->state) {
            case s_text: {
                if (ch == ESC || ch == '\377') {
                    callback_data(parser, text_start, p - text_start);
                    parser->state = ch == ESC ? s_esc : s_iac;
                    text_start = NULL;
                } else if (!text_start)
                    text_start = p;
                break;
            }
            case s_esc: {
                if (ch == '<') {
                    parser->state = s_open;
                } else if (ch == '|') {
                    parser->state = s_text;
                    if (parser->on_arg_end)
                        parser->on_arg_end(parser);
                } else if (ch == '>') {
                    parser->state = s_close;
                } else {
                    parser->state = s_text;
                    /* The previous char (ESC) was part of text, but we can't
                     * necessarily reach it any more (it might have been in the
                     * buffer for the previous call). If so, just send a
                     * callback for it here. */
                    if (p - 1 < buf)
                        callback_data(parser, "\033", 1);
                    else
                        text_start = p - 1;
                    goto reread;
                }
                break;
            }
            case s_iac:
                /* This is a special case; the MUD sends additional prompt
                 * messages every second in BC mode, and "normal" prompt
                 * messages are wrapped in BC protocol as well, but they will
                 * be followed by TELNET GOAHEAD (not inside the tag). Proxy
                 * code could just append GOAHEAD to account for the periodic
                 * messages, but we can't have two consecutive GOAHEADs since
                 * that will make clients show an empty prompt line. Proxy code
                 * could also buffer some more to see whether it needs to
                 * output GOAHEAD or not, but it would delay prompt output
                 * until next packet from server if GOAHEAD wasn't present.
                 * Just don't consider GOAHEAD part of the text in the parser,
                 * and unconditionally output it in proxy code. */
                parser->state = s_text;
                if (ch != '\371') {
                    /* Not GOAHEAD, so output the IAC to client */
                    if (p - 1 < buf)
                        callback_data(parser, "\377", 1);
                    else
                        text_start = p - 1;
                    goto reread;
                }
                break;
            case s_open:
            case s_open_n: {
                if (!IS_DIGIT(ch)) {
                    parser->state = s_text;
                    break;
                }
                parser->partial_code *= 10;
                parser->partial_code += (ch - '0');
                if (parser->state == s_open)
                    parser->state = s_open_n;
                else {
                    parser->state = s_text;
                    tag_push(parser, parser->partial_code);
                    parser->partial_code = 0;
                }
                break;
            }
            case s_close:
            case s_close_n: {
                if (!IS_DIGIT(ch)) {
                    parser->state = s_text;
                    break;
                }
                parser->partial_code *= 10;
                parser->partial_code += (ch - '0');
                if (parser->state == s_close)
                    parser->state = s_close_n;
                else {
                    parser->state = s_text;
                    tag_pop(parser);
                    parser->partial_code = 0;
                }
                break;
            }
        }
    }
    if (parser->state == s_text)
        callback_data(parser, text_start, p - text_start);
}
