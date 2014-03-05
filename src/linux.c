#include "watchdogd.h"
#include "linux.h"
#include "sub.h"

#ifdef __linux__

int IsSwaparea(struct libmnt_fs *fs, void *unused);

int PingWatchdog(watchdog_t * watchdog)
{
	if (watchdog == NULL) {
		return -1;
	}

	if (ioctl(watchdog->fd, WDIOC_KEEPALIVE, &watchdog->fd) == 0) {
		return 0;
	}

	if (ioctl(watchdog->fd, WDIOC_KEEPALIVE, &watchdog->fd) == 0) {
		return 0;
	}

	Logmsg(LOG_ERR, "WDIOC_KEEPALIVE ioctl failed: %s", strerror(errno));

	return -1;
}

int CloseWatchdog(watchdog_t * watchdog)
{
	if (watchdog == NULL) {
		return -1;
	}

	int options = WDIOS_DISABLECARD;

	if (ioctl(watchdog->fd, WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_DISABLECARD ioctl failed: %s",
		       strerror(errno));
	}

	if (write(watchdog->fd, "V", strlen("V")) < 0) {
		Logmsg(LOG_CRIT, "write to watchdog device failed: %s",
		       strerror(errno));
		CloseWraper(&watchdog->fd);
		WatchdogDestroy(watchdog);
		Logmsg(LOG_CRIT, "unable to close watchdog device");
		return -1;
	} else {
		CloseWraper(&watchdog->fd);
		WatchdogDestroy(watchdog);
	}

	return 0;
}

