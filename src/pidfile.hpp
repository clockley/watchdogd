#ifndef PIDFILE_H
#define PIDFILE_H
#include "watchdogd.hpp"
int DeletePidFile(pidfile_t * const);
int WritePidFile(pidfile_t * const, pid_t);
int OpenPidFile(const char * const);
#endif
