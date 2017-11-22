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

#define _DEFAULT_SOURCE
#include "watchdogd.hpp"
#include "logutils.hpp"
#include <netdb.h>
#include "sub.hpp"
#include "errorlist.hpp"
#include "threads.hpp"
#include "testdir.hpp"
#include "exe.hpp"
#include "network_tester.hpp"
#include "dbusapi.hpp"
#include "linux.hpp"

extern volatile sig_atomic_t stop;
static pthread_mutex_t managerlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t workerupdate = PTHREAD_COND_INITIALIZER;

static long pageSize = 0;
static pthread_once_t getPageSize = PTHREAD_ONCE_INIT;
static void GetPageSize(void)
{
	pageSize = sysconf(_SC_PAGESIZE);
}

void *DbusHelper(void * arg)
{
	struct dbusinfo * info = (struct dbusinfo *)arg;
	Watchdog * wdt = *info->wdt;
	cfgoptions * config = *info->config;
	unsigned int cmd = 0;

	long version = wdt->GetFirmwareVersion();
	char *identity = (char*)wdt->GetIdentity();
	int timeout = wdt->GetRawTimeout();

	while (stop == 0) {
		int ret = read(info->fd, &cmd, sizeof(unsigned int));
		if (ret < 0 && errno != EINTR) {
			break;
		}
		int x = 0;
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &x);
		switch (cmd) {
			case DBUSGETIMOUT:
				{
					write(info->fd, &timeout, sizeof(int));
				};
				break;
			case DBUSTIMELEFT:
				{
					int timeleft = wdt->GetTimeleft();
					write(info->fd, &timeleft, sizeof(int));
				};
				break;
			case DBUSGETPATH:
				{
					write(info->fd, config->devicepath, strlen(config->devicepath));
				};
				break;
			case DBUSVERSION:
				{
					write(info->fd, &version, sizeof(version));
				};
				break;
			case DBUSGETNAME:
				{
					write(info->fd, identity, strlen((char *)identity));
				};
				break;
			case DBUSHUTDOWN:
				{
					Shutdown(9221996, config);
				};
				break;
		}
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &x);
	}

	pthread_exit(NULL);
}

struct SystemdWatchdog
{
	struct timespec tp;
	pid_t pid;
};

static void *ServiceManagerKeepAliveNotification(void * arg)
{
	struct SystemdWatchdog *tArg = (struct SystemdWatchdog*)arg;

	while (true) {
		int ret = sd_pid_notify(tArg->pid, 0, "WATCHDOG=1");
		if (ret < 0) {
			Logmsg(LOG_ERR, "%s", MyStrerror(-ret));
		}
		nanosleep(&tArg->tp, NULL);
	}

	return NULL;
}

static void * CheckNetworkInterfacesThread(void *arg)
{
	struct cfgoptions *s = (struct cfgoptions *)arg;
	int retries = 0;
	while (true) {
		pthread_mutex_lock(&managerlock);

		char *ifname;

		if (NetMonCheckNetworkInterfaces(&ifname) == false) {
			retries += 1;
			if (retries > 12) {
				Logmsg(LOG_ERR, "network interface: %s is disconected", ifname);
				s->error |= NETWORKDOWN;
			}
		} else {
			if (s->error & NETWORKDOWN) {
				s->error &= !NETWORKDOWN;
			}
			retries = 0;
		}

		pthread_cond_wait(&workerupdate, &managerlock);
		pthread_mutex_unlock(&managerlock);
	}

	return NULL;
}

static void *Sync(void *arg)
{
	for (;;) {
		pthread_mutex_lock(&managerlock);

		sync();

		pthread_cond_wait(&workerupdate, &managerlock);

		pthread_mutex_unlock(&managerlock);
	}

	return NULL;
}

