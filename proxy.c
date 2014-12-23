#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <err.h>
#include "parser.h"
#include "proxy.h"
#include "buffer.h"
#include "color.h"
#include "room.h"

struct
proxy_state *proxy_state_new(size_t bufsize)
{
	struct proxy_state *st = calloc(1, sizeof(struct proxy_state));
	if (!st)
		goto err;
	st->obuf = buffer_new(bufsize);
	st->tmpbuf = buffer_new(bufsize);
	if (!st->obuf || !st->tmpbuf)
		goto err;
	st->db = PQconnectdb("dbname=batmud");
	if (PQstatus(st->db) != CONNECTION_OK) {
		warnx("PQconnectdb: %s", PQerrorMessage(st->db));
		goto err;
	}
	if (db_init(st->db) < 0)
		goto err;
	return st;
err:
	proxy_state_free(st);
	return NULL;
}

void
proxy_state_free(struct proxy_state *state)
{
	if (state) {
		buffer_free(state->obuf);
		buffer_free(state->tmpbuf);
		free(state->argstr);
		room_free(state->room);
		PQfinish(state->db);
		free(state);
	}
}

void
on_open(struct bc_parser *parser)
{
	struct proxy_state *st = parser->data;
	/*
	 * If we are already inside a tag and there is something pending
	 * output, we need to either clear it (output incomplete tag) or have
	 * the next tag's processed output appended to it so that we get the
	 * entire processed contents contents of the outer tag in on_close. For
	 * now, let's just do the first option.
	 */
	if (parser->tag && (st->tmpbuf->len || st->argstr))
		on_close(parser);
}

void
on_prompt(struct bc_parser *parser)
{
	struct proxy_state *st = parser->data;
	/*
	 * Output deferred stuff. If there is none, this is a GOAHEAD *not*
	 * following a 10 closing tag; it'll get passed to client normally.
	 */
	if (st->tmpbuf->len) {
		buffer_append_buf(st->obuf, st->tmpbuf);
		buffer_clear(st->tmpbuf);
	}
}

