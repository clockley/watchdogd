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
#include <sys/wait.h>
#include "sub.hpp"
#include "init.hpp"
#include "pidfile.hpp"
#include "daemon.hpp"

static void CloseFileDescriptors(long maxfd)
{
	long fd = 0;

	while (fd < maxfd || fd == maxfd) {
		if (fd == STDIN_FILENO || fd == STDOUT_FILENO
		    || fd == STDERR_FILENO) {
			fd = fd + 1;
		} else {
			close((int)fd);
			fd = fd + 1;
		}
	}
}

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
