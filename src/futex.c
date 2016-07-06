/*
 * Copyright 2016 Christian Lockley
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you may 
 * not use this file except in compliance with the License. You may obtain 
 * a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License. 
 */
 
#ifndef __cplusplus
#include <stdatomic.h>
#else
#include <atomic>
using namespace std;
#endif

#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>

long FutexWait(atomic_int *addr, int val)
{
	return syscall(SYS_futex, addr, FUTEX_WAIT, val, NULL, NULL, 0);
}

long FutexWake(atomic_int *addr)
{
	return syscall(SYS_futex, addr, FUTEX_WAKE, 1, NULL, NULL, 0);
}
