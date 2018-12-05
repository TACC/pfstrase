AC_INIT([pfstrased], [0.0.1], [rtevans@tacc.utexas.edu])
AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability])
AC_PROG_CC
AM_PROG_CC_C_O

AC_SEARCH_LIBS([ev_run], [ev], [libev=yes], [AC_MSG_ERROR([Failed to find lib libev.so])])
AC_CHECK_HEADERS([ev.h], [ev=true], [AC_MSG_ERROR([Failed to find header ev.h])])

AC_SEARCH_LIBS([amqp_new_connection], [rabbitmq], [librabbitmq=yes], [librabbitmq=no])
AC_CHECK_HEADERS([amqp.h], [amqp=true], [AC_MSG_ERROR([Failed to find header amqp.h])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
