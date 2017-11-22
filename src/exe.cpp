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
#include "logutils.hpp"
#include "linux.hpp"
#include "sub.hpp"
#include "exe.hpp"
#include "user.hpp"

int Spawn(int timeout, struct cfgoptions *const config, const char *file,
	  const char *args, ...)
{
	static spawnattr_t attr;

	if (file == NULL) {
		return -1;
	}

	va_list a;
	va_start(a, args);
	int ret = SpawnAttr(&attr, file, args, a);
	va_end(a);
	return ret;
}

int SpawnAttr(spawnattr_t * spawnattr, const char *file, const char *args, ...)
{
	pid_t intermediate = fork();
	pid_t mpid = getppid();
	if (intermediate == 0) {

		OnParentDeathSend(SIGKILL);
		pid_t worker = fork();
		if (worker == 0) {
#if defined(NSIG)
			ResetSignalHandlers(NSIG);
#endif
			setpgid(0, mpid);
			if (nice(spawnattr->nice) == -1) {
				Logmsg(LOG_ERR, "nice failed: %s",
				       MyStrerror(errno));
			}

			char buf[512] = {"watchdogdRepairScript="};
			strncat(buf, file, sizeof(buf) - strlen(buf) - 1);
			buf[511] = '\0';

			int fd = sd_journal_stream_fd(buf, LOG_INFO, true);

			if (fd < 0) {
				Logmsg(LOG_CRIT, "Unable to open log file for helper executable");
			}

			va_list ap;
			const char *array[64] = { "\0" };
			int argno = 0;

			va_start(ap, args);

			while (args != NULL && argno < 63) {
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

		char stack[2048] = {0};
		if (spawnattr->timeout > 0) {
			pid_t timer = clone([](void *s)->int {
				struct timeval tv;
				tv.tv_sec = *(int*)s;
				syscall(SYS_select, 0, NULL, NULL, NULL, &tv);
				_Exit(0);
			}, stack+sizeof(stack), CLONE_VM|CLONE_FILES|CLONE_FS|SIGCHLD, &spawnattr->timeout);
			if (timer < 0) {
				abort();
			}

			int ret = 0;
			pid_t first = wait(&ret);
			if (first == timer) {
				Logmsg(LOG_ERR, "binary %s exceeded time limit %i",
				       file, spawnattr->timeout);
				kill(worker, SIGKILL);
				wait(NULL);
				_Exit(EXIT_FAILURE);
			} else {
				syscall(SYS_tgkill, timer, timer, SIGKILL);
				wait(NULL);
				_Exit(WEXITSTATUS(ret));
			}
		} else {
			int ret = 0;
			wait(&ret);
			_Exit(WEXITSTATUS(ret));
		}
	} else {
		int status = 0;
		while (waitpid(intermediate, &status, 0) != intermediate) ;
		return WEXITSTATUS(status);
	}

	return 0;
}
