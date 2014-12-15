/*
Copyright (c) 2012, Martin Crossley <mjcross@users.sf.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * Based on code from server-framework 2.3
 */

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
#include "logutils.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

static sig_atomic_t logTarget = INVALID_LOG_TARGET;
static FILE* logFile = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static sig_atomic_t applesquePriority = 0;
static sig_atomic_t autoUpperCase = 0;
static sig_atomic_t autoPeriod = 1;
static unsigned int logMask = 0xff;

static bool IsTty(void)
{
	if (logTarget != STANDARD_ERROR) {
		return false;
	}

	if (isatty(fileno(stderr)) == 0) {
		return false;
	}
	return true;
}

static void ResetTextColor(void)
{
	if (IsTty() == false) {
		return;
	}

	fprintf(stderr, "%s", "\x1B[0m");
}

static void SetTextColor(int priority)
{
	if (IsTty() == false) {
		return;
	}

	ResetTextColor();

	switch (priority) {
	case LOG_EMERG:
		fprintf(stderr, "%s", KRED);
		break;
	case LOG_ALERT:
		fprintf(stderr, "%s", KRED);
		break;
	case LOG_CRIT:
		fprintf(stderr, "%s", KRED);
		break;
	case LOG_ERR:
		fprintf(stderr, "%s", KRED);
		break;
	case LOG_WARNING:
		fprintf(stderr, "%s", KMAG);
		break;
	case LOG_NOTICE:
		;
		break;
	case LOG_INFO:
		;
		break;
	case LOG_DEBUG:
		;
		break;
	default:
		assert(false);
	}
	
}

static void CloseOldTarget(sig_atomic_t oldTarget)
{
	if (oldTarget == INVALID_LOG_TARGET) {
		return;
	}

	if (oldTarget == SYSTEM_LOG) {
		closelog();
		return;
	}

	if (oldTarget == STANDARD_ERROR) {
		return;
	}

	if (oldTarget == FILE_NEW || oldTarget == FILE_APPEND) {
		if (logFile != NULL) {
			fflush(logFile);
			fclose(logFile);
			logFile = NULL;
			return;
		} else {
			return;
		}
	}

}

void SetAutoPeriod(bool x) {
	if (x) {
		autoPeriod = 1;
	} else {
		autoPeriod = 0;
	}
}

void SetAutoUpperCase(bool x) {
	if (x) {
		autoUpperCase = 1;
	} else {
		autoUpperCase = 0;
	}
}

void HashTagPriority(bool x) {
	if (x) {
		applesquePriority = 1;
	} else {
		applesquePriority = 0;
	}
}

static void SetLogMask(unsigned int mask) {
	setlogmask(mask);
	if (mask != 0) {
		logMask = mask;
	}
}

void SetLogTarget(sig_atomic_t target, ...)
{
	pthread_mutex_lock(&mutex);

	if (target == STANDARD_ERROR) {
		CloseOldTarget(logTarget);
		logTarget = STANDARD_ERROR;
	}

	if (target == SYSTEM_LOG) {

		if (logTarget != SYSTEM_LOG) {
			openlog("watchdogd", LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON);
		}

		CloseOldTarget(logTarget);
		logTarget = SYSTEM_LOG;
	}

	if (target == FILE_NEW || target == FILE_APPEND) {
		closelog();
		va_list ap;
		va_start(ap, target);

		const char * fileName = va_arg(ap, const char *);

		assert(fileName != NULL);

		if (target == FILE_NEW) {
			logFile = fopen(fileName, "w");
			if (logFile == NULL) {
				if (logTarget == SYSTEM_LOG) {
					syslog(LOG_ALERT, "%m");
				} else {
					fprintf(stderr, "%s\n", strerror(errno));
				}
			} else {
				CloseOldTarget(logTarget);
				logTarget = FILE_NEW;
			}
		} else if (target == FILE_APPEND) {
			logFile = fopen(fileName, "a");
			if (logFile == NULL) {
				if (logTarget == SYSTEM_LOG) {
					syslog(LOG_ALERT, "%m");
				} else {
					fprintf(stderr, "%s\n", strerror(errno));
				}
			} else {
				CloseOldTarget(logTarget);
				logTarget = FILE_APPEND;
			}
		} else {
			assert(false);
		}

		va_end(ap);
	}

	pthread_mutex_unlock(&mutex);
}

