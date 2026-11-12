/*
 * Platform threading primitives for the PX68K vendored libretro-common subset.
 * API follows libretro-common rthreads (sthread/slock/scond).
 */
#include <stdlib.h>
#include <string.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
struct sthread { HANDLE handle; };
struct slock { CRITICAL_SECTION mutex; };
struct scond { CONDITION_VARIABLE cond; };
struct thread_start { void (*fn)(void *); void *arg; };
static DWORD WINAPI rthreads_thread_start(LPVOID opaque)
{
   struct thread_start *start = (struct thread_start*)opaque;
   void (*fn)(void*) = start->fn;
   void *arg = start->arg;
   free(start);
   fn(arg);
   return 0;
}
sthread_t *sthread_create(void (*fn)(void *), void *arg)
{
   sthread_t *thread;
   struct thread_start *start;
   if (!fn) return NULL;
   thread = (sthread_t*)calloc(1, sizeof(*thread));
   start = (struct thread_start*)malloc(sizeof(*start));
   if (!thread || !start) { free(thread); free(start); return NULL; }
   start->fn = fn; start->arg = arg;
   thread->handle = CreateThread(NULL, 0, rthreads_thread_start, start, 0, NULL);
   if (!thread->handle) { free(start); free(thread); return NULL; }
   return thread;
}
void sthread_join(sthread_t *thread)
{
   if (!thread) return;
   WaitForSingleObject(thread->handle, INFINITE);
   CloseHandle(thread->handle);
   free(thread);
}
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)malloc(sizeof(*lock));
   if (lock) InitializeCriticalSection(&lock->mutex);
   return lock;
}
void slock_free(slock_t *lock) { if (lock) { DeleteCriticalSection(&lock->mutex); free(lock); } }
void slock_lock(slock_t *lock) { EnterCriticalSection(&lock->mutex); }
void slock_unlock(slock_t *lock) { LeaveCriticalSection(&lock->mutex); }
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)malloc(sizeof(*cond));
   if (cond) InitializeConditionVariable(&cond->cond);
   return cond;
}
void scond_free(scond_t *cond) { free(cond); }
void scond_wait(scond_t *cond, slock_t *lock) { SleepConditionVariableCS(&cond->cond, &lock->mutex, INFINITE); }
void scond_signal(scond_t *cond) { WakeConditionVariable(&cond->cond); }
void scond_broadcast(scond_t *cond) { WakeAllConditionVariable(&cond->cond); }

#elif defined(__SWITCH__)
#include <switch.h>
struct sthread { Thread thread; };
struct slock { Mutex mutex; };
struct scond { CondVar cond; };
struct thread_start { void (*fn)(void *); void *arg; };
static void rthreads_thread_start(void *opaque)
{
   struct thread_start *start = (struct thread_start*)opaque;
   void (*fn)(void*) = start->fn;
   void *arg = start->arg;
   free(start);
   fn(arg);
}
sthread_t *sthread_create(void (*fn)(void *), void *arg)
{
   sthread_t *thread;
   struct thread_start *start;
   if (!fn) return NULL;
   thread = (sthread_t*)calloc(1, sizeof(*thread));
   start = (struct thread_start*)malloc(sizeof(*start));
   if (!thread || !start) { free(thread); free(start); return NULL; }
   start->fn = fn; start->arg = arg;
   if (R_FAILED(threadCreate(&thread->thread, rthreads_thread_start, start,
         NULL, 128 * 1024, 0x2B, -2)))
   { free(start); free(thread); return NULL; }
   threadStart(&thread->thread);
   return thread;
}
void sthread_join(sthread_t *thread)
{
   if (!thread) return;
   threadWaitForExit(&thread->thread);
   threadClose(&thread->thread);
   free(thread);
}
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)malloc(sizeof(*lock));
   if (lock) mutexInit(&lock->mutex);
   return lock;
}
void slock_free(slock_t *lock) { free(lock); }
void slock_lock(slock_t *lock) { mutexLock(&lock->mutex); }
void slock_unlock(slock_t *lock) { mutexUnlock(&lock->mutex); }
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)malloc(sizeof(*cond));
   if (cond) condvarInit(&cond->cond);
   return cond;
}
void scond_free(scond_t *cond) { free(cond); }
void scond_wait(scond_t *cond, slock_t *lock) { condvarWait(&cond->cond, &lock->mutex); }
void scond_signal(scond_t *cond) { condvarWakeOne(&cond->cond); }
void scond_broadcast(scond_t *cond) { condvarWakeAll(&cond->cond); }

#else
#include <pthread.h>
struct sthread { pthread_t thread; };
struct slock { pthread_mutex_t mutex; };
struct scond { pthread_cond_t cond; };
struct thread_start { void (*fn)(void *); void *arg; };
static void *rthreads_thread_start(void *opaque)
{
   struct thread_start *start = (struct thread_start*)opaque;
   void (*fn)(void*) = start->fn;
   void *arg = start->arg;
   free(start);
   fn(arg);
   return NULL;
}
sthread_t *sthread_create(void (*fn)(void *), void *arg)
{
   sthread_t *thread;
   struct thread_start *start;
   if (!fn) return NULL;
   thread = (sthread_t*)calloc(1, sizeof(*thread));
   start = (struct thread_start*)malloc(sizeof(*start));
   if (!thread || !start) { free(thread); free(start); return NULL; }
   start->fn = fn; start->arg = arg;
   if (pthread_create(&thread->thread, NULL, rthreads_thread_start, start) != 0)
   { free(start); free(thread); return NULL; }
   return thread;
}
void sthread_join(sthread_t *thread)
{
   if (!thread) return;
   pthread_join(thread->thread, NULL);
   free(thread);
}
slock_t *slock_new(void)
{
   slock_t *lock = (slock_t*)malloc(sizeof(*lock));
   if (lock && pthread_mutex_init(&lock->mutex, NULL) != 0) { free(lock); return NULL; }
   return lock;
}
void slock_free(slock_t *lock) { if (lock) { pthread_mutex_destroy(&lock->mutex); free(lock); } }
void slock_lock(slock_t *lock) { pthread_mutex_lock(&lock->mutex); }
void slock_unlock(slock_t *lock) { pthread_mutex_unlock(&lock->mutex); }
scond_t *scond_new(void)
{
   scond_t *cond = (scond_t*)malloc(sizeof(*cond));
   if (cond && pthread_cond_init(&cond->cond, NULL) != 0) { free(cond); return NULL; }
   return cond;
}
void scond_free(scond_t *cond) { if (cond) { pthread_cond_destroy(&cond->cond); free(cond); } }
void scond_wait(scond_t *cond, slock_t *lock) { pthread_cond_wait(&cond->cond, &lock->mutex); }
void scond_signal(scond_t *cond) { pthread_cond_signal(&cond->cond); }
void scond_broadcast(scond_t *cond) { pthread_cond_broadcast(&cond->cond); }
#endif
