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

int CreateLinkedListOfExes(const char *path, ProcessList * p)
{
	assert(p != NULL);
	assert(path != NULL);

	struct stat buffer;
	struct dirent *ent = NULL;
	struct dirent *direntbuf = NULL;

	size_t size = 0;
	int fd = 0;

	list_init(&p->children);

	fd = open(path, O_DIRECTORY | O_RDONLY);

	if (fd == -1) {
		fprintf(stderr, "watchdogd: %s: %s\n", path, strerror(errno));
		return 1;
	}

	DIR *dir = fdopendir(fd);

	if (dir == NULL) {
		Logmsg(LOG_ERR, "watchdogd: %s", strerror(errno));
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
		}

		struct child *child = (struct child *)calloc(1, sizeof(*child));

		if (child == NULL) {
			goto error;
		}

		Wasprintf((char **)&child->name, "%s/%s", path, ent->d_name); //Will have to free this memeory to use v3 repair script config

		if (child->name == NULL) {
			assert(child != NULL);
			free(child);
			goto error;
		}

		if (IsRepairScriptConfig(ent->d_name) == 0) {
			child->legacy = true;
		} else {
			repair_t rs = {0};

			if (LoadRepairScriptLink(&rs, child->name) == false) {
				free((void*)child->name);
				free(child);
				continue;
			}

			free((void*)child->name);
			child->name = NULL;

			child->name = RepairScriptGetExecStart(&rs);

			if (child->name == NULL) {
				fprintf(stderr, "Ignoring malformed repair file: %s\n", ent->d_name);
				free(child);
				continue;
			}

			if (fd < 0) {
				fprintf(stderr, "unable to open file %s:\n", strerror(errno));
				free((void*)child->name);
				free((void*)child);
				continue;
			}

			if (IsExe(child->name, false) < 0) {
				fprintf(stderr, "%s is not an executable\n", child->name);
				free((void*)child->name);
				free((void*)child);
				continue;
			}

			child->spawnattr.timeout = RepairScriptGetTimeout(&rs);
			child->spawnattr.user = RepairScriptGetUser(&rs);
			child->spawnattr.workingDirectory = RepairScriptGetWorkingDirectory(&rs);
			child->spawnattr.nice = RepairScriptGetNice(&rs);
			child->legacy = false;
			
		}

		list_add(&child->entry, &p->children);
	}

	assert(fd != -1);

	close(fd);
	closedir(dir);
	free(direntbuf);

	return 0;

 error:
	assert(fd != -1);
	close(fd);
	Logmsg(LOG_ERR, "watchdogd: %s", strerror(errno));
	closedir(dir);
	free(direntbuf);

	return -1;
}

void FreeExeList(ProcessList * p)
{
	assert(p != NULL);

	struct child *c = NULL;
	struct child *next = NULL;

	list_for_each_entry(c, next, &p->children, entry, struct child *) {
		list_del(&c->entry);
		free((void *)c->name);
		free((void*)c->spawnattr.user);
		free((void*)c->spawnattr.workingDirectory);
		free(c);
	}
}

int ExecuteRepairScripts(ProcessList * p, struct cfgoptions *s)
{
	assert(s != NULL);
	assert(p != NULL);

	struct child *c = NULL;
	struct child *next = NULL;

	list_for_each_entry(c, next, &p->children, entry, struct child *) {

		if (c->legacy == true) {
			c->ret =
			    Spawn(s->repairBinTimeout, s, c->name, c->name, "test",
				  NULL);

			if (c->ret == 0) {
				continue;
			}

			char buf[8] = { 0 };
			snprintf(buf, sizeof(buf), "%i", c->ret);

			c->ret =
			    Spawn(s->repairBinTimeout, s, c->name, c->name, "repair",
				  buf, NULL);

			if (c->ret != 0) {
				c->ret = 0;	//reset to zero
				return -1;	//exit
			}
		} else {
			int timeout = 0;
			if (c->spawnattr.timeout < 0 || c->spawnattr.timeout > 499999) {
				timeout = s->repairBinTimeout;
			} else {
				timeout = c->spawnattr.timeout;
			}

			c->ret =
			    SpawnAttr(&c->spawnattr, s, c->name, c->name, "test",
				  NULL);

			if (c->ret == 0) {
				continue;
			}

			char buf[8] = { 0 };
			snprintf(buf, sizeof(buf), "%i", c->ret);

			c->ret =
			    SpawnAttr(&c->spawnattr, s, c->name, c->name, "repair",
				  buf, NULL);

			if (c->ret != 0) {
				c->ret = 0;	//reset to zero
				return -1;	//exit
			}
		}
	}

	return 0;
}
