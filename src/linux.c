/*
 * Copyright 2013-2016 Christian Lockley
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
#define _GNU_SOURCE
#include "watchdogd.h"
#include "linux.h"
#include "sub.h"
#include "repair.h"
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

	Logmsg(LOG_ERR, "WDIOC_KEEPALIVE ioctl failed: %s", MyStrerror(errno));

	return -1;
}

int CloseWatchdog(watchdog_t * const watchdog)
{
	if (watchdog == NULL) {
		return -1;
	}

	if (write(GetFd(watchdog), "V", strlen("V")) < 0) {
		Logmsg(LOG_CRIT, "write to watchdog device failed: %s",
		       MyStrerror(errno));
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

static bool CanMagicClose(watchdog_t * const wdt)
{
	struct watchdog_info watchDogInfo;

	if (ioctl(GetFd(wdt), WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", MyStrerror(errno));
		return false;
	}

	return WDIOF_MAGICCLOSE & watchDogInfo.options;
}

bool PrintWdtInfo(watchdog_t * const wdt)
{
	struct watchdog_info watchDogInfo;

	assert(wdt != NULL);

	if (ioctl(GetFd(wdt), WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", MyStrerror(errno));
	} else {
		if (strcasecmp((const char*)watchDogInfo.identity, "software watchdog") != 0) {
			Logmsg(LOG_DEBUG, "Hardware watchdog '%s', version %lu",
			       watchDogInfo.identity, watchDogInfo.firmware_version);
		} else {
			Logmsg(LOG_DEBUG, "%s, version %lu",
			       watchDogInfo.identity, watchDogInfo.firmware_version);
		}
		return true;
	}

	return false;
}

unsigned char *GetWatchdogIdentity(watchdog_t * const wdt)
{
	static struct watchdog_info watchDogInfo;

	assert(wdt != NULL);

	PingWatchdog(wdt);

	if (ioctl(GetFd(wdt), WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", MyStrerror(errno));
		return NULL;
	}

	return watchDogInfo.identity;
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
		       "unable to open watchdog device: %s", MyStrerror(errno));
		WatchdogDestroy(watchdog);
		return NULL;
	}

	if (PingWatchdog(watchdog) != 0) {
		WatchdogDestroy(watchdog);
		return NULL;
	}

	if (CanMagicClose(watchdog) == false) {
		Logmsg(LOG_ALERT,
		       "watchdog device does not support magic close char");
	}

	strcpy((char *)watchdog->path, path);

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
		       MyStrerror(errno));
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
		       MyStrerror(errno));
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
		       MyStrerror(errno));
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

	SetTimeout(watchdog, timeout);

	if (EnableWatchdog(watchdog) < 0) {
		return -1;
	}

	SetTimeout(watchdog, timeout);

	return PingWatchdog(watchdog);
}

int GetWatchdogBootStatus(watchdog_t * const wdt)
{
	if (wdt == NULL) {
		return -1;
	}

	int status = 0;

	ioctl(GetFd(wdt), WDIOC_GETBOOTSTATUS, &status);

	return status;
}

long unsigned GetWatchdogStatus(watchdog_t * const wdt)
{
	if (wdt == NULL) {
		return 0;
	}

	long unsigned status = 0;

	ioctl(GetFd(wdt), WDIOC_GETBOOTSTATUS, &status);

	return status;
}

long GetFirmwareVersion(watchdog_t * const wdt)
{
	if (wdt == NULL) {
		return 0;
	}

	struct watchdog_info watchDogInfo = {0};

	ioctl(GetFd(wdt), WDIOC_GETSUPPORT, &watchDogInfo);

	return watchDogInfo.firmware_version;
}

long GetTimeleft(watchdog_t *const wdt)
{
//This function is only used by the dbus api
	if (wdt == NULL) {
		return -1;
	}

	int timeleft = 0;

	if (ioctl(GetFd(wdt), WDIOC_GETTIMELEFT, &timeleft) < 0) {
		return -1;
	}

	return timeleft;
}

int GetRawTimeout(watchdog_t * const wdt)
{
	if (wdt == NULL) {
		return -1;
	}
	int timeout = 0;
	ioctl(GetFd(wdt), WDIOC_GETTIMEOUT, &timeout);
	return timeout;
}

static int ConfigureKernelOutOfMemoryKiller(void)
{
	int fd = 0;
	int dfd = 0;

	dfd = open("/proc/self", O_DIRECTORY | O_RDONLY);

	if (dfd == -1) {
		Logmsg(LOG_ERR, "open failed: %s", MyStrerror(errno));
		return -1;
	}

	fd = openat(dfd, "oom_score_adj", O_WRONLY);

	if (fd == -1) {
		Logmsg(LOG_ERR, "open failed: %s", MyStrerror(errno));

		close(dfd);

		return -1;
	}

	if (write(fd, "-1000", strlen("-1000")) < 0) {
		Logmsg(LOG_ERR, "write failed: %s", MyStrerror(errno));
		close(fd);
		close(dfd);
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

	if (stat("/var/lib/systemd/random-seed", &buf) == 0) {
		return "/var/lib/systemd/random-seed";
	}

	return NULL;
}

const char *GetDefaultRandomSeedPathName(void)
{
	return GuessRandomSeedFilename();
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

	char buf[512] = { 0 };

	int ret = read(fd, buf, sizeof(buf));

	if (ret == -1) {
		goto error;
	}

	close(fd);

	fd = open(filename, O_TRUNC | O_CREAT, 0600);

	if (fd < 1) {
		goto error;
	}

	if (fchown(fd, 0, 0) < 0) {
		fprintf(stderr, "%s\n", MyStrerror(errno));
	}

	ret = write(fd, buf, sizeof(buf));

	if (fsync(fd) != 0) {
		goto error;
	}

	if (ret == -1) {
		goto error;
	}

	close(fd);

	fprintf(stderr, "saved random seed\n");

	return 0;
 error:
	fprintf(stderr, "%s\n", MyStrerror(errno));

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

int StopNetwork(void)
{
	struct ifaddrs *ifaddr;

	if (getifaddrs(&ifaddr) == -1) {
		return -1;
	}

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockfd < 0) {
		return -1;
	}

	for (struct ifaddrs * ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_flags & IFF_LOOPBACK) {
			continue;
		}

		if (!(ifa->ifa_flags & IFF_UP)) {
			continue;
		}

		struct ifreq ifr;
		memset(&ifr, 0, sizeof ifr);
		strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
		ifr.ifr_flags &= ~IFF_UP;

		ioctl(sockfd, SIOCSIFFLAGS, &ifr);
	}

	close(sockfd);
	freeifaddrs(ifaddr);

	return 0;
}

static bool IsRootStorageDaemon(pid_t pid)
{
//http://www.freedesktop.org/wiki/Software/systemd/RootStorageDaemons/
	char path[512] = { "\0" };
	snprintf(path, sizeof(path) - 1, "/proc/%ld/cmdline", (long)pid);

	int fd = open(path, O_RDONLY | O_CLOEXEC);

	char buf[32] = { "\0" };

	if (fd < 0) {
		goto error;
	}

	if (read(fd, (void *)buf, 64) < 0) {
		goto error;
	}

	if (strstr(buf, "@") == NULL) {	//kernel null terminates arguments so we only read the first arg.
		goto error;
	}

	close(fd);

	return true;

 error:
	if (fd >= 0) {
		close(fd);
	}

	return false;
}

bool DontKillProcess(pid_t pid)
{
	if (IsRootStorageDaemon(pid) == true) {
		return true;
	}

	return false;
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

int LinuxRunningSystemd(void)
{
	return sd_booted();
}

bool PlatformInit(void)
{
	sd_notifyf(0, "READY=1\n" "MAINPID=%lu", (unsigned long)getpid());

	if (ConfigureKernelOutOfMemoryKiller() < 0) {
		Logmsg(LOG_ERR, "unable to configure out of memory killer");
		return false;
	}

	prctl(PR_SET_DUMPABLE, 0, 0, 0, 0); //prevent children from ptrace() ing main process and helpers

	return true;
}

int NativeShutdown(int errorcode, int kexec)
{
//http://cgit.freedesktop.org/systemd/systemd/tree/src/core/manager.c?id=f49fd1d57a429d4a05ac86352c017a845f8185b3
	extern sig_atomic_t stopPing;

	stopPing = 1;

	if (LinuxRunningSystemd() == 1) {
		if (kexec == 1) {
			kill(1, SIGRTMIN + 6);
		}

		if (errorcode == WECMDREBOOT) {
			kill(1, SIGINT);
		}

		if (errorcode == WETEMP) {
			kill(1, SIGRTMIN + 4);
		}

		if (errorcode == WECMDRESET) {
			kill(1, SIGRTMIN + 15);
		}
	}

	return -1;
}

int GetConsoleColumns(void)
{
	struct winsize w = { 0 };
	if (ioctl(0, TIOCGWINSZ, &w) < 0) {
		return 80;
	}

	return w.ws_col;
}

int SystemdWatchdogEnabled(const int unset, long long int *const interval)
{
	if (LinuxRunningSystemd() == 0) {
		return 0;
	}

	const char *str = getenv("WATCHDOG_USEC");
	if (str == NULL) {
		return -1;
	}

	const char *watchdogPid = getenv("WATCHDOG_PID");
	if (watchdogPid != NULL) {
		if (getpid() != (pid_t) strtoll(watchdogPid, NULL, 10)) {
			return -1;
		}
	} else {
		Logmsg(LOG_WARNING,
		       "Your version of systemd is out of date. Upgrade for better integration with watchdogd");
	}

	if (interval != NULL) {
		if (strtoll(str, NULL, 10) < 0) {
			return -1;
		}

		if (strtoll(str, NULL, 10) == 0) {
			return 0;
		}

		*interval = strtoll(str, NULL, 10);
	}

	if (unset != 0) {
		if (unsetenv("WATCHDOG_PID") < 0) {
			Logmsg(LOG_WARNING,
			       "Unable to delete WATCHDOG_PID environment variable:%s",
			       MyStrerror(errno));
		} else if (unsetenv("WATCHDOG_USEC") < 0) {
			return -1;
		}
	}

	return 1;
}

bool OnParentDeathSend(uintptr_t sig)
{
	if (prctl(PR_SET_PDEATHSIG, (uintptr_t  *)sig) == -1) {
		return false;
	}

	return true;
}

int NoNewProvileges(void)
{
	if (prctl(PR_SET_NO_NEW_PRIVS, 0, 0, 0, 0) < 0) {
		if (errno != 0) {
			return -errno;
		} else {
			return 1;
		}
	}

	return 0;
}

int GetCpuCount(void)
{
	return get_nprocs();
}

bool LoadKernelModule(void)
{
	pid_t pid = fork();
	if (pid == 0) {
		if (execl("/sbin/modprobe", "modprobe", "softdog", NULL) == -1) {
			_Exit(1);
		}
	}

	if (pid == -1) {
		abort();
	}

	int ret = 0;

	waitpid(pid, &ret, 0);

	if (WEXITSTATUS(ret) == 0) {
		return true;
	}

	return false;
}

bool MakeDeviceFile(const char *file)
{
	return true;
}

static bool GetDeviceMajorMinor(struct dev *m, char *name)
{
	if (name == NULL || m == NULL) {
		return false;
	}

	char *tmp = basename(name);
	size_t len = 0;
	char *buf = NULL;
	struct dev tmpdev = {0};

	DIR *dir = opendir("/sys/dev/char");

	if (dir == NULL) {
		return false;
	}

	for (struct dirent *node = readdir(dir); node != NULL; node = readdir(dir)) {
		if (node->d_name[0] == '.') {
			continue;
		}
		Wasnprintf(&len, &buf, "/sys/dev/char/%s/uevent", node->d_name);

		if (buf == NULL) {
			abort();
		}

		FILE *fp = fopen(buf, "r");

		if (fp == NULL) {
			abort();
		}

		int x = 1;
		while (getline(&buf, &len, fp) != -1) {		
			char *const name = strtok(buf, "=");
			char *const value = strtok(NULL, "=");
		
			if (Validate(name, value) == false) {
				continue;
			}
			if (x == 1) {
				tmpdev.major = strtoul(value, NULL, 0);
				x++;
			} else if (x == 2) {
				tmpdev.minor = strtoul(value, NULL, 0);
				x++;
			} else if (x == 3) {
				strcpy(tmpdev.name, value);
				x = 1;
			}
		}

		if (strcmp(tmpdev.name, tmp) == 0) {
			closedir(dir);
			free(buf);
			fclose(fp);
			strcpy(m->name, tmpdev.name);
			m->major = tmpdev.major;
			m->minor = tmpdev.minor;
			return true;
		}


		fclose(fp);
	}
	closedir(dir);
	free(buf);
	return false;
}

int ConfigWatchdogNowayoutIsSet(char *name)
{
	bool found = false;
	char *buf = NULL;
	gzFile config = gzopen("/proc/config.gz", "r");

	if (config == NULL) {
		return -1;
	}

	gzbuffer(config, 8192);

	while (true) {
		char buf[4096] = {'\0'};
		int bytesRead = gzread(config, buf, sizeof(buf) - 1);
		if (strstr(buf, "# CONFIG_WATCHDOG_NOWAYOUT is not set") != NULL) {
			found = true;
			break;
		}

		if (bytesRead < sizeof(buf) - 1) {
			if (gzeof(config)) {
				break;
			} else {
				break;
			}
		}
	}

	gzclose(config);
	
	struct dev ad = {0};

	GetDeviceMajorMinor(&ad, name);
	char * devicePath = NULL;
	Wasprintf(&devicePath, "/sys/dev/char/%lu:%lu/device/driver", ad.major, ad.minor);
	buf = (char *)calloc(1, 4096);
	if (devicePath == NULL) {
		abort();
	}
	readlink(devicePath, buf, 4096 - 1);
	Wasprintf(&buf, "/sys/module/%s/parameters/nowayout", basename(buf));
	free(devicePath);
	FILE *fp = fopen(buf, "r");

	if (fp != NULL) {
		if (fgetc(fp) == '1') {
			found = false;
		}
		fclose(fp);
	}

	free(buf);

	if (found) {
		return 0;
	}

	return 1;
}

bool IsClientAdmin(int sock)
{
	struct ucred peercred;
	socklen_t len = sizeof(struct ucred);

	if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &peercred, &len) != 0) {
		return false;
	}

	if (peercred.uid == 0) {
		return true;
	}
	return false;
}

#endif
