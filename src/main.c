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

#include "watchdogd.h"

#include "sub.h"
#include "main.h"
#include "init.h"
#include "threads.h"

static volatile sig_atomic_t quit = 0;

volatile sig_atomic_t shutdown = 0;
bool logToSyslog = false;
struct parent parent;

int main(int argc, char **argv)
{
	struct cfgoptions options;
	watchdog_t *watchdog;

	if (SetDefaultConfig(&options) == false) {
		return EXIT_FAILURE;
	}

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

	if (StartHelperThreads(&options) != 0) {
		Abend(&options);
	}

	if ((options.options & NOACTION) == 0) {
		watchdog = OpenWatchdog(options.devicepath);
		if (watchdog == NULL) {
			Abend(&options);
		}

		if (ConfigureWatchdogTimeout(watchdog, options.watchdogTimeout)
		    < 0 && options.watchdogTimeout != -1
		    && IsDaemon(&options) == 0) {
			fprintf(stderr,
				"unable to set watchdog device timeout\n");
			fprintf(stderr, "program exiting\n");
			/*Can't use Abend() because we need to shut down watchdog device after we open it */
			EndDaemon(CloseWatchdog(watchdog), &options, false);
			exit(EXIT_FAILURE);
		} else {
			if (ConfigureWatchdogTimeout
			    (watchdog, options.watchdogTimeout) < 0
			    && options.watchdogTimeout != -1) {
				Logmsg(LOG_ERR,
				       "unable to set watchdog device timeout");
				Logmsg(LOG_ERR, "program exiting");
				EndDaemon(CloseWatchdog(watchdog), &options,
					  false);
				exit(EXIT_FAILURE);
			}
		}

		if (options.watchdogTimeout != -1
		    && CheckWatchdogTimeout(watchdog,
					    options.sleeptime) == true) {
			Logmsg(LOG_ERR,
			       "WDT timeout is less than or equal watchdog daemon ping interval");
			Logmsg(LOG_ERR,
			       "Using this interval may result in spurious reboots");

			if (!(options.options & FORCE)) {
				CloseWatchdog(watchdog);
				Logmsg(LOG_WARNING,
				       "use the -f option to force this configuration");
				Abend(&options);
			}
		}
	}

	if (SetupAuxManagerThread(&options) < 0) {
		Abend(&options);
	}

	struct timespec rqtp;

	clock_gettime(CLOCK_MONOTONIC, &rqtp);

	while (quit == 0) {
		if ((options.options & NOACTION) == 0) {
			PingWatchdog(watchdog);
		}

		if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &rqtp, NULL)
		    != 0) {
			Logmsg(LOG_ERR, "clock_nanosleep failed %s",
			       strerror(errno));
		}

		rqtp.tv_sec += options.sleeptime;
		NormalizeTimespec(&rqtp);
	}

	if (EndDaemon
	    ((options.options & NOACTION) == 0 ? CloseWatchdog(watchdog) : 0,
	     &options, false) < 0) {
		DeletePidFile(&options);
		exit(EXIT_FAILURE);
	}

	DeletePidFile(&options);

	return EXIT_SUCCESS;
}

static int InstallSignalAction(struct sigaction *act, ...)
{
	va_list ap;
	struct sigaction oact;
	sigemptyset(&oact.sa_mask);
	va_start(ap, act);

	assert(act != NULL);

	int sig = 0;

	while ((sig = va_arg(ap, int)) != 0) {
		if (sigaction(sig, NULL, &oact) < 0) {
			goto error;
		}

		if (oact.sa_handler != SIG_IGN) {
			if (sigaction(sig, act, NULL) < 0) {
				goto error;
			}
		}
	}

	va_end(ap);

	return 0;
error:
	va_end(ap);
	assert(sig != 0);
	Logmsg(LOG_ERR, "sigaction failed: %s:", strerror(errno));
	return sig;
}

int SetupSignalHandlers(int isDaemon)
{
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_handler = SignalHandler;
	act.sa_flags = SA_SIGINFO;

	int ret = InstallSignalAction(&act, SIGTERM, SIGINT, 0);

	if (ret !=  0) {
		return -1;
	}

	if (isDaemon) {
		struct sigaction IgnoredSignals;
		IgnoredSignals.sa_handler = SIG_IGN;
		IgnoredSignals.sa_flags = 0;
		sigemptyset(&IgnoredSignals.sa_mask);

		ret = InstallSignalAction(&IgnoredSignals, SIGHUP, 0);
		if (ret != 0) {
			return -1;
		}
	}

	return 0;
}

void SignalHandler(int signum)
{
	quit = 1;
	return;
}

void Abend(struct cfgoptions *s)
{
	assert(s != NULL);

	Logmsg(LOG_INFO, "stopping watchdog daemon");

	config_destroy(&s->cfg);

	DeletePidFile(s);

	exit(EXIT_FAILURE);
}

static void PrintConfiguration(struct cfgoptions *s)
{
	Logmsg(LOG_INFO,
	       "int=%is realtime=%s sync=%s softboot=%s force=%s mla=%.2f mem=%li",
	       s->sleeptime, s->options & REALTIME ? "yes" : "no",
	       s->options & SYNC ? "yes" : "no",
	       s->options & SOFTBOOT ? "yes" : "no",
	       s->options & FORCE ? "yes" : "no", s->maxLoadOne,
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
