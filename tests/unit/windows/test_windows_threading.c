/**
 * @file test_windows_threading.c
 * @brief Windows Thread Pool 기능 테스트
 *
 * Windows Thread Pool API를 사용한 스레딩 시스템을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include "libetude/platform/windows_threading.h"
#include "libetude/types.h"

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return 0; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

// 테스트용 전역 변수
static volatile LONG g_counter = 0;
static volatile LONG g_completed_tasks = 0;

/**
 * @brief 간단한 카운터 증가 작업
 */
static void simple_counter_task(void* context) {
    UNREFERENCED_PARAMETER(context);

    InterlockedIncrement(&g_counter);
    Sleep(10); // 10ms 작업 시뮬레이션
    InterlockedIncrement(&g_completed_tasks);
}

/**
 * @brief 컨텍스트 데이터를 사용하는 작업
 */
static void context_task(void* context) {
    if (context) {
        int* value = (int*)context;
        InterlockedAdd(&g_counter, *value);
    }
    InterlockedIncrement(&g_completed_tasks);
}

/**
 * @brief CPU 집약적 작업 시뮬레이션
 */
static void cpu_intensive_task(void* context) {
    UNREFERENCED_PARAMETER(context);

    // 간단한 계산 작업
    volatile double result = 0.0;
    for (int i = 0; i < 100000; i++) {
        result += sqrt((double)i);
    }

    InterlockedIncrement(&g_completed_tasks);
}

/**
 * @brief Thread Pool 초기화/정리 테스트
 */
static int test_threadpool_lifecycle(void) {
    printf("\n=== Thread Pool 생명주기 테스트 ===\n");

    ETWindowsThreadPool pool;

    // 초기화 테스트
    ETResult result = et_windows_threadpool_init(&pool, 2, 8);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 초기화 성공");
    TEST_ASSERT(et_windows_threadpool_is_initialized(), "Thread Pool 초기화 상태 확인");

    // 상태 조회 테스트
    LONG active_items;
    DWORD min_threads, max_threads;
    result = et_windows_threadpool_get_status(&active_items, &min_threads, &max_threads);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 상태 조회 성공");
    TEST_ASSERT(min_threads == 2, "최소 스레드 수 확인");
    TEST_ASSERT(max_threads == 8, "최대 스레드 수 확인");
    TEST_ASSERT(active_items == 0, "초기 활성 작업 수 확인");

    printf("Thread Pool 설정: min=%lu, max=%lu, active=%ld\n",
           min_threads, max_threads, active_items);

    // 정리 테스트
    et_windows_threadpool_finalize();
    TEST_ASSERT(!et_windows_threadpool_is_initialized(), "Thread Pool 정리 후 상태 확인");

    return 1;
}

/**
 * @brief 기본 설정 초기화 테스트
 */
static int test_default_initialization(void) {
    printf("\n=== 기본 설정 초기화 테스트 ===\n");

    ETWindowsThreadPool pool;

    // 기본 설정으로 초기화
    ETResult result = et_windows_threadpool_init_default(&pool);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "기본 설정 초기화 성공");

    // 시스템 정보 확인
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    printf("시스템 CPU 코어 수: %lu\n", sys_info.dwNumberOfProcessors);

    DWORD min_threads, max_threads;
    et_windows_threadpool_get_status(NULL, &min_threads, &max_threads);
    printf("설정된 스레드 수: min=%lu, max=%lu\n", min_threads, max_threads);

    TEST_ASSERT(min_threads == sys_info.dwNumberOfProcessors, "최소 스레드 수가 CPU 코어 수와 일치");
    TEST_ASSERT(max_threads == sys_info.dwNumberOfProcessors * 2, "최대 스레드 수가 CPU 코어 수의 2배");

    et_windows_threadpool_finalize();
    return 1;
}

/**
 * @brief 비동기 작업 제출 테스트
 */
