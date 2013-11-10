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

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <inttypes.h>
#include <stdint.h>
#include <config.h>

#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <getopt.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "watchdogd.h"
#include "sub.h"
#include "main.h"
#include "init.h"
#include "threads.h"

#ifdef __linux__
#include "linux.h"
#endif

static volatile sig_atomic_t quit = 0;

struct flock fl;

volatile sig_atomic_t shutdown = 0;

bool logToSyslog = false;

int main(int argc, char **argv)
{
	static struct cfgoptions options = {.confile =
		    "/etc/watchdogd.conf",.priority = 16,
		.pidpathname = "/var/run/watchdogd.pid",.sleeptime =
		    1,.watchdogTimeout = -1,
		.logtickinterval = 1800,.maxLoadOne = 0,.maxLoadFive =
		    0,.maxLoadFifteen = 0,
		.minfreepages = 0,.testExeReturnValue = 0,
		.repairBinTimeout = 0,
		.testBinTimeout = 0,.options = 0,.error = 0
	};

	int fd = 0;

	if (ParseCommandLine(&argc, argv, &options) != 0) {
		return EXIT_FAILURE;
	}

	if (LoadConfigurationFile(&options) < 0) {
		Abend(&options);
	}

	if (IsDaemon(&options) && Daemon(&options) < 0) {
		Abend(&options);
	}

	if (SetupSignalHandlers(IsDaemon(&options)) < 0) {
		Abend(&options);
	}

	Logmsg(LOG_INFO, "starting daemon (%s)", PACKAGE_VERSION);

	PrintConfiguration(&options);

	if (ConfigureKernelOutOfMemoryKiller() < 0) {
		Logmsg(LOG_ERR, "unable to configure out of memory killer");
	}

	if (SetupTestFork(&options) < 0) {
		Abend(&options);
	}

	if (SetupExeDir(&options) < 0) {
		Abend(&options);
	}

	if (options.testexepathname != NULL) {
		if (SetupTestBinThread(&options) < 0) {
			Abend(&options);
		}
	}

	if (options.options & LOGTICK)
		if (SetupLogTick(&options) < 0)
			Abend(&options);

	if (options.maxLoadOne > 0) {
		if (SetupLoadAvgThread(&options) < 0) {
			Abend(&options);
		}
	}

	if (options.options & SYNC) {
		if (SetupSyncThread(&options) < 0) {
			Abend(&options);
		}
	}

	if (options.minfreepages != 0) {
		if (SetupMinPagesThread(&options) < 0) {
			Abend(&options);
		}
	}

	if (options.options & ENABLEPIDCHECKER) {
		if (StartPidFileTestThread(&options) < 0) {
			Abend(&options);
		}
	}

	if ((options.options & NOACTION) == 0) {
		if (OpenWatchdog(&fd, options.devicepath) < 0) {
			Abend(&options);
		}

		if (ConfigureWatchdogTimeout(&fd, &options) < 0
		    && options.watchdogTimeout != -1 && IsDaemon(&options) == 0)
		{
			fprintf(stderr,
				"unable to set watchdog device timeout\n");
			fprintf(stderr, "program exiting\n");
			/*Can't use Abend() because we need to shut down watchdog device after we open it */
			EndDaemon(CloseWatchdog(&fd), &options, false);
			exit(EXIT_FAILURE);
		} else if (ConfigureWatchdogTimeout(&fd, &options) < 0
			   && options.watchdogTimeout != -1) {
			Logmsg(LOG_ERR,
			       "unable to set watchdog device timeout");
			Logmsg(LOG_ERR, "program exiting");
			EndDaemon(CloseWatchdog(&fd), &options, false);
			exit(EXIT_FAILURE);
		}

		if (options.watchdogTimeout != -1 && CheckDeviceAndDaemonTimeout
		    (NULL, options.watchdogTimeout, options.sleeptime) < 0) {
			Logmsg(LOG_ERR,
			       "WDT timeout is less than watchdog daemon ping interval");
			Logmsg(LOG_ERR,
			       "Using this interval may result in spurious reboots");
		}
	}

	if (SetupAuxManagerThread(&options) < 0) {
		Abend(&options);
	}

	struct timespec rqtp;

	clock_gettime(CLOCK_MONOTONIC, &rqtp);

	while (quit == 0) {
		if ((options.options & NOACTION) == 0) {
			PingWatchdog(&fd);
		}

		if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &rqtp, NULL) != 0) {
			Logmsg(LOG_ERR, "clock_nanosleep failed %s", strerror(errno));
		}

		rqtp.tv_sec += options.sleeptime;
		NormalizeTimespec(&rqtp);
	}

	if (EndDaemon
	    ((options.options & NOACTION) == 0 ? CloseWatchdog(&fd) : 0,
	     &options, false) < 0) {
		DeletePidFile(&options);
		exit(EXIT_FAILURE);
	}

	DeletePidFile(&options);

	exit(EXIT_SUCCESS);

	return EXIT_SUCCESS;
}

