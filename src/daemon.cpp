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
#include "sub.hpp"
#include "init.hpp"
#include "pidfile.hpp"
#include "daemon.hpp"
#include "logutils.hpp"

int Daemonize(struct cfgoptions *const s)
{
	assert(s != NULL);

	if (s == NULL) {
		return -1;
	}

	if (IsDaemon(s) == 0) { //shall we daemonize?
		return 0;
	}

	kill(getppid(), SIGUSR1);
	SetLogTarget(SYSTEM_LOG);
	return 0;
}
