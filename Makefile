PROG=		bcproxy
SRCS=		bcproxy.c buffer.c client_parser.c db.c net.c parser.c proxy.c room.c
NOGCCERROR?=	# apparently some old mk-files set -Werror if this is unset
WARNINGS=	yes
LINKS=		${BINDIR}/${PROG} ${BINDIR}/test_parser
# required for asprintf on glibc
COPTS+=		-D_GNU_SOURCE
COPTS+=		-I${.OBJDIR}

net.o: version.h
version.h: .git
	echo '#define PROXY_VERSION "'$$(git describe --dirty)'"' > $@

.include "conf.mk"
.ifndef HAVE_LIBTLS

LIBRESSL:=		libressl-3.3.1
LIBRESSL_SHA256:=	a6d331865e0164a13ac85a228e52517f7cf8f8488f2f95f34e7857302f97cfdb
LIBRESSL_URL:=		https://cdn.openbsd.org/pub/OpenBSD/LibreSSL/${LIBRESSL}.tar.gz

LIBSSL:=	${.CURDIR}/${LIBRESSL}/ssl/.libs/libssl.a
LIBCRYPTO:=	${.CURDIR}/${LIBRESSL}/crypto/.libs/libcrypto.a
LIBTLS:=	${.CURDIR}/${LIBRESSL}/tls/.libs/libtls.a
COPTS+=		-I${.CURDIR}/${LIBRESSL}/include
LDADD+=		${LIBSSL} ${LIBCRYPTO} ${LIBTLS} -lpthread

${SRCS}: ${LIBTLS}
${PROG}: ${LIBSSL} ${LIBCRYPTO} ${LIBTLS}

# this is mostly linux-specific -- my non-linux machines already have
# libressl/libtls, so I can't be bothered to make this portable
${.CURDIR}/${LIBRESSL}/configure: ${.CURDIR}/${LIBRESSL}
${.CURDIR}/${LIBRESSL}: ${LIBRESSL}.tar.gz
	tar -xC ${.CURDIR} -zf ${LIBRESSL}.tar.gz
${LIBRESSL}.tar.gz:
	@echo "downloading ${LIBRESSL_URL}..."
	wget -nv -O libressl.tar.gz ${LIBRESSL_URL} || curl -o libressl.tar.gz ${LIBRESSL_URL}
	echo ${LIBRESSL_SHA256} libressl.tar.gz | sha256sum -c
	mv libressl.tar.gz $@

# --with-openssldir is a dirty hack to find a cert bundle at runtime. libressl
# assumes "cert.pem" under openssldir, but eg. Debian's cert bundles instead
# live in /etc/ssl/certs/ca-certificates.crt, so while this hack means that
# relocating the source directory requires rebuilding libressl, this is still
# easier than finding the system cert bundle and patching libtls with its
# location.
${LIBCRYPTO}: ${.CURDIR}/${LIBRESSL}/configure
	cd ${.CURDIR}/${LIBRESSL} && ./configure --with-openssldir=${.CURDIR}/${LIBRESSL}/apps/openssl && ${.MAKE}
${LIBSSL}: ${LIBCRYPTO}
${LIBTLS}: ${LIBCRYPTO}
.endif # !HAVE_LIBTLS

.include <bsd.prog.mk>
