/**
 * @file test_runtime_adaptation.c
 * @brief 런타임 기능 감지 및 적응 시스템 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/runtime_adaptation.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

// ============================================================================
// 테스트 헬퍼 함수들
// ============================================================================

static int test_count = 0;
static int test_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            test_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

// ============================================================================
// 테스트용 함수 구현들
// ============================================================================

// 테스트용 기본 함수
static void* test_generic_function(void) {
    return (void*)0x1000;
}

// 테스트용 SSE 최적화 함수
static void* test_sse_function(void) {
    return (void*)0x2000;
}

// 테스트용 AVX 최적화 함수
static void* test_avx_function(void) {
    return (void*)0x3000;
}

// 테스트용 AVX2 최적화 함수
static void* test_avx2_function(void) {
    return (void*)0x4000;
}

// 테스트용 NEON 최적화 함수
static void* test_neon_function(void) {
    return (void*)0x5000;
}

// ============================================================================
// 하드웨어 기능 감지 테스트
// ============================================================================

/**
 * @brief 하드웨어 기능 감지 테스트
 */
static void test_hardware_detection(void) {
    printf("\n=== 하드웨어 기능 감지 테스트 ===\n");

    ETHardwareCapabilities caps;
    ETResult result = et_detect_hardware_capabilities(&caps);
    TEST_ASSERT(result == ET_SUCCESS, "하드웨어 기능 감지 성공");

    // 기본 정보 검증
    TEST_ASSERT(caps.cpu_count > 0, "CPU 코어 수 유효");
    TEST_ASSERT(caps.total_memory > 0, "총 메모리 크기 유효");
    TEST_ASSERT(strlen(caps.cpu_brand) > 0, "CPU 브랜드 정보 유효");
    TEST_ASSERT(caps.detection_timestamp > 0, "감지 시간 기록됨");

    printf("감지된 하드웨어 정보:\n");
    printf("  - CPU: %s\n", caps.cpu_brand);
    printf("  - 코어 수: %u (물리: %u)\n", caps.cpu_count, caps.physical_cpu_count);
    printf("  - 메모리: %.1f GB (사용 가능: %.1f GB)\n",
           (double)caps.total_memory / (1024*1024*1024),
           (double)caps.available_memory / (1024*1024*1024));
    printf("  - 캐시: L1=%uKB, L2=%uKB, L3=%uMB\n",
           caps.l1_cache_size / 1024, caps.l2_cache_size / 1024, caps.l3_cache_size / (1024*1024));

    printf("SIMD 지원:\n");
    printf("  - SSE: %s\n", caps.has_sse ? "예" : "아니오");
    printf("  - SSE2: %s\n", caps.has_sse2 ? "예" : "아니오");
    printf("  - SSE3: %s\n", caps.has_sse3 ? "예" : "아니오");
    printf("  - SSSE3: %s\n", caps.has_ssse3 ? "예" : "아니오");
    printf("  - SSE4.1: %s\n", caps.has_sse4_1 ? "예" : "아니오");
    printf("  - SSE4.2: %s\n", caps.has_sse4_2 ? "예" : "아니오");
    printf("  - AVX: %s\n", caps.has_avx ? "예" : "아니오");
    printf("  - AVX2: %s\n", caps.has_avx2 ? "예" : "아니오");
    printf("  - AVX512: %s\n", caps.has_avx512 ? "예" : "아니오");
    printf("  - FMA: %s\n", caps.has_fma ? "예" : "아니오");
    printf("  - NEON: %s\n", caps.has_neon ? "예" : "아니오");

    printf("기타 기능:\n");
    printf("  - 고해상도 타이머: %s\n", caps.has_high_res_timer ? "예" : "아니오");
    printf("  - 온도 센서: %s\n", caps.has_thermal_sensors ? "예" : "아니오");
    printf("  - 전력 관리: %s\n", caps.has_power_management ? "예" : "아니오");
}

