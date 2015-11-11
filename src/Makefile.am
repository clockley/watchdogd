SUBDIRS = threadpool
AM_CFLAGS = $(DEPS_CFLAGS) $(PTHREAD_CFLAGS) $(MOUNT_CFLAGS) $(SYSTEMD_DAEMON_CFLAGS) $(SYSTEMD_JOURNAL_CFLAGS) -I$(top_srcdir)/src/threadpool $(ZLIB_CFLAGS)
watchdogd_LDADD = $(DEPS_LIBS) $(PTHREAD_LIBS) $(MOUNT_LIBS) $(CLOCK_GETTIME_LIBS) $(SYSTEMD_DAEMON_LIBS) $(SYSTEMD_JOURNAL_LIBS) $(top_builddir)/src/threadpool/libthreadpool.la $(ZLIB_LIBS)
sbin_PROGRAMS = watchdogd
watchdogd_SOURCES = main.c threads.c sub.c sub.h init.c daemon.c shutdown.c logutils.c logutils.h linux.c linux.h exe.c testdir.c list.h errorlist.h list.c main.h init.h watchdogd.h threads.h testdir.h exe.h pidfile.c pidfile.h configfile.c configfile.h daemon.h user.c user.h repair.c repair.h snprintf.c snprintf.h killtree.c killtree.h network_tester.c network_tester.h identify.c identify.h bootstatus.c bootstatus.h
dist_sbin_SCRIPTS = wd_identify.sh
