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
 */

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <inttypes.h>
#include <stdint.h>

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>
#include "watchdogd.h"
#include "sub.h"

void Logmsg(int priority, const char *fmt, ...)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	extern bool logToSyslog;

	char buf[256] = { 0 };

	va_list args;

	if (logToSyslog == false) {

		va_start(args, fmt);

		pthread_mutex_lock(&mutex);

		switch (priority) {
		case LOG_EMERG:
			fprintf(stderr, "<0>");
			break;
		case LOG_ALERT:
			fprintf(stderr, "<1>");
			break;
		case LOG_CRIT:
			fprintf(stderr, "<2>");
			break;
		case LOG_ERR:
			fprintf(stderr, "<3>");
			break;
		case LOG_WARNING:
			fprintf(stderr, "<4>");
			break;
		case LOG_NOTICE:
			fprintf(stderr, "<5>");
			break;
		case LOG_INFO:
			fprintf(stderr, "<6>");
			break;
		case LOG_DEBUG:
			fprintf(stderr, "<7>");
			break;
		}

		vfprintf(stderr, fmt, args);
		fprintf(stderr, "\n");

		pthread_mutex_unlock(&mutex);

		va_end(args);
		return;
	}

	va_start(args, fmt);

	vsnprintf(buf, sizeof(buf) - 1, fmt, args);

	syslog(priority, "%s", buf);

	va_end(args);
}
