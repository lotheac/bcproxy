#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>

#define BATHOST "batmud.bat.org"
#define BATPORT "23"
#define BC_ENABLE "\033bc 1\n"

/* Binds to all localhost addresses using TCP and the provided servname,
 * printing diagnostics and errors on stderr. Returns a socket suitable for
 * listening on, or negative on error. */
static int bindall(const char *servname) {
    int sock = -1;
    int ret;
    struct addrinfo *result, *rp;
    struct addrinfo hints = {
        .ai_family = AF_INET6,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };
    result = NULL;
    ret = getaddrinfo("::1", servname, &hints, &result);
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

static int handle_connection(int client) {
    char buf[BUFSZ];
    int status = 1;
    int server = connect_batmud();
    if (server == -1)
        return -1;
    fprintf(stderr, "connected to batmud\n");

    fd_set rset;
    const int nfds = (server > client ? server : client) + 1;
    ssize_t recvd, sent;

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
        /* FIXME TESTING - handle server fd differently*/
        int from, to;
        if (FD_ISSET(server, &rset)) {
            from = server;
            to = client;
        } else {
            from = client;
            to = server;
        }

        if (1) {
            /* We don't do anything for data coming from the client; just send
             * it along. */
            recvd = recv(from, buf, BUFSZ, MSG_DONTWAIT);
            if (recvd == -1) {
                perror("handle_connection: client recv");
                goto out;
            } else if (recvd == 0) {
                fprintf(stderr, "client disconnect\n");
                status = 0;
                goto out;
            }
            sent = sendall(to, buf, recvd);
            if (sent != recvd) {
                fprintf(stderr, "sent only %zd bytes out of %zd from client "
                        "to server\n", sent, recvd);
                goto out;
            }
        }
    }

    status = 0;
out:
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
