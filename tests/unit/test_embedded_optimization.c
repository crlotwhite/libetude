#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "libetude/embedded_optimization.h"
#include "libetude/error.h"

// 테스트 헬퍼 함수
static void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

static void assert_success(ETResult result, const char* operation) {
    if (result != ET_SUCCESS) {
        printf("FAILED: %s - Error: %s\n", operation, et_error_string(result));
        exit(1);
    }
    printf("PASSED: %s\n", operation);
}

static void assert_not_null(void* ptr, const char* description) {
    if (!ptr) {
        printf("FAILED: %s is null\n", description);
        exit(1);
    }
    printf("PASSED: %s is not null\n", description);
}

// 기본 컨텍스트 생성 테스트
void test_embedded_context_creation() {
    print_test_header("Embedded Context Creation Test");

    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_NORMAL;
    config.constraints.max_memory_bytes = 1024 * 1024; // 1MB
    config.constraints.max_cpu_freq_mhz = 1000;         // 1GHz
    config.constraints.max_power_mw = 500;              // 500mW
    config.constraints.has_fpu = true;
    config.constraints.has_simd = true;
    config.constraints.cache_size_kb = 256;
    config.constraints.flash_size_kb = 4096;
    config.constraints.ram_size_kb = 1024;

    config.enable_memory_pooling = true;
    config.enable_cache_optimization = true;
    config.min_pool_size = 64 * 1024; // 64KB

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Embedded context");

    ETEmbeddedMode mode = et_embedded_get_mode(ctx);
    assert(mode == ET_EMBEDDED_MODE_NORMAL);
    printf("PASSED: Mode is correctly set to NORMAL\n");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Context destroyed successfully\n");
}

// 최소 메모리 모드 테스트
void test_minimal_memory_mode() {
    print_test_header("Minimal Memory Mode Test");

    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_MINIMAL;
    config.constraints.max_memory_bytes = 256 * 1024; // 256KB
    config.constraints.max_cpu_freq_mhz = 500;         // 500MHz
    config.constraints.max_power_mw = 200;             // 200mW
    config.constraints.has_fpu = true;
    config.constraints.has_simd = false;
    config.constraints.cache_size_kb = 64;
    config.constraints.flash_size_kb = 1024;
    config.constraints.ram_size_kb = 256;

    config.enable_memory_pooling = true;
    config.enable_in_place_ops = true;
    config.enable_layer_streaming = true;
    config.min_pool_size = 32 * 1024; // 32KB

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Minimal memory context");

    // 최소 메모리 모드 활성화 테스트
    ETResult result = et_embedded_enable_minimal_memory_mode(ctx, true);
    assert_success(result, "Enable minimal memory mode");

    // 메모리 제한 설정 테스트
    result = et_embedded_set_memory_limit(ctx, 128 * 1024); // 128KB
    assert_success(result, "Set memory limit");

    // 메모리 가용성 체크 테스트
    bool available = et_embedded_check_memory_available(ctx, 64 * 1024); // 64KB 요청
    printf("Memory availability check (64KB): %s\n", available ? "Available" : "Not available");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Minimal memory mode test completed\n");
}

// 초저전력 모드 테스트
void test_ultra_low_power_mode() {
    print_test_header("Ultra Low Power Mode Test");

    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_ULTRA_LOW;
    config.constraints.max_memory_bytes = 64 * 1024;   // 64KB
    config.constraints.max_cpu_freq_mhz = 100;         // 100MHz
    config.constraints.max_power_mw = 50;              // 50mW
    config.constraints.has_fpu = false;
    config.constraints.has_simd = false;
    config.constraints.cache_size_kb = 0;
    config.constraints.flash_size_kb = 256;
    config.constraints.ram_size_kb = 64;

    config.enable_memory_pooling = true;
    config.enable_in_place_ops = true;
    config.enable_layer_streaming = true;
    config.enable_dynamic_freq = true;
    config.enable_sleep_mode = true;
    config.use_fixed_point = true;
    config.enable_quantization = true;
    config.default_quantization = 4;
    config.idle_timeout_ms = 100;
    config.min_pool_size = 16 * 1024; // 16KB

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Ultra low power context");

    // 저전력 모드 활성화 테스트
    ETResult result = et_embedded_enable_low_power_mode(ctx, true);
    assert_success(result, "Enable low power mode");

    // CPU 주파수 설정 테스트
    result = et_embedded_set_cpu_frequency(ctx, 50); // 50MHz
    assert_success(result, "Set CPU frequency to 50MHz");

    // 슬립 모드 테스트
    result = et_embedded_enter_sleep_mode(ctx);
    assert_success(result, "Enter sleep mode");

    result = et_embedded_exit_sleep_mode(ctx);
    assert_success(result, "Exit sleep mode");

    // 전력 예산 체크 테스트
    bool power_ok = et_embedded_check_power_budget(ctx, 30); // 30mW 요청
    printf("Power budget check (30mW): %s\n", power_ok ? "OK" : "Exceeded");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Ultra low power mode test completed\n");
}

