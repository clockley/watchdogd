/*
 * Copyright 2013-2014 Christian Lockley
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you may 
 * not use this file except in compliance with the License. You may obtain 
 * a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License. 
 */
#include "watchdogd.h"
#include "linux.h"
#include "sub.h"

#ifdef __linux__
int PingWatchdog(watchdog_t * const watchdog)
{
	if (watchdog == NULL) {
		return -1;
	}

	int tmp = 0;

	if (ioctl(GetFd(watchdog), WDIOC_KEEPALIVE, &tmp) == 0) {
		return 0;
	}

	if (ioctl(GetFd(watchdog), WDIOC_KEEPALIVE, &tmp) == 0) {
		return 0;
	}

	Logmsg(LOG_ERR, "WDIOC_KEEPALIVE ioctl failed: %s", strerror(errno));

	return -1;
}

int CloseWatchdog(watchdog_t * const watchdog)
{
	if (watchdog == NULL) {
		return -1;
	}

	int options = WDIOS_DISABLECARD;

	if (ioctl(GetFd(watchdog), WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_DISABLECARD ioctl failed: %s",
		       strerror(errno));
	}

	if (write(GetFd(watchdog), "V", strlen("V")) < 0) {
		Logmsg(LOG_CRIT, "write to watchdog device failed: %s",
		       strerror(errno));
		close(GetFd(watchdog));
		WatchdogDestroy(watchdog);
		Logmsg(LOG_CRIT, "unable to close watchdog device");
		return -1;
	} else {
		close(GetFd(watchdog));
		WatchdogDestroy(watchdog);
	}

	return 0;
}

static bool PrintWdtInfo(watchdog_t * const wdt)
{
	struct watchdog_info watchDogInfo;

	assert(wdt != NULL);

	if (ioctl(GetFd(wdt), WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", strerror(errno));
	} else {
		Logmsg(LOG_DEBUG, "Hardware watchdog '%s', version %lu",
		       watchDogInfo.identity, watchDogInfo.firmware_version);
		return true;
	}

	return false;
}

watchdog_t *OpenWatchdog(const char *const path)
{
	if (path == NULL) {
		return NULL;
	}

	watchdog_t *watchdog = WatchdogConstruct();
	if (watchdog == NULL)
		return NULL;

	SetFd(watchdog, open(path, O_WRONLY | O_CLOEXEC));
	if (GetFd(watchdog) == -1) {
		Logmsg(LOG_ERR,
		       "unable to open watchdog device: %s", strerror(errno));
		WatchdogDestroy(watchdog);
		return NULL;
	}

	if (PingWatchdog(watchdog) != 0) {
		WatchdogDestroy(watchdog);
		return NULL;
	}

	PrintWdtInfo(watchdog);
	return watchdog;
}

int GetOptimalPingInterval(watchdog_t * const watchdog)
{
	int timeout = 0;

	if (ioctl(GetFd(watchdog), WDIOC_GETTIMEOUT, &timeout) < 0) {
		return 1;
	}

	timeout /= 2;

	if (timeout < 1)
		return 1;

	return timeout;
}

static int DisableWatchdog(watchdog_t * const watchdog)
{
	assert(watchdog != NULL);

	if (watchdog == NULL) {
		return -1;
	}

	int options = WDIOS_DISABLECARD;
	if (ioctl(GetFd(watchdog), WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_DISABLECARD ioctl failed: %s",
		       strerror(errno));
		return -1;
	}

	return 0;
}

static int EnableWatchdog(watchdog_t * const watchdog)
{
	assert(watchdog != NULL);

	if (watchdog == NULL) {
		return -1;
	}

	int options = WDIOS_ENABLECARD;
	if (ioctl(GetFd(watchdog), WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_ENABLECARD ioctl failed: %s",
		       strerror(errno));
		return -1;
	}

	return 0;
}

int ConfigureWatchdogTimeout(watchdog_t * const watchdog, int timeout)
{
	struct watchdog_info watchDogInfo;

	assert(watchdog != NULL);

	if (watchdog == NULL) {
		return -1;
	}

	if (timeout <= 0)
		return 0;

	if (DisableWatchdog(watchdog) < 0) {
		return -1;
	}

	if (ioctl(GetFd(watchdog), WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_CRIT, "WDIOC_GETSUPPORT ioctl failed: %s",
		       strerror(errno));
		return -1;
	}

	if (!(watchDogInfo.options & WDIOF_SETTIMEOUT)) {
		return -1;
	}

	int oldTimeout = timeout;

	if (ioctl(GetFd(watchdog), WDIOC_SETTIMEOUT, &timeout) < 0) {
		int defaultTimeout = GetOptimalPingInterval(watchdog);

		fprintf(stderr, "watchdogd: unable to set WDT timeout\n");
		fprintf(stderr, "using default: %i", defaultTimeout);

		if (EnableWatchdog(watchdog) < 0) {
			return -1;
		}

		SetTimeout(watchdog, defaultTimeout);

		return PingWatchdog(watchdog);
	}

	if (timeout != oldTimeout) {
		fprintf(stderr, "watchdogd: Actual WDT timeout: %i seconds\n",
			timeout);
		fprintf(stderr,
			"watchdogd: Timeout specified in the configuration file: %i\n",
			oldTimeout);
	}

	if (EnableWatchdog(watchdog) < 0) {
		return -1;
	}

	SetTimeout(watchdog, timeout);

	return PingWatchdog(watchdog);
}

static int LegacyOutOfMemoryKillerConfig(void)
{
	int fd = open("/proc/self/om_adj",O_WRONLY);
	
	if (fd < 0)  {
		Logmsg(LOG_ERR, "open failed: %s", strerror(errno));
		return -1;
	}

	if (write(fd, "-17", strlen("-17")) < 0) {
		Logmsg(LOG_ERR, "write failed: %s", strerror(errno));
		close(fd);
		return -1;
	} else {
		close(fd);
	}

	return 0;
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

		close(dfd);

		if (LegacyOutOfMemoryKillerConfig() == 0) {
			return 0;
		}

		return -1;
	}

	if (write(fd, "-1000", strlen("-1000")) < 0) {
		Logmsg(LOG_ERR, "write failed: %s", strerror(errno));
		CloseWraper(&fd);
		CloseWraper(&dfd);
		return -1;
	}

	close(fd);
	close(dfd);

	return 0;
}

