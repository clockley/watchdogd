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
#include "watchdogd.hpp"
#include "sub.hpp"
#include "logutils.hpp"
#include "linux.hpp"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define KRESET "\x1B[0m"

#define READ 0
#define WRITE 1

static sig_atomic_t logTarget = INVALID_LOG_TARGET;
static int logFile = -1;
static pthread_mutex_t mutex;
static sig_atomic_t applesquePriority = 0;
static sig_atomic_t autoUpperCase = 0;
static sig_atomic_t autoPeriod = 1;
static unsigned int logMask = 0xff;
static 	locale_t locale;
static int fd[2] = {-1, -1};
static bool noLog = false;
static int ipri[] = { LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING,
	LOG_NOTICE, LOG_INFO, LOG_DEBUG
};

static pthread_once_t initMutex = PTHREAD_ONCE_INIT;

static void MutexInit(void)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&mutex, &attr);
	pthread_mutexattr_destroy(&attr);
}

enum {
	SHORT,
	LONG
};

static const char * const spri[][2] = {
		{"LOG_EMERG", "LOG_Emergency"},
		{"LOG_ALERT", ""},
		{"LOG_CRIT", "LOG_Critical"},
		{"LOG_ERR", "LOG_Error"},
		{"LOG_WARNING", ""},
		{"LOG_NOTICE", ""},
		{"LOG_INFO", ""},
		{"LOG_DEBUG", ""}
};

struct message {
	char message[2048];
	int pri;
};

static_assert(PIPE_BUF > sizeof(struct message), "message struct cannot be witten atomically on this platform");

static int SystemdSyslog(int priority, const char *format, va_list ap)
{
	static __thread char buf[2048] = {"MESSAGE="};
	static __thread char p[64] = { '\0' };

	struct iovec iov[3] = { 0 };

	portable_snprintf(p, sizeof(p) - 1, "PRIORITY=%i", priority);

	portable_vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), format, ap);

	iov[0].iov_base = buf;
	iov[0].iov_len = strlen(buf);

	iov[1].iov_base = p;
	iov[1].iov_len = strlen(p);

	iov[2].iov_base = (void *)"SYSLOG_IDENTIFIER=watchdogd";
	iov[2].iov_len = strlen("SYSLOG_IDENTIFIER=watchdogd");

	return sd_journal_sendv(iov, 3);
}

static ssize_t Syslog(int p, char *m)
{
	struct message buf = {0};
	strcpy(buf.message, m);
	buf.pri = p;
	return write(fd[WRITE], &buf, sizeof(struct message));
}

static bool IsTty(void)
{
	if (logTarget != STANDARD_ERROR) {
		return false;
	}

	if (isatty(fileno(stderr)) == 0) {
		return false;
	}
	return true;
}

static char const * SetTextColor(int priority)
{
	if (IsTty() == false) {
		return "";
	}

	switch (priority) {
	case LOG_EMERG:
		return KRED;
		break;
	case LOG_ALERT:
		return KRED;
		break;
	case LOG_CRIT:
		return KRED;
		break;
	case LOG_ERR:
		return KRED;
		break;
	case LOG_WARNING:
		return KRED;
		break;
	case LOG_NOTICE:
		return "";
		break;
	case LOG_INFO:
		return "";
		break;
	case LOG_DEBUG:
		return "";
		break;
	default:
		assert(false);
	}
	return "";

}

static void CloseOldTarget(sig_atomic_t oldTarget)
{
	if (oldTarget == INVALID_LOG_TARGET) {
		return;
	}

	if (oldTarget == SYSTEM_LOG) {
		closelog();
		return;
	}

	if (oldTarget == STANDARD_ERROR) {
		return;
	}

	if (oldTarget == FILE_NEW || oldTarget == FILE_APPEND) {
		if (logFile != -1) {
			close(logFile);
			logFile = -1;
			return;
		} else {
			return;
		}
	}

}

void SetAutoPeriod(bool x)
{
	if (x) {
		autoPeriod = 1;
	} else {
		autoPeriod = 0;
	}
}

void SetAutoUpperCase(bool x)
{
	if (x) {
		autoUpperCase = 1;
	} else {
		autoUpperCase = 0;
	}
}

void HashTagPriority(bool x)
{
	if (x) {
		applesquePriority = 1;
	} else {
		applesquePriority = 0;
	}
}

static void SetLogMask(unsigned int mask)
{
	pthread_once(&initMutex, MutexInit);
	pthread_mutex_lock(&mutex);
	setlogmask(mask);
	logMask = setlogmask(mask);;
	pthread_mutex_unlock(&mutex);
}

