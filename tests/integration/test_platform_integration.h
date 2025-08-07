/**
 * @file test_platform_integration.h
 * @brief 크로스 플랫폼 통합 테스트 헤더
 *
 * 플랫폼 간 호환성 검증 및 실제 하드웨어에서의 동작 검증을 위한
 * 통합 테스트 헤더 파일입니다.
 */

#ifndef TEST_PLATFORM_INTEGRATION_H
#define TEST_PLATFORM_INTEGRATION_H

#include <libetude/platform/common.h>
#include <libetude/platform/factory.h>
#include <libetude/platform/audio.h>
#include <libetude/platform/system.h>
#include <libetude/platform/threading.h>
#include <libetude/platform/memory.h>
#include <libetude/platform/filesystem.h>
#include <libetude/platform/network.h>
#include <libetude/platform/dynlib.h>
#include <libetude/error.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// 통합 테스트 매크로
#define INTEGRATION_TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "INTEGRATION TEST FAILED: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

#define INTEGRATION_TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "INTEGRATION TEST FAILED: Expected %d, got %d at %s:%d\n", \
                    (int)(expected), (int)(actual), __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

#define INTEGRATION_TEST_ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        double diff = fabs((double)(expected) - (double)(actual)); \
        if (diff > (tolerance)) { \
            fprintf(stderr, "INTEGRATION TEST FAILED: Expected %f, got %f (diff: %f > %f) at %s:%d\n", \
                    (double)(expected), (double)(actual), diff, (double)(tolerance), __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

// 성능 측정 구조체
typedef struct {
    uint64_t start_time;
    uint64_t end_time;
    double elapsed_seconds;
    const char* operation_name;
} PerformanceMeasurement;

// 벤치마크 결과 구조체
typedef struct {
    double min_time;
    double max_time;
    double avg_time;
    double std_dev;
    int iterations;
    const char* test_name;
} BenchmarkResult;

// 스트레스 테스트 설정
typedef struct {
    int thread_count;
    int iterations_per_thread;
    int duration_seconds;
    bool enable_memory_stress;
    bool enable_cpu_stress;
    bool enable_io_stress;
} StressTestConfig;

// 호환성 테스트 결과
typedef struct {
    bool audio_compatible;
    bool system_compatible;
    bool threading_compatible;
    bool memory_compatible;
    bool filesystem_compatible;
    bool network_compatible;
    bool dynlib_compatible;
    char platform_name[64];
    char architecture[32];
} CompatibilityResult;

// 통합 테스트 함수들
ETResult run_cross_platform_compatibility_tests(void);
ETResult run_hardware_validation_tests(void);
ETResult run_performance_benchmark_tests(void);
ETResult run_stress_and_stability_tests(void);

// 플랫폼 간 호환성 테스트
ETResult test_audio_cross_platform_compatibility(void);
ETResult test_system_cross_platform_compatibility(void);
ETResult test_threading_cross_platform_compatibility(void);
ETResult test_memory_cross_platform_compatibility(void);
ETResult test_filesystem_cross_platform_compatibility(void);
ETResult test_network_cross_platform_compatibility(void);
ETResult test_dynlib_cross_platform_compatibility(void);

// 실제 하드웨어 검증 테스트
ETResult test_real_hardware_audio_devices(void);
ETResult test_real_hardware_cpu_features(void);
ETResult test_real_hardware_memory_limits(void);
ETResult test_real_hardware_storage_performance(void);
ETResult test_real_hardware_network_interfaces(void);

// 성능 벤치마크 테스트
ETResult benchmark_audio_latency(BenchmarkResult* result);
ETResult benchmark_memory_allocation_speed(BenchmarkResult* result);
ETResult benchmark_threading_overhead(BenchmarkResult* result);
ETResult benchmark_filesystem_io_speed(BenchmarkResult* result);
ETResult benchmark_network_throughput(BenchmarkResult* result);

// 스트레스 테스트
ETResult stress_test_memory_allocation(const StressTestConfig* config);
ETResult stress_test_threading_contention(const StressTestConfig* config);
ETResult stress_test_audio_streaming(const StressTestConfig* config);
ETResult stress_test_filesystem_operations(const StressTestConfig* config);
ETResult stress_test_mixed_workload(const StressTestConfig* config);

// 안정성 검증 테스트
ETResult stability_test_long_running_audio(void);
ETResult stability_test_memory_leak_detection(void);
ETResult stability_test_resource_exhaustion(void);
ETResult stability_test_error_recovery(void);

// 회귀 테스트
ETResult regression_test_performance_baseline(void);
ETResult regression_test_api_compatibility(void);
ETResult regression_test_behavior_consistency(void);

// 유틸리티 함수들
void start_performance_measurement(PerformanceMeasurement* measurement, const char* operation_name);
void end_performance_measurement(PerformanceMeasurement* measurement);
void calculate_benchmark_statistics(const double* times, int count, BenchmarkResult* result);
void print_compatibility_result(const CompatibilityResult* result);
void print_benchmark_result(const BenchmarkResult* result);
bool is_running_in_ci_environment(void);
bool has_sufficient_system_resources(void);

// 플랫폼별 특화 테스트
#ifdef LIBETUDE_PLATFORM_WINDOWS
ETResult test_windows_specific_features(void);
ETResult test_windows_audio_wasapi_integration(void);
ETResult test_windows_memory_virtual_alloc(void);
#endif

#ifdef LIBETUDE_PLATFORM_LINUX
ETResult test_linux_specific_features(void);
ETResult test_linux_audio_alsa_integration(void);
ETResult test_linux_memory_mmap_integration(void);
#endif

#ifdef LIBETUDE_PLATFORM_MACOS
ETResult test_macos_specific_features(void);
ETResult test_macos_audio_coreaudio_integration(void);
ETResult test_macos_memory_mach_integration(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // TEST_PLATFORM_INTEGRATION_H