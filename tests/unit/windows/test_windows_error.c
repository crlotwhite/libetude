/* LibEtude Windows 오류 처리 시스템 단위 테스트 */
/* Copyright (c) 2025 LibEtude Project */

#include "libetude/platform/windows_error.h"
#include "libetude/platform/windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 테스트 결과 카운터 */
static int g_tests_run = 0;
static int g_tests_passed = 0;

/* 테스트 매크로 */
#define TEST_ASSERT(condition, message) \
    do { \
        g_tests_run++; \
        if (condition) { \
            g_tests_passed++; \
            printf("PASS: %s\n", message); \
        } else { \
            printf("FAIL: %s\n", message); \
        } \
    } while(0)

/* 테스트용 오류 콜백 */
static ETWindowsErrorInfo g_last_callback_error = { 0 };
static bool g_callback_called = false;

static void test_error_callback(const ETWindowsErrorInfo* error_info, void* user_data) {
    (void)user_data; /* 미사용 매개변수 */

    if (error_info) {
        g_last_callback_error = *error_info;
        g_callback_called = true;
    }
}

/* 테스트용 폴백 콜백 */
static bool g_fallback_called = false;
static ETWindowsErrorCode g_fallback_error_code = 0;

static ETResult test_fallback_callback(ETWindowsErrorCode error_code, void* context) {
    (void)context; /* 미사용 매개변수 */

    g_fallback_called = true;
    g_fallback_error_code = error_code;
    return ET_SUCCESS;
}

/* 오류 처리 시스템 초기화/정리 테스트 */
static void test_error_system_init_finalize(void) {
    printf("\n=== Testing Error System Init/Finalize ===\n");

    /* 초기화 테스트 */
    ETResult result = et_windows_error_init();
    TEST_ASSERT(result == ET_SUCCESS, "Error system initialization");

    /* 중복 초기화 테스트 */
    result = et_windows_error_init();
    TEST_ASSERT(result == ET_ERROR_ALREADY_INITIALIZED, "Duplicate initialization prevention");

    /* 정리 테스트 */
    et_windows_error_finalize();

    /* 재초기화 테스트 */
    result = et_windows_error_init();
    TEST_ASSERT(result == ET_SUCCESS, "Error system re-initialization");
}

/* 오류 보고 및 조회 테스트 */
static void test_error_reporting(void) {
    printf("\n=== Testing Error Reporting ===\n");

    /* 오류 보고 테스트 */
    ETResult result = et_windows_report_error(
        ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
        ERROR_FILE_NOT_FOUND,
        E_FAIL,
        "test_module.c",
        "test_function",
        123,
        "Test error message with parameter: %d", 42);

    TEST_ASSERT(result == ET_SUCCESS, "Error reporting");

    /* 마지막 오류 정보 조회 테스트 */
    ETWindowsErrorInfo error_info;
    result = et_windows_get_last_error_info(&error_info);
    TEST_ASSERT(result == ET_SUCCESS, "Last error info retrieval");
    TEST_ASSERT(error_info.error_code == ET_WINDOWS_ERROR_WASAPI_INIT_FAILED, "Error code match");
    TEST_ASSERT(error_info.win32_error == ERROR_FILE_NOT_FOUND, "Win32 error code match");
    TEST_ASSERT(error_info.hresult == E_FAIL, "HRESULT match");
    TEST_ASSERT(strcmp(error_info.module_name, "test_module.c") == 0, "Module name match");
    TEST_ASSERT(strcmp(error_info.function_name, "test_function") == 0, "Function name match");
    TEST_ASSERT(error_info.line_number == 123, "Line number match");
}

/* 오류 메시지 조회 테스트 */
static void test_error_messages(void) {
    printf("\n=== Testing Error Messages ===\n");

    /* 영어 메시지 테스트 */
    const char* msg_en = et_windows_get_error_message(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED);
    TEST_ASSERT(msg_en != NULL, "English error message retrieval");
    TEST_ASSERT(strlen(msg_en) > 0, "English error message not empty");

    /* 한국어 메시지 테스트 */
    const char* msg_ko = et_windows_get_error_message_korean(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED);
    TEST_ASSERT(msg_ko != NULL, "Korean error message retrieval");
    TEST_ASSERT(strlen(msg_ko) > 0, "Korean error message not empty");

    /* 알 수 없는 오류 코드 테스트 */
    const char* unknown_msg = et_windows_get_error_message((ETWindowsErrorCode)0xFFFF);
    TEST_ASSERT(unknown_msg != NULL, "Unknown error message handling");
}