static void *Ping(void *arg)
{
	struct cfgoptions *s = (struct cfgoptions *)arg;
	static char buf[NI_MAXHOST];
	for (;;) {
		pthread_mutex_lock(&managerlock);
		if (ping_send(s->pingObj) > 0) {
			for (pingobj_iter_t * iter =
			     ping_iterator_get(s->pingObj); iter != NULL;
			     iter = ping_iterator_next(iter)) {
				double latency = -1.0;
				size_t len = sizeof(latency);
				ping_iterator_get_info(iter, PING_INFO_LATENCY,
						       &latency, &len);

				len = NI_MAXHOST;
				ping_iterator_get_info(iter, PING_INFO_ADDRESS,
						       &buf, &len);

				if (latency > 0.0) {
					void *cxt =
					    ping_iterator_get_context(iter);
					if (cxt != NULL) {
						if (s->error & PINGFAILED) {
							s->error &= !PINGFAILED;
						}
						free(cxt);
						ping_iterator_set_context(iter,
									  NULL);
					}
					continue;
				} else {
					Logmsg(LOG_ERR,
					       "no response from ping (target: %s)",
					       buf);
				}

				memset(buf, 0, NI_MAXHOST);

				if (ping_iterator_get_context(iter) == NULL) {
					ping_iterator_set_context(iter,
								  calloc(1,
									 sizeof
									 (int
									  )));
					void *cxt =
					    ping_iterator_get_context(iter);
					if (cxt == NULL) {
						Logmsg(LOG_ERR,
						       "unable to allocate memory for ping context");
						s->error |= PINGFAILED;
					} else {
						int *retries = (int *)cxt;
						*retries = *retries + 1;
					}
				} else {
					int *retries = (int *)
					    ping_iterator_get_context(iter);
					if (*retries > 3) {	//FIXME: This should really be a config value.
						free(ping_iterator_get_context
						     (iter));
						ping_iterator_set_context(iter,
									  NULL);
						s->error |= PINGFAILED;
					} else {
						*retries += 1;
					}
				}
			}
		} else {
			Logmsg(LOG_ERR, "%s", ping_get_error(s->pingObj));
			s->error |= PINGFAILED;
		}

		pthread_cond_wait(&workerupdate, &managerlock);
		pthread_mutex_unlock(&managerlock);
	}

	return NULL;
}

static void *LoadAvgThread(void *arg)
{
	struct cfgoptions *s = (struct cfgoptions *)arg;

	double load[3] = { 0 };

	for (;;) {
		pthread_mutex_lock(&managerlock);

		if (getloadavg(load, 3) == -1) {
			Logmsg(LOG_CRIT,
			       "watchdogd: getloadavg() failed load monitoring disabled");
			/*pthread_exit(NULL); */
		}

		if (load[0] > s->maxLoadOne || load[1] > s->maxLoadFive
		    || load[2] > s->maxLoadFifteen) {
			s->error |= LOADAVGTOOHIGH;
		} else {
			if (s->error & LOADAVGTOOHIGH)
				s->error &= !LOADAVGTOOHIGH;
		}

		pthread_cond_wait(&workerupdate, &managerlock);
		pthread_mutex_unlock(&managerlock);
	}

	return NULL;
}

static void *TestDirThread(void *arg)
{
	//This thread is a bit different as we don't want to prevent the other
	//tests from running.

	struct cfgoptions *s = (struct cfgoptions *)arg;

	for (;;) {
		if (ExecuteRepairScripts() < 0) {
			s->error |= SCRIPTFAILED;
		} else {
			if (s->error & SCRIPTFAILED) {
				s->error &= !SCRIPTFAILED;
			}
		}

		sleep(30);

		if (stop == 1) {
			pthread_exit(NULL);
		}
	}

	return NULL;
}

static void *TestBinThread(void *arg)
{
	//This thread is a bit different as we don't want to prevent the other
	//tests from running.

	struct cfgoptions *s = (struct cfgoptions *)arg;
	struct timespec rqtp;

	rqtp.tv_sec = 5;
	rqtp.tv_nsec = 5 * 1000;

	for (;;) {
		int ret = Spawn(s->testBinTimeout, s, s->testexepathname,
				s->testexepathname, "test", NULL);
		if (ret == 0) {
			s->testExeReturnValue = 0;
		} else {
			s->testExeReturnValue = ret;
		}

		nanosleep(&rqtp, NULL);

		if (stop == 1) {
			pthread_exit(NULL);
		}
	}

	return NULL;
}

