#ifndef WATCHDOGD_H
#define WATCHDOGD_H

#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64
#define PREFER_PORTABLE_SNPRINTF
#define HAVE_SNPRINTF
#include <atomic>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <climits>
#include <clocale>
#include <config.h>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <libconfig.h>
#include <libgen.h>
#include <oping.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include "snprintf.hpp"
#include "watchdog.hpp"
#include "linux.hpp"
#include "watchdog.hpp"

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

#include "list.hpp"

#if defined __cplusplus
#define restrict
#endif

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#define SOFTBOOT 0x1
#define SYNC 0x2
#define RESERVED 0x4
#define ENABLEPING 0x8
#define KEXEC 0x10
#define DAEMONIZE 0x20
#define ENABLEPIDCHECKER 0x40
#define FORCE 0x80
#define NOACTION 0x100
#define REALTIME 0x200
#define VERBOSE 0x400
#define IDENTIFY 0x800
#define BUSYBOXDEVOPTCOMPAT 0x1000
#define LOGLVLSETCMDLN 0x2000

#define SCRIPTFAILED 0x1
#define FORKFAILED 0x2
#define OUTOFMEMORY 0x4
#define LOADAVGTOOHIGH 0x8
#define UNKNOWNPIDFILERROR 0x10
#define PIDFILERROR 0x20
#define PINGFAILED 0x40
#define NETWORKDOWN 0x80

//TODO: Split this struct into an options struct(values read in from config file) and a runtime struct.
struct cfgoptions {
	cfgoptions() {
		options |= DAEMONIZE;
	};
	config_t cfg = {0};
	double maxLoadFifteen = 0.0;
	double maxLoadOne = 0.0;
	double maxLoadFive = 0.0;
	double retryLimit = 0.0;
	long loopExit = -1;
	const config_setting_t *ipAddresses = NULL;
	const config_setting_t *networkInterfaces = NULL;
	pingobj_t *pingObj = NULL;
	const config_setting_t *pidFiles = NULL;
	const char *devicepath = NULL;
	const char *testexepath = "/etc/watchdog.d";
	const char *exepathname = NULL;
	const char *testexepathname = NULL;
	const char *confile = "/etc/watchdogd.conf";
	const char *randomSeedPath = NULL;
	const char *logTarget = NULL;
	const char *logUpto = NULL;
	time_t sleeptime = -1;
	int sigtermDelay = 0;
	unsigned long minfreepages = 0;
	unsigned long options = 0;
	int priority;
	int watchdogTimeout = -1;
	int testExeReturnValue = 0;
	int testBinTimeout = 60;
	int repairBinTimeout = 60;
	int allocatableMemory = 0;
	volatile std::atomic_uint error = {0};
	bool haveConfigFile = false;
};

struct ProcessList {
	struct list head;
};

typedef struct ProcessList ProcessList;

struct spawnattr_t {
	char *workingDirectory;
	const char *repairFilePathname;
	char *execStart;
	char *user;
	char *group;
	mode_t umask;
	int timeout;
	int nice;
	bool noNewPrivileges;
	bool hasUmask;
};

struct repaircmd_t {
	struct list entry;
	std::atomic_bool mode;
	char retString[32];
	const char *path;
	spawnattr_t spawnattr;
	int ret;
	bool legacy;
};

struct dbusinfo
{
	cfgoptions **config;
	Watchdog **wdt;
	pid_t childPid;
	int fd;
};

struct identinfo {
	char daemonVersion[8];
	char name[128];
	char deviceName[128];
	long unsigned flags;
	long timeout;
	long firmwareVersion;
};

struct dev {
	char name[64];
	unsigned long minor;
	unsigned long major;
};

extern ProcessList processes;

enum logTarget_t {
	INVALID_LOG_TARGET,
	STANDARD_ERROR,
	SYSTEM_LOG,
	FILE_APPEND,
	FILE_NEW,
};

#endif