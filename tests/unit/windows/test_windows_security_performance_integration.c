/**
 * @file test_windows_security_performance_integration.c
 * @brief Windows 보안 및 성능 기능 통합 테스트
 * @author LibEtude Project
 * @version 1.0.0
 *
 * DEP/ASLR/UAC 호환성 테스트 및 SIMD/Thread Pool 성능 벤치마크
 * Large Page 메모리 할당 테스트 및 성능 측정
 * Requirements: 3.1, 3.2, 3.3, 6.1, 6.2, 6.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include "libetude/platform/windows_security.h"
#include "libetude/platform/windows_simd.h"
#include "libetude/platform/windows_threading.h"
#include "libetude/platform/windows_large_pages.h"
#include "libetude/error.h"

/* 테스트 결과 구조체 */
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
} TestResults;

static TestResults g_test_results = { 0 };

/* 성능 벤치마크 결과 */
typedef struct {
    double dep_aslr_overhead_ms;
    double simd_speedup_factor;
    double threading_efficiency;
    double large_page_improvement;
} PerformanceMetrics;

static PerformanceMetrics g_performance = { 0 };

/* 테스트 매크로 */
#define TEST_START(name) \
    do { \
        printf("테스트 시작: %s\n", name); \
        g_test_results.total_tests++; \
    } while(0)

#define TEST_PASS(name) \
    do { \
        printf("  ✓ %s 통과\n", name); \
        g_test_results.passed_tests++; \
    } while(0)

#define TEST_FAIL(name, reason) \
    do { \
        printf("  ✗ %s 실패: %s\n", name, reason); \
        g_test_results.failed_tests++; \
    } while(0)

#define TEST_SKIP(name, reason) \
    do { \
        printf("  ⚠ %s 건너뜀: %s\n", name, reason); \
        g_test_results.skipped_tests++; \
    } while(0)

/**
 * @brief DEP 및 ASLR 호환성 통합 테스트
 * Requirements: 3.1, 3.2
 */
static void test_dep_aslr_compatibility_integration(void) {
    TEST_START("DEP 및 ASLR 호환성 통합");

    /* DEP 호환성 확인 */
    bool dep_compatible = et_windows_check_dep_compatibility();
    if (dep_compatible) {
        TEST_PASS("DEP 호환성 확인");
    } else {
        TEST_FAIL("DEP 호환성 확인", "DEP가 지원되지 않거나 비활성화됨");
    }

    /* ASLR 호환성 확인 */
    bool aslr_compatible = et_windows_check_aslr_compatibility();
    if (aslr_compatible) {
        TEST_PASS("ASLR 호환성 확인");
    } else {
        TEST_FAIL("ASLR 호환성 확인", "ASLR이 지원되지 않음");
    }

    /* 통합 보안 상태 조회 */
    ETWindowsSecurityStatus security_status;
    bool status_result = et_windows_get_security_status(&security_status);

    if (status_result) {
        TEST_PASS("통합 보안 상태 조회");
        printf("    DEP 활성화: %s\n", security_status.dep_enabled ? "예" : "아니오");
        printf("    ASLR 지원: %s\n", security_status.aslr_enabled ? "예" : "아니오");
        printf("    Large Address Aware: %s\n", security_status.large_address_aware ? "예" : "아니오");
    } else {
        TEST_FAIL("통합 보안 상태 조회", "상태 정보를 가져올 수 없음");
    }

    /* ASLR 호환 메모리 할당 성능 테스트 */
    const int allocation_count = 1000;
    const size_t allocation_size = 4096;

    DWORD start_time = GetTickCount();

    void** allocations = (void**)malloc(allocation_count * sizeof(void*));
    if (allocations) {
        for (int i = 0; i < allocation_count; i++) {
            allocations[i] = et_windows_alloc_aslr_compatible(allocation_size);
            if (allocations[i]) {
                /* 메모리 사용 테스트 */
                memset(allocations[i], 0xAA, allocation_size);
            }
        }

        /* 메모리 해제 */
        for (int i = 0; i < allocation_count; i++) {
            if (allocations[i]) {
                et_windows_free_aslr_compatible(allocations[i]);
            }
        }

        free(allocations);

        DWORD end_time = GetTickCount();
        g_performance.dep_aslr_overhead_ms = (double)(end_time - start_time);

        TEST_PASS("ASLR 호환 메모리 할당 성능");
        printf("    %d회 할당/해제 시간: %.2f ms\n", allocation_count, g_performance.dep_aslr_overhead_ms);
    } else {
        TEST_FAIL("ASLR 호환 메모리 할당 성능", "메모리 할당 실패");
    }
}

