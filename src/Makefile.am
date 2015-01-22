SUBDIRS = threadpool
AM_CFLAGS = $(DEPS_CFLAGS) $(PTHREAD_CFLAGS) $(MOUNT_CFLAGS) $(SYSTEMD_DAEMON_CFLAGS) -I$(top_srcdir)/src/threadpool
watchdogd_LDADD = $(DEPS_LIBS) $(PTHREAD_LIBS) $(MOUNT_LIBS) $(CLOCK_GETTIME_LIBS) $(SYSTEMD_DAEMON_LIBS) $(top_builddir)/src/threadpool/libthreadpool.la
sbin_PROGRAMS = watchdogd
watchdogd_SOURCES = main.c threads.c sub.c sub.h init.c daemon.c shutdown.c logutils.c logutils.h linux.c linux.h exe.c testdir.c list.h errorlist.h list.c main.h init.h watchdogd.h threads.h testdir.h exe.h pidfile.c pidfile.h configfile.c configfile.h daemon.h user.c user.h repair.c repair.h myvsnprintf_ss.c mysnprintf_ss.c myvsnprintf_ss.h mysnprintf_ss.h $(top_srcdir)/src/threadpool/threadpool.h
