/**
 * @file test_platform_mocking.h
 * @brief 플랫폼 추상화 레이어 모킹 프레임워크 헤더
 *
 * 테스트용 모킹 인터페이스와 테스트 유틸리티를 제공합니다.
 */

#ifndef TEST_PLATFORM_MOCKING_H
#define TEST_PLATFORM_MOCKING_H

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
#include <stdarg.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// 모킹 호출 기록
typedef struct MockCall {
    const char* function_name;
    void* parameters;
    size_t parameter_size;
    ETResult return_value;
    uint64_t timestamp;
    struct MockCall* next;
} MockCall;

// 모킹 기대값
typedef struct MockExpectation {
    const char* function_name;
    void* expected_parameters;
    size_t parameter_size;
    ETResult return_value;
    int call_count;
    int max_calls;
    bool (*parameter_matcher)(const void* expected, const void* actual, size_t size);
    struct MockExpectation* next;
} MockExpectation;

// 모킹 컨텍스트
typedef struct {
    MockCall* call_history;
    MockExpectation* expectations;
    int total_calls;
    int failed_expectations;
    bool strict_mode; // 엄격 모드: 예상하지 않은 호출 시 실패
    bool recording_mode; // 기록 모드: 실제 호출을 기록
} MockContext;

// 전역 모킹 컨텍스트
extern MockContext g_mock_context;

// 모킹 프레임워크 함수들
void mock_init(void);
void mock_cleanup(void);
void mock_reset(void);
void mock_set_strict_mode(bool strict);
void mock_set_recording_mode(bool recording);

// 기대값 설정
void mock_expect_call(const char* function_name, const void* parameters, size_t param_size,
                     ETResult return_value, int max_calls);
void mock_expect_call_with_matcher(const char* function_name, const void* parameters, size_t param_size,
                                  ETResult return_value, int max_calls,
                                  bool (*matcher)(const void*, const void*, size_t));

// 호출 기록
void mock_record_call(const char* function_name, const void* parameters, size_t param_size,
                     ETResult return_value);

// 검증 함수들
bool mock_verify_all_expectations(void);
bool mock_verify_call_count(const char* function_name, int expected_count);
bool mock_verify_call_order(const char** function_names, int count);
bool mock_verify_no_unexpected_calls(void);

// 호출 기록 조회
int mock_get_call_count(const char* function_name);
MockCall* mock_get_call_history(const char* function_name);
void mock_print_call_history(void);

// 매개변수 매처 함수들
bool mock_match_exact(const void* expected, const void* actual, size_t size);
bool mock_match_ignore(const void* expected, const void* actual, size_t size);
bool mock_match_string(const void* expected, const void* actual, size_t size);
bool mock_match_pointer_not_null(const void* expected, const void* actual, size_t size);

// 모킹된 오디오 인터페이스
typedef struct {
    ETAudioInterface base;
    MockContext* context;

    // 모킹 설정
    ETResult open_output_device_result;
    ETResult open_input_device_result;
    ETResult start_stream_result;
    ETResult stop_stream_result;
    ETResult pause_stream_result;
    ETResult set_callback_result;
    ETResult enumerate_devices_result;

    // 모킹 데이터
    ETAudioDeviceInfo* mock_devices;
    int mock_device_count;
    uint32_t mock_latency;
    ETAudioState mock_state;

    // 호출 카운터
    int open_output_device_calls;
    int open_input_device_calls;
    int close_device_calls;
    int start_stream_calls;
    int stop_stream_calls;
    int pause_stream_calls;
    int set_callback_calls;
    int enumerate_devices_calls;
    int get_latency_calls;
    int get_state_calls;
} MockAudioInterface;

