#ifndef NET_H
#define NET_H
#include "config.h"
#ifdef HAVE_LIBTLS
#include <tls.h>
#else
#include "faketls.h"
#endif

ssize_t tls_sendall(struct tls *, int, const char *, size_t);
ssize_t sendall(int, const char *, size_t);
struct tls *connect_batmud(int *);

#endif /* NET_H */
