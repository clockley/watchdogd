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
const int MAX_WORKER_THREADS = 16;
#include "watchdogd.h"
#include "sub.h"
#include <dirent.h>
#include "testdir.h"
#include "exe.h"
#include "repair.h"

//The dirent_buf_size function was written by Ben Hutchings and released under the following license.

//Permission is hereby granted, free of charge, to any person obtaining a copy of this
//software and associated documentation files (the "Software"), to deal in the Software
//without restriction, including without limitation the rights to use, copy, modify, merge,
//publish, distribute, sublicense, and/or sell copies of the Software, and to permit
//persons to whom the Software is furnished to do so, subject to the following
//condition:
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//DEALINGS IN THE SOFTWARE.

//Copied into this program Sun September 8, 2013 by Christian Lockley

size_t DirentBufSize(DIR * dirp)
{
	long name_max;
	size_t name_end;
#if defined(HAVE_FPATHCONF) && defined(HAVE_DIRFD) \
&& defined(_PC_NAME_MAX)
	name_max = fpathconf(dirfd(dirp), _PC_NAME_MAX);
	if (name_max == -1)
#if defined(NAME_MAX)
		name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#else
		return (size_t) (-1);
#endif
#else
#if defined(NAME_MAX)
	name_max = (NAME_MAX > 255) ? NAME_MAX : 255;
#else
#error "buffer size for readdir_r cannot be determined"
#endif
#endif
	name_end =
	    (size_t) offsetof(struct dirent,
			      d_name) + (unsigned long)name_max + 1;
	return (name_end > sizeof(struct dirent)
		? name_end : sizeof(struct dirent));
}

int CreateLinkedListOfExes(const char *repairScriptFolder, ProcessList * p,
			   struct cfgoptions *const config)
{
	assert(p != NULL);
	assert(repairScriptFolder != NULL);

	struct stat buffer;
	struct dirent *ent = NULL;
	struct dirent *direntbuf = NULL;

	size_t size = 0;
	int fd = 0;

	list_init(&p->head);

	fd = open(repairScriptFolder, O_DIRECTORY | O_RDONLY);

	if (fd == -1) {
		fprintf(stderr, "watchdogd: %s: %s\n", repairScriptFolder,
			MyStrerror(errno));
		return 1;
	}

	DIR *dir = fdopendir(fd);

	if (dir == NULL) {
		Logmsg(LOG_ERR, "watchdogd: %s", MyStrerror(errno));
		close(fd);
		return -1;
	}

	size = DirentBufSize(dir);

	int error = 0;

	if (size == ((size_t) (-1))) {
		goto error;
	}

	direntbuf = (struct dirent *)calloc(1, size);

	if (direntbuf == NULL) {
		goto error;
	}

	errno = 0;

	while ((error = readdir_r(dir, direntbuf, &ent)) == 0 && ent != NULL) {

		int statfd = dirfd(dir);

		if (statfd == -1) {
			goto error;
		}

		if (strchr(".", ent->d_name[0]) != NULL) {
			continue;
		}

		if (fstatat(statfd, ent->d_name, &buffer, 0) < 0)
			continue;

		if (IsRepairScriptConfig(ent->d_name) == 0) {
			if (S_ISREG(buffer.st_mode) == 0) {
				continue;
			}

			if (!(buffer.st_mode & S_IXUSR))
				continue;

			if (!(buffer.st_mode & S_IRUSR))
				continue;
		} else {
			if (S_ISREG(buffer.st_mode) == 0) {
				continue;
			}
		}

		repaircmd_t *cmd = (repaircmd_t *) calloc(1, sizeof(*cmd));

		if (cmd == NULL) {
			goto error;
		}

		Wasprintf((char **)&cmd->path, "%s/%s", repairScriptFolder, ent->d_name);	//Will have to free this memeory to use v3 repair script config

		if (cmd->path == NULL) {
			assert(cmd != NULL);
			free(cmd);
			goto error;
		}

		if (IsRepairScriptConfig(ent->d_name) == 0) {
			cmd->legacy = true;
		} else {
			//For V3 repair scripts cmd->path refers to the pathname of the repair script config file
			if (LoadRepairScriptLink
			    (&cmd->spawnattr, (char *)cmd->path) == false) {
				free((void *)cmd->path);
				free(cmd);
				continue;
			}

			cmd->spawnattr.repairFilePathname = strdup(cmd->path);
			free((void *)cmd->path);
			cmd->path = NULL;

			cmd->path = cmd->spawnattr.execStart;
			cmd->spawnattr.logDirectory = config->logdir;

			if (cmd->path == NULL) {
				fprintf(stderr,
					"Ignoring malformed repair file: %s\n",
					ent->d_name);
				free(cmd);
				continue;
			}

			if (fd < 0) {
				fprintf(stderr, "unable to open file %s:\n",
					MyStrerror(errno));
				free((void *)cmd->path);
				free((void *)cmd);
				continue;
			}

			if (IsExe(cmd->path, false) < 0) {
				fprintf(stderr, "%s is not an executable\n",
					cmd->path);
				free((void *)cmd->path);
				free((void *)cmd);
				continue;
			}

			cmd->legacy = false;
		}

		list_add(&cmd->entry, &p->head);
	}

