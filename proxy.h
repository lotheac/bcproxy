#ifndef proxy_h
#define proxy_h
#include "parser.h"

struct proxy_state {
    char    *obuf;
    char    *argbuf;
    char    *argstr;
    size_t  olen;
    size_t  arglen;
    size_t  osz;
    size_t  argsz;
};

void on_open(struct bc_parser *parser);
void on_close(struct bc_parser *parser);
void on_arg_end(struct bc_parser *parser);

void on_arg(struct bc_parser *parser, const char *buf, size_t len);
void on_text(struct bc_parser *parser, const char *buf, size_t len);

#endif /* proxy_h */