static void *MinPagesThread(void *arg)
{
	struct cfgoptions *s = (struct cfgoptions *)arg;
	struct sysinfo infostruct;

	pthread_once(&getPageSize, GetPageSize);

	if (pageSize < 0) {
		Logmsg(LOG_ERR, "%s", MyStrerror(errno));
		return NULL;
	}

	for (;;) {
		pthread_mutex_lock(&managerlock);

		if (sysinfo(&infostruct) == -1) {	/*sysinfo not in POSIX */
			Logmsg(LOG_CRIT,
			       "watchdogd: sysinfo() failed free page monitoring disabled");
			/*pthread_exit(NULL); */
		}

		unsigned long fpages =
		    (infostruct.freeram + infostruct.freeswap) / 1024;

		if (fpages < s->minfreepages * (unsigned long)(pageSize / 1024)) {
			s->error |= OUTOFMEMORY;
		} else {
			if (s->error & OUTOFMEMORY)
				s->error &= !OUTOFMEMORY;
		}

		pthread_cond_wait(&workerupdate, &managerlock);
		pthread_mutex_unlock(&managerlock);
	}

	return NULL;
}

static void *TestMemoryAllocation(void *arg)
{
	struct cfgoptions *config = (struct cfgoptions *)arg;

	if (config->allocatableMemory <= 0) {
		return NULL;
	}

	pthread_once(&getPageSize, GetPageSize);

	for (;;) {
		pthread_mutex_lock(&managerlock);

		void *buf = mmap(NULL, config->allocatableMemory * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0 ,0);

		if (buf == MAP_FAILED) {
			Logmsg(LOG_ALERT, "mmap failed: %s", MyStrerror(errno));
			config->error |= OUTOFMEMORY;
		}

		memset(buf, 0, (config->allocatableMemory * pageSize));

		if (munmap(buf, config->allocatableMemory * pageSize) != 0) {
			Logmsg(LOG_CRIT, "munmap failed: %s", MyStrerror(errno));
			assert(false);
		}

		pthread_cond_wait(&workerupdate, &managerlock);
		pthread_mutex_unlock(&managerlock);
	}

	return NULL;
}

static void *TestFork(void *arg)
{
	assert(arg != NULL);

	struct cfgoptions *s = (struct cfgoptions *)arg;
	pid_t pid = 0;

	for (;;) {
		pid = fork();

		if (pid == 0) {
			_Exit(EXIT_SUCCESS);
		} else if (pid < 0) {
			if (errno == EAGAIN) {
				s->error |= FORKFAILED;
			}
		} else {
			if (waitpid(pid, NULL, 0) != pid) {
				Logmsg(LOG_ERR, "watchdogd: %s",
				       MyStrerror(errno));
				Logmsg(LOG_ERR, "watchdogd: waitpid failed");
			}
		}
		sleep(60);
	}

	return NULL;
}

