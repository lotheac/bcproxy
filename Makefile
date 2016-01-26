PROG=		bcproxy
SRCS=		bcproxy.c buffer.c color.c db.c parser.c proxy.c room.c
LDADD!=		pkg-config --libs libpq
COPTS!=		pkg-config --cflags libpq
COPTS+=		-std=c99 -Wall -Wextra -pedantic
LINKS=		${BINDIR}/${PROG} ${BINDIR}/test_parser
MAN=

.if !empty(HOST_OS) && exists(${.CURDIR}/${HOST_OS}.mk)
.include "${HOST_OS}.mk"
.endif
.include <bsd.prog.mk>
