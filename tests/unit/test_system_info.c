/**
 * @file test_system_info.c
 * @brief 시스템 정보 추상화 레이어 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/system.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// 간단한 테스트 프레임워크
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s:%d - %s\n", __FILE__, __LINE__, #condition); \
            return -1; \
        } \
    } while(0)

#define TEST_SUCCESS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return 0; \
    } while(0)

// ============================================================================
// 테스트 함수들
// ============================================================================

int test_system_info_basic(void) {
    printf("Testing basic system info...\n");

    ETSystemInfo info;
    ETResult result = et_get_system_info(&info);

    TEST_ASSERT(result == ET_SUCCESS);
    TEST_ASSERT(info.total_memory > 0);
    TEST_ASSERT(info.cpu_count > 0);
    TEST_ASSERT(info.platform_type != ET_PLATFORM_UNKNOWN);
    TEST_ASSERT(strlen(info.system_name) > 0);

    printf("  System: %s\n", info.system_name);
    printf("  OS Version: %s\n", info.os_version);
    printf("  CPU: %s\n", info.cpu_name);
    printf("  CPU Count: %u\n", info.cpu_count);
    printf("  Total Memory: %.2f GB\n", (double)info.total_memory / (1024.0 * 1024.0 * 1024.0));
    printf("  Platform: %d\n", info.platform_type);
    printf("  Architecture: %d\n", info.architecture);

    TEST_SUCCESS();
}

int test_memory_info(void) {
    printf("Testing memory info...\n");

    ETMemoryInfo info;
    ETResult result = et_get_memory_info(&info);

    TEST_ASSERT(result == ET_SUCCESS);
    TEST_ASSERT(info.total_physical > 0);
    TEST_ASSERT(info.page_size > 0);
    TEST_ASSERT(info.allocation_granularity > 0);

    printf("  Total Physical: %.2f GB\n", (double)info.total_physical / (1024.0 * 1024.0 * 1024.0));
    printf("  Available Physical: %.2f GB\n", (double)info.available_physical / (1024.0 * 1024.0 * 1024.0));
    printf("  Page Size: %u bytes\n", info.page_size);
    printf("  Allocation Granularity: %u bytes\n", info.allocation_granularity);

    TEST_SUCCESS();
}

int test_cpu_info(void) {
    printf("Testing CPU info...\n");

    ETCPUInfo info;
    ETResult result = et_get_cpu_info(&info);

    TEST_ASSERT(result == ET_SUCCESS);
    TEST_ASSERT(info.logical_cores > 0);
    TEST_ASSERT(info.physical_cores > 0);
    TEST_ASSERT(info.cache_line_size > 0);

    printf("  Vendor: %s\n", info.vendor);
    printf("  Brand: %s\n", info.brand);
    printf("  Family: %u, Model: %u, Stepping: %u\n", info.family, info.model, info.stepping);
    printf("  Physical Cores: %u, Logical Cores: %u\n", info.physical_cores, info.logical_cores);
    printf("  Cache Line Size: %u bytes\n", info.cache_line_size);
    printf("  L1: %u KB, L2: %u KB, L3: %u KB\n",
           info.l1_cache_size, info.l2_cache_size, info.l3_cache_size);
    printf("  Base Freq: %u MHz, Max Freq: %u MHz\n",
           info.base_frequency_mhz, info.max_frequency_mhz);

    TEST_SUCCESS();
}

int test_high_resolution_timer(void) {
    printf("Testing high resolution timer...\n");

    uint64_t time1, time2;
    ETResult result1 = et_get_high_resolution_time(&time1);
    ETResult result2 = et_get_high_resolution_time(&time2);

    TEST_ASSERT(result1 == ET_SUCCESS);
    TEST_ASSERT(result2 == ET_SUCCESS);
    TEST_ASSERT(time2 >= time1); // 시간은 단조증가해야 함

    printf("  Timer resolution test passed\n");
    printf("  Time1: %llu ns\n", (unsigned long long)time1);
    printf("  Time2: %llu ns\n", (unsigned long long)time2);
    printf("  Difference: %llu ns\n", (unsigned long long)(time2 - time1));

    TEST_SUCCESS();
}

int test_simd_features(void) {
    printf("Testing SIMD features...\n");

    uint32_t features = et_get_simd_features();

    printf("  SIMD Features: 0x%08X\n", features);

    char feature_str[256];
    ETResult result = et_simd_features_to_string(features, feature_str, sizeof(feature_str));
    TEST_ASSERT(result == ET_SUCCESS);

    printf("  Supported SIMD: %s\n", feature_str);

    // 기본적인 하드웨어 기능 확인
    bool has_simd = et_has_hardware_feature(ET_HW_FEATURE_SIMD);
    bool has_timer = et_has_hardware_feature(ET_HW_FEATURE_HIGH_RES_TIMER);

    printf("  Has SIMD: %s\n", has_simd ? "Yes" : "No");
    printf("  Has High-Res Timer: %s\n", has_timer ? "Yes" : "No");

    TEST_SUCCESS();
}

int test_memory_usage(void) {
    printf("Testing memory usage...\n");

    ETMemoryUsage usage;
    ETResult result = et_get_memory_usage(&usage);

    TEST_ASSERT(result == ET_SUCCESS);
    TEST_ASSERT(usage.process_memory_usage > 0);

    printf("  Process Memory: %.2f MB\n", (double)usage.process_memory_usage / (1024.0 * 1024.0));
    printf("  Process Peak Memory: %.2f MB\n", (double)usage.process_peak_memory / (1024.0 * 1024.0));
    printf("  CPU Usage: %.1f%%\n", usage.cpu_usage_percent);
    printf("  Memory Usage: %.1f%%\n", usage.memory_usage_percent);

    TEST_SUCCESS();
}

int test_sleep_function(void) {
    printf("Testing sleep function...\n");

    uint64_t start_time, end_time;
    et_get_high_resolution_time(&start_time);

    ETResult result = et_sleep(100); // 100ms 대기

    et_get_high_resolution_time(&end_time);

    TEST_ASSERT(result == ET_SUCCESS);

    uint64_t elapsed_ms = (end_time - start_time) / 1000000; // ns를 ms로 변환
    printf("  Requested sleep: 100ms, Actual: %llu ms\n", (unsigned long long)elapsed_ms);

    // 대략적인 시간 확인 (90ms ~ 200ms 범위에서 허용)
    TEST_ASSERT(elapsed_ms >= 90 && elapsed_ms <= 200);

    TEST_SUCCESS();
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main(void) {
    printf("=== LibEtude System Info Tests ===\n\n");

    int failed_tests = 0;

    // 테스트 실행
    if (test_system_info_basic() != 0) failed_tests++;
    if (test_memory_info() != 0) failed_tests++;
    if (test_cpu_info() != 0) failed_tests++;
    if (test_high_resolution_timer() != 0) failed_tests++;
    if (test_simd_features() != 0) failed_tests++;
    if (test_memory_usage() != 0) failed_tests++;
    if (test_sleep_function() != 0) failed_tests++;

    printf("\n=== Test Results ===\n");
    if (failed_tests == 0) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("%d test(s) failed!\n", failed_tests);
        return 1;
    }
}