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
#define _DEFAULT_SOURCE
#include "watchdogd.hpp"
#include "sub.hpp"
#include <dirent.h>
#include <semaphore.h>
#include "testdir.hpp"
#include "exe.hpp"
#include "repair.hpp"
#include <sys/eventfd.h>
#include "threadpool.hpp"
#include "futex.hpp"

static std::atomic_int sem = {0};
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

int CreateLinkedListOfExes(char *repairScriptFolder, ProcessList * p,
			   struct cfgoptions *const config)
{
	assert(p != NULL);
	assert(repairScriptFolder != NULL);

	struct stat buffer;
	struct dirent *ent = NULL;

	list_init(&p->head);

	int fd = open(repairScriptFolder, O_DIRECTORY | O_RDONLY);

	if (fd == -1) {
		if (!(IDENTIFY & config->options)) {
			fprintf(stderr, "watchdogd: %s: %s\n", repairScriptFolder,
				MyStrerror(errno));
		}

		if (config->options & FORCE || config->options & SOFTBOOT || config->haveConfigFile == false) {
			return 0;
		}

		return 1;
	}

	DIR *dir = fdopendir(fd);

	if (dir == NULL) {
		Logmsg(LOG_ERR, "watchdogd: %s", MyStrerror(errno));
		close(fd);
		return -1;
	}

	errno = 0;

	while ((ent = readdir(dir)) != NULL) {

		int statfd = dirfd(dir);

		if (statfd == -1) {
			goto error;
		}

		if (strchr(".", ent->d_name[0]) != NULL && !(config->options & FORCE)) {
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

		for (size_t i = 0; i < strlen(repairScriptFolder); i += 1) {
			if (repairScriptFolder[i] == '/' && i < strlen(repairScriptFolder)
			    && repairScriptFolder[i+1] == '\0') {
				repairScriptFolder[i] = '\0';
			}
		}


		Wasprintf((char **)&cmd->path, "%s/%s", repairScriptFolder, ent->d_name);

		if (cmd->path == NULL) {
			assert(cmd != NULL);
			free(cmd);
			goto error;
		}

		if (IsRepairScriptConfig(ent->d_name) == 0) {
			cmd->spawnattr = NULL;
		} else {
			cmd->spawnattr = (spawnattr_t *)calloc(1, sizeof(spawnattr_t *));
			//For V3 repair scripts cmd->path refers to the pathname of the repair script config file
			if (LoadRepairScriptLink
			    (cmd->spawnattr, (char *)cmd->path) == false) {
				free((void *)cmd->path);
				free(cmd);
				continue;
			}

			cmd->spawnattr->repairFilePathname = strdup(cmd->path);
			free((void *)cmd->path);
			cmd->path = NULL;

			cmd->path = cmd->spawnattr->execStart;

			if (cmd->path == NULL) {
				fprintf(stderr,
					"Ignoring malformed repair file: %s\n",
					ent->d_name);
				free(cmd->spawnattr);
				free(cmd);
				continue;
			}

			if (fd < 0) {
				fprintf(stderr, "unable to open file %s:\n",
					MyStrerror(errno));
				free(cmd->spawnattr);
				free((void *)cmd->path);
				free((void *)cmd);
				continue;
			}

			if (IsExe(cmd->path, false) < 0) {
				fprintf(stderr, "%s is not an executable\n",
					cmd->path);
				free(cmd->spawnattr);
				free((void *)cmd->path);
				free((void *)cmd);
				continue;
			}
		}

		list_add(&cmd->entry, &p->head);
	}
	
	assert(fd != -1);

	close(fd);
	closedir(dir);

	return 0;

 error:
	assert(fd != -1);
	close(fd);
	Logmsg(LOG_ERR, "watchdogd: %s", MyStrerror(errno));
	closedir(dir);

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
		if (c->spawnattr != NULL) {
			free((void *)c->spawnattr->user);
			free((void *)c->spawnattr->group);
			free((void *)c->spawnattr->workingDirectory);
			free((void *)c->spawnattr->repairFilePathname);
			free((void *)c->spawnattr->noNewPrivileges);
			free((void*)c->spawnattr);
		}
		free(c);
	}
}

