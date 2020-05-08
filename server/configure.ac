AC_INIT([pfstrase_server], [0.0.1], [rtevans@tacc.utexas.edu])
AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability])
AC_PROG_CC
AM_PROG_CC_C_O

AC_SEARCH_LIBS([ev_run], [ev], [libev=yes], [AC_MSG_ERROR([Failed to find lib libev.so])])
AC_CHECK_HEADERS([ev.h], [ev=true], [AC_MSG_ERROR([Failed to find header ev.h])])

AC_SEARCH_LIBS([json_object_new_object], [json-c], [libjson=yes], [libjson=no])
AC_CHECK_HEADERS([json/json.h], [json=true], [AC_MSG_ERROR([Failed to find header json.h])])

AC_SEARCH_LIBS([initscr], [ncurses], [libncurses=yes], [libncurses=no])
AC_CHECK_HEADERS([ncurses.h], [ncurses=true], [AC_MSG_ERROR([Failed to find header ncurses.h])])

AC_SEARCH_LIBS([PQconnectdb], [pq], [libpq=yes], [libpq=no])
AC_CHECK_HEADERS([libpq-fe.h], [pqfe=true], [AC_MSG_ERROR([Failed to find header libpq-fe.h])])

AC_SEARCH_LIBS([shm_open], [rt], [librt=yes], [librt=no])
AC_SEARCH_LIBS([sem_open], [pthread], [libpthread=yes], [libpthread=no])

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
