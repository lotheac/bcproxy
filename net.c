#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "net.h" /* <tls.h> */

static int
socket_setblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		err(1, "fcntl F_GETFL");
	if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0)
		err(1, "fcntl F_SETFL");
	return flags;
}

/*
 * Blocking call to send all data out on a TLS socket. Prints errors on stderr
 * and returns -1 on error, the number of bytes sent on success.
 */
ssize_t
tls_sendall(struct tls *ctx, int fd, const char *buf, size_t len)
{
	const char *start = buf;
	int flags = socket_setblocking(fd);
	while (len > 0) {
		ssize_t ret;
		ret = tls_write(ctx, buf, len);
		if (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT)
			continue;
		if (ret < 0) {
			warnx("tls_write: %s", tls_error(ctx));
			return -1;
		}
		buf += ret;
		len -= ret;
	}
	if (fcntl(fd, F_SETFL, flags))
		err(1, "fcntl restore flags");
	return (buf - start);
}

/*
 * Like tls_sendall, but uses plain sockets/no TLS.
 */
ssize_t
sendall(int fd, const char *buf, size_t len)
{
	const char *start = buf;
	int flags = socket_setblocking(fd);
	while (len > 0) {
		ssize_t ret;
		ret = send(fd, buf, len, 0);
		if (ret < 0 && errno == EINTR)
			continue;
		else if (ret < 0) {
			warn("send");
			return -1;
		}
		buf += ret;
		len -= ret;
	}
	if (fcntl(fd, F_SETFL, flags))
		err(1, "fcntl restore flags");
	return (buf - start);
}

/*
 * Connects to BatMUD via TCP and enables batclient mode (by sending BC_ENABLE)
 * and returns a tls context and socket fd, or NULL on error.
 */
struct tls *
connect_batmud(int *fd)
{
	struct tls *ctx = NULL;
	const char *bc_magic = "\033bc 1\n";
	const char *hostname = "batmud.bat.org";
	char *servname = "2022";

	ctx = tls_client();
	int ret;
	struct addrinfo *result, *rp;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	ret = getaddrinfo(hostname, servname, &hints, &result);
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		*fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (*fd < 0)
			continue;
		ret = connect(*fd, rp->ai_addr, rp->ai_addrlen);
		if (ret != -1)
			break;
	}
	freeaddrinfo(result);

	if (!rp) {
		close(*fd);
		return NULL;
	}

	if (tls_connect_socket(ctx, *fd, hostname) < 0)
		errx(1, "tls_connect_socket: %s", tls_error(ctx));
	if (tls_handshake(ctx) < 0)
		errx(1, "tls_handshake: %s", tls_error(ctx));

	(void) tls_sendall(ctx, *fd, bc_magic, strlen(bc_magic));
	return ctx;
}
