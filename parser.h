#ifndef bc_parser_h
#define bc_parser_h

struct tag;
struct tag {
	int	code;
	struct	tag *prev;
};

struct bc_parser;

/* Callback function whose arguments are the parser that calls it and pointer
 * to the start of the match, plus length. */
typedef void (*bc_callback) (struct bc_parser *, const char *, size_t);
/* Callback function for notification, carries no data (just the pointer to the
 * parser) */
typedef void (*bc_notify) (struct bc_parser *);

struct bc_parser {
	/* Public */
	/* Called with data pointer to regular text. */
	bc_callback	on_text;
	/* Called with data pointer inside BC tag. */
	bc_callback	on_tag_text;
	/* Called when tag's opening argument ends, ie. at 'ESC|'. */
	bc_notify	on_arg_end;
	/* Called when a tag is opened. */
	bc_notify	on_open;
	/* Called when a tag is closed. */
	bc_notify	on_close;
	/* Called upon TELNET GOAHEAD after closing tag number 10, ie. ESC
	 * 10\xFF\xF9 */
	bc_notify	on_prompt;
	/* application data pointer; not used by parser */
	void		*data;
	struct		tag *tag;
	/* Private */
	int		partial_code;
	int		state;
};

void	bc_parse(struct bc_parser *, const char *, size_t);

#endif /* bc_parser_h */