	assert(fd != -1);

	close(fd);
	closedir(dir);
	free(direntbuf);

	return 0;

 error:
	assert(fd != -1);
	close(fd);
	Logmsg(LOG_ERR, "watchdogd: %s", MyStrerror(errno));
	closedir(dir);
	free(direntbuf);

	return -1;
}

void FreeExeList(ProcessList * p)
{
	assert(p != NULL);

	repaircmd_t *c = NULL;
	repaircmd_t *next = NULL;

	list_for_each_entry(c, next, &p->head, entry) {
		list_del(&c->entry);
		free((void *)c->path);
		if (c->legacy == false) {
			free((void *)c->spawnattr.user);
			free((void *)c->spawnattr.group);
			free((void *)c->spawnattr.workingDirectory);
			free((void *)c->spawnattr.repairFilePathname);
			free((void *)c->spawnattr.workingDirectory);
			free((void *)c->spawnattr.umask);
			free((void *)c->spawnattr.noNewPrivileges);
		}
		free(c);
	}
}

static void ThrottleWorkerThreadCreation(struct cfgoptions *const config,
					 Container * const container)
{
	sched_yield();

	int numberOfCpus = GetCpuCount();

	double loadavg;

	if (getloadavg(&loadavg, 1) != -1) {
		if (loadavg < numberOfCpus && container->workerThreadCount < MAX_WORKER_THREADS) {
			return;
		}
	}

	if (numberOfCpus > MAX_WORKER_THREADS) {
		numberOfCpus = MAX_WORKER_THREADS;
	}

	while (container->workerThreadCount > numberOfCpus * 2) {
		sleep((config->repairBinTimeout / 2 ==
		       0) ? 1 : (config->repairBinTimeout / 2));
	}
}

static void *__ExecScriptWorkerThreadLegacy(void *a)
{
	assert(a != NULL);

	Container *container = (Container *) a;
	__ExecWorker *worker = container->targ;
	repaircmd_t *c = worker->command;
	struct cfgoptions *s = worker->config;

	pthread_barrier_wait(&container->membarrier);

	container->workerThreadCount += 1;

	if (worker->retString == NULL) {
		c->ret =
		    Spawn(s->repairBinTimeout, s, c->path, c->path,
			  worker->mode, NULL);
	} else {
		c->ret =
		    Spawn(s->repairBinTimeout, s, c->path, c->path,
			  worker->mode, worker->retString, NULL);
	}

	free(worker->mode);
	free(worker->retString);

	if (worker->last == true) {
		pthread_barrier_wait(&worker->barrier);
	}

	free(worker);

	container->workerThreadCount -= 1;

	return NULL;
}

static void *__ExecScriptWorkerThread(void *a)
{
	assert(a != NULL);

	Container *container = (Container *) a;
	__ExecWorker *worker = container->targ;
	repaircmd_t *c = worker->command;
	struct cfgoptions *s = worker->config;

	pthread_barrier_wait(&container->membarrier);

	container->workerThreadCount += 1;

	if (worker->retString == NULL) {
		c->ret =
		    SpawnAttr(&c->spawnattr, c->path, c->path, worker->mode,
			      NULL);
	} else {
		c->ret =
		    SpawnAttr(&c->spawnattr, c->path, c->path, worker->mode, worker->retString,
			      NULL);
	}

	free(worker->mode);
	free(worker->retString);

	if (worker->last == true) {
		pthread_barrier_wait(&worker->barrier);
	}

	free(worker);

	container->workerThreadCount -= 1;

	return NULL;
}

