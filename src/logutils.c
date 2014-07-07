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

static sig_atomic_t logTarget = invalidTarget;
static FILE* logFile = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void CloseOldTarget(logTargets oldTarget)
{

	if (oldTarget == invalidTarget) {
		return;
	}

	if (oldTarget == systemLog) {
		closelog();
		return;
	}

	if (oldTarget == standardError) {
		;
	}

	if (oldTarget == newFile || oldTarget == file) {
		if (logFile != NULL) {
			fflush(logFile);
			fclose(logFile);
			logFile = NULL;
		}
	}

}

void SetLogTarget(logTargets target, ...)
{
	pthread_mutex_lock(&mutex);

	if (target == standardError) {
		CloseOldTarget(logTarget);
		logTarget = standardError;
	}

	if (target == systemLog) {

		if (logTarget != systemLog) {
			openlog("watchdogd", LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON);
		}

		CloseOldTarget(logTarget);
		logTarget = systemLog;
	}

	if (target == newFile || target == file) {
		closelog();
		va_list ap;
		va_start(ap, target);

		const char * fileName = va_arg(ap, const char *);

		assert(fileName != NULL);

		if (target == newFile) {
			logFile = fopen(fileName, "w");
			if (logFile == NULL) {
				if (logTarget == systemLog) {
					syslog(LOG_ALERT, "%m", errno);
				} else {
					fprintf(stderr, "%s\n", strerror(errno));
				}
			} else {
				CloseOldTarget(logTarget);
				logTarget = newFile;
			}
		} else if (target == file) {
			logFile = fopen(fileName, "a");
			if (logFile == NULL) {
				if (logTarget == systemLog) {
					syslog(LOG_ALERT, "%m", errno);
				} else {
					fprintf(stderr, "%s\n", strerror(errno));
				}
			} else {
				CloseOldTarget(logTarget);
				logTarget = file;
			}
		} else {
			assert(false);
		}

		va_end(ap);
	}

	pthread_mutex_unlock(&mutex);
}

void Logmsg(int priority, const char *fmt, ...)
{
	extern bool logToSyslog;

	char buf[2048] = { '\0' };

	assert(fmt != NULL);

	va_list args;

	if (logTarget == standardError || logTarget == file || logTarget == newFile) {
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

		if (logTarget == standardError) {
			fprintf(stderr, "%s\n", buf);
		} else {
			if (logFile != NULL) {
				fprintf(logFile, "%s\n", buf);
			} else {
				fprintf(stderr, "%s\n", buf);
			}
		}

		return;
	}

	if (logTarget == systemLog) {
		va_start(args, fmt);

		vsnprintf(buf, sizeof(buf) - 1, fmt, args);

		va_end(args);

		assert(buf[sizeof(buf) - 1] == '\0');

		syslog(priority, "%s", buf);

		return;
	}
}
