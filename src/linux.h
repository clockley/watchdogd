/*
 * Copyright 2013-2014 Christian Lockley
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
#include <sys/types.h>
#include <ifaddrs.h>
#include <linux/if.h>
#include "errorlist.h"

int DisablePageFiles(void);
int RemountRootReadOnly(void);
int CloseWatchdog(watchdog_t *const);
int PingWatchdog(watchdog_t *const);
int GetOptimalPingInterval(watchdog_t * const);
int SaveRandomSeed(const char *);
const char *GetDefaultRandomSeedPathName(void);
watchdog_t *OpenWatchdog(const char *const);
int ConfigureWatchdogTimeout(watchdog_t *const , int);
int StopNetwork(void);
int _Shutdown(int, bool);
int NativeShutdown(int, int);
int LinuxRunningSystemd(void);
bool DontKillProcess(pid_t);
#endif
