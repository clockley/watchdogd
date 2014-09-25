#ifndef TESTDIR_H
#define TESTDIR_H
int CreateLinkedListOfExes(const char *repairScriptFolder, ProcessList * p, struct cfgoptions *const);
int ExecuteRepairScripts(ProcessList * p, struct cfgoptions *s);
void FreeExeList(ProcessList * p);
size_t DirentBufSize(DIR * dirp);
#endif
