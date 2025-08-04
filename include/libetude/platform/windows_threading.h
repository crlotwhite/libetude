/**
 * @file windows_threading.h
 * @brief Windows Thread Pool API를 사용한 스레딩 시스템 헤더
 *
 * Windows Thread Pool API를 활용한 효율적인 작업 스케줄링과
 * 스레드 관리 기능을 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_WINDOWS_THREADING_H
#define LIBETUDE_PLATFORM_WINDOWS_THREADING_H

#ifdef _WIN32

#include <windows.h>
#include <stdbool.h>
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 작업 콜백 함수 타입
 *
 * Thread Pool에서 실행될 작업의 콜백 함수 타입입니다.
 *
 * @param context 작업 실행 시 전달받을 컨텍스트 데이터
 */
typedef void (*ETThreadPoolCallback)(void* context);

/**
 * @brief Windows Thread Pool 래퍼 구조체
 *
 * Windows Thread Pool API를 래핑한 구조체입니다.
 * 내부 구현은 불투명하며, 함수를 통해서만 접근해야 합니다.
 */
typedef struct {
    PTP_POOL thread_pool;              ///< Windows Thread Pool 핸들
    PTP_CLEANUP_GROUP cleanup_group;   ///< 정리 그룹
    TP_CALLBACK_ENVIRON callback_env;  ///< 콜백 환경
} ETWindowsThreadPool;

// Thread Pool 관리 함수들

/**
 * @brief Windows Thread Pool 초기화
 *
 * @param pool Thread Pool 구조체 포인터
 * @param min_threads 최소 스레드 수 (1 이상)
 * @param max_threads 최대 스레드 수 (min_threads 이상)
 * @return ETResult 초기화 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_INVALID_PARAMETER: 잘못된 매개변수
 *   - ET_RESULT_SYSTEM_ERROR: 시스템 오류
 */
ETResult et_windows_threadpool_init(ETWindowsThreadPool* pool,
                                  DWORD min_threads, DWORD max_threads);

/**
 * @brief Windows Thread Pool 정리
 *
 * 모든 작업이 완료될 때까지 대기한 후 Thread Pool을 정리합니다.
 */
void et_windows_threadpool_finalize(void);

// 작업 제출 함수들

/**
 * @brief 비동기 작업을 Thread Pool에 제출
 *
 * 작업을 Thread Pool에 제출하고 즉시 반환합니다.
 * 작업 완료를 기다리지 않습니다.
 *
 * @param callback 실행할 콜백 함수
 * @param context 콜백에 전달할 컨텍스트 (NULL 가능)
 * @return ETResult 제출 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_INVALID_PARAMETER: 잘못된 매개변수
 *   - ET_RESULT_INVALID_STATE: Thread Pool이 초기화되지 않음
 *   - ET_RESULT_OUT_OF_MEMORY: 메모리 부족
 *   - ET_RESULT_SYSTEM_ERROR: 시스템 오류
 */
ETResult et_windows_threadpool_submit_async(ETThreadPoolCallback callback, void* context);

/**
 * @brief 동기 작업을 Thread Pool에 제출
 *
 * 작업을 Thread Pool에 제출하고 완료까지 대기합니다.
 *
 * @param callback 실행할 콜백 함수
 * @param context 콜백에 전달할 컨텍스트 (NULL 가능)
 * @return ETResult 제출 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_INVALID_PARAMETER: 잘못된 매개변수
 *   - ET_RESULT_INVALID_STATE: Thread Pool이 초기화되지 않음
 *   - ET_RESULT_OUT_OF_MEMORY: 메모리 부족
 *   - ET_RESULT_SYSTEM_ERROR: 시스템 오류
 *   - ET_RESULT_TIMEOUT: 타임아웃 발생
 */
ETResult et_windows_threadpool_submit_sync(ETThreadPoolCallback callback, void* context);

/**
 * @brief 작업을 Thread Pool에 제출 (대기 옵션 지정)
 *
 * @param callback 실행할 콜백 함수
 * @param context 콜백에 전달할 컨텍스트 (NULL 가능)
 * @param wait_for_completion 완료까지 대기할지 여부
 * @return ETResult 제출 결과
 */
