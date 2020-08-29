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

/*
 * https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * https://invisible-island.net/xterm/xterm.faq.html#color_by_number
 *
 * XXX there are interesting historical reasons for the differences in how
 * indexed and direct color sequences are handled in terminal emulators, see
 * the above links.
 * given the above, using colons in the control sequence as parameter substring
 * separators would be preferred, since according to Dickey, ISO-8613-6
 * specifies colons for that purpose. but since tinyfugue only understands
 * semicolons in this context, and since this entire compilation unit exists to
 * enable compatibility with tinyfugue, we use semicolons here.
 *
 * the indexed (256) color control sequence we generate contains, at maximum:
 *     2 fixed bytes: ESC [
 *     2 bytes for foreground/background selection ("38" or "48")
 *     3 fixed bytes: ; 5 ;
 *     3 bytes for the color index
 *     2 fixed bytes: m NUL
 */
static char colorbuf[2+2+3+3+2];

char *
colorstr(bool foreground, uint8_t r, uint8_t g, uint8_t b)
{
	snprintf(colorbuf, sizeof(colorbuf), "\x1b[%s;5;%hhum",
	    foreground ? "38" : "48", rgb_to_xterm(r, g, b));
	return colorbuf;
}
