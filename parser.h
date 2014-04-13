#ifndef bc_parser_h
#define bc_parser_h

struct bc_parser;

/* Callback function whose arguments are the parser that calls it and pointer
 * to the start of the match, plus length. */
typedef void (*bc_callback) (struct bc_parser *parser, const char *loc, size_t len);
/* Callback function for notification, carries no data (just the pointer to the
 * parser) */
typedef void (*bc_notify) (struct bc_parser *parser);

struct bc_parser {
    int         state;
    int         code;
    bc_callback on_text;
    bc_callback on_arg;
    bc_notify   on_arg_end;
    bc_notify   on_open;
    bc_notify   on_close;
    void        *data; /* application data pointer */
};

void bc_parse(struct bc_parser *parser, const char *buf, size_t len);

#endif /* bc_parser_h */