/**
 * @brief UAC 권한 관리 통합 테스트
 * Requirements: 3.3
 */
static void test_uac_permission_management_integration(void) {
    TEST_START("UAC 권한 관리 통합");

    /* UAC 상태 조회 */
    ETUACStatus uac_status;
    bool uac_result = et_windows_get_uac_status(&uac_status);

    if (uac_result) {
        TEST_PASS("UAC 상태 조회");
        printf("    현재 레벨: %s\n",
               uac_status.current_level == ET_UAC_LEVEL_ELEVATED ? "관리자 권한" :
               uac_status.current_level == ET_UAC_LEVEL_USER ? "일반 사용자" : "알 수 없음");
        printf("    UAC 활성화: %s\n", uac_status.uac_enabled ? "예" : "아니오");
    } else {
        TEST_FAIL("UAC 상태 조회", "UAC 상태를 가져올 수 없음");
        return;
    }

    /* 기능 제한 모드 초기화 */
    ETRestrictedModeConfig config;
    et_windows_init_restricted_mode(&config, uac_status.current_level);

    TEST_PASS("기능 제한 모드 초기화");
    printf("    파일 작업: %s\n", config.allow_file_operations ? "허용" : "제한");
    printf("    레지스트리 접근: %s\n", config.allow_registry_access ? "허용" : "제한");
    printf("    네트워크 접근: %s\n", config.allow_network_access ? "허용" : "제한");
    printf("    하드웨어 접근: %s\n", config.allow_hardware_access ? "허용" : "제한");
    printf("    시스템 변경: %s\n", config.allow_system_changes ? "허용" : "제한");

    /* 권한별 기능 테스트 */
    const char* test_paths[] = {
        "C:\\Users\\TestUser\\Documents\\test.txt",
        "C:\\Windows\\System32\\test.dll",
        "C:\\Program Files\\TestApp\\test.exe"
    };

    for (int i = 0; i < 3; i++) {
        bool file_access = et_windows_check_file_access_permission(&config, test_paths[i]);
        printf("    파일 접근 (%s): %s\n",
               i == 0 ? "사용자 폴더" : i == 1 ? "시스템 폴더" : "Program Files",
               file_access ? "허용" : "제한");
    }

    /* 네트워크 및 하드웨어 접근 권한 확인 */
    bool network_access = et_windows_check_network_access_permission(&config);
    bool hardware_access = et_windows_check_hardware_access_permission(&config);

    if (network_access) {
        TEST_PASS("네트워크 접근 권한 확인");
    } else {
        printf("    ⚠ 네트워크 접근 제한됨\n");
    }

    if (uac_status.current_level == ET_UAC_LEVEL_ELEVATED && hardware_access) {
        TEST_PASS("하드웨어 접근 권한 확인 (관리자)");
    } else if (uac_status.current_level == ET_UAC_LEVEL_USER && !hardware_access) {
        TEST_PASS("하드웨어 접근 제한 확인 (일반 사용자)");
    } else {
        printf("    ⚠ 하드웨어 접근 권한 상태: %s\n", hardware_access ? "허용" : "제한");
    }
}

/**
 * @brief SIMD 최적화 성능 벤치마크
 * Requirements: 6.1
 */