static bool PrintWdtInfo(watchdog_t * wdt)
{
	struct watchdog_info watchDogInfo;

	if (ioctl(wdt->fd, WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", strerror(errno));
	} else {
		Logmsg(LOG_DEBUG, "Hardware watchdog '%s', version %lu",
		       watchDogInfo.identity, watchDogInfo.firmware_version);
		return true;
	}

	return false;
}

watchdog_t *OpenWatchdog(const char *path)
{

	if (path == NULL) {
		return NULL;
	}

	watchdog_t *watchdog = WatchdogConstruct();
	if (watchdog == NULL)
		return NULL;

	watchdog->fd = open(path, O_WRONLY | O_CLOEXEC);
	if (watchdog->fd == -1) {
		Logmsg(LOG_ERR,
		       "unable to open watchdog device: %s", strerror(errno));
		WatchdogDestroy(watchdog);
		return NULL;
	}

	if (PingWatchdog(watchdog) != 0) {
		free(watchdog);
		return NULL;
	}

	PrintWdtInfo(watchdog);
	return watchdog;
}

int ConfigureWatchdogTimeout(watchdog_t * watchdog, int timeout)
{
	struct watchdog_info watchDogInfo;

	assert(watchdog != NULL);

	if (watchdog == NULL) {
		return -1;
	}

	if (timeout <= 0)
		return 0;

	int options = WDIOS_DISABLECARD;
	if (ioctl(watchdog->fd, WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_DISABLECARD ioctl failed: %s",
		       strerror(errno));
		return -1;
	}

	if (ioctl(watchdog->fd, WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_CRIT, "WDIOC_GETSUPPORT ioctl failed: %s",
		       strerror(errno));
		return -1;
	}

	if (!(watchDogInfo.options & WDIOF_SETTIMEOUT)) {
		return -1;
	}

	int oldTimeout = timeout;

	if (ioctl(watchdog->fd, WDIOC_SETTIMEOUT, &timeout) < 0) {
		fprintf(stderr, "watchdogd: unable to set WDT timeout \n");
		return -1;
	}

	if (timeout != oldTimeout) {
		fprintf(stderr, "watchdogd: Actual WDT timeout: %i seconds\n",
			timeout);
		fprintf(stderr,
			"watchdogd: Timeout specified in the configuration file: %i\n",
			oldTimeout);
	}

	options = WDIOS_ENABLECARD;
	if (ioctl(watchdog->fd, WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_ENABLECARD ioctl failed: %s",
		       strerror(errno));
		return -1;
	}

	watchdog->timeout = timeout;

	return PingWatchdog(watchdog);
}

int ConfigureKernelOutOfMemoryKiller(void)
{
	int fd = 0;
	int dfd = 0;

	dfd = open("/proc/self", O_DIRECTORY | O_RDONLY);

	if (dfd == -1) {
		Logmsg(LOG_ERR, "open failed: %s", strerror(errno));
		return -1;
	}

	fd = openat(dfd, "oom_score_adj", O_WRONLY);

	if (fd == -1) {
		Logmsg(LOG_ERR, "open failed: %s", strerror(errno));
		CloseWraper(&fd);	//CloseWraper is totally wacko, I really should remove it.
		CloseWraper(&dfd);
		return -1;
	}

	if (write(fd, "-1000", strlen("-1000")) < 0) {
		Logmsg(LOG_ERR, "write failed: %s", strerror(errno));
		CloseWraper(&fd);
		CloseWraper(&dfd);
		return -1;
	}

	CloseWraper(&fd);
	CloseWraper(&dfd);

	return 0;
}

int RemountRootReadOnly(void)
{
	struct libmnt_context *context;
	context = mnt_new_context();

	if (mnt_context_set_target(context, "/") < 0) {
		return -1;
	}

	if (mnt_context_set_mflags(context, MS_RDONLY | MS_REMOUNT) < 0) {
		mnt_free_context(context);
		return -1;
	}

	if (mnt_context_mount(context) == 0) {
		mnt_free_context(context);
		return 0;
	}

	if (mnt_context_helper_executed(context) == 1) {
		if (mnt_context_get_helper_status(context) == 0) {
			mnt_free_context(context);
			return 0;
		} else {
			mnt_free_context(context);
			return -1;
		}
	}

	if (mnt_context_syscall_called(context) == 1) {
		if (mnt_context_get_syscall_errno(context) != 0) {
			mnt_free_context(context);
			return -1;
		}
	}

	mnt_free_context(context);

	return -1;
}

int UnmountAll(void)
{
	struct libmnt_context *context;
	struct libmnt_iter *iter;
	struct libmnt_fs *currentfs;

	iter = mnt_new_iter(MNT_ITER_BACKWARD);
	context = mnt_new_context();

	if (iter == NULL) {
		return -1;
	}

	int returnValue = 0;
	int ret = 0;

	while (mnt_context_next_umount
	       (context, iter, &currentfs, &returnValue, &ret) == 0) ;

	//mnt_context_finalize_umount(context);

	mnt_free_context(context);

	return 0;
}

int IsSwaparea(struct libmnt_fs *fs, void *unused)
{
	assert(fs != NULL);

	(void)unused;
	return mnt_fs_is_swaparea(fs);
}

int DisablePageFiles(void)
{
	struct libmnt_iter *iterator = NULL;
	struct libmnt_fs *nextfs = NULL;

	struct libmnt_table *tableOfPageFiles = mnt_new_table();
	iterator = mnt_new_iter(MNT_ITER_BACKWARD);
	nextfs = mnt_new_fs();

	if (tableOfPageFiles == NULL || nextfs == NULL || iterator == NULL) {
		return -1;
	}

	if (mnt_table_parse_swaps(tableOfPageFiles, NULL) < 0) {
		return -1;
	}

	int ret = 0;

	while ((ret =
		mnt_table_find_next_fs(tableOfPageFiles, iterator, IsSwaparea,
				       NULL, &nextfs)) == 0) {

		char const *swapPath = mnt_fs_get_srcpath(nextfs);

		if (swapPath == NULL) {
			/*print error message and ... */
			continue;
		}

		if (swapoff(swapPath) < 0)
			continue;
	}

	mnt_free_iter(iterator);
	mnt_free_table(tableOfPageFiles);

	if (ret < 0)
		return -1;

	return 0;
}

int _Shutdown(int errorcode, bool kexec)
{
	if (errorcode == WETEMP) {
		sync();
		errno = 0;
		if (reboot(LINUX_REBOOT_CMD_POWER_OFF) == -1) {
			if (errno == EPERM) {
				return -1;
			} else {
				abort();
			}
		}
	}

	if (kexec == true) {
		sync();
		if (reboot(LINUX_REBOOT_CMD_KEXEC) == -1) {
			if (errno == EPERM) {
				return -1;
			}
		}
	}

	sync();
	if (reboot(LINUX_REBOOT_CMD_RESTART) == -1) {
		if (errno == EPERM) {
			return -1;
		} else {
			abort();
		}
	}

	return -1;
}

void WriteUserAccountingDatabaseRecord(int reboot)
{
	struct utmpx utmpxStruct;

	strncpy(utmpxStruct.ut_user, reboot == 1 ? "reboot" : "shutdown",
		sizeof(utmpxStruct.ut_user));
	strncpy(utmpxStruct.ut_line, "~", sizeof(utmpxStruct.ut_user));
	strncpy(utmpxStruct.ut_id, "~~", sizeof(utmpxStruct.ut_id));
	utmpxStruct.ut_pid = 0;
	utmpxStruct.ut_type = RUN_LVL;
	gettimeofday(&utmpxStruct.ut_tv, NULL);

	setutxent();

	pututxline(&utmpxStruct);

	endutxent();
}
#endif
