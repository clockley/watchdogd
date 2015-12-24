#ifndef THREADPOOL_H
#define THREADPOOL_H
bool ThreadPoolAddTask(void *(*)(void*), void *, bool);
bool ThreadPoolNew(void);
bool ThreadPoolCancel(void);
#endif
