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

#ifndef WATCHDOG_H
#define WATCHDOG_H
class Watchdog {
	const char path[64] = {'\0'};
	int fd = -1;
	int timeout = 0;
	int pingInterval = 0;
	bool CanMagicClose();
public:
	int Ping();
	int Close();
	int Open(const char *const);
	int GetOptimalPingInterval();
	long GetFirmwareVersion();
	bool PrintWdtInfo();
	unsigned char *Getdentity();
	int ConfigureWatchdogTimeout(int);
	int GetWatchdogBootStatus();
	long GetTimeleft();
	int GetRawTimeout();
	bool CheckWatchdogTimeout(int);
	long unsigned GetStatus();
	unsigned char * GetIdentity();
	int Disable();
	int Enable();
	void SetPingInterval(int i) {
		pingInterval = i;
	}
	int GetPingInterval() {
		return pingInterval;
	}
};
#endif
