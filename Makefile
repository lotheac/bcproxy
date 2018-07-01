PROG=		bcproxy
SRCS=		bcproxy.c buffer.c client_parser.c db.c net.c parser.c proxy.c room.c
LDADD!=		pkg-config --libs libpq
COPTS!=		pkg-config --cflags libpq
NOGCCERROR?=	# apparently some old mk-files set -Werror if this is unset
WARNINGS=	yes
LINKS=		${BINDIR}/${PROG} ${BINDIR}/test_parser

.include "conf.mk"
.include <bsd.prog.mk>
