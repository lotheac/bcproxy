#include <tls.h>
int
main(void)
{
	/* just a compilation/link test */
	struct tls *client = tls_client();
	tls_connect(client, "localhost", "https");
	return 0;
}