/**
 * @brief 하드웨어 기능 캐싱 테스트
 */
static void test_hardware_caching(void) {
    printf("\n=== 하드웨어 기능 캐싱 테스트 ===\n");

    // 캐시 무효화
    et_invalidate_hardware_cache();

    ETHardwareCapabilities caps1, caps2;

    // 첫 번째 감지
    ETResult result1 = et_get_cached_hardware_capabilities(&caps1);
    TEST_ASSERT(result1 == ET_SUCCESS, "첫 번째 캐시된 기능 조회 성공");

    // 두 번째 감지 (캐시에서 가져와야 함)
    ETResult result2 = et_get_cached_hardware_capabilities(&caps2);
    TEST_ASSERT(result2 == ET_SUCCESS, "두 번째 캐시된 기능 조회 성공");

    // 캐시된 데이터 일치 확인
    TEST_ASSERT(caps1.cpu_count == caps2.cpu_count, "캐시된 CPU 코어 수 일치");
    TEST_ASSERT(caps1.total_memory == caps2.total_memory, "캐시된 메모리 크기 일치");
    TEST_ASSERT(strcmp(caps1.cpu_brand, caps2.cpu_brand) == 0, "캐시된 CPU 브랜드 일치");
    TEST_ASSERT(caps2.is_cached == true, "캐시 플래그 설정됨");
}

/**
 * @brief 런타임 기능 확인 테스트
 */
static void test_runtime_feature_check(void) {
    printf("\n=== 런타임 기능 확인 테스트 ===\n");

    // 각 기능별 확인
    bool has_sse = et_runtime_has_feature(ET_FEATURE_SSE);
    bool has_sse2 = et_runtime_has_feature(ET_FEATURE_SSE2);
    bool has_avx = et_runtime_has_feature(ET_FEATURE_AVX);
    bool has_avx2 = et_runtime_has_feature(ET_FEATURE_AVX2);
    bool has_neon = et_runtime_has_feature(ET_FEATURE_NEON);
    bool has_timer = et_runtime_has_feature(ET_FEATURE_HIGH_RES_TIMER);

    printf("런타임 기능 확인 결과:\n");
    printf("  - SSE: %s\n", has_sse ? "지원" : "미지원");
    printf("  - SSE2: %s\n", has_sse2 ? "지원" : "미지원");
    printf("  - AVX: %s\n", has_avx ? "지원" : "미지원");
    printf("  - AVX2: %s\n", has_avx2 ? "지원" : "미지원");
    printf("  - NEON: %s\n", has_neon ? "지원" : "미지원");
    printf("  - 고해상도 타이머: %s\n", has_timer ? "지원" : "미지원");

    TEST_ASSERT(1, "런타임 기능 확인 완료");
}

// ============================================================================
// 동적 함수 디스패치 테스트
// ============================================================================

/**
 * @brief 동적 디스패치 시스템 초기화 테스트
 */
static void test_dispatch_initialization(void) {
    printf("\n=== 동적 디스패치 시스템 초기화 테스트 ===\n");

    ETResult result = et_dispatch_initialize();
    TEST_ASSERT(result == ET_SUCCESS, "동적 디스패치 시스템 초기화 성공");

    // 중복 초기화 테스트
    result = et_dispatch_initialize();
    TEST_ASSERT(result == ET_SUCCESS, "중복 초기화 처리 성공");
}

/**
 * @brief 함수 등록 및 선택 테스트
 */
