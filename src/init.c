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
#include "init.h"
#include "sub.h"
#include "testdir.h"

int SetSchedulerPolicy(int priority)
{
	struct sched_param param;
	param.sched_priority = priority;

	if (sched_setscheduler(0, SCHED_RR, &param) < 0) {
		assert(errno != ESRCH);
		fprintf(stderr, "watchdogd: sched_setscheduler failed %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

int InitializePosixMemlock(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
		fprintf(stderr, "watchdogd: unable to lock memory %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static const char *LibconfigWraperConfigSettingSourceFile(const config_setting_t *
						   setting)
{
	const char *fileName = config_setting_source_file(setting);

	if (fileName == NULL)
		return "(NULL)";

	return fileName;
}

static bool SetDefaultLogTarget(struct cfgoptions *const cfg)
{
	assert(cfg != NULL);

	errno = 0;

	if (strcmp(cfg->logTarget, "auto") == 0) {
		if (IsDaemon(cfg)) {
			SetLogTarget(systemLog);
			return true;
		} else {
			SetLogTarget(standardError);
			return true;
		}
	}

	if (strcmp(cfg->logTarget, "syslog") == 0) {
		SetLogTarget(systemLog);
		return true;
	}

	if (strcmp(cfg->logTarget, "stderr") == 0) {
		SetLogTarget(standardError);
		return true;
	}

	if (strchr(cfg->logTarget, ':') == NULL) {
		 return false;
	}

	char *tmp;

	char *copyOfLogTarget = strdup(cfg->logTarget);

	if (copyOfLogTarget == NULL) {
		fprintf(stderr, "%s\n", strerror(errno));
		return false;
	}

	char * mode = strtok_r(copyOfLogTarget, ":", &tmp);

	if (mode == NULL) {
		goto error;
	}

	if (strcmp(mode, "file") == 0) {
		char * fileName = strtok_r(NULL, ":", &tmp);

		if (fileName == NULL) {
			goto error;
		}

		FILE* fp = fopen(fileName, "a");
		if (fp == NULL) {
			goto error;
		}

		struct stat buf;

		if (fstat(fileno(fp), &buf) < 0) {
			fclose(fp);
			goto error;
		}

		fclose(fp);

		if (S_ISDIR(buf.st_mode)
			|| S_ISCHR(buf.st_mode)
			|| S_ISBLK(buf.st_mode)
			|| S_ISFIFO(buf.st_mode)
			|| S_ISSOCK(buf.st_mode)
			|| S_ISCHR(buf.st_mode) || !S_ISREG(buf.st_mode)) {
				fprintf(stderr, "Invalid file type\n");
				goto error;
		}

		SetLogTarget(file, fileName);
	} else if (strcmp(mode, "newfile") == 0) {
		char * fileName = strtok_r(NULL, ":", &tmp);

		if (fileName == NULL) {
			goto error;
		}

		FILE* fp = fopen(fileName, "w");
		if (fp == NULL) {
			goto error;
		}

		struct stat buf;

		if (fstat(fileno(fp), &buf) < 0) {
			fclose(fp);
			goto error;
		}

		fclose(fp);

		if (S_ISDIR(buf.st_mode)
			|| S_ISCHR(buf.st_mode)
			|| S_ISBLK(buf.st_mode)
			|| S_ISFIFO(buf.st_mode)
			|| S_ISSOCK(buf.st_mode)
			|| S_ISCHR(buf.st_mode) || !S_ISREG(buf.st_mode)) {
				fprintf(stderr, "Invalid file type\n");
				goto error;
		}

		SetLogTarget(newFile, fileName);
	} else {
		goto error;
	}

	if (copyOfLogTarget != NULL) {
		free(copyOfLogTarget);
	}

	return true;
 error:
	if (errno != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}

	if (copyOfLogTarget != NULL) {
		free(copyOfLogTarget);
	}

	return false;
}

static bool LoadConfigurationFile(config_t *const config, const char *const fileName)
{
	assert(config != NULL);
	assert(fileName != NULL);

	int tmp = 0;

	config_init(config);

	if (config == NULL) {
		return false;
	}

	if (!config_read_file(config, fileName)
	    && config_error_file(config) == NULL) {
		fprintf(stderr,
			"watchdogd: cannot open configuration file: %s\n",
			fileName);
		config_destroy(config);
		return false;
	} else if (!config_read_file(config, fileName)) {
		fprintf(stderr, "watchdogd: %s:%d: %s\n",
			config_error_file(config),
			config_error_line(config),
			config_error_text(config));
		config_destroy(config);
		return false;
	}

	return true;
}

int ReadConfigurationFile(struct cfgoptions *const cfg)
{
	assert(cfg != NULL);

	int tmp = 0;

	if (LoadConfigurationFile(&cfg->cfg, cfg->confile) == false) {
		return -1;
	}

	if (config_lookup_string(&cfg->cfg, "repair-binary", &cfg->exepathname)
	    == CONFIG_FALSE) {
		cfg->exepathname = NULL;
	}

	if (cfg->exepathname != NULL && IsExe(cfg->exepathname, false) < 0) {
		fprintf(stderr, "watchdogd: %s: Invalid executeable image\n",
			cfg->exepathname);
		fprintf(stderr, "watchdogd: ignoring repair-binary option\n");
		cfg->exepathname = NULL;
	}

	if (config_lookup_string
	    (&cfg->cfg, "test-binary", &cfg->testexepathname) == CONFIG_FALSE) {
		cfg->testexepathname = NULL;
	}

	if (cfg->testexepathname != NULL
	    && IsExe(cfg->testexepathname, false) < 0) {
		fprintf(stderr, "watchdogd: %s: Invalid executeable image\n",
			cfg->testexepathname);
		fprintf(stderr, "watchdogd: ignoring test-binary option\n");
		cfg->testexepathname = NULL;
	}

	if (config_lookup_string(&cfg->cfg, "test-directory", &cfg->testexepath)
	    == CONFIG_FALSE) {
		cfg->testexepath = "/etc/watchdog.d";
	}

	if (config_lookup_string(&cfg->cfg, "random-seed", &cfg->randomSeedPath)
	    == CONFIG_FALSE) {
		cfg->randomSeedPath = GetDefaultRandomSeedPathName();
	}

	if (CreateLinkedListOfExes(cfg->testexepath, &processes) < 0) {
		fprintf(stderr, "watchdogd: CreateLinkedListOfExes failed\n");
		return -1;
	}

	if (config_lookup_string(&cfg->cfg, "watchdog-device", &cfg->devicepath)
	    == CONFIG_FALSE) {
		cfg->devicepath = "/dev/watchdog";
	}

	if (config_lookup_string(&cfg->cfg, "log-target", &cfg->logTarget) == CONFIG_FALSE) {
		cfg->logTarget = "auto";
	}

	if (SetDefaultLogTarget(cfg) == false) {
		fprintf(stderr, "Invalid log target\n");
		return -1;
	}

	if (config_lookup_int(&cfg->cfg, "min-memory", &tmp) == CONFIG_TRUE) {
		if (tmp < 0) {
			fprintf(stderr,
				"illegal value for configuration file"
				" entry named \"min-memory\"\n");
			fprintf(stderr,
				"disabled memory free page monitoring\n");
			cfg->minfreepages = 0;
		} else {
			cfg->minfreepages = (unsigned long)tmp;
		}
	}

	if (config_lookup_string(&cfg->cfg, "pid-pathname", &cfg->pidfile.name)
	    == CONFIG_FALSE) {
		cfg->pidfile.name = "/var/run/watchdogd.pid";
	}

	if (config_lookup_string(&cfg->cfg, "log-dir", &cfg->logdir) ==
	    CONFIG_FALSE) {
		cfg->logdir = "/var/log/watchdogd";
	}

	if (MakeLogDir(cfg) < 0)
		return -1;

	if (config_lookup_bool(&cfg->cfg, "daemonize", &tmp) == CONFIG_TRUE) {
		if (!tmp) {
			cfg->options &= !DAEMONIZE;
		}
	}

	if (config_lookup_bool(&cfg->cfg, "force", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			cfg->options |= FORCE;
		}
	}

	if (config_lookup_bool(&cfg->cfg, "sync", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			cfg->options |= SYNC;
		}
	}

	if (config_lookup_bool(&cfg->cfg, "use-kexec", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			cfg->options |= KEXEC;
		}
	}

	if (config_lookup_bool(&cfg->cfg, "lock-memory", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			if (InitializePosixMemlock() < 0) {
				return -1;
			}
		}
	} else {
		InitializePosixMemlock();
	}

	if (config_lookup_bool(&cfg->cfg, "use-pid-file", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			cfg->options |= USEPIDFILE;
		} else {
			cfg->options &= !USEPIDFILE;
		}
	}

	if (config_lookup_bool(&cfg->cfg, "softboot", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			cfg->options |= SOFTBOOT;
		}
	}

	if (config_lookup_int(&cfg->cfg, "realtime-priority", &tmp) ==
	    CONFIG_TRUE) {
		if (CheckPriority(tmp) < 0) {
			fprintf(stderr,
				"illegal value for configuration file entry named \"realtime-priority\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
			cfg->priority = GetDefaultPriority();
		} else {
			cfg->priority = tmp;
		}
	} else {
		cfg->priority = GetDefaultPriority();
	}

	config_set_auto_convert(&cfg->cfg, true);

	if (config_lookup_float(&cfg->cfg, "max-load-1", &cfg->maxLoadOne) ==
	    CONFIG_TRUE) {
		if (cfg->maxLoadOne < 0 || cfg->maxLoadOne > 100L) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"max-load-1\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
			cfg->maxLoadOne = 0L;
		}
	}

	if (config_lookup_float(&cfg->cfg, "max-load-5", &cfg->maxLoadFive) ==
	    CONFIG_TRUE) {
		if (cfg->maxLoadFive <= 0 || cfg->maxLoadFive > 100L) {
			if (cfg->maxLoadFive != 0) {
				fprintf(stderr,
					"watchdogd: illegal value for"
					" configuration file entry named \"max-load-5\"\n");
			}

			fprintf(stderr, "watchdogd: using default value\n");

			cfg->maxLoadFive = cfg->maxLoadOne * 0.75;
		}
	} else {
		cfg->maxLoadFive = cfg->maxLoadOne * 0.75;
	}

	if (config_lookup_float(&cfg->cfg, "max-load-15", &cfg->maxLoadFifteen)
	    == CONFIG_TRUE) {
		if (cfg->maxLoadFifteen <= 0 || cfg->maxLoadFifteen > 100L) {

			if (cfg->maxLoadFifteen != 0) {
				fprintf(stderr,
					"watchdogd: illegal value for"
					" configuration file entry named \"max-load-15\"\n");
			}

			fprintf(stderr, "watchdogd: using default value\n");
			cfg->maxLoadFifteen = cfg->maxLoadOne * 0.5;
		}
	} else {
		cfg->maxLoadFifteen = cfg->maxLoadOne * 0.5;
	}

	if (config_lookup_float(&cfg->cfg, "retry-timeout", &cfg->retryLimit) ==
	    CONFIG_TRUE) {
		if (cfg->retryLimit > 86400L) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"retry-timeout\"\n");
			fprintf(stderr, "watchdogd: disabling retry timeout\n");
			cfg->retryLimit = 0L;
		}
	} else {
		cfg->retryLimit = 0L;
	}

	config_set_auto_convert(&cfg->cfg, false);

	if (config_lookup_bool(&cfg->cfg, "realtime-scheduling", &tmp)) {
		if (tmp) {
			if (SetSchedulerPolicy(cfg->priority) < 0) {
				return -1;
			}
			cfg->options |= REALTIME;
		}
	} else {
		if (SetSchedulerPolicy(cfg->priority) < 0) {
			return -1;
		}
		cfg->options |= REALTIME;
	}

	if (config_lookup_int(&cfg->cfg, "watchdog-timeout", &tmp) ==
	    CONFIG_TRUE) {
		if (tmp < 0 || tmp > 60 || tmp == 0) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"watchdog-timeout\"\n");
			fprintf(stderr, "watchdogd: using device default\n");
			cfg->watchdogTimeout = -1;
		} else {
			cfg->watchdogTimeout = tmp;
		}
	}

	if (config_lookup_int(&cfg->cfg, "repair-timeout", &tmp) == CONFIG_TRUE) {
		if (tmp < 0 || tmp > 499999) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"repair-timeout\"\n");
			fprintf(stderr,
				"watchdogd: disabled repair binary timeout\n");
			cfg->repairBinTimeout = 0;
		} else {
			cfg->repairBinTimeout = tmp;
		}
	}

	if (config_lookup_int(&cfg->cfg, "test-timeout", &tmp) == CONFIG_TRUE) {
		if (tmp < 0 || tmp > 499999) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"test-timeout\"\n");
			fprintf(stderr,
				"watchdogd: disabled test binary timeout\n");
			cfg->testBinTimeout = 0;
		} else {
			cfg->testBinTimeout = tmp;
		}
	}

	if (config_lookup_int(&cfg->cfg, "interval", &tmp) == CONFIG_TRUE) {
		if (tmp < 0 || tmp > 60 || tmp == 0) {
			tmp = 1;
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"interval\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
		} else {
			cfg->sleeptime = (time_t) tmp;
		}
	} else {
		cfg->sleeptime = -1;
	}

	if (config_lookup_int(&cfg->cfg, "sigterm-delay", &cfg->sigtermDelay) == CONFIG_FALSE) {
		cfg->sigtermDelay = 5;
	} else {
		if (cfg->sigtermDelay <= 1 || cfg->sigtermDelay > 300) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"sigterm-delay\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
			cfg->sigtermDelay = 5;
		}
	}

	cfg->pidFiles = config_lookup(&cfg->cfg, "pid-files");

	if (cfg->pidFiles != NULL) {
		if (config_setting_is_array(cfg->pidFiles) == CONFIG_FALSE) {
			fprintf(stderr,
				"watchdogd: %s:%i: illegal type for configuration file entry"
				" \"pid-files\" expected array\n",
				LibconfigWraperConfigSettingSourceFile
				(cfg->pidFiles),
				config_setting_source_line(cfg->pidFiles));
			return -1;
		}

		if (config_setting_length(cfg->pidFiles) > 0) {
			cfg->options |= ENABLEPIDCHECKER;
		}
	}

	cfg->ipAddresses = config_lookup(&cfg->cfg, "ping");

	if (cfg->ipAddresses != NULL) {
		if (config_setting_is_array(cfg->ipAddresses) == CONFIG_FALSE) {
			fprintf(stderr,
				"watchdogd: %s:%i: illegal type for configuration file entry"
				" \"ip-address\" expected array\n",
				LibconfigWraperConfigSettingSourceFile
				(cfg->ipAddresses),
				config_setting_source_line(cfg->ipAddresses));
			return -1;
		}

		if (config_setting_length(cfg->ipAddresses) > 0) {
			cfg->pingObj = ping_construct();
			if (cfg->pingObj == NULL) {
				Logmsg(LOG_CRIT,
				       "unable to allocate memory for ping object");
				FatalError(cfg);
			}

			cfg->options |= ENABLEPING;
		}
	}

	return 0;
}

