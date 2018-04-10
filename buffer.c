#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"

buffer *
buffer_new(size_t initial_size)
{
	buffer *buf = malloc(sizeof(buffer));
	char *data = malloc(initial_size);
	if (!buf || !data) {
		free(buf);
		free(data);
		return NULL;
	}
	buf->data = data;
	buf->len = 0;
	buf->sz = initial_size;
	return buf;
}

void
buffer_free(buffer *buf)
{
	if (buf) {
		free(buf->data);
		free(buf);
	}
}

int
buffer_append(buffer *buf, const char *input, size_t len)
{
	if (buf->len + len > buf->sz) {
		size_t newsz;
		if (len > buf->sz)
			newsz = len * 2;
		else
			newsz = buf->sz * 2;
		char *newp = realloc(buf->data, newsz);
		if (!newp)
			return -1;
		buf->data = newp;
		buf->sz = newsz;
	}
	memcpy(buf->data + buf->len, input, len);
	buf->len += len;
	return 0;
}

int
buffer_append_iso8859_1(buffer *buf, const char *input, size_t len)
{
	/* Worst case: every input character is invalid, and we need 3 output
	 * bytes (U+FFFD in UTF-8) */
	if (buf->len + 3*len > buf->sz) {
		size_t newsz = 3*len + buf->sz;
		char *newp = realloc(buf->data, newsz);
		if (!newp)
			return -1;
		buf->data = newp;
		buf->sz = newsz;
	}
	uint8_t *out = (uint8_t *) buf->data + buf->len;
	const uint8_t *in;
	uint8_t *end = (uint8_t *) input + len;
	for (in = (const uint8_t *) input; in < end; in++) {
		if (*in <= 0x7e)
			*out++ = *in;
		else if (*in >= 0xa0) {
			/* use *in as unicode codepoint. our codepoints are
			 * thus 8 bits, 2 of which will be in the first byte
			 * and the rest in the second byte. */
			*out++ = 0xc0 | (*in >> 6);
			*out++ = 0x80 | (*in & 0x3f);
		} else {
			/* unknown character. use U+FFFD. */
			*out++ = 0xef;
			*out++ = 0xbf;
			*out++ = 0xbd;
		}
	}
	size_t bytes = out - (uint8_t *)(buf->data + buf->len);
	buf->len += bytes;
	return 0;
}

int
buffer_append_buf(buffer *buf, const buffer *ibuf)
{
	return buffer_append(buf, ibuf->data, ibuf->len);
}

int
buffer_append_str(buffer *buf, const char *str)
{
	size_t len = strlen(str);
	return buffer_append(buf, str, len);
}

void
buffer_clear(buffer *buf)
{
	buf->len = 0;
}
