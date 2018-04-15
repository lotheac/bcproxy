#ifndef FAKETLS_H
#define FAKETLS_H

#define TLS_WANT_POLLIN -2
#define TLS_WANT_POLLOUT -3

struct tls {
	int fd;
};

#define tls_error(ctx) strerror(errno)
#define tls_handshake(ctx) 0
int tls_connect_socket(struct tls *, int, const char *);
struct tls *tls_client(void);
ssize_t tls_read(struct tls *, void *, size_t);
ssize_t tls_write(struct tls *, const void *, size_t);

#endif