/* 오류 콜백 테스트 */
static void test_error_callback(void) {
    printf("\n=== Testing Error Callback ===\n");

    /* 콜백 등록 테스트 */
    g_callback_called = false;
    ETResult result = et_windows_set_error_callback(test_error_callback, NULL);
    TEST_ASSERT(result == ET_SUCCESS, "Error callback registration");

    /* 오류 발생 시 콜백 호출 테스트 */
    et_windows_report_error(
        ET_WINDOWS_ERROR_DIRECTSOUND_INIT_FAILED,
        0, S_OK, "test.c", "test_func", 1,
        "Callback test error");

    TEST_ASSERT(g_callback_called == true, "Error callback invocation");
    TEST_ASSERT(g_last_callback_error.error_code == ET_WINDOWS_ERROR_DIRECTSOUND_INIT_FAILED,
               "Callback error code match");

    /* 콜백 제거 테스트 */
    result = et_windows_remove_error_callback();
    TEST_ASSERT(result == ET_SUCCESS, "Error callback removal");

    /* 콜백 제거 후 호출되지 않는지 테스트 */
    g_callback_called = false;
    et_windows_report_error(
        ET_WINDOWS_ERROR_THREAD_POOL_CREATION_FAILED,
        0, S_OK, "test.c", "test_func", 2,
        "No callback test error");

    TEST_ASSERT(g_callback_called == false, "No callback after removal");
}

/* 폴백 메커니즘 테스트 */
static void test_fallback_mechanism(void) {
    printf("\n=== Testing Fallback Mechanism ===\n");

    /* 폴백 콜백 등록 테스트 */
    g_fallback_called = false;
    ETResult result = et_windows_register_fallback(
        ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED,
        test_fallback_callback, NULL);
    TEST_ASSERT(result == ET_SUCCESS, "Fallback callback registration");

    /* 폴백 실행 테스트 */
    result = et_windows_execute_fallback(ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED);
    TEST_ASSERT(result == ET_SUCCESS, "Fallback execution");
    TEST_ASSERT(g_fallback_called == true, "Fallback callback invocation");
    TEST_ASSERT(g_fallback_error_code == ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED,
               "Fallback error code match");

    /* 등록되지 않은 오류에 대한 폴백 실행 테스트 */
    result = et_windows_execute_fallback(ET_WINDOWS_ERROR_ETW_EVENT_WRITE_FAILED);
    TEST_ASSERT(result == ET_ERROR_NOT_FOUND, "Unregistered fallback handling");

    /* 폴백 전략 설정 테스트 */
    result = et_windows_set_fallback_strategy(
        ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED,
        ET_WINDOWS_FALLBACK_DEGRADED);
    TEST_ASSERT(result == ET_SUCCESS, "Fallback strategy setting");
}

/* 오류 통계 테스트 */
static void test_error_statistics(void) {
    printf("\n=== Testing Error Statistics ===\n");

    /* 통계 초기화 */
    ETResult result = et_windows_reset_error_statistics();
    TEST_ASSERT(result == ET_SUCCESS, "Error statistics reset");

    /* 통계 조회 (초기 상태) */
    ETWindowsErrorStatistics stats;
    result = et_windows_get_error_statistics(&stats);
    TEST_ASSERT(result == ET_SUCCESS, "Error statistics retrieval");
    TEST_ASSERT(stats.total_errors == 0, "Initial total errors count");
    TEST_ASSERT(stats.critical_errors == 0, "Initial critical errors count");

    /* 오류 발생 후 통계 확인 */
    et_windows_report_error(
        ET_WINDOWS_ERROR_SECURITY_CHECK_FAILED,
        0, S_OK, "test.c", "test_func", 1,
        "Critical error for statistics test");

    result = et_windows_get_error_statistics(&stats);
    TEST_ASSERT(result == ET_SUCCESS, "Error statistics after error");
    TEST_ASSERT(stats.total_errors == 1, "Total errors count increment");
    TEST_ASSERT(stats.critical_errors == 1, "Critical errors count increment");
}

/* 성능 저하 상태 테스트 */
static void test_degradation_state(void) {
    printf("\n=== Testing Degradation State ===\n");

    /* 초기 성능 저하 상태 조회 */
    ETWindowsDegradationState state;
    ETResult result = et_windows_get_degradation_state(&state);
    TEST_ASSERT(result == ET_SUCCESS, "Initial degradation state retrieval");
    TEST_ASSERT(state.performance_scale_factor == 1.0f, "Initial performance scale factor");

    /* 성능 저하 적용 테스트 */
    ETWindowsDegradationState new_state = { 0 };
    new_state.audio_quality_reduced = true;
    new_state.simd_optimization_disabled = true;
    new_state.performance_scale_factor = 0.8f;

    result = et_windows_apply_degradation(&new_state);
    TEST_ASSERT(result == ET_SUCCESS, "Degradation state application");

    /* 적용된 성능 저하 상태 확인 */
    result = et_windows_get_degradation_state(&state);
    TEST_ASSERT(result == ET_SUCCESS, "Applied degradation state retrieval");
    TEST_ASSERT(state.audio_quality_reduced == true, "Audio quality degradation applied");
    TEST_ASSERT(state.simd_optimization_disabled == true, "SIMD optimization disabled");
    TEST_ASSERT(state.performance_scale_factor == 0.8f, "Performance scale factor applied");

    /* 복구 시도 테스트 */
    result = et_windows_attempt_recovery();
    TEST_ASSERT(result == ET_SUCCESS || result == ET_ERROR_OPERATION_FAILED, "Recovery attempt");
}

