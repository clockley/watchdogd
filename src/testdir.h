#ifndef TESTDIR_H
#define TESTDIR
int CreateLinkedListOfExes(const char *path, struct parent *p);
int ExecuteRepairScripts(void *arg1, struct cfgoptions *s);
void FreeExeList(struct parent *p);
#endif
