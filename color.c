#include <err.h>
#include <stdio.h>
#include "color.h"

/*
 * https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * the direct color control sequence we generate contains, at maximum:
 *     2 fixed bytes: ESC [
 *     2 bytes for foreground/background selection ("38" or "48")
 *     3 fixed bytes: : 2 :
 *     2 fixed bytes: 0 :
 *       - this is Pi, the "color space identifier", which is ignored by
 *       xterm
 *     3 bytes for Pr, the red color value
 *     1 byte: :
 *     3 bytes for Pg, the green color value
 *     1 byte: :
 *     3 bytes for Pb, the blue color value
 *     2 bytes: m NUL
 */
static char colorbuf[2+2+3+2+3+1+3+1+3+2];

char *
colorstr(bool foreground, uint8_t r, uint8_t g, uint8_t b)
{
	snprintf(colorbuf, sizeof(colorbuf), "\x1b[%s:2:0:%hhu:%hhu:%hhum",
	    foreground ? "38" : "48", r, g, b);
	return colorbuf;
}
