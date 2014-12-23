#ifndef proxy_h
#define proxy_h
#include "parser.h"
#include "buffer.h"

struct proxy_state {
	buffer *	obuf;
	buffer *	tmpbuf;
	char *		argstr;
	struct room *	room;
};

struct proxy_state *	proxy_state_new(size_t bufsize);
void			proxy_state_free(struct proxy_state *state);

void	on_open(struct bc_parser *);
void	on_close(struct bc_parser *);
void	on_arg_end(struct bc_parser *);
void	on_prompt(struct bc_parser *);

void	on_text(struct bc_parser *, const char *, size_t);
void	on_tag_text(struct bc_parser *, const char *, size_t);

#endif /* proxy_h */
