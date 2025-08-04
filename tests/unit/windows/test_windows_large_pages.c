/**
 * @file test_windows_large_pages.c
 * @brief Windows Large Page 메모리 지원 테스트
 *
 * Windows Large Page 메모리 할당 및 관리 기능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include "libetude/platform/windows_large_pages.h"
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

// 테스트 상수
#define TEST_SMALL_SIZE (4 * 1024)          // 4KB
#define TEST_MEDIUM_SIZE (64 * 1024)        // 64KB
#define TEST_LARGE_SIZE (2 * 1024 * 1024)   // 2MB
#define TEST_HUGE_SIZE (8 * 1024 * 1024)    // 8MB

/**
 * @brief Large Page 초기화/정리 테스트
 */
static int test_large_page_lifecycle(void) {
    printf("\n=== Large Page 생명주기 테스트 ===\n");

    // 초기화 테스트
    ETResult result = et_windows_large_pages_init();
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "Large Page 초기화 성공");

    // 상태 정보 조회
    ETLargePageInfo info;
    result = et_windows_large_pages_get_info(&info);
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "Large Page 상태 정보 조회 성공");

    printf("Large Page Support Info:\n");
    printf("  System Support: %s\n", info.is_supported ? "Yes" : "No");
    printf("  Privilege Enabled: %s\n", info.privilege_enabled ? "Yes" : "No");
    printf("  Large Page 크기: %zu bytes (%.1f MB)\n",
           info.large_page_size, (double)info.large_page_size / (1024 * 1024));

    // 상태 문자열 테스트
    char status_str[1024];
    result = et_windows_large_pages_status_to_string(status_str, sizeof(status_str));
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "상태 문자열 변환 성공");
    printf("상태 문자열:\n%s\n", status_str);

    // 중복 초기화 테스트
    result = et_windows_large_pages_init();
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "중복 초기화 처리");

    // 정리 테스트
    et_windows_large_pages_finalize();
    printf("PASS: Large Page 정리 완료\n");

    return 1;
}

/**
 * @brief 권한 활성화 테스트
 */
