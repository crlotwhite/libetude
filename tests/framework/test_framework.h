/**
 * @file test_framework.h
 * @brief 간단한 테스트 프레임워크 헤더
 * @author LibEtude Project
 * @version 1.0.0
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 테스트 케이스 구조체
 */
typedef struct {
    char name[128];                 /**< 테스트 이름 */
    void (*setup)();               /**< 설정 함수 */
    void (*teardown)();            /**< 정리 함수 */
    void (*test_func)();           /**< 테스트 함수 */
    bool passed;                   /**< 통과 여부 */
    char error_message[256];       /**< 오류 메시지 */
} TestCase;

/**
 * @brief 테스트 스위트 구조체
 */
typedef struct {
    char name[64];                 /**< 스위트 이름 */
    TestCase* test_cases;          /**< 테스트 케이스 배열 */
    int num_test_cases;            /**< 테스트 케이스 수 */
    int capacity;                  /**< 배열 용량 */
    int passed_count;              /**< 통과한 테스트 수 */
    int failed_count;              /**< 실패한 테스트 수 */
} TestSuite;

// ============================================================================
// 테스트 스위트 관리 함수
// ============================================================================

/**
 * @brief 테스트 스위트를 생성합니다
 */
TestSuite* test_suite_create(const char* name);

/**
 * @brief 테스트 스위트를 해제합니다
 */
void test_suite_destroy(TestSuite* suite);

/**
 * @brief 테스트 케이스를 추가합니다
 */
int test_suite_add_case(TestSuite* suite, const char* name,
                       void (*setup)(), void (*teardown)(), void (*test_func)());

/**
 * @brief 테스트 스위트를 실행합니다
 */
void test_suite_run(TestSuite* suite);

// ============================================================================
// 테스트 결과 관리 함수
// ============================================================================

/**
 * @brief 모든 테스트 결과를 출력합니다
 */
void test_print_summary();

/**
 * @brief 테스트 실패를 기록합니다
 */
void test_fail(const char* message);

/**
 * @brief 테스트 성공을 기록합니다
 */
void test_pass();

/**
 * @brief 전체 테스트 실행 결과를 반환합니다
 */
int test_get_exit_code();

// ============================================================================
// 어설션 매크로
// ============================================================================

/**
 * @brief 조건이 참인지 확인합니다
 */
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Assertion failed: %s (at %s:%d)", \
                    #condition, __FILE__, __LINE__); \
            test_fail(msg); \
            return; \
        } \
    } while(0)

/**
 * @brief 두 정수가 같은지 확인합니다
 */
#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Expected %d, but got %d (at %s:%d)", \
                    (expected), (actual), __FILE__, __LINE__); \
            test_fail(msg); \
            return; \
        } \
    } while(0)

/**
 * @brief 두 부동소수점이 거의 같은지 확인합니다
 */
#define TEST_ASSERT_EQUAL_FLOAT(expected, actual, tolerance) \
    do { \
        if (fabsf((expected) - (actual)) > (tolerance)) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Expected %f, but got %f (tolerance: %f, at %s:%d)", \
                    (expected), (actual), (tolerance), __FILE__, __LINE__); \
            test_fail(msg); \
            return; \
        } \
    } while(0)

/**
 * @brief 포인터가 NULL이 아닌지 확인합니다
 */
#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Pointer should not be NULL: %s (at %s:%d)", \
                    #ptr, __FILE__, __LINE__); \
            test_fail(msg); \
            return; \
        } \
    } while(0)

/**
 * @brief 포인터가 NULL인지 확인합니다
 */
#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Pointer should be NULL: %s (at %s:%d)", \
                    #ptr, __FILE__, __LINE__); \
            test_fail(msg); \
            return; \
        } \
    } while(0)

/**
 * @brief 문자열이 같은지 확인합니다
 */
#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Expected \"%s\", but got \"%s\" (at %s:%d)", \
                    (expected), (actual), __FILE__, __LINE__); \
            test_fail(msg); \
            return; \
        } \
    } while(0)

/**
 * @brief 테스트를 성공으로 표시합니다
 */
#define TEST_PASS() \
    do { \
        test_pass(); \
    } while(0)

/**
 * @brief 테스트를 실패로 표시합니다
 */
#define TEST_FAIL(message) \
    do { \
        test_fail(message); \
        return; \
    } while(0)

// ============================================================================
// 편의 매크로
// ============================================================================

/**
 * @brief 테스트 케이스 추가 매크로
 */
#define ADD_TEST(suite, test_func) \
    test_suite_add_case(suite, #test_func, NULL, NULL, test_func)

/**
 * @brief 설정/정리 함수가 있는 테스트 케이스 추가 매크로
 */
#define ADD_TEST_WITH_SETUP(suite, test_func, setup_func, teardown_func) \
    test_suite_add_case(suite, #test_func, setup_func, teardown_func, test_func)

#ifdef __cplusplus
}
#endif

#endif // TEST_FRAMEWORK_H