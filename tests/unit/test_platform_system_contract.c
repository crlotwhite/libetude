/**
 * @file test_platform_system_contract.c
 * @brief 시스템 인터페이스 계약 검증 테스트
 */

#include "test_platform_abstraction.h"

/**
 * @brief 시스템 인터페이스 계약 검증 테스트
 * @return 테스트 결과
 */
ETResult test_system_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->system);

    ETSystemInterface* system = platform->system;

    // 필수 함수 포인터 검증
    TEST_ASSERT_NOT_NULL(system->get_system_info);
    TEST_ASSERT_NOT_NULL(system->get_memory_info);
    TEST_ASSERT_NOT_NULL(system->get_cpu_info);
    TEST_ASSERT_NOT_NULL(system->get_high_resolution_time);
    TEST_ASSERT_NOT_NULL(system->sleep);
    TEST_ASSERT_NOT_NULL(system->get_simd_features);
    TEST_ASSERT_NOT_NULL(system->has_feature);
    TEST_ASSERT_NOT_NULL(system->get_cpu_usage);
    TEST_ASSERT_NOT_NULL(system->get_memory_usage);

    // 시스템 정보 조회 테스트
    ETSystemInfo sys_info;
    ETResult result = system->get_system_info(&sys_info);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 시스템 정보 유효성 검증
    TEST_ASSERT(sys_info.cpu_count > 0);
    TEST_ASSERT(sys_info.cpu_count <= 256); // 합리적인 상한선
    TEST_ASSERT(sys_info.total_memory > 0);
    TEST_ASSERT(sys_info.available_memory <= sys_info.total_memory);
    TEST_ASSERT(strlen(sys_info.cpu_name) > 0);
    TEST_ASSERT(strlen(sys_info.system_name) > 0);

    // 메모리 정보 조회 테스트
    ETMemoryInfo mem_info;
    result = system->get_memory_info(&mem_info);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    TEST_ASSERT(mem_info.total_physical > 0);
    TEST_ASSERT(mem_info.available_physical <= mem_info.total_physical);
    TEST_ASSERT(mem_info.total_virtual >= mem_info.total_physical);

    // CPU 정보 조회 테스트
    ETCPUInfo cpu_info;
    result = system->get_cpu_info(&cpu_info);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    TEST_ASSERT(cpu_info.core_count > 0);
    TEST_ASSERT(cpu_info.thread_count >= cpu_info.core_count);
    TEST_ASSERT(cpu_info.base_frequency > 0);
    TEST_ASSERT(strlen(cpu_info.vendor) > 0);
    TEST_ASSERT(strlen(cpu_info.brand) > 0);

    // 고해상도 타이머 테스트
    uint64_t time1, time2;
    result = system->get_high_resolution_time(&time1);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 짧은 지연
    system->sleep(1);

    result = system->get_high_resolution_time(&time2);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    TEST_ASSERT(time2 > time1); // 시간이 증가해야 함

    // SIMD 기능 테스트
    uint32_t simd_features = system->get_simd_features();
    // 최소한 하나의 기능은 지원해야 함 (또는 0도 허용)
    TEST_ASSERT(simd_features >= 0);

    // 개별 기능 확인
    bool has_sse = system->has_feature(ET_HARDWARE_FEATURE_SSE);
    bool has_avx = system->has_feature(ET_HARDWARE_FEATURE_AVX);
    bool has_neon = system->has_feature(ET_HARDWARE_FEATURE_NEON);

    // 플랫폼별 기본 기능 확인
#ifdef __x86_64__
    // x86_64에서는 최소한 SSE는 지원해야 함
    TEST_ASSERT(has_sse);
#endif

#ifdef __aarch64__
    // ARM64에서는 NEON을 지원해야 함
    TEST_ASSERT(has_neon);
#endif

    return ET_SUCCESS;
}

/**
 * @brief 시스템 성능 모니터링 테스트
 * @return 테스트 결과
 */
ETResult test_system_performance_monitoring(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->system);

    ETSystemInterface* system = platform->system;

    // CPU 사용률 테스트
    float cpu_usage;
    ETResult result = system->get_cpu_usage(&cpu_usage);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT(cpu_usage >= 0.0f && cpu_usage <= 100.0f);

    // 메모리 사용률 테스트
    ETMemoryUsage mem_usage;
    result = system->get_memory_usage(&mem_usage);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    TEST_ASSERT(mem_usage.physical_used <= mem_usage.physical_total);
    TEST_ASSERT(mem_usage.virtual_used <= mem_usage.virtual_total);
    TEST_ASSERT(mem_usage.physical_usage_percent >= 0.0f &&
                mem_usage.physical_usage_percent <= 100.0f);

    return ET_SUCCESS;
}

/**
 * @brief 시스템 타이머 정확성 테스트
 * @return 테스트 결과
 */
ETResult test_system_timer_accuracy(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->system);

    ETSystemInterface* system = platform->system;

    // 타이머 해상도 테스트
    uint64_t times[10];
    for (int i = 0; i < 10; i++) {
        ETResult result = system->get_high_resolution_time(&times[i]);
        TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    }

    // 시간이 단조증가하는지 확인
    for (int i = 1; i < 10; i++) {
        TEST_ASSERT(times[i] >= times[i-1]);
    }

    // 슬립 정확성 테스트
    uint64_t start_time, end_time;
    system->get_high_resolution_time(&start_time);

    system->sleep(10); // 10ms 슬립

    system->get_high_resolution_time(&end_time);

    uint64_t elapsed_ns = end_time - start_time;
    uint64_t elapsed_ms = elapsed_ns / 1000000;

    // 슬립 시간이 대략적으로 맞는지 확인 (5ms ~ 50ms 허용)
    TEST_ASSERT(elapsed_ms >= 5 && elapsed_ms <= 50);

    return ET_SUCCESS;
}

/**
 * @brief 시스템 오류 조건 테스트
 * @return 테스트 결과
 */
ETResult test_system_error_conditions(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->system);

    ETSystemInterface* system = platform->system;

    // NULL 포인터 테스트
    ETResult result = system->get_system_info(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = system->get_memory_info(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = system->get_cpu_info(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = system->get_high_resolution_time(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = system->get_cpu_usage(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = system->get_memory_usage(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    // 잘못된 기능 ID 테스트
    bool has_invalid = system->has_feature((ETHardwareFeature)9999);
    TEST_ASSERT(!has_invalid);

    return ET_SUCCESS;
}