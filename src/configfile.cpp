/*
 * Copyright 2013-2017 Christian Lockley
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
#include "watchdogd.hpp"
#include "sub.hpp"
#include "init.hpp"
#include "testdir.hpp"
#include "daemon.hpp"
#include "configfile.hpp"
#include "network_tester.hpp"
#include "repair.hpp"
#include "logutils.hpp"

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
			SetLogTarget(SYSTEM_LOG);
			return true;
		} else {
			SetLogTarget(STANDARD_ERROR);
			return true;
		}
	}

	if (strcmp(cfg->logTarget, "syslog") == 0) {
		SetLogTarget(SYSTEM_LOG);
		return true;
	}

	if (strcmp(cfg->logTarget, "stderr") == 0) {
		SetLogTarget(STANDARD_ERROR);
		return true;
	}

	if (strchr(cfg->logTarget, ':') == NULL) {
		 return false;
	}

	char *tmp;

	char *copyOfLogTarget = strdup(cfg->logTarget);

	if (copyOfLogTarget == NULL) {
		fprintf(stderr, "%s\n", MyStrerror(errno));
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
			|| S_ISBLK(buf.st_mode)
			|| S_ISFIFO(buf.st_mode)
			|| S_ISSOCK(buf.st_mode)
			|| S_ISCHR(buf.st_mode) || !S_ISREG(buf.st_mode)) {
				fprintf(stderr, "Invalid file type\n");
				goto error;
		}

		SetLogTarget(FILE_APPEND, fileName);
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
			|| S_ISBLK(buf.st_mode)
			|| S_ISFIFO(buf.st_mode)
			|| S_ISSOCK(buf.st_mode)
			|| S_ISCHR(buf.st_mode) || !S_ISREG(buf.st_mode)) {
				fprintf(stderr, "Invalid file type\n");
				goto error;
		}

		SetLogTarget(FILE_NEW, fileName);
	} else {
		goto error;
	}

	if (copyOfLogTarget != NULL) {
		free(copyOfLogTarget);
	}

	return true;
 error:
	if (errno != 0) {
		fprintf(stderr, "%s\n", MyStrerror(errno));
	}

	if (copyOfLogTarget != NULL) {
		free(copyOfLogTarget);
	}

	return false;
}

void NoWhitespace(register char *s)
{
	if (isspace(*s)) {
		s[strlen((char*)memmove(s, s+1, (strlen(s)-1)*sizeof(char)))-1] = '\0';
		NoWhitespace(s);
	} else if (*s == '\0') {
		return;
	}
	NoWhitespace(s+1);
}

static bool ConvertLegacyWatchdogConfigfile(char * path, char **ptr)
{
	char **ping = NULL;
	char **pidFiles = NULL;
	unsigned int pidLen = 0;
	unsigned int pLen = 0;
	char *buf = NULL;
	size_t len = 0;
	size_t size = 0;

	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return false;

	FILE * configFile = open_memstream(ptr, &size);
	if (configFile == NULL) {
		fclose(fp);
		return false;
	}

	unsigned long line = 1;

	static const char* errorMessages[] = {"missing initializer for", "option initlized to null value"};
	while (getline(&buf, &len, fp) != -1) {
		++line;
		if (*buf == '#')
			continue;
		char *const name = strtok(buf, "=");
		char *const value = strtok(NULL, "=");

		if (Validate(name, value) == false) {
			fprintf(stderr, "watchdogd: syntax error on line %lu:\n      %s %s", line, value == NULL ? errorMessages[0] : errorMessages[1], value == NULL ? buf: "\n");
			continue;
		}

		NoWhitespace(value);
		NoWhitespace(name);

		if (value[strlen(value)-1] == 34 && value[strlen(value)] == '\0') {
			value[strlen((char*)memmove(value, value+1, (strlen(value)-1)*sizeof(char)))-2] = '\0';
		}

		if (strcasecmp(name, "ping") == 0) {
			if (value[0] == '\0') {
				continue;
			}
			if ((ping = (char**)realloc(ping, pLen+2*sizeof(char*))) == NULL) {
				abort();
			}
			ping[pLen++] = strdup(value);
		} else if (strcasecmp(name, "pidfile") == 0) {
			if (value[0] == '\0') {
				continue;
			}
			if ((pidFiles = (char**)realloc(pidFiles, pidLen+2*sizeof(char*))) == NULL) {
				abort();
			}
			pidFiles[pidLen++] = strdup(value);
		} else {
			fprintf(configFile, "%s = \"%s\"\n", name, value);
		}
	}

	free(buf);
	if (pidFiles != NULL) {
		fprintf(configFile, "pid-files = [");
		for (size_t i = 0; i < pidLen; i++) {
			if (i == 0)
				fprintf(configFile, "\"%s\"", pidFiles[i]);
			else
				fprintf(configFile, ", \"%s\"", pidFiles[i]);
			free(pidFiles[i]);
		}
		fprintf(configFile, "]\n");
	}

	if (ping != NULL) {
		fprintf(configFile, "ping = [");
		for (size_t i = 0; i < pLen; i++) {
			if (i == 0)
				fprintf(configFile, "\"%s\"", ping[i]);
			else
				fprintf(configFile, ", \"%s\"", ping[i]);
			free(ping[i]);
		}
		fprintf(configFile, "]\n");
	}
	fclose(configFile);
	free(ping);
	free(pidFiles);
	return true;
}

static bool LoadConfigurationFile(config_t *const config, const char *const fileName, struct cfgoptions *const cfg)
{
	assert(config != NULL);
	assert(fileName != NULL);

	config_init(config);

	if (config == NULL) {
		return false;
	}

	if (!config_read_file(config, fileName)
	    && config_error_file(config) == NULL) {
	    	if (!(IDENTIFY & cfg->options)) {
			fprintf(stderr,
				"watchdogd: unable to open configuration file: %s\n",
				fileName);
			fprintf(stderr, "Using default values\n");
		}
		cfg->haveConfigFile = false;
	} else if (!config_read_file(config, fileName)) {
		char *ptr = NULL;
		if (ConvertLegacyWatchdogConfigfile((char*)fileName, &ptr) == false) {
			fprintf(stderr, "watchdogd: %s:%d: %s\n",
				config_error_file(config),
				config_error_line(config),
				config_error_text(config));
			config_destroy(config);
			free(ptr);
			return false;
		} else {
			config_destroy(config);
			config_init(config);
			if (config_read_string(config, ptr) == CONFIG_FALSE) {
				fprintf(stderr, "watchdogd: %s:%d: %s\n",
					config_error_file(config),
					config_error_line(config),
					config_error_text(config));
				config_destroy(config);
				free(ptr);
				return false;
			}
			free(ptr);
		}
	}

	return true;
}

int ReadConfigurationFile(struct cfgoptions *const cfg)
{
	assert(cfg != NULL);

	int tmp = 0;

	if (LoadConfigurationFile(&cfg->cfg, cfg->confile, cfg) == false) {
		CreateLinkedListOfExes((char*)cfg->testexepath, &processes, cfg);
		return -1;
	}

	config_set_auto_convert(&cfg->cfg, true);

	config_lookup_string(&cfg->cfg, "script", &cfg->pluginScript);

	cfg->filern = (FileDescriptorPlugin*)calloc(1, sizeof(FileDescriptorPlugin));

	if ((cfg->filern->setting = config_lookup(&cfg->cfg, "filern")) != nullptr) {
		if (config_setting_lookup_int(cfg->filern->setting, "interval", &cfg->filern->interval) == CONFIG_FALSE) {
			cfg->filern->interval = 300;
		}
		config_setting_lookup_string(cfg->filern->setting, "script", &cfg->filern->script);
		config_setting_lookup_int(cfg->filern->setting, "warning", &cfg->filern->warning);
		if (cfg->filern->script == nullptr && cfg->pluginScript == nullptr) {
				free((void*)cfg->filern);
				cfg->filern = nullptr;	
		}
	} else {
		free((void*)cfg->filern);
		cfg->filern = nullptr;
	}

	if (!(cfg->options & BUSYBOXDEVOPTCOMPAT)) {
		if (config_lookup_string(&cfg->cfg, "watchdog-device", &cfg->devicepath)
		    == CONFIG_FALSE) {
			cfg->devicepath = "/dev/watchdog";
		}
	}

	if (cfg->options & IDENTIFY) {
		return 0;
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
		cfg->testexepath = "/usr/libexec/watchdog/scripts";
	}

	if (config_lookup_string(&cfg->cfg, "log-target", &cfg->logTarget) == CONFIG_FALSE) {
		cfg->logTarget = "auto";
	}


	if (!(cfg->options & LOGLVLSETCMDLN)) {
		if (config_lookup_string(&cfg->cfg, "log-up-to", &cfg->logUpto) == CONFIG_TRUE) {
			LogUpTo(cfg->logUpto, false);
		}

		if (config_lookup_int(&cfg->cfg, "log-up-to", &tmp) == CONFIG_TRUE) {
			LogUpToInt(tmp, false);
		}
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

	if (config_lookup_string(&cfg->cfg, "pid-pathname", &cfg->pidfileName)
	    == CONFIG_FALSE) {
		cfg->pidfileName = "/run/watchdogd.pid";
	}

	if (CreateLinkedListOfExes((char*)cfg->testexepath, &processes, cfg) < 0) {
		fprintf(stderr, "watchdogd: CreateLinkedListOfExes failed\n");
		return -1;
	}

	if (config_lookup_bool(&cfg->cfg, "daemonize", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			cfg->options |= DAEMONIZE;
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

	if (config_lookup_bool(&cfg->cfg, "os-x-hashtag-loging", &tmp) == CONFIG_TRUE) {
		if (tmp) {
			SetAutoPeriod(true);
			HashTagPriority(true);
			//SetAutoUpperCase(true); //lot's of magic in logmsg.c
		} else {
			SetAutoPeriod(false);
			HashTagPriority(false);
		}
	} else {
		SetAutoPeriod(false);
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

	if (config_lookup_int(&cfg->cfg, "allocatable-memory", &cfg->allocatableMemory) == CONFIG_FALSE) {
		cfg->allocatableMemory = 0;
	}

	if (config_lookup_int(&cfg->cfg, "sigterm-delay", &cfg->sigtermDelay) == CONFIG_TRUE) {
		if (cfg->sigtermDelay <= 1 || cfg->sigtermDelay > 300) {
			fprintf(stderr,
				"watchdogd: illegal value for configuration file entry named \"sigterm-delay\"\n");
			fprintf(stderr, "watchdogd: using default value\n");
			cfg->sigtermDelay = 5;
		}
	} else {
		cfg->sigtermDelay = 5;
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

	cfg->networkInterfaces = config_lookup(&cfg->cfg, "network-interfaces");

	if (cfg->networkInterfaces != NULL) {
		if (config_setting_is_array(cfg->networkInterfaces) == CONFIG_FALSE) {
			fprintf(stderr,
				"watchdogd: %s:%i: illegal type for configuration file entry"
				" \"network-interfaces\" expected array\n",
				LibconfigWraperConfigSettingSourceFile
				(cfg->networkInterfaces),
				config_setting_source_line(cfg->networkInterfaces));
			return -1;
		}

		if (config_setting_length(cfg->networkInterfaces) > 0) {
			NetMonInit();
			for (int cnt = 0; cnt < config_setting_length(cfg->networkInterfaces); cnt++) {
				if (NetMonAdd(config_setting_get_string_elem(cfg->networkInterfaces, cnt)) == false) {
					Logmsg(LOG_ALERT, "Unable to add network interface: %s",
							config_setting_get_string_elem(cfg->networkInterfaces, cnt));
				}
			}
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
