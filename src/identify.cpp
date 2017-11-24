/*
 * Copyright 2016-2017 Christian Lockley
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

int Identify(long timeout, const char * identity, const char * deviceName, bool verbose)
{
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
		close(fd);
		goto direct;
	}

	read(fd, &buf, sizeof(buf));
	close(fd);

	if (verbose) {
		printf("watchdog was set to %li seconds\n", buf.timeout);
	}

	printf("%s\n", buf.name);

	return 0;

direct:
	if (!identity)
		goto error;
	if (verbose) {
		printf("watchdog was set to %li seconds\n", timeout);
	}

	printf("%s\n", identity);
	return 0;

error:
	if (access(deviceName, R_OK|W_OK) != 0) {
		printf("%s\n", "unable to open watchdog device, this operation requires permission from system Administrator");
		return 0;
	}

	printf("%s\n", "Unable to open watchdog device");

	return 0;
}
