#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include "faketls.h"
/*
 * Fake implementations of libtls functions, that don't actually do TLS and
 * just pass plain data to send/recv.
 */

int
tls_connect_socket(struct tls *ctx, int s, const char *servername
    __attribute__((unused)))
{
	ctx->fd = s;
	return 0;
}

struct tls *
tls_client(void)
{
	struct tls *ctx = malloc(sizeof(struct tls));
	ctx->fd = -1;
	return ctx;
}

ssize_t
tls_read(struct tls *ctx, void *buf, size_t buflen)
{
	ssize_t ret = recv(ctx->fd, buf, buflen, 0);
	if (ret < 0 && errno == EAGAIN)
		ret = TLS_WANT_POLLIN;
	return ret;
}

ssize_t
tls_write(struct tls *ctx, const void *buf, size_t buflen)
{
	ssize_t ret = send(ctx->fd, buf, buflen, 0);
	if (ret < 0 && errno == EAGAIN)
		ret = TLS_WANT_POLLOUT;
	return ret;
}