static void __WaitForWorkers(struct cfgoptions *s, Container const *container)
{
	if (s->repairBinTimeout > 0) {	//Just sleep
		struct timespec rqtp = { 0 };
		clock_gettime(CLOCK_MONOTONIC, &rqtp);
		rqtp.tv_sec += s->repairBinTimeout;
		NormalizeTimespec(&rqtp);
		while (clock_nanosleep
		       (CLOCK_MONOTONIC, TIMER_ABSTIME, &rqtp, NULL) != 0
		       && errno == EINTR) {
			;
		}
	} else {		//if not given a timeout value busy wait
		sched_yield();
		while (container->workerThreadCount != 0) {
			sleep(1);
		}
	}
}

static void *__ExecuteRepairScriptsLegacy(void *a)
{
	struct executeScriptsStruct *arg = (struct executeScriptsStruct *)a;
	ProcessList *p = arg->list;
	struct cfgoptions *s = arg->config;

	Container container = { 0 };

	repaircmd_t *c = NULL;
	repaircmd_t *next = NULL;
	unsigned count = 0;

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->legacy == false) {
			continue;
		}

		pthread_barrier_init(&container.membarrier, NULL, 2);

		container.targ =
		    (__ExecWorker *) calloc(1, sizeof(__ExecWorker));
		if (container.targ == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}
		container.targ->command = c;
		container.targ->config = s;
		container.targ->mode = strdup("test");
		if (container.targ->mode == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}
		container.targ->last = false;
		container.targ->retString = NULL;

		int ret =
		    CreateDetachedThread(__ExecScriptWorkerThreadLegacy,
					 &container);
		if (ret != 0) {
			Logmsg(LOG_ERR, "Unable to create thread %s",
			       MyStrerror(-ret));
			abort();
		}
		pthread_barrier_wait(&container.membarrier);
		pthread_barrier_destroy(&container.membarrier);

		ThrottleWorkerThreadCreation(s, &container);
	}

	c = NULL;
	next = NULL;

	__WaitForWorkers(s, &container);

	memset(&container, 0, sizeof(container));

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->legacy == false) {
			continue;
		}

		if (c->ret == 0) {
			continue;
		}

		container.targ =
		    (__ExecWorker *) calloc(1, sizeof(__ExecWorker));
		if (container.targ == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}
		container.targ->command = c;
		container.targ->config = s;
		Wasprintf(&container.targ->retString, "%i", c->ret);
		if (container.targ->retString == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s", MyStrerror(errno));	//tell user then crash
		}
		container.targ->mode = strdup("repair");
		if (container.targ->mode == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}

		pthread_barrier_init(&container.membarrier, NULL, 2);

		if (next == NULL && s->repairBinTimeout <= 0) {
			container.targ->last = true;
			pthread_barrier_init(&container.targ->barrier, NULL, 2);
		}

		int ret =
		    CreateDetachedThread(__ExecScriptWorkerThreadLegacy, &container);

		if (ret != 0) {
			Logmsg(LOG_ERR, "Unable to create thread %s",
			       MyStrerror(-ret));
			abort();
		}

		pthread_barrier_wait(&container.membarrier);
		pthread_barrier_destroy(&container.membarrier);

		if (next == NULL) {
			pthread_barrier_wait(&container.targ->barrier);
			pthread_barrier_destroy(&container.targ->barrier);
		}
	}

	__WaitForWorkers(s, &container);

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->legacy == false) {
			continue;
		}
		if (c->ret != 0) {
			Logmsg(LOG_DEBUG, "repair %s script failed", c->path);
			arg->ret = 1;
			break;
		}
	}

	pthread_barrier_wait(&arg->barrier);
	return NULL;
}

