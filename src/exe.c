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
#include "exe.h"
#include "user.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void *WaitThread(void *arg)
{
	int *ret = (int *)arg;

	wait(ret);

	pthread_mutex_lock(&lock);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&lock);

	return NULL;
}

static void *ExitIfParentDied(void *arg)
{
	while (getppid() != 1) {
		sleep(1);
	}

	exit(0);
}

int Spawn(int timeout, struct cfgoptions *const config, const char *file,
	  const char *args, ...)
{
	int status = 0;

	if (file == NULL) {
		return -1;
	}

	pid_t pid = fork();

	switch (pid) {
	case -1:
		return -1;
	case 0:
		{
#if defined(NSIG)
			ResetSignalHandlers(NSIG);
#endif

#ifdef __linux__
			OnParentDeathSend(SIGKILL);
#else
			CreateDetachedThread(ExitIfParentDied, NULL);
#endif

			pid_t worker = fork();

			if (worker == 0) {
#ifdef __linux__
				OnParentDeathSend(SIGKILL);
#else
				CreateDetachedThread(ExitIfParentDied, NULL);
#endif
				struct sched_param param;
				param.sched_priority = 0;

				sched_setscheduler(getpid(), SCHED_OTHER,
						   &param);
				nice(0);

				int dfd = open(config->logdir,
					       O_DIRECTORY | O_RDONLY);

				if (dfd < 0) {
					Logmsg(LOG_CRIT,
					       "open failed: %s: %s",
					       config->logdir, strerror(errno));
					return -1;
				}

				int fd = openat(dfd, "repair.out",
						O_RDWR | O_APPEND | O_CREAT,
						S_IWUSR | S_IRUSR);

				if (fd < 0) {
					Logmsg(LOG_CRIT, "open failed: %s",
					       strerror(errno));
					close(dfd);
					return -1;
				} else {
					close(dfd);
				}

				fsync(fd);

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
				const char *array[33] = {"\0"};
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
						int ret =
						    pthread_mutex_unlock(&lock);

						if (ret == 1) {
							Logmsg(LOG_ERR, "%s",
							       strerror(errno));
							assert(ret != 0);
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
					//assert(ret != EXIT_SUCCESS); //Can't count on that.
					if (ret == EXIT_SUCCESS) {
						_Exit(EXIT_FAILURE);
					} else {
						assert(WEXITSTATUS(ret) !=
						       EXIT_SUCCESS);
						_Exit(WEXITSTATUS(ret));
					}
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

int SpawnAttr(spawnattr_t *spawnattr, const char *file, const char *args, ...)
{
	int status = 0;

	if (file == NULL) {
		return -1;
	}

	if (spawnattr == NULL) {
		return -1;
	}

	pid_t pid = fork();

	switch (pid) {
	case -1:
		return -1;
	case 0:
		{
#if defined(NSIG)
			ResetSignalHandlers(NSIG);
#endif

#ifdef __linux__
			OnParentDeathSend(SIGKILL);
#else
			CreateDetachedThread(ExitIfParentDied, NULL);
#endif

			pid_t worker = fork();

			if (worker == 0) {
#ifdef __linux__
				OnParentDeathSend(SIGKILL);
#else
				CreateDetachedThread(ExitIfParentDied, NULL);
#endif
				struct sched_param param;
				param.sched_priority = 0;

				sched_setscheduler(getpid(), SCHED_OTHER,
						   &param);

				if (nice(spawnattr->nice) == -1) {
					Logmsg(LOG_ERR, "nice failed: %s", strerror(errno));
				}

				int dfd = open(spawnattr->logDirectory,
					       O_DIRECTORY | O_RDONLY);

				if (dfd < 0) {
					Logmsg(LOG_CRIT,
					       "open failed: %s: %s",
					       spawnattr->logDirectory, strerror(errno));
					return -1;
				}

				int fd = openat(dfd, "repair.out",
						O_RDWR | O_APPEND | O_CREAT,
						S_IWUSR | S_IRUSR);

				if (fd < 0) {
					Logmsg(LOG_CRIT, "open failed: %s",
					       strerror(errno));
					close(dfd);
					return -1;
				} else {
					close(dfd);
				}

				fsync(fd);

				va_list ap;
				const char *array[33] = {"\0"};
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
						Logmsg(LOG_CRIT, "Unable to change working directory to: %s: %s",
						spawnattr->workingDirectory, strerror(errno));
					}
				}

				if (RunAsUser(spawnattr->user, spawnattr->group) != 0) {
					Logmsg(LOG_CRIT, "Unable to run: %s as user: %s", file, spawnattr->user);
				}

				if (spawnattr->noNewPrivileges == true) {
					int ret = NoNewProvileges();
					if (ret != 0) {
						if (ret < 0) {
							Logmsg(LOG_CRIT, "NoNewPrivileges %s", strerror(-ret));
						} else {
							Logmsg(LOG_CRIT, "unable to set no new privleges bit");
						}
					}

				}

				if (spawnattr->umask != NULL) {
					long long ret = strtoll(spawnattr->umask, (char **)NULL, 10);

					if (ret < 0) {
						Logmsg(LOG_ERR, "error parsing configfile option \"Umask\": %s", strerror(errno));
					}

					mode_t mode = (mode_t)ret;

					umask(mode);
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

				execv(file, (char *const *)array);

				Logmsg(LOG_CRIT, "execv failed %s",
				       file);

				close(fd);
				return -1;
			}

			if (worker == -1) {
				return -1;
			}

			if (spawnattr->timeout > 0) {
				int ret = 0;

				pthread_mutex_lock(&lock);

				if (CreateDetachedThread(WaitThread, &ret) < 0) {
					Logmsg(LOG_ERR, "%s", strerror(errno));
					return -1;
				}

				struct timespec rqtp;

				clock_gettime(CLOCK_REALTIME, &rqtp);

				rqtp.tv_sec += (time_t) spawnattr->timeout;
				NormalizeTimespec(&rqtp);

				bool once = false;
				int returnValue = 0;

				errno = 0;
				do {
					returnValue =
					    pthread_cond_timedwait(&cond, &lock,
								   &rqtp);
					if (once == false) {
						int ret =
						    pthread_mutex_unlock(&lock);

						if (ret == 1) {
							Logmsg(LOG_ERR, "%s",
							       strerror(errno));
							assert(ret != 0);
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
					       file, spawnattr->timeout);
					//assert(ret != EXIT_SUCCESS); //Can't count on that.
					if (ret == EXIT_SUCCESS) {
						_Exit(EXIT_FAILURE);
					} else {
						assert(WEXITSTATUS(ret) !=
						       EXIT_SUCCESS);
						_Exit(WEXITSTATUS(ret));
					}
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
