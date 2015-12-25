/*
 * Copyright 2015 Christian Lockley
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
#include "logutils.h"
#include "sub.h"

int Identify(watchdog_t * const wdt, bool verbose)
{
	SetLogTarget(STANDARD_ERROR);
	struct watchdog_info watchDogInfo;

	int ret = 1;

	if (wdt == NULL) {
		struct sockaddr_un address = {0};
		struct identinfo buf;
		int fd = -1;

		address.sun_family = AF_UNIX;
		strncpy(address.sun_path, "\0watchdogd.wdt.identity", sizeof(address.sun_path)-1);

		fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);

		if (fd < 0) {
			goto error;
		}

		if (connect(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
			goto error;
		}
		read(fd, &buf, sizeof(buf));
		close(fd);

		if (verbose) {
			printf("watchdog was set to %li seconds\n", buf.timeout);
		}

		printf("%s\n", buf.name);

		return 0;
	error:
		printf("%s\n", "Unable to open watchdog device");
		

		if (fd >= 0) {
			close(fd);
		}
		return 1;

	}

	if (verbose) {
		printf("watchdog was set to %i seconds\n", GetRawTimeout(wdt));
	}

	if (ioctl(GetFd(wdt), WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		printf("%s\n", MyStrerror(errno));
	} else {
		printf("%s\n", watchDogInfo.identity);
		ret = 0;
	}

	CloseWatchdog(wdt);

	return ret;
}
