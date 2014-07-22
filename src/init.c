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

#include <getopt.h>
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

int ParseCommandLine(int *argc, char **argv, struct cfgoptions *cfg)
{
	int opt = 0;

	const struct option longOptions[] = {
		{"no-action", no_argument, 0, 'q'},
		{"foreground", no_argument, 0, 'F'},
		{"force", no_argument, 0, 'f'},
		{"sync", no_argument, 0, 's'},
		{"softboot", no_argument, 0, 'b'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'h'},
		{"config-file", required_argument, 0, 'c'},
		{0, 0, 0, 0}
	};

	int tmp = 0;
	while ((opt = getopt_long(*argc, argv, "qsfFbc:", longOptions, &tmp)) != -1) {

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
		case 'v':
		case 'h':
			Usage();
			return 1;
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
	printf("%s\n", PACKAGE_STRING);
	printf("Copyright 2013-2014 Christian Lockley. All rights reserved.\n");
	return 0;
}

int Usage(void)
{
	PrintVersionString();
	printf("%s [-F|--foreground] [-b|--softboot] [-s|--sync] [-b|--softboot]\n"
		"\t  [-f|--force] [-c <config_file>|--config-file <config_file>]\n",
	       PACKAGE_NAME);
	return 0;
}
