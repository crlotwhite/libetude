/**
 * @file test_math_simple.c
 * @brief 기본 수학 함수들에 대한 단위 테스트
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <float.h>

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/error.h"

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, epsilon, message) \
    do { \
        if (fabs((a) - (b)) > (epsilon)) { \
            printf("FAIL: %s (expected: %f, got: %f)\n", message, (double)(b), (double)(a)); \
            return 0; \
        } \
    } while(0)

/**
 * @brief 기본 산술 연산 테스트
 */
static int test_basic_arithmetic() {
    printf("Testing basic arithmetic operations...\n");

    // 덧셈 테스트
    float a = 2.5f;
    float b = 3.7f;
    float result = a + b;
    TEST_ASSERT_FLOAT_EQ(result, 6.2f, 0.001f, "Addition test");

    // 뺄셈 테스트
    result = a - b;
    TEST_ASSERT_FLOAT_EQ(result, -1.2f, 0.001f, "Subtraction test");

    // 곱셈 테스트
    result = a * b;
    TEST_ASSERT_FLOAT_EQ(result, 9.25f, 0.001f, "Multiplication test");

    // 나눗셈 테스트
    result = a / b;
    TEST_ASSERT_FLOAT_EQ(result, 0.6756f, 0.001f, "Division test");

    printf("Basic arithmetic operations: PASS\n");
    return 1;
}

/**
 * @brief 삼각함수 테스트
 */
static int test_trigonometric_functions() {
    printf("Testing trigonometric functions...\n");

    const float pi = 3.14159265359f;

    // sin 테스트
    float result = sinf(pi / 2.0f);
    TEST_ASSERT_FLOAT_EQ(result, 1.0f, 0.001f, "sin(π/2) test");

    result = sinf(0.0f);
    TEST_ASSERT_FLOAT_EQ(result, 0.0f, 0.001f, "sin(0) test");

    // cos 테스트
    result = cosf(0.0f);
    TEST_ASSERT_FLOAT_EQ(result, 1.0f, 0.001f, "cos(0) test");

    result = cosf(pi / 2.0f);
    TEST_ASSERT_FLOAT_EQ(result, 0.0f, 0.001f, "cos(π/2) test");

    // tan 테스트
    result = tanf(pi / 4.0f);
    TEST_ASSERT_FLOAT_EQ(result, 1.0f, 0.001f, "tan(π/4) test");

    printf("Trigonometric functions: PASS\n");
    return 1;
}

/**
 * @brief 지수 및 로그 함수 테스트
 */
static int test_exponential_functions() {
    printf("Testing exponential and logarithmic functions...\n");

    // exp 테스트
    float result = expf(0.0f);
    TEST_ASSERT_FLOAT_EQ(result, 1.0f, 0.001f, "exp(0) test");

    result = expf(1.0f);
    TEST_ASSERT_FLOAT_EQ(result, 2.71828f, 0.001f, "exp(1) test");

    // log 테스트
    result = logf(1.0f);
    TEST_ASSERT_FLOAT_EQ(result, 0.0f, 0.001f, "log(1) test");

    result = logf(2.71828f);
    TEST_ASSERT_FLOAT_EQ(result, 1.0f, 0.001f, "log(e) test");

    // pow 테스트
    result = powf(2.0f, 3.0f);
    TEST_ASSERT_FLOAT_EQ(result, 8.0f, 0.001f, "pow(2,3) test");

    result = powf(4.0f, 0.5f);
    TEST_ASSERT_FLOAT_EQ(result, 2.0f, 0.001f, "pow(4,0.5) test");

    printf("Exponential and logarithmic functions: PASS\n");
    return 1;
}

/**
 * @brief 제곱근 및 절댓값 함수 테스트
 */
static int test_utility_functions() {
    printf("Testing utility functions...\n");

    // sqrt 테스트
    float result = sqrtf(4.0f);
    TEST_ASSERT_FLOAT_EQ(result, 2.0f, 0.001f, "sqrt(4) test");

    result = sqrtf(9.0f);
    TEST_ASSERT_FLOAT_EQ(result, 3.0f, 0.001f, "sqrt(9) test");

    // fabs 테스트
    result = fabsf(-5.5f);
    TEST_ASSERT_FLOAT_EQ(result, 5.5f, 0.001f, "fabs(-5.5) test");

    result = fabsf(3.2f);
    TEST_ASSERT_FLOAT_EQ(result, 3.2f, 0.001f, "fabs(3.2) test");

    // floor 테스트
    result = floorf(3.7f);
    TEST_ASSERT_FLOAT_EQ(result, 3.0f, 0.001f, "floor(3.7) test");

    result = floorf(-2.3f);
    TEST_ASSERT_FLOAT_EQ(result, -3.0f, 0.001f, "floor(-2.3) test");

    // ceil 테스트
    result = ceilf(3.2f);
    TEST_ASSERT_FLOAT_EQ(result, 4.0f, 0.001f, "ceil(3.2) test");

    result = ceilf(-2.7f);
    TEST_ASSERT_FLOAT_EQ(result, -2.0f, 0.001f, "ceil(-2.7) test");

    printf("Utility functions: PASS\n");
    return 1;
}

/**
 * @brief 특수 값 처리 테스트
 */
static int test_special_values() {
    printf("Testing special values...\n");

    // NaN 테스트
    float nan_val = sqrtf(-1.0f);
    TEST_ASSERT(isnan(nan_val), "NaN detection test");

    // 무한대 테스트
    float inf_val = 1.0f / 0.0f;
    TEST_ASSERT(isinf(inf_val), "Infinity detection test");

    // 0으로 나누기 처리
    float zero_div = logf(0.0f);
    TEST_ASSERT(isinf(zero_div) && zero_div < 0, "log(0) = -inf test");

    printf("Special values: PASS\n");
    return 1;
}

/**
 * @brief 메인 테스트 함수
 */
int main() {
    printf("=== Math Simple Test Suite ===\n");

    int tests_passed = 0;
    int total_tests = 5;

    if (test_basic_arithmetic()) tests_passed++;
    if (test_trigonometric_functions()) tests_passed++;
    if (test_exponential_functions()) tests_passed++;
    if (test_utility_functions()) tests_passed++;
    if (test_special_values()) tests_passed++;

    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);

    if (tests_passed == total_tests) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}