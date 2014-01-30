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

#include "watchdogd.h"
#include "sub.h"
#include <dirent.h>
#include <config.h>
#include "testdir.h"
#include "exe.h"

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

size_t DirentBufSize(DIR * dirp);

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

int CreateLinkedListOfExes(const char *path, struct parent *p)
{
	assert(p != NULL);

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

	if (dir == NULL)
		return -1;

	size = DirentBufSize(dir);

	if (size == ((size_t) (-1))) {
		closedir(dir);
		return -1;
	}

	direntbuf = malloc(size);

	if (direntbuf == NULL) {
		closedir(dir);
		return -1;
	}

	errno = 0;

	int error = 0;

	while ((error = readdir_r(dir, direntbuf, &ent)) == 0 && ent != NULL) {

		if (error != 0) {
			goto error;
		}

		int statfd = dirfd(dir);

		if (statfd == -1) {
			goto error;
		}

		if (fstatat(statfd, ent->d_name, &buffer, 0) < 0)
			continue;

		if (S_ISREG(buffer.st_mode) == 0)
			continue;

		if (!(buffer.st_mode & S_IXUSR))
			continue;

		if (!(buffer.st_mode & S_IRUSR))
			continue;

		struct child *child = malloc(sizeof(*child));

		if (child == NULL) {
			goto error;
		}

		Wasprintf((char **)&child->name, "%s/%s", path, ent->d_name);

		if (child->name == NULL) {
			goto error;
		}

		child->ret = 0;

		list_add(&child->entry, &p->children);
	}

	closedir(dir);
	free(direntbuf);

	return 0;

 error:
	Logmsg(LOG_ERR, "watchdogd: %s", strerror(errno));
	closedir(dir);
	free(direntbuf);

	return -1;
}

void FreeExeList(struct parent *p)
{
	assert(p != NULL);

	struct child *c = NULL;
	struct child *next = NULL;

	list_for_each_entry(c, next, &p->children, entry) {
		list_del(&c->entry);
		free((void *)c->name);
		free(c);
	}
}

int ExecuteRepairScripts(void *arg1, struct cfgoptions *s)
{
	assert(s != NULL);
	assert(arg1 != NULL);

	struct parent *p = arg1;

	struct child *c = NULL;
	struct child *next = NULL;

	list_for_each_entry(c, next, &p->children, entry) {
		int ret =
		    Spawn(s->repairBinTimeout, s, c->name, c->name, "test",
			  NULL);

		if (ret != 0) {
			c->ret = ret;
		}
	}

	int ret = 0;

	list_for_each_entry(c, next, &p->children, entry) {
		if (c->ret == 0)
			continue;

		Logmsg(LOG_CRIT, "test binary %s returned %i", c->name, c->ret);

		char buf[8] = { 0x00 };

		snprintf(buf, sizeof(buf), "%i", c->ret);

		if (Spawn
		    (s->repairBinTimeout, s, c->name, c->name, "repair", buf,
		     NULL) != EXIT_SUCCESS) {
			ret = -1;
		}
	}

	return ret;

}
