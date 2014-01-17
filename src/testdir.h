#ifndef TESTDIR_H
#define TESTDIR
int CreateLinkedListOfExes(const char *path, void *arg);
int ExecuteRepairScripts(void *arg1, struct cfgoptions *s);
void FreeExeList(void *arg);
#endif
