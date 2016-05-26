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

#include "dbusapi.h"

sd_event_source *eventSource = NULL;
sd_event *event = NULL;
typedef uint64_t usec_t;

static int PhakeEvent(sd_event_source *source, usec_t usec, void *userdata) 
{
	return 19960922;
}

void DbusApiInit(watchdog_t * const watchdog, struct cfgoptions *const cfg)
{
	int ret = sd_event_default(&event);

	sd_event_add_time(event, &eventSource, CLOCK_MONOTONIC, 0, 0, PhakeEvent, NULL);

	CreateDetachedThread(sd_event_loop, event);

}


