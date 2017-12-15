#ifndef SUB_H
#define SUB_H
#include "watchdogd.hpp"
int EndDaemon(struct cfgoptions *s, int keepalive);
int IsDaemon(struct cfgoptions *const s);

int Shutdown(int errorcode, struct cfgoptions *arg);

int UnmountAll(void);
void ResetSignalHandlers(size_t maxsigno);
int IsExe(const char *pathname, bool returnfildes);
void NormalizeTimespec(struct timespec *const tp);
int CreateDetachedThread(void *(*startFunction) (void *), void *const arg);
void FatalError(struct cfgoptions *s);
#endif
