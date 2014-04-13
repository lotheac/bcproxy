#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "bc_parser.h"

#define BATHOST "batmud.bat.org"
#define BATPORT "23"
#define BC_ENABLE "\033bc 1\n"

struct proxy_state {
    char    *obuf;
    char    *argbuf;
    size_t  olen;
    size_t  arglen;
    size_t  osz;
    size_t  argsz;
    int     ignore; /* Don't output anything if this is set */
};

/* Binds to loopback address using TCP and the provided servname, printing
 * diagnostics and errors on stderr. Returns a socket suitable for listening
 * on, or negative on error. */
static int bindall(const char *servname) {
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
        fprintf(stderr, "bindall: getaddrinfo: %s\n", gai_strerror(ret));
        goto cleanup;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        /* NI_MAXHOST / NI_MAXSERV are defined in RFC 2553 which is obsoleted
         * by RFC 3493; they are not available on illumos when conforming to
         * _XOPEN_SOURCE=600. For numeric host and serv these are enough */
        char host[INET6_ADDRSTRLEN], serv[6];
        fprintf(stderr, "bindall: binding to ");
        ret = getnameinfo(rp->ai_addr, rp->ai_addrlen, host, INET6_ADDRSTRLEN,
                          serv, 6, NI_NUMERICHOST | NI_NUMERICSERV);
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

/* Blocking call to send all data out on socket. Prints errors on stderr and
 * returns -1 on error, the number of bytes sent on success. */
ssize_t sendall(int fd, const void *buf, size_t len) {
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

/* Connects to BatMUD via TCP and enables batclient mode (by sending BC_ENABLE)
 * and returns a fd, or negative value on error. */
static int connect_batmud(void) {
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

    /* Don't send terminating null byte */
    (void) sendall(sock, BC_ENABLE, sizeof(BC_ENABLE) - 1);

    return sock;
}

#define BUFSZ 4096

static void on_close(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;
    st->ignore = 0;
}

static void on_arg(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    if (st->arglen + len > st->argsz) {
        char *newp = realloc(st->argbuf, st->arglen + BUFSZ);
        if (!newp) {
            perror("realloc arg buffer");
            return;
        }
        st->argbuf = newp;
        st->argsz += BUFSZ;
    }
    assert(st->arglen + len <= st->argsz);
    memcpy(st->argbuf + st->arglen, buf, len);
    st->arglen += len;
}

static void on_arg_end(struct bc_parser *parser) {
    struct proxy_state *st = parser->data;
    char *argstr = malloc(st->arglen + 1);
    memcpy(argstr, st->argbuf, st->arglen);
    argstr[st->arglen] = '\0';
    /* FIXME magic numbers */
    switch (parser->code) {
        case 10: /* Message with type */
            if (strcmp(argstr, "spec_prompt") == 0) {
                /* These are unwanted since they show up every second in
                 * addition to the normal prompt line */
                st->ignore = 1;
            }
            break;
        case 22: /* Message attributes */
        case 23:
        case 24:
        case 25:
            /* FIXME assert */
            assert(st->olen + st->arglen <= st->osz);
            memcpy(st->obuf + st->olen, st->argbuf, st->arglen);
            st->olen += st->arglen;
    }

    st->arglen = 0;
    free(argstr);
}

static void on_text(struct bc_parser *parser, const char *buf, size_t len) {
    struct proxy_state *st = parser->data;
    /* FIXME assert */
    assert(st->olen + len <= st->osz);
    memcpy(st->obuf + st->olen, buf, len);
    st->olen += len;
}

static int handle_connection(int client) {
    char ibuf[BUFSZ];
    int status = 1;
    int server = connect_batmud();
    if (server == -1)
        return -1;
    fprintf(stderr, "connected to batmud\n");

    fd_set rset;
    const int nfds = (server > client ? server : client) + 1;
    ssize_t recvd, sent, bytes_to_send;

    struct proxy_state st = { 0 };
    st.obuf = malloc(BUFSZ);
    if (!st.obuf) {
        perror("malloc");
        goto out;
    }
    st.argbuf = malloc(BUFSZ);
    if (!st.argbuf) {
        perror("malloc");
        goto out;
    }
    st.osz = BUFSZ;
    struct bc_parser parser = {
        .data = &st,
        .on_text = on_text,
        .on_arg = on_arg,
        .on_arg_end = on_arg_end,
        .on_close = on_close,
    };

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
            fprintf(stderr, "%s disconnect\n",
                    from == client ? "client" : "server");
            status = 0;
            goto out;
        }

        if (from == client) {
            /* We don't do anything for data coming from the client; just send
             * it along. */
            bytes_to_send = recvd;
            sent = sendall(to, ibuf, recvd);
        } else {
            bc_parse(&parser, ibuf, recvd);
            /* Callbacks will fill obuf and set st.len. */
            bytes_to_send = st.olen;
            sent = sendall(to, st.obuf, st.olen);
            st.olen = 0;
        }
        if (sent != bytes_to_send) {
            fprintf(stderr, "sent only %zd bytes out of %zd to %s\n",
                    sent,
                    bytes_to_send,
                    to == client ? "client" : "server");
            goto out;
        }
    }

    status = 0;
out:
    free(st.obuf);
    free(st.argbuf);
    (void) shutdown(server, SHUT_RDWR);
    (void) shutdown(client, SHUT_RDWR);
    return status;
}

int main(int argc, char **argv) {
    int exit_status = 1;
    int listenfd = -1;
    int conn = -1;

    if (argc != 2) {
        fprintf(stderr, "usage: bcproxy listening_port\n");
        return 1;
    }

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

    exit_status = handle_connection(conn);
exit:
    if (conn != -1)
        while (-1 == close(conn) && EINTR == errno);
    if (listenfd != -1)
        while (-1 == close(listenfd) && EINTR == errno);
    return exit_status;
}
