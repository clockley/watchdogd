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

#define DBUSAPI_PROTOTYPES
#include "dbusapi.hpp"
#include <errno.h>
#include <stdlib.h>
#include <zlib.h>
#define MAX_CLIENT_ID 4096
typedef uint64_t usec_t;

static sd_event *event = NULL;
static sd_bus *bus = NULL;

static sd_event_source *clients[MAX_CLIENT_ID] = {NULL};
static short freeIds[MAX_CLIENT_ID] = {-1};
static usec_t clientTimeout[MAX_CLIENT_ID] = {0};

static int lastAllocatedId = 0;
static int openSlots = MAX_CLIENT_ID;
static int lastFreedSlot = -1;

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
		SD_BUS_METHOD("ReloadConfig", "", "b", ReloadService, 0),
        SD_BUS_VTABLE_END
};

static int ReloadService(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	return sd_bus_reply_method_return(m, "b", kill(getppid(), SIGHUP) == 0);
}

static int Timeout(sd_event_source *source, usec_t usec, void *userdata)
{
	int cmd = DBUSHUTDOWN;

	do
		write(fd, &cmd, sizeof(long));
	while (errno != 0);

	return -1;
}

static int PmonRemove(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{

	int id = 0;
	sd_bus_message_read(m, "u", &id);

	char *sid = (char*)sd_event_source_get_userdata(clients[id]);
	if (sid == NULL) {
		return sd_bus_reply_method_return(m, "b", false);
	}

	if (strcmp((char*)sd_event_source_get_userdata(clients[id]), sd_bus_message_get_sender(m)) != 0) {
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

	char *sid = (char*)sd_event_source_get_userdata(clients[id]);
	if (sid == NULL) {
		return sd_bus_reply_method_return(m, "b", false);
	}

	if (strcmp((char*)sd_event_source_get_userdata(clients[id]), sd_bus_message_get_sender(m)) != 0) {
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

static int DevicePath(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	int cmd = DBUSGETPATH;
	write(fd, &cmd, sizeof(int));
	read(fd, path, sizeof(path));

	return sd_bus_reply_method_return(m, "s", path);
}

static int Identity(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	int cmd = DBUSGETNAME;
	write(fd, &cmd, sizeof(int));
	read(fd, identity, sizeof(identity));

	return sd_bus_reply_method_return(m, "s", identity);
}

static int Version(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	int cmd = DBUSVERSION;
	write(fd, &cmd, sizeof(long));
	read(fd, &version, sizeof(long));

	return sd_bus_reply_method_return(m, "x", version);
}

static int GetTimeoutDbus(sd_bus_message *m, void *userdata, sd_bus_error *retError)
{
	int cmd = DBUSGETIMOUT;
	write(fd, &cmd, sizeof(long));
	read(fd, &timeout, sizeof(long));

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

static int ReloadDbusDaemon(void)
{
	static const unsigned char cFile[] = {
		0x78, 0x9c, 0x8d, 0x8f, 0xc1, 0x4a, 0xc4, 0x30, 0x10, 0x86, 0xef, 0x0b,
		0xfb, 0x0e, 0xe3, 0xdc, 0xdb, 0x71, 0x6f, 0x22, 0xed, 0x2e, 0xb4, 0x5d,
		0x41, 0x10, 0x5d, 0xb0, 0x7b, 0xf0, 0x24, 0xb1, 0x49, 0xdb, 0x60, 0xcd,
		0x94, 0x24, 0x35, 0xfa, 0xf6, 0xa6, 0x15, 0x56, 0xc5, 0xcb, 0x1e, 0x33,
		0xf9, 0x66, 0xfe, 0xff, 0xcb, 0x76, 0x1f, 0x6f, 0x03, 0xbc, 0x2b, 0xeb,
		0x34, 0x9b, 0x1c, 0x37, 0xe9, 0x25, 0x82, 0x32, 0x0d, 0x4b, 0x6d, 0xba,
		0x1c, 0x8f, 0xf5, 0x4d, 0x72, 0x85, 0xbb, 0xed, 0x7a, 0x95, 0x5d, 0x54,
		0x0f, 0x65, 0xfd, 0x74, 0xd8, 0xc3, 0xcb, 0xe4, 0x1a, 0x36, 0xad, 0xee,
		0xe0, 0x70, 0x2c, 0xee, 0x6e, 0x4b, 0xc0, 0x84, 0xa8, 0xb5, 0x4a, 0x49,
		0xe5, 0x5e, 0x3d, 0x8f, 0x44, 0x55, 0x5d, 0x41, 0x95, 0x14, 0xc7, 0x47,
		0x28, 0x26, 0x07, 0xe5, 0x02, 0x4f, 0x56, 0xf8, 0x98, 0x00, 0x31, 0x80,
		0x68, 0x7f, 0x8f, 0xeb, 0x15, 0x60, 0xef, 0xfd, 0x78, 0x4d, 0x14, 0x42,
		0x48, 0x7f, 0xed, 0xa7, 0x6c, 0x3b, 0x72, 0x5e, 0x18, 0x29, 0xac, 0x74,
		0x24, 0x63, 0x1e, 0xcd, 0x5b, 0xa7, 0xdc, 0x54, 0x7a, 0x89, 0x73, 0xa5,
		0xd3, 0x24, 0x3e, 0x00, 0xb2, 0x91, 0x07, 0xdd, 0x7c, 0xc2, 0xe4, 0x94,
		0xcd, 0xd1, 0x32, 0x7b, 0x5c, 0xe6, 0xf1, 0x47, 0x0c, 0x03, 0x07, 0xe0,
		0x10, 0x05, 0xe3, 0xf1, 0x34, 0x08, 0xdf, 0xf4, 0x92, 0x3b, 0xb9, 0x41,
		0xfa, 0x8b, 0x38, 0x65, 0xe4, 0x73, 0xec, 0xe1, 0xb5, 0x59, 0xea, 0x9e,
		0xc5, 0x6b, 0xe3, 0x95, 0x6d, 0x45, 0xa3, 0xce, 0xa2, 0x47, 0xe1, 0xfb,
		0x1c, 0x69, 0x96, 0xfc, 0x47, 0x66, 0xf4, 0xed, 0x30, 0xcb, 0xfd, 0xf8,
		0x6e, 0xbf, 0x00, 0x9f, 0x86, 0x86, 0x3b
	};

	const uLong cFileLen = 247;
	Bytef *file = (Bytef*) calloc(1, 512);
	uLongf size = 512;
	uncompress(file, &size, cFile, cFileLen);

	sd_bus_error error;
	sd_bus_message *m = NULL;

	FILE *fp = fopen("/etc/dbus-1/system.d/watchdogd.conf", "wex");
	if (fp == NULL) {
		sd_bus_call_method(bus, "org.freedesktop.DBus", "/", "org.freedesktop.DBus", "ReloadConfig",&error, &m, "");
		free(file);
		return -1;
	}

	fputs((const char*)file, fp);
	fclose(fp);

	sd_bus_call_method(bus, "org.freedesktop.DBus", "/", "org.freedesktop.DBus", "ReloadConfig",&error, &m, "");
	free(file);
	return 0;
}

void * DbusApiInit(void * sock)
{
	fd = *((int*)sock);
	sd_event_source *busSource = NULL;
	sd_bus_slot *slot = NULL;

	int ret = sd_event_default(&event);

	char tmp = '0';
	read(fd, &tmp, sizeof(char));

	ret = sd_bus_open_system(&bus);

	ret = sd_bus_add_object_vtable(bus, &slot, "/org/watchdogd1",
				"org.watchdogd1", watchdogPmon, NULL);

	ret = sd_bus_request_name(bus, "org.watchdogd1", 0);

	if (ret < 0) {
		ReloadDbusDaemon();
		ret = sd_bus_request_name(bus, "org.watchdogd1", 0);
	}

	sd_event_add_io(event, &busSource, sd_bus_get_fd(bus), EPOLLIN, BusHandler, NULL);

	sd_event_loop(event);
	quick_exit(0);
}