// 모킹된 시스템 인터페이스
typedef struct {
    ETSystemInterface base;
    MockContext* context;

    // 모킹 설정
    ETResult get_system_info_result;
    ETResult get_memory_info_result;
    ETResult get_cpu_info_result;
    ETResult get_high_resolution_time_result;
    ETResult sleep_result;
    ETResult get_cpu_usage_result;
    ETResult get_memory_usage_result;

    // 모킹 데이터
    ETSystemInfo mock_system_info;
    ETMemoryInfo mock_memory_info;
    ETCPUInfo mock_cpu_info;
    uint64_t mock_time;
    uint32_t mock_simd_features;
    float mock_cpu_usage;
    ETMemoryUsage mock_memory_usage;

    // 호출 카운터
    int get_system_info_calls;
    int get_memory_info_calls;
    int get_cpu_info_calls;
    int get_high_resolution_time_calls;
    int sleep_calls;
    int get_simd_features_calls;
    int has_feature_calls;
    int get_cpu_usage_calls;
    int get_memory_usage_calls;
} MockSystemInterface;

// 모킹된 스레딩 인터페이스
typedef struct {
    ETThreadInterface base;
    MockContext* context;

    // 모킹 설정
    ETResult create_thread_result;
    ETResult join_thread_result;
    ETResult detach_thread_result;
    ETResult set_thread_priority_result;
    ETResult set_thread_affinity_result;
    ETResult get_current_thread_id_result;
    ETResult create_mutex_result;
    ETResult lock_mutex_result;
    ETResult unlock_mutex_result;
    ETResult try_lock_mutex_result;
    ETResult create_semaphore_result;
    ETResult wait_semaphore_result;
    ETResult post_semaphore_result;
    ETResult create_condition_result;
    ETResult wait_condition_result;
    ETResult signal_condition_result;
    ETResult broadcast_condition_result;

    // 모킹 데이터
    ETThreadID mock_thread_id;

    // 호출 카운터
    int create_thread_calls;
    int join_thread_calls;
    int detach_thread_calls;
    int destroy_thread_calls;
    int set_thread_priority_calls;
    int set_thread_affinity_calls;
    int get_current_thread_id_calls;
    int create_mutex_calls;
    int lock_mutex_calls;
    int unlock_mutex_calls;
    int try_lock_mutex_calls;
    int destroy_mutex_calls;
    int create_semaphore_calls;
    int wait_semaphore_calls;
    int post_semaphore_calls;
    int destroy_semaphore_calls;
    int create_condition_calls;
    int wait_condition_calls;
    int signal_condition_calls;
    int broadcast_condition_calls;
    int destroy_condition_calls;
} MockThreadingInterface;

// 모킹 인터페이스 생성 함수들
MockAudioInterface* mock_audio_interface_create(void);
void mock_audio_interface_destroy(MockAudioInterface* mock);
void mock_audio_interface_reset(MockAudioInterface* mock);

MockSystemInterface* mock_system_interface_create(void);
void mock_system_interface_destroy(MockSystemInterface* mock);
void mock_system_interface_reset(MockSystemInterface* mock);

MockThreadingInterface* mock_threading_interface_create(void);
void mock_threading_interface_destroy(MockThreadingInterface* mock);
void mock_threading_interface_reset(MockThreadingInterface* mock);

// 테스트 데이터 생성 유틸리티
void generate_test_audio_device_info(ETAudioDeviceInfo* info, int index);
void generate_test_system_info(ETSystemInfo* info);
void generate_test_memory_info(ETMemoryInfo* info);
void generate_test_cpu_info(ETCPUInfo* info);

// 테스트 검증 유틸리티
bool verify_audio_format_equal(const ETAudioFormat* expected, const ETAudioFormat* actual);
bool verify_system_info_valid(const ETSystemInfo* info);
bool verify_memory_info_valid(const ETMemoryInfo* info);
bool verify_cpu_info_valid(const ETCPUInfo* info);

// 자동화된 테스트 실행 및 보고
typedef struct {
    const char* test_name;
    ETResult (*test_function)(void);
    const char* description;
    bool enabled;
} AutomatedTest;

typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    double total_time;
    char report_file[256];
} TestReport;

// 자동화된 테스트 실행
ETResult run_automated_test_suite(AutomatedTest* tests, int test_count, TestReport* report);
void generate_test_report(const TestReport* report, const char* format); // "xml", "json", "text"
void save_test_report(const TestReport* report, const char* filename);

// CI/CD 통합 유틸리티
bool is_ci_environment(void);
void set_ci_environment_variables(void);
void export_test_results_for_ci(const TestReport* report);

#ifdef __cplusplus
}
#endif

#endif // TEST_PLATFORM_MOCKING_H