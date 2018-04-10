#include <assert.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "parser.h"
#include "proxy.h"
#include "room.h"

#define BATHOST "batmud.bat.org"
#define BATPORT "23"
#define BC_ENABLE "\033bc 1\n"

/*
 * Binds to loopback address using TCP and the provided servname, printing
 * diagnostics and errors on stderr. Returns a socket suitable for listening
 * on, or negative on error.
 */
static int
bindall(const char *servname)
{
	int sock = -1;
	int ret;
	struct addrinfo *result, *rp;
	struct addrinfo hints = {
		.ai_family = AF_INET6,
		.ai_socktype = SOCK_STREAM,
		/* Without AI_PASSIVE we'll get loopback addresses */
	};
	result = NULL;
	ret = getaddrinfo(NULL, servname, &hints, &result);
	if (ret != 0) {
		warnx("bindall: getaddrinfo: %s", gai_strerror(ret));
		goto cleanup;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		char host[NI_MAXHOST], serv[NI_MAXSERV];
		fprintf(stderr, "bindall: binding to ");
		ret = getnameinfo(rp->ai_addr, rp->ai_addrlen, host,
		    NI_MAXHOST, serv, NI_MAXSERV, NI_NUMERICHOST |
		    NI_NUMERICSERV);
		if (ret != 0)
			fprintf(stderr, "<unknown> (getnameinfo: %s)",
			    gai_strerror(ret));
		else
			fprintf(stderr, "%s,%s", host, serv);
		fprintf(stderr, ": ");
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (-1 == sock) {
			perror("socket");
			continue;
		}
		int one = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
		if (0 == bind(sock, rp->ai_addr, rp->ai_addrlen)) {
			fprintf(stderr, "success\n");
			goto cleanup;
		} else {
			perror("bind");
			while (-1 == close(sock) && EINTR == errno);
			sock = -1;
		}
	}

cleanup:
	if (result)
		freeaddrinfo(result);
	return sock;
}

/*
 * Blocking call to send all data out on socket. Prints errors on stderr and
 * returns -1 on error, the number of bytes sent on success.
 */
ssize_t
sendall(int fd, const void *buf, size_t len)
{
	ssize_t sent = 0;
	ssize_t ret;
	while ((size_t) sent < len) {
		ret = send(fd, (char *)buf + sent, len, 0);
		if (-1 == ret) {
			if (EINTR == errno)
				continue;
			perror("sendall: send");
			return -1;
		} else
			sent += ret;
	}
	return sent;
}

/*
 * Connects to BatMUD via TCP and enables batclient mode (by sending BC_ENABLE)
 * and returns a fd, or negative value on error.
 */
static int
connect_batmud(void)
{
	int sock = -1;
	int ret;
	struct addrinfo *result, *rp;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	ret = getaddrinfo(BATHOST, BATPORT, &hints, &result);
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock == -1)
			continue;
		ret = connect(sock, rp->ai_addr, rp->ai_addrlen);
		if (ret != -1)
			break;
	}
	freeaddrinfo(result);

	if (!rp) {
		if (sock != -1)
			while (-1 == close(sock) && EINTR == errno);
		return -1;
	}

	(void) sendall(sock, BC_ENABLE, strlen(BC_ENABLE));

	return sock;
}

#define BUFSZ 2048

