#ifndef THREADPOOL_H
#define THREADPOOL_H
extern unsigned long numberOfRepairScripts;
bool ThreadPoolAddTask(void *(*)(void*), void *, bool);
bool ThreadPoolNew(int threads = numberOfRepairScripts);
bool ThreadPoolCancel(void);
#endif