static int test_privilege_activation(void) {
    printf("\n=== 권한 활성화 테스트 ===\n");

    // 권한 활성화 시도
    bool privilege_result = et_windows_enable_large_page_privilege();

    if (privilege_result) {
        printf("PASS: SeLockMemoryPrivilege 권한 활성화 성공\n");
    } else {
        printf("INFO: SeLockMemoryPrivilege 권한 활성화 실패 (관리자 권한 필요할 수 있음)\n");
        printf("PASS: 권한 활성화 함수 정상 동작\n");
    }

    // 초기화 후 권한 상태 확인
    et_windows_large_pages_init();

    ETLargePageInfo info;
    et_windows_large_pages_get_info(&info);

    printf("Privilege status after init: %s\n", info.privilege_enabled ? "Enabled" : "Disabled");

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 기본 메모리 할당/해제 테스트
 */
static int test_basic_allocation(void) {
    printf("\n=== 기본 메모리 할당/해제 테스트 ===\n");

    et_windows_large_pages_init();

    // 다양한 크기로 할당 테스트
    struct {
        size_t size;
        const char* description;
    } test_sizes[] = {
        {TEST_SMALL_SIZE, "작은 크기 (4KB)"},
        {TEST_MEDIUM_SIZE, "중간 크기 (64KB)"},
        {TEST_LARGE_SIZE, "큰 크기 (2MB)"},
        {TEST_HUGE_SIZE, "매우 큰 크기 (8MB)"}
    };

    for (int i = 0; i < 4; i++) {
        printf("Test: %s\n", test_sizes[i].description);

        void* memory = et_windows_alloc_large_pages(test_sizes[i].size);
        TEST_ASSERT(memory != NULL, "메모리 할당 성공");

        // 메모리 쓰기 테스트
        memset(memory, 0xAA, test_sizes[i].size);

        // 메모리 읽기 테스트
        unsigned char* bytes = (unsigned char*)memory;
        bool write_success = true;
        for (size_t j = 0; j < test_sizes[i].size; j += 4096) {
            if (bytes[j] != 0xAA) {
                write_success = false;
                break;
            }
        }
        TEST_ASSERT(write_success, "메모리 읽기/쓰기 정상 동작");

        // 메모리 해제
        et_windows_free_large_pages(memory, test_sizes[i].size);
        printf("PASS: 메모리 해제 완료\n");
    }

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 메모리 재할당 테스트
 */
static int test_memory_reallocation(void) {
    printf("\n=== 메모리 재할당 테스트 ===\n");

    et_windows_large_pages_init();

    // 초기 할당
    size_t initial_size = TEST_MEDIUM_SIZE;
    void* memory = et_windows_alloc_large_pages(initial_size);
    TEST_ASSERT(memory != NULL, "초기 메모리 할당 성공");

    // 데이터 쓰기
    memset(memory, 0x55, initial_size);

    // 크기 확장
    size_t new_size = TEST_LARGE_SIZE;
    void* new_memory = et_windows_realloc_large_pages(memory, initial_size, new_size);
    TEST_ASSERT(new_memory != NULL, "메모리 재할당 (확장) 성공");

    // 기존 데이터 확인
    unsigned char* bytes = (unsigned char*)new_memory;
    bool data_preserved = true;
    for (size_t i = 0; i < initial_size; i += 4096) {
        if (bytes[i] != 0x55) {
            data_preserved = false;
            break;
        }
    }
    TEST_ASSERT(data_preserved, "재할당 후 기존 데이터 보존");

    // 크기 축소
    size_t smaller_size = TEST_SMALL_SIZE;
    void* smaller_memory = et_windows_realloc_large_pages(new_memory, new_size, smaller_size);
    TEST_ASSERT(smaller_memory != NULL, "메모리 재할당 (축소) 성공");

    // 축소된 데이터 확인
    bytes = (unsigned char*)smaller_memory;
    data_preserved = true;
    for (size_t i = 0; i < smaller_size; i += 1024) {
        if (bytes[i] != 0x55) {
            data_preserved = false;
            break;
        }
    }
    TEST_ASSERT(data_preserved, "축소 후 데이터 보존");

    // NULL로 해제 테스트
    void* null_result = et_windows_realloc_large_pages(smaller_memory, smaller_size, 0);
    TEST_ASSERT(null_result == NULL, "크기 0으로 재할당 시 NULL 반환");

    // NULL에서 할당 테스트
    void* from_null = et_windows_realloc_large_pages(NULL, 0, TEST_MEDIUM_SIZE);
    TEST_ASSERT(from_null != NULL, "NULL에서 재할당 성공");

    et_windows_free_large_pages(from_null, TEST_MEDIUM_SIZE);

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 정렬된 메모리 할당 테스트
 */
static int test_aligned_allocation(void) {
    printf("\n=== 정렬된 메모리 할당 테스트 ===\n");

    et_windows_large_pages_init();

    // 다양한 정렬 크기 테스트
    size_t alignments[] = {4096, 8192, 16384, 65536, 1024*1024};
    int num_alignments = sizeof(alignments) / sizeof(alignments[0]);

    for (int i = 0; i < num_alignments; i++) {
        size_t alignment = alignments[i];
        size_t size = TEST_MEDIUM_SIZE;

        printf("Alignment test: %zu bytes alignment\n", alignment);

        void* memory = et_windows_alloc_aligned_large_pages(size, alignment);
        TEST_ASSERT(memory != NULL, "정렬된 메모리 할당 성공");

        // 정렬 확인
        uintptr_t addr = (uintptr_t)memory;
        bool is_aligned = (addr % alignment) == 0;
        TEST_ASSERT(is_aligned, "메모리 주소 정렬 확인");

        printf("  Allocated address: 0x%p (alignment: %s)\n",
               memory, is_aligned ? "OK" : "FAIL");

        // 메모리 사용 테스트
        memset(memory, 0x77, size);

        et_windows_free_large_pages(memory, size);
    }

    // 잘못된 정렬 크기 테스트
    void* invalid_memory = et_windows_alloc_aligned_large_pages(TEST_SMALL_SIZE, 3); // 2의 거듭제곱이 아님
    TEST_ASSERT(invalid_memory == NULL, "잘못된 정렬 크기 처리");

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 통계 및 상태 추적 테스트
 */
static int test_statistics_tracking(void) {
    printf("\n=== 통계 및 상태 추적 테스트 ===\n");

    et_windows_large_pages_init();

    // 통계 초기화
    et_windows_large_pages_reset_stats();

    ETLargePageInfo initial_info;
    et_windows_large_pages_get_info(&initial_info);
    TEST_ASSERT(initial_info.allocation_count == 0, "초기 할당 횟수 0");
    TEST_ASSERT(initial_info.total_allocated == 0, "초기 할당량 0");

    // 여러 번 할당
    const int num_allocations = 5;
    void* allocations[5];
    size_t sizes[5] = {TEST_SMALL_SIZE, TEST_MEDIUM_SIZE, TEST_LARGE_SIZE,
                       TEST_HUGE_SIZE, TEST_MEDIUM_SIZE};

    for (int i = 0; i < num_allocations; i++) {
        allocations[i] = et_windows_alloc_large_pages(sizes[i]);
        TEST_ASSERT(allocations[i] != NULL, "할당 성공");
    }

    // 통계 확인
    ETLargePageInfo after_alloc_info;
    et_windows_large_pages_get_info(&after_alloc_info);

    printf("Statistics after allocation:\n");
    printf("  Total allocations: %ld\n", after_alloc_info.allocation_count);
    printf("  Large Page allocated: %zu bytes\n", after_alloc_info.total_allocated);
    printf("  Fallback allocated: %zu bytes\n", after_alloc_info.fallback_allocated);
    printf("  Fallback count: %ld\n", after_alloc_info.fallback_count);

    TEST_ASSERT(after_alloc_info.allocation_count == num_allocations, "할당 횟수 추적");

    // 메모리 해제
    for (int i = 0; i < num_allocations; i++) {
        et_windows_free_large_pages(allocations[i], sizes[i]);
    }

    // 상태 문자열 출력
    char status_buffer[2048];
    et_windows_large_pages_status_to_string(status_buffer, sizeof(status_buffer));
    printf("Final status:\n%s\n", status_buffer);

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 성능 벤치마크 테스트
 */
static int test_performance_benchmark(void) {
    printf("\n=== 성능 벤치마크 테스트 ===\n");

    et_windows_large_pages_init();

    const int num_iterations = 100;
    const size_t test_size = TEST_LARGE_SIZE;

    // Large Page 할당 성능 측정
    DWORD start_time = GetTickCount();

    for (int i = 0; i < num_iterations; i++) {
        void* memory = et_windows_alloc_large_pages(test_size);
        if (memory) {
            // 간단한 메모리 접근
            volatile char* ptr = (volatile char*)memory;
            *ptr = 0x42;

            et_windows_free_large_pages(memory, test_size);
        }
    }

    DWORD end_time = GetTickCount();
    DWORD large_page_time = end_time - start_time;

    // 일반 메모리 할당 성능 측정 (비교용)
    start_time = GetTickCount();

    for (int i = 0; i < num_iterations; i++) {
        void* memory = VirtualAlloc(NULL, test_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (memory) {
            // 간단한 메모리 접근
            volatile char* ptr = (volatile char*)memory;
            *ptr = 0x42;

            VirtualFree(memory, 0, MEM_RELEASE);
        }
    }

    end_time = GetTickCount();
    DWORD regular_time = end_time - start_time;

    printf("Performance comparison (%d iterations, size %zu bytes):\n", num_iterations, test_size);
    printf("  Large Page allocation: %lu ms\n", large_page_time);
    printf("  Regular memory allocation: %lu ms\n", regular_time);

    if (large_page_time > 0 && regular_time > 0) {
        double ratio = (double)regular_time / large_page_time;
        printf("  Performance ratio: %.2fx\n", ratio);
    }

    // 통계 정보 출력
    ETLargePageInfo perf_info;
    et_windows_large_pages_get_info(&perf_info);
    printf("Benchmark statistics:\n");
    printf("  Total allocations: %ld\n", perf_info.allocation_count);
    printf("  Fallback count: %ld\n", perf_info.fallback_count);
    if (perf_info.allocation_count > 0) {
        double success_rate = 100.0 * (perf_info.allocation_count - perf_info.fallback_count) / perf_info.allocation_count;
        printf("  Large Page success rate: %.1f%%\n", success_rate);
    }

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 오류 처리 테스트
 */
static int test_error_handling(void) {
    printf("\n=== 오류 처리 테스트 ===\n");

    // 초기화되지 않은 상태에서 상태 조회
    ETLargePageInfo info;
    ETResult result = et_windows_large_pages_get_info(&info);
    // 자동 초기화되므로 성공해야 함
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "자동 초기화 후 상태 조회");

    et_windows_large_pages_finalize();

    // NULL 포인터 처리
    result = et_windows_large_pages_get_info(NULL);
    TEST_ASSERT(result == LIBETUDE_ERROR_INVALID_ARGUMENT, "NULL 포인터 오류 처리");

    result = et_windows_large_pages_status_to_string(NULL, 100);
    TEST_ASSERT(result == LIBETUDE_ERROR_INVALID_ARGUMENT, "NULL 버퍼 오류 처리");

    // 0 크기 할당
    et_windows_large_pages_init();
    void* zero_memory = et_windows_alloc_large_pages(0);
    TEST_ASSERT(zero_memory == NULL, "0 크기 할당 처리");

    // NULL 메모리 해제 (크래시하지 않아야 함)
    et_windows_free_large_pages(NULL, 100);
    printf("PASS: NULL 메모리 해제 처리\n");

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 새로운 기능 테스트
 */
static int test_new_features(void) {
    printf("\n=== 새로운 기능 테스트 ===\n");

    et_windows_large_pages_init();

    // 메모리 통계 조회 테스트
    size_t total_memory, available_memory, large_page_total, large_page_free;
    ETResult result = et_windows_large_pages_get_memory_stats(&total_memory, &available_memory,
                                                             &large_page_total, &large_page_free);
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "메모리 통계 조회 성공");

    printf("System memory info:\n");
    printf("  Total memory: %.1f GB\n", (double)total_memory / (1024*1024*1024));
    printf("  Available: %.1f GB\n", (double)available_memory / (1024*1024*1024));
    printf("  Large Page total: %.1f MB\n", (double)large_page_total / (1024*1024));
    printf("  Large Page available: %.1f MB\n", (double)large_page_free / (1024*1024));

    // 활성 할당 추적 테스트
    void* test_allocs[3];
    test_allocs[0] = et_windows_alloc_large_pages(TEST_SMALL_SIZE);
    test_allocs[1] = et_windows_alloc_large_pages(TEST_MEDIUM_SIZE);
    test_allocs[2] = et_windows_alloc_large_pages(TEST_LARGE_SIZE);

    ETMemoryAllocation active_allocs[10];
    size_t active_count;
    result = et_windows_large_pages_get_active_allocations(active_allocs, 10, &active_count);
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "활성 할당 목록 조회 성공");

    printf("Active allocation count: %zu\n", active_count);
    for (size_t i = 0; i < active_count && i < 3; i++) {
        printf("  Allocation %zu: address=%p, size=%zu, Large Page=%s\n",
               i, active_allocs[i].address, active_allocs[i].size,
               active_allocs[i].is_large_page ? "Yes" : "No");
    }

    // 벤치마크 테스트
    double large_page_time, regular_time;
    result = et_windows_large_pages_benchmark(TEST_MEDIUM_SIZE, 10, &large_page_time, &regular_time);
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "성능 벤치마크 실행 성공");

    printf("Benchmark results (10 iterations, %d bytes):\n", TEST_MEDIUM_SIZE);
    printf("  Large Page: %.2f ms\n", large_page_time);
    printf("  Regular memory: %.2f ms\n", regular_time);
    if (regular_time > 0) {
        printf("  Performance ratio: %.2fx\n", regular_time / large_page_time);
    }

    // 메모리 해제
    for (int i = 0; i < 3; i++) {
        if (test_allocs[i]) {
            et_windows_free_large_pages(test_allocs[i],
                i == 0 ? TEST_SMALL_SIZE : (i == 1 ? TEST_MEDIUM_SIZE : TEST_LARGE_SIZE));
        }
    }

    et_windows_large_pages_finalize();
    return 1;
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("Windows Large Page Memory Test Started\n");
    printf("=====================================\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 각 테스트 실행
    total_tests++; if (test_large_page_lifecycle()) passed_tests++;
    total_tests++; if (test_privilege_activation()) passed_tests++;
    total_tests++; if (test_basic_allocation()) passed_tests++;
    total_tests++; if (test_memory_reallocation()) passed_tests++;
    total_tests++; if (test_aligned_allocation()) passed_tests++;
    total_tests++; if (test_statistics_tracking()) passed_tests++;
    total_tests++; if (test_performance_benchmark()) passed_tests++;
    total_tests++; if (test_error_handling()) passed_tests++;


    // 결과 출력
    printf("\n=====================================\n");
    printf("Test results: %d/%d passed\n", passed_tests, total_tests);

    if (passed_tests == total_tests) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}

#else
int main(void) {
    printf("This test runs only on Windows platform.\n");
    return 0;
}
#endif // _WIN32