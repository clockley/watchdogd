AM_CXXFLAGS = $(PTHREAD_CFLAGS) $(DEPENDENCIES_CFLAGS)
AM_CFLAGS = $(DEPENDENCIES_CFLAGS)
watchdogd_LDADD = $(DEPENDENCIES_LIBS) $(PTHREAD_LIBS)
sbin_PROGRAMS = watchdogd
watchdogd_SOURCES = src/main.cpp src/threads.cpp src/sub.cpp src/sub.h src/init.cpp src/shutdown.cpp src/logutils.cpp src/logutils.h src/linux.cpp src/linux.h src/exe.cpp src/testdir.cpp src/list.h src/errorlist.h src/list.cpp src/main.h src/init.h src/watchdogd.h src/threads.h src/testdir.h src/exe.h src/configfile.cpp src/configfile.h src/user.cpp src/user.h src/repair.cpp src/repair.h src/snprintf.cpp src/snprintf.h src/network_tester.cpp src/network_tester.h src/identify.cpp src/identify.h src/bootstatus.cpp src/bootstatus.h src/multicall.cpp src/multicall.h src/threadpool.cpp src/threadpool.h src/futex.cpp src/futex.h src/dbusapi.c src/dbusapi.h src/watchdog.cpp src/watchdog.hpp
dist_sbin_SCRIPTS = wd_identify
dist_man_MANS = man/watchdogd.8
EXTRA_DIST		= contrib/systemd/watchdogd.service conf/example.repair install_dependencies.sh contrib/dbus/watchdogd.conf  wd_identify
systemdsystemunit_DATA =  contrib/systemd/watchdogd.service

dbusconfdir = $(DBUS_CONFDIR)
dbusconf_DATA = contrib/dbus/watchdogd.conf

watchdogddir = $(sysconfdir)
dist_watchdogd_DATA = conf/watchdogd.conf
sysconf_DATA = conf/watchdogd.conf
ACLOCAL_AMFLAGS = -I m4
dist_doc_DATA = README LICENSE INSTALL MAN.TXT

DISTCHECK_CONFIGURE_FLAGS = \
                   --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

