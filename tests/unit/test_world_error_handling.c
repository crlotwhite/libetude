/**
 * @file test_world_error_handling.c
 * @brief WORLD4UTAU 에러 처리 및 로깅 시스템 단위 테스트
 *
 * world4utau 예제 프로젝트의 에러 처리와 로깅 시스템에 대한
 * 포괄적인 단위 테스트를 제공합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

// world4utau 에러 처리 시스템 포함
#include "../../examples/world4utau/include/world_error.h"

// 테스트 프레임워크 (간단한 구현)
static int g_test_count = 0;
static int g_test_passed = 0;
static int g_test_failed = 0;

// 테스트 콜백을 위한 전역 변수들
static bool g_error_callback_called = false;
static ETError g_last_callback_error;
static bool g_log_callback_called = false;
static ETLogLevel g_last_log_level;
static char g_last_log_message[512];

// =============================================================================
// 테스트 프레임워크 매크로
// =============================================================================

#define TEST_ASSERT(condition, message) \
    do { \
        g_test_count++; \
        if (condition) { \
            printf("✓ PASS: %s\n", message); \
            g_test_passed++; \
        } else { \
            printf("✗ FAIL: %s\n", message); \
            g_test_failed++; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) \
    TEST_ASSERT(strcmp(expected, actual) == 0, message)

// =============================================================================
// 테스트 콜백 함수들
// =============================================================================

/**
 * @brief 테스트용 에러 콜백 함수
 */
static void test_error_callback(const ETError* error, void* user_data)
{
    g_error_callback_called = true;
    if (error) {
        g_last_callback_error = *error;
    }

    // user_data 검증
    int* test_data = (int*)user_data;
    if (test_data && *test_data == 12345) {
        // 콜백 데이터가 올바르게 전달됨
    }
}

/**
 * @brief 테스트용 로그 콜백 함수
 */
static void test_log_callback(ETLogLevel level, const char* message, void* user_data)
{
    g_log_callback_called = true;
    g_last_log_level = level;
    strncpy(g_last_log_message, message, sizeof(g_last_log_message) - 1);
    g_last_log_message[sizeof(g_last_log_message) - 1] = '\0';

    // user_data 검증
    int* test_data = (int*)user_data;
    if (test_data && *test_data == 54321) {
        // 콜백 데이터가 올바르게 전달됨
    }
}

/**
 * @brief 테스트 상태 초기화
 */
static void reset_test_state(void)
{
    g_error_callback_called = false;
    g_log_callback_called = false;
    g_last_log_level = ET_LOG_DEBUG;
    memset(g_last_log_message, 0, sizeof(g_last_log_message));
    memset(&g_last_callback_error, 0, sizeof(g_last_callback_error));

    // 에러 상태 초기화
    world_clear_error();
}

// =============================================================================
// 에러 코드 테스트
// =============================================================================

/**
 * @brief 에러 코드 문자열 변환 테스트
 */
static void test_error_code_strings(void)
{
    printf("\n=== 에러 코드 문자열 변환 테스트 ===\n");

    // UTAU 인터페이스 관련 에러
    const char* msg1 = world_get_error_string(WORLD_ERROR_UTAU_INVALID_PARAMS);
    TEST_ASSERT_NOT_NULL(msg1, "UTAU 잘못된 파라미터 에러 메시지 존재");
    TEST_ASSERT(strlen(msg1) > 0, "UTAU 잘못된 파라미터 에러 메시지 비어있지 않음");

    const char* msg2 = world_get_error_string(WORLD_ERROR_UTAU_PARSE_FAILED);
    TEST_ASSERT_NOT_NULL(msg2, "UTAU 파싱 실패 에러 메시지 존재");
    TEST_ASSERT(strstr(msg2, "파싱") != NULL, "UTAU 파싱 실패 메시지에 '파싱' 포함");

    // WORLD 분석 관련 에러
    const char* msg3 = world_get_error_string(WORLD_ERROR_F0_EXTRACTION_FAILED);
    TEST_ASSERT_NOT_NULL(msg3, "F0 추출 실패 에러 메시지 존재");
    TEST_ASSERT(strstr(msg3, "F0") != NULL, "F0 추출 실패 메시지에 'F0' 포함");

    // WORLD 합성 관련 에러
    const char* msg4 = world_get_error_string(WORLD_ERROR_SYNTHESIS_FAILED);
    TEST_ASSERT_NOT_NULL(msg4, "합성 실패 에러 메시지 존재");
    TEST_ASSERT(strstr(msg4, "합성") != NULL, "합성 실패 메시지에 '합성' 포함");

    // 존재하지 않는 에러 코드 (libetude 기본 처리로 폴백)
    const char* msg5 = world_get_error_string((WorldErrorCode)-9999);
    TEST_ASSERT_NOT_NULL(msg5, "존재하지 않는 에러 코드도 메시지 반환");
}

