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

int Identify(watchdog_t * const wdt)
{
	SetLogTarget(STANDARD_ERROR);
	struct watchdog_info watchDogInfo;

	int ret = 1;

	if (wdt == NULL) {
		printf("Unable to open watchdog\n");
		return ret;
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
