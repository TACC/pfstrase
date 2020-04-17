bin_PROGRAMS = pfstrase_client pfstrase_server

AM_DISTCHECK_CONFIGURE_FLAGS = \
  --with-systemduserunitdir=$$dc_install_base/$(systemduserunitdir)

if HAVE_SYSTEMD
systemduserunit_DATA = \
	pfstrase.service \
	pfstrase.conf
endif

pfstrase_client_CPPFLAGS = -D_GNU_SOURCE
pfstrase_client_SOURCES = 	 \
	cpu.h 		 \
	cpu.c 		 \
	daemonize.h	 \
	daemonize.c	 \
	collect.h 	\
	collect.c	\
	exports.h	\
	exports.c	\
	lfs_utils.h	\
	lfs_utils.c	\
	llite.h		\
	llite.c		\
	osc.h 		\
	osc.c		\
	pfstrase_client.c \
	sysinfo.h	\
	sysinfo.c	\
	socket_utils.h \
	socket_utils.c \
	stats.h \
	stats.c \
        target.h \
	target.c	

EXTRA_DIST = pfstrase.service pfstrase.conf pfstrase_initd.sh

pfstrase_server_CPPFLAGS = -D_GNU_SOURCE

pfstrase_server_SOURCES = 	 \
	cpu.h 		 \
	cpu.c 		 \
	daemonize.h	 \
	daemonize.c	 \
	collect.h 	\
	collect.c	\
	exports.h	\
	exports.c	\
	lfs_utils.h	\
	lfs_utils.c	\
	llite.h		\
	llite.c		\
	osc.h 		\
	osc.c		\
	pfstrase_server.c    \
	screen.h 	\
	screen.c 	\
	socket_utils.h \
	socket_utils.c \
	stats.h \
	stats.c \
	sysinfo.h	\
	sysinfo.c	\
        target.h \
	target.c	