/**
 * @brief 에러 설정 및 조회 테스트
 */
static void test_error_setting_and_retrieval(void)
{
    printf("\n=== 에러 설정 및 조회 테스트 ===\n");

    reset_test_state();

    // 에러 설정 전 상태 확인
    const ETError* error = world_get_last_error();
    TEST_ASSERT_NULL(error, "초기 상태에서는 에러가 없음");

    // 에러 설정
    WORLD_SET_ERROR(WORLD_ERROR_UTAU_INVALID_PARAMS, "테스트 에러 메시지: %d", 123);

    // 에러 조회
    error = world_get_last_error();
    TEST_ASSERT_NOT_NULL(error, "에러 설정 후 에러 정보 존재");
    TEST_ASSERT_EQUAL(WORLD_ERROR_UTAU_INVALID_PARAMS, error->code, "에러 코드 일치");
    TEST_ASSERT(strstr(error->message, "테스트 에러 메시지") != NULL, "에러 메시지 포함");
    TEST_ASSERT(strstr(error->message, "123") != NULL, "포맷된 메시지 포함");
    TEST_ASSERT_NOT_NULL(error->file, "파일명 정보 존재");
    TEST_ASSERT_NOT_NULL(error->function, "함수명 정보 존재");
    TEST_ASSERT(error->line > 0, "라인 번호 정보 존재");

    // 에러 클리어
    world_clear_error();
    error = world_get_last_error();
    TEST_ASSERT_NULL(error, "에러 클리어 후 에러 정보 없음");
}

/**
 * @brief 에러 콜백 테스트
 */
static void test_error_callback_functionality(void)
{
    printf("\n=== 에러 콜백 테스트 ===\n");

    reset_test_state();

    int test_data = 12345;

    // 콜백 설정
    world_set_error_callback(test_error_callback, &test_data);

    // 에러 발생
    WORLD_SET_ERROR(WORLD_ERROR_ANALYSIS_FAILED, "콜백 테스트 에러");

    // 콜백 호출 확인
    TEST_ASSERT(g_error_callback_called, "에러 콜백이 호출됨");
    TEST_ASSERT_EQUAL(WORLD_ERROR_ANALYSIS_FAILED, g_last_callback_error.code, "콜백에서 올바른 에러 코드 수신");
    TEST_ASSERT(strstr(g_last_callback_error.message, "콜백 테스트") != NULL, "콜백에서 올바른 에러 메시지 수신");

    // 콜백 제거 후 테스트
    reset_test_state();
    world_set_error_callback(NULL, NULL);

    WORLD_SET_ERROR(WORLD_ERROR_CACHE_READ_FAILED, "콜백 제거 후 에러");
    TEST_ASSERT(!g_error_callback_called, "콜백 제거 후 콜백 호출되지 않음");
}

// =============================================================================
// 로깅 시스템 테스트
// =============================================================================

/**
 * @brief 로깅 시스템 초기화 테스트
 */
static void test_logging_initialization(void)
{
    printf("\n=== 로깅 시스템 초기화 테스트 ===\n");

    // 로깅 시스템 초기화
    ETResult result = world_init_logging();
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "로깅 시스템 초기화 성공");

    // 중복 초기화 테스트
    result = world_init_logging();
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "중복 초기화도 성공");

    // 정리
    world_cleanup_logging();
}

/**
 * @brief 로그 레벨 테스트
 */
static void test_log_levels(void)
{
    printf("\n=== 로그 레벨 테스트 ===\n");

    world_init_logging();

    // 기본 로그 레벨 확인
    ETLogLevel default_level = world_get_log_level();
    TEST_ASSERT(default_level >= ET_LOG_DEBUG && default_level <= ET_LOG_FATAL, "기본 로그 레벨이 유효 범위 내");

    // 로그 레벨 설정
    world_set_log_level(ET_LOG_WARNING);
    ETLogLevel current_level = world_get_log_level();
    TEST_ASSERT_EQUAL(ET_LOG_WARNING, current_level, "로그 레벨 설정 성공");

    // 다른 레벨로 변경
    world_set_log_level(ET_LOG_ERROR);
    current_level = world_get_log_level();
    TEST_ASSERT_EQUAL(ET_LOG_ERROR, current_level, "로그 레벨 변경 성공");

    world_cleanup_logging();
}

