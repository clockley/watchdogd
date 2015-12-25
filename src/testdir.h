#ifndef TESTDIR_H
#define TESTDIR_H
struct executeScriptsStruct
{
	ProcessList *list;
	struct cfgoptions *config;
};

struct container {
	volatile _Atomic(unsigned long long) workerThreadCount;
	struct cfgoptions *config;
	repaircmd_t *cmd;
};

typedef struct container Container;

#define TEST true
#define REPAIR false

int CreateLinkedListOfExes(char *repairScriptFolder, ProcessList * p, struct cfgoptions *const);
int ExecuteRepairScripts(void);
void FreeExeList(ProcessList * p);
size_t DirentBufSize(DIR * dirp);
bool ExecuteRepairScriptsPreFork(ProcessList *, struct cfgoptions *);
#endif
