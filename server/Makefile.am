bin_PROGRAMS = pfstrase_server pfstop

AM_DISTCHECK_CONFIGURE_FLAGS = \
  --with-systemduserunitdir=$$dc_install_base/$(systemduserunitdir)

if HAVE_SYSTEMD
systemduserunit_DATA = \
	pfstrase_server.service \
	pfstrase_server.conf
endif

EXTRA_DIST = pfstrase_server.service pfstrase_server.conf man/pfstop.1.gz man/pfstrase_server.1.gz map_nids.py qhost.py

pfstrase_server_CPPFLAGS = -D_GNU_SOURCE -O3

pfstrase_server_SOURCES = 	 \
	daemonize.h	 \
	daemonize.c	 \
	pfstrase_server.c    \
	shmmap.c \
	shmmap.h \
	socket_server.h \
	socket_server.c \
	stats.h \
	stats.c

if PSQL
pfstrase_server_SOURCES += \
	pq.h \
	pq.c
pfstrase_server_CPPFLAGS += -DPSQL
endif

pfstop_CPPFLAGS = -D_GNU_SOURCE

pfstop_SOURCES = 	 \
	screen.h 	\
	screen.c 	\
	shmmap.c \
	shmmap.h \
	stats.h \
	stats.c
