COPTS+=		-D__EXTENSIONS__ -m64
libpq_rpath!=	pkg-config --libs-only-L libpq | sed 's,-L,-R,g'
LDADD+=		${libpq_rpath} -m64 -lnsl -lsocket
