#ifndef FUTEX_H
#define FUTEX_H
long FutexWait(std::atomic_int *, int);
long FutexWake(std::atomic_int *);
#endif
