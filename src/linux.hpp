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
#include "errorlist.hpp"
#include <systemd/sd-daemon.h>
#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

int DisablePageFiles(void);
int RemountRootReadOnly(void);
int SaveRandomSeed(const char *);
const char *GetDefaultRandomSeedPathName(void);
int StopNetwork(void);
int _Shutdown(int, bool);
int NativeShutdown(int, int);
int LinuxRunningSystemd(void);
bool DontKillProcess(pid_t);
bool PlatformInit(void);
int GetConsoleColumns(void);
int SystemdWatchdogEnabled(const int, long long int *const);
bool OnParentDeathSend(uintptr_t);
int NoNewProvileges(void);
int GetCpuCount(void);
bool LoadKernelModule(void);
bool MakeDeviceFile(const char *);
int ConfigWatchdogNowayoutIsSet(char *);
bool IsClientAdmin(int);
bool KillAllOtherThreads(void);
#endif