static const char *GuessRandomSeedFilename(void)
{
	struct stat buf;

	if (stat("/var/run/random-seed", &buf) == 0) {
		return "/var/run/random-seed";
	}

	if (stat("/usr/lib/systemd/systemd-random-seed", &buf) == 0) {
		return "/usr/lib/systemd/systemd-random-seed";
	}

	return NULL;
}

const char *GetDefaultRandomSeedPathName(void)
{
	return "/usr/lib/systemd/systemd-random-seed";
}

int SaveRandomSeed(const char *filename)
{
	if (filename == NULL) {
		filename = GuessRandomSeedFilename();
	}

	if (filename == NULL) {
		return -1;
	}

	int fd = open("/dev/urandom", O_RDONLY);

	if (fd < 0) {
		return -1;
	}

	char buf[512] = {0};

	int ret = read(fd, buf, sizeof(buf));

	if (ret == -1) {
		goto error;
	}

	close(fd);

	fd = open(filename, O_TRUNC|O_CREAT, 0600);

	if (fd < 1) {
		goto error;
	}

	if (fchown(fd, 0, 0) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}

	ret = write(fd, buf, sizeof(buf));

	if (ret == -1) {
		goto error;
	}

	close(fd);
	return 0;
 error:
	fprintf(stderr, "%s\n", strerror(errno));

	if (fd >= 0) {
		close(fd);
	}

	return -1;
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
	struct libmnt_fs *currentfs = mnt_new_fs();

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

static int IsSwaparea(struct libmnt_fs *fs, void *unused)
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
	sync();

	if (errorcode == WETEMP) {
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
		if (reboot(LINUX_REBOOT_CMD_KEXEC) == -1) {
			if (errno == EPERM) {
				return -1;
			}
		}
	}

	if (reboot(LINUX_REBOOT_CMD_RESTART) == -1) {
		if (errno == EPERM) {
			return -1;
		} else {
			abort();
		}
	}

	return -1;
}
#endif