/* 로깅 기능 테스트 */
static void test_logging_functionality(void) {
    printf("\n=== Testing Logging Functionality ===\n");

    /* 임시 로그 파일 경로 */
    const char* log_path = "test_error_log.txt";

    /* 로깅 활성화 테스트 */
    ETResult result = et_windows_enable_error_logging(log_path);
    TEST_ASSERT(result == ET_SUCCESS, "Error logging enablement");

    /* 오류 발생 (로그에 기록됨) */
    et_windows_report_error(
        ET_WINDOWS_ERROR_WASAPI_BUFFER_UNDERRUN,
        0, S_OK, "test.c", "test_func", 1,
        "Logging test error");

    /* 시스템 정보 로깅 테스트 */
    result = et_windows_log_system_info();
    TEST_ASSERT(result == ET_SUCCESS, "System info logging");

    /* 오류 보고서 생성 테스트 */
    const char* report_path = "test_error_report.txt";
    result = et_windows_generate_error_report(report_path);
    TEST_ASSERT(result == ET_SUCCESS, "Error report generation");

    /* 로깅 비활성화 테스트 */
    result = et_windows_disable_error_logging();
    TEST_ASSERT(result == ET_SUCCESS, "Error logging disablement");

    /* 테스트 파일 정리 */
    remove(log_path);
    remove(report_path);
}

/* 기본 폴백 콜백 등록 테스트 */
static void test_default_fallbacks(void) {
    printf("\n=== Testing Default Fallbacks ===\n");

    /* 기본 폴백 등록 테스트 */
    ETResult result = et_windows_register_default_fallbacks();
    TEST_ASSERT(result == ET_SUCCESS, "Default fallbacks registration");

    /* WASAPI 폴백 테스트 (실제 실행은 하지 않고 등록만 확인) */
    result = et_windows_execute_fallback(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED);
    TEST_ASSERT(result == ET_SUCCESS || result == ET_ERROR_INVALID_PARAMETER,
               "WASAPI fallback execution attempt");
}

/* 매크로 헬퍼 테스트 */
static void test_macro_helpers(void) {
    printf("\n=== Testing Macro Helpers ===\n");

    /* Win32 오류 보고 매크로 테스트 */
    SetLastError(ERROR_ACCESS_DENIED);
    ET_WINDOWS_REPORT_WIN32_ERROR(ET_WINDOWS_ERROR_REGISTRY_ACCESS_DENIED,
                                 "Macro test: Win32 error reporting");

    /* 마지막 오류 확인 */
    ETWindowsErrorInfo error_info;
    ETResult result = et_windows_get_last_error_info(&error_info);
    TEST_ASSERT(result == ET_SUCCESS, "Macro error info retrieval");
    TEST_ASSERT(error_info.error_code == ET_WINDOWS_ERROR_REGISTRY_ACCESS_DENIED,
               "Macro error code match");
    TEST_ASSERT(error_info.win32_error == ERROR_ACCESS_DENIED, "Macro Win32 error match");

    /* HRESULT 오류 보고 매크로 테스트 */
    ET_WINDOWS_REPORT_HRESULT_ERROR(ET_WINDOWS_ERROR_COM_INIT_FAILED, E_OUTOFMEMORY,
                                   "Macro test: HRESULT error reporting");

    result = et_windows_get_last_error_info(&error_info);
    TEST_ASSERT(result == ET_SUCCESS, "Macro HRESULT error info retrieval");
    TEST_ASSERT(error_info.error_code == ET_WINDOWS_ERROR_COM_INIT_FAILED,
               "Macro HRESULT error code match");
    TEST_ASSERT(error_info.hresult == E_OUTOFMEMORY, "Macro HRESULT match");
}

/* 메인 테스트 함수 */
int main(void) {
    printf("LibEtude Windows Error Handling System Unit Tests\n");
    printf("================================================\n");

    /* 테스트 실행 */
    test_error_system_init_finalize();
    test_error_reporting();
    test_error_messages();
    test_error_callback();
    test_fallback_mechanism();
    test_error_statistics();
    test_degradation_state();
    test_logging_functionality();
    test_default_fallbacks();
    test_macro_helpers();

    /* 정리 */
    et_windows_error_finalize();

    /* 결과 출력 */
    printf("\n=== Test Results ===\n");
    printf("Tests Run: %d\n", g_tests_run);
    printf("Tests Passed: %d\n", g_tests_passed);
    printf("Tests Failed: %d\n", g_tests_run - g_tests_passed);
    printf("Success Rate: %.1f%%\n",
           g_tests_run > 0 ? (100.0 * g_tests_passed / g_tests_run) : 0.0);

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}