static void test_simd_optimization_benchmark(void) {
    TEST_START("SIMD 최적화 성능 벤치마크");

    /* CPU 기능 감지 */
    ETWindowsCPUFeatures cpu_features = et_windows_detect_cpu_features();

    printf("    감지된 CPU 기능:\n");
    printf("      SSE4.1: %s\n", cpu_features.has_sse41 ? "지원" : "미지원");
    printf("      AVX: %s\n", cpu_features.has_avx ? "지원" : "미지원");
    printf("      AVX2: %s\n", cpu_features.has_avx2 ? "지원" : "미지원");
    printf("      AVX-512: %s\n", cpu_features.has_avx512 ? "지원" : "미지원");

    if (!cpu_features.has_avx2) {
        TEST_SKIP("SIMD 성능 벤치마크", "AVX2가 지원되지 않음");
        return;
    }

    /* 벡터 연산 성능 테스트 */
    const int vector_size = 10000;
    const int iterations = 1000;

    float* a = (float*)malloc(vector_size * sizeof(float));
    float* b = (float*)malloc(vector_size * sizeof(float));
    float* c_fallback = (float*)malloc(vector_size * sizeof(float));
    float* c_avx2 = (float*)malloc(vector_size * sizeof(float));

    if (!a || !b || !c_fallback || !c_avx2) {
        TEST_FAIL("SIMD 성능 벤치마크", "메모리 할당 실패");
        goto cleanup_simd;
    }

    /* 테스트 데이터 초기화 */
    for (int i = 0; i < vector_size; i++) {
        a[i] = (float)(i * 0.1);
        b[i] = (float)(i * 0.2);
    }

    /* 기본 구현 성능 측정 */
    DWORD start_time = GetTickCount();
    for (int i = 0; i < iterations; i++) {
        et_windows_simd_vector_add_fallback(a, b, c_fallback, vector_size);
    }
    DWORD fallback_time = GetTickCount() - start_time;

    /* AVX2 구현 성능 측정 */
    start_time = GetTickCount();
    for (int i = 0; i < iterations; i++) {
        et_windows_simd_vector_add_avx2(a, b, c_avx2, vector_size);
    }
    DWORD avx2_time = GetTickCount() - start_time;

    /* 결과 검증 */
    bool results_match = true;
    for (int i = 0; i < vector_size; i++) {
        if (fabs(c_fallback[i] - c_avx2[i]) > 1e-5f) {
            results_match = false;
            break;
        }
    }

    if (results_match) {
        TEST_PASS("SIMD 연산 결과 정확성");
    } else {
        TEST_FAIL("SIMD 연산 결과 정확성", "기본 구현과 AVX2 구현 결과 불일치");
    }

    /* 성능 분석 */
    if (fallback_time > 0 && avx2_time > 0) {
        g_performance.simd_speedup_factor = (double)fallback_time / avx2_time;
        TEST_PASS("SIMD 성능 벤치마크");
        printf("    기본 구현: %lu ms\n", fallback_time);
        printf("    AVX2 구현: %lu ms\n", avx2_time);
        printf("    성능 향상: %.2fx\n", g_performance.simd_speedup_factor);

        if (g_performance.simd_speedup_factor >= 1.5) {
            printf("    ✓ 유의미한 성능 향상 달성\n");
        } else {
            printf("    ⚠ 성능 향상이 기대치보다 낮음\n");
        }
    } else {
        TEST_FAIL("SIMD 성능 벤치마크", "성능 측정 실패");
    }

cleanup_simd:
    free(a);
    free(b);
    free(c_fallback);
    free(c_avx2);
}

/**
 * @brief Thread Pool 성능 벤치마크
 * Requirements: 6.2
 */
static void test_thread_pool_performance_benchmark(void) {
    TEST_START("Thread Pool 성능 벤치마크");

    /* Thread Pool 초기화 */
    ETWindowsThreadPool pool;
    ETResult result = et_windows_threadpool_init(&pool, 4, 8);

    if (result != ET_SUCCESS) {
        TEST_FAIL("Thread Pool 초기화", "초기화 실패");
        return;
    }

    TEST_PASS("Thread Pool 초기화");

    /* CPU 집약적 작업 정의 */
    static volatile LONG completed_tasks = 0;

    void cpu_intensive_task(void* context) {
        UNREFERENCED_PARAMETER(context);

        /* 간단한 계산 작업 */
        volatile double result = 0.0;
        for (int i = 0; i < 50000; i++) {
            result += sqrt((double)i);
        }

        InterlockedIncrement(&completed_tasks);
    }

    const int num_tasks = 100;
    InterlockedExchange(&completed_tasks, 0);

    /* Thread Pool 성능 측정 */
    DWORD start_time = GetTickCount();

    for (int i = 0; i < num_tasks; i++) {
        result = et_windows_threadpool_submit_async(cpu_intensive_task, NULL);
        if (result != ET_SUCCESS) {
            TEST_FAIL("작업 제출", "비동기 작업 제출 실패");
            goto cleanup_threadpool;
        }
    }

    /* 모든 작업 완료 대기 */
    result = et_windows_threadpool_wait_all(30000); /* 30초 타임아웃 */
    DWORD threadpool_time = GetTickCount() - start_time;

    if (result != ET_SUCCESS) {
        TEST_FAIL("Thread Pool 작업 완료", "작업 완료 대기 실패");
        goto cleanup_threadpool;
    }

    LONG final_completed = InterlockedCompareExchange(&completed_tasks, 0, 0);

    if (final_completed == num_tasks) {
        TEST_PASS("Thread Pool 작업 완료");
    } else {
        TEST_FAIL("Thread Pool 작업 완료", "일부 작업이 완료되지 않음");
    }

    /* 단일 스레드 성능과 비교 */
    InterlockedExchange(&completed_tasks, 0);
    start_time = GetTickCount();

    for (int i = 0; i < num_tasks; i++) {
        cpu_intensive_task(NULL);
    }

    DWORD single_thread_time = GetTickCount() - start_time;

    /* 성능 분석 */
    if (threadpool_time > 0 && single_thread_time > 0) {
        g_performance.threading_efficiency = (double)single_thread_time / threadpool_time;
        TEST_PASS("Thread Pool 성능 분석");
        printf("    Thread Pool: %lu ms\n", threadpool_time);
        printf("    단일 스레드: %lu ms\n", single_thread_time);
        printf("    효율성: %.2fx\n", g_performance.threading_efficiency);

        if (g_performance.threading_efficiency >= 2.0) {
            printf("    ✓ 우수한 멀티스레딩 성능\n");
        } else if (g_performance.threading_efficiency >= 1.5) {
            printf("    ✓ 양호한 멀티스레딩 성능\n");
        } else {
            printf("    ⚠ 멀티스레딩 효율성이 낮음\n");
        }
    } else {
        TEST_FAIL("Thread Pool 성능 분석", "성능 측정 실패");
    }

cleanup_threadpool:
    et_windows_threadpool_finalize();
}

