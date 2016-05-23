/*
 * Copyright 2016 Christian Lockley
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
#include "bootstatus.h"
#include "sub.h"

int WriteBootStatus(watchdog_t * const watchdog, struct cfgoptions * const config)
{
	if (watchdog == NULL || config == NULL) {
		return -1;
	}

	int fd = open("/run/watchdogd.status", O_WRONLY|O_CLOEXEC|O_CREAT, 0644);

	if (fd < 0) {
		return fd;
	}

	dprintf(fd, "Reset cause   : 0x%04x\n", GetWatchdogBootStatus(watchdog));
	dprintf(fd, "Timeout (sec) : %i\n", GetRawTimeout(watchdog));
	dprintf(fd, "Kick Interval : %lli\n", (long long)config->sleeptime);

	close(fd);

	return 0;
}
