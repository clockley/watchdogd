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

#include "watchdogd.h"
#include <sys/wait.h>
#include "sub.h"
#include "init.h"
#include "pidfile.h"
#include "daemon.h"

static int CloseStandardFileDescriptors(void)
{
	int ret = 0;
	struct stat statBuf;

	int fd = open("/dev/null", O_RDWR);

	if (fd < 0) {
		Logmsg(LOG_CRIT, "Unable to open: /dev/null: %s:",
		       MyStrerror(errno));
		return -1;
	}

	if (fstat(fd, &statBuf) != 0) {
		Logmsg(LOG_CRIT, "Stat failed %s", MyStrerror(errno));
		goto error;
	} else if (S_ISCHR(statBuf.st_mode) == 0
		   && S_ISBLK(statBuf.st_mode) == 0) {
		Logmsg(LOG_CRIT, "/dev/null is not a unix device file");
		goto error;
	}

	ret = dup2(fd, STDIN_FILENO);
	if (ret < 0) {
		Logmsg(LOG_CRIT, "dup2 failed: STDIN_FILENO: %s",
		       MyStrerror(errno));
		goto error;
	}

	ret = dup2(fd, STDOUT_FILENO);
	if (ret < 0) {
		Logmsg(LOG_CRIT, "dup2 failed: STDOUT_FILENO: %s",
		       MyStrerror(errno));
		goto error;
	}

	ret = dup2(fd, STDERR_FILENO);
	if (ret < 0) {
		Logmsg(LOG_CRIT, "dup2 failed: STDERR_FILENO: %s",
		       MyStrerror(errno));
		goto error;
	}

	close(fd);
	return 0;
 error:
	close(fd);
	return -1;
}

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

	long maxfd = sysconf(_SC_OPEN_MAX);

	if (maxfd < 0 && errno == EINVAL) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		exit(EXIT_FAILURE);
	}

	if (maxfd < 0) {
		fprintf(stderr,
			"watchdogd: sysconf(_SC_OPEN_MAX) returned indefinite limit\n");
		fprintf(stderr,
			"watchdogd: will close file descriptors %u, %u, and %u\n",
			STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO);
		fprintf(stderr,
			"watchdogd: the state of other file descriptors is undefined\n");
	} else {
		CloseFileDescriptors(maxfd);
	}

	ResetSignalHandlers(NSIG);

	sigset_t sa;

	if (sigfillset(&sa) != 0) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		exit(EXIT_FAILURE);	//If sigfillset fails exit. Cannot use sa uninitialized.
	}

	if (sigprocmask(SIG_UNBLOCK, &sa, NULL) != 0) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		fprintf(stderr, "watchdogd: unable to reset signal mask\n");
	}

	errno = 0;

	pid_t pid = fork();

	if (pid < 0) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		if (waitpid(pid, NULL, 0) != pid) {
			fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
			_Exit(EXIT_FAILURE);
		}

		_Exit(EXIT_SUCCESS);
	}

	pid_t sid = setsid();

	if (sid < 0) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (signal(SIGHUP, SIG_IGN) == SIG_ERR) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		_Exit(EXIT_FAILURE);
	}

	pid = fork();

	if (pid < 0) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		_Exit(EXIT_SUCCESS);
	}

	if (signal(SIGHUP, SIG_DFL) == SIG_ERR) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (s->options & USEPIDFILE) {
		int pidcret = 0;
		s->pidfile.fd = OpenPidFile(s->pidfile.name);

		if (s->pidfile.fd < 0) {
			pidcret = -1;
		}

		if (LockFile(s->pidfile.fd, getpid()) < 0) {
			fprintf(stderr, "watchdogd: LockFile failed: %s\n",
				MyStrerror(errno));
			pidcret = -1;
		}

		if (WritePidFile(&s->pidfile, getpid()) < 0) {
			pidcret = -1;
		}
		if (pidcret != 0) {
			if (s->options & FORCE) {
				goto skipPidFile;
			} else {
				return pidcret;
			}
		}
	}

skipPidFile:

	if (chdir("/") < 0) {
		fprintf(stderr, "watchdogd: %s\n", MyStrerror(errno));
		exit(EXIT_FAILURE);
	}

	umask(0);

	if (CloseStandardFileDescriptors() < 0) {
		return -1;
	}

	StartLogger();

	return 0;
}