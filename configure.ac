#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIR([m4])
AC_INIT([watchdogd],[0.7.1],[clockley1@gmail.com])
AM_INIT_AUTOMAKE([1.7] [foreign dist-xz dist-zip])

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC_STDC

#AC_CHECK_PROGS([M4], gm4 m4, no)
#AS_IF([test "x$M4" = "xno"], AC_MSG_ERROR([m4 missing]))

# Checks for libraries.
AM_PROG_LIBTOOL
PKG_CHECK_MODULES([MOUNT], [mount >= 2.22.0])
PKG_CHECK_MODULES([ZLIB], [zlib])
PKG_CHECK_MODULES([DEPS], [libconfig >= 1.4.8])
PKG_CHECK_MODULES([SYSTEMD], [libsystemd])
AC_DEFINE([HAVE_SD_JOURNAL], 1, [Define if sd_journal_sendv is available.])
AC_DEFINE([HAVE_SD_NOTIFY], 1, [Define if sd_notify is available.])
AC_CHECK_LIB([oping],[ping_construct])
AC_CHECK_HEADERS([oping.h])

AC_SEARCH_LIBS(clock_gettime, rt, [CLOCK_GETTIME_LIBS=$LIBS])
AC_SUBST(CLOCK_GETTIME_LIBS)

RRA_WITH_SYSTEMD_UNITDIR

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
     AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),,
     [with_systemdsystemunitdir=auto])
AS_IF([test "x$with_systemdsystemunitdir" = "xyes" -o "x$with_systemdsystemunitdir" = "xauto"], [
     def_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)

     AS_IF([test "x$def_systemdsystemunitdir" = "x"],
         [AS_IF([test "x$with_systemdsystemunitdir" = "xyes"],
                [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
          with_systemdsystemunitdir=no],
         [with_systemdsystemunitdir=$def_systemdsystemunitdir])])
AS_IF([test "x$with_systemdsystemunitdir" != "xno"],
      [AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])])
AM_CONDITIONAL(HAVE_SYSTEMD, [test "x$with_systemdsystemunitdir" != "xno"])
AX_PTHREAD

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK

AC_CHECK_FUNCS([clock_gettime strerror strerror_r strtol fpathconf dirfd open openat fstatat vsnprintf snprintf sprintf getopt_long])

AC_CONFIG_FILES([Makefile
                 src/Makefile
		 src/threadpool/Makefile
		 conf/Makefile
		 contrib/Makefile
		 contrib/systemd/Makefile
                 man/Makefile])
AC_OUTPUT
