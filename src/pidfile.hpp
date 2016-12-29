#ifndef PIDFILE_H
#define PIDFILE_H
#include <sys/types.h>
#include <unistd.h>
class Pidfile {
	const char * name = NULL;
	int ret = 0;
public:
	int Open(const char *const);
	int Write(pid_t);
	int Delete();
};
#endif
