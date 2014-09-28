/*
 * Copyright 2014 Christian Lockley
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
#include "user.h"

int RunAsUser(const char *restrict const user)
{
	assert(user != NULL);

	if (user == NULL) {
		return -1;
	}

	struct passwd pwd = {0};
	struct passwd *result = NULL;

	long int initlen = sysconf(_SC_GETPW_R_SIZE_MAX);
	size_t len = 0;

	if (initlen == -1) {
		len = 4096;
	} else {
		len = (size_t) initlen;
	}

	char *buf = (char*)calloc(1, len);

	if (buf == NULL) {
		return -1;
	}

	int ret = getpwnam_r(user, &pwd, buf, len, &result);

	if (result == NULL) {
		goto error;
	}

	if (ret != 0) {
		goto error;
	}

	if (setgid(pwd.pw_gid) != 0) {
		goto error;
	}

	if (setuid(pwd.pw_uid) != 0) {
		goto error;
	}

	if (setgid(0) == 0) {
		goto error;
	}

	free(buf);
	return 0;
 error:
	{
		int serrno = errno;
		free(buf);
		errno = serrno;
	}
		return -1;
}
