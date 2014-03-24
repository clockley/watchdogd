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

void Logmsg(int priority, const char *fmt, ...)
{
	extern bool logToSyslog;

	char buf[2048] = { '\0' };

	assert(fmt != NULL);

	va_list args;

	if (logToSyslog == false) {
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

		fprintf(stderr, "%s\n", buf);

		return;
	}

	va_start(args, fmt);

	vsnprintf(buf, sizeof(buf) - 1, fmt, args);

	va_end(args);

	assert(buf[sizeof(buf) - 1] == '\0');

	syslog(priority, "%s", buf);
}
