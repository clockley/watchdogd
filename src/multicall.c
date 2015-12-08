/*
 * Copyright 2015 Christian Lockley
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

#include <libgen.h>
#include <stdio.h>
#include <errno.h>

char * GetExeName(void)
{
	static char buf[1024] = {'\0'};

	if (buf[0] != '\0') {
		return buf;
	}

	FILE* fp = fopen("/proc/self/cmdline", "r");

	if (fp == NULL) {
		return buf;
	}

	errno = 0;

	fread(buf, sizeof(buf) - 1, 1, fp);

	fclose(fp);

	if (errno != 0) {
		return buf;
	}

	return memmove(buf, basename(buf), sizeof(buf) - 1);
}
