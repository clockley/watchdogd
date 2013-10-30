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

#include <sys/wait.h>
#include <stdarg.h>
#include "watchdogd.h"
#include "sub.h"

int Spawn(int timeout, void *aarg, const char *file, const char *args, ...)
{
	struct cfgoptions *s = aarg;
	int status = 0;

	pid_t pid = fork();

	switch (pid) {
	case -1:
		return -1;
	case 0:
		{
#if defined(_NSIG)
			ResetSignalHandlers(_NSIG);
#endif

			pid_t worker = fork();

			if (worker == 0) {
				int dfd =
				    open(s->logdir, O_DIRECTORY | O_RDONLY);

				if (dfd < 0) {
					Logmsg(LOG_CRIT,
					       "open failed: %s: %s", s->logdir,
					       strerror(errno));
				}

				int fd = openat(dfd, "repair.out",
						O_RDWR | O_APPEND | O_CREAT,
						S_IWUSR | S_IRUSR);

				if (fd < 0) {
					Logmsg(LOG_CRIT, "open failed: %s",
					       strerror(errno));
					return -1;
				} else {
					close(dfd);
				}

				if (dup2(fd, STDOUT_FILENO) < 0) {
					Logmsg(LOG_CRIT,
					       "dup2 failed: STDOUT_FILENO: %s",
					       strerror(errno));
					return -1;
				}

				if (dup2(fd, STDERR_FILENO) < 0) {
					Logmsg(LOG_CRIT,
					       "dup2 failed: STDERR_FILENO %s",
					       strerror(errno));
					return -1;
				}

				va_list ap;
				const char *array[33];
				int argno = 0;

				va_start(ap, args);

				while (args != NULL && argno < 32) {
					array[argno++] = args;
					args = va_arg(ap, const char *);
				}
				array[argno] = NULL;
				va_end(ap);

				execv(file, (char *const *)array);

				close(fd);
				return -1;
			}

			if (worker == -1) {
				return -1;
			}

			int ret = 0;

			if (timeout > 0) {
				pid_t timer = fork();

				if (timer == 0) {
					struct timespec rqtp;

					rqtp.tv_sec = timeout;
					rqtp.tv_nsec = timeout * 1000;

					nanosleep(&rqtp, &rqtp);

					_Exit(0);
				}

				if (timer == -1) {
					return -1;
				}

				pid_t firstoexit = wait(&ret);

				if (firstoexit == timer) {
					kill(worker, SIGKILL);
					Logmsg(LOG_ERR,
					       "binary %s exceeded time limit %ld",
					       file, timeout);
					_Exit(0);
				} else if (firstoexit == worker) {
					kill(timer, SIGKILL);
					_Exit(WEXITSTATUS(ret));
				} else {
					_Exit(0);
				}
			} else {
				wait(&ret);
				_Exit(WEXITSTATUS(ret));
			}
		}
	default:
		errno = 0;
		if (waitpid(pid, &status, 0) != pid && errno == EINTR) {
			kill(pid, SIGKILL);
			return 0;
		}

		if (WIFEXITED(status) == 1 && WEXITSTATUS(status) == 0) {
			return 0;
		}

		return WEXITSTATUS(status);
	}
}
