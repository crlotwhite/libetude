/**
 * @file test_platform_abstraction.c
 * @brief 플랫폼 추상화 레이어 단위 테스트 구현
 *
 * 플랫폼 추상화 레이어의 모든 인터페이스에 대한 단위 테스트를 구현합니다.
 */

#include "test_platform_abstraction.h"
#include <time.h>

// 전역 테스트 컨텍스트
TestContext g_test_context = {0};

/**
 * @brief 테스트 스위트 실행
 * @param suite 실행할 테스트 스위트
 * @return 실행 결과
 */
ETResult run_test_suite(TestSuite* suite) {
    if (!suite || !suite->tests) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    printf("Running test suite: %s\n", suite->name);
    printf("=====================================\n");

    // 스위트 설정
    if (suite->setup) {
        ETResult setup_result = suite->setup();
        if (setup_result != ET_SUCCESS) {
            printf("Setup failed for suite %s\n", suite->name);
            return setup_result;
        }
    }

    clock_t suite_start = clock();

    // 각 테스트 실행
    for (int i = 0; i < suite->test_count; i++) {
        TestCase* test = &suite->tests[i];
        printf("  Running: %s... ", test->name);
        fflush(stdout);

        // 메모리 추적 초기화
        reset_memory_tracking();

        clock_t test_start = clock();
        ETResult result = test->test_func();
        clock_t test_end = clock();

        double test_time = ((double)(test_end - test_start)) / CLOCKS_PER_SEC;

        g_test_context.stats.total_tests++;

        if (result == ET_SUCCESS) {
            printf("PASS (%.3fs)\n", test_time);
            g_test_context.stats.passed_tests++;
        } else {
            printf("FAIL (%.3fs)\n", test_time);
            g_test_context.stats.failed_tests++;
            if (g_test_context.verbose) {
                printf("    Description: %s\n", test->description);
            }
        }

        // 메모리 누수 검사
        check_memory_leaks();
    }

    clock_t suite_end = clock();
    double suite_time = ((double)(suite_end - suite_start)) / CLOCKS_PER_SEC;
    g_test_context.stats.total_time += suite_time;

    // 스위트 정리
    if (suite->teardown) {
        suite->teardown();
    }

    printf("Suite completed in %.3fs\n\n", suite_time);
    return ET_SUCCESS;
}

/**
 * @brief 모든 플랫폼 테스트 실행
 * @return 실행 결과
 */
ETResult run_all_platform_tests(void) {
    printf("Starting Platform Abstraction Layer Tests\n");
    printf("==========================================\n\n");

    // 테스트 컨텍스트 초기화
    memset(&g_test_context, 0, sizeof(TestContext));
    g_test_context.verbose = true;

    // 인터페이스 계약 테스트
    TestCase interface_tests[] = {
        {"Audio Interface Contract", test_audio_interface_contract, "오디오 인터페이스 계약 검증"},
        {"System Interface Contract", test_system_interface_contract, "시스템 인터페이스 계약 검증"},
        {"Threading Interface Contract", test_threading_interface_contract, "스레딩 인터페이스 계약 검증"},
        {"Memory Interface Contract", test_memory_interface_contract, "메모리 인터페이스 계약 검증"},
        {"Filesystem Interface Contract", test_filesystem_interface_contract, "파일시스템 인터페이스 계약 검증"},
        {"Network Interface Contract", test_network_interface_contract, "네트워크 인터페이스 계약 검증"},
        {"Dynamic Library Interface Contract", test_dynlib_interface_contract, "동적 라이브러리 인터페이스 계약 검증"}
    };

    TestSuite interface_suite = {
        .name = "Interface Contract Tests",
        .tests = interface_tests,
        .test_count = sizeof(interface_tests) / sizeof(TestCase),
        .setup = NULL,
        .teardown = NULL
    };

    ETResult result = run_test_suite(&interface_suite);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 오류 조건 테스트
    TestCase error_tests[] = {
        {"Error Conditions", test_error_conditions, "오류 조건 처리 테스트"},
        {"Boundary Values", test_boundary_values, "경계값 테스트"},
        {"Resource Cleanup", test_resource_cleanup, "리소스 정리 테스트"}
    };

    TestSuite error_suite = {
        .name = "Error Condition Tests",
        .tests = error_tests,
        .test_count = sizeof(error_tests) / sizeof(TestCase),
        .setup = NULL,
        .teardown = NULL
    };

    result = run_test_suite(&error_suite);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 플랫폼별 구현 테스트
#ifdef LIBETUDE_PLATFORM_WINDOWS
    TestCase windows_tests[] = {
        {"Windows Implementations", test_windows_implementations, "Windows 플랫폼 구현 테스트"}
    };

    TestSuite windows_suite = {
        .name = "Windows Platform Tests",
        .tests = windows_tests,
        .test_count = sizeof(windows_tests) / sizeof(TestCase),
        .setup = NULL,
        .teardown = NULL
    };

    result = run_test_suite(&windows_suite);
    if (result != ET_SUCCESS) {
        return result;
    }
#endif

#ifdef LIBETUDE_PLATFORM_LINUX
    TestCase linux_tests[] = {
        {"Linux Implementations", test_linux_implementations, "Linux 플랫폼 구현 테스트"}
    };

    TestSuite linux_suite = {
        .name = "Linux Platform Tests",
        .tests = linux_tests,
        .test_count = sizeof(linux_tests) / sizeof(TestCase),
        .setup = NULL,
        .teardown = NULL
    };

    result = run_test_suite(&linux_suite);
    if (result != ET_SUCCESS) {
        return result;
    }
#endif

#ifdef LIBETUDE_PLATFORM_MACOS
    TestCase macos_tests[] = {
        {"macOS Implementations", test_macos_implementations, "macOS 플랫폼 구현 테스트"}
    };

    TestSuite macos_suite = {
        .name = "macOS Platform Tests",
        .tests = macos_tests,
        .test_count = sizeof(macos_tests) / sizeof(TestCase),
        .setup = NULL,
        .teardown = NULL
    };

    result = run_test_suite(&macos_suite);
    if (result != ET_SUCCESS) {
        return result;
    }
#endif

    // 결과 출력
    print_test_results(&g_test_context.stats);

    return (g_test_context.stats.failed_tests == 0) ? ET_SUCCESS : ET_ERROR_TEST_FAILED;
}

