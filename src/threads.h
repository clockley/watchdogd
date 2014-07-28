#if !defined(THREADS_H)
#define THREADS_H
int StartHelperThreads(struct cfgoptions *options);
int StartServiceManagerKeepAliveNotification(void *arg);
int SetupLogTick(void *arg);
int SetupAuxManagerThread(void *arg);
int SetupTestBinThread(void *arg);
int SetupLoadAvgThread(void *arg);
int SetupMinPagesThread(void *arg);
int SetupExeDir(void *arg);
int StartPingThread(void *arg);
int SetupTestFork(void *arg);
int SetupSyncThread(void *arg);
int StartPidFileTestThread(void *arg);
int StartPingThread(void *arg);
#endif
