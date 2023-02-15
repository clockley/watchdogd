#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([watchdogd],[4.1.4],[clockley1@gmail.com])
: ${CXXFLAGS=$CXXFLAGS" -fdebug-prefix-map=$(pwd)=."}
AC_CONFIG_AUX_DIR([.build-aux])
AM_INIT_AUTOMAKE([1.15] [foreign subdir-objects tar-pax])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([.m4])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([src/config.h])

# Checks for programs.
AX_CXX_COMPILE_STDCXX_11([ext], [mandatory])
AX_PTHREAD

PKG_CHECK_MODULES([DEPENDENCIES], [zlib libconfig >= 1.4.8 libsystemd >= 233 liboping >= 1.10.0], [:], [bash install_dependencies.sh; PKG_CHECK_MODULES([DEPENDENCIES], [zlib libconfig >= 1.7.3 libsystemd >= 233 liboping >= 1.10.0])])

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

AC_ARG_WITH(dbusconfdir,
	AC_HELP_STRING([--with-dbusconfdir=PATH], [path to D-Bus config directory]),
	[path_dbusconf=${withval}],
	[
		if test "$prefix" = "`$PKG_CONFIG --variable=prefix dbus-1`"
		then path_dbusconf="`$PKG_CONFIG --variable=sysconfdir dbus-1`"
		fi
	]
)
if (test -z "${path_dbusconf}"); then
	if test "${prefix}" = "/usr/local" && test "${sysconfdir}" = '${prefix}/etc' && test -d /etc/dbus-1/system.d
	then DBUS_CONFDIR='/etc/dbus-1/system.d'
	else DBUS_CONFDIR='${sysconfdir}/dbus-1/system.d'
	fi
else
	[path_dbusconf="$(echo ${path_dbusconf} | sed 's:^'"${prefix}"':${prefix}:')" ; ]
	[path_dbusconf="$(echo ${path_dbusconf} | sed 's:^'"${sysconfdir}"':${sysconfdir}:')" ; ]
	DBUS_CONFDIR="${path_dbusconf}/dbus-1/system.d"
fi
AC_SUBST(DBUS_CONFDIR)
AC_ARG_WITH(dbusdatadir, AC_HELP_STRING([--with-dbusdatadir=PATH],
	[path to D-Bus data directory]), [path_dbusdata=${withval}],
	[
		if test "$prefix" = "`$PKG_CONFIG --variable=prefix dbus-1`"
		then path_dbusdata="`$PKG_CONFIG --variable=datadir dbus-1`"
		fi
	]
)
if (test -z "${path_dbusdata}"); then
		DBUS_DATADIR='${datadir}/dbus-1/system-services'
else
	[path_dbusconf="$(echo ${path_dbusdata} | sed 's:^'"${prefix}"':${prefix}:')" ; ]
	[path_dbusconf="$(echo ${path_dbusdata} | sed 's:^'"${datadir}"':${datadir}:')" ; ]
	DBUS_DATADIR="${path_dbusdata}/dbus-1/system-services"
fi
AC_SUBST(DBUS_DATADIR)
AC_DEFINE(CODENAME, "Loofahs Pails Efferent", "release codename or phrase")
AC_CONFIG_FILES([Makefile
		 wd_identify
		 contrib/systemd/watchdogd.service])
AC_OUTPUT
