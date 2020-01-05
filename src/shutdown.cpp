/*
 * Copyright 2013-2020 Christian Lockley
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

#define _DEFAULT_SOURCE
#include "watchdogd.hpp"
#include "sub.hpp"
#include "exe.hpp"
#include "errorlist.hpp"
#include "logutils.hpp"
#include "linux.hpp"

int Shutdown(int errorcode, struct cfgoptions *arg)
{
	if (arg->options & NOACTION) {
		Logmsg(LOG_DEBUG, "shutdown() errorcode=%i, kexec=%s",
		       errorcode, arg->options & KEXEC ? "true" : "false");
		return 0;
	}

	if (errorcode != WECMDREBOOT && errorcode != WECMDRESET) {
		char buf[64] = { "\0" };
		snprintf(buf, sizeof(buf), "%d\n", errorcode);

		if (Spawn
		    (arg->repairBinTimeout, arg, arg->exepathname,
		     arg->exepathname, buf, NULL) == 0)
			return 0;
	}

	EndDaemon(arg, true);	//point of no return

	return NativeShutdown(errorcode, arg->options & KEXEC ? 1 : 0);
}