void
on_close(struct bc_parser *parser)
{
	assert(parser->tag);
	struct proxy_state *st = parser->data;
	/* Make sure tmpbuf is null terminated. It might have other nulls
	 * besides, but that's ok. */
	buffer_append(st->tmpbuf, "", 1);
	/* Set the length back to what it was so that we don't include the NUL
	 * in output where we don't want to. */
	st->tmpbuf->len -= 1;
	char *tmpstr = st->tmpbuf->data;
	int defer = 0;

	switch (parser->tag->code) {
	case 5: /* connection success */
	case 6: /* connection failure */
		break;
	case 10: /* Message with type */
		if (st->argstr) {
			if (strcmp(st->argstr, "spec_prompt") == 0) {
				/*
				 * The MUD sends spec_prompt messages every
				 * second in addition to prompting after user
				 * input. The periodic messages are *not*
				 * followed by TELNET GOAHEAD, but the other
				 * ones are, and we don't want to output the
				 * periodic prompts, so defer output to
				 * on_prompt (ie. avoid clearing tmpbuf).
				 */
				defer = 1;
				break;
			} else if (strcmp(st->argstr, "spec_map") == 0) {
				if (strcmp(tmpstr, "NoMapSupport") == 0)
					break;
			} else if (strcmp(st->argstr, "spec_news") != 0) {
				buffer_append_str(st->obuf, st->argstr);
				buffer_append_str(st->obuf, ": ");
			}
		}
		buffer_append_buf(st->obuf, st->tmpbuf);
		break;
	case 11: /* Clear screen */
		break;
	case 20: /* Set fg color */
	case 21: /* Set bg color */
		if (st->argstr) {
			/* Recent xterm supports closest match for ISO-8613-3
			 * color controls (24-bit), but tf doesn't, so we need
			 * to do the approximation ourselves. */
			uint32_t rgb;
			char *out = NULL;
			int is_fg = parser->tag->code == 20;
			if (sscanf(st->argstr, "%6x", &rgb) != 1) {
				perror("color sscanf");
				break;
			}
			asprintf(&out, "\033[%u8;5;%um%s\033[0m",
			    is_fg ? 3 : 4,
			    rgb_to_xterm(rgb),
			    tmpstr);
			if (!out) {
				perror("color asprintf");
				break;
			}
			buffer_append_str(st->obuf, out);
			free(out);
		}
		break;
	case 22: /* Bold */
	case 23: /* Italic */
	case 24: /* Underlined */
	case 25: /* Blink */
	case 31: /* "in-game link" */
		buffer_append_buf(st->obuf, st->tmpbuf);
		break;
	case 40: /* clear skill/spell status */
	case 41: /* spell rounds left */
	case 42: /* skill rounds left */
		break;
	case 50: { /* full hp/sp/ep status */
#ifdef ENABLE_FULLHP
		int hp, hpmax, sp, spmax, ep, epmax;
		if (sscanf(st->tmpbuf->data, "%d %d %d %d %d %d", &hp, &hpmax,
		    &sp, &spmax, &ep, &epmax) != 6) {
			perror("full health status sscanf");
			break;
		}
		char *out = NULL;
		asprintf(&out, "[fhp]%d/%d %d/%d %d/%d\n", hp, hpmax, sp, spmax,
		    ep, epmax);
		if (!out) {
			perror("full health status asprintf");
			break;
		}
		buffer_append_str(st->obuf, out);
		free(out);
#endif
		break;
	}
	case 51: { /* partial hp/sp/ep status */
		int hp, sp, ep;
		if (sscanf(st->tmpbuf->data, "%d %d %d", &hp, &sp, &ep) != 6) {
			perror("partial health status sscanf");
			break;
		}
		char *out = NULL;
		asprintf(&out, "[hp]%d %d %d\n", hp, sp, ep);
		if (!out) {
			perror("partial health status asprintf");
			break;
		}
		buffer_append_str(st->obuf, out);
		free(out);
		break;
	}
	case 52: /* player name, race, level etc. and exp */
	case 53: /* exp */
	case 54: /* player status */
	case 60: /* player location */
	case 62: /* party status */
	case 63: /* player left party */
		break;
	case 64: /* prot status */
		buffer_append_str(st->obuf, "[prots]");
		buffer_append_buf(st->obuf, st->tmpbuf);
		buffer_append_str(st->obuf, "\n");
		break;
	case 70: /* target health */
		buffer_append_str(st->obuf, "[target]");
		buffer_append_buf(st->obuf, st->tmpbuf);
		buffer_append_str(st->obuf, "\n");
		break;
	case 99:
		if (strncmp(tmpstr, "BAT_MAPPER;;", strlen("BAT_MAPPER;;")) == 0) {
			char *msg = NULL;
			struct room *new = NULL;
			if (strcmp(tmpstr, "BAT_MAPPER;;REALM_MAP") == 0) {
				asprintf(&msg, "Exited to map from %s.\n",
				    st->room ?  st->room->area : "(unknown)");
			} else {
				new = room_new(tmpstr);
				if (!new) {
					warnx("failed to allocate new room: \n"
					   "%s", tmpstr);
					break;
				}
				db_add_room(st->db, new);
				if (!st->room || strcmp(st->room->area, new->area) != 0)
					asprintf(&msg, "Entered area %s with "
					    "direction %s, %sdoors\n",
					    new->area, new->direction,
					    new->indoors ? "in" : "out");
				else {
					asprintf(&msg, "Moved (%s) --%s-> "
					    "(%s),%sdoors\n", st->room->id,
					    new->direction, new->id,
					    new->indoors ? "in" : "out");
					db_add_exit(st->db, st->room, new);
				}
			}
			room_free(st->room);
			st->room = new;
			if (msg) {
				buffer_append_str(st->obuf, msg);
				free(msg);
			}
		}
		break;
	default: {
		char *str = NULL;
		int len = asprintf(&str, "[unknown tag %d]%s\n",
		    parser->tag->code, tmpstr);
		if (str)
			buffer_append(st->obuf, str, len);
		else
			perror("default asprintf");
		free(str);
		break;
	}
	}

	free(st->argstr);
	st->argstr = NULL;
	if (!defer)
		buffer_clear(st->tmpbuf);
}

void
on_tag_text(struct bc_parser *parser, const char *buf, size_t len)
{
	struct proxy_state *st = parser->data;
	buffer_append(st->tmpbuf, buf, len);
}

void
on_arg_end(struct bc_parser *parser)
{
	struct proxy_state *st = parser->data;
	/*
	 * XXX We assume that control code arguments don't contain NUL bytes,
	 * but if they do, the failure is graceful: string comparisons fail
	 * earlier instead of at the end of the allocated buffer
	 */
	st->argstr = malloc(st->tmpbuf->len + 1);
	if (st->argstr) {
		memcpy(st->argstr, st->tmpbuf->data, st->tmpbuf->len);
		st->argstr[st->tmpbuf->len] = '\0';
	} else
		perror("argstr: malloc");

	buffer_clear(st->tmpbuf);
}

void
on_text(struct bc_parser *parser, const char *buf, size_t len)
{
	struct proxy_state *st = parser->data;
	buffer_append(st->obuf, buf, len);
}
