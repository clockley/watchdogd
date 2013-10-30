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
#endif
