/*
#-------------------------------------------------------------------------------
# Copyright (c) 2014 Martin Marinov.
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the GNU Public License v3.0
# which accompanies this distribution, and is available at
# http://www.gnu.org/licenses/gpl.html
# 
# Contributors:
#     Martin Marinov - initial API and implementation
#-------------------------------------------------------------------------------
*/
#include "threading.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>


#if WINHEAD
	#include <windows.h>

	#define ETIMEDOUT WAIT_TIMEOUT

	typedef CRITICAL_SECTION pthread_mutex_t;

	struct winthreadargs {
		thread_function f;
		void * ctx;
	}typedef winthreadargs_t;

	DWORD WINAPI threadFunc(LPVOID arg) {
		winthreadargs_t * args = (winthreadargs_t *) arg;
		thread_function f = args->f;
		void * ctx = args->ctx;
		free(args);
		f(ctx);
		return 0;
	}
#else
	#include <errno.h>
	#include <pthread.h>
	#include <unistd.h>
	#include <sys/time.h>

#endif

	void thread_sleep(uint32_t milliseconds) {
#if WINHEAD
		Sleep(milliseconds);
#else
		usleep(1000 * milliseconds);
#endif
	}

	void thread_start(thread_function f, void * ctx) {
#if WINHEAD
		winthreadargs_t * args = (winthreadargs_t *) malloc(sizeof(winthreadargs_t));
		if (args == NULL) return;
		args->f = f;
		args->ctx = ctx;
		CreateThread(NULL,0,threadFunc,(LPVOID) args,0,NULL);
#else
		pthread_t thread;
		pthread_create (&thread, NULL, (void *) f, ctx);
#endif
	}

	void mutex_init(mutex_t * mutex) {
#if WINHEAD
		mutex->handle = (void *) CreateEvent (0, FALSE, FALSE, 0);
		mutex->condition = (void *) malloc(sizeof(CRITICAL_SECTION));
		if (mutex->condition == NULL) { mutex->valid = 0; return; }
		InitializeCriticalSection((CRITICAL_SECTION *) mutex->condition);
#else
		mutex->handle = malloc(sizeof(pthread_mutex_t));
		mutex->condition = malloc(sizeof(pthread_cond_t));
		if (mutex->handle == NULL || mutex->condition == NULL) {
			free(mutex->handle);
			free(mutex->condition);
			mutex->handle = NULL;
			mutex->condition = NULL;
			mutex->valid = 0;
			return;
		}

		pthread_mutex_init((pthread_mutex_t *) mutex->handle, NULL);
		pthread_cond_init((pthread_cond_t *) mutex->condition, NULL);
#endif
		mutex->valid = 1;
	}

	void critical_enter(mutex_t * mutex) {
		if (!mutex->valid) return;
#if WINHEAD
		EnterCriticalSection((CRITICAL_SECTION *) mutex->condition);
#else
		pthread_mutex_lock((pthread_mutex_t *) mutex->handle);
#endif
	}

	void critical_leave(mutex_t * mutex) {
		if (!mutex->valid) return;
#if WINHEAD
		LeaveCriticalSection((CRITICAL_SECTION *) mutex->condition);
#else
		pthread_mutex_unlock((pthread_mutex_t *) mutex->handle);
#endif
	}

	int mutex_waitforever(mutex_t * imutex) {
		if (!imutex->valid) return THREAD_NOT_INITED;
#if WINHEAD
		return (WaitForSingleObject ((HANDLE) imutex->handle, INFINITE) == WAIT_TIMEOUT) ? (THREAD_TIMEOUT) : (THREAD_OK);
#else
		if (imutex->handle == NULL || imutex->condition == NULL) return THREAD_NOT_INITED;

		pthread_mutex_t * mutex = (pthread_mutex_t *) imutex->handle;
		pthread_cond_t * cond = (pthread_cond_t *) imutex->condition;

		pthread_mutex_lock(mutex);
		const int res = pthread_cond_wait(cond, mutex);
		pthread_mutex_unlock(mutex);

		if (res == ETIMEDOUT)
			return THREAD_TIMEOUT;
#endif

		return THREAD_OK;
	}

	int mutex_wait(mutex_t * imutex) {
		if (!imutex->valid) return THREAD_NOT_INITED;
#if WINHEAD
		return (WaitForSingleObject ((HANDLE) imutex->handle, 100) == WAIT_TIMEOUT) ? (THREAD_TIMEOUT) : (THREAD_OK);
#else
		if (imutex->handle == NULL || imutex->condition == NULL) return THREAD_NOT_INITED;

		pthread_mutex_t * mutex = (pthread_mutex_t *) imutex->handle;
		pthread_cond_t * cond = (pthread_cond_t *) imutex->condition;

		struct timeval tp;
		struct timespec ts;
		gettimeofday(&tp, NULL);

		ts.tv_sec = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;

		ts.tv_nsec += 30 * 1000000;

		ts.tv_sec += ts.tv_nsec / 1000000000L;
		ts.tv_nsec = ts.tv_nsec % 1000000000L;

		pthread_mutex_lock(mutex);
		const int res = pthread_cond_timedwait(cond, mutex, &ts);
		pthread_mutex_unlock(mutex);

		if (res == ETIMEDOUT)
			return THREAD_TIMEOUT;

		if (res != ETIMEDOUT && res != 0) {
			fprintf(stderr, "Thread returned result of %d: %s\n", res, strerror(res)); fflush(stderr);
			return THREAD_TIMEOUT;
		}
#endif

		return THREAD_OK;
	}

	void mutex_signal(mutex_t * imutex) {
		if (!imutex->valid) return;
#if WINHEAD
		SetEvent ((HANDLE) imutex->handle);
#else
		pthread_mutex_t * mutex = (pthread_mutex_t *) imutex->handle;
		pthread_cond_t * cond = (pthread_cond_t *) imutex->condition;

		pthread_mutex_lock(mutex);
		pthread_cond_broadcast(cond);
		pthread_mutex_unlock(mutex);
#endif
	}

	void mutex_free(mutex_t * imutex) {
		if (!imutex->valid) return;
#if WINHEAD
		CloseHandle ((HANDLE) imutex->handle);
		DeleteCriticalSection((CRITICAL_SECTION *) imutex->condition);
		free(imutex->condition);
#else
		pthread_mutex_t * mutex = (pthread_mutex_t *) imutex->handle;
		pthread_cond_t * cond = (pthread_cond_t *) imutex->condition;

		pthread_mutex_destroy(mutex);
		pthread_cond_destroy(cond);
#endif
		imutex->valid = 0;
	}

	void semaphore_init(semaphore_t * semaphore) {
		semaphore->count = 0;
		mutex_init(&semaphore->locker);
		mutex_init(&semaphore->signaller);
	}

	void semaphore_enter(semaphore_t * semaphore) {
		critical_enter(&semaphore->locker);
		semaphore->count++;
		critical_leave(&semaphore->locker);
	}

	void semaphore_leave(semaphore_t * semaphore) {
		critical_enter(&semaphore->locker);
		semaphore->count--;
		if (semaphore -> count == 0)
			mutex_signal(&semaphore->signaller);
		critical_leave(&semaphore->locker);
	}

	void semaphore_wait(semaphore_t * semaphore) {
		int locked = 1;
		critical_enter(&semaphore->locker);
		if (semaphore -> count < 0) { critical_leave(&semaphore->locker); return; }
		if (semaphore -> count == 0)
			locked = 0;
		critical_leave(&semaphore->locker);
		if (!locked) return;

		while (mutex_wait(&semaphore->signaller) != THREAD_OK) {
			int locked = 1;
			critical_enter(&semaphore->locker);
			if (semaphore -> count < 0) { critical_leave(&semaphore->locker); return; }
			if (semaphore -> count == 0)
				locked = 0;
			critical_leave(&semaphore->locker);
			if (!locked) return;
		}
	}

	void semaphore_free(semaphore_t * semaphore) {
		mutex_free(&semaphore->locker);
		mutex_free(&semaphore->signaller);
	}

	void lockvar_init(locking_variable_t * var) {
		if (var == NULL) return;

		mutex_init(&var->signaller);
		mutex_init(&var->locker);

		critical_enter(&var->locker);
		var->someonewaiting = 0;
		var->set = 0;
		critical_leave(&var->locker);
	}

	int lockvar_waitandgetval(locking_variable_t * var) {
		if (var == NULL) return -1;

		critical_enter(&var->locker);
		if (var->someonewaiting < 0) { critical_leave(&var->locker); return -1; }
		var->someonewaiting++;
		critical_leave(&var->locker);

		int set = 0;
		int val = -1;
		critical_enter(&var->locker);
		set = var->set;
		val = var->value;
		critical_leave(&var->locker);
		if (set) {
			critical_enter(&var->locker);
			var->someonewaiting--;
			critical_leave(&var->locker);
			return val;
		}

		mutex_waitforever(&var->signaller);

		critical_enter(&var->locker);
		if (!var->set) { critical_leave(&var->locker); return -1; }

		val = var->value;

		var->someonewaiting--;

		critical_leave(&var->locker);

		return val;
	}

	void lockvar_free(locking_variable_t * var) {
		if (var == NULL) return;
		if (var->someonewaiting) return;

		mutex_free(&var->signaller);
		mutex_free(&var->locker);
	}

	void lockvar_setval(locking_variable_t * var, int value) {
		if (var == NULL) return;

		int someonewaiting = 1;
		critical_enter(&var->locker);
		if (var->someonewaiting < 0) { critical_leave(&var->locker); return; }

		var->value = value;
		var->set = 1;

		someonewaiting = var->someonewaiting;
		critical_leave(&var->locker);

		if (!someonewaiting) return;

		while (someonewaiting > 0) {
			mutex_signal(&var->signaller);

			critical_enter(&var->locker);
			someonewaiting = var->someonewaiting;
			critical_leave(&var->locker);
		}
	}
