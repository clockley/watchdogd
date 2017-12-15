/*
 * Copyright 2017 Christian Lockley
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
#include "watchdog.hpp"
#include "logutils.hpp"
#include <sys/wait.h>
#include <sys/syscall.h>
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <chrono>
#include <thread>
#include <exception>
#include <libconfig.h++>
#include <unistd.h>
#include <getopt.h>

class PrintUsage : public std::exception {
    virtual const char* what() const throw() {
        return "Print usage";
    }
};

class Getopt {
    char * configFile = nullptr;
    bool foreground = false;
    public:
        char * GetConfigFile(void) {
            return configFile;
        };
        bool RunInforeground(void) {
            return foreground;
        };
        Getopt(int *count, char ***arguments)
        {
            foreground = false;
            struct option longOptions[] = {
                {"foreground", no_argument, 0, 'F'},
                {"config-file", required_argument, 0, 'c'},
                {0, 0, 0, 0}
            };
	        const char *opstr = "Fc:";
            int tmp = 0, opt = 0;
            opterr = 0;
            while ((opt = getopt_long(*count, *arguments, opstr, longOptions, &tmp)) != -1) {
                switch (opt) {
                case 'F':
                    foreground = true;
                    break;
                case 'c':
                    configFile = optarg;
                    break;
                default:
                	throw PrintUsage();
			        break;
                }
            }
        };
};

static volatile sig_atomic_t flag = 1;

void Sighnldr(int sig)
{
    flag = 0;
};

int main(int count, char **arguments)
{
    Watchdog wdt;

    signal(SIGTERM, Sighnldr);
    signal(SIGINT, Sighnldr);
    signal(SIGHUP, Sighnldr);
    std::string watchdogPath = "/dev/watchdog";
    try {
        libconfig::Config configFile;
        Getopt Opt(&count, &arguments);
        if (Opt.GetConfigFile() == nullptr) {
            if (wdt.Open(watchdogPath.c_str())) {
                std::cerr << "Unable to open watchdog device: " << std::strerror(errno) << std::endl;
                return 1;
            }
        } else {
            try {
                configFile.readFile(Opt.GetConfigFile());
            }
            catch (libconfig::FileIOException &ex) {
                std::cerr << "I/O error while reading file." << std::endl;
                return 1;
            }
            catch(const libconfig::ParseException &ex) {
                std::cerr << "Parse error at " << ex.getFile()
                    << ":" << ex.getLine() << " - " << ex.getError() << std::endl;
                return 1;
            }
            configFile.setAutoConvert(true);
            if (configFile.lookupValue("watchdog-device", watchdogPath) != true) {
                watchdogPath = "/dev/watchdog";
            }
            if (wdt.Open(watchdogPath.c_str())) {
                std::cerr << "Unable to open watchdog device: " << std::strerror(errno) << std::endl;
                return 1;
            }
        }

        wdt.Close();

        daemon(0, 0);
        SetLogTarget(SYSTEM_LOG);

        wdt.Open(watchdogPath.c_str());
        struct timeval tv = {0};
        tv.tv_sec = 1;
        while (flag) {
            wdt.Ping();
            syscall(SYS_select, 0, NULL, NULL, NULL, &tv);
        }
        wdt.Close();
    } catch(PrintUsage &ex) {
        std::cout << "wd_keepalive Version 1\n";
        std::cout << "wd_keepalive [-c | --config-file <config_file>]\n";
        return 1;
    }
}