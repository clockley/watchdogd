#include <sys/wait.h>
#include "sub.h"
#include "watchdogd.h"

int CloseStandardFileDescriptors(void)
{
	int fd = 0;
	int ret = 0;
	struct stat statBuf;

	fd = open("/dev/null", O_RDWR);

	if (fd < 0) {
		Logmsg(LOG_CRIT, "Unable to open: /dev/null: %s:",
		       strerror(errno));
		return -1;
	}

	if (fstat(fd, &statBuf) != 0) {
		Logmsg(LOG_CRIT, "Stat failed %s", strerror(errno));
		return -1;
	} else if (S_ISCHR(statBuf.st_mode) == 0 && S_ISBLK(statBuf.st_mode) == 0) {
		Logmsg(LOG_CRIT, "/dev/null is not a unix device file");
		return -1;
	}

	ret = dup2(fd, STDIN_FILENO);
	if (ret < 0) {
		Logmsg(LOG_CRIT, "dup2 failed: STDIN_FILENO: %s",
		       strerror(errno));
		return -1;
	}

	ret = dup2(fd, STDOUT_FILENO);
	if (ret < 0) {
		Logmsg(LOG_CRIT, "dup2 failed: STDOUT_FILENO: %s",
		       strerror(errno));
		return -1;
	}

	ret = dup2(fd, STDERR_FILENO);
	if (ret < 0) {
		Logmsg(LOG_CRIT, "dup2 failed: STDERR_FILENO: %s",
		       strerror(errno));
		return -1;
	}

	if (CloseWraper(&fd) < 0)
		return -1;

	return 0;
}

void CloseFileDescriptors(long maxfd)
{
	long fd = 0;

	while (fd < maxfd || fd == maxfd) {
		if (fd == STDIN_FILENO || fd == STDOUT_FILENO
		    || fd == STDERR_FILENO) {
			fd = fd + 1;
		} else {
			close((int)fd);
			fd = fd + 1;
		}
	}
}

int Daemon(void *arg)
{
	struct cfgoptions *s = arg;

	pid_t pid = 0;
	long maxfd = 0;

	extern bool logToSyslog;
	logToSyslog = true;

	if (s == NULL) {
		return -1;
	}

	maxfd = sysconf(_SC_OPEN_MAX);

	if (maxfd < 0 && errno == EINVAL) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (maxfd < 0) {
		fprintf(stderr,
			"watchdogd: sysconf(_SC_OPEN_MAX) returned indefinite limit\n");
		fprintf(stderr,
			"watchdogd: will close file descriptors %u, %u, and %u\n",
			STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO);
		fprintf(stderr,
			"watchdogd: the state of other file descriptors is undefined\n");
	} else {
		CloseFileDescriptors(maxfd);
	}

	ResetSignalHandlers(_NSIG);

	sigset_t sa;

	if (sigfillset(&sa) != 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		exit(EXIT_FAILURE);	//If sigfillset fails exit. Cannot use sa uninitialized.
	}

	if (sigprocmask(SIG_UNBLOCK, &sa, NULL) != 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		fprintf(stderr, "watchdogd: unable to reset signal mask\n");
	}

	errno = 0;

	pid = fork();

	if (pid < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		if (waitpid(pid, NULL, 0) != pid) {
			fprintf(stderr, "watchdogd: %s\n", strerror(errno));
			_Exit(EXIT_FAILURE);
		}

		_Exit(EXIT_SUCCESS);
	}

	pid_t sid = setsid();

	if (sid < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (signal(SIGHUP, SIG_IGN) == SIG_ERR) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		_Exit(EXIT_FAILURE);
	}

	pid = fork();

	if (pid < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		_Exit(EXIT_SUCCESS);
	}

	if (signal(SIGHUP, SIG_DFL) == SIG_ERR) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		_Exit(EXIT_FAILURE);
	}

	if (s->options & USEPIDFILE) {
		s->lockfd = OpenPidFile(s->pidpathname);

		if (s->lockfd < 0) {
			return -1;
		}

		if (LockFile(s->lockfd, getpid()) < 0) {
			fprintf(stderr, "watchdogd: LockFile failed: %s\n", strerror(errno));
			return -1;
		}

		if (WritePidFile(s->lockfd, getpid(), s->pidpathname) < 0) {
			return -1;
		}
	}

	if (chdir("/") < 0) {
		fprintf(stderr, "watchdogd: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	umask(0);

	if (CloseStandardFileDescriptors() < 0)
		return -1;

	openlog("watchdogd", LOG_PID | LOG_NOWAIT | LOG_CONS, LOG_DAEMON);

	return 0;
}
