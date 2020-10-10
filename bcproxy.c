#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client_parser.h"
#include "config.h"
#include "db.h"
#include "net.h"
#include "parser.h"
#include "postgres.h"
#include "proxy.h"
#include "room.h"

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
			warn("bindall: socket");
			continue;
		}
		int one = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
		if (bind(sock, rp->ai_addr, rp->ai_addrlen) < 0)
			err(1, "bind");
		fprintf(stderr, "success\n");
		goto cleanup;
	}

cleanup:
	if (result)
		freeaddrinfo(result);
	return sock;
}
#define BUFSZ (64*1024)

static int
handle_connection(int client, int dumpfd, struct bc_parser *parser)
{
	char *ibuf, *convbuf;
	int status = -1;
	int server = -1;
	struct tls *ctx;
	struct proxy_state *st = parser->data;
	ssize_t recvd, sent, bytes_to_send;

	ibuf = malloc(BUFSZ);
	convbuf = malloc(BUFSZ);
	if (!ibuf || !convbuf)
		errx(1, "failed to allocate buffers");

	if ((ctx = connect_batmud(&server)) == NULL)
		errx(1, "failed to connect");
	warnx("connected to batmud");

	/*
	 * Set the fd's nonblocking by default and use blocking mode only when
	 * writing.
	 * Also set TCP_NODELAY in both directions; MUD traffic should be sent
	 * as soon as possible, and in the pathological case, if the receiver
	 * is using delayed ACKs, enabling Nagle's algorithm can cause large
	 * delays (eg. OpenBSD delayed ACK uses 200ms).
	 */
	int fds[] = { client, server, -1 };
	for (int *fd = fds; *fd != -1; fd++) {
		int one = 1;
		int flags = fcntl(*fd, F_GETFL, 0);
		if (flags == -1)
			err(1, "fcntl F_GETFL");
		if (fcntl(*fd, F_SETFL, flags | O_NONBLOCK) < 0)
			err(1, "fcntl F_SETFL");
		if (setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, &one,
		    sizeof(one)) == -1)
			err(1, "setsockopt TCP_NODELAY");
	}

	for(;;) {
		struct pollfd pfd[2];
		int nready;
		int from, to;

		pfd[0].fd = server;
		pfd[0].events = POLLIN;
		pfd[1].fd = client;
		pfd[1].events = POLLIN;

		nready = poll(pfd, 2, -1);
		if (nready == -1)
			err(1, "poll");
		if (pfd[0].revents & (POLLERR|POLLNVAL))
			errx(1, "bad server fd %d", pfd[0].fd);
		if (pfd[1].revents & (POLLERR|POLLNVAL))
			errx(1, "bad client fd %d", pfd[1].fd);
		if (pfd[0].revents & POLLIN) {
			from = server;
			to = client;
			recvd = tls_read(ctx, ibuf, BUFSZ);
			if (recvd == TLS_WANT_POLLIN ||
			    recvd == TLS_WANT_POLLOUT)
				continue;
			if (recvd == -1)
				warnx("tls_read: %s", tls_error(ctx));
		} else if (pfd[1].revents & POLLIN) {
			from = client;
			to = server;
			recvd = recv(from, ibuf, BUFSZ, 0);
			if (recvd == -1)
				warn("recv");
		}

		if (recvd == -1)
			goto out;

		if (pfd[0].revents & POLLHUP || recvd == 0) {
			warnx("server disconnect");
			status = 0;
			goto out;
		}
		if (pfd[1].revents & POLLHUP || recvd == 0) {
			warnx("client disconnect");
			status = 0;
			goto out;
		}

		if (from == client) {
			bytes_to_send = client_utf8_to_iso8859_1(convbuf, ibuf,
			    recvd);
			assert(bytes_to_send <= recvd);
			sent = tls_sendall(ctx, to, convbuf, bytes_to_send);
		} else {
			size_t len = recvd;
			const char *buf = ibuf;
			while (dumpfd >= 0 && len > 0) {
				ssize_t nw;
				if ((nw = write(dumpfd, buf, len)) < 0)
					err(1, "writing dumpfile");
				buf += nw;
				len -= nw;
			}
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

out:
	shutdown(server, SHUT_RDWR);
	shutdown(client, SHUT_RDWR);
	close(server);
	close(client);
	free(ibuf);
	free(convbuf);
	return status;
}

static int
test_parser(size_t bufsz, struct bc_parser *parser)
{
	char *buf;
	ssize_t n;
	struct proxy_state *st = parser->data;
	buf = malloc(bufsz);
	if (!buf)
		err(1, "test_parser: malloc");
	while ((n = read(STDIN_FILENO, buf, bufsz)) > 0) {
		bc_parse(parser, buf, n);
		write(STDOUT_FILENO, st->obuf->data, st->obuf->len);
		buffer_clear(st->obuf);
	}
	free(buf);
	return 0;
}

static void
usage(void)
{
	errx(1, "usage: bcproxy [-w file] listening_port");
}

extern char *optarg;
extern int optind;

struct db postgres_db = {
	.dbp_init = postgres_init,
	.dbp_free = postgres_free,
	.add_room = postgres_add_room,
	.add_exit = postgres_add_exit,
};

int
main(int argc, char **argv)
{
	int exit_status = 1;
	int listenfd = -1;
	int conn = -1;
	int dumpfd = -1;
	struct db *db;
	int testmode = 0;

	struct bc_parser parser = {
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
		testmode = 1;
		db = &(struct db) {
			.dbp = NULL,
			.dbp_init = NULL,
			.dbp_free = NULL,
			.add_room = NULL,
			.add_exit = NULL,
		};
	} else
		db = &postgres_db;

	db_init(db);
	parser.data = proxy_state_new(BUFSZ, db);
	if (!parser.data)
		errx(1, "failed to initialize proxy_state");

	if (testmode)
		return test_parser(BUFSZ, &parser);

	int ch;
	while ((ch = getopt(argc, argv, "w:")) != -1) {
		switch (ch) {
		case 'w':
			if ((dumpfd = open(optarg, O_WRONLY|O_CREAT, 0644)) < 0)
				err(1, "%s", optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/* send() may cause SIGPIPE so ignore that */
	sigaction(SIGPIPE,
	    &(const struct sigaction) { .sa_handler = SIG_IGN, .sa_flags = SA_RESTART },
	    NULL);

	listenfd = bindall(argv[0]);
	if (listenfd < 0)
		goto exit;
	if (listen(listenfd, 0) == -1)
		err(1, "listen");

retry_accept:
	conn = accept(listenfd, NULL, NULL);
	if (conn == -1) {
		if (errno == EINTR)
			goto retry_accept;
		err(1, "accept");
	}

	exit_status = handle_connection(conn, dumpfd, &parser) == 0 ? 0 : 1;
	proxy_state_free((struct proxy_state *)parser.data);
	db_free(db);
exit:
	if (conn != -1)
		close(conn);
	if (dumpfd != -1)
		close(dumpfd);
	if (listenfd != -1)
		close(listenfd);
	return exit_status;
}
