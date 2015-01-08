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

#include "sub.h"
#include "main.h"
#include "init.h"
#include "configfile.h"
#include "threads.h"
#include "pidfile.h"
#include "daemon.h"

#define DISARM_WATCHDOG_BEFORE_REBOOT true

static volatile sig_atomic_t quit = 0;

volatile sig_atomic_t stop = 0;
volatile sig_atomic_t stopPing = 0;
ProcessList processes;

int main(int argc, char **argv)
{
	struct cfgoptions options;
	watchdog_t *watchdog = NULL;

	if (SetDefaultConfig(&options) == false) {
		return EXIT_FAILURE;
	}

	if (MyStrerrorInit() == false) {
		perror("Unable to create a new locale object");
		return EXIT_FAILURE;
	}

	const int ret = ParseCommandLine(&argc, argv, &options);

	if (ret < 0) {
		return EXIT_FAILURE;
	} else if (ret != 0) {
		return EXIT_SUCCESS;
	}

	if (ReadConfigurationFile(&options) < 0) {
		return EXIT_FAILURE;
	}

	if (Daemonize(&options) < 0) {
		FatalError(&options);
	}

	if (PingInit(&options) < 0) {
		return EXIT_FAILURE;
	}

	if (SetupSignalHandlers(IsDaemon(&options)) < 0) {
		FatalError(&options);
	}

	Logmsg(LOG_INFO, "starting daemon (%s)", PACKAGE_VERSION);

	PrintConfiguration(&options);

	if (StartHelperThreads(&options) != 0) {
		FatalError(&options);
	}

	if (!(options.options & NOACTION)) {
		watchdog = OpenWatchdog(options.devicepath);
		if (watchdog == NULL) {
			FatalError(&options);
		}

		if (ConfigureWatchdogTimeout(watchdog, options.watchdogTimeout)
		    < 0 && options.watchdogTimeout != -1) {
			Logmsg(LOG_ERR,
			       "unable to set watchdog device timeout\n");
			Logmsg(LOG_ERR, "program exiting\n");
			/*Can't use FatalError() because we need to shut down watchdog device after we open it */
			EndDaemon(&options, false);
			CloseWatchdog(watchdog);
			return EXIT_FAILURE;
		}

		if (options.sleeptime == -1) {
			options.sleeptime = GuessSleeptime(watchdog);
			Logmsg(LOG_INFO, "ping interval autodetect: %i",
			       options.sleeptime);
		}

		assert(options.sleeptime >= 1);

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
				return EXIT_FAILURE;
			}
		}
	} else {
		if (options.sleeptime == -1) {
			options.sleeptime = 60;
		}
	}

	assert(options.sleeptime >= 1);

	if (SetupAuxManagerThread(&options) < 0) {
		FatalError(&options);
	}

	if (PlatformInit() != true) {
		FatalError(&options);
	}

	struct timespec rqtp;

	clock_gettime(CLOCK_MONOTONIC, &rqtp);

	while (quit == 0 && stop == 0) {
		if (options.options & NOACTION) {
			assert(watchdog == NULL);
		} else {
			PingWatchdog(watchdog);
		}

		if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &rqtp, NULL)
		    != 0) {
			if (errno != 0) {
				Logmsg(LOG_ERR, "clock_nanosleep failed %s",
				       MyStrerror(errno));
			}
		}

		rqtp.tv_sec += options.sleeptime;
		NormalizeTimespec(&rqtp);
	}

	if (stop == 1) {
		while (true) {
			if (stopPing == 1) {
				if (DISARM_WATCHDOG_BEFORE_REBOOT) {
					CloseWatchdog(watchdog);
				} else {
					close(GetFd(watchdog));
				}
			} else {
				PingWatchdog(watchdog);
			}

			sleep(1);
		}
	}

	CloseWatchdog(watchdog);

	DeletePidFile(&options.pidfile);

	if (EndDaemon(&options, false) < 0) {
		return EXIT_FAILURE;
	}

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
	Logmsg(LOG_ERR, "sigaction failed: %s:", MyStrerror(errno));
	return sig;
}

int SetupSignalHandlers(int isDaemon)
{
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	sigemptyset(&act.sa_mask);
	act.sa_handler = SignalHandler;

	int ret = InstallSignalAction(&act, SIGTERM, SIGINT, 0);

	if (ret != 0) {
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

static void SignalHandler(int signum)
{
	quit = 1;
}

static void PrintConfiguration(struct cfgoptions *const cfg)
{
	Logmsg(LOG_INFO,
	       "int=%is realtime=%s sync=%s softboot=%s force=%s mla=%.2f mem=%li",
	       cfg->sleeptime, cfg->options & REALTIME ? "yes" : "no",
	       cfg->options & SYNC ? "yes" : "no",
	       cfg->options & SOFTBOOT ? "yes" : "no",
	       cfg->options & FORCE ? "yes" : "no", cfg->maxLoadOne,
	       cfg->minfreepages);

	if (cfg->options & ENABLEPING) {
		for (int cnt = 0; cnt < config_setting_length(cfg->ipAddresses);
		     cnt++) {
			const char *ipAddress =
			    config_setting_get_string_elem(cfg->ipAddresses,
							   cnt);

			assert(ipAddress != NULL);

			Logmsg(LOG_INFO, "ping: %s", ipAddress);
		}
	} else {
		Logmsg(LOG_INFO, "ping: no ip adresses to ping");
	}

	if (cfg->options & ENABLEPIDCHECKER) {
		for (int cnt = 0; cnt < config_setting_length(cfg->pidFiles);
		     cnt++) {
			const char *pidFilePathName =
			    config_setting_get_string_elem(cfg->pidFiles, cnt);

			assert(pidFilePathName != NULL);

			Logmsg(LOG_DEBUG, "pidfile: %s", pidFilePathName);
		}
	} else {
		Logmsg(LOG_INFO, "pidfile: no server process to check");
	}

	Logmsg(LOG_INFO, "test=%s(%i) repair=%s(%i) no_act=%s",
	       cfg->testexepathname == NULL ? "no" : cfg->testexepathname,
	       cfg->testBinTimeout,
	       cfg->exepathname == NULL ? "no" : cfg->exepathname,
	       cfg->repairBinTimeout, cfg->options & NOACTION ? "yes" : "no");
}