void SetLogTarget(sig_atomic_t target, ...)
{
	pthread_once(&initMutex, MutexInit);
	pthread_mutex_lock(&mutex);

	if (target == STANDARD_ERROR) {
		CloseOldTarget(logTarget);
		logTarget = STANDARD_ERROR;
	}

	if (target == SYSTEM_LOG) {
		CloseOldTarget(logTarget);
		logTarget = SYSTEM_LOG;
	}

	if (target == FILE_NEW || target == FILE_APPEND) {
		closelog();
		va_list ap;
		va_start(ap, target);

		const char *fileName = va_arg(ap, const char *);

		assert(fileName != NULL);

		if (target == FILE_NEW) {
			logFile = open(fileName, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
			if (logFile == -1) {
				if (logTarget == SYSTEM_LOG) {
					syslog(LOG_ALERT, "%m");
				} else {
					fprintf(stderr, "%s\n",
						MyStrerror(errno));
				}
			} else {
				CloseOldTarget(logTarget);
				logTarget = FILE_NEW;
			}
		} else if (target == FILE_APPEND) {
			logFile = open(fileName, O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);
			if (logFile == -1) {
				if (logTarget == SYSTEM_LOG) {
					syslog(LOG_ALERT, "%m");
				} else {
					fprintf(stderr, "%s\n",
						MyStrerror(errno));
				}
			} else {
				CloseOldTarget(logTarget);
				logTarget = FILE_APPEND;
			}
		} else {
			assert(false);
		}

		va_end(ap);
	}

	pthread_mutex_unlock(&mutex);
}

bool LogUpToInt(long pri, bool cmdln)
{
	if (pri < 0 || pri > 7) {
		if (cmdln) {
			fprintf(stderr, "illegal command line option argument for -l/--loglevel\n");
			return false;
		}
		fprintf(stderr,
			"illegal value for configuration file entry named"
			" \"log-up-to\": %li\n", pri);
		return false;
	}

	SetLogMask(LOG_UPTO(ipri[pri]));

	return true;
}

static bool LogUpToString(const char *const str, bool cmdln)
{
	char *tmp = strdup(str);

	if (tmp == NULL) {
		return false;
	}

	for (int i = 0; tmp[i] != '\0'; i += 1) {
		if (tmp[i] == '-' || ispunct(tmp[i])) {
			tmp[i] = '_';
		}
		tmp[i] = toupper(tmp[i]);
	}

	if (strstr(tmp, "LOG_") == NULL) {
		char *buf = NULL;
		Wasprintf(&buf, "%s%s", "LOG_", tmp);
		free(tmp);
		tmp = buf;
	}

	bool matched = false;

	if (strcasecmp(tmp, "log_none") == 0) {
		noLog = true;
		free(tmp);
		return true;
	}

	for (size_t i = 0; i < ARRAY_SIZE(spri); i += 1) {
		if (strcasecmp(tmp, spri[i][SHORT]) == 0) {
			SetLogMask(LOG_UPTO(ipri[i]));
			matched = true;
		}

		if (strcasecmp(tmp, spri[i][LONG]) == 0) {
			SetLogMask(LOG_UPTO(ipri[i]));
			matched = true;
		}
	}

	if (matched == false && cmdln == false) {
		fprintf(stderr,
			"illegal value for configuration file entry named"
			" \"log-up-to\": %s\n", str);
	} else if (matched == false) {
		fprintf(stderr, "illegal command line option argument for [ -l | --loglevel ]\n");
	}

	free(tmp);

	return matched;
}

bool LogUpTo(const char *const str, bool cmdln)
{
	assert(str != NULL);

	if (str == NULL) {
		return false;
	}

	errno = 0;

	long logPri = ConvertStringToInt(str);

	if (errno != 0) {
		return LogUpToString(str, cmdln);
	}

	return LogUpToInt(logPri, cmdln);
}

void __attribute__ ((format (printf, 2, 3))) Logmsg(int priority, const char *const fmt, ...)
{
	assert(fmt != NULL);

	if ((LOG_MASK(LOG_PRI(priority)) & logMask) == 0 || noLog == true) {
		return;
	}

	va_list args;

	static __thread char buf[2048];

	if ((logTarget == STANDARD_ERROR || logTarget == FILE_APPEND
	     || logTarget == FILE_NEW) && applesquePriority == 0) {
		strcat(buf, SetTextColor(priority));
		switch (priority) {
		case LOG_EMERG:
			strncpy(buf+strlen(SetTextColor(priority)), "<0>", sizeof(buf) - strlen(buf));
			break;
		case LOG_ALERT:
			strncpy(buf+strlen(SetTextColor(priority)), "<1>", sizeof(buf) - strlen(buf));
			break;
		case LOG_CRIT:
			strncpy(buf+strlen(SetTextColor(priority)), "<2>", sizeof(buf) - strlen(buf));
			break;
		case LOG_ERR:
			strncpy(buf+strlen(SetTextColor(priority)), "<3>", sizeof(buf) - strlen(buf));
			break;
		case LOG_WARNING:
			strncpy(buf+strlen(SetTextColor(priority)), "<4>", sizeof(buf) - strlen(buf));
			break;
		case LOG_NOTICE:
			strncpy(buf+strlen(SetTextColor(priority)), "<5>", sizeof(buf) - strlen(buf));
			break;
		case LOG_INFO:
			strncpy(buf+strlen(SetTextColor(priority)), "<6>", sizeof(buf) - strlen(buf));
			break;
		case LOG_DEBUG:
			strncpy(buf+strlen(SetTextColor(priority)), "<7>", sizeof(buf) - strlen(buf));
			break;
		default:
			assert(false);
		}
		va_start(args, fmt);
		portable_vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), fmt,
			  args);
		va_end(args);

		assert(buf[sizeof(buf) - 1] == '\0');
	} else if (logTarget != SYSTEM_LOG && applesquePriority == 1) {
		va_start(args, fmt);
		portable_vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), fmt,
			  args);
		va_end(args);

		if (autoPeriod == 1) {
			if (buf[strlen(buf) - 1] != '.') {
				strncat(buf, ".", sizeof(buf) - strlen(buf) - 1);
			}
		}

		if (autoUpperCase == 1) {
			if (strstr(buf, "=") == NULL) {
				if (islower(buf[0])) {
					buf[0] = toupper(buf[0]);
				}
			}
		}

		switch (priority) {
		case LOG_EMERG:
			strncat(buf, " #System #Panic", sizeof(buf) - strlen(buf) - 1);
			break;
		case LOG_ALERT:
			strncat(buf, " #System #Attention", sizeof(buf) - strlen(buf) - 1);
			break;
		case LOG_CRIT:
			strncat(buf, " #System #Critical", sizeof(buf) - strlen(buf) - 1);
			break;
		case LOG_ERR:
			strncat(buf, " #System #Error", sizeof(buf) - strlen(buf) - 1);
			break;
		case LOG_WARNING:
			strncat(buf, " #System #Warning", sizeof(buf) - strlen(buf) - 1);
			break;
		case LOG_NOTICE:
			strncat(buf, " #System #Notice", sizeof(buf) - strlen(buf) - 1);
			break;
		case LOG_INFO:
			strncat(buf, " #System #Comment", sizeof(buf) - strlen(buf) - 1);
			break;
		case LOG_DEBUG:
			strncat(buf, " #System #Developer", sizeof(buf) - strlen(buf) - 1);
			break;
		default:
			assert(false);
		}
	}

	if (logTarget == STANDARD_ERROR || logTarget == FILE_APPEND
	    || logTarget == FILE_NEW) {
		assert(buf[sizeof(buf) - 1] == '\0');

		const char * format;
		if (strstr(buf, "\n") != NULL && strcmp(strstr(buf, "\n") + 1, "\0") == 0) {
			format = "%s";
		} else {
			format = "%s\n";
		}

		if (logTarget == STANDARD_ERROR) {
			int len = portable_snprintf(NULL, 0, format, buf) + 1;
			char t[len+strlen(KRESET)];
			portable_snprintf(t, sizeof(t), format, buf);
			strncat(t, KRESET, sizeof(t) - strlen(t) - 1);
			write(STDERR_FILENO, t, strlen(t));
		} else {
			int len = portable_snprintf(NULL, 0, format, buf) + 1;
			char t[len];
			portable_snprintf(t, sizeof(t), format, buf);
			write(logFile, t, strlen(t));
		}
	}

	if (logTarget == SYSTEM_LOG) {
		if (true) {
			va_start(args, fmt);
			SystemdSyslog(priority, fmt, args); //async-signal safe
			va_end(args);
			return;
		}

	}
}

bool MyStrerrorInit(void)
{
	locale = newlocale(LC_CTYPE_MASK|LC_NUMERIC_MASK|LC_TIME_MASK|
			   LC_COLLATE_MASK|LC_MONETARY_MASK|LC_MESSAGES_MASK,
			   "",(locale_t)0);

	if (locale == (locale_t)0) {
		return false;
	}

	return true;
}

void FreeLocale(void)
{
	freelocale(locale);
}

char * MyStrerror(int error)
{
	return strerror_l(error, locale);
}
