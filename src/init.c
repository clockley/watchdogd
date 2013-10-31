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

#include <config.h>
#include <libconfig.h>
#include "init.h"
#include "watchdogd.h"
#include "sub.h"
#include "testdir.h"

int SetSchedulerPolicy(int priority)
{
	struct sched_param param;
	param.sched_priority = priority;

	if (sched_setscheduler(getpid(), SCHED_RR, &param) < 0) {
		fprintf(stderr, "watchdogd: sched_setscheduler  failed %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

int InitializePosixMemlock(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int LoadConfigurationFile(void *arg)
{
	assert(arg != NULL);

	struct cfgoptions *s = arg;
	int tmp = 0;

	config_init(&s->cfg);

	if (!config_read_file(&s->cfg, s->confile)
	    && config_error_file(&s->cfg) == NULL) {
		fprintf(stderr,
			"watchdogd: cannot open configuration file: %s\n",
			config_error_file(&s->cfg));
		config_destroy(&s->cfg);
		return -1;
	} else if (!config_read_file(&s->cfg, s->confile)) {
		fprintf(stderr, "watchdogd: %s:%d: %s\n",
			config_error_file(&s->cfg),
			config_error_line(&s->cfg), config_error_text(&s->cfg));
		config_destroy(&s->cfg);
		return -1;
	}

	if (config_lookup_string(&s->cfg, "repair-binary", &s->exepathname) ==
	    CONFIG_FALSE) {
		s->exepathname = NULL;
	}

	if (s->exepathname != NULL && IsExe(s->exepathname, false) < 0) {
		fprintf(stderr, "watchdogd: %s: Invalid executeable image\n",
			s->exepathname);
		fprintf(stderr, "watchdogd: ignoring repair-binary option\n");
		s->exepathname = NULL;
	}

	if (config_lookup_string(&s->cfg, "test-binary", &s->testexepathname) ==
	    CONFIG_FALSE) {
		s->testexepathname = NULL;
	}

	if (s->exepathname != NULL && IsExe(s->testexepathname, false) < 0) {
		fprintf(stderr, "watchdogd: %s: Invalid executeable image\n",
			s->testexepathname);
		fprintf(stderr, "watchdogd: ignoring test-binary option\n");
		s->testexepathname = NULL;
	}

	if (config_lookup_string(&s->cfg, "test-directory", &s->testexepath) ==
	    CONFIG_FALSE) {
		s->testexepath = "/etc/watchdog.d";
	}

	if (CreateLinkedListOfExes(s->testexepath, &parent) < 0) {
		fprintf(stderr, "watchdogd: CreateLinkedListOfExes failed\n");
		return -1;
	}

	if (config_lookup_string(&s->cfg, "watchdog-device", &s->devicepath) ==
	    CONFIG_FALSE) {
		s->devicepath = "/dev/watchdog";
	}

	if (config_lookup_int(&s->cfg, "min-memory", &tmp) == CONFIG_TRUE) {
		if (tmp < 0) {
			fprintf(stderr,
				"illegal value for configuration file"
				" entry named \"min-memory\"\n");
			fprintf(stderr,
				"disabled memory free page monitoring\n");
			s->minfreepages = 0;
		} else {
			s->minfreepages = (unsigned long)tmp;
		}
	}

	if (config_lookup_string(&s->cfg, "pid-pathname", &s->pidpathname) ==
	    CONFIG_FALSE) {
		s->pidpathname = "/var/run/watchdogd.pid";
	}

	if (config_lookup_string(&s->cfg, "log-dir", &s->logdir) ==
	    CONFIG_FALSE) {
		s->logdir = "/var/log/watchdogd";
	}

	if (MakeLogDir(arg) < 0)
		return -1;

	if (config_lookup_bool(&s->cfg, "daemonize", &tmp) == CONFIG_TRUE
	    && !(s->options & FOREGROUNDSETFROMCOMMANDLINE)) {
		if (tmp) {
			s->options |= DAEMONIZE;
		}
	} else {
		s->options |= DAEMONIZE;
	}


	if (!s->options & SYNC) {
		if (config_lookup_bool(&s->cfg, "sync", &tmp) == CONFIG_TRUE) {
			if (tmp) {
				s->options |= SYNC;
			} /*no need to unset if false because all options are false by default*/
		}
	}

	if (config_lookup_bool(&s->cfg, "use-kexec", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			s->options |= KEXEC;
		}
	}

	if (config_lookup_bool(&s->cfg, "logtick", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			if (config_lookup_int(&s->cfg, "logtick-interval",
					      &s->logtickinterval) ==
			    CONFIG_TRUE) {
				if (s->logtickinterval < 0
				    || s->logtickinterval > 7200
				    || s->logtickinterval == 0) {
					fprintf(stderr,
						"illegal value for configuration file entry named \"logtick-interval\"\n");
					fprintf(stderr,
						"watchdogd: using default value\n");
					s->options |= LOGTICK;
					s->logtickinterval = 1800;
				} else {
					s->options |= LOGTICK;
				}
			}
		}
	}

	if (config_lookup_bool(&s->cfg, "lock-memory", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			if (InitializePosixMemlock() < 0) {
				return -1;
			}
		}
	}

	if (config_lookup_bool(&s->cfg, "use-pid-file", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			s->options |= USEPIDFILE;
		}
	}

	if (config_lookup_bool(&s->cfg, "softboot", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			s->options |= SOFTBOOT;
		}
	}

	if (config_lookup_int(&s->cfg, "realtime-priority", &tmp) ==
	    CONFIG_TRUE) {
		if (CheckPriority(tmp) < 0) {
			fprintf(stderr,
				"illegal value for configuration file entry named \"realtime-priority\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
			s->priority = 1;
		} else {
			s->priority = tmp;
		}
	}

	config_set_auto_convert(&s->cfg, true);

	if (config_lookup_float(&s->cfg, "max-load-1", &s->maxLoadOne) ==
	    CONFIG_TRUE) {
		if (s->maxLoadOne < 0 || s->maxLoadOne > 100L) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"max-load-1\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
			s->maxLoadOne = 0L;
		}
	}

	if (config_lookup_float(&s->cfg, "max-load-5", &s->maxLoadFive) ==
	    CONFIG_TRUE) {
		if (s->maxLoadFive <= 0 || s->maxLoadFive > 100L) {
			if (s->maxLoadFive != 0) {
				fprintf(stderr,
					"watchdogd: illegal value for"
					" configuration file entry named \"max-load-5\"\n");
			}

			fprintf(stderr, "watchdogd: using default value\n");

			s->maxLoadFive = s->maxLoadOne * 0.75;
		}
	} else {
		s->maxLoadFive = s->maxLoadOne * 0.75;
	}

	if (config_lookup_float(&s->cfg, "max-load-15", &s->maxLoadFifteen) ==
	    CONFIG_TRUE) {
		if (s->maxLoadFifteen <= 0 || s->maxLoadFifteen > 100L) {

			if (s->maxLoadFifteen != 0) {
				fprintf(stderr,
					"watchdogd: illegal value for"
					" configuration file entry named \"max-load-15\"\n");
			}

			fprintf(stderr, "watchdogd: using default value\n");
			s->maxLoadFifteen = s->maxLoadOne * 0.5;
		}
	} else {
		s->maxLoadFifteen = s->maxLoadOne * 0.5;
	}

	config_set_auto_convert(&s->cfg, false);

	if (config_lookup_bool(&s->cfg, "realtime-scheduling", &tmp)) {
		if (tmp) {
			if (SetSchedulerPolicy(s->priority) < 0) {
				return -1;
			}
		}
	}

	if (config_lookup_int(&s->cfg, "watchdog-timeout", &tmp) == CONFIG_TRUE) {
		if (tmp < 0 || tmp > 60 || tmp == 0) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"watchdog-timeout\"\n");
			fprintf(stderr, "watchdogd: using device default\n");
			s->watchdogTimeout = -1;
		} else {
			s->watchdogTimeout = tmp;
		}
	}

	if (config_lookup_int(&s->cfg, "repair-timeout", &tmp) == CONFIG_TRUE) {
		if (tmp < 0 || tmp > 499999) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"repair-timeout\"\n");
			fprintf(stderr,
				"watchdogd: disabled repair binary timeout\n");
			s->repairBinTimeout = 0;
		} else {
			s->repairBinTimeout = tmp;
		}
	}

	if (config_lookup_int(&s->cfg, "test-timeout", &tmp) == CONFIG_TRUE) {
		if (tmp < 0 || tmp > 499999) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"test-timeout\"\n");
			fprintf(stderr,
				"watchdogd: disabled test binary timeout\n");
			s->testBinTimeout = 0;
		} else {
			s->testBinTimeout = tmp;
		}
	}

	if (config_lookup_int(&s->cfg, "interval", &tmp) == CONFIG_TRUE) {
		if (tmp < 0 || tmp > 60 || tmp == 0) {
			tmp = 1;
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"interval\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
		} else {
			s->sleeptime = (time_t) tmp;
		}
	}

	s->pidFiles = config_lookup(&s->cfg, "pid-files");

	if (s->pidFiles != NULL) {
		if (config_setting_is_array(s->pidFiles) == CONFIG_FALSE) {
			fprintf(stderr,
				"watchdogd: %s:%i: illegal type for configuration file entry"
				" \"pid-files\" expected array\n",
				LibconfigWraperConfigSettingSourceFile(s->
								       pidFiles),
				config_setting_source_line(s->pidFiles));
			return -1;
		}

		if (config_setting_length(s->pidFiles) > 0) {
			s->options |= ENABLEPIDCHECKER;
		}
	}

	return 0;
}

