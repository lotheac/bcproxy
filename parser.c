#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <err.h>
#include "parser.h"

#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

enum state {
	s_text = 0,	/* Regular text, or text inside BC tags (including args
			 * for opening tags) */
	s_esc,
	s_open,		/* ESC< */
	s_open_n,	/* ESC<[0-9] */
	s_close,	/* ESC> */
	s_close_n,	/* ESC>[0-9] */
	s_after_10,	/* After closing tag 10; on_prompt is only called with
			 * ESC>20\xFF\xF9 */
	s_iac,		/* TELNET IAC \377 (\xFF) after closing tag 10 */
};

/*
 * Calls text or tag_text callback depending on parser state. Does nothing if
 * either buf or the callback function is NULL, but asserts that parser is in
 * the correct state.
 */
static void
callback_data(struct bc_parser *parser, const char *buf, size_t len)
{
	assert(parser->state == s_text || parser->state == s_iac);
	if (!buf)
		return;
	if (parser->tag && parser->on_tag_text)
		parser->on_tag_text(parser, buf, len);
	else if (parser->on_text)
		parser->on_text(parser, buf, len);
}

static void
tag_push(struct bc_parser *parser, int code)
{
	struct tag *new = malloc(sizeof(struct tag));
	if (!new) {
		warn("parser: ignoring tag %d due to malloc", code);
		return;
	}
	if (parser->on_open)
		parser->on_open(parser);
	new->prev = parser->tag;
	new->code = code;
	parser->tag = new;
}

static void
tag_pop(struct bc_parser *parser)
{
	struct tag *tag = parser->tag;
	/* MUD can send some extraneous ending tags */
	if (!tag || parser->partial_code != tag->code)
		return;
	if (parser->on_close)
		parser->on_close(parser);
	parser->tag = tag->prev;
	free(tag);
}

void
bc_parse(struct bc_parser *parser, const char *buf, size_t len)
{
	const char *text_start = NULL;
	const char *p;

	for (p = buf; p < buf + len; p++) {
		char ch = *p;
reread:
		switch (parser->state) {
		case s_after_10:
			if (ch == '\377') {
				parser->state = s_iac;
				break;
			}
			else
				parser->state = s_text;
			/* FALLTHROUGH */
		case s_text: {
			if (ch == '\033') {
				callback_data(parser, text_start, p -
				    text_start);
				parser->state = s_esc;
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
				/*
				 * The previous char (ESC) was part of text,
				 * but we can't necessarily reach it any more
				 * (it might have been in the buffer for the
				 * previous call). If so, just send a
				 * callback for it here.
				 */
				if (p - 1 < buf)
					callback_data(parser, "\033", 1);
				else
					text_start = p - 1;
				goto reread;
			}
			break;
		}
		case s_iac:
			if (ch == '\371') {
				if (parser->on_prompt)
					parser->on_prompt(parser);
				/* Sam deal as with ESC above */
				if (p - 1 < buf)
					callback_data(parser, "\377", 1);
				else
					text_start = p - 1;
			}
			parser->state = s_text;
			goto reread;
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
				if (parser->partial_code == 10)
					parser->state = s_after_10;
				else
					parser->state = s_text;
				tag_pop(parser);
				parser->partial_code = 0;
			}
			break;
		}
		}
	}
	if (text_start)
		callback_data(parser, text_start, p - text_start);
}
