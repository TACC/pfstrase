bin_PROGRAMS = pfstrase_client

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
	cpuid.h 		 \
	cpuid.c 		 \
	daemonize.h	 \
	daemonize.c	 \
	collect.h 	\
	collect.c	\
	exports.h	\
	exports.c	\
	intel_pmc3.h    \
	intel_pmc3.c    \
	intel_skx_imc.h    \
	intel_skx_imc.c    \
	lfs_utils.h	\
	lfs_utils.c	\
	llite.h		\
	llite.c		\
	osc.h 		\
	osc.c		\
	pci.h \
	pci.c \
	pfstrase_client.c \
	sysinfo.h	\
	sysinfo.c	\
	socket_client.h \
	socket_client.c \
        target.h \
	target.c	

if INFINIBAND
pfstrase_client_SOURCES	+= \
	ib_sw.c
endif

EXTRA_DIST = pfstrase.service pfstrase.conf pfstrase_initd.sh