int PingInit(struct cfgoptions *const cfg)
{
	assert(cfg != NULL);

	if (cfg == NULL) {
		return -1;
	}

	if (cfg->options & ENABLEPING) {
		for (int cnt = 0; cnt < config_setting_length(cfg->ipAddresses);
		     cnt++) {
			const char *ipAddress =
			    config_setting_get_string_elem(cfg->ipAddresses,
							   cnt);

			if (ping_host_add(cfg->pingObj, ipAddress) != 0) {
				fprintf(stderr, "watchdogd: %s\n",
					ping_get_error(cfg->pingObj));
				ping_destroy(cfg->pingObj);
				return -1;
			}
		}

		for (pingobj_iter_t * iter = ping_iterator_get(cfg->pingObj);
		     iter != NULL; iter = ping_iterator_next(iter)) {
			ping_iterator_set_context(iter, NULL);
		}
	}

	return 0;
}

int ParseCommandLine(int *argc, char **argv, struct cfgoptions *cfg)
{
	int opt = 0;

	while ((opt = getopt(*argc, argv, "qsfFbc:")) != -1) {
		switch (opt) {
		case 'F':
			cfg->options &= !DAEMONIZE;
			break;
		case 'c':
			cfg->confile = optarg;
			break;
		case 's':
			cfg->options |= SYNC;
			break;
		case 'q':
			cfg->options |= NOACTION;
			break;
		case 'f':
			cfg->options |= FORCE;
			break;
		case 'b':
			cfg->options |= SOFTBOOT;
			break;
		default:
			Usage();
			return -1;
		}
	}

	return 0;
}

