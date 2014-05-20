AM_CFLAGS = $(DEPS_CFLAGS) $(PTHREAD_CFLAGS) $(MOUNT_CFLAGS)
watchdogd_LDADD = $(DEPS_LIBS) $(PTHREAD_LIBS) $(MOUNT_LIBS) $(CLOCK_GETTIME_LIBS)
bin_PROGRAMS = watchdogd
watchdogd_SOURCES = main.c threads.c sub.c sub.h init.c daemon.c shutdown.c logutils.c linux.c linux.h exe.c testdir.c list.h errorlist.h list.c main.h init.h watchdogd.h threads.h testdir.h exe.h pidfile.c pidfile.h
