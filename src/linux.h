/*
 * Copyright 2013 Christian Lockley
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
 * 
 */
#if !defined(LINUX_H) && defined(__linux__)
#define LINUX_H
#include <linux/types.h>
#include <stdbool.h>
#include <libmount.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <utmpx.h>
#include <utmp.h>
#include <sys/swap.h>
#include "errorlist.h"

int DisablePageFiles(void);
int RemountRootReadOnly(void);
int CloseWatchdog(watchdog_t *watchdog);
int PingWatchdog(watchdog_t *watchdog);
watchdog_t *OpenWatchdog(const char *path);
int ConfigureWatchdogTimeout(watchdog_t *watchdog, int timeout);
int _Shutdown(int errorcode, bool kexec);
#endif
