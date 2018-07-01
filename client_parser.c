#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client_parser.h"

enum client_state {
	s_text = 0,
	s_utf8_continuation,
	s_iac,		/* TELNET IAC (\xff) */
	s_iac_3byte,	/* TELNET IAC + WILL/WONT/DO/DONT */
};

static enum client_state state = s_text;
static enum client_state stored_state = s_text;
static unsigned continuation_bytes_left = 0;
static uint32_t codepoint = 0;

/*
 * Convert UTF-8 to ISO-8859-1, but pass telnet commands as-is.
 * dst should be at least the same size as src (ie. len bytes long).
 * Returns the number of bytes written to dst.
*/
size_t
client_utf8_to_iso8859_1(char *dst, const char *src, size_t len)
{
	char *origdst = dst;
	const char *p;

	for (p = src; p < src + len; p++) {
		unsigned char ch = *p;
		/*
		 * TELNET IAC could occur at any time, even in the middle of a
		 * utf8 character's bytes (though that is maybe unlikely). So
		 * special-case it here and store current state.
		 */
		if (ch == 0xff && state != s_iac && state != s_iac_3byte) {
			stored_state = state;
			state = s_iac;
			continue;
		}
		switch (state) {
		case s_text:
			if (ch < 0x80) {
				*dst++ = ch;
			} else if ((ch & 0xe0) == 0xc0) {
				/* first 3 bits: 110 */
				state = s_utf8_continuation;
				continuation_bytes_left = 1;
				/* 5 + 6 bits */
				codepoint = (ch & 0x1f) << 6;
			} else if ((ch & 0xf0) == 0xe0) {
				/* first 4 bits: 1110 */
				state = s_utf8_continuation;
				continuation_bytes_left = 2;
				/* 4 + 6 + 6 bits */
				codepoint = (ch & 0xf) << 12;
			} else if ((ch & 0xf8) == 0xf0) {
				/* first 5 bits: 11110 */
				state = s_utf8_continuation;
				continuation_bytes_left = 3;
				/* 3 + 6 + 6 + 6 bits */
				codepoint = (ch & 0x7) << 18;
			} else {
				/* invalid byte */
				*dst++ = '?';
				state = s_text;
			}
			break;
		case s_utf8_continuation:
			if ((ch & 0xc0) != 0x80) {
				/* first two bits are not 10; invalid
				 * continuation byte. */
				*dst++ = '?';
				codepoint = 0;
				state = s_text;
				continue;
			}
			continuation_bytes_left--;
			/* 6 new bits */
			uint32_t newbits = (ch & 0x3f);
			codepoint |= (newbits << (continuation_bytes_left * 6));
			if (!continuation_bytes_left) {
				if (codepoint >= 0xa0 && codepoint <= 0xff)
					*dst++ = codepoint;
				else
					*dst++ = '?';
				codepoint = 0;
				state = s_text;
			}
			break;
		case s_iac:
			if (ch == 0xff) {
				/*
				 * TELNET-escaped 0xff byte - this makes no
				 * sense coming from the client as utf-8
				 * strings cannot contain 0xff (and the client
				 * does not send xterm control sequences or
				 * other binary to the server). Treat this as
				 * invalid byte.
				 */
				*dst++ = '?';
				state = stored_state;
				continue;
			}
			*dst++ = '\xff';
			*dst++ = ch;
			/* IAC WILL/WONT/DO/DONT */
			if (ch == 0xfb || ch == 0xfc || ch == 0xfc ||
			    ch == 0xfe)
				state = s_iac_3byte;
			else
				state = stored_state;
			break;
		case s_iac_3byte:
			/* third byte of 3-byte command */
			*dst++ = ch;
			state = stored_state;
			break;
		}
	}
	return (dst - origdst);
}