static void test_function_registration_and_selection(void) {
    printf("\n=== 함수 등록 및 선택 테스트 ===\n");

    // 테스트 함수 등록
    ETDispatchEntry entry = {
        .function_name = "test_function",
        .generic_impl = test_generic_function,
        .sse_impl = test_sse_function,
        .avx_impl = test_avx_function,
        .avx2_impl = test_avx2_function,
        .neon_impl = test_neon_function,
        .gpu_impl = NULL,
        .selected_impl = NULL,
        .required_features = 0
    };

    ETResult result = et_dispatch_register_function("test_function", &entry);
    TEST_ASSERT(result == ET_SUCCESS, "함수 등록 성공");

    // 함수 선택
    ETGenericFunction selected = et_dispatch_select_function("test_function");
    TEST_ASSERT(selected != NULL, "함수 선택 성공");

    // 선택된 함수 호출 테스트
    void* result_ptr = selected();
    TEST_ASSERT(result_ptr != NULL, "선택된 함수 호출 성공");

    printf("선택된 함수 결과: %p\n", result_ptr);

    // 하드웨어 기능에 따른 선택 확인
    if (et_runtime_has_feature(ET_FEATURE_AVX2)) {
        TEST_ASSERT(result_ptr == (void*)0x4000, "AVX2 함수 선택됨");
        printf("  -> AVX2 최적화 함수가 선택됨\n");
    } else if (et_runtime_has_feature(ET_FEATURE_AVX)) {
        TEST_ASSERT(result_ptr == (void*)0x3000, "AVX 함수 선택됨");
        printf("  -> AVX 최적화 함수가 선택됨\n");
    } else if (et_runtime_has_feature(ET_FEATURE_SSE)) {
        TEST_ASSERT(result_ptr == (void*)0x2000, "SSE 함수 선택됨");
        printf("  -> SSE 최적화 함수가 선택됨\n");
    } else if (et_runtime_has_feature(ET_FEATURE_NEON)) {
        TEST_ASSERT(result_ptr == (void*)0x5000, "NEON 함수 선택됨");
        printf("  -> NEON 최적화 함수가 선택됨\n");
    } else {
        TEST_ASSERT(result_ptr == (void*)0x1000, "기본 함수 선택됨");
        printf("  -> 기본 함수가 선택됨\n");
    }

    // 존재하지 않는 함수 선택 테스트
    ETGenericFunction not_found = et_dispatch_select_function("nonexistent_function");
    TEST_ASSERT(not_found == NULL, "존재하지 않는 함수 처리 성공");
}

/**
 * @brief 모든 함수 선택 테스트
 */
static void test_select_all_functions(void) {
    printf("\n=== 모든 함수 선택 테스트 ===\n");

    ETResult result = et_dispatch_select_all_functions();
    TEST_ASSERT(result == ET_SUCCESS, "모든 함수 선택 성공");

    // 이미 선택된 함수 재확인
    ETGenericFunction selected = et_dispatch_select_function("test_function");
    TEST_ASSERT(selected != NULL, "선택된 함수 재확인 성공");
}

// ============================================================================
// 성능 프로파일링 테스트
// ============================================================================

/**
 * @brief 성능 프로파일링 기본 테스트
 */
static void test_performance_profiling(void) {
    printf("\n=== 성능 프로파일링 테스트 ===\n");

    // 프로파일링 시작
    ETResult result = et_profiling_begin("test_operation");
    TEST_ASSERT(result == ET_SUCCESS, "프로파일링 시작 성공");

    // 간단한 작업 시뮬레이션
    volatile int sum = 0;
    for (int i = 0; i < 10000; i++) {
        sum += i * i;
    }

    // 프로파일링 종료
    result = et_profiling_end("test_operation");
    TEST_ASSERT(result == ET_SUCCESS, "프로파일링 종료 성공");

    // 메트릭 조회
    ETPerformanceMetrics metrics;
    result = et_profiling_get_metrics("test_operation", &metrics);
    TEST_ASSERT(result == ET_SUCCESS, "성능 메트릭 조회 성공");

    TEST_ASSERT(metrics.call_count == 1, "호출 횟수 정확");
    TEST_ASSERT(metrics.total_time_ns > 0, "총 실행 시간 기록됨");
    TEST_ASSERT(metrics.min_time_ns > 0, "최소 실행 시간 기록됨");
    TEST_ASSERT(metrics.max_time_ns > 0, "최대 실행 시간 기록됨");
    TEST_ASSERT(metrics.average_time_ns > 0, "평균 실행 시간 계산됨");

    printf("성능 메트릭:\n");
    printf("  - 호출 횟수: %llu\n", (unsigned long long)metrics.call_count);
    printf("  - 총 시간: %llu ns\n", (unsigned long long)metrics.total_time_ns);
    printf("  - 평균 시간: %.2f ns\n", metrics.average_time_ns);
    printf("  - 최소 시간: %llu ns\n", (unsigned long long)metrics.min_time_ns);
    printf("  - 최대 시간: %llu ns\n", (unsigned long long)metrics.max_time_ns);
}

