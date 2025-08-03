/* LibEtude Windows 스레딩 헤더 파일 */
/* Copyright (c) 2025 LibEtude Project */

#ifndef LIBETUDE_WINDOWS_THREADING_H
#define LIBETUDE_WINDOWS_THREADING_H

#ifdef _WIN32

#include <windows.h>
#include <process.h>

/* pthread 대체 타입 정의 */
typedef HANDLE et_thread_t;
typedef CRITICAL_SECTION et_mutex_t;
typedef CONDITION_VARIABLE et_cond_t;
typedef DWORD et_thread_key_t;

/* 스레드 함수 타입 */
typedef unsigned (__stdcall *et_thread_func_t)(void* arg);

/* 스레드 속성 */
typedef struct {
    size_t stack_size;
    int priority;
    bool detached;
} et_thread_attr_t;

#ifdef __cplusplus
extern "C" {
#endif

/* 스레드 관리 함수 */
int et_thread_create(et_thread_t* thread, const et_thread_attr_t* attr,
                    et_thread_func_t start_routine, void* arg);
int et_thread_join(et_thread_t thread, void** retval);
int et_thread_detach(et_thread_t thread);
void et_thread_exit(void* retval);
et_thread_t et_thread_self(void);
int et_thread_equal(et_thread_t t1, et_thread_t t2);

/* 뮤텍스 관리 함수 */
int et_mutex_init(et_mutex_t* mutex, const void* attr);
int et_mutex_destroy(et_mutex_t* mutex);
int et_mutex_lock(et_mutex_t* mutex);
int et_mutex_trylock(et_mutex_t* mutex);
int et_mutex_unlock(et_mutex_t* mutex);

/* 조건 변수 관리 함수 */
int et_cond_init(et_cond_t* cond, const void* attr);
int et_cond_destroy(et_cond_t* cond);
int et_cond_wait(et_cond_t* cond, et_mutex_t* mutex);
int et_cond_timedwait(et_cond_t* cond, et_mutex_t* mutex, const struct timespec* abstime);
int et_cond_signal(et_cond_t* cond);
int et_cond_broadcast(et_cond_t* cond);

/* 스레드 로컬 스토리지 함수 */
int et_key_create(et_thread_key_t* key, void (*destructor)(void*));
int et_key_delete(et_thread_key_t key);
void* et_getspecific(et_thread_key_t key);
int et_setspecific(et_thread_key_t key, const void* value);

/* 스레드 속성 함수 */
int et_thread_attr_init(et_thread_attr_t* attr);
int et_thread_attr_destroy(et_thread_attr_t* attr);
int et_thread_attr_setstacksize(et_thread_attr_t* attr, size_t stacksize);
int et_thread_attr_setdetachstate(et_thread_attr_t* attr, int detachstate);

/* 스레드 동기화 상수 */
#define ET_THREAD_CREATE_JOINABLE 0
#define ET_THREAD_CREATE_DETACHED 1

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* LIBETUDE_WINDOWS_THREADING_H */