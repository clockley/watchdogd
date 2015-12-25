/*
 * Copyright 2013-2015 Christian Lockley
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
#include "exe.h"
#include "user.h"
#include "killtree.h"

int Spawn(int timeout, struct cfgoptions *const config, const char *file,
	  const char *args, ...)
{
	spawnattr_t attr = {.workingDirectory = NULL,.repairFilePathname = NULL,
		.execStart = NULL,.logDirectory = config->logdir,.user = 0,
		.group = NULL,.umask = 0,.timeout = timeout,.nice =
		    0,.noNewPrivileges = false,
		.hasUmask = false
	};
	va_list a;
	va_start(a, args);
	int ret = SpawnAttr(&attr, file, args, a);
	va_end(a);
	return ret;
}

int SpawnAttr(spawnattr_t * spawnattr, const char *file, const char *args, ...)
{
	pid_t intermediate = fork();

	if (intermediate == 0) {
		OnParentDeathSend(SIGKILL);
		pid_t worker = fork();
		if (worker == 0) {
#if defined(NSIG)
			ResetSignalHandlers(NSIG);
#endif

			if (nice(spawnattr->nice) == -1) {
				Logmsg(LOG_ERR, "nice failed: %s",
				       MyStrerror(errno));
			}

			int dfd = open(spawnattr->logDirectory,
				       O_DIRECTORY | O_RDONLY);

			if (dfd < 0) {
				Logmsg(LOG_CRIT,
				       "open failed: %s: %s",
				       spawnattr->logDirectory,
				       MyStrerror(errno));
				return -1;
			}

			int fd = openat(dfd, "repair.out",
					O_RDWR | O_APPEND | O_CREAT,
					S_IWUSR | S_IRUSR);

			if (fd < 0) {
				Logmsg(LOG_CRIT, "open failed: %s",
				       MyStrerror(errno));
				close(dfd);
				return -1;
			} else {
				close(dfd);
			}

			fsync(fd);

			va_list ap;
			const char *array[33] = { "\0" };
			int argno = 0;

			va_start(ap, args);

			while (args != NULL && argno < 32) {
				array[argno++] = args;
				args = va_arg(ap, const char *);
			}
			array[argno] = NULL;
			va_end(ap);

			if (spawnattr->workingDirectory != NULL) {
				if (chdir(spawnattr->workingDirectory) < 0) {
					Logmsg(LOG_CRIT,
					       "Unable to change working directory to: %s: %s",
					       spawnattr->workingDirectory,
					       MyStrerror(errno));
				}
			}

			if (RunAsUser(spawnattr->user, spawnattr->group) != 0) {
				Logmsg(LOG_CRIT,
				       "Unable to run: %s as user: %s", file,
				       spawnattr->user);
			}

			if (spawnattr->noNewPrivileges == true) {
				int ret = NoNewProvileges();
				if (ret != 0) {
					if (ret < 0) {
						Logmsg(LOG_CRIT,
						       "NoNewPrivileges %s",
						       MyStrerror(-ret));
					} else {
						Logmsg(LOG_CRIT,
						       "unable to set no new privleges bit");
					}
				}
			}

			if (spawnattr->hasUmask == true) {
				umask(spawnattr->umask);
			}

			if (dup2(fd, STDOUT_FILENO) < 0) {
				Logmsg(LOG_CRIT,
				       "dup2 failed: STDOUT_FILENO: %s",
				       MyStrerror(errno));
				return -1;
			}

			if (dup2(fd, STDERR_FILENO) < 0) {
				Logmsg(LOG_CRIT,
				       "dup2 failed: STDERR_FILENO %s",
				       MyStrerror(errno));
				return -1;
			}

			execv(file, (char *const *)array);

			Logmsg(LOG_CRIT, "execv failed %s", file);

			close(fd);
			return -1;
		}

		pid_t timer = fork();

		if (timer == 0) {
			int t = spawnattr->timeout;
			while (t > 0) {
				sleep(1);
				t -= 1;
			}
			_Exit(0);
		}

		pid_t first = wait(NULL);
		if (first == timer) {
			Logmsg(LOG_ERR, "binary %s exceeded time limit %ld",
			       file, spawnattr->timeout);
			kill(worker, SIGKILL);
			int status = 0;
			wait(&status);
			_Exit(EXIT_FAILURE);
		} else {
			kill(timer, SIGKILL);
			wait(NULL);
			_Exit(WEXITSTATUS(first));
		}
	} else {
		int status = 0;
		while (waitpid(intermediate, &status, 0) != intermediate) ;
		return status;
	}

	return 0;
}
