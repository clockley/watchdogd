#ifndef SUB_H
#define SUB_H
#include "watchdogd.h"
int EndDaemon(struct cfgoptions *s, int keepalive);
int CloseWraper(const int *pfd);
void CloseFileDescriptors(long maxfd);
int ConfigureKernelOutOfMemoryKiller(void);
int Shutdown(int errorcode, struct cfgoptions *arg);

void Logmsg(int priority, const char *fmt, ...);
void SetLogTarget(sig_atomic_t target, ...);

int Daemon(struct cfgoptions *s);
int UnmountAll(void);
int Wasprintf(char **ret, const char *format, ...);
void ResetSignalHandlers(int maxsigno);
int IsExe(const char *pathname, bool returnfildes);
void NormalizeTimespec(struct timespec *const tp);
int CreateDetachedThread(void *(*startFunction) (void *), void *arg);

int LockFile(int fd, pid_t pid);
int UnlockFile(int fd, pid_t pid);
int IsDaemon(struct cfgoptions *const s);

// watchdog_t obj methods
watchdog_t *WatchdogConstruct(void);
void WatchdogDestroy(watchdog_t * dog);

bool CheckWatchdogTimeout(watchdog_t * wdt, int timeout);
void SetFd(watchdog_t * wdt, int fd);
int GetFd(watchdog_t * wdt);
void SetTimeout(watchdog_t * wdt, int timeout);
int GetTimeout(watchdog_t * wdt);
int GuessSleeptime(watchdog_t * const watchdog);
//

void FatalError(struct cfgoptions *s);
long ConvertStringToInt(const char *const str);
#endif
