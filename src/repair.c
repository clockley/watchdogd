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
#include "sub.h"
#include "repair.h"

static bool ParseConfigfile(char *name, char *value, repair_t *obj)
{
	if (strcmp(name, "ExecStart") == 0) {
		obj->execStart = strdup(value);
	}

	if (strcmp(name, "Timeout") == 0) {
		long ret = strtol(value, (char **)NULL, 10);

		if (ret == 0 && errno == EINVAL) {
			ret = -1;
		}

		obj->timeout = ret;
	}

	if (strcmp(name, "User") == 0) {
		obj->user = strdup(value);
	}

	if (strcmp(name, "WorkingDirectory") == 0) {
		obj->workingDirectory = strdup(value);
	}

	if (strcmp(name, "NoNewPrivileges") == 0) {
		int ret = (int)strtol(value, (char **)NULL, 10);

		if (ret == 0 && errno == EINVAL) {
			obj->noNewPrivileges = false;
		}

		char *tmp = strdup(value);

		if (tmp == NULL) {
			fprintf(stderr, "watchdogd: out of memory: %s", strerror(errno));
			return false;
		}

		for (int i = strlen(tmp); i >= 0 ; i -= 1) {
			tmp[i] = tolower(tmp[i]);
		}

		if (ret < 0) {
			obj->noNewPrivileges= true;
		} else if (strcmp(tmp, "true")) {
			obj->noNewPrivileges= true;
		} else if (strcmp(tmp, "yes")) {
			obj->noNewPrivileges = true;
		} else {
			obj->noNewPrivileges = false;
		}

		free(tmp);
	}

	if (strcmp(name, "Nice") == 0) {
		int ret = (int)strtol(value, (char **)NULL, 10);

		if (ret == 0 && errno == EINVAL) {
			ret = -1;
		}

		obj->nice = ret;
	}

	return true;
}

static void StripNewline(char *str)
{
	if (str == NULL) {
		return;
	}

	strtok(str, "\n");
}

static bool Validate(char * name, char *value)
{
	if (name == NULL) {
		return false;
	}

	if (value == NULL) {
		return false;
	}

	StripNewline(name);
	StripNewline(value);

	if (strcmp(name, "\n") == 0 || strcmp(value, "\n") == 0 || strcmp(value, "\0") == 0) {
		return false;
	}

	return true;
}

int IsRepairScriptConfig(const char *filename)
{
	char *filext = strrchr(filename, '.');

	if (strcmp(filext, ".repair") == 0) {
		return 1;
	}

	return 0;
}

bool LoadRepairScriptLink(repair_t *obj, char * const filename)
{
	if (IsRepairScriptConfig(filename) == 0) {
		return false;
	}

	FILE *fp = fopen(filename, "r");

	if (fp == NULL) {
		return false;
	}

	memset(obj, 0, sizeof(repair_t));
	char *buf = NULL;
	size_t len = 0;
	while (getline(&buf, &len, fp) != -1) {
		char *const name = strtok(buf, "=");
		char *const value = strtok(NULL, "=");

		if (Validate(name, value) == false) {
			continue;
		}

		ParseConfigfile(name, value, obj);
	}

	free(buf);
	fclose(fp);

	return true;
}

char *RepairScriptGetExecStart(repair_t *obj)
{
	return obj->execStart;
}

char *RepairScriptGetUser(repair_t *obj)
{
	return obj->user;
}

long RepairScriptGetTimeout(repair_t *obj)
{
	return obj->timeout;
}

int RepairScriptGetNice(repair_t *obj)
{
	return obj->nice;
}

char * RepairScriptGetWorkingDirectory(repair_t *obj)
{
	return obj->workingDirectory;
}

bool NoNewPrivileges(repair_t *obj)
{
	return obj->noNewPrivileges;
}
