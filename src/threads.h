#if !defined(THREADS_H)
#define THREADS_H
void GetPageSize(void);
void *ManagerThread(void *arg);
void *MarkTime(void *arg);
void *LoadAvgThread(void *arg);
void *TestFork(void *arg);
void *Sync(void *arg);
void *MinPagesThread(void *arg);
void *TestPidfileThread(void *arg);
void *CheckIfAbleToOpenFile(void *arg);
int StartHelperThreads(struct cfgoptions *options);
int SetupLogTick(void *arg);
int SetupAuxManagerThread(void *arg);
int SetupTestBinThread(void *arg);
int SetupLoadAvgThread(void *arg);
int SetupMinPagesThread(void *arg);
int SetupExeDir(void *arg);
int SetupTestFork(void *arg);
int SetupSyncThread(void *arg);
int StartPidFileTestThread(void *arg);
#endif
