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

#include "watchdogd.h"
#include <sys/wait.h>
#include "sub.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void *WaitThread(void *arg);
static void *WaitThread(void *arg)
{
	int *ret = (int *)arg;

	wait(ret);

	pthread_mutex_lock(&lock);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);

	return NULL;
}

int Spawn(int timeout, struct cfgoptions *s, const char *file, const char *args,
	  ...)
{
	int status = 0;

	pid_t pid = fork();

	switch (pid) {
	case -1:
		return -1;
	case 0:
		{
#if defined(NSIG)
			ResetSignalHandlers(NSIG);
#endif

			pid_t worker = fork();

			if (worker == 0) {
				struct sched_param param;
				param.sched_priority = 0;

				sched_setscheduler(getpid(), SCHED_OTHER,
						   &param);
				nice(0);

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

				fsync(fd);

				if (fd < 0) {
					Logmsg(LOG_CRIT, "open failed: %s",
					       strerror(errno));
					close(dfd);
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

				Logmsg(LOG_CRIT, "execv failed %s",
				       strerror(errno));

				close(fd);
				return -1;
			}

			if (worker == -1) {
				return -1;
			}

			if (timeout > 0) {
				int ret = 0;

				pthread_mutex_lock(&lock);

				if (CreateDetachedThread(WaitThread, &ret) < 0) {
					Logmsg(LOG_ERR, "%s", strerror(errno));
					return -1;
				}

				struct timespec rqtp;

				clock_gettime(CLOCK_REALTIME, &rqtp);

				rqtp.tv_sec += (time_t) timeout;
				NormalizeTimespec(&rqtp);

				bool once = false;
				int returnValue = 0;

				errno = 0;
				do {
					returnValue =
					    pthread_cond_timedwait(&cond, &lock,
								   &rqtp);
					if (once == false) {
						if (pthread_mutex_unlock(&lock)
						    != 0) {
							Logmsg
							    (LOG_ERR,
							     "pthread_mutex_unlock failed aborting %s, %i",
							     __FILE__,
							     __LINE__);
							abort();
						}
						once = true;
					}
				} while (returnValue != ETIMEDOUT
					 && kill(worker, 0) == 0 && errno == 0);

				if (returnValue == ETIMEDOUT) {
					kill(worker, SIGKILL);
					Logmsg(LOG_ERR,
					       "binary %s exceeded time limit %ld",
					       file, timeout);
					_Exit(0);
				} else if (returnValue != 0) {
					Logmsg(LOG_ERR,
					       "unknown error in timeout code");
				}

				_Exit(WEXITSTATUS(ret));
			} else {
				int ret = 0;
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

		return WEXITSTATUS(status);
	}
}

/*int WaitOnProcesses(void *arg1, void *arg)
{
	assert(arg != NULL);
	struct listOfRunningProcess *p = arg1;
	struct cfgoptions *s = arg;

	struct process *c = NULL;
	struct process *next = NULL;

	list_for_each_entry(c, next, &p->children, entry) {
		pid_t pid = 0;
		int ret = 0;
		do
		{
			pid = waitpid(c->pid, &ret, WNOHANG);
			sleep(1);
		} while (pid == -1 && errno == EINTR);

		if (pid == c->pid) {
			if ((WIFEXITED(ret) != 0 && WEXITSTATUS(ret) == 0) || WIFSIGNALED(ret) != 0) {
				list_del(&c->entry);
				free((void *)c->name);
				free(c);
			} else {
				c->pid = WEXITSTATUS(ret);
			}
		}

		if (pid == 0 && difftime(time(NULL), c->starttime) < c->timeout) {
			kill(pid, SIGKILL);
			list_del(&c->entry);
			free((void *)c->name);
			free(c);
		}
	}

	return 0;
}

int SpawnAsync(void *aarg, void *arg, const char *file, const char *args, ...)
{
	struct listOfRunningProcess *p = arg;
	struct cfgoptions *s = aarg;
	int status = 0;

	pid_t pid = fork();

	switch (pid) {
	case -1:
		return -1;
	case 0:
		{
#if defined(NSIG)
			ResetSignalHandlers(NSIG);
#endif
			struct sched_param param;
			param.sched_priority = 0;

			sched_setscheduler(getpid(), SCHED_OTHER, &param);
			nice(0);

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
			va_end(ap);

			execv(file, (char *const *)array);

			close(fd);
			return -2;
		}
	default:
		{
			struct process *process = malloc(sizeof(*process));

			if (process == NULL)
				return -1;

			Wasprintf((char **)&process->name, "%s", file);

			if (time(&process->starttime) == (time_t)-1)
				return -1;

			process->pid = pid;
			process->ret = 0;

			if (s->testBinTimeout)
				process->timeout = s->testBinTimeout;
			else
				process->timeout = -1

			list_add(&process->entry, &p->children);
		}

		return 0;
	}
}*/
