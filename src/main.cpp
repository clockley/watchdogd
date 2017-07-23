/*
 * Copyright 2016 Christian Lockley
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
#include "main.hpp"
#include "init.hpp"
#include "configfile.hpp"
#include "threads.hpp"
#include "identify.hpp"
#include "bootstatus.hpp"
#include "multicall.hpp"
#include "daemon.hpp"
#include "pidfile.hpp"
#include "dbusapi.hpp"
#include <systemd/sd-event.h>
const bool DISARM_WATCHDOG_BEFORE_REBOOT = true;
static volatile sig_atomic_t quit = 0;
volatile sig_atomic_t stop = 0;
volatile sig_atomic_t stopPing = 0;
ProcessList processes;

static void PrintConfiguration(struct cfgoptions *const cfg)
{
	Logmsg(LOG_INFO,
	       "int=%is realtime=%s sync=%s softboot=%s force=%s mla=%.2f mem=%li pid=%li",
	       cfg->sleeptime, cfg->options & REALTIME ? "yes" : "no",
	       cfg->options & SYNC ? "yes" : "no",
	       cfg->options & SOFTBOOT ? "yes" : "no",
	       cfg->options & FORCE ? "yes" : "no", cfg->maxLoadOne, cfg->minfreepages, getppid());

	if (cfg->options & ENABLEPING) {
		for (int cnt = 0; cnt < config_setting_length(cfg->ipAddresses); cnt++) {
			const char *ipAddress = config_setting_get_string_elem(cfg->ipAddresses,
									       cnt);

			assert(ipAddress != NULL);

			Logmsg(LOG_INFO, "ping: %s", ipAddress);
		}
	} else {
		Logmsg(LOG_INFO, "ping: no machine to check");
	}

	if (cfg->options & ENABLEPIDCHECKER) {
		for (int cnt = 0; cnt < config_setting_length(cfg->pidFiles); cnt++) {
			const char *pidFilePathName = config_setting_get_string_elem(cfg->pidFiles, cnt);

			assert(pidFilePathName != NULL);

			Logmsg(LOG_DEBUG, "pidfile: %s", pidFilePathName);
		}
	} else {
		Logmsg(LOG_INFO, "pidfile: no server process to check");
	}

	Logmsg(LOG_INFO, "test=%s(%i) repair=%s(%i) no_act=%s",
	       cfg->testexepathname == NULL ? "no" : cfg->testexepathname,
	       cfg->testBinTimeout,
	       cfg->exepathname == NULL ? "no" : cfg->exepathname,
	       cfg->repairBinTimeout, cfg->options & NOACTION ? "yes" : "no");
}

static void BlockSignals()
{
	sigset_t set;

	sigemptyset(&set);

	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGUSR2);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGHUP);

	pthread_sigmask(SIG_BLOCK, &set, NULL);
}

static int SignalHandler(sd_event_source * s, const signalfd_siginfo * si, void *cxt)
{
	switch (sd_event_source_get_signal(s)) {
	case SIGTERM:
	case SIGINT:
		quit = 1;
		sd_event_exit((sd_event *) cxt, 0);
		break;
	case SIGHUP:
		//reload;
		break;
	}
	return 1;
}

static void InstallSignalHandlers(sd_event * event)
{
	int r1 = sd_event_add_signal(event, NULL, SIGTERM, SignalHandler, event);
	int r2 = sd_event_add_signal(event, NULL, SIGHUP, SignalHandler, event);
	int r3 = sd_event_add_signal(event, NULL, SIGUSR2, SignalHandler, event);
	int r4 = sd_event_add_signal(event, NULL, SIGINT, SignalHandler, event);

	if (r1 < 0 || r2 < 0 || r3 < 0 || r4 < 0) {
		abort();
	}
}

static int Pinger(sd_event_source * s, uint64_t usec, void *cxt)
{
	Watchdog *watchdog = (Watchdog *) cxt;
	watchdog->Ping();

	sd_event_now(sd_event_source_get_event(s), CLOCK_REALTIME, &usec);
	usec += watchdog->GetPingInterval() * 1000000;
	sd_event_add_time(sd_event_source_get_event(s), &s, CLOCK_REALTIME, usec, 1, Pinger, (void *)cxt);
	return 0;
}

static bool InstallPinger(sd_event * e, int time, Watchdog * w)
{
	sd_event_source *s = NULL;
	uint64_t usec = 0;
	w->SetPingInterval(time);

	sd_event_now(e, CLOCK_REALTIME, &usec);

	sd_event_add_time(e, &s, CLOCK_REALTIME, usec, 1, Pinger, (void *)w);
	return true;
}

static int ServiceMain(int argc, char **argv, int fd, bool restarted)
{
	cfgoptions options;
	Watchdog watchdog;
	cfgoptions *tmp = &options;
	Watchdog *tmp2 = &watchdog;
	Pidfile pidfile;

	struct dbusinfo temp = {.config = &tmp,.wdt = &tmp2 };;
	temp.fd = fd;

	if (MyStrerrorInit() == false) {
		std::perror("Unable to create a new locale object");
		return EXIT_FAILURE;
	}

	int ret = ParseCommandLine(&argc, argv, &options);

	if (ret < 0) {
		return EXIT_FAILURE;
	} else if (ret != 0) {
		return EXIT_SUCCESS;
	}

	if (strcasecmp(GetExeName(), "wd_identify") == 0 || strcasecmp(GetExeName(), "wd_identify.sh") == 0) {
		options.options |= IDENTIFY;
	}

	if (ReadConfigurationFile(&options) < 0) {
		return EXIT_FAILURE;
	}

	if (IsDaemon(&options) == true) {
		Daemonize(&options);
		pidfile.Open(options.pidfileName);
		pidfile.Write(getppid());
	}

	if (options.options & IDENTIFY) {
		watchdog.Open(options.devicepath);

		ret = Identify(watchdog.GetRawTimeout(), (const char *)watchdog.GetIdentity(),
			       options.devicepath, options.options & VERBOSE);

		watchdog.Close();
		return ret;
	}

	if (PingInit(&options) < 0) {
		return EXIT_FAILURE;
	}

	if (restarted) {
		Logmsg(LOG_INFO,"restarting service (%s)", PACKAGE_VERSION);
	} else {
		Logmsg(LOG_INFO, "starting service (%s)", PACKAGE_VERSION);
	}

	PrintConfiguration(&options);

	if (ExecuteRepairScriptsPreFork(&processes, &options) == false) {
		Logmsg(LOG_ERR, "ExecuteRepairScriptsPreFork failed");
		FatalError(&options);
	}

	sd_event *event = NULL;
	sd_event_default(&event);

	BlockSignals();
	InstallSignalHandlers(event);

	if (StartHelperThreads(&options) != 0) {
		FatalError(&options);
	}

	pthread_t dbusThread = {0};

	if (!(options.options & NOACTION)) {
		errno = 0;

		ret = watchdog.Open(options.devicepath);

		if (errno == EBUSY && ret < 0) {
			Logmsg(LOG_ERR, "Unable to open watchdog device");
			return EXIT_FAILURE;
		} else if (ret <= -1) {
			LoadKernelModule();
			Logmsg(LOG_INFO, "Trying to load software watchdog timer...");
			ret = watchdog.Open(options.devicepath);
			if (ret == 0) {
				Logmsg(LOG_INFO, "Successfully loaded watchdog device");
			}
		}

		if (ret <= -1) {
			FatalError(&options);
		}

		if (watchdog.ConfigureWatchdogTimeout(options.watchdogTimeout)
		    < 0 && options.watchdogTimeout != -1) {
			Logmsg(LOG_ERR, "unable to set watchdog device timeout\n");
			Logmsg(LOG_ERR, "program exiting\n");
			EndDaemon(&options, false);
			watchdog.Close();
			return EXIT_FAILURE;
		}

		if (options.sleeptime == -1) {
			options.sleeptime = watchdog.GetOptimalPingInterval();
			Logmsg(LOG_INFO, "ping interval autodetect: %i", options.sleeptime);
		}

		if (options.watchdogTimeout != -1 && watchdog.CheckWatchdogTimeout(options.sleeptime) == true) {
			Logmsg(LOG_ERR, "WDT timeout is less than or equal watchdog daemon ping interval");
			Logmsg(LOG_ERR, "Using this interval may result in spurious reboots");

			if (!(options.options & FORCE)) {
				watchdog.Close();
				Logmsg(LOG_WARNING, "use the -f option to force this configuration");
				return EXIT_FAILURE;
			}
		}

		WriteBootStatus(watchdog.GetStatus(), "/run/watchdogd.status", options.sleeptime,
				watchdog.GetRawTimeout());

		static struct identinfo i;

		strncpy(i.name, (char *)watchdog.GetIdentity(), sizeof(i.name) - 1);
		i.timeout = watchdog.GetRawTimeout();
		strncpy(i.daemonVersion, PACKAGE_VERSION, sizeof(i.daemonVersion) - 1);
		strncpy(i.deviceName, options.devicepath, sizeof(i.deviceName) - 1);
		i.flags = watchdog.GetStatus();
		i.firmwareVersion = watchdog.GetFirmwareVersion();

		CreateDetachedThread(IdentityThread, &i);
		pthread_attr_t attr = {0};
		pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN*2);
		pthread_attr_setguardsize(&attr, 0);
		pthread_create(&dbusThread, &attr, DbusHelper, &temp);
		InstallPinger(event, options.sleeptime, &watchdog);

		write(fd, "", sizeof(char));
	}

	if (SetupAuxManagerThread(&options) < 0) {
		FatalError(&options);
	}

	if (PlatformInit() != true) {
		FatalError(&options);
	}

	sd_event_loop(event);

	if (IsDaemon(&options) == true) {
		pidfile.Delete();
	}

	if (stop == 1) {
		while (true) {
			if (stopPing == 1) {
				if (DISARM_WATCHDOG_BEFORE_REBOOT) {
					watchdog.Close();
				}
			} else {
				watchdog.Ping();
			}

			sleep(1);
		}
	}

	if (!(options.options & NOACTION)) {
		pthread_cancel(dbusThread);
		pthread_join(dbusThread, NULL);
	}

	watchdog.Close();

	unlink("/run/watchdogd.status");

	if (EndDaemon(&options, false) < 0) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void ClosePipe(int *fd)
{
	close(*fd);
	close(fd[1]);
}

int main(int argc, char **argv)
{
	opterr = 0;
	ParseCommandLine(&argc, argv, NULL, true);
	opterr = 1;
	int com[2] = {-1};
	pipe2(com, O_CLOEXEC);
	int com1[2] = {-1};
	pipe2(com1, O_CLOEXEC);
	int sock[2] = {-1};

	pid_t pid = fork();

	if (pid == 0) {
		setsid();
		socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sock);
		pid = fork();
		if (pid != 0) {
			close(sock[1]);
			ClosePipe(com);
			ClosePipe(com1);
			CreateDetachedThread(DbusApiInit, &sock);
			close(0);close(1);close(2);
			waitpid(pid, NULL, 0);
			quick_exit(0);
		}
		goto daemon;
	} else {
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, SIGTERM);
		sigaddset(&mask, SIGINT);
		sigaddset(&mask, SIGHUP);
		sigaddset(&mask, SIGUSR1);

		pthread_sigmask(SIG_BLOCK, &mask, NULL);
		pid_t x = getpid();
		close(com1[0]);
		write(com1[1], &x, sizeof(pid_t));
		close(com1[1]);
		close(com[1]);
		read(com[0], &pid, sizeof(pid_t));

		int sfd = signalfd(-1, &mask, SFD_CLOEXEC);

		while (true) {
			struct signalfd_siginfo si = {0};
			ssize_t ret = read (sfd, &si, sizeof(si));
			switch (si.ssi_signo) {
			case SIGTERM:
			case SIGINT:
 				kill(pid, SIGTERM);
				break;
			case SIGHUP:
				kill(pid, si.ssi_int);
				break;
			case SIGUSR1:
				quick_exit(0);
				break;
			}
		}

	}

daemon:
	pid_t shell = 0;
	close(com1[1]);
	close(com[0]);
	read(com1[0], &shell, sizeof(pid_t));
	close(com1[0]);
	close(sock[0]);

	sigset_t mask;
	bool restarted = false;
	char name[64] = {0};
	sd_bus *bus = NULL;
	sd_bus_message *m = NULL;
	sd_bus_error error = {0};
	int fildes[2] = {0};
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGUSR1);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	int sfd = signalfd (-1, &mask, SFD_CLOEXEC);
	pid = getpid();
	write(com[1], &pid, sizeof(pid));
	close(com[1]);
init:
	waitpid(-1, NULL, WNOHANG);

	pipe(fildes);
	pid = fork();

	if (pid == 0) {
		ClosePipe(com);
		unshare(CLONE_NEWIPC);
		close(sfd);
		ResetSignalHandlers(64);
		sigfillset(&mask);
		sigprocmask(SIG_UNBLOCK, &mask, NULL);
		close(fildes[1]);
		read(fildes[0], fildes+1, sizeof(int));
		close(fildes[0]);
		_Exit(ServiceMain(argc, argv, sock[1], restarted));
	}

	sd_bus_open_system(&bus);
	sprintf(name, "watchdogd.%i.scope", pid);
	sd_bus_message_new_method_call(bus, &m, "org.freedesktop.systemd1",
				"/org/freedesktop/systemd1",
				"org.freedesktop.systemd1.Manager",
				"StartTransientUnit");
	sd_bus_message_append(m, "ss", name, "fail");
	sd_bus_message_open_container(m, 'a', "(sv)");
	sd_bus_message_append(m, "(sv)", "Description", "s", " ");
	sd_bus_message_append(m, "(sv)", "KillSignal", "i", SIGTERM);
	sd_bus_message_append(m, "(sv)", "PIDs", "au", 1, (uint32_t) pid);
	sd_bus_message_close_container(m);
	sd_bus_message_append(m, "a(sa(sv))", 0);
	sd_bus_message * reply;
	sd_bus_call(bus, m, 0, &error, &reply);
	sd_bus_flush_close_unref(bus);

	close(fildes[0]);
	close(fildes[1]);

	sd_notifyf(0, "READY=1\n" "MAINPID=%lu", (unsigned long)getpid());

	while (true) {
		struct signalfd_siginfo si = {0};
		read (sfd, &si, sizeof(si));
		switch (si.ssi_signo) {
		case SIGUSR1:
			if (getppid() != 1) {
				kill(shell, SIGUSR1);
			}
			si.ssi_signo = 0;
			break;
		case SIGHUP:
			sd_bus_open_system(&bus);

			sd_bus_call_method(bus, "org.freedesktop.systemd1",
					"/org/freedesktop/systemd1",
					"org.freedesktop.systemd1.Manager", "StopUnit", &error,
					NULL, "ss", name, "ignore-dependencies");
			sd_bus_flush_close_unref(bus);
			restarted = true;
			read(sfd, &si, sizeof(si));
			if (si.ssi_signo != SIGCHLD) {
				kill(shell, si.ssi_signo);
			}
			si.ssi_signo = 0;
			goto init;
			break;
		case SIGINT:
		case SIGTERM:
		case SIGCHLD:
			sd_bus_open_system(&bus);

			sd_bus_call_method(bus, "org.freedesktop.systemd1",
					"/org/freedesktop/systemd1",
					"org.freedesktop.systemd1.Manager", "StopUnit", &error,
					NULL, "ss", name, "ignore-dependencies");
			sd_bus_flush_close_unref(bus);
			kill(shell, SIGUSR1);
			si.ssi_signo = 0;
			_Exit(si.ssi_status);
			break;
		}
	}
}
