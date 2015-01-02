#ifndef TESTDIR_H
#define TESTDIR_H
struct executeScriptsStruct
{
	ProcessList *list;
	struct cfgoptions *config;
	pthread_barrier_t barrier;
#ifndef __cplusplus
	volatile _Atomic(int) ret;
#else
	std::atomic<volatile int> ret;
#endif
};

struct __ExecScriptWorkerThread
{
	repaircmd_t *command;
	struct cfgoptions *config;
	char * mode;
	char * retString;
	pthread_barrier_t barrier;
	bool last;
};

typedef struct __ExecScriptWorkerThread __ExecWorker;

struct container {
#ifndef __cplusplus
	volatile _Atomic(unsigned long long) workerThreadCount;
#else
	std::atomic<volatile unsigned long long> workerThreadCount;
#endif
	__ExecWorker *targ;
};

typedef struct container Container;

int CreateLinkedListOfExes(const char *repairScriptFolder, ProcessList * p, struct cfgoptions *const);
int ExecuteRepairScripts(ProcessList * p, struct cfgoptions *s);
void FreeExeList(ProcessList * p);
size_t DirentBufSize(DIR * dirp);
#endif
