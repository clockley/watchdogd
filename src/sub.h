#ifndef SUB_H
#define SUB_H
#include "watchdogd.h"
int EndDaemon(int exitv, void *arg, int keepalive);
int CloseStandardFileDescriptors(void);
int CloseWraper(const int *pfd);
void CloseFileDescriptors(long maxfd);
int ConfigureKernelOutOfMemoryKiller(void);
int Shutdown(int errorcode, void *arg);
void Logmsg(int priority, const char *fmt, ...);
int Daemon(void *arg);
void *TestDirThread(void *arg);
void *TestBinThread(void *arg);
int UnmountAll(void);
int Wasprintf(char **ret, const char *format, ...);
int IsDaemon(void *arg);
void WriteUserAccountingDatabaseRecord(int reboot);
void ResetSignalHandlers(int maxsigno);
int IsExe(const char *pathname, int returnfildes);
void NormalizeTimespec(void *arg);
int CreateDetachedThread(void *(*startFunction) (void *), void *arg);
int DeletePidFile(void *arg);
int OpenPidFile(const char *path);
int LockFile(int fd, pid_t pid);
int UnlockFile(int fd, pid_t pid);
int WritePidFile(int fd, pid_t pid, const char *name);
#endif