static void *TestPidfileThread(void *arg)
{
	assert(arg != NULL);

	struct cfgoptions *s = (struct cfgoptions *)arg;

	for (;;) {
		pthread_mutex_lock(&managerlock);

		for (int cnt = 0; cnt < config_setting_length(s->pidFiles);
		     cnt++) {
			const char *pidFilePathName = NULL;
			pidFilePathName =
			    config_setting_get_string_elem(s->pidFiles, cnt);

			if (pidFilePathName == NULL) {
				s->error |= UNKNOWNPIDFILERROR;
				break;
			}

			int fd = 0;

			time_t startTime = 0;

			if (time(&startTime) == (time_t) (-1)) {
				s->error |= UNKNOWNPIDFILERROR;
				break;
			}

			time_t currentTime = 0;

			do {
				struct timespec rqtp;
				rqtp.tv_sec = 0;
				rqtp.tv_nsec = 250;

				if (time(&currentTime) == (time_t) (-1)) {
					currentTime = 0;
					break;
				}

				fd = open(pidFilePathName,
					  O_RDONLY | O_CLOEXEC);
				if (fd >= 0) {
					break;
				}

				nanosleep(&rqtp, NULL);
			} while (difftime(currentTime, startTime) <=
				 s->retryLimit);

			if (fd < 0) {
				Logmsg(LOG_ERR, "cannot open %s: %s",
				       pidFilePathName, MyStrerror(errno));
				s->error |= PIDFILERROR;
				break;
			}

			struct stat buffer;

			if (fstat(fd, &buffer) != 0) {
				Logmsg(LOG_ERR, "%s: %s", pidFilePathName,
				       MyStrerror(errno));
				if (s->options & SOFTBOOT) {
					close(fd);
					s->error |= PIDFILERROR;
					break;
				} else {
					close(fd);
					continue;
				}
			} else {
				if (S_ISBLK(buffer.st_mode) == true
				    || S_ISCHR(buffer.st_mode) == true
				    || S_ISDIR(buffer.st_mode) == true
				    || S_ISFIFO(buffer.st_mode) == true
				    || S_ISSOCK(buffer.st_mode) == true
				    || S_ISREG(buffer.st_mode) == false) {
					Logmsg(LOG_ERR,
					       "invalid file type %s",
					       pidFilePathName);
					close(fd);
					s->error |= PIDFILERROR;
					break;
				}
			}

			char buf[64] = { 0 };
			if (pread(fd, buf, sizeof(buf), 0) == -1) {
				Logmsg(LOG_ERR, "unable to read pidfile %s: %s",
				       pidFilePathName, MyStrerror(errno));
				close(fd);
				s->error |= PIDFILERROR;
				break;
			}

			close(fd);

			errno = 0;

			pid_t pid = (pid_t) strtol(buf, (char **)NULL, 10);

			if (pid == 0) {
				Logmsg(LOG_ERR, "strtol failed: %s",
				       MyStrerror(errno));
				s->error |= UNKNOWNPIDFILERROR;
				break;
			}

			if (kill(pid, 0) == -1) {
				Logmsg(LOG_ERR,
				       "unable to send null signal to pid %i: %s: %s",
				       pid, pidFilePathName, MyStrerror(errno));
				if (errno == ESRCH) {
					s->error |= PIDFILERROR;
					break;
				}

				if (s->options & SOFTBOOT) {
					s->error |= PIDFILERROR;
					break;
				}
			}
		}

		pthread_cond_wait(&workerupdate, &managerlock);
		pthread_mutex_unlock(&managerlock);
	}

	return NULL;
}

void *IdentityThread(void *arg)
{

	struct identinfo *i = (struct identinfo*)arg;
	struct sockaddr_un address = {0};
	address.sun_family = AF_UNIX;
	strncpy(address.sun_path, "\0watchdogd.wdt.identity", sizeof(address.sun_path)-1);
	int fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);

	if (fd < 0) {

		return NULL;
	}

	if (bind(fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
		close(fd);
		return NULL;
	}

	if (listen(fd, 2) == -1) {
		close(fd);
		return NULL;
	}

	while (true) {
		int conection = accept(fd, NULL, NULL);

		if (IsClientAdmin(conection) == false) {
			struct identinfo tmp = *i;
			tmp.flags = 0;
			tmp.firmwareVersion = 0;
			memset(tmp.deviceName, 7, sizeof(tmp.deviceName));
			write(conection, &tmp, sizeof(tmp));
			close(conection);
			continue;
		}

		write(conection, i, sizeof(*i));

		close(conection);
	}

	return NULL;
}