/**
 * @brief 로그 카테고리 테스트
 */
static void test_log_categories(void)
{
    printf("\n=== 로그 카테고리 테스트 ===\n");

    world_init_logging();

    // 모든 카테고리 문자열 테스트
    for (int i = 0; i < 7; i++) {
        const char* category_str = world_log_category_string((WorldLogCategory)i);
        TEST_ASSERT_NOT_NULL(category_str, "카테고리 문자열 존재");
        TEST_ASSERT(strlen(category_str) > 0, "카테고리 문자열 비어있지 않음");
    }

    // 잘못된 카테고리 테스트
    const char* invalid_category = world_log_category_string((WorldLogCategory)999);
    TEST_ASSERT_STRING_EQUAL("UNKNOWN", invalid_category, "잘못된 카테고리는 UNKNOWN 반환");

    // 카테고리 활성화/비활성화 테스트
    world_set_log_category_enabled(WORLD_LOG_ANALYSIS, false);
    TEST_ASSERT(!world_is_log_category_enabled(WORLD_LOG_ANALYSIS), "카테고리 비활성화 성공");

    world_set_log_category_enabled(WORLD_LOG_ANALYSIS, true);
    TEST_ASSERT(world_is_log_category_enabled(WORLD_LOG_ANALYSIS), "카테고리 활성화 성공");

    world_cleanup_logging();
}

/**
 * @brief 로그 콜백 테스트
 */
static void test_log_callback_functionality(void)
{
    printf("\n=== 로그 콜백 테스트 ===\n");

    world_init_logging();
    reset_test_state();

    int test_data = 54321;

    // 콜백 설정
    world_set_log_callback(test_log_callback, &test_data);

    // 로그 출력
    world_log(WORLD_LOG_UTAU_INTERFACE, ET_LOG_INFO, "테스트 로그 메시지: %d", 456);

    // 콜백 호출 확인
    TEST_ASSERT(g_log_callback_called, "로그 콜백이 호출됨");
    TEST_ASSERT_EQUAL(ET_LOG_INFO, g_last_log_level, "콜백에서 올바른 로그 레벨 수신");
    TEST_ASSERT(strstr(g_last_log_message, "테스트 로그 메시지") != NULL, "콜백에서 올바른 로그 메시지 수신");
    TEST_ASSERT(strstr(g_last_log_message, "456") != NULL, "콜백에서 포맷된 메시지 수신");

    // 콜백 제거
    world_clear_log_callback();
    reset_test_state();

    world_log(WORLD_LOG_SYNTHESIS, ET_LOG_ERROR, "콜백 제거 후 로그");
    TEST_ASSERT(!g_log_callback_called, "콜백 제거 후 콜백 호출되지 않음");

    world_cleanup_logging();
}

/**
 * @brief 향상된 로깅 기능 테스트
 */
static void test_enhanced_logging(void)
{
    printf("\n=== 향상된 로깅 기능 테스트 ===\n");

    world_init_logging();
    reset_test_state();

    world_set_log_callback(test_log_callback, NULL);

    // 타임스탬프 포함 로깅
    world_set_log_timestamps(true);
    world_log_enhanced(WORLD_LOG_PERFORMANCE, ET_LOG_INFO, "타임스탬프 테스트");
    TEST_ASSERT(g_log_callback_called, "향상된 로깅 콜백 호출됨");

    reset_test_state();

    // 성능 로깅 테스트
    world_log_performance(WORLD_LOG_PERFORMANCE, "테스트 작업", 123.45, "추가 정보");
    TEST_ASSERT(g_log_callback_called, "성능 로깅 콜백 호출됨");
    TEST_ASSERT(strstr(g_last_log_message, "123.45") != NULL, "성능 로깅에 시간 정보 포함");
    TEST_ASSERT(strstr(g_last_log_message, "추가 정보") != NULL, "성능 로깅에 추가 정보 포함");

    reset_test_state();

    // 메모리 로깅 테스트
    world_log_memory(WORLD_LOG_MEMORY, "테스트 할당", 1024 * 1024, true);
    TEST_ASSERT(g_log_callback_called, "메모리 로깅 콜백 호출됨");
    TEST_ASSERT(strstr(g_last_log_message, "할당") != NULL, "메모리 로깅에 할당 정보 포함");

    world_cleanup_logging();
}

