#include <err.h>
#include <stdio.h>
#include "color.h"

static const uint32_t base_colors[16] = {
	0x000000,
	0x800000,
	0x008000,
	0x808000,
	0x000080,
	0x800080,
	0x008080,
	0xc0c0c0,
	0x808080,
	0xff0000,
	0x00ff00,
	0xffff00,
	0x0000ff,
	0xff00ff,
	0x00ffff,
	0xffffff,
};

/* linearly scale input from [a,b] to [c,d] */
static int
scale(int x, int a, int b, int c, int d)
{
	return c + (x - a) * (d - c) / (b - a);
}

static uint8_t
rgb_to_xterm(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t rgb = (r << 16) | (g << 8) | b;
	for (int i = 0; i < 16; i++) {
		if (rgb == base_colors[i])
			return i;
	}
	if (r == g && r == b) {
		/* grayscale; there are 24 colors plus two more for base black
		 * and white. The ramp begins at index 232. */
		unsigned index = scale(r, 0, UINT8_MAX, 0, 25);
		if (index == 0)
			return 0;
		else if (index == 25)
			return 15;
		else
			return 231 + index;
	}

	uint8_t rx, gx, bx;
	/* xterm color cube is 6x6x6, starting from index 16 */
	rx = scale(r, 0, UINT8_MAX, 0, 5);
	gx = scale(g, 0, UINT8_MAX, 0, 5);
	bx = scale(b, 0, UINT8_MAX, 0, 5);
	return 16 + 36 * rx + 6 * gx + bx;
}

char *
colorstr_alloc(bool foreground, uint8_t r, uint8_t g, uint8_t b)
{
	char *out = NULL;
	if (asprintf(&out, "\x1b[%u;5;%hhum",
	    foreground ? 38 : 48,
	    rgb_to_xterm(r, g, b)) < 0) {
		warnx("color alloc");
		return NULL;
	}
	return out;
}