ETResult et_windows_threadpool_submit_work(ETThreadPoolCallback callback,
                                         void* context,
                                         bool wait_for_completion);

// 상태 관리 함수들

/**
 * @brief 모든 작업 완료까지 대기
 *
 * 현재 실행 중인 모든 작업이 완료될 때까지 대기합니다.
 *
 * @param timeout_ms 타임아웃 (밀리초, INFINITE는 무한 대기)
 * @return ETResult 대기 결과
 *   - ET_RESULT_SUCCESS: 모든 작업 완료
 *   - ET_RESULT_TIMEOUT: 타임아웃 발생
 *   - ET_RESULT_INVALID_STATE: Thread Pool이 초기화되지 않음
 */
ETResult et_windows_threadpool_wait_all(DWORD timeout_ms);

/**
 * @brief Thread Pool 상태 정보 조회
 *
 * @param active_work_items 활성 작업 항목 수 (출력, NULL 가능)
 * @param min_threads 최소 스레드 수 (출력, NULL 가능)
 * @param max_threads 최대 스레드 수 (출력, NULL 가능)
 * @return ETResult 조회 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_INVALID_STATE: Thread Pool이 초기화되지 않음
 */
ETResult et_windows_threadpool_get_status(LONG* active_work_items,
                                        DWORD* min_threads,
                                        DWORD* max_threads);

/**
 * @brief Thread Pool 설정 변경
 *
 * 실행 중인 Thread Pool의 스레드 수 설정을 변경합니다.
 *
 * @param min_threads 새로운 최소 스레드 수 (0이면 변경하지 않음)
 * @param max_threads 새로운 최대 스레드 수 (0이면 변경하지 않음)
 * @return ETResult 설정 변경 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_INVALID_STATE: Thread Pool이 초기화되지 않음
 *   - ET_RESULT_SYSTEM_ERROR: 시스템 오류
 */
ETResult et_windows_threadpool_configure(DWORD min_threads, DWORD max_threads);

/**
 * @brief Thread Pool 초기화 상태 확인
 *
 * @return bool Thread Pool이 초기화되었는지 여부
 */
bool et_windows_threadpool_is_initialized(void);

// 편의 함수들

/**
 * @brief 기본 설정으로 Thread Pool 초기화
 *
 * 시스템 CPU 코어 수에 기반하여 적절한 스레드 수로 초기화합니다.
 *
 * @param pool Thread Pool 구조체 포인터
 * @return ETResult 초기화 결과
 */
static inline ETResult et_windows_threadpool_init_default(ETWindowsThreadPool* pool) {
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    DWORD num_cores = sys_info.dwNumberOfProcessors;
    DWORD min_threads = num_cores;
    DWORD max_threads = num_cores * 2;

    return et_windows_threadpool_init(pool, min_threads, max_threads);
}

/**
 * @brief CPU 집약적 작업용 Thread Pool 초기화
 *
 * CPU 집약적 작업에 최적화된 설정으로 초기화합니다.
 *
 * @param pool Thread Pool 구조체 포인터
 * @return ETResult 초기화 결과
 */
static inline ETResult et_windows_threadpool_init_cpu_intensive(ETWindowsThreadPool* pool) {
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    DWORD num_cores = sys_info.dwNumberOfProcessors;
    DWORD min_threads = num_cores;
    DWORD max_threads = num_cores; // CPU 집약적 작업은 코어 수와 동일하게

    return et_windows_threadpool_init(pool, min_threads, max_threads);
}

/**
 * @brief I/O 집약적 작업용 Thread Pool 초기화
 *
 * I/O 집약적 작업에 최적화된 설정으로 초기화합니다.
 *
 * @param pool Thread Pool 구조체 포인터
 * @return ETResult 초기화 결과
 */
static inline ETResult et_windows_threadpool_init_io_intensive(ETWindowsThreadPool* pool) {
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    DWORD num_cores = sys_info.dwNumberOfProcessors;
    DWORD min_threads = num_cores;
    DWORD max_threads = num_cores * 4; // I/O 집약적 작업은 더 많은 스레드

    return et_windows_threadpool_init(pool, min_threads, max_threads);
}

#ifdef __cplusplus
}
#endif

#endif // _WIN32

#endif // LIBETUDE_PLATFORM_WINDOWS_THREADING_H