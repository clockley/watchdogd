#ifndef TESTDIR_H
#define TESTDIR
int CreateLinkedListOfExes(const char *path, struct parent *p);
int ExecuteRepairScripts(struct parent *p, struct cfgoptions *s);
void FreeExeList(struct parent *p);
#endif
