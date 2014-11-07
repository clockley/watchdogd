#ifndef WATCHDOGD_H
#define WATCHDOGD_H
#define _XOPEN_SOURCE 700
#define _ISOC11_SOURCE
#define _FILE_OFFSET_BITS 64

#include <assert.h>
#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <libconfig.h>
#include <oping.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

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

#if defined __cplusplus
#define restrict
#endif

struct pidfile {
	const char *name;
	int fd;
};

typedef struct pidfile pidfile_t;

//TODO: Split this struct into an options struct(values read in from config file) and a runtime struct.
struct cfgoptions {
	config_t cfg;
	double maxLoadFifteen;
	double maxLoadOne;
	double maxLoadFive;
	double retryLimit;
	const config_setting_t *ipAddresses;
	pingobj_t *pingObj;
	const config_setting_t *pidFiles;
	const char *devicepath;
	const char *testexepath;
	const char *exepathname;
	const char *testexepathname;
	const char *confile;
	const char *logdir;
	const char *randomSeedPath;
	const char *logTarget;
	pidfile_t pidfile;
	time_t sleeptime;
	int sigtermDelay;
	unsigned long minfreepages;

	unsigned int options;
#define SOFTBOOT 0x1
#define SYNC 0x2
#define USEPIDFILE 0x4
#define ENABLEPING 0x8
#define KEXEC 0x10
#define DAEMONIZE 0x20
#define ENABLEPIDCHECKER 0x40
#define FORCE 0x80
#define NOACTION 0x100
#define REALTIME 0x200

	int priority;
	int watchdogTimeout;
	int testExeReturnValue;
	int testBinTimeout;
	int repairBinTimeout;
#ifndef __cplusplus
	volatile _Atomic(unsigned int) error;
#else
	std::atomic<unsigned int> error;
#endif
#define SCRIPTFAILED 0x1
#define FORKFAILED 0x2
#define OUTOFMEMORY 0x4
#define LOADAVGTOOHIGH 0x8
#define UNKNOWNPIDFILERROR 0x10
#define PIDFILERROR 0x20
#define PINGFAILED 0x40
};

struct ProcessList {
	struct list head;
};

typedef struct ProcessList ProcessList;

struct spawnattr {
	char *workingDirectory;
	const char *repairFilePathname;
	char *execStart;
	const char *logDirectory;
	char *user;
	char *group;
	char *umask;
	int timeout;
	int nice;
	bool noNewPrivileges;
};

typedef struct spawnattr spawnattr_t;

struct repaircmd_t {
	struct list entry;
	const char *path;
	spawnattr_t spawnattr;
	int ret;
	bool legacy;
};

typedef struct repaircmd_t repaircmd_t;

struct watchdogDevice {
	const char *path;
	int fd;
	int timeout;
};

typedef struct watchdogDevice watchdog_t;

extern ProcessList processes;

enum logTarget_t {
	INVALID_LOG_TARGET,
	STANDARD_ERROR,
	SYSTEM_LOG,
	FILE_APPEND,
	FILE_NEW,
};

#include "linux.h"

#endif
