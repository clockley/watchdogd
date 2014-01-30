/*
 * Copyright 2013 Christian Lockley
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

#define _BSD_SOURCE
#include "watchdogd.h"
#include "sub.h"
#include "exe.h"
#include "errorlist.h"

int KillAll(void);
int TermAll(void);
int StartInit(void);
int StopInit(void);

int Shutdown(int errorcode, struct cfgoptions *arg)
{
	if (arg->options & NOACTION) {
		Logmsg(LOG_DEBUG, "shutdown() errorcode=%i, kexec=%s",
		       errorcode, arg->options & KEXEC ? "true" : "false");
		return 0;
	}

/*	if (errorcode == WECMDRESET) {
		kill(getpid(), SIGUSR1);

		struct timespec rqtp;

		rqtp.tv_sec = 960;
		rqtp.tv_nsec = 960 * 1000;

		nanosleep(&rqtp, &rqtp);

		_Shutdown(errorcode, false);

		abort();
	}*/

	if (errorcode != WECMDREBOOT && errorcode != WECMDRESET) {
		char buf[64] = { "\0" };
		snprintf(buf, sizeof(buf), "%d\n", errorcode);

		if (Spawn
		    (arg->repairBinTimeout, arg, arg->exepathname,
		     arg->exepathname, buf, NULL) == 0)
			return 0;
	}

	EndDaemon(0, arg, true);	//point of no return

	StopInit();

	for (int i = 0; i < _NSIG; i += 1) {
		signal(i, SIG_IGN);
	}

	int i = 0;
	while (TermAll() == -1 && i < 2)
		i += 1;

	sleep(2);
	sync();
	sleep(3);

	for (int j = 0; j < 5; j += 1) {
		KillAll();
	}

	WriteUserAccountingDatabaseRecord(errorcode ==
					  WECMDREBOOT ? true : false);

	acct(NULL);		//acct not in POSIX

	DisablePageFiles();

	UnmountAll();

	RemountRootReadOnly();

	return _Shutdown(errorcode, arg->options & KEXEC ? 1 : 0);
}

int KillAll(void)
{
	sync();
	if (kill(-1, SIGKILL) != 0) {
		if (errno != ESRCH)
			return -1;
	}

	sync();
	return 0;
}

int TermAll(void)
{
	if (kill(-1, SIGTERM) != 0) {
		if (errno != ESRCH)
			return -1;
	}

	return 0;
}

int StartInit(void)
{
	if (kill(1, SIGCONT) == -1)
		return -1;

	return 0;
}

int StopInit(void)
{
	if (kill(1, SIGTSTP) == -1)
		return -1;

	return 0;
}
