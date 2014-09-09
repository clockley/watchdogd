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

static void SetTextColor(int priority)
{
	if (logTarget != STANDARD_ERROR) {
		return;
	}

	if (isatty(fileno(stderr)) == 0) {
		return;
	}
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

void ResetTestColor()
{
	fprintf(stderr, "%s", "\x1B[0m");
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

void Logmsg(int priority, const char *const fmt, ...)
{
	char buf[2048] = { '\0' };

	assert(fmt != NULL);

	va_list args;

	if (logTarget == STANDARD_ERROR || logTarget == FILE_APPEND || logTarget == FILE_NEW) {
		va_start(args, fmt);

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

		if (logTarget == STANDARD_ERROR) {
			SetTextColor(priority);
			fprintf(stderr, "%s\n", buf);
			ResetTestColor();
		} else {
			if (logFile != NULL) {
				fprintf(logFile, "%s\n", buf);
			} else {
				fprintf(stderr, "%s\n", buf);
			}
		}

		return;
	}

	if (logTarget == SYSTEM_LOG) {
		va_start(args, fmt);

		vsnprintf(buf, sizeof(buf) - 1, fmt, args);

		va_end(args);

		assert(buf[sizeof(buf) - 1] == '\0');

		syslog(priority, "%s", buf);

		return;
	}
}
