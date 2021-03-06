/*
 * Copyright 2013-2020 Christian Lockley
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
#define SD_JOURNAL_SUPPRESS_LOCATION
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <linux/watchdog.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <ifaddrs.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-journal.h>

int _Shutdown(int, bool);
int NativeShutdown(int, int);
int LinuxRunningSystemd(void);
bool PlatformInit(void);
int GetConsoleColumns(void);
int SystemdWatchdogEnabled(pid_t *, long long int *const);
bool OnParentDeathSend(uintptr_t);
int NoNewProvileges(void);
int GetCpuCount(void);
bool LoadKernelModule(void);
bool MakeDeviceFile(const char *);
int ConfigWatchdogNowayoutIsSet(char *);
bool IsClientAdmin(int);
bool GetDeviceMajorMinor(struct dev *, char *);
char *FindBestWatchdogDevice(void);
#endif