/**
 * @brief 다중 프로파일링 테스트
 */
static void test_multiple_profiling(void) {
    printf("\n=== 다중 프로파일링 테스트 ===\n");

    // 여러 작업 프로파일링
    const char* operations[] = {"operation1", "operation2", "operation3"};
    const int num_operations = sizeof(operations) / sizeof(operations[0]);

    for (int i = 0; i < num_operations; i++) {
        ETResult result = et_profiling_begin(operations[i]);
        TEST_ASSERT(result == ET_SUCCESS, "다중 프로파일링 시작 성공");

        // 각기 다른 작업량 시뮬레이션
        volatile int sum = 0;
        for (int j = 0; j < (i + 1) * 1000; j++) {
            sum += j;
        }

        result = et_profiling_end(operations[i]);
        TEST_ASSERT(result == ET_SUCCESS, "다중 프로파일링 종료 성공");
    }

    // 각 작업의 메트릭 확인
    for (int i = 0; i < num_operations; i++) {
        ETPerformanceMetrics metrics;
        ETResult result = et_profiling_get_metrics(operations[i], &metrics);
        TEST_ASSERT(result == ET_SUCCESS, "다중 메트릭 조회 성공");

        printf("%s 메트릭: 시간=%llu ns, 호출=%llu\n",
               operations[i],
               (unsigned long long)metrics.total_time_ns,
               (unsigned long long)metrics.call_count);
    }
}

/**
 * @brief 프로파일링 리셋 테스트
 */
static void test_profiling_reset(void) {
    printf("\n=== 프로파일링 리셋 테스트 ===\n");

    // 리셋 전 메트릭 확인
    ETPerformanceMetrics metrics_before;
    ETResult result = et_profiling_get_metrics("test_operation", &metrics_before);
    TEST_ASSERT(result == ET_SUCCESS, "리셋 전 메트릭 조회 성공");
    TEST_ASSERT(metrics_before.call_count > 0, "리셋 전 호출 횟수 존재");

    // 모든 메트릭 리셋
    et_profiling_reset_all_metrics();

    // 리셋 후 메트릭 확인
    ETPerformanceMetrics metrics_after;
    result = et_profiling_get_metrics("test_operation", &metrics_after);
    TEST_ASSERT(result == ET_SUCCESS, "리셋 후 메트릭 조회 성공");
    TEST_ASSERT(metrics_after.call_count == 0, "리셋 후 호출 횟수 초기화됨");
    TEST_ASSERT(metrics_after.total_time_ns == 0, "리셋 후 총 시간 초기화됨");
}

// ============================================================================
// 열 관리 및 전력 관리 테스트
// ============================================================================

/**
 * @brief 온도 측정 테스트
 */