/**
 * @brief 테스트 결과 출력
 * @param stats 테스트 통계
 */
void print_test_results(const TestStats* stats) {
    printf("Test Results Summary\n");
    printf("====================\n");
    printf("Total Tests: %d\n", stats->total_tests);
    printf("Passed: %d\n", stats->passed_tests);
    printf("Failed: %d\n", stats->failed_tests);
    printf("Skipped: %d\n", stats->skipped_tests);
    printf("Total Time: %.3fs\n", stats->total_time);

    if (stats->failed_tests == 0) {
        printf("\nAll tests PASSED! ✓\n");
    } else {
        printf("\n%d tests FAILED! ✗\n", stats->failed_tests);
    }
}

/**
 * @brief 메모리 할당 (추적 포함)
 * @param size 할당할 크기
 * @param file 파일명
 * @param line 라인 번호
 * @return 할당된 메모리 포인터
 */
void* test_malloc(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (ptr) {
        MemoryAllocation* alloc = malloc(sizeof(MemoryAllocation));
        if (alloc) {
            alloc->ptr = ptr;
            alloc->size = size;
            alloc->file = file;
            alloc->line = line;
            alloc->next = g_test_context.allocations;
            g_test_context.allocations = alloc;
        }
    }
    return ptr;
}

/**
 * @brief 메모리 해제 (추적 포함)
 * @param ptr 해제할 포인터
 * @param file 파일명
 * @param line 라인 번호
 */
void test_free(void* ptr, const char* file, int line) {
    if (!ptr) return;

    MemoryAllocation** current = &g_test_context.allocations;
    while (*current) {
        if ((*current)->ptr == ptr) {
            MemoryAllocation* to_remove = *current;
            *current = (*current)->next;
            free(to_remove);
            free(ptr);
            return;
        }
        current = &(*current)->next;
    }

    // 추적되지 않은 메모리 해제 (경고)
    printf("Warning: Freeing untracked memory at %s:%d\n", file, line);
    free(ptr);
}

/**
 * @brief 메모리 누수 검사
 */
void check_memory_leaks(void) {
    int leak_count = 0;
    MemoryAllocation* current = g_test_context.allocations;

    while (current) {
        printf("Memory leak detected: %zu bytes at %s:%d\n",
               current->size, current->file, current->line);
        leak_count++;
        current = current->next;
    }

    g_test_context.memory_leak_count += leak_count;

    if (leak_count > 0) {
        printf("Total memory leaks in this test: %d\n", leak_count);
    }
}

/**
 * @brief 메모리 추적 초기화
 */
void reset_memory_tracking(void) {
    // 기존 할당 정보 정리
    MemoryAllocation* current = g_test_context.allocations;
    while (current) {
        MemoryAllocation* next = current->next;
        free(current);
        current = next;
    }
    g_test_context.allocations = NULL;
}

// 메인 테스트 실행 함수
int main(int argc, char* argv[]) {
    // 명령행 인수 처리
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            g_test_context.verbose = true;
        }
    }

    ETResult result = run_all_platform_tests();

    // 전체 메모리 누수 검사
    if (g_test_context.memory_leak_count > 0) {
        printf("\nTotal memory leaks detected: %d\n", g_test_context.memory_leak_count);
        return 1;
    }

    return (result == ET_SUCCESS) ? 0 : 1;
}