/**
 * @file windows_threading.c
 * @brief Windows Thread Pool API를 사용한 스레딩 시스템
 *
 * Windows Thread Pool API를 활용하여 효율적인 작업 스케줄링과
 * 스레드 관리를 제공합니다.
 */

#include <windows.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libetude/types.h"
#include "libetude/platform/windows.h"

// 작업 콜백 함수 타입 정의
typedef void (*ETThreadPoolCallback)(void* context);

// 작업 항목 구조체
typedef struct {
    ETThreadPoolCallback callback;  // 실행할 콜백 함수
    void* context;                 // 콜백에 전달할 컨텍스트
    volatile LONG completed;       // 완료 상태 (InterlockedXxx 사용)
    HANDLE completion_event;       // 완료 이벤트 (선택사항)
} ETThreadPoolWorkItem;

// Windows Thread Pool 관리 구조체
typedef struct {
    PTP_POOL thread_pool;              // Windows Thread Pool 핸들
    PTP_CLEANUP_GROUP cleanup_group;   // 정리 그룹
    TP_CALLBACK_ENVIRON callback_env;  // 콜백 환경
    DWORD min_threads;                 // 최소 스레드 수
    DWORD max_threads;                 // 최대 스레드 수
    volatile LONG active_work_items;   // 활성 작업 항목 수
    bool is_initialized;               // 초기화 상태
    CRITICAL_SECTION lock;             // 동기화용 크리티컬 섹션
} ETWindowsThreadPoolImpl;

// 전역 스레드 풀 인스턴스
static ETWindowsThreadPoolImpl g_thread_pool = {0};

/**
 * @brief Thread Pool 작업 콜백 래퍼 함수
 *
 * Windows Thread Pool API에서 호출되는 콜백 함수입니다.
 *
 * @param instance Thread Pool 인스턴스
 * @param context 작업 컨텍스트 (ETThreadPoolWorkItem*)
 * @param work 작업 핸들
 */
static VOID CALLBACK thread_pool_work_callback(PTP_CALLBACK_INSTANCE instance,
                                             PVOID context,
                                             PTP_WORK work) {
    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(work);

    ETThreadPoolWorkItem* work_item = (ETThreadPoolWorkItem*)context;
    if (!work_item || !work_item->callback) {
        return;
    }

    // 작업 실행
    work_item->callback(work_item->context);

    // 완료 상태 설정
    InterlockedExchange(&work_item->completed, 1);

    // 완료 이벤트 신호 (설정된 경우)
    if (work_item->completion_event) {
        SetEvent(work_item->completion_event);
    }

    // 활성 작업 항목 수 감소
    InterlockedDecrement(&g_thread_pool.active_work_items);
}

/**
 * @brief Windows Thread Pool 초기화
 *
 * @param pool Thread Pool 구조체 포인터
 * @param min_threads 최소 스레드 수
 * @param max_threads 최대 스레드 수
 * @return ETResult 초기화 결과
 */
ETResult et_windows_threadpool_init(ETWindowsThreadPool* pool,
                                  DWORD min_threads, DWORD max_threads) {
    if (!pool) {
        return ET_RESULT_INVALID_PARAMETER;
    }

    // 이미 초기화된 경우 정리 후 재초기화
    if (g_thread_pool.is_initialized) {
        et_windows_threadpool_finalize();
    }

    // 크리티컬 섹션 초기화
    InitializeCriticalSection(&g_thread_pool.lock);

    // Thread Pool 생성
    g_thread_pool.thread_pool = CreateThreadpool(NULL);
    if (!g_thread_pool.thread_pool) {
        DeleteCriticalSection(&g_thread_pool.lock);
        return ET_RESULT_SYSTEM_ERROR;
    }

    // 스레드 수 설정
    g_thread_pool.min_threads = min_threads;
    g_thread_pool.max_threads = max_threads;

    SetThreadpoolThreadMinimum(g_thread_pool.thread_pool, min_threads);
    SetThreadpoolThreadMaximum(g_thread_pool.thread_pool, max_threads);

    // 정리 그룹 생성
    g_thread_pool.cleanup_group = CreateThreadpoolCleanupGroup();
    if (!g_thread_pool.cleanup_group) {
        CloseThreadpool(g_thread_pool.thread_pool);
        DeleteCriticalSection(&g_thread_pool.lock);
        return ET_RESULT_SYSTEM_ERROR;
    }

    // 콜백 환경 초기화
    InitializeThreadpoolEnvironment(&g_thread_pool.callback_env);
    SetThreadpoolCallbackPool(&g_thread_pool.callback_env, g_thread_pool.thread_pool);
    SetThreadpoolCallbackCleanupGroup(&g_thread_pool.callback_env,
                                    g_thread_pool.cleanup_group, NULL);

    g_thread_pool.active_work_items = 0;
    g_thread_pool.is_initialized = true;

    // 외부 구조체에 내부 구조체 포인터 설정
    pool->thread_pool = g_thread_pool.thread_pool;
    pool->cleanup_group = g_thread_pool.cleanup_group;
    pool->callback_env = g_thread_pool.callback_env;

    return ET_RESULT_SUCCESS;
}

