AM_CFLAGS = $(DEPS_CFLAGS) $(PTHREAD_CFLAGS) $(MOUNT_CFLAGS) $(SYSTEMD_DAEMON_CFLAGS) $(SYSTEMD_JOURNAL_CFLAGS) $(ZLIB_CFLAGS)
watchdogd_LDADD = $(DEPS_LIBS) $(PTHREAD_LIBS) $(MOUNT_LIBS) $(CLOCK_GETTIME_LIBS) $(SYSTEMD_DAEMON_LIBS) $(SYSTEMD_JOURNAL_LIBS) $(ZLIB_LIBS)
sbin_PROGRAMS = watchdogd
watchdogd_SOURCES = src/main.c src/threads.c src/sub.c src/sub.h src/init.c src/daemon.c src/shutdown.c src/logutils.c src/logutils.h src/linux.c src/linux.h src/exe.c src/testdir.c src/list.h src/errorlist.h src/list.c src/main.h src/init.h src/watchdogd.h src/threads.h src/testdir.h src/exe.h src/pidfile.c src/pidfile.h src/configfile.c src/configfile.h src/daemon.h src/user.c src/user.h src/repair.c src/repair.h src/snprintf.c src/snprintf.h src/killtree.c src/killtree.h src/network_tester.c src/network_tester.h src/identify.c src/identify.h src/bootstatus.c src/bootstatus.h src/multicall.c src/multicall.h src/threadpool.c src/threadpool.h src/futex.c src/futex.h
dist_sbin_SCRIPTS = wd_identify
dist_man_MANS = man/watchdogd.8
EXTRA_DIST		= contrib/systemd/watchdogd.service conf/example.repair
if HAVE_SYSTEMD
systemdsystemunit_DATA =  contrib/systemd/watchdogd.service
endif
watchdogddir = $(sysconfdir)
dist_watchdogd_DATA = conf/watchdogd.conf
sysconf_DATA = conf/watchdogd.conf
ACLOCAL_AMFLAGS = -I m4
dist_doc_DATA = README LICENSE INSTALL MAN.TXT

DISTCHECK_CONFIGURE_FLAGS = \
                   --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

