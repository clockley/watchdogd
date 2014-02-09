#ifndef WATCHDOGD_H
#define WATCHDOGD_H
#define _XOPEN_SOURCE 700
#define _ISOC11_SOURCE

#include <assert.h>
#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <iso646.h>
#include <libconfig.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef NSIG
#if defined(_NSIG)
#define NSIG _NSIG		/* For BSD/SysV */
#elif defined(_SIGMAX)
#define NSIG (_SIGMAX + 1)	/* For QNX */
#elif defined(SIGMAX)
#define NSIG (SIGMAX + 1)	/* For djgpp */
#else
#define NSIG 64			/* Use a reasonable default value */
#endif
#endif

#include "list.h"

//TODO: Split this struct into an options struct(values read in from config file) and a runtime struct.
struct cfgoptions {
	config_t cfg;
	double maxLoadFifteen;
	double maxLoadOne;
	double maxLoadFive;
	double retryLimit;
	const config_setting_t *pidFiles;
	const char *devicepath;
	const char *testexepath;
	const char *exepathname;
	const char *testexepathname;
	const char *confile;
	const char *pidpathname;
	const char *logdir;
	time_t sleeptime;
	unsigned long minfreepages;

	unsigned int options;
#define SOFTBOOT 0x1
#define SYNC 0x2
#define USEPIDFILE 0x4
#define LOGTICK 0x8
#define KEXEC 0x10
#define DAEMONIZE 0x20
#define FOREGROUNDSETFROMCOMMANDLINE 0x40
#define ENABLEPIDCHECKER 0x80
#define FORCE 0x100
#define NOACTION 0x200
#define REALTIME 0x400

	int priority;
	int lockfd;
	int watchdogTimeout;
	int logtickinterval;
	int testExeReturnValue;
	int testBinTimeout;
	int repairBinTimeout;

	volatile unsigned int error;
#define SCRIPTFAILED 0x1
#define FORKFAILED 0x2
#define OUTOFMEMORY 0x4
#define LOADAVGTOOHIGH 0x8
#define UNKNOWNPIDFILERROR 0x10
#define PIDFILERROR 0x20
};

struct parent {
	struct list children;
};

struct child {
	struct list entry;
	const char *name;
	int ret;
};

struct watchdogDevice {
	const char *path;
	int fd;
	int timeout;
	void *tem;
};

typedef struct watchdogDevice watchdog_t;

extern struct parent parent;

struct listOfRunningProcess {
	struct list children;
};

struct process {
	struct list entry;
	const char *name;
	time_t starttime;
	time_t timeout;
	pid_t pid;
	int ret;
};

#ifdef __linux__
#include "linux.h"
#endif

#endif
