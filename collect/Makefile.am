bin_PROGRAMS = pfstrased

AM_DISTCHECK_CONFIGURE_FLAGS = \
  --with-systemduserunitdir=$$dc_install_base/$(systemduserunitdir)

if HAVE_SYSTEMD
systemduserunit_DATA = \
	pfstrase.service \
	pfstrase.conf
endif

pfstrased_CPPFLAGS = -D_GNU_SOURCE

pfstrased_SOURCES = 	 \
	cpu.h 		 \
	cpu.c 		 \
	daemonize.h	 \
	daemonize.c	 \
	dict.h		\
	dict.c		\
	utils.h 	\
	utils.c		\
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
	sysinfo.h	\
	sysinfo.c	\
	socket_utils.h \
	socket_utils.c \
	serverd.c       \
        target.h \
	target.c	


EXTRA_DIST = pfstrase.service pfstrase.conf pfstrase_initd.sh
