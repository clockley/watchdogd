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
static watchdog_t * watchdog = NULL;
static struct cfgoptions *config  = NULL;
static sd_event_source *eventSource = NULL;
static sd_event *event = NULL;
static sd_bus_slot *slot = NULL;
static sd_bus *bus = NULL;

typedef uint64_t usec_t;

static const sd_bus_vtable watchdogPmon[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("DevicePath", "", "s", DevicePath, 0),
        SD_BUS_METHOD("Identity", "", "s", Identity, 0),
        SD_BUS_METHOD("Version", "", "x", Version, 0),
        SD_BUS_METHOD("GetTimeout", "", "x", GetTimeoutDbus, 0),
        SD_BUS_METHOD("GetTimeleft", "", "x", GetTimeleftDbus, 0),
        SD_BUS_VTABLE_END
};

static int DevicePath(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	char coal = '0';
	sd_bus_message_read(m, "", &coal);
	return sd_bus_reply_method_return(m, "s", watchdog->path);
}

static int Identity(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	char coal = '0';
	sd_bus_message_read(m, "", &coal);
	return sd_bus_reply_method_return(m, "s", GetWatchdogIdentity(watchdog));
}

static int Version(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	char coal = '0';
	sd_bus_message_read(m, "", &coal);
	return sd_bus_reply_method_return(m, "x", GetWatchdogIdentity(watchdog));
}

static int GetTimeoutDbus(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	char coal = '0';
	sd_bus_message_read(m, "", &coal);
	return sd_bus_reply_method_return(m, "x", GetRawTimeout(watchdog));
}

static int GetTimeleftDbus(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	char coal = '0';
	sd_bus_message_read(m, "", &coal);
	return sd_bus_reply_method_return(m, "x", GetTimeleft(watchdog));
}

static void * ReceivingThread(void *arg)
{
	while (true) {
		sd_bus_process(bus, NULL);
		sd_bus_wait(bus, (uint64_t) -1);
	}
}

static int PhakeEvent(sd_event_source *source, usec_t usec, void *userdata) 
{
	return 19960922;
}

void DbusApiInit(watchdog_t * const wdt, struct cfgoptions *const cfg)
{
	watchdog = wdt;
	config = cfg;
	int ret = sd_event_default(&event);

	sd_event_add_time(event, &eventSource, CLOCK_MONOTONIC, 0, 0, PhakeEvent, NULL);

	CreateDetachedThread(sd_event_loop, event);

	int r = sd_bus_open_system(&bus);

	r = sd_bus_add_object_vtable(bus, &slot, "/org/watchdogd",
				"org.watchdogd", watchdogPmon, NULL);

	r = sd_bus_request_name(bus, "org.watchdogd", 0);


	short change = 0;
	CreateDetachedThread(ReceivingThread, &change);

}
