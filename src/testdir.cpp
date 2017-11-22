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
#include <libgen.h>
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
#include "logutils.hpp"
#include <sys/ipc.h>
#include <sys/shm.h>
#include "linux.hpp"

static std::atomic_int sem = {0};
unsigned long numberOfRepairScripts = 0;

static void DeleteDuplicates(ProcessList * p)
{
	if (!p)
		return;

	if (list_is_empty(&p->head))
		return;

	repaircmd_t *c = NULL;
	repaircmd_t *next = NULL;

	list_for_each_entry(c, next, &p->head, entry) {
		repaircmd_t *b = NULL;
		repaircmd_t *next2 = NULL;
		++numberOfRepairScripts;
		if (c->legacy) {
			list_for_each_entry(b, next2, &p->head, entry) {
				if (!b->legacy) {
					if (strcmp(c->path, b->path) == 0) {
						--numberOfRepairScripts;
						Logmsg(LOG_INFO, "Using configuration info for %s script", b->path);
						list_del(&c->entry);
						free((void *)c->path);
						free((void *)c);
					}
				}
			}
		}
	}
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

			char * rel = basename(strdup(cmd->path));

			if (strcmp(rel, cmd->path) == 0) {
				free((void*)rel);
				Wasprintf((char **)&rel, "%s/%s", repairScriptFolder, cmd->path);
				free((void*)cmd->path);
				cmd->path = cmd->spawnattr.execStart = rel;
			}

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
	DeleteDuplicates(p);
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
		if (c->legacy == false) {
			free((void *)c->spawnattr.user);
			free((void *)c->spawnattr.group);
			free((void *)c->spawnattr.workingDirectory);
			free((void *)c->spawnattr.repairFilePathname);
			free((void *)c->spawnattr.noNewPrivileges);
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

	if (c->legacy == false) {
		if (c->retString[0] == '\0') {
			c->ret =
			    SpawnAttr(&c->spawnattr, c->path, c->path, c->mode == TEST ? "test": "repair",
				      NULL);
		} else {
			c->ret =
			    SpawnAttr(&c->spawnattr, c->path, c->path, c->mode == TEST ? "test": "repair", c->retString,
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
	struct timeval tv = {0};
	tv.tv_sec = 1;
	while (container->workerThreadCount != 0) {
		syscall(SYS_select, 0, NULL, NULL, NULL, &tv);
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
				c->legacy == false ? c->spawnattr.repairFilePathname : c->path);
			return 1;
		}
	}

	return 0;
}

struct repairscriptTranctions *rst = NULL;

bool ExecuteRepairScriptsPreFork(ProcessList * p, struct cfgoptions *s)
{
	if (list_is_empty(&p->head)) {
		return true;
	}

	int shmid = shmget(IPC_PRIVATE, sysconf(_SC_PAGESIZE), (IPC_CREAT|IPC_EXCL|SHM_NORESERVE|0600));
	rst = (repairscriptTranctions*)shmat(shmid, NULL, 0);
	struct shmid_ds buf = {0};
	shmctl(shmid, IPC_RMID, &buf);

	pid_t pid = fork();

	if (pid == -1) {
		Logmsg(LOG_ERR, "%s\n", MyStrerror(errno));
		return false;
	} else if (pid == 0) {
		unsetenv("NOTIFY_SOCKET");

		ThreadPoolNew();

		while (true) {
			FutexWait(&rst->sem, 0);

			struct executeScriptsStruct ess;
			ess.list = p;
			ess.config = s;
			rst->ret = __ExecuteRepairScripts(&ess);
		}
		quick_exit(0);
	}

	return true;
}

int ExecuteRepairScripts(void)
{
	if (rst == NULL) {
		pthread_exit(NULL);
	}

	FutexWake(&rst->sem);

	if (rst->ret != 0) {
		return -1;
	}

	return 0;
}
