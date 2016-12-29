AM_CXXFLAGS = $(PTHREAD_CFLAGS) $(DEPENDENCIES_CFLAGS)
AM_CFLAGS = $(DEPENDENCIES_CFLAGS)
watchdogd_LDADD = $(DEPENDENCIES_LIBS) $(PTHREAD_LIBS)
sbin_PROGRAMS = watchdogd
watchdogd_SOURCES = src/main.cpp src/threads.cpp src/sub.cpp src/sub.hpp src/init.cpp src/shutdown.cpp src/logutils.cpp src/logutils.hpp src/linux.cpp src/linux.hpp src/exe.cpp src/testdir.cpp src/list.hpp src/errorlist.hpp src/list.cpp src/main.hpp src/init.hpp src/watchdogd.hpp src/threads.hpp src/testdir.hpp src/exe.hpp src/configfile.cpp src/configfile.hpp src/user.cpp src/user.hpp src/repair.cpp src/repair.hpp src/snprintf.cpp src/snprintf.hpp src/network_tester.cpp src/network_tester.hpp src/identify.cpp src/identify.hpp src/bootstatus.cpp src/bootstatus.hpp src/multicall.cpp src/multicall.hpp src/threadpool.cpp src/threadpool.hpp src/futex.cpp src/futex.hpp src/dbusapi.c src/dbusapi.h src/watchdog.cpp src/watchdog.hpp
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

