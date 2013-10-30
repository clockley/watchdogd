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

#include <pthread.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include "sub.h"
#include "watchdogd.h"
#include "errorlist.h"
#include "threads.h"
#include "testdir.h"

extern volatile sig_atomic_t shutdown;
static pthread_mutex_t managerlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t workerupdate = PTHREAD_COND_INITIALIZER;
static pthread_once_t getPageSize = PTHREAD_ONCE_INIT;
static long pageSize = 0;

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

	struct cfgoptions *s = arg;
	struct timespec rqtp;

	rqtp.tv_sec = (time_t) s->logtickinterval;
	rqtp.tv_nsec = (time_t) s->logtickinterval * 1000;

	for (;;) {
		//Logmsg(LOG_INFO, "still alive after %llu interval(s)", intervals);
		Logmsg(LOG_INFO, "still alive");
		nanosleep(&rqtp, NULL);

		if (shutdown == 1) {
			break;
		}
	}

	pthread_exit(NULL);

	return NULL;
}

void *LoadAvgThread(void *arg)
{
	struct cfgoptions *s = arg;

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

	struct cfgoptions *s = arg;
	struct timespec rqtp;

	rqtp.tv_sec = 5;
	rqtp.tv_nsec = 5 * 1000;

	for (;;) {
		if (ExecuteRepairScripts(&parent, arg) < 0) {
			s->error |= SCRIPTFAILED;
		}

		nanosleep(&rqtp, NULL);

		if (shutdown == 1) {
			break;
		}
	}

	pthread_exit(NULL);

	return NULL;
}

void *TestBinThread(void *arg)
{
	//This thread is a bit different as we don't want to prevent the other
	//tests from running.

	struct cfgoptions *s = arg;
	struct timespec rqtp;

	rqtp.tv_sec = 5;
	rqtp.tv_nsec = 5 * 1000;

	for (;;) {
		int ret = Spawn(s->testBinTimeout, arg, s->testexepathname,
				s->testexepathname, "test", NULL);
		if (ret == 0) {
			s->testExeReturnValue = 0;
		} else {
			s->testExeReturnValue = ret;
		}

		nanosleep(&rqtp, NULL);

		if (shutdown == 1) {
			break;
		}
	}

	pthread_exit(NULL);

	return NULL;
}

void *MinPagesThread(void *arg)
{
	struct cfgoptions *s = arg;
	struct sysinfo infostruct;

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

	struct cfgoptions *s = arg;
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

	struct cfgoptions *s = arg;

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

			int fd = open(pidFilePathName, O_RDONLY|O_CLOEXEC);

			if (fd < 0) {
				Logmsg(LOG_ERR, "cannot open %s: %s",
				       pidFilePathName, strerror(errno));
				if (s->options & SOFTBOOT) {
					s->error |= PIDFILERROR;
					break;
				} else {
					continue;
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
				Logmsg(LOG_ERR,"strtol failed: %s", strerror(errno));
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

	struct cfgoptions *s = arg;
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
			if (Shutdown
			    (WESYSOVERLOAD,
			     s->options & KEXEC ? 1 : 0, arg) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & OUTOFMEMORY) {
			Logmsg(LOG_ERR,
			       "less than configured free pages available");
			if (Shutdown
			    (WEOTHER, s->options & KEXEC ? 1 : 0, arg) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & FORKFAILED) {
			Logmsg(LOG_ERR,
			       "process table test failed because fork failed");
			if (Shutdown
			    (WEOTHER, s->options & KEXEC ? 1 : 0, arg) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & SCRIPTFAILED) {
			Logmsg(LOG_ERR, "repair script failed");
			if (Shutdown
			    (WESCRIPT, s->options & KEXEC ? 1 : 0, arg) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->error & PIDFILERROR || s->error & UNKNOWNPIDFILERROR) {
			Logmsg(LOG_ERR, "pid file test failed");
			if (Shutdown
			    (WESCRIPT, s->options & KEXEC ? 1 : 0, arg) < 0) {
				Logmsg(LOG_ERR,
				       "watchdogd: Unable to shutdown system");
				exit(EXIT_FAILURE);
			}
		}

		if (s->testExeReturnValue > 0 || s->testExeReturnValue < 0) {
			Logmsg(LOG_ERR, "check executable failed");
			if (Shutdown
			    (s->testExeReturnValue,
			     s->options & KEXEC ? 1 : 0, arg) < 0) {
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