/**
 * @brief Large Page 메모리 성능 테스트
 * Requirements: 6.3
 */
static void test_large_page_memory_performance(void) {
    TEST_START("Large Page 메모리 성능");

    /* Large Page 초기화 */
    ETResult result = et_windows_large_pages_init();
    if (result != ET_SUCCESS) {
        TEST_FAIL("Large Page 초기화", "초기화 실패");
        return;
    }

    /* Large Page 권한 활성화 */
    bool privilege_enabled = et_windows_enable_large_page_privilege();
    if (privilege_enabled) {
        TEST_PASS("Large Page 권한 활성화");
    } else {
        TEST_SKIP("Large Page 성능 테스트", "권한 활성화 실패 (관리자 권한 필요)");
        et_windows_large_pages_finalize();
        return;
    }

    /* Large Page 정보 조회 */
    ETLargePageInfo info;
    result = et_windows_large_pages_get_info(&info);

    if (result == ET_SUCCESS && info.is_supported) {
        TEST_PASS("Large Page 지원 확인");
        printf("    Large Page 크기: %.1f MB\n", (double)info.large_page_size / (1024 * 1024));
        printf("    권한 활성화: %s\n", info.privilege_enabled ? "예" : "아니오");
    } else {
        TEST_SKIP("Large Page 성능 테스트", "Large Page가 지원되지 않음");
        et_windows_large_pages_finalize();
        return;
    }

    /* 성능 비교 테스트 */
    const size_t test_size = 8 * 1024 * 1024; /* 8MB */
    const int iterations = 100;

    /* Large Page 할당 성능 */
    DWORD start_time = GetTickCount();

    for (int i = 0; i < iterations; i++) {
        void* large_memory = et_windows_alloc_large_pages(test_size);
        if (large_memory) {
            /* 메모리 접근 테스트 */
            volatile char* ptr = (volatile char*)large_memory;
            for (size_t j = 0; j < test_size; j += 4096) {
                ptr[j] = 0x42;
            }

            et_windows_free_large_pages(large_memory, test_size);
        }
    }

    DWORD large_page_time = GetTickCount() - start_time;

    /* 일반 메모리 할당 성능 */
    start_time = GetTickCount();

    for (int i = 0; i < iterations; i++) {
        void* regular_memory = VirtualAlloc(NULL, test_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (regular_memory) {
            /* 메모리 접근 테스트 */
            volatile char* ptr = (volatile char*)regular_memory;
            for (size_t j = 0; j < test_size; j += 4096) {
                ptr[j] = 0x42;
            }

            VirtualFree(regular_memory, 0, MEM_RELEASE);
        }
    }

    DWORD regular_time = GetTickCount() - start_time;

    /* 성능 분석 */
    if (large_page_time > 0 && regular_time > 0) {
        g_performance.large_page_improvement = (double)regular_time / large_page_time;
        TEST_PASS("Large Page 성능 분석");
        printf("    Large Page: %lu ms\n", large_page_time);
        printf("    일반 메모리: %lu ms\n", regular_time);
        printf("    성능 향상: %.2fx\n", g_performance.large_page_improvement);

        if (g_performance.large_page_improvement >= 1.2) {
            printf("    ✓ Large Page 성능 향상 확인\n");
        } else {
            printf("    ⚠ Large Page 성능 향상이 미미함\n");
        }
    } else {
        TEST_FAIL("Large Page 성능 분석", "성능 측정 실패");
    }

    /* 통계 정보 출력 */
    result = et_windows_large_pages_get_info(&info);
    if (result == ET_SUCCESS) {
        printf("    할당 통계:\n");
        printf("      총 할당 횟수: %ld\n", info.allocation_count);
        printf("      Large Page 할당량: %.1f MB\n", (double)info.total_allocated / (1024 * 1024));
        printf("      폴백 할당 횟수: %ld\n", info.fallback_count);

        if (info.allocation_count > 0) {
            double success_rate = 100.0 * (info.allocation_count - info.fallback_count) / info.allocation_count;
            printf("      Large Page 성공률: %.1f%%\n", success_rate);
        }
    }

    et_windows_large_pages_finalize();
}

/**
 * @brief 통합 성능 보고서 생성
 */
static void generate_performance_report(void) {
    printf("\n=== 통합 성능 보고서 ===\n");

    printf("보안 기능 성능:\n");
    printf("  DEP/ASLR 오버헤드: %.2f ms (1000회 할당)\n", g_performance.dep_aslr_overhead_ms);

    printf("\n최적화 기능 성능:\n");
    printf("  SIMD 성능 향상: %.2fx\n", g_performance.simd_speedup_factor);
    printf("  Thread Pool 효율성: %.2fx\n", g_performance.threading_efficiency);
    printf("  Large Page 성능 향상: %.2fx\n", g_performance.large_page_improvement);

    /* 종합 성능 점수 계산 */
    double overall_score = 0.0;
    int score_components = 0;

    if (g_performance.simd_speedup_factor > 0) {
        overall_score += g_performance.simd_speedup_factor;
        score_components++;
    }

    if (g_performance.threading_efficiency > 0) {
        overall_score += g_performance.threading_efficiency;
        score_components++;
    }

    if (g_performance.large_page_improvement > 0) {
        overall_score += g_performance.large_page_improvement;
        score_components++;
    }

    if (score_components > 0) {
        overall_score /= score_components;
        printf("\n종합 성능 점수: %.2fx\n", overall_score);

        if (overall_score >= 2.0) {
            printf("✓ 우수한 성능 최적화 달성\n");
        } else if (overall_score >= 1.5) {
            printf("✓ 양호한 성능 최적화 달성\n");
        } else {
            printf("⚠ 성능 최적화 개선 필요\n");
        }
    }
}

/**
 * @brief 테스트 결과 요약 출력
 */
static void print_test_summary(void) {
    printf("\n=== 테스트 결과 요약 ===\n");
    printf("총 테스트: %d\n", g_test_results.total_tests);
    printf("통과: %d\n", g_test_results.passed_tests);
    printf("실패: %d\n", g_test_results.failed_tests);
    printf("건너뜀: %d\n", g_test_results.skipped_tests);

    double success_rate = 0.0;
    if (g_test_results.total_tests > 0) {
        success_rate = (double)g_test_results.passed_tests / g_test_results.total_tests * 100.0;
    }

    printf("성공률: %.1f%%\n", success_rate);

    if (g_test_results.failed_tests == 0) {
        printf("✓ 모든 테스트 통과!\n");
    } else {
        printf("✗ %d개 테스트 실패\n", g_test_results.failed_tests);
    }
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== Windows 보안 및 성능 기능 통합 테스트 ===\n\n");

    /* Windows 플랫폼 초기화 */
    printf("Windows 플랫폼 초기화 중...\n");

    /* 개별 테스트 실행 */
    test_dep_aslr_compatibility_integration();
    printf("\n");

    test_uac_permission_management_integration();
    printf("\n");

    test_simd_optimization_benchmark();
    printf("\n");

    test_thread_pool_performance_benchmark();
    printf("\n");

    test_large_page_memory_performance();
    printf("\n");

    /* 성능 보고서 생성 */
    generate_performance_report();

    /* 테스트 결과 요약 */
    print_test_summary();

    return (g_test_results.failed_tests == 0) ? 0 : 1;
}

#else /* !_WIN32 */

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif /* _WIN32 */