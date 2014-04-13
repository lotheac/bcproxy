#include <unistd.h>
#include <string.h>
#include "bc_parser.h"

#define ESC '\033'
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

enum state {
    s_text = 0,
    s_esc,
    s_open,     /* ESC< */
    s_open_n,   /* ESC<[0-9] */
    s_close,    /* ESC> */
    s_close_n,  /* ESC>[0-9] */
};

void bc_parser_init(struct bc_parser *parser) {
    memset(parser, 0, sizeof(struct bc_parser));
}

size_t bc_parse(struct bc_parser *parser, const char *buf, size_t len) {
    const char *text_start = NULL;
    const char *p;
    for (p = buf; p < buf + len; p++) {
        char ch = *p;
        switch (parser->state) {
            case s_text: {
                if (!text_start)
                    text_start = p;
                if (ch == ESC) {
                    if(parser->on_text)
                        parser->on_text(parser, text_start, p - text_start);
                    parser->state = s_esc;
                    text_start = NULL;
                }
                break;
            }
            case s_esc: {
                if (ch == '<') {
                    parser->state = s_open;
                    parser->code = 0;
                } else if (ch == '|') {
                    parser->state = s_text;
                    if (parser->on_arg_end) {
                        parser->on_arg_end(parser);
                    }
                } else if (ch == '>') {
                    parser->state = s_close;
                    parser->code = 0;
                }
                break;
            }
            case s_open:
            case s_open_n: {
                if (!IS_DIGIT(ch)) {
                    parser->state = s_text;
                    break;
                }
                parser->code *= 10;
                parser->code += (ch - '0');
                if (parser->state == s_open)
                    parser->state = s_open_n;
                else {
                    if (parser->on_open)
                        parser->on_open(parser);
                    parser->state = s_text;
                }
                break;
            }
            case s_close:
            case s_close_n: {
                if (!IS_DIGIT(ch)) {
                    parser->state = s_text;
                    break;
                }
                parser->code *= 10;
                parser->code += (ch - '0');
                if (parser->state == s_close)
                    parser->state = s_close_n;
                else {
                    if (parser->on_close)
                        parser->on_close(parser);
                    parser->state = s_text;
                }
                break;
            }
        }
    }
    if (parser->state == s_text && parser->on_text)
        parser->on_text(parser, text_start, p - text_start);
    /* XXX maybe this function should be void since we never fail */
    return len;
}
