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

#include "watchdogd.h"
#ifndef REPAIR_H
#define REPAIR_H
bool LoadRepairScriptLink(repair_t *obj, char * const filename);
char * RepairScriptGetExecStart(repair_t *obj);
char *RepairScriptGetUser(repair_t *obj);
long RepairScriptGetTimeout(repair_t *obj);
bool DestroyRepairScriptObj(repair_t *obj, int internalOnly);
int IsRepairScriptConfig(const char *filename);
#endif
