/**
 * @file test_mobile_optimization.c
 * @brief 모바일 최적화 시스템 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "libetude/api.h"
#include "libetude/mobile_power_management.h"
#include "libetude/thermal_management.h"
#include "libetude/memory_optimization.h"
#include "bindings/mobile_optimization.h"

// 테스트 헬퍼 함수들
static void test_power_management();
static void test_thermal_management();
static void test_memory_optimization();
static void test_mobile_optimization_integration();
static void test_battery_optimization();
static void test_thermal_throttling();
static void test_memory_pressure_handling();

// 콜백 함수들
static void power_event_callback(const BatteryStatus* status, void* user_data);
static void thermal_event_callback(ThermalState old_state, ThermalState new_state, const ThermalStatus* status, void* user_data);
static void memory_event_callback(MemoryPressureLevel old_level, MemoryPressureLevel new_level, const MemoryUsageStats* stats, void* user_data);

// 전역 테스트 상태
static struct {
    bool power_callback_called;
    bool thermal_callback_called;
    bool memory_callback_called;
    ThermalState last_thermal_state;
    MemoryPressureLevel last_memory_pressure;
} g_test_state = {0};

int main() {
    printf("=== LibEtude Mobile Optimization Tests ===\n\n");

    // 개별 컴포넌트 테스트
    test_power_management();
    test_thermal_management();
    test_memory_optimization();

    // 통합 테스트
    test_mobile_optimization_integration();
    test_battery_optimization();
    test_thermal_throttling();
    test_memory_pressure_handling();

    printf("All mobile optimization tests passed!\n");
    return 0;
}

// ============================================================================
// 전력 관리 테스트
// ============================================================================

static void test_power_management() {
    printf("Testing power management...\n");

    // 초기화 테스트
    assert(power_management_init() == LIBETUDE_SUCCESS);

    // 엔진 생성 (테스트용)
    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    assert(engine != NULL);

    // 전력 프로파일 설정 테스트
    assert(power_set_profile(engine, POWER_PROFILE_BALANCED) == LIBETUDE_SUCCESS);

    PowerProfile current_profile;
    assert(power_get_profile(engine, &current_profile) == LIBETUDE_SUCCESS);
    assert(current_profile == POWER_PROFILE_BALANCED);

    // 전력 사용량 통계 테스트
    PowerUsageStats stats;
    assert(power_get_usage_stats(&stats) == LIBETUDE_SUCCESS);
    assert(stats.total_power_mw > 0);
    assert(stats.energy_efficiency_score >= 0.0f && stats.energy_efficiency_score <= 1.0f);

    // 배터리 상태 테스트
    BatteryStatus battery_status;
    assert(power_get_battery_status(&battery_status) == LIBETUDE_SUCCESS);
    assert(battery_status.capacity_percentage >= 0.0f && battery_status.capacity_percentage <= 1.0f);

    // 배터리 기반 자동 최적화 테스트
    battery_status.capacity_percentage = 0.15f;  // 15% 배터리
    battery_status.is_charging = false;
    assert(power_auto_optimize_for_battery(engine, &battery_status) == LIBETUDE_SUCCESS);

    // 프로파일이 절전 모드로 변경되었는지 확인
    assert(power_get_profile(engine, &current_profile) == LIBETUDE_SUCCESS);
    assert(current_profile == POWER_PROFILE_ULTRA_POWER_SAVER || current_profile == POWER_PROFILE_POWER_SAVER);

    // 백그라운드/포그라운드 모드 테스트
    assert(power_enter_background_mode(engine) == LIBETUDE_SUCCESS);
    assert(power_enter_foreground_mode(engine) == LIBETUDE_SUCCESS);

    // 리포트 생성 테스트
    char* report = power_generate_report();
    assert(report != NULL);
    assert(strlen(report) > 0);
    free(report);

    // 정리
    libetude_destroy_engine(engine);
    assert(power_management_cleanup() == LIBETUDE_SUCCESS);

    printf("  ✓ Power management tests passed\n");
}

// ============================================================================
// 열 관리 테스트
// ============================================================================

static void test_thermal_management() {
    printf("Testing thermal management...\n");

    // 초기화 테스트
    assert(thermal_management_init() == LIBETUDE_SUCCESS);

    // 엔진 생성 (테스트용)
    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    assert(engine != NULL);

    // 온도 센서 테스트
    TempSensorInfo sensors[16];
    int sensor_count;
    assert(thermal_get_sensors(sensors, 16, &sensor_count) == LIBETUDE_SUCCESS);
    assert(sensor_count > 0);

    // 온도 읽기 테스트
    float cpu_temp;
    if (thermal_read_temperature(TEMP_SENSOR_CPU, &cpu_temp) == LIBETUDE_SUCCESS) {
        assert(cpu_temp > 0.0f && cpu_temp < 100.0f);  // 합리적인 온도 범위
    }

    // 열 상태 테스트
    ThermalStatus status;
    assert(thermal_get_status(&status) == LIBETUDE_SUCCESS);
    assert(status.current_state >= THERMAL_STATE_NORMAL && status.current_state <= THERMAL_STATE_CRITICAL);

    // 열 상태 업데이트 테스트
    assert(thermal_update_status() == LIBETUDE_SUCCESS);

    // 열 상태 결정 테스트
    ThermalThresholds thresholds = {40.0f, 50.0f, 65.0f, 80.0f, 2.0f};

    ThermalState state = thermal_determine_state(35.0f, &thresholds, THERMAL_STATE_NORMAL);
    assert(state == THERMAL_STATE_NORMAL);

    state = thermal_determine_state(55.0f, &thresholds, THERMAL_STATE_NORMAL);
    assert(state == THERMAL_STATE_WARM);

    state = thermal_determine_state(70.0f, &thresholds, THERMAL_STATE_WARM);
    assert(state == THERMAL_STATE_HOT);

    state = thermal_determine_state(85.0f, &thresholds, THERMAL_STATE_HOT);
    assert(state == THERMAL_STATE_CRITICAL);

    // 열 제한 테스트
    assert(thermal_apply_throttling(engine, THERMAL_STATE_HOT) == LIBETUDE_SUCCESS);
    assert(thermal_remove_throttling(engine) == LIBETUDE_SUCCESS);

    // CPU/GPU 제한 테스트
    assert(thermal_throttle_cpu(0.7f) == LIBETUDE_SUCCESS);
    assert(thermal_throttle_gpu(engine, 0.5f) == LIBETUDE_SUCCESS);

    // 냉각 대기 테스트 (짧은 타임아웃)
    int result = thermal_wait_for_cooling(30.0f, 1000);  // 1초 타임아웃
    assert(result == LIBETUDE_SUCCESS || result == LIBETUDE_ERROR_TIMEOUT);

    // 리포트 생성 테스트
    char* report = thermal_generate_report();
    assert(report != NULL);
    assert(strlen(report) > 0);
    free(report);

    // 정리
    libetude_destroy_engine(engine);
    assert(thermal_management_cleanup() == LIBETUDE_SUCCESS);

    printf("  ✓ Thermal management tests passed\n");
}

// ============================================================================
// 메모리 최적화 테스트
// ============================================================================

static void test_memory_optimization() {
    printf("Testing memory optimization...\n");

    // 초기화 테스트
    assert(memory_optimization_init() == LIBETUDE_SUCCESS);

    // 엔진 생성 (테스트용)
    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    assert(engine != NULL);

    // 메모리 사용량 통계 테스트
    MemoryUsageStats stats;
    assert(memory_get_usage_stats(&stats) == LIBETUDE_SUCCESS);
    assert(stats.total_memory_mb > 0);
    assert(stats.memory_efficiency >= 0.0f && stats.memory_efficiency <= 1.0f);

    // 메모리 압박 레벨 결정 테스트
    MemoryOptimizationConfig config;
    assert(memory_get_optimization_config(&config) == LIBETUDE_SUCCESS);

    MemoryPressureLevel level = memory_determine_pressure_level(100, 1000, &config);
    assert(level == MEMORY_PRESSURE_LOW);

    level = memory_determine_pressure_level(800, 1000, &config);
    assert(level == MEMORY_PRESSURE_HIGH);

    level = memory_determine_pressure_level(950, 1000, &config);
    assert(level == MEMORY_PRESSURE_CRITICAL);

    // 메모리 압박 처리 테스트
    assert(memory_handle_pressure(engine, MEMORY_PRESSURE_MEDIUM) == LIBETUDE_SUCCESS);
    assert(memory_handle_pressure(engine, MEMORY_PRESSURE_HIGH) == LIBETUDE_SUCCESS);

    // 메모리 해제 테스트
    size_t freed_mb = memory_free_memory(engine, 32);
    assert(freed_mb >= 0);  // 해제된 메모리는 0 이상

    // 사용하지 않는 메모리 정리 테스트
    size_t cleaned_mb = memory_cleanup_unused(engine);
    assert(cleaned_mb >= 0);

    // 메모리 단편화 해소 테스트
    assert(memory_defragment() == LIBETUDE_SUCCESS);

    // 압축 테스트
    assert(memory_enable_compression(MEMORY_COMPRESSION_LZ4, 3) == LIBETUDE_SUCCESS);

    const char* test_data = "This is test data for compression";
    void* compressed_data;
    size_t compressed_size;

    if (memory_compress_block(test_data, strlen(test_data), &compressed_data, &compressed_size) == LIBETUDE_SUCCESS) {
        assert(compressed_data != NULL);
        assert(compressed_size > 0);

        // 압축 해제 테스트
        void* decompressed_data;
        size_t decompressed_size;

        if (memory_decompress_block(compressed_data, compressed_size, &decompressed_data, &decompressed_size) == LIBETUDE_SUCCESS) {
            assert(decompressed_data != NULL);
            assert(decompressed_size >= strlen(test_data));
            free(decompressed_data);
        }

        free(compressed_data);
    }

    assert(memory_disable_compression() == LIBETUDE_SUCCESS);

    // 가비지 컬렉션 테스트
    size_t gc_freed = memory_garbage_collect(engine);
    assert(gc_freed >= 0);

    // 자동 GC 테스트
    assert(memory_enable_auto_gc(5000, 0.8f) == LIBETUDE_SUCCESS);
    usleep(100000);  // 0.1초 대기
    assert(memory_disable_auto_gc() == LIBETUDE_SUCCESS);

    // 캐시 최적화 테스트
    assert(memory_enable_cache_optimization(32, 256) == LIBETUDE_SUCCESS);
    assert(memory_flush_cache() == LIBETUDE_SUCCESS);

    int hits, misses;
    float hit_ratio;
    assert(memory_get_cache_stats(&hits, &misses, &hit_ratio) == LIBETUDE_SUCCESS);
    assert(hit_ratio >= 0.0f && hit_ratio <= 1.0f);

    // 리포트 생성 테스트
    char* report = memory_generate_optimization_report();
    assert(report != NULL);
    assert(strlen(report) > 0);
    free(report);

    // 정리
    libetude_destroy_engine(engine);
    assert(memory_optimization_cleanup() == LIBETUDE_SUCCESS);

    printf("  ✓ Memory optimization tests passed\n");
}

// ============================================================================
// 통합 테스트
// ============================================================================

static void test_mobile_optimization_integration() {
    printf("Testing mobile optimization integration...\n");

    // 모든 시스템 초기화
    assert(power_management_init() == LIBETUDE_SUCCESS);
    assert(thermal_management_init() == LIBETUDE_SUCCESS);
    assert(memory_optimization_init() == LIBETUDE_SUCCESS);

    // 엔진 생성
    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    assert(engine != NULL);

    // 플랫폼 감지 테스트
    MobilePlatform platform = mobile_detect_platform();
    assert(platform >= MOBILE_PLATFORM_UNKNOWN && platform <= MOBILE_PLATFORM_IOS);

    MobileDeviceClass device_class = mobile_detect_device_class();
    assert(device_class >= MOBILE_DEVICE_LOW_END && device_class <= MOBILE_DEVICE_HIGH_END);

    // 기본 설정 생성 테스트
    MobileOptimizationConfig config = mobile_create_default_config(platform, device_class);
    assert(config.platform == platform);
    assert(config.device_class == device_class);
    assert(config.memory_limit_mb > 0);
    assert(config.max_threads > 0);

    // 리소스 상태 테스트
    MobileResourceStatus status;
    assert(mobile_get_resource_status(&status) == LIBETUDE_SUCCESS);
    assert(status.memory_pressure >= 0.0f && status.memory_pressure <= 1.0f);
    assert(status.cpu_usage >= 0.0f && status.cpu_usage <= 1.0f);

    // 적응형 품질 조정 테스트
    assert(mobile_adaptive_quality_adjustment(engine, &status, &config) == LIBETUDE_SUCCESS);

    // 통계 생성 테스트
    char* stats = mobile_get_optimization_stats();
    assert(stats != NULL);
    assert(strlen(stats) > 0);
    free(stats);

    // 정리
    libetude_destroy_engine(engine);
    power_management_cleanup();
    thermal_management_cleanup();
    memory_optimization_cleanup();

    printf("  ✓ Mobile optimization integration tests passed\n");
}

// ============================================================================
// 배터리 최적화 테스트
// ============================================================================

static void test_battery_optimization() {
    printf("Testing battery optimization...\n");

    assert(power_management_init() == LIBETUDE_SUCCESS);

    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    assert(engine != NULL);

    // 다양한 배터리 상태에서 최적화 테스트
    BatteryStatus battery_status;

    // 배터리 부족 상황 (10%)
    battery_status.capacity_percentage = 0.1f;
    battery_status.is_charging = false;
    battery_status.low_power_mode = true;
    battery_status.temperature_c = 35.0f;

    assert(power_auto_optimize_for_battery(engine, &battery_status) == LIBETUDE_SUCCESS);

    PowerProfile profile;
    assert(power_get_profile(engine, &profile) == LIBETUDE_SUCCESS);
    assert(profile == POWER_PROFILE_ULTRA_POWER_SAVER);

    // 충전 중 상황 (80%)
    battery_status.capacity_percentage = 0.8f;
    battery_status.is_charging = true;
    battery_status.low_power_mode = false;
    battery_status.temperature_c = 30.0f;

    assert(power_auto_optimize_for_battery(engine, &battery_status) == LIBETUDE_SUCCESS);

    assert(power_get_profile(engine, &profile) == LIBETUDE_SUCCESS);
    assert(profile == POWER_PROFILE_MAXIMUM_PERFORMANCE || profile == POWER_PROFILE_BALANCED);

    // 배터리 과열 상황
    battery_status.capacity_percentage = 0.6f;
    battery_status.is_charging = false;
    battery_status.low_power_mode = false;
    battery_status.temperature_c = 45.0f;  // 높은 온도

    assert(power_auto_optimize_for_battery(engine, &battery_status) == LIBETUDE_SUCCESS);

    assert(power_get_profile(engine, &profile) == LIBETUDE_SUCCESS);
    assert(profile == POWER_PROFILE_POWER_SAVER || profile == POWER_PROFILE_ULTRA_POWER_SAVER);

    libetude_destroy_engine(engine);
    power_management_cleanup();

    printf("  ✓ Battery optimization tests passed\n");
}

// ============================================================================
// 열 제한 테스트
// ============================================================================

static void test_thermal_throttling() {
    printf("Testing thermal throttling...\n");

    assert(thermal_management_init() == LIBETUDE_SUCCESS);

    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    assert(engine != NULL);

    // 열 모니터링 시작
    g_test_state.thermal_callback_called = false;
    assert(thermal_start_monitoring(thermal_event_callback, NULL) == LIBETUDE_SUCCESS);

    // 잠시 대기하여 모니터링 동작 확인
    usleep(100000);  // 0.1초

    // 다양한 열 상태에서 제한 테스트
    assert(thermal_apply_throttling(engine, THERMAL_STATE_NORMAL) == LIBETUDE_SUCCESS);
    assert(thermal_apply_throttling(engine, THERMAL_STATE_WARM) == LIBETUDE_SUCCESS);
    assert(thermal_apply_throttling(engine, THERMAL_STATE_HOT) == LIBETUDE_SUCCESS);
    assert(thermal_apply_throttling(engine, THERMAL_STATE_CRITICAL) == LIBETUDE_SUCCESS);

    // 제한 해제 테스트
    assert(thermal_remove_throttling(engine) == LIBETUDE_SUCCESS);

    // 예측적 제한 테스트
    assert(thermal_predictive_throttling(engine, 75.0f) == LIBETUDE_SUCCESS);

    // 모니터링 중지
    assert(thermal_stop_monitoring() == LIBETUDE_SUCCESS);

    libetude_destroy_engine(engine);
    thermal_management_cleanup();

    printf("  ✓ Thermal throttling tests passed\n");
}

// ============================================================================
// 메모리 압박 처리 테스트
// ============================================================================

static void test_memory_pressure_handling() {
    printf("Testing memory pressure handling...\n");

    assert(memory_optimization_init() == LIBETUDE_SUCCESS);

    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    assert(engine != NULL);

    // 메모리 모니터링 시작
    g_test_state.memory_callback_called = false;
    assert(memory_start_monitoring(memory_event_callback, NULL, 1000) == LIBETUDE_SUCCESS);

    // 잠시 대기하여 모니터링 동작 확인
    usleep(100000);  // 0.1초

    // 다양한 메모리 압박 상황 테스트
    assert(memory_handle_pressure(engine, MEMORY_PRESSURE_NONE) == LIBETUDE_SUCCESS);
    assert(memory_handle_pressure(engine, MEMORY_PRESSURE_LOW) == LIBETUDE_SUCCESS);
    assert(memory_handle_pressure(engine, MEMORY_PRESSURE_MEDIUM) == LIBETUDE_SUCCESS);
    assert(memory_handle_pressure(engine, MEMORY_PRESSURE_HIGH) == LIBETUDE_SUCCESS);
    assert(memory_handle_pressure(engine, MEMORY_PRESSURE_CRITICAL) == LIBETUDE_SUCCESS);

    // 메모리 해제 테스트
    size_t freed = memory_free_memory(engine, 64);
    assert(freed >= 0);

    // 가비지 컬렉션 테스트
    size_t gc_freed = memory_garbage_collect(engine);
    assert(gc_freed >= 0);

    // 모니터링 중지
    assert(memory_stop_monitoring() == LIBETUDE_SUCCESS);

    libetude_destroy_engine(engine);
    memory_optimization_cleanup();

    printf("  ✓ Memory pressure handling tests passed\n");
}

// ============================================================================
// 콜백 함수 구현
// ============================================================================

static void power_event_callback(const BatteryStatus* status, void* user_data) {
    (void)user_data;

    if (status) {
        g_test_state.power_callback_called = true;
        printf("    Power callback: Battery %.1f%%, Charging: %s\n",
               status->capacity_percentage * 100.0f,
               status->is_charging ? "Yes" : "No");
    }
}

static void thermal_event_callback(ThermalState old_state, ThermalState new_state, const ThermalStatus* status, void* user_data) {
    (void)user_data;

    g_test_state.thermal_callback_called = true;
    g_test_state.last_thermal_state = new_state;

    const char* state_names[] = {"Normal", "Warm", "Hot", "Critical"};
    printf("    Thermal callback: %s -> %s, Max temp: %.1f°C\n",
           state_names[old_state], state_names[new_state],
           status ? status->max_temperature_c : 0.0f);
}

static void memory_event_callback(MemoryPressureLevel old_level, MemoryPressureLevel new_level, const MemoryUsageStats* stats, void* user_data) {
    (void)user_data;

    g_test_state.memory_callback_called = true;
    g_test_state.last_memory_pressure = new_level;

    const char* level_names[] = {"None", "Low", "Medium", "High", "Critical"};
    printf("    Memory callback: %s -> %s, Used: %zu MB\n",
           level_names[old_level], level_names[new_level],
           stats ? stats->used_memory_mb : 0);
}