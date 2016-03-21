/*
 * Copyright 2013-2015 Christian Lockley
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
#include "multicall.h"

int SetSchedulerPolicy(int priority)
{
	struct sched_param param;
	param.sched_priority = priority;

#ifndef SCHED_RESET_ON_FORK
#define SCHED_RESET_ON_FORK 0
#endif

	if (sched_setscheduler(0, SCHED_RR|SCHED_RESET_ON_FORK, &param) < 0) {
		assert(errno != ESRCH);
		fprintf(stderr, "watchdogd: sched_setscheduler failed %s\n",
			MyStrerror(errno));
		return -1;
	}

	return 0;
}

int InitializePosixMemlock(void)
{
	if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
		fprintf(stderr, "watchdogd: unable to lock memory %s\n",
			MyStrerror(errno));
		return -1;
	}

	return 0;
}

static void PrintHelpIdentify(void);

int ParseCommandLine(int *argc, char **argv, struct cfgoptions *cfg)
{
	int opt = 0;

	const struct option longOptions[] = {
		{"no-action", no_argument, 0, 'q'},
		{"foreground", no_argument, 0, 'F'},
		{"debug", no_argument, 0, 'd'},
		{"force", no_argument, 0, 'f'},
		{"sync", no_argument, 0, 's'},
		{"softboot", no_argument, 0, 'b'},
		{"help", no_argument, 0, 'h'},
		{"verbose", no_argument, 0, 'v'},
		{"version", no_argument, 0, 'V'},
		{"config-file", required_argument, 0, 'c'},
		{"loop-exit", required_argument, 0, 'X'},
		{"identify", no_argument, 0, 'i'},
		{0, 0, 0, 0}
	};

	int tmp = 0;
	while ((opt =
		getopt_long(*argc, argv, "ihqsfFbVvndc:X:", longOptions,
			    &tmp)) != -1) {

		switch (opt) {
		case 'd':
		case 'F':
			cfg->options &= !DAEMONIZE;
			break;
		case 'c':
			cfg->confile = optarg;
			break;
		case 's':
			cfg->options |= SYNC;
			break;
		case 'n':
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
			cfg->options |= VERBOSE;
			break;
		case 'X':
			cfg->loopExit = ConvertStringToInt(optarg);
			if (cfg->loopExit <= 0) {
				Logmsg(LOG_ERR, "optarg must be greater than 0 for X option");
				return -1;
			}
			break;
		case 'i':
			cfg->options |= IDENTIFY;
			break;
		case 'V':
			PrintVersionString();
			return 1;
		case 'h':
			if (cfg->options & IDENTIFY) {
				PrintHelpIdentify();
			} else {
				Usage();
			}
			return 1;
		default:
			if (cfg->options & IDENTIFY) {
				PrintHelpIdentify();
			} else {
				Usage();
			}
			return -1;
		}
	}

	return 0;
}

int MakeLogDir(struct cfgoptions *const cfg)
{
	assert(cfg != NULL);

	errno = 0;
	if (mkdir(cfg->logdir, 0750) != 0) {
		if (errno != EEXIST) {
			Logmsg(LOG_ERR, "watchdog: %s", MyStrerror(errno));
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
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
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
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		return -1;
	}

	min = sched_get_priority_min(SCHED_RR);

	if (min < 0) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
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

bool SetDefaultConfig(struct cfgoptions *const options)
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
	options->loopExit = -1;
	options->haveConfigFile = true;

	return true;
}

int PrintVersionString(void)
{
	printf("%s\n", PACKAGE_STRING);
	printf("Copyright 2013-2015 Christian Lockley. All rights reserved.\n");
	printf("Licensed under the Apache License, Version 2.0.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n");

	return 0;
}

static void PrintHelpIdentify(void)
{
	char *buf = NULL;
	Wasprintf(&buf, "Usage: %s [OPTION]", GetExeName());

	assert(buf != NULL);

	if (buf == NULL) {
		abort();
	}

	char *help[][2] = {
		{buf, ""},
		{"Get watchdog device status.", ""},
		{"", ""},
		{"  -c, --config-file ", "path to configuration file"},
		{"  -i, --identify", "identify hardware watchdog"},
		{"  -h, --help", "this help"},
		{"  -V, --version", "print version info"},
	};

	for (size_t i = 0; i < ARRAY_SIZE(help); i += 1) {
		bool isterm = isatty(STDOUT_FILENO) == 1;
		long col = GetConsoleColumns();
		if (col >= 80) { //KLUGE
			col += col / 2;
		}
		long len = 0;
		if (isterm && i > 2) {
			printf("\x1B[1m");
		}
		len += printf("%-22s", help[i][0]);

		if (isterm && i > 2) {
			printf("\x1B[22m");
		}
		
		char *ptr = strdup(help[i][1]);

		if (ptr == NULL) {
			perror(PACKAGE_NAME);
			return;
		}

		char *save = NULL;
		char *tmp = strtok_r(ptr, " ", &save);
		if (isterm) {
			printf("\e[1;34m");
		}
		while (tmp != NULL) {
			len += strlen(tmp);
			if (len > col) {
				len = 0;
				len = printf("                      ");
				len += printf("%s", tmp);
				tmp = strtok_r(NULL, " ", &save);
				if (tmp != NULL) {
					len += printf(" ");
				}
			} else {
				len += printf("%s", tmp);
				tmp = strtok_r(NULL, " ", &save);
				if (tmp != NULL) {
					len += printf(" ");
				}
			}
		}
		if (isterm) {
			printf("\x1B[39m\x1B[22m");
		}
		free(ptr);
		printf("\n");
	}
	free(buf);
}

static void PrintHelpMain(void)
{
//Emulate the gnu --help output.
	const char *const help[][2] = {
		{"Usage: " PACKAGE_NAME " [OPTION]", ""},
		{"A watchdog daemon for linux.", ""},
		{"", ""},
		{"  -b, --softboot", "ignore file open errors"},
		{"  -c, --config-file ", "path to configuration file"},
		{"  -f, --force",
		 "force a ping interval or timeout even if the ping interval is less than the timeout"},
		{"  -F, --foreground", "run in foreground mode"},
		{"  -i, --identify", "identify hardware watchdog"},
		{"  -s, --sync", "sync file-systems regularly"},
		{"  -h, --help", "this help"},
		{"  -V, --version", "print version info"},
		{isatty(STDOUT_FILENO) == 1 ? "  -X, --loop-exit \033[4mnum\033[0m ":
		"  -X, --loop-exit num", " Run  for  'num'  loops  then  exit"}
	};

	for (size_t i = 0; i < ARRAY_SIZE(help); i += 1) {
		bool isterm = isatty(STDOUT_FILENO) == 1;
		long col = GetConsoleColumns();
		if (col >= 80) { //KLUGE
			col += col / 2;
		}
		long len = 0;
		if (isterm && i > 2) {
			printf("\x1B[1m");
		}

		len += printf("%-22s", help[i][0]);

		if (isterm && i > 2) {
			printf("\x1B[22m");
		}

		char *ptr = strdup(help[i][1]);

		if (ptr == NULL) {
			perror(PACKAGE_NAME);
			return;
		}

		char *save = NULL;
		char *tmp = strtok_r(ptr, " ", &save);
		if (isterm) {
			printf("\e[1;34m");
		}
		while (tmp != NULL) {
			len += strlen(tmp);
			if (len > col) {
				len = 0;
				len = printf("                      ");
				len += printf("%s", tmp);
				tmp = strtok_r(NULL, " ", &save);
				if (tmp != NULL) {
					len += printf(" ");
				}
			} else {
				len += printf("%s", tmp);
				tmp = strtok_r(NULL, " ", &save);
				if (tmp != NULL) {
					len += printf(" ");
				}
			}
		}
		if (isterm) {
			printf("\x1B[39m\x1B[22m");
		}
		free(ptr);
		printf("\n");
	}
}

int Usage(void)
{
	if (strcasecmp(GetExeName(), "wd_identify") == 0 ||
		strcasecmp(GetExeName(), "wd_identify.sh") == 0) {
		PrintHelpIdentify();
	} else {
		PrintHelpMain();
	}

	return 0;
}
