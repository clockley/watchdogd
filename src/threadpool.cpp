/*
 * Copyright 2016-2020 Christian Lockley
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

#include "watchdogd.hpp"
#include <semaphore.h>

constexpr size_t MAX_WORKERS = 16;
static std::atomic_bool canceled = {false};
extern unsigned long numberOfRepairScripts;

struct threadpool
{
	sem_t sem;
	void *(*func)(void*);
	union {
		void * arg;
		int active;
	};
};

static struct threadpool threads[MAX_WORKERS] = {0};

static void *Worker(void *arg)
{
	struct threadpool * t = (struct threadpool *)arg;

	while (true) {
		sem_wait(&t->sem);
		__sync_synchronize();
		t->func(t->arg);

		__atomic_store_n(&t->active, 0, __ATOMIC_SEQ_CST);

		__sync_synchronize();
	}
	return NULL;
}

bool ThreadPoolNew(size_t numberOfThreads = numberOfRepairScripts)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setguardsize(&attr, 0);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN*2);
	pthread_t thread = {0};
	if (numberOfThreads > MAX_WORKERS)
		numberOfThreads = MAX_WORKERS;
	for (size_t i = 0; i < numberOfThreads; i++) {
		sem_init(&threads[i].sem, 0, 0);
		pthread_create(&thread, &attr, Worker, &threads[i]);
	}
	pthread_attr_destroy(&attr);
	return true;
}

bool ThreadPoolAddTask(void *(*entry)(void*), void * arg, bool retry)
{

	if (entry == NULL) {
		return false;
	}

	if (canceled == true) {
		return false;
	}

	do {
		for (size_t i = 0; i < numberOfRepairScripts; i++) {
			if (__sync_val_compare_and_swap(&threads[i].active, 0, 1) == 0) {
				threads[i].func = entry;
				threads[i].arg = arg;
				__sync_synchronize();
				sem_post(&threads[i].sem);
				return true;
			}
		}
	} while (retry);

	return false;
}
