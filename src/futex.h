#ifndef FUTEX_H
#define FUTEX_H
long FutexWait(_Atomic int *, int);
long FutexWake(_Atomic int *);
#endif
