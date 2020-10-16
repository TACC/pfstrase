AC_INIT([pfstrased], [1.0.0], [rtevans@tacc.utexas.edu])
AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability])
AC_PROG_CC
AM_PROG_CC_C_O

AC_SEARCH_LIBS([ev_run], [ev], [libev=yes], [AC_MSG_ERROR([Failed to find lib libev])])
AC_CHECK_HEADERS([ev.h], [ev=true], [AC_MSG_ERROR([Failed to find header ev.h])])

AC_SEARCH_LIBS([json_object_new_object], [json-c], [libjson=yes], [libjson=no])
AC_CHECK_HEADERS([json/json.h], [json=true], [AC_MSG_ERROR([Failed to find header json.h])])

AC_ARG_ENABLE([infiniband],
[  --enable-infiniband	Enable Infiniband support (must be set to true or false)],
[case "${enableval}" in
  yes) AC_SEARCH_LIBS([mad_rpc_open_port], [ibmad], [have_ib=yes], 
    [AC_MSG_ERROR([Unable to find libibmad.so!])])
  if test "x${have_ib}" = xyes; then 
     AC_CHECK_HEADERS([infiniband/umad.h infiniband/mad.h], [infiniband=true],
     [AC_MSG_ERROR([Unable to find umad.h or mad.h on this system!])])  
  fi
  ;;
  no)  infiniband=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-infiniband]) ;;
esac],
[AC_SEARCH_LIBS([mad_rpc_open_port], [ibmad], [have_ib=yes],
  [AC_MSG_ERROR([Unable to find libibmad.so!])])
  if test "x${have_ib}" = xyes; then 
     AC_CHECK_HEADERS([infiniband/umad.h infiniband/mad.h], [infiniband=true], 
     [AC_MSG_ERROR([Unable to find umad.h or mad.h on this system!])])  
  fi
])
AM_CONDITIONAL([INFINIBAND], [test x$infiniband = xtrue])

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemduserunitdir],
     [AS_HELP_STRING([--with-systemduserunitdir=DIR], [Directory for systemd user service files])],,
     [with_systemduserunitdir=auto])
AS_IF([test "x$with_systemduserunitdir" = "xyes" -o "x$with_systemduserunitdir" = "xauto"], [
     def_systemduserunitdir=$($PKG_CONFIG --variable=systemduserunitdir systemd)

     AS_IF([test "x$def_systemduserunitdir" = "x"],
   [AS_IF([test "x$with_systemduserunitdir" = "xyes"],
    [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
    with_systemduserunitdir=no],
   [with_systemduserunitdir="$def_systemduserunitdir"])])
AS_IF([test "x$with_systemduserunitdir" != "xno"],
      [AC_SUBST([systemduserunitdir], [$with_systemduserunitdir])])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemduserunitdir" != "xno"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