// 고정소수점 및 양자화 테스트
void test_fixed_point_and_quantization() {
    print_test_header("Fixed Point and Quantization Test");

    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_MINIMAL;
    config.constraints.max_memory_bytes = 512 * 1024;
    config.constraints.max_cpu_freq_mhz = 200;
    config.constraints.max_power_mw = 100;
    config.constraints.has_fpu = false; // FPU 없음
    config.min_pool_size = 32 * 1024;

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Fixed point context");

    // 고정소수점 연산 활성화 테스트
    ETResult result = et_embedded_enable_fixed_point(ctx, true);
    assert_success(result, "Enable fixed point arithmetic");

    // 양자화 레벨 설정 테스트
    result = et_embedded_set_quantization_level(ctx, 8); // INT8
    assert_success(result, "Set quantization to 8 bits");

    result = et_embedded_set_quantization_level(ctx, 4); // INT4
    assert_success(result, "Set quantization to 4 bits");

    // 잘못된 양자화 레벨 테스트
    result = et_embedded_set_quantization_level(ctx, 7); // 지원하지 않는 레벨
    assert(result != ET_SUCCESS);
    printf("PASSED: Invalid quantization level correctly rejected\n");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Fixed point and quantization test completed\n");
}

// 프리셋 테스트
void test_presets() {
    print_test_header("Preset Configuration Test");

    ETEmbeddedConfig config = {0};
    config.constraints.max_memory_bytes = 1024 * 1024;
    config.constraints.max_cpu_freq_mhz = 1000;
    config.constraints.max_power_mw = 1000;
    config.min_pool_size = 64 * 1024;

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Preset test context");

    // 마이크로컨트롤러 프리셋 테스트
    ETResult result = et_embedded_apply_microcontroller_preset(ctx);
    assert_success(result, "Apply microcontroller preset");

    ETEmbeddedMode mode = et_embedded_get_mode(ctx);
    assert(mode == ET_EMBEDDED_MODE_ULTRA_LOW);
    printf("PASSED: Microcontroller preset applied correctly\n");

    // IoT 디바이스 프리셋 테스트
    result = et_embedded_apply_iot_device_preset(ctx);
    assert_success(result, "Apply IoT device preset");

    mode = et_embedded_get_mode(ctx);
    assert(mode == ET_EMBEDDED_MODE_MINIMAL);
    printf("PASSED: IoT device preset applied correctly\n");

    // 엣지 디바이스 프리셋 테스트
    result = et_embedded_apply_edge_device_preset(ctx);
    assert_success(result, "Apply edge device preset");

    mode = et_embedded_get_mode(ctx);
    assert(mode == ET_EMBEDDED_MODE_NORMAL);
    printf("PASSED: Edge device preset applied correctly\n");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Preset configuration test completed\n");
}

// 성능 통계 테스트
void test_performance_stats() {
    print_test_header("Performance Statistics Test");

    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_NORMAL;
    config.constraints.max_memory_bytes = 1024 * 1024;
    config.constraints.max_cpu_freq_mhz = 800;
    config.constraints.max_power_mw = 500;
    config.enable_memory_pooling = true;
    config.min_pool_size = 64 * 1024;

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Stats test context");

    // 통계 가져오기 테스트
    ETEmbeddedStats stats;
    ETResult result = et_embedded_get_stats(ctx, &stats);
    assert_success(result, "Get performance stats");

    printf("Initial stats:\n");
    printf("  Current memory: %zu bytes\n", stats.current_memory_usage);
    printf("  Peak memory: %zu bytes\n", stats.peak_memory_usage);
    printf("  Current power: %u mW\n", stats.current_power_mw);
    printf("  CPU frequency: %u MHz\n", stats.current_cpu_freq_mhz);
    printf("  CPU utilization: %.1f%%\n", stats.cpu_utilization * 100.0f);

    // 통계 리셋 테스트
    result = et_embedded_reset_stats(ctx);
    assert_success(result, "Reset stats");

    result = et_embedded_get_stats(ctx, &stats);
    assert_success(result, "Get stats after reset");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Performance statistics test completed\n");
}

