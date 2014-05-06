#ifndef TESTDIR_H
#define TESTDIR
int CreateLinkedListOfExes(const char *path, ProcessList *p);
int ExecuteRepairScripts(ProcessList *p, struct cfgoptions *s);
void FreeExeList(ProcessList *p);
#endif