static int
handle_connection(int client, struct bc_parser *parser, struct proxy_state *st)
{
	char ibuf[BUFSZ];
	int status = -1;
	int server;

	fd_set rset;
	ssize_t recvd, sent, bytes_to_send;
	if ((server = connect_batmud()) < 0)
		errx(1, "failed to connect");
	warnx("connected to batmud");
	const int nfds = (server > client ? server : client) + 1;

	for(;;) {
		int nready;
		FD_ZERO(&rset);
		FD_SET(client, &rset);
		FD_SET(server, &rset);
retry_select:
		nready = select(nfds, &rset, NULL, NULL, NULL);
		if (nready < 0) {
			if (errno == EINTR)
				goto retry_select;
			perror("handle_connection: select");
			goto out;
		}
		int from, to;
		if (FD_ISSET(server, &rset)) {
			from = server;
			to = client;
		} else {
			from = client;
			to = server;
		}

		recvd = recv(from, ibuf, BUFSZ, MSG_DONTWAIT);
		if (recvd == -1) {
			perror("handle_connection: recv");
			goto out;
		} else if (recvd == 0) {
			warnx("%s disconnect",
			    from == client ? "client" : "server");
			status = 0;
			goto out;
		}

		if (from == client) {
			/* TODO convert utf8->iso8859-1, except for telnet
			 * command bytes client may send */
			bytes_to_send = recvd;
			sent = sendall(to, ibuf, bytes_to_send);
		} else {
			/* parser handles ISO-8859-1->UTF-8 conversion */
			bc_parse(parser, ibuf, recvd);
			bytes_to_send = st->obuf->len;
			sent = sendall(to, st->obuf->data, bytes_to_send);
			buffer_clear(st->obuf);
		}
		if (sent != bytes_to_send) {
			warnx("sent only %zd of %zd bytes to %s", sent,
			    bytes_to_send,
			    to == client ? "client" : "server");
			goto out;
		}
	}

	status = 0;
out:
	(void) shutdown(server, SHUT_RDWR);
	(void) shutdown(client, SHUT_RDWR);
	return status;
}

void
test_parser(size_t bufsz, struct bc_parser *parser, struct proxy_state *st)
{
	char *buf;
	ssize_t n;
	buf = malloc(bufsz);
	if (!buf)
		errx(1, "test_parser: malloc");
	while ((n = read(STDIN_FILENO, buf, bufsz)) > 0) {
		bc_parse(parser, buf, n);
		write(STDOUT_FILENO, st->obuf->data, st->obuf->len);
		buffer_clear(st->obuf);
	}
	free(buf);
}

int
main(int argc, char **argv)
{
	int exit_status = 1;
	int listenfd = -1;
	int conn = -1;

	struct proxy_state *st = proxy_state_new(BUFSZ);
	if (!st)
		errx(1, "failed to initialize proxy_state");

	struct bc_parser parser = {
		.data = st,
		.on_open = on_open,
		.on_text = on_text,
		.on_tag_text = on_tag_text,
		.on_arg_end = on_arg_end,
		.on_close = on_close,
		.on_prompt = on_prompt,
		.on_telnet_command = on_telnet_command,
	};

	if (!setlocale(LC_CTYPE, ""))
		err(1, "setlocale");

	if (strcmp("test_parser", getprogname()) == 0) {
		size_t bufsz;
		if (argc != 2 || sscanf(argv[1], "%zu", &bufsz) != 1 ||
		    bufsz == 0)
			errx(1, "usage: test_parser buffer_size");
		test_parser(bufsz, &parser, st);
		return 0;
	}

	if (argc != 2)
		errx(1, "usage: bcproxy listening_port");

	/* send() may cause SIGPIPE so ignore that */
	sigaction(SIGPIPE,
	    &(const struct sigaction) { .sa_handler = SIG_IGN },
	    NULL);

	listenfd = bindall(argv[1]);
	if (listenfd < 0)
		goto exit;
	if (listen(listenfd, 0) == -1) {
		perror("listen");
		goto exit;
	}

retry_accept:
	conn = accept(listenfd, NULL, NULL);
	if (conn == -1) {
		if (errno == EINTR)
			goto retry_accept;
		perror("accept");
		goto exit;
	}

	exit_status = handle_connection(conn, &parser, st) == 0 ? 0 : 1;
	proxy_state_free(st);
exit:
	if (conn != -1)
		while (-1 == close(conn) && EINTR == errno);
	if (listenfd != -1)
		while (-1 == close(listenfd) && EINTR == errno);
	return exit_status;
}
