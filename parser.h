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
    /* Public */
    bc_callback on_text;     /* Called with data pointer to regular text. */
    bc_callback on_tag_text; /* Called with data pointer inside BC tag. */
    bc_notify   on_arg_end;  /* Called when tag's opening argument ends, ie. at
                                'ESC|'. */
    bc_notify   on_open;     /* Called when a tag is opened. */
    bc_notify   on_close;    /* Called when a tag is closed. */
    void        *data;       /* application data pointer; not used by parser */
    int         code;        /* Code of previous open/close encountered. */
    /* Private */
    int         state;
    unsigned    in_tag; /* Nesting level for tags */
};

void bc_parse(struct bc_parser *parser, const char *buf, size_t len);

#endif /* bc_parser_h */