static int test_async_work_submission(void) {
    printf("\n=== 비동기 작업 제출 테스트 ===\n");

    ETWindowsThreadPool pool;
    ETResult result = et_windows_threadpool_init(&pool, 2, 4);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 초기화");

    // 전역 카운터 초기화
    InterlockedExchange(&g_counter, 0);
    InterlockedExchange(&g_completed_tasks, 0);

    const int num_tasks = 10;

    // 비동기 작업 제출
    for (int i = 0; i < num_tasks; i++) {
        result = et_windows_threadpool_submit_async(simple_counter_task, NULL);
        TEST_ASSERT(result == ET_RESULT_SUCCESS, "비동기 작업 제출 성공");
    }

    // 모든 작업 완료 대기
    result = et_windows_threadpool_wait_all(5000); // 5초 타임아웃
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "모든 작업 완료 대기");

    // 결과 확인
    LONG final_counter = InterlockedCompareExchange(&g_counter, 0, 0);
    LONG final_completed = InterlockedCompareExchange(&g_completed_tasks, 0, 0);

    printf("실행된 작업 수: %ld, 완료된 작업 수: %ld\n", final_counter, final_completed);
    TEST_ASSERT(final_counter == num_tasks, "모든 카운터 작업 완료");
    TEST_ASSERT(final_completed == num_tasks, "모든 작업 완료 확인");

    et_windows_threadpool_finalize();
    return 1;
}

/**
 * @brief 동기 작업 제출 테스트
 */
static int test_sync_work_submission(void) {
    printf("\n=== 동기 작업 제출 테스트 ===\n");

    ETWindowsThreadPool pool;
    ETResult result = et_windows_threadpool_init(&pool, 2, 4);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 초기화");

    // 전역 카운터 초기화
    InterlockedExchange(&g_counter, 0);
    InterlockedExchange(&g_completed_tasks, 0);

    const int num_tasks = 5;

    // 동기 작업 제출 (순차적으로 완료됨)
    for (int i = 0; i < num_tasks; i++) {
        result = et_windows_threadpool_submit_sync(simple_counter_task, NULL);
        TEST_ASSERT(result == ET_RESULT_SUCCESS, "동기 작업 제출 및 완료");

        // 각 작업이 완료된 후 카운터 확인
        LONG current_counter = InterlockedCompareExchange(&g_counter, 0, 0);
        TEST_ASSERT(current_counter == i + 1, "동기 작업 순차 완료 확인");
    }

    LONG final_counter = InterlockedCompareExchange(&g_counter, 0, 0);
    LONG final_completed = InterlockedCompareExchange(&g_completed_tasks, 0, 0);

    printf("최종 카운터: %ld, 완료된 작업 수: %ld\n", final_counter, final_completed);
    TEST_ASSERT(final_counter == num_tasks, "모든 동기 작업 완료");
    TEST_ASSERT(final_completed == num_tasks, "모든 작업 완료 확인");

    et_windows_threadpool_finalize();
    return 1;
}

/**
 * @brief 컨텍스트 데이터 전달 테스트
 */
static int test_context_data_passing(void) {
    printf("\n=== 컨텍스트 데이터 전달 테스트 ===\n");

    ETWindowsThreadPool pool;
    ETResult result = et_windows_threadpool_init(&pool, 2, 4);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 초기화");

    // 전역 카운터 초기화
    InterlockedExchange(&g_counter, 0);
    InterlockedExchange(&g_completed_tasks, 0);

    const int num_tasks = 5;
    int values[5] = {10, 20, 30, 40, 50};
    int expected_sum = 10 + 20 + 30 + 40 + 50; // 150

    // 각기 다른 값을 가진 작업 제출
    for (int i = 0; i < num_tasks; i++) {
        result = et_windows_threadpool_submit_async(context_task, &values[i]);
        TEST_ASSERT(result == ET_RESULT_SUCCESS, "컨텍스트 작업 제출 성공");
    }

    // 모든 작업 완료 대기
    result = et_windows_threadpool_wait_all(3000);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "컨텍스트 작업 완료 대기");

    LONG final_counter = InterlockedCompareExchange(&g_counter, 0, 0);
    LONG final_completed = InterlockedCompareExchange(&g_completed_tasks, 0, 0);

    printf("최종 카운터 합계: %ld (예상: %d), 완료된 작업 수: %ld\n",
           final_counter, expected_sum, final_completed);

    TEST_ASSERT(final_counter == expected_sum, "컨텍스트 데이터 정확한 전달");
    TEST_ASSERT(final_completed == num_tasks, "모든 컨텍스트 작업 완료");

    et_windows_threadpool_finalize();
    return 1;
}

/**
 * @brief 성능 벤치마크 테스트
 */
