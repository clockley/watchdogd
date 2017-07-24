#ifndef SUB_H
#define SUB_H
#include "watchdogd.hpp"
#include "logutils.hpp"

int EndDaemon(struct cfgoptions *s, int keepalive);
int IsDaemon(struct cfgoptions *const s);

int Shutdown(int errorcode, struct cfgoptions *arg);

int UnmountAll(void);
int Wasprintf(char **ret, const char *format, ...);
int Wasnprintf(size_t *, char **, const char *, ...);
void ResetSignalHandlers(size_t maxsigno);
int IsExe(const char *pathname, bool returnfildes);
void NormalizeTimespec(struct timespec *const tp);
int CreateDetachedThread(void *(*startFunction) (void *), void *const arg);

int LockFile(int fd, pid_t pid);
int UnlockFile(int fd, pid_t pid);

void FatalError(struct cfgoptions *s);
long ConvertStringToInt(const char *const str);
#endif
