PROG=		bcproxy
SRCS=		bcproxy.c buffer.c client_parser.c db.c net.c parser.c proxy.c room.c
LDADD!=		pkg-config --libs libpq
COPTS!=		pkg-config --cflags libpq
COPTS+=		-Wall -Wextra -pedantic
LINKS=		${BINDIR}/${PROG} ${BINDIR}/test_parser

.include "config.mk"
.include <bsd.prog.mk>
