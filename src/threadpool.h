#ifndef SIMPLE_THREADPOOL
#define SIMPLE_THREADPOOL
bool ThreadPoolAddTask(void *(*)(void*), void *, bool);
bool ThreadPoolNew(void);
bool ThreadPoolCancel(void);
#endif
