#ifndef TESTDIR_H
#define TESTDIR
int CreateLinkedListOfExes(const char *path, void *arg);
int ExecuteRepairScripts(void *arg1, void *arg);
void FreeExeList(void *arg);
#endif
