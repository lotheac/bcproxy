#include <string.h>

int
main(void)
{
	char dst[10];
	strlcpy(dst, "hello", 10);
	return 0;
}
