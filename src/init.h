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
#if !defined(INIT_H)
#define INIT_H
int MakeLogDir(struct cfgoptions *s);
int SetSchedulerPolicy(int priority);
int CheckPriority(int priority);
int InitializePosixMemlock(void);
int Usage(void);
int PrintVersionString(void);
int ReadConfigurationFile(struct cfgoptions *const cfg);
int ParseCommandLine(int *argc, char **argv, struct cfgoptions *s);
bool SetDefaultConfig(struct cfgoptions *options);
int GetDefaultPriority(void);
int PingInit(struct cfgoptions *const cfg);
#endif