const char
*LibconfigWraperConfigSettingSourceFile(const config_setting_t * setting)
{
	const char *fileName = config_setting_source_file(setting);

	if (fileName == NULL)
		return "(NULL)";

	return fileName;
}

int OpenAndWriteToPidFile(void *arg)
{
	struct cfgoptions *s = arg;
	extern struct flock fl;
	mode_t oumask = 0;

	if (s == NULL) {
		return -1;
	}

	oumask = umask(0027);

	errno = 0;

	s->lockfd =
	    open(s->pidpathname, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);

	if (s->lockfd < 0) {
		fprintf(stderr, "watchdogd: unable to open pid file.\n");
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		umask(oumask);
		return -1;
	}

	errno = 0;

	if (fcntl(s->lockfd, F_SETLKW, &fl) != 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		umask(oumask);
		return -1;
	}

	errno = 0;

	if (dprintf(s->lockfd, "%d\n", getpid()) < 0) {
		fprintf(stderr, "watchdogd: unable to write pid to %s: %s\n",
			s->pidpathname, strerror(errno));
	}

	umask(oumask);

	return 0;
}

int ParseCommandLine(int *argc, char **argv, void *arg)
{
	struct cfgoptions *s = arg;
	int opt = 0;

	while ((opt = getopt(*argc, argv, "qsFc:")) != -1) {
		switch (opt) {
		case 'F':
			s->options |= FOREGROUNDSETFROMCOMMANDLINE;
			break;
		case 'c':
			s->confile = optarg;
			break;
		case 's':
			s->options |= SYNC;
			break;
		case 'q':
			s->options |= NOACTION;
			break;
		default:
			Usage();
			return -1;
		}
	}

	return 0;
}

int MakeLogDir(void *arg)
{
	struct cfgoptions *s = arg;

	errno = 0;
	if (mkdir(s->logdir, 0750) != 0) {
		if (errno != EEXIST) {
			Logmsg(LOG_ERR, "watchdog: %s", strerror(errno));
			return -1;
		} else {
			return 0;
		}
	}

	return 0;
}

int CheckPriority(int priority)
{
	int max = 0;
	int min = 0;

	max = sched_get_priority_max(SCHED_RR);

	if (max < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		return -1;
	}

	min = sched_get_priority_min(SCHED_RR);

	if (min < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		return -1;
	}

	if (priority < min) {
		return -2;
	}

	if (priority > max) {
		return -2;
	}

	return 0;
}

int PrintVersionString(void)
{
	printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright 2013 Christian Lockley. All rights reserved.\n");
	return 0;
}

int Usage(void)
{
	PrintVersionString();
	printf("%s [-F] [-c <config_file>]\n", PACKAGE_NAME);
	return 0;
}
