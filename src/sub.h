#ifndef SUB_H
#define SUB_H
#include "watchdogd.h"
int EndDaemon(int exitv, struct cfgoptions *s, int keepalive);
int CloseStandardFileDescriptors(void);
int CloseWraper(const int *pfd);
void CloseFileDescriptors(long maxfd);
int ConfigureKernelOutOfMemoryKiller(void);
int Shutdown(int errorcode, struct cfgoptions *arg);
void Logmsg(int priority, const char *fmt, ...);
int Daemon(struct cfgoptions *s);
void *TestDirThread(void *arg);
void *TestBinThread(void *arg);
int UnmountAll(void);
int Wasprintf(char **ret, const char *format, ...);
int IsDaemon(const struct cfgoptions *s);
void WriteUserAccountingDatabaseRecord(int reboot);
void ResetSignalHandlers(int maxsigno);
int IsExe(const char *pathname, int returnfildes);
void NormalizeTimespec(struct timespec *tp);
int CreateDetachedThread(void *(*startFunction) (void *), void *arg);
int DeletePidFile(struct cfgoptions *s);
int OpenPidFile(const char *path);
int LockFile(int fd, pid_t pid);
int UnlockFile(int fd, pid_t pid);
int WritePidFile(int fd, pid_t pid, const char *name);
watchdog_t *WatchdogConstruct(void);
void WatchdogDestroy(watchdog_t * dog);
bool CheckWatchdogTimeout(watchdog_t * wdt, int timeout);
#endif