/**
 * @brief Windows Thread Pool 정리
 */
void et_windows_threadpool_finalize(void) {
    if (!g_thread_pool.is_initialized) {
        return;
    }

    EnterCriticalSection(&g_thread_pool.lock);

    // 모든 작업 완료 대기
    CloseThreadpoolCleanupGroupMembers(g_thread_pool.cleanup_group, FALSE, NULL);

    // 콜백 환경 정리
    DestroyThreadpoolEnvironment(&g_thread_pool.callback_env);

    // 정리 그룹 닫기
    if (g_thread_pool.cleanup_group) {
        CloseThreadpoolCleanupGroup(g_thread_pool.cleanup_group);
        g_thread_pool.cleanup_group = NULL;
    }

    // Thread Pool 닫기
    if (g_thread_pool.thread_pool) {
        CloseThreadpool(g_thread_pool.thread_pool);
        g_thread_pool.thread_pool = NULL;
    }

    g_thread_pool.is_initialized = false;
    g_thread_pool.active_work_items = 0;

    LeaveCriticalSection(&g_thread_pool.lock);
    DeleteCriticalSection(&g_thread_pool.lock);

    memset(&g_thread_pool, 0, sizeof(g_thread_pool));
}

/**
 * @brief 작업을 Thread Pool에 제출
 *
 * @param callback 실행할 콜백 함수
 * @param context 콜백에 전달할 컨텍스트
 * @param wait_for_completion 완료까지 대기할지 여부
 * @return ETResult 제출 결과
 */
ETResult et_windows_threadpool_submit_work(ETThreadPoolCallback callback,
                                         void* context,
                                         bool wait_for_completion) {
    if (!callback || !g_thread_pool.is_initialized) {
        return ET_RESULT_INVALID_PARAMETER;
    }

    // 작업 항목 생성
    ETThreadPoolWorkItem* work_item = (ETThreadPoolWorkItem*)malloc(sizeof(ETThreadPoolWorkItem));
    if (!work_item) {
        return ET_RESULT_OUT_OF_MEMORY;
    }

    work_item->callback = callback;
    work_item->context = context;
    work_item->completed = 0;
    work_item->completion_event = NULL;

    // 완료 대기가 필요한 경우 이벤트 생성
    if (wait_for_completion) {
        work_item->completion_event = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!work_item->completion_event) {
            free(work_item);
            return ET_RESULT_SYSTEM_ERROR;
        }
    }

    // Thread Pool 작업 생성
    PTP_WORK work = CreateThreadpoolWork(thread_pool_work_callback,
                                       work_item,
                                       &g_thread_pool.callback_env);
    if (!work) {
        if (work_item->completion_event) {
            CloseHandle(work_item->completion_event);
        }
        free(work_item);
        return ET_RESULT_SYSTEM_ERROR;
    }

    // 활성 작업 항목 수 증가
    InterlockedIncrement(&g_thread_pool.active_work_items);

    // 작업 제출
    SubmitThreadpoolWork(work);

    ETResult result = ET_RESULT_SUCCESS;

    // 완료 대기 (요청된 경우)
    if (wait_for_completion) {
        DWORD wait_result = WaitForSingleObject(work_item->completion_event, INFINITE);
        if (wait_result != WAIT_OBJECT_0) {
            result = ET_RESULT_TIMEOUT;
        }
        CloseHandle(work_item->completion_event);
    }

    // 작업 정리
    WaitForThreadpoolWorkCallbacks(work, FALSE);
    CloseThreadpoolWork(work);
    free(work_item);

    return result;
}

