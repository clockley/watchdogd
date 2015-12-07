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
#include <sys/prctl.h>
#include <linux/watchdog.h>
#include <sys/reboot.h>
#include <sys/sysinfo.h>
#include <linux/reboot.h>
#include <utmpx.h>
#include <utmp.h>
#include <sys/swap.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <linux/if.h>
#include "errorlist.h"

#ifdef HAVE_SD_NOTIFY
#include <systemd/sd-daemon.h>
#endif

#ifdef HAVE_SD_JOURNAL
#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>
#endif

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
bool PlatformInit(void);
int GetConsoleColumns(void);
int SystemdWatchdogEnabled(const int, long long int *const);
bool OnParentDeathSend(int);
int NoNewProvileges(void);
int GetCpuCount(void);
bool LoadKernelModule(void);
bool MakeDeviceFile(const char *);
int ConfigWatchdogNowayoutIsSet(void);
bool PrintWdtInfo(watchdog_t * const);
int GetWatchdogBootStatus(watchdog_t * const);
int GetRawTimeout(watchdog_t * const wdt);
char *GetWatchdogIdentity(watchdog_t * const wdt);
#endif
