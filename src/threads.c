/*
 * Copyright 2013 Christian Lockley
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
 * 
 */

#define _BSD_SOURCE
#include "watchdogd.h"
#include <pthread.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include "sub.h"
#include "errorlist.h"
#include "threads.h"
#include "testdir.h"
#include "exe.h"

extern volatile sig_atomic_t shutdown;
static pthread_mutex_t managerlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t workerupdate = PTHREAD_COND_INITIALIZER;

static long pageSize = 0;

/*static time_t Time(time_t *tloc)
{
	struct timespec tp;
	int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (tloc == NULL)
		return tp.tv_sec;
	else
		*tloc = tp.tv_sec;

	return ret;
}*/

void GetPageSize(void)
{
	pageSize = sysconf(_SC_PAGESIZE);
}

void *Sync(void *arg)
{
	(void)arg;
	for (;;) {
		pthread_mutex_lock(&managerlock);

		sync();

		pthread_cond_wait(&workerupdate, &managerlock);

		pthread_mutex_unlock(&managerlock);
	}

	pthread_exit(NULL);

	return NULL;
}

void *MarkTime(void *arg)
{
	/*The thread sends mark mesages to the system log daemon */

	struct cfgoptions *s = (struct cfgoptions *)arg;
	struct timespec rqtp;

	rqtp.tv_sec = (time_t) s->logtickinterval;
	rqtp.tv_nsec = (time_t) s->logtickinterval * 1000;

	for (;;) {
		//Logmsg(LOG_INFO, "still alive after %llu interval(s)", intervals);
		Logmsg(LOG_INFO, "still alive");
		nanosleep(&rqtp, NULL);

		if (shutdown == 1) {
			pthread_exit(NULL);
		}
	}

	pthread_exit(NULL);

	return NULL;
}

void *LoadAvgThread(void *arg)
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

	pthread_exit(NULL);

	return NULL;
}

void *TestDirThread(void *arg)
{
	//This thread is a bit different as we don't want to prevent the other
	//tests from running.

	struct cfgoptions *s = (struct cfgoptions *)arg;
	struct timespec rqtp;

	rqtp.tv_sec = 5;
	rqtp.tv_nsec = 5 * 1000;

	for (;;) {
		if (ExecuteRepairScripts(&parent, s) < 0) {
			s->error |= SCRIPTFAILED;
		}

		nanosleep(&rqtp, NULL);

		if (shutdown == 1) {
			pthread_exit(NULL);
		}
	}

	pthread_exit(NULL);

	return NULL;
}

void *TestBinThread(void *arg)
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

		if (shutdown == 1) {
			pthread_exit(NULL);
		}
	}

	pthread_exit(NULL);

	return NULL;
}

void *MinPagesThread(void *arg)
{
	struct cfgoptions *s = (struct cfgoptions *)arg;
	struct sysinfo infostruct;
	static pthread_once_t getPageSize = PTHREAD_ONCE_INIT;

	pthread_once(&getPageSize, GetPageSize);

	if (pageSize < 0) {
		Logmsg(LOG_ERR, "%s", strerror(errno));
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

	pthread_exit(NULL);

	return NULL;
}

void *TestFork(void *arg)
{
	assert(arg != NULL);

	struct cfgoptions *s = (struct cfgoptions *)arg;
	pid_t pid = 0;

	for (;;) {
		pthread_mutex_lock(&managerlock);

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
				       strerror(errno));
				Logmsg(LOG_ERR, "watchdogd: waitpid failed");
			}
		}

		pthread_cond_wait(&workerupdate, &managerlock);
		pthread_mutex_unlock(&managerlock);
	}

	pthread_exit(NULL);

	return NULL;
}

