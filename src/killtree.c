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
#include "testdir.h"

static void killtree(pid_t process)
{
	struct stat buffer;
	struct dirent *ent = NULL;
	struct dirent *direntbuf = NULL;
	int fd = open("/proc", O_DIRECTORY | O_RDONLY);
	if (fd < 0) {
		return;
	}
	DIR * procFolder =  fdopendir(fd);

	if (procFolder == NULL) {
		close(fd);
		return;
	}

	size_t size = DirentBufSize(procFolder);
	direntbuf = (struct dirent *)calloc(1, size);

	if (direntbuf == NULL) {
		close(fd);
		closedir(procFolder);
	}

	char *buf = NULL;
	size_t len = 0;
	char *saveptr;
	int error = 0;

	if (kill(process, SIGSTOP) != 0) {
		free(direntbuf);
		close(fd);
		closedir(procFolder);
		return;
	}

	while ((error = readdir_r(procFolder, direntbuf, &ent)) == 0 && ent != NULL) {
		if (fstatat(fd, ent->d_name, &buffer, 0) < 0)
			continue;
		if (!S_ISDIR(buffer.st_mode))
			continue;

		if (ConvertStringToInt(ent->d_name) == -1) {
			continue;
		}

		if (process == ConvertStringToInt(ent->d_name)) {
			continue;
		}

		int piddirfd = openat(fd, ent->d_name, O_DIRECTORY | O_RDONLY);
		int pid = openat(piddirfd, "stat", O_RDONLY);
		close(piddirfd);
		FILE * pidinfo = fdopen(pid, "r");
		getline(&buf, &len, pidinfo);
		fclose(pidinfo);
		pid_t ppid = ConvertStringToInt(strtok_r(strstr(buf, ")")+4," ", &saveptr));
		if (ppid == process) {
			kill(ConvertStringToInt(ent->d_name), SIGSTOP);
			kill(ConvertStringToInt(ent->d_name), SIGKILL);
			kill(ConvertStringToInt(ent->d_name), SIGCONT);
			waitpid(ConvertStringToInt(ent->d_name), NULL, WNOHANG);
		}
	}

	kill(process, SIGKILL);
	kill(process, SIGCONT);

	close(fd);
	free(buf);
	free(direntbuf);
	closedir(procFolder);
}

static int fd[2];

bool InitKillProcess(void)
{
	pipe(fd);

	pid_t pid = fork();

	if (pid != -1) {
		//close(fd[0]);
		wait(NULL);
	} else if (pid == -1) {
		Logmsg(LOG_ERR, "%s\n", MyStrerror(errno));
		return false;
	}

	if (pid == 0) {
		pid_t pid = fork();

		if (pid > 0) {
			_Exit(0);
		}


		if (pid < 0 ) {
			Logmsg(LOG_ERR, "fork failed: %s", MyStrerror(errno));
			abort();
		}

		close(fd[1]);

		pid_t p[1];

		while (read(fd[0], p, sizeof(pid_t)) != 0) {
			killtree(p[0]);
		}

		exit(0);
	}

	return true;
}

int QueueKill(pid_t pid)
{
	pid_t a[1];
	a[0] = pid;
	write(fd[1], a, sizeof (pid_t));

	return 0;
}
