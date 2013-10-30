#ifndef SUB_H
#define SUB_H
int EndDaemon(int exitv, void *arg, int keepalive);
int CloseStandardFileDescriptors(void);
int CloseWraper(const int *pfd);
void CloseFileDescriptors(long maxfd);
int ConfigureKernelOutOfMemoryKiller(void);
int Shutdown(int errorcode, int kexec, void *arg);
void Logmsg(int priority, const char *fmt, ...);
int Daemon(void *arg);
int OpenAndWriteToPidFile(void *arg);
int LoadConfigurationFile(void *arg);
int DeletePidFile(void *arg);
int Spawn(int timeout, void *aarg, const char *file, const char *args, ...);
void *TestDirThread(void *arg);
void *TestBinThread(void *arg);
int UnmountAll(void);
int Wasprintf(char **ret, const char *format, ...);
int ParseCommandLine(int *argc, char **argv, void *arg);
int IsDaemon(void *arg);
void WriteUserAccountingDatabaseRecord(int reboot);
void ResetSignalHandlers(int maxsigno);
int IsExe(const char *pathname, int returnfildes);
#endif