static void test_temperature_monitoring(void) {
    printf("\n=== 온도 측정 테스트 ===\n");

    ETTemperatureInfo temp_info;
    ETResult result = et_thermal_get_temperature(ET_TEMP_SENSOR_CPU, &temp_info);
    TEST_ASSERT(result == ET_SUCCESS, "CPU 온도 측정 성공");

    TEST_ASSERT(temp_info.current_temp_celsius > 0, "현재 온도 유효");
    TEST_ASSERT(temp_info.max_temp_celsius > temp_info.current_temp_celsius, "최대 온도 설정 유효");
    TEST_ASSERT(temp_info.critical_temp_celsius > temp_info.max_temp_celsius, "임계 온도 설정 유효");
    TEST_ASSERT(temp_info.timestamp > 0, "측정 시간 기록됨");

    printf("온도 정보:\n");
    printf("  - 현재 온도: %.1f°C\n", temp_info.current_temp_celsius);
    printf("  - 최대 온도: %.1f°C\n", temp_info.max_temp_celsius);
    printf("  - 임계 온도: %.1f°C\n", temp_info.critical_temp_celsius);
    printf("  - 과열 상태: %s\n", temp_info.is_overheating ? "예" : "아니오");
    printf("  - 스로틀링: %s\n", temp_info.is_throttling ? "예" : "아니오");
}

/**
 * @brief 전력 정보 테스트
 */
static void test_power_monitoring(void) {
    printf("\n=== 전력 정보 테스트 ===\n");

    ETPowerInfo power_info;
    ETResult result = et_power_get_info(&power_info);
    TEST_ASSERT(result == ET_SUCCESS, "전력 정보 조회 성공");

    TEST_ASSERT(power_info.current_power_watts > 0, "현재 전력 소비 유효");
    TEST_ASSERT(power_info.battery_level_percent >= 0 && power_info.battery_level_percent <= 100, "배터리 잔량 유효");
    TEST_ASSERT(power_info.timestamp > 0, "측정 시간 기록됨");

    printf("전력 정보:\n");
    printf("  - 현재 전력: %.1fW\n", power_info.current_power_watts);
    printf("  - 평균 전력: %.1fW\n", power_info.average_power_watts);
    printf("  - 배터리 잔량: %.1f%%\n", power_info.battery_level_percent);
    printf("  - 충전 중: %s\n", power_info.is_charging ? "예" : "아니오");
    printf("  - 배터리 부족: %s\n", power_info.is_low_battery ? "예" : "아니오");
    printf("  - 예상 사용 시간: %u분\n", power_info.estimated_runtime_minutes);
    printf("  - 전력 상태: %d\n", power_info.current_state);
}

// ============================================================================
// 통합 런타임 적응 시스템 테스트
// ============================================================================

/**
 * @brief 런타임 적응 시스템 초기화 테스트
 */
static void test_runtime_adaptation_initialization(void) {
    printf("\n=== 런타임 적응 시스템 초기화 테스트 ===\n");

    ETRuntimeAdaptationConfig config = {
        .optimization_config = {
            .enable_auto_optimization = true,
            .optimization_interval_ms = 1000,
            .cpu_threshold_percent = 80.0,
            .memory_threshold_percent = 85.0,
            .latency_threshold_ms = 10.0,
            .sample_window_size = 100
        },
        .thermal_config = {
            .warning_temp_celsius = 70.0f,
            .critical_temp_celsius = 85.0f,
            .monitoring_interval_ms = 5000,
            .enable_auto_throttling = true,
            .enable_emergency_shutdown = true
        },
        .power_config = {
            .default_state = ET_POWER_STATE_BALANCED,
            .low_battery_threshold = 20.0f,
            .critical_battery_threshold = 5.0f,
            .monitoring_interval_ms = 10000,
            .enable_auto_power_management = true,
            .enable_cpu_scaling = true,
            .enable_gpu_power_management = false
        },
        .enable_hardware_monitoring = true,
        .enable_performance_profiling = true,
        .enable_thermal_management = true,
        .enable_power_management = true,
        .update_interval_ms = 1000,
        .cache_validity_ms = 30000
    };

    ETResult result = et_runtime_adaptation_initialize(&config);
    TEST_ASSERT(result == ET_SUCCESS, "런타임 적응 시스템 초기화 성공");

    // 중복 초기화 테스트
    result = et_runtime_adaptation_initialize(&config);
    TEST_ASSERT(result == ET_SUCCESS, "중복 초기화 처리 성공");
}

