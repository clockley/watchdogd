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

#ifndef DBUSAPI_H
#define DBUSAPI_H
#include "watchdogd.h"
#include "linux.h"
#include "sub.h"
#include "logutils.h"
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

void DbusApiInit(watchdog_t * const watchdog, struct cfgoptions *const cfg);
static int DevicePath(sd_bus_message *, void *, sd_bus_error *);
static int Identity(sd_bus_message *, void *, sd_bus_error *);
static int Version(sd_bus_message *, void *, sd_bus_error *);
static int GetTimeoutDbus(sd_bus_message *, void *, sd_bus_error *);
static int GetTimeleftDbus(sd_bus_message *, void *, sd_bus_error *);
#endif
