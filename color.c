#include <err.h>
#include <stdio.h>
#include "color.h"

char *
colorstr_alloc(bool foreground, uint8_t r, uint8_t g, uint8_t b)
{
	char *out = NULL;
	if (asprintf(&out, "\x1b[%u;2;%hhu;%hhu;%hhum",
	    foreground ? 38 : 48,
	    r, g, b) < 0) {
		warnx("color alloc");
		return NULL;
	}
	return out;
}
