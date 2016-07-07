/*
 * Copyright 2014-2915 Christian Lockley
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

static bool SetGroup(const char *restrict const group)
{
	if (group == NULL) {
		errno = -1;
		return false;
	}

	long int initlen = sysconf(_SC_GETGR_R_SIZE_MAX);
	size_t len = 0;
	int ret = 0;

	if (initlen == -1) {
		len = 4096;
	} else {
		len = (size_t) initlen;
	}

	struct group grp = {0};
	struct group *result = NULL;

	char buf [len];
	memset(buf, 0, sizeof(len));

	if (strtoll(group, NULL, 10) != 0) {
		gid_t gid = (gid_t)strtoll(group, NULL, 10);

		if (getgrgid_r(gid, &grp, buf, len, &result) != 0) {
			goto error;
		}

		if (setgid(grp.gr_gid) != 0) {
			goto error;
		}

		return true;
	}

	ret = getgrnam_r(group, &grp, buf, len, &result);

	if (ret != 0) {
		goto error;
	}

	if (setgid(grp.gr_gid) != 0) {
		goto error;
	}

	return true;

 error:
	return false;
}

int RunAsUser(const char *restrict const user, const char *restrict const group)
{
	struct passwd pwd = {0};
	struct passwd *result = NULL;

	long int initlen = sysconf(_SC_GETPW_R_SIZE_MAX);
	size_t len = 0;

	if (initlen == -1) {
		len = 4096;
	} else {
		len = (size_t) initlen;
	}

	char buf [len];
	memset(buf, 0, sizeof(len));

	if (user != NULL) {
		if (strtoll(user, NULL, 10) != 0) {
			uid_t uid = (uid_t)strtoll(group, NULL, 10);

			if (getpwuid_r(uid, &pwd, buf, len, &result) != 0) {
				goto error;
			}

			if (setgid(pwd.pw_uid) != 0) {
				goto error;
			}

			return 0;
		}

		int ret = getpwnam_r(user, &pwd, buf, len, &result);

		if (result == NULL) {
			goto error;
		}

		if (ret != 0) {
			goto error;
		}
	}

	if (group == NULL && user != NULL) {
		if (setgid(pwd.pw_gid) != 0) {
			goto error;
		}

		if (setuid(pwd.pw_uid) != 0) {
			goto error;
		}

		if (setgid(0) == 0) {
			goto error;
		}
	} else {
		if (SetGroup(group) == false) {

			if (errno != -1) {
				Logmsg(LOG_ERR, "unable run executable in group: %s", group);
				Logmsg(LOG_ERR, "trying default group");
			}

			if (setgid(pwd.pw_gid) != 0) {
				goto error;
			}

			if (user != NULL && setuid(pwd.pw_uid) != 0) {
				goto error;
			}

			if (setgid(0) == 0 && pwd.pw_gid != 0) {
				goto error;
			}
		} else {
			if (user != NULL && setuid(pwd.pw_uid) != 0) {
				goto error;
			}
		}
	}

	return 0;
 error:
		return -1;
}
