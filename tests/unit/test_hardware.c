/**
 * @file test_hardware.c
 * @brief 하드웨어 감지 모듈 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "tests/framework/test_framework.h"
#include "libetude/hardware.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// 테스트 함수들
// ============================================================================

void test_hardware_detect_cpu(void) {
    LibEtudeHardwareCPUInfo cpu_info;
    LibEtudeErrorCode result = libetude_hardware_detect_cpu(&cpu_info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, cpu_info.logical_cores);
    TEST_ASSERT_NOT_EQUAL(0, cpu_info.physical_cores);
    TEST_ASSERT_NOT_EQUAL(0, cpu_info.cache_line_size);

    // CPU 제조사가 설정되었는지 확인
    TEST_ASSERT_NOT_EQUAL(0, strlen(cpu_info.vendor));

    printf("CPU 정보:\n");
    printf("  제조사: %s\n", cpu_info.vendor);
    printf("  브랜드: %s\n", cpu_info.brand);
    printf("  물리 코어: %u개\n", cpu_info.physical_cores);
    printf("  논리 코어: %u개\n", cpu_info.logical_cores);
    printf("  캐시 라인 크기: %u bytes\n", cpu_info.cache_line_size);
}

void test_hardware_detect_simd_features(void) {
    uint32_t features = libetude_hardware_detect_simd_features();

    // SIMD 기능 문자열 변환 테스트
    char feature_string[256];
    LibEtudeErrorCode result = libetude_hardware_simd_features_to_string(
        features, feature_string, sizeof(feature_string));

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, strlen(feature_string));

    printf("SIMD 기능: %s (0x%08X)\n", feature_string, features);

    // 일반적인 SIMD 기능들이 감지되는지 확인
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // x86/x64 플랫폼에서는 최소한 SSE는 지원해야 함
    TEST_ASSERT_TRUE((features & LIBETUDE_SIMD_SSE) != 0 || (features & LIBETUDE_SIMD_SSE2) != 0);
#elif defined(__ARM_NEON) || defined(__aarch64__)
    // ARM 플랫폼에서는 NEON 지원 확인
    TEST_ASSERT_TRUE((features & LIBETUDE_SIMD_NEON) != 0);
#endif
}

void test_hardware_detect_memory(void) {
    LibEtudeHardwareMemoryInfo memory_info;
    LibEtudeErrorCode result = libetude_hardware_detect_memory(&memory_info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, memory_info.total_physical);
    TEST_ASSERT_NOT_EQUAL(0, memory_info.page_size);
    TEST_ASSERT_TRUE(memory_info.available_physical <= memory_info.total_physical);
    TEST_ASSERT_NOT_EQUAL(0, memory_info.recommended_pool_size);

    printf("메모리 정보:\n");
    printf("  총 물리 메모리: %.2f GB\n",
           (double)memory_info.total_physical / (1024.0 * 1024.0 * 1024.0));
    printf("  사용 가능한 물리 메모리: %.2f GB\n",
           (double)memory_info.available_physical / (1024.0 * 1024.0 * 1024.0));
    printf("  페이지 크기: %u bytes\n", memory_info.page_size);
    printf("  할당 단위: %u bytes\n", memory_info.allocation_granularity);
    printf("  메모리 제약 상태: %s\n", memory_info.memory_constrained ? "예" : "아니오");
    printf("  권장 메모리 풀 크기: %.2f MB\n",
           (double)memory_info.recommended_pool_size / (1024.0 * 1024.0));
    printf("  현재 프로세스 메모리 사용량: %.2f MB\n",
           (double)memory_info.process_memory_usage / (1024.0 * 1024.0));
    printf("  최대 프로세스 메모리 사용량: %.2f MB\n",
           (double)memory_info.process_peak_memory_usage / (1024.0 * 1024.0));

    // 권장 메모리 풀 크기가 적절한 범위 내에 있는지 확인
    TEST_ASSERT_TRUE(memory_info.recommended_pool_size >= 64 * 1024 * 1024); // 최소 64MB
    TEST_ASSERT_TRUE(memory_info.recommended_pool_size <= 2ULL * 1024 * 1024 * 1024); // 최대 2GB
}

void test_hardware_detect_gpu(void) {
    LibEtudeHardwareGPUInfo gpu_info;
    LibEtudeErrorCode result = libetude_hardware_detect_gpu(&gpu_info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_NOT_EQUAL(0, strlen(gpu_info.name));

    printf("GPU 정보:\n");
    printf("  이름: %s\n", gpu_info.name);
    printf("  제조사: %s\n", gpu_info.vendor);
    printf("  사용 가능: %s\n", gpu_info.available ? "예" : "아니오");
    printf("  백엔드: %s\n",
           (gpu_info.backend == LIBETUDE_GPU_CUDA) ? "CUDA" :
           (gpu_info.backend == LIBETUDE_GPU_OPENCL) ? "OpenCL" :
           (gpu_info.backend == LIBETUDE_GPU_METAL) ? "Metal" : "없음");
}

void test_hardware_detect_full(void) {
    LibEtudeHardwareInfo info;
    LibEtudeErrorCode result = libetude_hardware_detect(&info);

    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
    TEST_ASSERT_TRUE(info.initialized);
    TEST_ASSERT_NOT_EQUAL(0, strlen(info.platform_name));
    TEST_ASSERT_TRUE(info.performance_tier >= 1 && info.performance_tier <= 5);

    printf("\n=== 전체 하드웨어 정보 ===\n");
    libetude_hardware_print_info(&info);
}

void test_hardware_optimization_functions(void) {
    LibEtudeHardwareInfo info;
    LibEtudeErrorCode result = libetude_hardware_detect(&info);
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);

    // 최적 스레드 수 계산 테스트
    uint32_t optimal_threads = libetude_hardware_get_optimal_thread_count(&info.cpu);
    TEST_ASSERT_TRUE(optimal_threads >= 1 && optimal_threads <= 16);

    // 최적 메모리 풀 크기 계산 테스트
    size_t optimal_pool_size = libetude_hardware_get_optimal_memory_pool_size(&info.memory);
    TEST_ASSERT_TRUE(optimal_pool_size >= 64 * 1024 * 1024); // 최소 64MB

    // 권장 메모리 풀 크기와 계산된 최적 크기 비교
    TEST_ASSERT_TRUE(optimal_pool_size == info.memory.recommended_pool_size);

    // GPU 사용 가능 여부 확인 테스트
    bool gpu_available = libetude_hardware_is_gpu_available(&info.gpu);

    printf("최적화 정보:\n");
    printf("  권장 스레드 수: %u개\n", optimal_threads);
    printf("  권장 메모리 풀 크기: %.2f MB\n",
           (double)optimal_pool_size / (1024.0 * 1024.0));
    printf("  GPU 사용 가능: %s\n", gpu_available ? "예" : "아니오");

    if (gpu_available) {
        printf("  GPU 이름: %s\n", info.gpu.name);
        printf("  GPU 제조사: %s\n", info.gpu.vendor);
        printf("  GPU 메모리: %.2f GB\n",
               (double)info.gpu.total_memory / (1024.0 * 1024.0 * 1024.0));
    }

    // 메모리 제약 상태에 따른 최적화 전략 테스트
    printf("  메모리 제약 상태: %s\n", info.memory.memory_constrained ? "예" : "아니오");
    if (info.memory.memory_constrained) {
        printf("  메모리 제약 상태에서는 더 작은 메모리 풀 크기 권장\n");
        TEST_ASSERT_TRUE(optimal_pool_size <= info.memory.available_physical / 4);
    }
}

void test_hardware_error_handling(void) {
    // NULL 포인터 테스트
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_detect_cpu(NULL));
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_detect_gpu(NULL));
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_detect_memory(NULL));
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_detect(NULL));

    // SIMD 기능 문자열 변환 오류 테스트
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_simd_features_to_string(0, NULL, 100));
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_INVALID_ARGUMENT,
                     libetude_hardware_simd_features_to_string(0, (char*)1, 0));
}

// ============================================================================
// 테스트 스위트 실행
// ============================================================================

int main(void) {
    printf("LibEtude 하드웨어 감지 모듈 테스트 시작\n");
    printf("=====================================\n\n");

    // 개별 컴포넌트 테스트
    RUN_TEST(test_hardware_detect_cpu);
    RUN_TEST(test_hardware_detect_simd_features);
    RUN_TEST(test_hardware_detect_memory);
    RUN_TEST(test_hardware_detect_gpu);

    // 통합 테스트
    RUN_TEST(test_hardware_detect_full);
    RUN_TEST(test_hardware_optimization_functions);

    // 오류 처리 테스트
    RUN_TEST(test_hardware_error_handling);

    printf("\n=====================================\n");
    printf("테스트 완료: %d개 성공, %d개 실패\n",
           test_framework_get_passed_count(),
           test_framework_get_failed_count());

    return test_framework_get_failed_count() == 0 ? 0 : 1;
}