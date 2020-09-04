PROG=		bcproxy
SRCS=		bcproxy.c buffer.c client_parser.c db.c net.c parser.c postgres.c proxy.c room.c
LDADD!=		pkg-config --libs libpq
COPTS!=		pkg-config --cflags libpq
NOGCCERROR?=	# apparently some old mk-files set -Werror if this is unset
WARNINGS=	yes
LINKS=		${BINDIR}/${PROG} ${BINDIR}/test_parser
# required for asprintf on glibc
COPTS+=		-D_GNU_SOURCE

.include "conf.mk"
.ifndef HAVE_LIBTLS
LIBTLS:=	${.CURDIR}/libtls-standalone/src/.libs/libtls.a
COPTS+=		-I${.CURDIR}/libtls-standalone/include
LDADD+=		${LIBTLS} -lssl -lcrypto -lpthread
${PROG}: ${LIBTLS}
${LIBTLS}:
	cd ${.CURDIR}/libtls-standalone && autoreconf -i && ./configure && ${.MAKE}
.endif
.include <bsd.prog.mk>
