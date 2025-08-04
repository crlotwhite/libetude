/**
 * @file threading.h
 * @brief 크로스 플랫폼 스레딩 추상화
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Windows와 POSIX 시스템 간의 스레딩 API 차이를 추상화합니다.
 */

#ifndef LIBETUDE_THREADING_H
#define LIBETUDE_THREADING_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LIBETUDE_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>

// Windows threading types
typedef HANDLE et_thread_t;
typedef CRITICAL_SECTION et_mutex_t;
typedef CONDITION_VARIABLE et_cond_t;

// Windows threading macros
#define et_mutex_init(m) InitializeCriticalSection(m)
#define et_mutex_destroy(m) DeleteCriticalSection(m)
#define et_mutex_lock(m) EnterCriticalSection(m)
#define et_mutex_unlock(m) LeaveCriticalSection(m)

#define et_cond_init(c) InitializeConditionVariable(c)
#define et_cond_destroy(c) ((void)0) // Windows doesn't need explicit cleanup
#define et_cond_wait(c, m) SleepConditionVariableCS(c, m, INFINITE)
#define et_cond_signal(c) WakeConditionVariable(c)
#define et_cond_broadcast(c) WakeAllConditionVariable(c)

#define et_thread_create(thread, attr, func, arg) \
    ((*(thread) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(func), (arg), 0, NULL)) ? 0 : -1)
#define et_thread_join(thread, retval) \
    (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0 ? 0 : -1)

// Compatibility aliases for existing code
typedef et_thread_t pthread_t;
typedef et_mutex_t pthread_mutex_t;
typedef et_cond_t pthread_cond_t;

#define pthread_mutex_init(m, attr) et_mutex_init(m)
#define pthread_mutex_destroy(m) et_mutex_destroy(m)
#define pthread_mutex_lock(m) et_mutex_lock(m)
#define pthread_mutex_unlock(m) et_mutex_unlock(m)

#define pthread_cond_init(c, attr) et_cond_init(c)
#define pthread_cond_destroy(c) et_cond_destroy(c)
#define pthread_cond_wait(c, m) et_cond_wait(c, m)
#define pthread_cond_signal(c) et_cond_signal(c)
#define pthread_cond_broadcast(c) et_cond_broadcast(c)

#define pthread_create(thread, attr, func, arg) et_thread_create(thread, attr, func, arg)
#define pthread_join(thread, retval) et_thread_join(thread, retval)

#else
// POSIX systems
#include <pthread.h>

typedef pthread_t et_thread_t;
typedef pthread_mutex_t et_mutex_t;
typedef pthread_cond_t et_cond_t;

#define et_mutex_init(m) pthread_mutex_init(m, NULL)
#define et_mutex_destroy(m) pthread_mutex_destroy(m)
#define et_mutex_lock(m) pthread_mutex_lock(m)
#define et_mutex_unlock(m) pthread_mutex_unlock(m)

#define et_cond_init(c) pthread_cond_init(c, NULL)
#define et_cond_destroy(c) pthread_cond_destroy(c)
#define et_cond_wait(c, m) pthread_cond_wait(c, m)
#define et_cond_signal(c) pthread_cond_signal(c)
#define et_cond_broadcast(c) pthread_cond_broadcast(c)

#define et_thread_create(thread, attr, func, arg) pthread_create(thread, attr, func, arg)
#define et_thread_join(thread, retval) pthread_join(thread, retval)

#endif

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_THREADING_H