PROG=		bcproxy
SRCS=		bcproxy.c buffer.c client_parser.c color.c db.c net.c parser.c proxy.c room.c
LDADD!=		pkg-config --libs libpq
COPTS!=		pkg-config --cflags libpq
COPTS+=		-std=c99 -Wall -Wextra -pedantic
LINKS=		${BINDIR}/${PROG} ${BINDIR}/test_parser
MAN=

.include "config.mk"
.if !empty(HOST_OS)
.-include "${HOST_OS}.mk"
.endif
.include <bsd.prog.mk>