// 제약 조건 검증 테스트
void test_constraint_validation() {
    print_test_header("Constraint Validation Test");

    ETEmbeddedConstraints constraints = {0};

    // 잘못된 제약 조건 테스트
    ETResult result = et_embedded_validate_constraints(&constraints);
    assert(result != ET_SUCCESS);
    printf("PASSED: Invalid constraints correctly rejected\n");

    // 올바른 제약 조건 설정
    constraints.max_memory_bytes = 1024 * 1024;
    constraints.max_cpu_freq_mhz = 1000;
    constraints.max_power_mw = 500;
    constraints.has_fpu = true;
    constraints.has_simd = true;
    constraints.cache_size_kb = 256;
    constraints.flash_size_kb = 4096;
    constraints.ram_size_kb = 1024;

    result = et_embedded_validate_constraints(&constraints);
    assert_success(result, "Validate correct constraints");

    printf("PASSED: Constraint validation test completed\n");
}

// 캐시 최적화 테스트
void test_cache_optimization() {
    print_test_header("Cache Optimization Test");

    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_NORMAL;
    config.constraints.max_memory_bytes = 2 * 1024 * 1024; // 2MB
    config.constraints.max_cpu_freq_mhz = 1000;
    config.constraints.max_power_mw = 800;
    config.constraints.cache_size_kb = 512; // 512KB 캐시
    config.enable_cache_optimization = true;
    config.cache_line_size = 64;
    config.min_pool_size = 128 * 1024;

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Cache optimization context");

    // 캐시 최적화 적용 테스트
    ETResult result = et_embedded_optimize_for_cache(ctx);
    assert_success(result, "Optimize for cache");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Cache optimization test completed\n");
}

// 진단 테스트
void test_diagnostics() {
    print_test_header("Diagnostics Test");

    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_NORMAL;
    config.constraints.max_memory_bytes = 1024 * 1024;
    config.constraints.max_cpu_freq_mhz = 800;
    config.constraints.max_power_mw = 400;
    config.constraints.has_fpu = true;
    config.constraints.has_simd = true;
    config.constraints.cache_size_kb = 256;
    config.enable_memory_pooling = true;
    config.enable_cache_optimization = true;
    config.min_pool_size = 64 * 1024;

    ETEmbeddedContext* ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Diagnostics context");

    // 설정 출력 테스트
    printf("\n--- Configuration Output ---\n");
    et_embedded_print_config(ctx);

    // 통계 출력 테스트
    printf("\n--- Statistics Output ---\n");
    et_embedded_print_stats(ctx);

    // 진단 실행 테스트
    printf("\n--- Diagnostics Output ---\n");
    ETResult result = et_embedded_run_diagnostics(ctx);
    assert_success(result, "Run diagnostics");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Diagnostics test completed\n");
}

// 오류 처리 테스트
void test_error_handling() {
    print_test_header("Error Handling Test");

    // NULL 포인터 테스트
    ETEmbeddedContext* null_ctx = NULL;
    ETResult result = et_embedded_set_mode(null_ctx, ET_EMBEDDED_MODE_NORMAL);
    assert(result != ET_SUCCESS);
    printf("PASSED: NULL context correctly handled\n");

    // NULL 설정으로 컨텍스트 생성 테스트
    ETEmbeddedContext* ctx = et_embedded_create_context(NULL);
    assert(ctx == NULL);
    printf("PASSED: NULL config correctly handled\n");

    // 잘못된 메모리 제한 테스트
    ETEmbeddedConfig config = {0};
    config.mode = ET_EMBEDDED_MODE_NORMAL;
    config.constraints.max_memory_bytes = 1024 * 1024;
    config.constraints.max_cpu_freq_mhz = 1000;
    config.constraints.max_power_mw = 500;
    config.min_pool_size = 64 * 1024;

    ctx = et_embedded_create_context(&config);
    assert_not_null(ctx, "Error handling context");

    // 너무 높은 CPU 주파수 설정 테스트
    result = et_embedded_set_cpu_frequency(ctx, 2000); // 제한보다 높음
    assert(result != ET_SUCCESS);
    printf("PASSED: Invalid CPU frequency correctly rejected\n");

    et_embedded_destroy_context(ctx);
    printf("PASSED: Error handling test completed\n");
}

// 메인 테스트 함수
int main() {
    printf("Starting LibEtude Embedded Optimization Tests...\n");

    test_embedded_context_creation();
    test_minimal_memory_mode();
    test_ultra_low_power_mode();
    test_fixed_point_and_quantization();
    test_presets();
    test_performance_stats();
    test_constraint_validation();
    test_cache_optimization();
    test_diagnostics();
    test_error_handling();

    printf("\n=== All Embedded Optimization Tests Passed! ===\n");
    return 0;
}