bool LogUpTo(const char * const str)
{
	assert(str != NULL);

	if (str == NULL) {
		return false;
	}

	char *tmp = strdup(str);

	assert(tmp != NULL);

	if (tmp == NULL) {
		return false;
	}

	pthread_mutex_lock(&mutex);

	for (int i = 0; tmp[i] != '\0'; i += 1) {
		if (tmp[i] == '-' || tmp[i] == '.') {
			tmp[i] = '_';
		}
		tmp[i] = toupper(tmp[i]);
	}

	if (strstr(tmp, "LOG_") == NULL) {
		char *buf  = calloc(1, strlen(tmp) + strlen("LOG_") + 2);
		snprintf(buf, strlen(tmp) + strlen("LOG_") + 1, "%s%s", "LOG_", tmp);
		free(tmp);
		tmp = NULL;
		tmp = buf;
	}

	static int ipri[] = {LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING,
		      LOG_NOTICE, LOG_INFO, LOG_DEBUG};

	static const char * spri[] = {"LOG_EMERG", "LOG_ALERT", "LOG_CRIT", "LOG_ERR",
		      "LOG_WARNING", "LOG_NOTICE", "LOG_INFO", "LOG_DEBUG"};

	bool matched = false;

	for (int i = 0; i < ARRAY_SIZE(spri); i += 1) {
		if (strcasecmp(tmp, spri[i]) == 0) {
			SetLogMask(LOG_UPTO(ipri[i]));
			matched = true;
		}
	}

	if (matched == false) {
		fprintf(stderr,
			"illegal value for configuration file entry named"
			" \"log-up-to\": %s\n", tmp);
	}

	free(tmp);

	pthread_mutex_unlock(&mutex);

	return true;
}

void Logmsg(int priority, const char *const fmt, ...)
{
	char buf[2048] = { '\0' };

	assert(fmt != NULL);

	if ((LOG_MASK (LOG_PRI (priority)) & logMask) == 0) {
		return;
	}

	va_list args;
	va_start(args, fmt);

	if ((logTarget == STANDARD_ERROR || logTarget == FILE_APPEND || logTarget == FILE_NEW) && applesquePriority == 0) {

		switch (priority) {
		case LOG_EMERG:
			snprintf(buf, sizeof(buf), "<0>");
			break;
		case LOG_ALERT:
			snprintf(buf, sizeof(buf), "<1>");
			break;
		case LOG_CRIT:
			snprintf(buf, sizeof(buf), "<2>");
			break;
		case LOG_ERR:
			snprintf(buf, sizeof(buf), "<3>");
			break;
		case LOG_WARNING:
			snprintf(buf, sizeof(buf), "<4>");
			break;
		case LOG_NOTICE:
			snprintf(buf, sizeof(buf), "<5>");
			break;
		case LOG_INFO:
			snprintf(buf, sizeof(buf), "<6>");
			break;
		case LOG_DEBUG:
			snprintf(buf, sizeof(buf), "<7>");
			break;
		default:
			assert(false);
		}

		vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), fmt,
			  args);
		va_end(args);

		assert(buf[sizeof(buf) - 1] == '\0');
	} else if (logTarget != SYSTEM_LOG && applesquePriority == 1) {

		vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), fmt,
			  args);
		va_end(args);

		if (autoPeriod == 1) {
			if (buf[strlen(buf) - 1] != '.') {
				strncat(buf, ".", sizeof(buf) - 1);
			}
		}

		if (autoUpperCase == 1) {
			if (strstr(buf, "=") == NULL) {
				if (islower(buf[0])) {
					buf[0] = toupper(buf[0]);
				}
			}
		}

		switch (priority) {
		case LOG_EMERG:
			strncat(buf, " #System #Panic", sizeof(buf) - 1);
			break;
		case LOG_ALERT:
			strncat(buf, " #System #Attention", sizeof(buf) - 1);
			break;
		case LOG_CRIT:
			strncat(buf, " #System #Critical", sizeof(buf) - 1);
			break;
		case LOG_ERR:
			strncat(buf, " #System #Error", sizeof(buf) - 1);
			break;
		case LOG_WARNING:
			strncat(buf, " #System #Warning", sizeof(buf) - 1);
			break;
		case LOG_NOTICE:
			strncat(buf, " #System #Notice", sizeof(buf) - 1);
			break;
		case LOG_INFO:
			strncat(buf, " #System #Comment", sizeof(buf) - 1);
			break;
		case LOG_DEBUG:
			strncat(buf, " #System #Developer ", sizeof(buf) - 1);
			break;
		default:
			assert(false);
		}
	}

	if (logTarget == STANDARD_ERROR || logTarget == FILE_APPEND || logTarget == FILE_NEW) {
		assert(buf[sizeof(buf) - 1] == '\0');

		if (logTarget == STANDARD_ERROR) {
			if (applesquePriority == 0) {
				SetTextColor(priority);
			}
			fprintf(stderr, "%s\n", buf);
			ResetTextColor();
		} else {
			if (logFile != NULL) {
				fprintf(logFile, "%s\n", buf);
			} else {
				fprintf(stderr, "%s\n", buf);
			}
		}
	}

	if (logTarget == SYSTEM_LOG) {
		vsnprintf(buf, sizeof(buf) - 1, fmt, args);

		va_end(args);

		assert(buf[sizeof(buf) - 1] == '\0');

		syslog(priority, "%s", buf);

		return;
	}
}
