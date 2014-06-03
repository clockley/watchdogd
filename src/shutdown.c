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

#define _BSD_SOURCE
#include "watchdogd.h"
#include "sub.h"
#include "exe.h"
#include "errorlist.h"

static int StartInit(void)
{
	if (kill(1, SIGCONT) == -1)
		return -1;

	return 0;
}

static int StopInit(void)
{
	if (kill(1, SIGTSTP) == -1)
		return -1;

	return 0;
}

static long ConvertStringToInt(const char *const str)
{
	assert(str != NULL);return strtol((str), (char **)NULL, 10);
}

static void KillAllProcesses(int sig)
{
	DIR *dirp = opendir("/proc/");

	errno = 0;

	for (struct dirent *ent = ent; ent != NULL; ent = readdir(dirp)) {
		pid_t ret = (pid_t)ConvertStringToInt(ent->d_name);
		if (ret == 0 || errno == EINVAL) {
			errno = 0;
			continue;
		} else {
			if (getsid(getpid()) == getsid(ret)) {
				continue;
			}

			if (ret == getpid()) {
				continue;
			}

			if (ret == 1) {
				continue;
			}

			kill(-ret, sig);
		}
	}

	closedir(dirp);
}

static void WriteUtmpx(int reboot)
{
	struct utmpx utmpxStruct = { 0 };

	strncpy(utmpxStruct.ut_user, reboot == 1 ? "reboot" : "shutdown",
		sizeof(utmpxStruct.ut_user) - 1);
	strncpy(utmpxStruct.ut_line, "~", sizeof(utmpxStruct.ut_user) - 1);
	strncpy(utmpxStruct.ut_id, "~~", sizeof(utmpxStruct.ut_id) - 1);
	utmpxStruct.ut_pid = 0;
	utmpxStruct.ut_type = RUN_LVL;
	gettimeofday(&utmpxStruct.ut_tv, NULL);

	setutxent();

	pututxline(&utmpxStruct);

	endutxent();
}

int Shutdown(int errorcode, struct cfgoptions *arg)
{
	if (arg->options & NOACTION) {
		Logmsg(LOG_DEBUG, "shutdown() errorcode=%i, kexec=%s",
		       errorcode, arg->options & KEXEC ? "true" : "false");
		return 0;
	}

	if (errorcode != WECMDREBOOT && errorcode != WECMDRESET) {
		char buf[64] = { "\0" };
		snprintf(buf, sizeof(buf), "%d\n", errorcode);

		if (Spawn
		    (arg->repairBinTimeout, arg, arg->exepathname,
		     arg->exepathname, buf, NULL) == 0)
			return 0;
	}

	EndDaemon(arg, true);	//point of no return

	StopInit();

	sigset_t mask;

	sigfillset(&mask);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

	setsid();

	sync();

	KillAllProcesses(SIGTERM);

	sync();

	KillAllProcesses(SIGKILL);

	WriteUtmpx(errorcode == WECMDREBOOT ? true : false);

	acct(NULL);		//acct not in POSIX

	DisablePageFiles();

	UnmountAll();

	RemountRootReadOnly();

	return _Shutdown(errorcode, arg->options & KEXEC ? 1 : 0);
}
