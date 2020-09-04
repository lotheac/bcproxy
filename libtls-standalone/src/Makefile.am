AM_CFLAGS = -I$(top_srcdir)/include -include tls_compat.h

lib_LTLIBRARIES = libtls.la

libtls_la_LDFLAGS = -version-info @LIBTLS_VERSION@ -no-undefined
libtls_la_LIBADD = -lcrypto -lssl -lcrypto $(PLATFORM_LDADD)
libtls_la_LIBADD += $(top_builddir)/compat/libcompat.la
libtls_la_LIBADD += $(top_builddir)/compat/libcompatnoopt.la

libtls_la_SOURCES = tls.c
libtls_la_SOURCES += tls_bio_cb.c
libtls_la_SOURCES += tls_client.c
libtls_la_SOURCES += tls_compat.c
libtls_la_SOURCES += tls_config.c
libtls_la_SOURCES += tls_conninfo.c
libtls_la_SOURCES += tls_keypair.c
libtls_la_SOURCES += tls_ocsp.c
libtls_la_SOURCES += tls_peer.c
libtls_la_SOURCES += tls_server.c
libtls_la_SOURCES += tls_util.c
libtls_la_SOURCES += tls_verify.c
noinst_HEADERS = tls_internal.h