static void * __ExecScriptWorkerThread(void *a)
{
	assert(a != NULL);

	__sync_synchronize();

	Container *container = (Container *) a;
	repaircmd_t *c = container->cmd;
	container->workerThreadCount += 1;
	sem = 1;
	FutexWake(&sem);
	__sync_synchronize();

	if (c->spawnattr == NULL) {
		if (c->retString[0] == '\0') {
			c->ret =
			    SpawnAttr(c->spawnattr, c->path, c->path, c->mode == TEST ? "test": "repair",
				      NULL);
		} else {
			c->ret =
			    SpawnAttr(c->spawnattr, c->path, c->path, c->mode == TEST ? "test": "repair", c->retString,
				      NULL);
		}
	} else {
		spawnattr_t attr = {
				.workingDirectory = NULL, .repairFilePathname = NULL,
	        		.execStart = NULL, .user = NULL, .group = NULL,
				.timeout = container->config->repairBinTimeout, .nice = 0, .umask = 0,
				.noNewPrivileges = false, .hasUmask = false
		};

		if (c->retString[0] == '\0') {
			if (c->mode == TEST) {
				c->ret = SpawnAttr(&attr, c->path, c->path, "test", NULL);
			} else {
				c->ret = SpawnAttr(&attr, c->path, c->path, "repair", c->path, NULL);
			}
		} else {
			if (c->mode == TEST) {
				c->ret = SpawnAttr(&attr, c->path, c->path, "test", NULL);
			} else {
				c->ret = SpawnAttr(&attr, c->path, c->path, "repair", c->retString, c->path, NULL);
			}
		}
	}

	c->retString[0] = '\0';

	container->workerThreadCount -= 1;

	__sync_synchronize();
	return NULL;
}

static void __WaitForWorkers(Container const *container)
{
	while (container->workerThreadCount != 0) {
		sleep(1);
	}
}

static int __ExecuteRepairScripts(void *a)
{
	struct executeScriptsStruct *arg = (struct executeScriptsStruct *)a;
	ProcessList *p = arg->list;
	struct cfgoptions *s = arg->config;

	Container container = {{0}, s, NULL};

	repaircmd_t *c = NULL;
	repaircmd_t *next = NULL;

	list_for_each_entry(c, next, &p->head, entry) {
		container.cmd = c;
		container.cmd->mode = TEST;

		c->retString[0] = '\0';
		ThreadPoolAddTask(__ExecScriptWorkerThread, &container, true);
		__sync_synchronize();
		FutexWait(&sem, 0);
		sem = 0;
	}

	c = NULL;
	next = NULL;

	__WaitForWorkers(&container);

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->ret == 0) {
			continue;
		}

		container.cmd = c;
		portable_snprintf(container.cmd->retString, sizeof(container.cmd->retString), "%i", c->ret);

		container.cmd->mode = REPAIR;

		ThreadPoolAddTask(__ExecScriptWorkerThread, &container, true);
		__sync_synchronize();
		FutexWait(&sem, 0);
		sem = 0;
	}

	__WaitForWorkers(&container);

	c = NULL;
	next = NULL;

	list_for_each_entry(c, next, &p->head, entry) {
		if (c->ret != 0) {
			Logmsg(LOG_ERR, "repair %s script failed",
				c->spawnattr == NULL ? c->spawnattr->repairFilePathname : c->path);
			return 1;
		}
	}

	return 0;
}

static int pipeFd[2] = {0};

bool ExecuteRepairScriptsPreFork(ProcessList * p, struct cfgoptions *s)
{
	if (list_is_empty(&p->head)) {
		return true;
	}

	pipe(pipeFd);

	pid_t pid = fork();

	if (pid == -1) {
		Logmsg(LOG_ERR, "%s\n", MyStrerror(errno));
		return false;
	} else if (pid == 0) {
		close(pipeFd[0]);
		pid_t pid = fork();

#if defined(__linux__)
		if (LinuxRunningSystemd() == 1) {
			unsetenv("NOTIFY_SOCKET");
		}
#endif

		unsetenv("LD_PRELOAD");

		if (pid > 0) {
			_Exit(0);
		}


		if (pid < 0) {
			Logmsg(LOG_ERR, "fork failed: %s", MyStrerror(errno));
			abort();
		}

		char b[1];

		ThreadPoolNew();

		while (true) {
			uint64_t value = 0;
			struct executeScriptsStruct ess;
			ess.list = p;
			ess.config = s;

			int ret = __ExecuteRepairScripts(&ess);

			write(pipeFd[1], &ret, sizeof(ret));
		}

		exit(0);
	}
	close(pipeFd[1]);
	return true;
}

int ExecuteRepairScripts(void)
{
	uint64_t value = 1;
	int ret = 0;

	read(pipeFd[0], &ret, sizeof(ret));

	if (ret != 0) {
		return -1;
	}

	return 0;
}