int MakeLogDir(struct cfgoptions *cfg)
{
	assert(cfg != NULL);

	errno = 0;
	if (mkdir(cfg->logdir, 0750) != 0) {
		if (errno != EEXIST) {
			Logmsg(LOG_ERR, "watchdog: %s", strerror(errno));
			return -1;
		} else {
			return 0;
		}
	}

	return 0;
}

int GetDefaultPriority(void)
{
	int ret = sched_get_priority_min(SCHED_RR);

	if (ret < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		return ret;
	}

	return ret;
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

bool SetDefaultConfig(struct cfgoptions * options)
{
	assert(options != NULL);

	if (options == NULL)
		return false;

	memset(options, 0, sizeof(*options));

	options->confile = "/etc/watchdogd.conf";
	options->sleeptime = 1;
	options->watchdogTimeout = -1;
	options->repairBinTimeout = 60;
	options->testBinTimeout = 60;
	options->options |= DAEMONIZE | USEPIDFILE;

	return true;
}

int PrintVersionString(void)
{
	printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright 2013-2014 Christian Lockley. All rights reserved.\n");
	return 0;
}

int Usage(void)
{
	PrintVersionString();
	printf("%s [-F] [-q] [-s] [-b] [-f] [-c <config_file>]\n",
	       PACKAGE_NAME);
	return 0;
}
