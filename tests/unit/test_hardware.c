/**
 * @file test_hardware.c
 * @brief 하드웨어 감지 모듈 단위 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/hardware.h"
#include <string.h>

void setUp(void) {
    // 테스트 전 초기화
}

void tearDown(void) {
    // 테스트 후 정리
}

void test_detect_cpu(void) {
    LibEtudeHardwareCPUInfo cpu_info;
    LibEtudeErrorCode result = libetude_hardware_detect_cpu(&cpu_info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, cpu_info.logical_cores);
    TEST_ASSERT_NOT_EQUAL(0, cpu_info.physical_cores);
    TEST_ASSERT_NOT_EQUAL(0, cpu_info.cache_line_size);
    TEST_ASSERT_NOT_EQUAL(0, strlen(cpu_info.vendor));

    printf("CPU 정보:\n");
    printf("  제조사: %s\n", cpu_info.vendor);
    printf("  브랜드: %s\n", cpu_info.brand);
    printf("  물리 코어: %u개\n", cpu_info.physical_cores);
    printf("  논리 코어: %u개\n", cpu_info.logical_cores);
    printf("  캐시 라인 크기: %u bytes\n", cpu_info.cache_line_size);
}

void test_detect_simd_features(void) {
    uint32_t features = libetude_hardware_detect_simd_features();

    // SIMD 기능 문자열 변환 테스트
    char feature_string[256];
    LibEtudeErrorCode result = libetude_hardware_simd_features_to_string(
        features, feature_string, sizeof(feature_string));

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, strlen(feature_string));

    printf("SIMD 기능: %s (0x%08X)\n", feature_string, features);

    // 플랫폼별 SIMD 기능 확인
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // x86/x64 플랫폼에서는 최소한 SSE는 지원해야 함
    TEST_ASSERT_TRUE((features & LIBETUDE_SIMD_SSE) != 0 || (features & LIBETUDE_SIMD_SSE2) != 0);
#elif defined(__ARM_NEON) || defined(__aarch64__)
    // ARM 플랫폼에서는 NEON 지원 확인
    TEST_ASSERT_TRUE((features & LIBETUDE_SIMD_NEON) != 0);
#endif
}

void test_detect_memory(void) {
    LibEtudeHardwareMemoryInfo memory_info;
    LibEtudeErrorCode result = libetude_hardware_detect_memory(&memory_info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, memory_info.total_physical);
    TEST_ASSERT_NOT_EQUAL(0, memory_info.page_size);
    TEST_ASSERT_LESS_OR_EQUAL(memory_info.available_physical, memory_info.total_physical);
    TEST_ASSERT_NOT_EQUAL(0, memory_info.recommended_pool_size);

    printf("메모리 정보:\n");
    printf("  총 물리 메모리: %.2f GB\n",
           (double)memory_info.total_physical / (1024.0 * 1024.0 * 1024.0));
    printf("  사용 가능한 물리 메모리: %.2f GB\n",
           (double)memory_info.available_physical / (1024.0 * 1024.0 * 1024.0));
    printf("  페이지 크기: %u bytes\n", memory_info.page_size);

    // 권장 메모리 풀 크기가 적절한 범위 내에 있는지 확인
    TEST_ASSERT_GREATER_OR_EQUAL(64 * 1024 * 1024, memory_info.recommended_pool_size); // 최소 64MB
    TEST_ASSERT_LESS_OR_EQUAL(2ULL * 1024 * 1024 * 1024, memory_info.recommended_pool_size); // 최대 2GB
}

void test_detect_gpu(void) {
    LibEtudeHardwareGPUInfo gpu_info;
    LibEtudeErrorCode result = libetude_hardware_detect_gpu(&gpu_info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, strlen(gpu_info.name));

    printf("GPU 정보:\n");
    printf("  이름: %s\n", gpu_info.name);
    printf("  제조사: %s\n", gpu_info.vendor);
    printf("  사용 가능: %s\n", gpu_info.available ? "예" : "아니오");
}

void test_detect_full_hardware(void) {
    LibEtudeHardwareInfo info;
    LibEtudeErrorCode result = libetude_hardware_detect(&info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_TRUE(info.initialized);
    TEST_ASSERT_NOT_EQUAL(0, strlen(info.platform_name));
    TEST_ASSERT_GREATER_OR_EQUAL(1, info.performance_tier);
    TEST_ASSERT_LESS_OR_EQUAL(5, info.performance_tier);

    printf("\n=== 전체 하드웨어 정보 ===\n");
    libetude_hardware_print_info(&info);
}

void test_error_handling(void) {
    // NULL 포인터 테스트
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT, libetude_hardware_detect_cpu(NULL));
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT, libetude_hardware_detect_gpu(NULL));
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT, libetude_hardware_detect_memory(NULL));
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT, libetude_hardware_detect(NULL));

    // SIMD 기능 문자열 변환 오류 테스트
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_simd_features_to_string(0, NULL, 100));

    char dummy_buffer;
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_simd_features_to_string(0, &dummy_buffer, 0));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_detect_cpu);
    RUN_TEST(test_detect_simd_features);
    RUN_TEST(test_detect_memory);
    RUN_TEST(test_detect_gpu);
    RUN_TEST(test_detect_full_hardware);
    RUN_TEST(test_error_handling);

    return UNITY_END();
}