bin_PROGRAMS = pfstrase_server

AM_DISTCHECK_CONFIGURE_FLAGS = \
  --with-systemduserunitdir=$$dc_install_base/$(systemduserunitdir)

if HAVE_SYSTEMD
systemduserunit_DATA = \
	pfstrase.service \
	pfstrase.conf
endif

EXTRA_DIST = pfstrase.service pfstrase.conf

pfstrase_server_CPPFLAGS = -D_GNU_SOURCE

pfstrase_server_SOURCES = 	 \
	daemonize.h	 \
	daemonize.c	 \
	pq.h \
	pq.c \
	pfstrase_server.c    \
	screen.h 	\
	screen.c 	\
	socket_server.h \
	socket_server.c \
	stats.h \
	stats.c

