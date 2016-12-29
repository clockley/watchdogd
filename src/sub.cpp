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

/*
 * This file contains functions that are used be more than one module.
 */
#include "watchdogd.hpp"
#include "sub.hpp"
#include "testdir.hpp"
#include "pidfile.hpp"

int CloseWraper(const int *pfd)
{
	if (pfd == NULL) {
		return -1;
	}

	return close(*pfd);
}

int IsDaemon(struct cfgoptions *const s)
{
	if (s->options & DAEMONIZE)
		return true;

	return false;
}

int LockFile(int fd, pid_t pid)
{
	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = pid;
	return fcntl(fd, F_SETLKW, &fl);
}

int UnlockFile(int fd, pid_t pid)
{
	struct flock fl;
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = pid;
	return fcntl(fd, F_SETLKW, &fl);
}

int Wasprintf(char **ret, const char *format, ...)
{
//http://stackoverflow.com/questions/4899221/substitute-or-workaround-for-asprintf-on-aix

	va_list ap;

	*ret = NULL;

	va_start(ap, format);
	int count = portable_vsnprintf(NULL, 0, format, ap);
	va_end(ap);

	if (count >= 0) {
		char *buffer = (char *)malloc((size_t) count + 1);

		if (buffer == NULL)
			return -1;

		va_start(ap, format);
		count = portable_vsnprintf(buffer, (size_t) count + 1, format, ap);
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

int Wasnprintf(size_t *len, char **ret, const char *format, ...)
{
	va_list ap;

	if (*len == 0) {
		*ret = NULL;
	}

	va_start(ap, format);
	int count = portable_vsnprintf(NULL, 0, format, ap);
	va_end(ap);

	if (count+1 < *len) {
		va_start(ap, format);
		count = portable_vsnprintf(*ret, (size_t) count + 1, format, ap);
		va_end(ap);
		return count;
	} else {
		free(*ret);
	}

	if (count >= 0) {
		char *buffer = (char *)malloc((size_t) count + 1);

		if (buffer == NULL)
			return -1;

		va_start(ap, format);
		count = portable_vsnprintf(buffer, (size_t) count + 1, format, ap);
		va_end(ap);

		if (count < 0) {
			free(buffer);
			*ret = NULL;
			*len = 0;
			return count;
		}
		*ret = buffer;
		*len = count;
	}

	return count;
}

int EndDaemon(struct cfgoptions *s, int keepalive)
{
	if (s == NULL)
		return -1;

	extern volatile sig_atomic_t stop;

	stop = 1;

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

	FreeExeList(&processes);
	Logmsg(LOG_INFO, "restarting system");
	closelog();

	SetLogTarget(STANDARD_ERROR);

	munlockall();

	return 0;
}

void ResetSignalHandlers(size_t maxsigno)
{
	if (maxsigno < 1)
		return;

	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);

	for (size_t i = 1; i < maxsigno; sigaction(i, &sa, NULL), i++) ;
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

long ConvertStringToInt(const char *const str)
{
	if (str == NULL) {
		return -1;
	}
	char *endptr = NULL;
	long ret = strtol((str), &endptr, 10);

	if (*endptr != '\0') {
		if (errno == 0) {
			errno = ERANGE;
		}
		return -1;
	}

	return ret;
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

int CreateDetachedThread(void *(*startFunction) (void *), void *const arg)
{
	pthread_t thread;
	pthread_attr_t attr;

	if (arg == NULL)
		return -1;

	if (*startFunction == NULL)
		return -1;

	if (pthread_attr_init(&attr) != 0)
		return -1;

	size_t stackSize = 0;
	if (pthread_attr_getstacksize(&attr, &stackSize) == 0) {
		const size_t targetStackSize = 131072;
		if ((targetStackSize >= PTHREAD_STACK_MIN)
		    && (stackSize > targetStackSize)) {
			if (pthread_attr_setstacksize(&attr, 1048576) != 0) {
				Logmsg(LOG_CRIT,
				       "pthread_attr_setstacksize: %s\n",
				       MyStrerror(errno));
			}
		}
	} else {
		Logmsg(LOG_CRIT, "pthread_attr_getstacksize: %s\n",
		       MyStrerror(errno));
	}

	pthread_attr_setguardsize(&attr, 0);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	errno = 0;

	if (pthread_create(&thread, &attr, startFunction, arg) != 0) {
		int ret = -errno;
		pthread_attr_destroy(&attr);
		return ret;
	}

	pthread_attr_destroy(&attr);

	return 0;
}

void FatalError(struct cfgoptions *s)
{
	assert(s != NULL);

	Logmsg(LOG_CRIT, "fatal error");

	config_destroy(&s->cfg);

	abort();
}
