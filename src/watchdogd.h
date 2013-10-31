#ifndef WATCHDOGD_H
#define WATCHDOGD_H
#define _XOPEN_SOURCE 700
#define _ISOC11_SOURCE

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <iso646.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libconfig.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#if !defined(_NSIG)
#define _NSIG 0
#endif
#if 0
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
#endif

#include "list.h"

struct cfgoptions {
	config_t cfg;
	double maxLoadFifteen;
	double maxLoadOne;
	double maxLoadFive;
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
};

struct parent parent;

#ifdef __linux__
#include "linux.h"
#endif

#endif