/**
 * @brief 비동기 작업을 Thread Pool에 제출 (완료 대기 없음)
 *
 * @param callback 실행할 콜백 함수
 * @param context 콜백에 전달할 컨텍스트
 * @return ETResult 제출 결과
 */
ETResult et_windows_threadpool_submit_async(ETThreadPoolCallback callback, void* context) {
    return et_windows_threadpool_submit_work(callback, context, false);
}

/**
 * @brief 동기 작업을 Thread Pool에 제출 (완료까지 대기)
 *
 * @param callback 실행할 콜백 함수
 * @param context 콜백에 전달할 컨텍스트
 * @return ETResult 제출 결과
 */
ETResult et_windows_threadpool_submit_sync(ETThreadPoolCallback callback, void* context) {
    return et_windows_threadpool_submit_work(callback, context, true);
}

/**
 * @brief 모든 작업 완료까지 대기
 *
 * @param timeout_ms 타임아웃 (밀리초, INFINITE는 무한 대기)
 * @return ETResult 대기 결과
 */
ETResult et_windows_threadpool_wait_all(DWORD timeout_ms) {
    if (!g_thread_pool.is_initialized) {
        return ET_RESULT_INVALID_STATE;
    }

    DWORD start_time = GetTickCount();

    while (InterlockedCompareExchange(&g_thread_pool.active_work_items, 0, 0) > 0) {
        if (timeout_ms != INFINITE) {
            DWORD elapsed = GetTickCount() - start_time;
            if (elapsed >= timeout_ms) {
                return ET_RESULT_TIMEOUT;
            }
        }

        Sleep(1); // 1ms 대기
    }

    return ET_RESULT_SUCCESS;
}

/**
 * @brief Thread Pool 상태 정보 조회
 *
 * @param active_work_items 활성 작업 항목 수 (출력)
 * @param min_threads 최소 스레드 수 (출력)
 * @param max_threads 최대 스레드 수 (출력)
 * @return ETResult 조회 결과
 */
ETResult et_windows_threadpool_get_status(LONG* active_work_items,
                                        DWORD* min_threads,
                                        DWORD* max_threads) {
    if (!g_thread_pool.is_initialized) {
        return ET_RESULT_INVALID_STATE;
    }

    if (active_work_items) {
        *active_work_items = InterlockedCompareExchange(&g_thread_pool.active_work_items, 0, 0);
    }

    if (min_threads) {
        *min_threads = g_thread_pool.min_threads;
    }

    if (max_threads) {
        *max_threads = g_thread_pool.max_threads;
    }

    return ET_RESULT_SUCCESS;
}

/**
 * @brief Thread Pool 설정 변경
 *
 * @param min_threads 새로운 최소 스레드 수 (0이면 변경하지 않음)
 * @param max_threads 새로운 최대 스레드 수 (0이면 변경하지 않음)
 * @return ETResult 설정 변경 결과
 */
ETResult et_windows_threadpool_configure(DWORD min_threads, DWORD max_threads) {
    if (!g_thread_pool.is_initialized) {
        return ET_RESULT_INVALID_STATE;
    }

    EnterCriticalSection(&g_thread_pool.lock);

    if (min_threads > 0) {
        if (!SetThreadpoolThreadMinimum(g_thread_pool.thread_pool, min_threads)) {
            LeaveCriticalSection(&g_thread_pool.lock);
            return ET_RESULT_SYSTEM_ERROR;
        }
        g_thread_pool.min_threads = min_threads;
    }

    if (max_threads > 0) {
        SetThreadpoolThreadMaximum(g_thread_pool.thread_pool, max_threads);
        g_thread_pool.max_threads = max_threads;
    }

    LeaveCriticalSection(&g_thread_pool.lock);
    return ET_RESULT_SUCCESS;
}

/**
 * @brief Thread Pool 초기화 상태 확인
 *
 * @return bool 초기화 여부
 */
bool et_windows_threadpool_is_initialized(void) {
    return g_thread_pool.is_initialized;
}