/**
 * @brief 로그 필터링 테스트
 */
static void test_log_filtering(void)
{
    printf("\n=== 로그 필터링 테스트 ===\n");

    world_init_logging();
    reset_test_state();

    world_set_log_callback(test_log_callback, NULL);

    // 레벨 필터링 테스트
    world_set_log_level(ET_LOG_ERROR);

    // INFO 레벨 로그 (필터링되어야 함)
    world_log_enhanced(WORLD_LOG_ANALYSIS, ET_LOG_INFO, "필터링될 메시지");
    TEST_ASSERT(!g_log_callback_called, "낮은 레벨 로그가 필터링됨");

    reset_test_state();

    // ERROR 레벨 로그 (통과되어야 함)
    world_log_enhanced(WORLD_LOG_ANALYSIS, ET_LOG_ERROR, "통과할 메시지");
    TEST_ASSERT(g_log_callback_called, "높은 레벨 로그가 통과됨");

    reset_test_state();

    // 카테고리 필터링 테스트
    world_set_log_level(ET_LOG_DEBUG); // 모든 레벨 허용
    world_set_log_category_enabled(WORLD_LOG_CACHE, false);

    world_log_enhanced(WORLD_LOG_CACHE, ET_LOG_INFO, "필터링될 카테고리");
    TEST_ASSERT(!g_log_callback_called, "비활성화된 카테고리 로그가 필터링됨");

    reset_test_state();

    world_log_enhanced(WORLD_LOG_SYNTHESIS, ET_LOG_INFO, "통과할 카테고리");
    TEST_ASSERT(g_log_callback_called, "활성화된 카테고리 로그가 통과됨");

    world_cleanup_logging();
}

// =============================================================================
// 매크로 테스트
// =============================================================================

/**
 * @brief 에러 처리 매크로 테스트
 */
static void test_error_macros(void)
{
    printf("\n=== 에러 처리 매크로 테스트 ===\n");

    reset_test_state();

    // WORLD_CHECK_ERROR 매크로 테스트 함수
    auto int test_check_error_macro(bool condition) {
        WORLD_CHECK_ERROR(condition, WORLD_ERROR_UTAU_INVALID_PARAMS, "조건 실패: %s", "테스트");
        return ET_SUCCESS;
    }

    // 조건이 참일 때
    int result = test_check_error_macro(true);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "조건이 참일 때 매크로 통과");

    // 조건이 거짓일 때
    result = test_check_error_macro(false);
    TEST_ASSERT_EQUAL(WORLD_ERROR_UTAU_INVALID_PARAMS, result, "조건이 거짓일 때 매크로 에러 반환");

    const ETError* error = world_get_last_error();
    TEST_ASSERT_NOT_NULL(error, "매크로로 설정된 에러 정보 존재");
    TEST_ASSERT(strstr(error->message, "조건 실패") != NULL, "매크로 에러 메시지 포함");

    // WORLD_CHECK_NULL 매크로 테스트
    auto int test_check_null_macro(void* ptr) {
        WORLD_CHECK_NULL(ptr, "포인터가 NULL입니다: %p", ptr);
        return ET_SUCCESS;
    }

    reset_test_state();

    int dummy = 42;
    result = test_check_null_macro(&dummy);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "NULL이 아닌 포인터에서 매크로 통과");

    result = test_check_null_macro(NULL);
    TEST_ASSERT_EQUAL(WORLD_ERROR_UTAU_INVALID_PARAMS, result, "NULL 포인터에서 매크로 에러 반환");
}

/**
 * @brief 로깅 매크로 테스트
 */
