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

/*
 * This file contains functions that are used be more than one module.
 */
#include "watchdogd.h"
#include "sub.h"
#include "testdir.h"

int CloseWraper(const int *pfd)
{
	int ret = 0;
	int count = 0;

	if (pfd == NULL)
		return -1;

	do {
		ret = close(*pfd);
		count = count + 1;

	}
	while (ret != 0 && errno == EINTR && count <= 8);

	if (ret != 0) {
		return -1;
	}

	return 0;
}

int IsDaemon(struct cfgoptions *const s)
{
	if (s->options & DAEMONIZE)
		return 1;

	return 0;
}

int DeletePidFile(pidfile_t * const pidfile)
{
	if (pidfile == NULL) {
		return 0;
	}

	UnlockFile(pidfile->fd, getpid());

	CloseWraper(&pidfile->fd);

	if (remove(pidfile->name) < 0) {
		Logmsg(LOG_ERR, "remove failed: %s", strerror(errno));
		return -2;
	}

	return 0;
}

int Wasprintf(char **ret, const char *format, ...)
{
//http://stackoverflow.com/questions/4899221/substitute-or-workaround-for-asprintf-on-aix

	va_list ap;

	*ret = NULL;

	va_start(ap, format);
	int count = vsnprintf(NULL, 0, format, ap);
	va_end(ap);

	if (count >= 0) {
		char *buffer = (char *)malloc((size_t) count + 1);

		if (buffer == NULL)
			return -1;

		va_start(ap, format);
		count = vsnprintf(buffer, (size_t) count + 1, format, ap);
		va_end(ap);

		if (count < 0) {
			free(buffer);
			*ret = NULL;
			return count;
		}
		*ret = buffer;
	}

	return count;
}

int EndDaemon(struct cfgoptions *s, int keepalive)
{
	if (s == NULL)
		return -1;

	extern volatile sig_atomic_t stop;

	stop = 1;

	struct sigaction dummy;
	dummy.sa_handler = SIG_IGN;
	dummy.sa_flags = 0;

	sigemptyset(&dummy.sa_mask);

	sigaddset(&dummy.sa_mask, SIGUSR2);

	sigaction(SIGUSR2, &dummy, NULL);

	if (killpg(getpgrp(), SIGUSR2) == -1) {
		Logmsg(LOG_ERR, "killpg failed %s", strerror(errno));
	}

	if (s->options & ENABLEPING) {
		for (pingobj_iter_t * iter = ping_iterator_get(s->pingObj);
		     iter != NULL; iter = ping_iterator_next(iter)) {
			free(ping_iterator_get_context(iter));
			ping_iterator_set_context(iter, NULL);
		}

		for (int cnt = 0; cnt < config_setting_length(s->ipAddresses);
		     cnt++) {
			const char *ipAddress =
			    config_setting_get_string_elem(s->ipAddresses, cnt);

			if (ping_host_remove(s->pingObj, ipAddress) != 0) {
				fprintf(stderr, "watchdogd: %s\n",
					ping_get_error(s->pingObj));
				ping_destroy(s->pingObj);
				return -1;
			}
		}

		ping_destroy(s->pingObj);
	}

	if (keepalive == 0) {
		FreeExeList(&processes);

		config_destroy(&s->cfg);

		Logmsg(LOG_INFO, "stopping watchdog daemon");
		closelog();
		munlockall();

		return 0;
	}

	DeletePidFile(&s->pidfile);
	FreeExeList(&processes);
	Logmsg(LOG_INFO, "restarting system");
	closelog();
	munlockall();

	return 0;
}

void ResetSignalHandlers(int maxsigno)
{
	if (maxsigno < 1)
		return;

	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);

	for (int i = 1; i < maxsigno; sigaction(i, &sa, NULL), i++) ;
}

void NormalizeTimespec(struct timespec *const tp)
{
	assert(tp != NULL);

	while (tp->tv_nsec < 0) {
		if (tp->tv_sec == 0) {
			tp->tv_nsec = 0;
			return;
		}

		tp->tv_sec -= 1;
		tp->tv_nsec += 1000000000;
	}

	while (tp->tv_nsec >= 1000000000) {
		tp->tv_nsec -= 1000000000;
		tp->tv_sec++;
	}
}

int IsExe(const char *pathname, bool returnfildes)
{
	struct stat buffer;

	if (pathname == NULL)
		return -1;

	int fildes = open(pathname, O_RDONLY | O_CLOEXEC);

	if (fildes == -1)
		return -1;

	if (fstat(fildes, &buffer) != 0) {
		close(fildes);
		return -1;
	}

	if (S_ISREG(buffer.st_mode) == 0) {
		close(fildes);
		return -1;
	}

	if (!(buffer.st_mode & S_IXUSR)) {
		close(fildes);
		return -1;
	}

	if (!(buffer.st_mode & S_IRUSR)) {
		close(fildes);
		return -1;
	}

	if (returnfildes == true)	//For use with fexecve
		return fildes;

	close(fildes);
	return 0;
}

int CreateDetachedThread(void *(*startFunction) (void *), void *arg)
{
	pthread_t thread;
	pthread_attr_t attr;

	if (arg == NULL)
		return -1;

	if (*startFunction == NULL)
		return -1;

	if (pthread_attr_init(&attr) != 0)
		return -1;

	pthread_attr_setguardsize(&attr, 0);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	errno = 0;

	if (pthread_create(&thread, &attr, startFunction, arg) != 0) {
		Logmsg(LOG_CRIT, "watchdogd: pthread_create failed: %s\n",
		       strerror(errno));
		pthread_attr_destroy(&attr);
		return -1;
	}

	pthread_attr_destroy(&attr);

	return 0;
}

watchdog_t *WatchdogConstruct(void)
{
	watchdog_t *dog = (watchdog_t *) calloc(1, sizeof(*dog));
	return dog;
}

void WatchdogDestroy(watchdog_t * dog)
{
	assert(dog != NULL);
	free(dog);
	dog = NULL;
}

bool CheckWatchdogTimeout(watchdog_t * const wdt, int timeout)
{
	if (timeout <= wdt->timeout) {
		return false;
	}
	return true;
}

void SetFd(watchdog_t * const wdt, int fd)
{
	assert(wdt != NULL);
	wdt->fd = fd;
}

int GetFd(watchdog_t * const wdt)
{
	assert(wdt != NULL);
	return wdt->fd;
}

void SetTimeout(watchdog_t * const wdt, int timeout)
{
	assert(wdt != NULL);
	wdt->timeout = timeout;
}

int GetTimeout(watchdog_t * const wdt)
{
	assert(wdt != NULL);
	return wdt->timeout;
}

void FatalError(struct cfgoptions *s)
{
	assert(s != NULL);

	Logmsg(LOG_CRIT, "fatal error");

	DeletePidFile(&s->pidfile);

	config_destroy(&s->cfg);

	abort();
}