static void *ManagerThread(void *arg)
{
	/*This thread gets data from the non main threads and
	   calls a function to take care of the problem */

	cfgoptions *s = (cfgoptions *)arg;
	struct timespec rqtp;

	rqtp.tv_sec = 5;
	rqtp.tv_nsec = 5 * 1000;

	for (;;) {
		pthread_mutex_lock(&managerlock);
		pthread_cond_broadcast(&workerupdate);
#if 0
		if (s->temptoohigh == 1) {
			/*Shutdown(true) */ ;
		}
#endif
		if (s->error & LOADAVGTOOHIGH) {
			Logmsg(LOG_ERR,
			       "polled load average exceed configured load average limit");
			if (Shutdown(WESYSOVERLOAD, s) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & OUTOFMEMORY) {
			Logmsg(LOG_ERR,
			       "less than configured free pages available");
			if (Shutdown(WEOTHER, s) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & FORKFAILED) {
			Logmsg(LOG_ERR,
			       "process table test failed because fork failed");
			if (Shutdown(WEOTHER, s) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & SCRIPTFAILED) {
			Logmsg(LOG_ERR, "repair script failed");
			if (Shutdown(WESCRIPT, s)
			    < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & PIDFILERROR || s->error & UNKNOWNPIDFILERROR) {
			Logmsg(LOG_ERR, "pid file test failed");
			if (Shutdown(WEPIDFILE, s)
			    < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->testExeReturnValue > 0 || s->testExeReturnValue < 0) {
			Logmsg(LOG_ERR, "check executable failed");
			if (Shutdown(s->testExeReturnValue, s) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & PINGFAILED) {
			Logmsg(LOG_ERR, "ping test failed... rebooting system");
			if (Shutdown(PINGFAILED, s) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & NETWORKDOWN) {
			Logmsg(LOG_ERR, "network down... rebooting system");
			if (Shutdown(PINGFAILED, s) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		pthread_mutex_unlock(&managerlock);
		nanosleep(&rqtp, NULL);

		if (stop == 1) {
			break;
		}
	}

	return NULL;
}

int StartHelperThreads(struct cfgoptions *options)
{

	if (StartServiceManagerKeepAliveNotification(NULL) < 0) {
		return -1;
	}

	if (SetupTestFork(options) < 0) {
		return -1;
	}

	if (SetupExeDir(options) < 0) {
		return -1;
	}

	if (options->testexepathname != NULL) {
		if (SetupTestBinThread(options) < 0) {
			return -1;
		}
	}

	if (options->maxLoadOne > 0) {
		if (SetupLoadAvgThread(options) < 0) {
			return -1;
		}
	}

	if (options->options & SYNC) {
		if (SetupSyncThread(options) < 0) {
			return -1;
		}
	}

	if (options->minfreepages != 0) {
		if (SetupMinPagesThread(options) < 0) {
			return -1;
		}
	}

	if (options->options & ENABLEPIDCHECKER) {
		if (StartPidFileTestThread(options) < 0) {
			return -1;
		}
	}

	if (options->options & ENABLEPING && StartPingThread(options) < 0) {
		return -1;
	}

	if (SetupTestMemoryAllocationThread(options) < 0) {
		return -1;
	}

	if (options->networkInterfaces != NULL) {
		StartCheckNetworkInterfacesThread(options);
	}

	return 0;
}

int SetupTestFork(void *arg)
{
	if (CreateDetachedThread(TestFork, arg) < 0)
		return -1;

	return 0;
}

int SetupExeDir(void *arg)
{
	extern struct repairscriptTranctions *rst;
	if (rst == NULL)
		return 0;
	if (CreateDetachedThread(TestDirThread, arg) < 0)
		return -1;

	return 0;
}

int SetupMinPagesThread(void *arg)
{
	struct cfgoptions *s = (struct cfgoptions *)arg;

	assert(arg != NULL);

	if (s->minfreepages == 0)
		return 0;

	if (CreateDetachedThread(MinPagesThread, arg) < 0)
		return -1;

	return 0;
}

int SetupLoadAvgThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(LoadAvgThread, arg) < 0)
		return -1;

	return 0;
}

int SetupTestBinThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(TestBinThread, arg) < 0)
		return -1;

	return 0;
}

int SetupTestMemoryAllocationThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(TestMemoryAllocation, arg) < 0) {
		return -1;
	}

	return 0;
}

int SetupAuxManagerThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(ManagerThread, arg) < 0)
		return -1;

	return 0;
}

int SetupSyncThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(Sync, arg) < 0)
		return -1;

	return 0;
}

int StartPidFileTestThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(TestPidfileThread, arg) < 0)
		return -1;

	return 0;
}

int StartPingThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(Ping, arg) < 0)
		return -1;

	return 0;
}

int StartServiceManagerKeepAliveNotification(void *arg)
{
	long long int usec = 0;
	pid_t pid = 0;
	int ret = SystemdWatchdogEnabled(&pid, &usec);

	if (ret == -1) {
		return 0;
	}

	usec *= 1000;
	usec /= 2;

	static struct SystemdWatchdog tArg;
	tArg.pid = pid;
	while (usec > 999999999) {
		tArg.tp.tv_sec += 1;
		usec -= 999999999;
	}

	tArg.tp.tv_nsec = (long)usec;

	if (CreateDetachedThread(ServiceManagerKeepAliveNotification, &tArg) < 0) {
		return -1;
	}

	return 0;
}

int StartCheckNetworkInterfacesThread(void *arg)
{
	assert(arg != NULL);

	if (CreateDetachedThread(CheckNetworkInterfacesThread, arg) < 0)
		return -1;

	return 0;
}