static void test_logging_macros(void)
{
    printf("\n=== 로깅 매크로 테스트 ===\n");

    world_init_logging();
    reset_test_state();

    world_set_log_callback(test_log_callback, NULL);

    // 기본 로깅 매크로 테스트
    WORLD_LOG_UTAU_INFO("UTAU 인터페이스 테스트: %d", 789);
    TEST_ASSERT(g_log_callback_called, "UTAU 로깅 매크로 호출됨");
    TEST_ASSERT(strstr(g_last_log_message, "UTAU_INTERFACE") != NULL, "UTAU 카테고리 정보 포함");
    TEST_ASSERT(strstr(g_last_log_message, "789") != NULL, "포맷된 메시지 포함");

    reset_test_state();

    // 향상된 로깅 매크로 테스트
    WORLD_LOG_ENHANCED_WARNING(WORLD_LOG_ANALYSIS, "분석 경고: %s", "테스트 경고");
    TEST_ASSERT(g_log_callback_called, "향상된 로깅 매크로 호출됨");
    TEST_ASSERT_EQUAL(ET_LOG_WARNING, g_last_log_level, "올바른 로그 레벨");
    TEST_ASSERT(strstr(g_last_log_message, "분석 경고") != NULL, "경고 메시지 포함");

    reset_test_state();

    // 성능 로깅 매크로 테스트
    WORLD_LOG_PERFORMANCE_TIMING(WORLD_LOG_PERFORMANCE, "매크로 테스트", 99.99);
    TEST_ASSERT(g_log_callback_called, "성능 로깅 매크로 호출됨");
    TEST_ASSERT(strstr(g_last_log_message, "99.99") != NULL, "성능 시간 정보 포함");

    reset_test_state();

    // 메모리 로깅 매크로 테스트
    WORLD_LOG_MEMORY_ALLOC(WORLD_LOG_MEMORY, "매크로 테스트", 2048);
    TEST_ASSERT(g_log_callback_called, "메모리 로깅 매크로 호출됨");
    TEST_ASSERT(strstr(g_last_log_message, "할당") != NULL, "메모리 할당 정보 포함");

    world_cleanup_logging();
}

// =============================================================================
// 스트레스 테스트
// =============================================================================

/**
 * @brief 대량 에러 처리 스트레스 테스트
 */
static void test_error_stress(void)
{
    printf("\n=== 에러 처리 스트레스 테스트 ===\n");

    reset_test_state();

    // 대량 에러 설정 및 조회
    const int stress_count = 1000;
    for (int i = 0; i < stress_count; i++) {
        WORLD_SET_ERROR(WORLD_ERROR_ANALYSIS_FAILED, "스트레스 테스트 에러 #%d", i);

        const ETError* error = world_get_last_error();
        if (!error || error->code != WORLD_ERROR_ANALYSIS_FAILED) {
            TEST_ASSERT(false, "스트레스 테스트 중 에러 처리 실패");
            return;
        }

        world_clear_error();
    }

    TEST_ASSERT(true, "대량 에러 처리 스트레스 테스트 통과");
}

/**
 * @brief 대량 로깅 스트레스 테스트
 */
static void test_logging_stress(void)
{
    printf("\n=== 로깅 스트레스 테스트 ===\n");

    world_init_logging();

    // 대량 로그 출력
    const int stress_count = 1000;
    for (int i = 0; i < stress_count; i++) {
        world_log(WORLD_LOG_PERFORMANCE, ET_LOG_INFO, "스트레스 테스트 로그 #%d", i);
    }

    TEST_ASSERT(true, "대량 로깅 스트레스 테스트 통과");

    world_cleanup_logging();
}

// =============================================================================
// 메인 테스트 실행 함수
// =============================================================================

/**
 * @brief 모든 테스트 실행
 */
int main(void)
{
    printf("WORLD4UTAU 에러 처리 및 로깅 시스템 단위 테스트 시작\n");
    printf("================================================\n");

    // libetude 에러 시스템 초기화
    et_init_logging();

    // 에러 코드 테스트
    test_error_code_strings();
    test_error_setting_and_retrieval();
    test_error_callback_functionality();

    // 로깅 시스템 테스트
    test_logging_initialization();
    test_log_levels();
    test_log_categories();
    test_log_callback_functionality();
    test_enhanced_logging();
    test_log_filtering();

    // 매크로 테스트
    test_error_macros();
    test_logging_macros();

    // 스트레스 테스트
    test_error_stress();
    test_logging_stress();

    // 테스트 결과 출력
    printf("\n================================================\n");
    printf("테스트 결과:\n");
    printf("  총 테스트: %d\n", g_test_count);
    printf("  통과: %d\n", g_test_passed);
    printf("  실패: %d\n", g_test_failed);
    printf("  성공률: %.1f%%\n", g_test_count > 0 ? (double)g_test_passed / g_test_count * 100.0 : 0.0);

    // libetude 에러 시스템 정리
    et_cleanup_logging();

    if (g_test_failed == 0) {
        printf("\n🎉 모든 테스트가 통과했습니다!\n");
        return 0;
    } else {
        printf("\n❌ %d개의 테스트가 실패했습니다.\n", g_test_failed);
        return 1;
    }
}