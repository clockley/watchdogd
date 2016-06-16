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

#define DBUSAPI_PROTOTYPES
#include "dbusapi.h"
#define MAX_CLIENT_ID 4096
typedef uint64_t usec_t;

static sd_event *event = NULL;
static sd_bus *bus = NULL;

static sd_event_source *clients[MAX_CLIENT_ID] = {NULL};
static short freeIds[MAX_CLIENT_ID] = {-1};
static usec_t clientTimeout[MAX_CLIENT_ID];

static _Atomic(int) lastAllocatedId = 0;
static _Atomic(int) openSlots = MAX_CLIENT_ID;
static _Atomic(int) lastFreedSlot = -1;

static long firmwareVersion = 0;
static long watchdogTimeout = 0;
static int fd = 0;
static char identity[64] = {'0'};
static char path[64] = {'0'};
static long version = 0;
static long timeout = 0;

static const sd_bus_vtable watchdogPmon[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("DevicePath", "", "s", DevicePath, 0),
        SD_BUS_METHOD("Identity", "", "s", Identity, 0),
        SD_BUS_METHOD("Version", "", "x", Version, 0),
        SD_BUS_METHOD("GetTimeout", "", "x", GetTimeoutDbus, 0),
        SD_BUS_METHOD("GetTimeleft", "", "x", GetTimeleftDbus, 0),
        SD_BUS_METHOD("PmonInit", "t", "u", PmonInit, 0),
        SD_BUS_METHOD("PmonPing", "u", "b", PmonPing, 0),
        SD_BUS_METHOD("PmonRemove", "u", "b", PmonRemove, 0),
        SD_BUS_VTABLE_END
};

static int Timeout(sd_event_source *source, usec_t usec, void *userdata) 
{
	int cmd = DBUSHUTDOWN;

	write(fd, &cmd, sizeof(long));

	return -1;
}

static int PmonRemove(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{

	int id = 0;
	sd_bus_message_read(m, "u", &id);

	char *sid = sd_event_source_get_userdata(clients[id]);
	if (sid == NULL) {
		return sd_bus_reply_method_return(m, "b", false);
	}

	if (strcmp(sd_event_source_get_userdata(clients[id]), sd_bus_message_get_sender(m)) != 0) {
		return sd_bus_reply_method_return(m, "b", false);
	}

	sd_event_source_set_enabled(clients[id], SD_EVENT_OFF);
	sd_event_source_unref(clients[id]); //systemd will gc object after unrefing it.
	clients[id] = NULL;
	lastFreedSlot += 1;
	openSlots += 1;
	freeIds[lastFreedSlot] = id;
	return sd_bus_reply_method_return(m, "b", true);
}

static int PmonPing(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	int id = 0;

	sd_bus_message_read(m, "u", &id);

	char *sid = sd_event_source_get_userdata(clients[id]);
	if (sid == NULL) {
		return sd_bus_reply_method_return(m, "b", false);
	}

	if (strcmp(sd_event_source_get_userdata(clients[id]), sd_bus_message_get_sender(m)) != 0) {
		return sd_bus_reply_method_return(m, "b", false);
	}


	uint64_t usec = 0;
	sd_event_now(event, CLOCK_MONOTONIC, &usec);
	sd_event_source_set_time(clients[id], clientTimeout[id]+usec);

	return sd_bus_reply_method_return(m, "b", true);
}

static int PmonInit(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	usec_t time = 0;
	sd_bus_message_read(m, "t", &time);
	int id = 0;
	if (openSlots == 0) {
		return sd_bus_reply_method_return(m, "u", -1);
	}

	uint64_t usec = 0;
	sd_event_now(event, CLOCK_MONOTONIC, &usec);
	usec += time;
	if (clients[lastAllocatedId] == NULL) {
		id = lastAllocatedId;

		sd_event_add_time(event, &clients[lastAllocatedId], CLOCK_MONOTONIC, usec, 1000,
					Timeout, (void *)sd_bus_message_get_sender(m));
		lastAllocatedId += 1;
		openSlots -= 1;
		clientTimeout[id] = time;
		return sd_bus_reply_method_return(m, "u", id);
	}

	id = freeIds[lastFreedSlot];

	sd_event_add_time(event, &clients[freeIds[lastFreedSlot]], CLOCK_MONOTONIC, usec, 1000,
				Timeout, (void *)sd_bus_message_get_sender(m));

	lastFreedSlot -= 1;
	openSlots -= 1;
	clientTimeout[id] = time;
	return sd_bus_reply_method_return(m, "u", id);
}

static void DevicePathInit(void)
{
	int cmd = DBUSGETPATH;

	write(fd, &cmd, sizeof(int));
	read(fd, path, sizeof(path));
}

static int DevicePath(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	static pthread_once_t initPath = PTHREAD_ONCE_INIT;
	pthread_once(&initPath, DevicePathInit);
	return sd_bus_reply_method_return(m, "s", path);
}

static void IdentityInit(void)
{
	int cmd = DBUSGETNAME;
	write(fd, &cmd, sizeof(int));
	read(fd, identity, sizeof(identity));
}

static int Identity(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	static pthread_once_t initIdentity = PTHREAD_ONCE_INIT;
	pthread_once(&initIdentity, IdentityInit);
	return sd_bus_reply_method_return(m, "s", identity);
}

static void VersionInit(void)
{
	int cmd = DBUSVERSION;
	write(fd, &cmd, sizeof(long));
	read(fd, &version, sizeof(long));
}

static int Version(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	static pthread_once_t initVersion = PTHREAD_ONCE_INIT;
	pthread_once(&initVersion, VersionInit);
	return sd_bus_reply_method_return(m, "x", version);
}

static void InitGetTimeoutDbus(void)
{
	int cmd = DBUSGETIMOUT;

	write(fd, &cmd, sizeof(long));
	read(fd, &timeout, sizeof(long));
}

static int GetTimeoutDbus(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	static pthread_once_t initTimeout = PTHREAD_ONCE_INIT;
	pthread_once(&initTimeout, InitGetTimeoutDbus);
	return sd_bus_reply_method_return(m, "x", timeout);
}

static int GetTimeleftDbus(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	int cmd = DBUSTIMELEFT;
	long buf = 0;

	write(fd, &cmd, sizeof(long));
	read(fd, &buf, sizeof(long));

	return sd_bus_reply_method_return(m, "x", buf);
}

static int BusHandler(sd_event_source *es, int fd, uint32_t revents, void *userdata)
{
	sd_bus_process(bus, NULL);
	return 1;
}

void DbusApiInit(int sock)
{
	fd = sock;
	sd_event_source *busSource = NULL;
	sd_bus_slot *slot = NULL;

	int ret = sd_event_default(&event);

	ret = sd_bus_open_system(&bus);

	ret = sd_bus_add_object_vtable(bus, &slot, "/org/watchdogd1",
				"org.watchdogd1", watchdogPmon, NULL);
	ret = sd_bus_request_name(bus, "org.watchdogd1", 0);

	sd_event_add_io(event, &busSource, sd_bus_get_fd(bus), EPOLLIN, BusHandler, NULL);

	sd_event_loop(event);
}