void *TestPidfileThread(void *arg)
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

			int fd = open(pidFilePathName, O_RDONLY | O_CLOEXEC);

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
				if (fd > 0) {
					break;
				}

				nanosleep(&rqtp, NULL);
			} while (difftime(currentTime, startTime) <=
				 s->retryLimit);

			if (fd < 0) {
				Logmsg(LOG_ERR, "cannot open %s: %s",
				       pidFilePathName, strerror(errno));
				if (s->options & SOFTBOOT) {
					s->error |= PIDFILERROR;
					break;
				} else {
					if (s->retryLimit < 1) {
						continue;
					} else {
						s->error |= PIDFILERROR;
						break;
					}
				}
			}

			struct stat buffer;

			if (fstat(fd, &buffer) != 0) {
				Logmsg(LOG_ERR, "%s: %s", pidFilePathName,
				       strerror(errno));
				if (s->options & SOFTBOOT) {
					s->error |= PIDFILERROR;
					break;
				} else {
					continue;
				}
			}

			if (S_ISBLK(buffer.st_mode) == true
			    || S_ISCHR(buffer.st_mode) == true
			    || S_ISDIR(buffer.st_mode) == true
			    || S_ISFIFO(buffer.st_mode) == true
			    || S_ISSOCK(buffer.st_mode) == true
			    || S_ISREG(buffer.st_mode) == false) {
				Logmsg(LOG_ERR, "invalid file type %s\n",
				       pidFilePathName);
				close(fd);
				continue;
			}

			char buf[64] = { 0x00 };
			if (pread(fd, buf, sizeof(buf), 0) == -1) {
				Logmsg(LOG_ERR, "unable to read pidfile %s: %s",
				       pidFilePathName, strerror(errno));
				close(fd);
				if (s->options & SOFTBOOT) {
					s->error |= PIDFILERROR;
					break;
				} else {
					continue;
				}
			} else {
				close(fd);
			}

			errno = 0;

			pid_t pid = (pid_t) strtol(buf, (char **)NULL, 10);

			if (pid == 0) {
				Logmsg(LOG_ERR, "strtol failed: %s",
				       strerror(errno));
				s->error |= UNKNOWNPIDFILERROR;
				break;
			}

			if (kill(pid, 0) == -1) {
				Logmsg(LOG_ERR,
				       "unable to send null signal to pid %l: %s: %s",
				       pid, pidFilePathName, strerror(errno));
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

void *ManagerThread(void *arg)
{
	/*This thread gets data from the non main threads and
	   calls a function to take care of the problem */

	struct cfgoptions *s = (struct cfgoptions *)arg;
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

		if (s->pingfailed == 1) {	//Need to find out how to use Linux network API.
			/*Shutdown(false) */ ;
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

		pthread_mutex_unlock(&managerlock);
		nanosleep(&rqtp, NULL);

		if (shutdown == 1) {
			break;
		}
	}

	pthread_exit(NULL);

	return NULL;
}

int StartHelperThreads(struct cfgoptions *options)
{
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

	if (options->options & LOGTICK)
		if (SetupLogTick(options) < 0)
			return -1;

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
	if (CreateDetachedThread(TestDirThread, arg) < 0)
		return -1;

	return 0;
}

int SetupMinPagesThread(void *arg)
{
	struct cfgoptions *s = (struct cfgoptions *)arg;

	if (arg == NULL)
		return -1;

	if (s->minfreepages == 0)
		return 0;

	if (CreateDetachedThread(MinPagesThread, arg) < 0)
		return -1;

	return 0;
}

int SetupLoadAvgThread(void *arg)
{
	if (arg == NULL)
		return -1;

	if (CreateDetachedThread(LoadAvgThread, arg) < 0)
		return -1;

	return 0;
}

int SetupTestBinThread(void *arg)
{
	if (arg == NULL)
		return -1;

	if (CreateDetachedThread(TestBinThread, arg) < 0)
		return -1;

	return 0;
}

int SetupAuxManagerThread(void *arg)
{
	if (arg == NULL)
		return -1;

	if (CreateDetachedThread(ManagerThread, arg) < 0)
		return -1;

	return 0;
}

int SetupSyncThread(void *arg)
{
	if (arg == NULL)
		return -1;

	if (CreateDetachedThread(Sync, arg) < 0)
		return -1;

	return 0;
}

int SetupLogTick(void *arg)
{
	if (CreateDetachedThread(MarkTime, arg) < 0)
		return -1;

	return 0;
}

int StartPidFileTestThread(void *arg)
{
	if (arg == NULL)
		return -1;

	if (CreateDetachedThread(TestPidfileThread, arg) < 0)
		return -1;

	return 0;
}
