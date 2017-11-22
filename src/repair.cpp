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
#include "sub.hpp"
#include "repair.hpp"
#include "configfile.hpp"
#include "logutils.hpp"

static bool ParseConfigfile(char *name, char *value, spawnattr_t * obj)
{
	if (strcasecmp(name, "ExecStart") == 0) {
		obj->execStart = strdup(value);
	}

	if (strcasecmp(name, "Timeout") == 0) {
		long ret = strtol(value, (char **)NULL, 10);

		if (ret == 0 && errno == EINVAL) {
			ret = -1;
		}

		obj->timeout = ret;
	}

	if (strcasecmp(name, "User") == 0) {
		obj->user = strdup(value);
	}

	if (strcasecmp(name, "Group") == 0) {
		obj->group = strdup(value);
	}

	if (strcasecmp(name, "WorkingDirectory") == 0) {
		obj->workingDirectory = strdup(value);
	}

	if (strcasecmp(name, "Umask") == 0) {
		char *tmp = strdup(value);
		if (tmp == NULL) {
			fprintf(stderr, "watchdogd: out of memory: %s", MyStrerror(errno));
			return false;
		}
		errno = 0;
		obj->umask = ConvertStringToInt(tmp);
		if (errno != 0) {
			Logmsg(LOG_ERR, "error parsing configfile option \"Umask\": %s", MyStrerror(errno));
			free(tmp);
			return false;
		}
		free(tmp);
		obj->hasUmask = true;
	}

	if (strcasecmp(name, "NoNewPrivileges") == 0) {
		int ret = (int)strtol(value, (char **)NULL, 10);

		if (ret == 0 && errno == EINVAL) {
			obj->noNewPrivileges = false;
		}

		if (ret > 0) {
			obj->noNewPrivileges = true;
		} else if (strcasecmp(value, "true")) {
			obj->noNewPrivileges = true;
		} else if (strcasecmp(value, "yes")) {
			obj->noNewPrivileges = true;
		} else {
			obj->noNewPrivileges = false;
		}
	}

	if (strcasecmp(name, "Nice") == 0) {
		int ret = (int)strtol(value, (char **)NULL, 10);

		if (ret == 0 && errno == EINVAL) {
			ret = -1;
		}

		obj->nice = ret;
	}

	return true;
}

void StripNewline(char *str)
{
	if (str == NULL) {
		return;
	}

	strtok(str, "\n");
}

bool Validate(char *name, char *value)
{
	if (name == NULL) {
		return false;
	}

	if (value == NULL) {
		return false;
	}

	StripNewline(name);
	StripNewline(value);

	if (strcmp(name, "\n") == 0 || strcmp(value, "\n") == 0
	    || strcmp(value, "\0") == 0) {
		return false;
	}

	return true;
}

int IsRepairScriptConfig(const char *filename)
{
	const char *filext = strrchr(filename, '.');

	if (strcasecmp(filext, ".repair") == 0) {
		return 1;
	}

	return 0;
}

bool LoadRepairScriptLink(spawnattr_t * obj, char *const filename)
{
	if (IsRepairScriptConfig(filename) == 0) {
		return false;
	}

	FILE *fp = fopen(filename, "r");

	if (fp == NULL) {
		return false;
	}

	char *buf = NULL;
	size_t len = 0;
	while (getline(&buf, &len, fp) != -1) {
		char *const name = strtok(buf, "=");
		char *const value = strtok(NULL, "=");

		if (Validate(name, value) == false) {
			continue;
		}

		NoWhitespace(name);
		NoWhitespace(value);

		ParseConfigfile(name, value, obj);
	}

	free(buf);
	fclose(fp);

	return true;
}
