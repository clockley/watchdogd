#ifndef FUTEX_H
#define FUTEX_H
long FutexWait(atomic_int *, int);
long FutexWake(atomic_int *);
#endif