static int test_performance_benchmark(void) {
    printf("\n=== 성능 벤치마크 테스트 ===\n");

    ETWindowsThreadPool pool;
    ETResult result = et_windows_threadpool_init(&pool, 4, 8);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 초기화");

    // 전역 카운터 초기화
    InterlockedExchange(&g_completed_tasks, 0);

    const int num_tasks = 100;

    // 성능 측정 시작
    DWORD start_time = GetTickCount();

    // CPU 집약적 작업 제출
    for (int i = 0; i < num_tasks; i++) {
        result = et_windows_threadpool_submit_async(cpu_intensive_task, NULL);
        TEST_ASSERT(result == ET_RESULT_SUCCESS, "CPU 집약적 작업 제출");
    }

    // 모든 작업 완료 대기
    result = et_windows_threadpool_wait_all(30000); // 30초 타임아웃
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "CPU 집약적 작업 완료 대기");

    DWORD end_time = GetTickCount();
    DWORD elapsed_time = end_time - start_time;

    LONG final_completed = InterlockedCompareExchange(&g_completed_tasks, 0, 0);

    printf("CPU 집약적 작업 성능:\n");
    printf("  작업 수: %d\n", num_tasks);
    printf("  완료된 작업: %ld\n", final_completed);
    printf("  총 소요 시간: %lu ms\n", elapsed_time);
    printf("  작업당 평균 시간: %.2f ms\n", (double)elapsed_time / num_tasks);

    TEST_ASSERT(final_completed == num_tasks, "모든 CPU 집약적 작업 완료");

    et_windows_threadpool_finalize();
    return 1;
}

/**
 * @brief Thread Pool 설정 변경 테스트
 */
static int test_threadpool_configuration(void) {
    printf("\n=== Thread Pool 설정 변경 테스트 ===\n");

    ETWindowsThreadPool pool;
    ETResult result = et_windows_threadpool_init(&pool, 2, 4);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 초기화");

    // 초기 설정 확인
    DWORD min_threads, max_threads;
    et_windows_threadpool_get_status(NULL, &min_threads, &max_threads);
    TEST_ASSERT(min_threads == 2 && max_threads == 4, "초기 설정 확인");

    // 설정 변경
    result = et_windows_threadpool_configure(4, 8);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "Thread Pool 설정 변경 성공");

    // 변경된 설정 확인
    et_windows_threadpool_get_status(NULL, &min_threads, &max_threads);
    printf("변경된 설정: min=%lu, max=%lu\n", min_threads, max_threads);
    TEST_ASSERT(min_threads == 4 && max_threads == 8, "변경된 설정 확인");

    // 부분 설정 변경 (최대 스레드만)
    result = et_windows_threadpool_configure(0, 12);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "부분 설정 변경 성공");

    et_windows_threadpool_get_status(NULL, &min_threads, &max_threads);
    TEST_ASSERT(min_threads == 4 && max_threads == 12, "부분 설정 변경 확인");

    et_windows_threadpool_finalize();
    return 1;
}

/**
 * @brief 오류 처리 테스트
 */
static int test_error_handling(void) {
    printf("\n=== 오류 처리 테스트 ===\n");

    // 초기화되지 않은 상태에서 작업 제출
    ETResult result = et_windows_threadpool_submit_async(simple_counter_task, NULL);
    TEST_ASSERT(result == ET_RESULT_INVALID_PARAMETER || result == ET_RESULT_INVALID_STATE,
                "초기화되지 않은 상태에서 작업 제출 오류 처리");

    // NULL 콜백으로 작업 제출
    ETWindowsThreadPool pool;
    et_windows_threadpool_init(&pool, 2, 4);

    result = et_windows_threadpool_submit_async(NULL, NULL);
    TEST_ASSERT(result == ET_RESULT_INVALID_PARAMETER, "NULL 콜백 오류 처리");

    // 잘못된 매개변수로 초기화
    ETWindowsThreadPool invalid_pool;
    result = et_windows_threadpool_init(NULL, 2, 4);
    TEST_ASSERT(result == ET_RESULT_INVALID_PARAMETER, "NULL 포인터 초기화 오류 처리");

    et_windows_threadpool_finalize();
    return 1;
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("Windows Thread Pool 테스트 시작\n");
    printf("=====================================\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 각 테스트 실행
    total_tests++; if (test_threadpool_lifecycle()) passed_tests++;
    total_tests++; if (test_default_initialization()) passed_tests++;
    total_tests++; if (test_async_work_submission()) passed_tests++;
    total_tests++; if (test_sync_work_submission()) passed_tests++;
    total_tests++; if (test_context_data_passing()) passed_tests++;
    total_tests++; if (test_performance_benchmark()) passed_tests++;
    total_tests++; if (test_threadpool_configuration()) passed_tests++;
    total_tests++; if (test_error_handling()) passed_tests++;

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", passed_tests, total_tests);

    if (passed_tests == total_tests) {
        printf("모든 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다.\n");
        return 1;
    }
}

#else
int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}
#endif // _WIN32