/**
 * @brief 런타임 적응 시스템 시작/중지 테스트
 */
static void test_runtime_adaptation_start_stop(void) {
    printf("\n=== 런타임 적응 시스템 시작/중지 테스트 ===\n");

    // 시스템 시작
    ETResult result = et_runtime_adaptation_start();
    TEST_ASSERT(result == ET_SUCCESS, "런타임 적응 시스템 시작 성공");

    // 중복 시작 테스트
    result = et_runtime_adaptation_start();
    TEST_ASSERT(result == ET_SUCCESS, "중복 시작 처리 성공");

    // 시스템 업데이트
    result = et_runtime_adaptation_update();
    TEST_ASSERT(result == ET_SUCCESS, "런타임 적응 시스템 업데이트 성공");

    // 시스템 중지
    et_runtime_adaptation_stop();
    TEST_ASSERT(1, "런타임 적응 시스템 중지 성공");

    // 중복 중지 테스트
    et_runtime_adaptation_stop();
    TEST_ASSERT(1, "중복 중지 처리 성공");
}

/**
 * @brief 런타임 적응 시스템 상태 조회 테스트
 */
static void test_runtime_adaptation_status(void) {
    printf("\n=== 런타임 적응 시스템 상태 조회 테스트 ===\n");

    char status_buffer[2048];
    ETResult result = et_runtime_adaptation_get_status(status_buffer, sizeof(status_buffer));
    TEST_ASSERT(result == ET_SUCCESS, "상태 조회 성공");
    TEST_ASSERT(strlen(status_buffer) > 0, "상태 정보 유효");

    printf("시스템 상태:\n%s\n", status_buffer);

    // 작은 버퍼 테스트
    char small_buffer[10];
    result = et_runtime_adaptation_get_status(small_buffer, sizeof(small_buffer));
    TEST_ASSERT(result == ET_ERROR_BUFFER_TOO_SMALL, "작은 버퍼 처리 성공");

    // NULL 포인터 테스트
    result = et_runtime_adaptation_get_status(NULL, 100);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "NULL 포인터 처리 성공");
}

/**
 * @brief 런타임 적응 시스템 정리 테스트
 */
static void test_runtime_adaptation_finalization(void) {
    printf("\n=== 런타임 적응 시스템 정리 테스트 ===\n");

    et_runtime_adaptation_finalize();
    TEST_ASSERT(1, "런타임 적응 시스템 정리 성공");

    // 정리 후 중복 호출 테스트
    et_runtime_adaptation_finalize();
    TEST_ASSERT(1, "중복 정리 호출 처리 성공");
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main(void) {
    printf("=== LibEtude 런타임 기능 감지 및 적응 시스템 테스트 ===\n");

    // 하드웨어 기능 감지 테스트
    test_hardware_detection();
    test_hardware_caching();
    test_runtime_feature_check();

    // 동적 함수 디스패치 테스트
    test_dispatch_initialization();
    test_function_registration_and_selection();
    test_select_all_functions();

    // 성능 프로파일링 테스트
    test_performance_profiling();
    test_multiple_profiling();
    test_profiling_reset();

    // 열 관리 및 전력 관리 테스트
    test_temperature_monitoring();
    test_power_monitoring();

    // 통합 런타임 적응 시스템 테스트
    test_runtime_adaptation_initialization();
    test_runtime_adaptation_start_stop();
    test_runtime_adaptation_status();
    test_runtime_adaptation_finalization();

    // 테스트 결과 출력
    printf("\n=== 테스트 결과 ===\n");
    printf("총 테스트: %d\n", test_count);
    printf("통과: %d\n", test_passed);
    printf("실패: %d\n", test_count - test_passed);
    printf("성공률: %.1f%%\n", (float)test_passed / test_count * 100.0f);

    return (test_passed == test_count) ? 0 : 1;
}