#ifndef PIDFILE_H
#define PIDFILE_H
#include "watchdogd.h"
int DeletePidFile(pidfile_t * const pidfile);
int WritePidFile(pidfile_t * pidfile, pid_t pid);
int OpenPidFile(const char *path);
#endif
