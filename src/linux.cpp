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
#include "watchdogd.hpp"
#include "linux.hpp"
#include "sub.hpp"
#include "repair.hpp"
#ifdef __linux__

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
	sd_notifyf(0, "READY=1\n" "MAINPID=%lu", (unsigned long)getppid());

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
		size_t bytesRead = gzread(config, buf, sizeof(buf) - 1);
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

bool KillAllOtherThreads(void)
{
	pid_t tid = syscall(SYS_gettid);

	DIR * dir = opendir("/proc/self/task");

	if (dir == NULL) {
		return false;
	}

	struct dirent *ent = NULL;

	while ((ent = readdir(dir)) != NULL) {
		if (strchr(".", ent->d_name[0]) != NULL) {
			continue;
		}

		pid_t ctid = (pid_t)ConvertStringToInt(ent->d_name);

		if (ctid == tid) {
			continue;
		}

		int ret = 0;
		do
			ret = syscall(SYS_tgkill, getpid(), ctid, 32);
		while(ret != -1);
	}

	if (closedir(dir) != 0) {
		abort();
	}

	return true;
}
#endif
