/*
 * Copyright 2013-2016 Christian Lockley
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
#include "linux.hpp"
#include <linux/if_link.h>

struct NetworkDevices {
	struct list head;
};

struct NetMonNode {
	struct list node;
	unsigned long long rx;
	char *name;
};

typedef struct NetMonNode NetMonNode;
static struct NetworkDevices networkDevices;

static bool NetMonIsValidNetworkInterface(const char *name)
{
	struct ifaddrs *ifaddr;

	getifaddrs(&ifaddr);

	for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_PACKET) {
			continue;
		}

		if (strcmp(name, ifa->ifa_name) == 0) {
			freeifaddrs(ifaddr);
			return true;
		}
	}

	freeifaddrs(ifaddr);

	return false;
}

bool NetMonCheckNetworkInterfaces(char **name)
{
	NetMonNode *c = NULL;
	NetMonNode *next = NULL;
	struct ifaddrs *ifaddr;
	*name = NULL;

	getifaddrs(&ifaddr);

	list_for_each_entry(c, next, &networkDevices.head, node) {
		for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr->sa_family != AF_PACKET) {
				continue;
			}

			if (strcmp(c->name, ifa->ifa_name) == 0) {
				assert(ifa->ifa_data != NULL);
				struct rtnl_link_stats *stats = (struct rtnl_link_stats*)ifa->ifa_data;
				if (stats->rx_bytes == c->rx) {
					*name = c->name;
					freeifaddrs(ifaddr);
					return false;
				}
				c->rx = stats->rx_bytes;
				break;
			}
		}
	}
	freeifaddrs(ifaddr);
	return true;
}

bool NetMonAdd(const char *name)
{
	if (NetMonIsValidNetworkInterface(name) == false) {
		return false;
	}

	struct NetMonNode *node = (struct NetMonNode*)calloc(1, sizeof(struct NetMonNode));

	if (node == NULL) {
		return false;
	}

	node->name = strdup(name);
	node->rx = -1;
	list_add(&node->node, &networkDevices.head);
	return true;
}

bool NetMonInit(void)
{
	list_init(&networkDevices.head);
	return true;
}