static void *__ExecuteRepairScripts(void *a)
{
	struct executeScriptsStruct *arg = (struct executeScriptsStruct *)a;
	ProcessList *p = arg->list;
	struct cfgoptions *s = arg->config;

	Container container = { 0 };

	repaircmd_t *c = NULL;
	repaircmd_t *next = NULL;

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->legacy == true) {
			continue;
		}
		container.targ =
		    (__ExecWorker *) calloc(1, sizeof(__ExecWorker));
		if (container.targ == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}
		container.targ->command = c;
		container.targ->config = s;
		container.targ->mode = strdup("test");
		if (container.targ->mode == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}
		container.targ->last = false;
		container.targ->retString = NULL;

		pthread_barrier_init(&container.membarrier, NULL, 2);

		int ret =
		    CreateDetachedThread(__ExecScriptWorkerThread,
					 &container);
		if (ret != 0) {
			Logmsg(LOG_ERR, "Unable to create thread %s",
			       MyStrerror(-ret));
			abort();
		}

		pthread_barrier_wait(&container.membarrier);
		pthread_barrier_destroy(&container.membarrier);

		ThrottleWorkerThreadCreation(s, &container);
	}

	c = NULL;
	next = NULL;

	__WaitForWorkers(s, &container);

	memset(&container, 0, sizeof(container));

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->legacy == true) {
			continue;
		}

		if (c->ret == 0) {
			continue;
		}

		container.targ =
		    (__ExecWorker *) calloc(1, sizeof(__ExecWorker));
		if (container.targ == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}
		container.targ->command = c;
		container.targ->config = s;
		Wasprintf(&container.targ->retString, "%i", c->ret);
		if (container.targ->retString == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s", MyStrerror(errno));
		}
		container.targ->mode = strdup("repair");
		if (container.targ->mode == NULL) {
			Logmsg(LOG_ERR, "unable to allocate memory: %s",
			       MyStrerror(errno));
		}

		if (next == NULL && s->repairBinTimeout <= 0) {
			container.targ->last = true;
			pthread_barrier_init(&container.targ->barrier, NULL, 2);
		}

		pthread_barrier_init(&container.membarrier, NULL, 2);

		int ret =
		    CreateDetachedThread(__ExecScriptWorkerThread, &container);

		if (ret != 0) {
			Logmsg(LOG_ERR, "Unable to create thread %s",
			       MyStrerror(-ret));
			abort();
		}

		pthread_barrier_wait(&container.membarrier);
		pthread_barrier_destroy(&container.membarrier);

		if (next == NULL) {
			pthread_barrier_wait(&container.targ->barrier);
			pthread_barrier_destroy(&container.targ->barrier);
		}
	}

	__WaitForWorkers(s, &container);

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->legacy == true) {
			continue;
		}
		if (c->ret != 0) {
			Logmsg(LOG_DEBUG, "repair %s script failed", c->spawnattr.repairFilePathname);
			arg->ret = 1;
			break;
		}
	}

	pthread_barrier_wait(&arg->barrier);
	return NULL;
}

int ExecuteRepairScripts(ProcessList * p, struct cfgoptions *s)
{
	assert(s != NULL);
	assert(p != NULL);

	pid_t pid = fork();

	if (pid == 0) {
		if (OnParentDeathSend(SIGKILL) == false) {
			CreateDetachedThread(ExitIfParentDied, NULL);
		}
		struct executeScriptsStruct ess;
		ess.list = p;
		ess.config = s;
		ess.ret = 0;

		pthread_barrier_init(&ess.barrier, NULL, 3);

		CreateDetachedThread(__ExecuteRepairScriptsLegacy, &ess);
		CreateDetachedThread(__ExecuteRepairScripts, &ess);

		pthread_barrier_wait(&ess.barrier);
		pthread_barrier_destroy(&ess.barrier);

		if (ess.ret != 0) {
			exit(1);
		}
		exit(0);
	}

	int ret = 0;
	if (pid != -1) {
		while (waitpid(pid, &ret, 0) == -1 && errno == EINTR) ;

		if (WEXITSTATUS(ret) != 0) {
			return -1;
		}
	} else {
		Logmsg(LOG_ERR, "fork failed: %s", MyStrerror(errno));
		return -1;
	}

	return 0;
}
