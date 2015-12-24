#define MAX_WORKERS 16
#include "watchdogd.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <unistd.h>
#define MAX_WORKERS 16

static 	_Atomic(bool) canceled = false;

struct threadpool
{
	pthread_t thread;
	sem_t sem;
	void *(*func)(void*);
	void * arg;
	_Atomic(int) active;
};

static struct threadpool threads[MAX_WORKERS] = {0};

static void *Worker(void *arg)
{
	struct threadpool * t = (struct threadpool *)arg;
	pthread_detach(pthread_self());
	while (true) {
		sem_wait(&t->sem);
		__sync_synchronize();
		t->func(t->arg);
		t->active = 0;
		__sync_synchronize();
	}
}

bool ThreadPoolNew(void)
{
	for (size_t i = 0; i < MAX_WORKERS; i++) {
		sem_init(&threads[i].sem, 0, 0);
		pthread_create(&threads[i].thread, NULL, Worker, &threads[i]);
	}
	return true;
}

bool ThreadPoolCancel(void)
{
	canceled = true;
	for (size_t i = 0; i < MAX_WORKERS; i++) {
		pthread_cancel(threads[i].thread);
	}
}

bool ThreadPoolAddTask(void *(*entry)(void*), void * arg, bool retry)
{
	if (canceled == true) {
		return false;
	}

	do {
		for (size_t i = 0; i < MAX_WORKERS; i++) {
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