int SetupSignalHandlers(int isDaemon)
{
	struct sigaction IgnoredSignals = {.sa_handler = SIG_IGN,.sa_flags = 0
	};

	sigemptyset(&IgnoredSignals.sa_mask);

	struct sigaction act;

	sigemptyset(&act.sa_mask);

	sigaddset(&act.sa_mask, SIGINT);
	sigaddset(&act.sa_mask, SIGTERM);
	sigaddset(&act.sa_mask, SIGCHLD);

	act.sa_handler = SignalHandler;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGTERM, &act, NULL) < 0) {
		Logmsg(LOG_ERR, "sigaction failed: %s", strerror(errno));
		return -1;
	}

	if (sigaction(SIGINT, &act, NULL)) {
		Logmsg(LOG_ERR, "sigaction failed: %s", strerror(errno));
		return -1;
	}

	if (isDaemon) {
		sigaddset(&IgnoredSignals.sa_mask, SIGHUP);
		sigaction(SIGHUP, &IgnoredSignals, NULL);
	}

	return 0;
}

void SignalHandler(int signum)
{
	quit = 1;
	return;
	(void)signum;
}

void Abend(void *arg)
{
	struct cfgoptions *s = arg;

	Logmsg(LOG_INFO, "stopping watchdog daemon");

	if (s == NULL) {
		exit(EXIT_FAILURE);
	}

	config_destroy(&s->cfg);

	DeletePidFile(s);

	exit(EXIT_FAILURE);
}

int
CheckDeviceAndDaemonTimeout(const int *fd, int deviceTimeout,
			    int daemonSleepTime)
{
	(void)fd;
	if (daemonSleepTime >= deviceTimeout)
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
	struct cfgoptions *s = arg;

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

static void PrintConfiguration(void *arg)
{
	struct cfgoptions *s = arg;

	Logmsg(LOG_INFO,
	       "int=%is realtime=%s sync=%s softboot=%s force=%s mla=%.2f mem=%li",
	       s->sleeptime, s->options & REALTIME ? "yes" : "no",
	       s->options & SYNC ? "yes" : "no",
	       s->options & SOFTBOOT ? "yes" : "no", "yes", s->maxLoadOne,
	       s->minfreepages);

	if (s->options & ENABLEPIDCHECKER) {
		for (int cnt = 0; cnt < config_setting_length(s->pidFiles);
		     cnt++) {
			const char *pidFilePathName = NULL;
			pidFilePathName =
			    config_setting_get_string_elem(s->pidFiles, cnt);

			Logmsg(LOG_DEBUG, "pidfile: %s", pidFilePathName);
		}
	} else {
		Logmsg(LOG_INFO, "pidfile: no server process to check");
	}

	Logmsg(LOG_INFO, "test=%s(%i) repair=%s(%i) no_act=%s",
	       s->testexepathname == NULL ? "no" : s->testexepathname,
	       s->testBinTimeout,
	       s->exepathname == NULL ? "no" : s->exepathname,
	       s->repairBinTimeout, s->options & NOACTION ? "yes" : "no");
}
