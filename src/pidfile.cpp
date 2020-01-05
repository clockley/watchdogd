/*
 * Copyright 2014-2020 Christian Lockley
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

#include "pidfile.hpp"
#include "sub.hpp"
#include "logutils.hpp"

int Pidfile::Write(pid_t pid)
{
	if (dprintf(ret, "%d\n", pid) < 0) {
		if (name != NULL) {
			fprintf(stderr,
				"watchdogd: unable to write pid to %s: %s\n",
				name, strerror(errno));
		} else {
			fprintf(stderr,
				"watchdogd: unable to write pid to %i: %s\n",
				ret, strerror(errno));
		}
		return -1;
	}

	fsync(ret);
	return 0;
}

int Pidfile::Open(const char *const path)
{
	name = path;
	mode_t oumask = umask(0027);
	ret = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (ret < 0) {
		fprintf(stderr, "watchdogd: open failed: %s\n",
			strerror(errno));
		if (errno == EEXIST) {
			ret = open(path, O_RDONLY | O_CLOEXEC);

			if (ret < 0) {
				umask(oumask);
				return ret;
			}

			char buf[64] = { 0 };
			if (pread(ret, buf, sizeof(buf), 0) == -1) {
				close(ret);
				umask(oumask);
				return -1;
			}

			errno = 0;
			pid_t pid = (pid_t) strtol(buf, (char **)NULL, 10);

			if (errno != 0) {
				umask(oumask);
				return -1;
			}

			fprintf(stderr,
				"watchdogd: checking if the pid is valid\n");
			if (kill(pid, 0) != 0 && errno == ESRCH) {
				close(ret);
				if (remove(path) < 0) {
					umask(oumask);
					return -1;
				} else {
					ret =
					    open(path,
						 O_WRONLY | O_CREAT | O_EXCL |
						 O_CLOEXEC, 0644);
					if (ret < 0) {
						fprintf(stderr,
							"watchdogd: open failed: %s\n",
							strerror(errno));
						umask(oumask);
						return ret;
					} else {
						fprintf(stdout,
							"successfully opened pid file\n");
					}
				}
			}
		}
	}

	umask(oumask);
	return ret;
}

int Pidfile::Delete()
{
	if (name == NULL) {
		return -1;
	}

	if (ret == 0) {
		return 0;
	}

	UnlockFile(ret, getpid());

	close(ret);

	if (remove(name) < 0) {
		Logmsg(LOG_ERR, "remove failed: %s", strerror(errno));
		return -2;
	}

	return 0;
}
