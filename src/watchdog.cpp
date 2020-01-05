/*
 * Copyright 2016-2020 Christian Lockley
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
#include "linux.hpp"
#include "watchdog.hpp"
#include "logutils.hpp"

int Watchdog::Ping()
{
	int tmp = 0;

	if (ioctl(fd, WDIOC_KEEPALIVE, &tmp) == 0) {
		return 0;
	}

	ioctl(fd, WDIOC_SETTIMEOUT, &timeout);

	if (ioctl(fd, WDIOC_KEEPALIVE, &tmp) == 0) {
		return 0;
	}

	Logmsg(LOG_ERR, "WDIOC_KEEPALIVE ioctl failed: %s", MyStrerror(errno));

	return -1;
}

int Watchdog::Close()
{
	if (fd == -1) {
		return 0;
	}

	if (write(fd, "V", strlen("V")) < 0) {
		Logmsg(LOG_CRIT, "write to watchdog device failed: %s",
		       MyStrerror(errno));
		close(fd);
		Logmsg(LOG_CRIT, "unable to close watchdog device");
		return -1;
	} else {
		close(fd);
	}

	return 0;
}

bool Watchdog::CanMagicClose()
{
	struct watchdog_info watchDogInfo = {0};

	if (ioctl(fd, WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", MyStrerror(errno));
		return false;
	}
	if (strcmp((const char*)GetIdentity(),"iamt_wdt") == 0) {
		return true; //iamt_wdt is broken
	}
	return (WDIOF_MAGICCLOSE & watchDogInfo.options);
}

bool Watchdog::PrintWdtInfo()
{
	struct watchdog_info watchDogInfo;

	if (ioctl(fd, WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", MyStrerror(errno));
	} else {
		if (strcasecmp((const char*)watchDogInfo.identity, "software watchdog") != 0) {
			Logmsg(LOG_DEBUG, "Hardware watchdog '%s', version %u",
			       watchDogInfo.identity, watchDogInfo.firmware_version);
		} else {
			Logmsg(LOG_DEBUG, "%s, version %u",
			       watchDogInfo.identity, watchDogInfo.firmware_version);
		}
		dev dev;
		GetDeviceMajorMinor(&dev, (char*)path);
		Logmsg(LOG_DEBUG, "Device: %s Major: %li Minor: %li", path, dev.major, dev.minor);
		return true;
	}

	return false;
}

unsigned char * Watchdog::GetIdentity()
{
	static struct watchdog_info watchDogInfo;

	this->Ping();

	if (ioctl(fd, WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_ERR, "%s", MyStrerror(errno));
		return NULL;
	}

	return watchDogInfo.identity;
}

int Watchdog::Open(const char *const path)
{
	if (path == NULL) {
		return -1;
	}

	fd = open(path, O_WRONLY | O_CLOEXEC);

	if (fd == -1) {

		Logmsg(LOG_ERR,
		       "unable to open watchdog device: %s", MyStrerror(errno));
		return -1;
	}

	if (this->Ping() != 0) {
		Close();
		return -1;
	}

	if (CanMagicClose() == false) {
		Logmsg(LOG_ALERT,
		       "watchdog device does not support magic close char");
	}

	strcpy((char *)this->path, path);

	return 0;
}

int Watchdog::GetOptimalPingInterval()
{
	int timeout = 0;

	if (ioctl(fd, WDIOC_GETTIMEOUT, &timeout) < 0) {
		return 1;
	}

	timeout /= 2;

	if (timeout < 1 || timeout == 0)
		return 1;

	return timeout;
}

int Watchdog::Disable()
{
	int options = WDIOS_DISABLECARD;
	if (ioctl(fd, WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_DISABLECARD ioctl failed: %s",
		       MyStrerror(errno));
		return -1;
	}

	return 0;
}

int Watchdog::Enable()
{
	int options = WDIOS_ENABLECARD;
	if (ioctl(fd, WDIOC_SETOPTIONS, &options) < 0) {
		Logmsg(LOG_CRIT, "WDIOS_ENABLECARD ioctl failed: %s",
		       MyStrerror(errno));
		return -1;
	}

	return 0;
}

int Watchdog::ConfigureWatchdogTimeout(int timeout)
{
	struct watchdog_info watchDogInfo;

	if (timeout <= 0)
		return 0;

	if (Disable() < 0) {
		return -1;
	}

	if (ioctl(fd, WDIOC_GETSUPPORT, &watchDogInfo) < 0) {
		Logmsg(LOG_CRIT, "WDIOC_GETSUPPORT ioctl failed: %s",
		       MyStrerror(errno));
		return -1;
	}

	if (!(watchDogInfo.options & WDIOF_SETTIMEOUT)) {
		return -1;
	}

	int oldTimeout = timeout;

	if (ioctl(fd, WDIOC_SETTIMEOUT, &timeout) < 0) {
		this->timeout = GetOptimalPingInterval();

		fprintf(stderr, "watchdogd: unable to set WDT timeout\n");
		fprintf(stderr, "using default: %i", this->timeout);

		if (Enable() < 0) {
			return -1;
		}

		return Ping();
	}

	if (timeout != oldTimeout) {
		fprintf(stderr, "watchdogd: Actual WDT timeout: %i seconds\n",
			timeout);
		fprintf(stderr,
			"watchdogd: Timeout specified in the configuration file: %i\n",
			oldTimeout);
	}

	this->timeout = timeout;

	if (Enable() < 0) {
		return -1;
	}

	return Ping();
}

long unsigned Watchdog::GetStatus()
{
	long unsigned status = 0;

	ioctl(fd, WDIOC_GETBOOTSTATUS, &status);

	return status;
}

long Watchdog::GetFirmwareVersion()
{
	struct watchdog_info watchDogInfo = {0};

	ioctl(fd, WDIOC_GETSUPPORT, &watchDogInfo);

	return watchDogInfo.firmware_version;
}

long Watchdog::GetTimeleft()
{
	int timeleft = 0;

	if (ioctl(fd, WDIOC_GETTIMELEFT, &timeleft) < 0) {
		return -1;
	}

	return timeleft;
}

int Watchdog::GetRawTimeout()
{
	int timeout = 0;
	ioctl(fd, WDIOC_GETTIMEOUT, &timeout);
	return timeout;
}

bool Watchdog::CheckWatchdogTimeout(int timeout)
{
	if (timeout <= this->timeout) {
		return false;
	}
	return true;
}
