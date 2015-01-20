#ifndef TESTDIR_H
#define TESTDIR_H
struct executeScriptsStruct
{
	ProcessList *list;
	struct cfgoptions *config;
	pthread_barrier_t barrier;
	volatile _Atomic(int) ret;
};

typedef struct __ExecScriptWorkerThread __ExecWorker;

struct container {
	volatile _Atomic(unsigned long long) workerThreadCount;
	struct cfgoptions *config;
	repaircmd_t *cmd;
	pthread_barrier_t membarrier;
};

typedef struct container Container;

#define TEST true
#define REPAIR false

int CreateLinkedListOfExes(const char *repairScriptFolder, ProcessList * p, struct cfgoptions *const);
int ExecuteRepairScripts(void);
void FreeExeList(ProcessList * p);
size_t DirentBufSize(DIR * dirp);
bool ExecuteRepairScriptsPreFork(ProcessList *, struct cfgoptions *);
#endif
