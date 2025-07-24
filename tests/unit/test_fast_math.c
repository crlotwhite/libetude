/**
 * @file test_fast_math.c
 * @brief FastApprox 기반 고속 수학 함수 단위 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/fast_math.h"
#include <math.h>
#include <float.h>

// 테스트 허용 오차
#define TOLERANCE_HIGH 0.01f    // 1% 오차
#define TOLERANCE_MED 0.05f     // 5% 오차
#define TOLERANCE_LOW 0.1f      // 10% 오차

// 상대 오차 계산 함수
static float relative_error(float expected, float actual) {
    if (fabsf(expected) < FLT_EPSILON) {
        return fabsf(actual);
    }
    return fabsf((actual - expected) / expected);
}

// 테스트 설정 함수
void setUp(void) {
    TEST_ASSERT_EQUAL(0, et_fast_math_init());
}

// 테스트 정리 함수
void tearDown(void) {
    et_fast_math_cleanup();
}

// 초기화 테스트
void test_fast_math_initialization(void) {
    // 여러 번 초기화해도 안전해야 함
    TEST_ASSERT_EQUAL(0, et_fast_math_init());
    TEST_ASSERT_EQUAL(0, et_fast_math_init());
}

// 지수 함수 기본값 테스트
void test_exponential_basic_values(void) {
    float test_values[] = {0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.5f, -0.5f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = expf(x);
        float actual = et_fast_exp(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "exp(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 지수 함수 극한값 테스트
void test_exponential_extreme_values(void) {
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, et_fast_exp(100.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, et_fast_exp(-100.0f));
}

// 지수 함수 경계값 테스트
void test_exponential_edge_cases(void) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_HIGH, 1.0f, et_fast_exp(0.0f));

    float expected_e = expf(1.0f);
    float actual_e = et_fast_exp(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(expected_e * TOLERANCE_MED, expected_e, actual_e);
}

// 로그 함수 기본값 테스트
void test_logarithm_basic_values(void) {
    float test_values[] = {1.0f, 2.0f, 0.5f, 10.0f, 0.1f, 2.718f, 100.0f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = logf(x);
        float actual = et_fast_log(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "log(%.3f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 로그 함수 특수값 테스트
void test_logarithm_special_values(void) {
    TEST_ASSERT_EQUAL_FLOAT(-INFINITY, et_fast_log(0.0f));
    TEST_ASSERT_TRUE(isnan(et_fast_log(-1.0f)));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_HIGH, 0.0f, et_fast_log(1.0f));
}

// 사인 함수 테스트
void test_sine_function(void) {
    float test_angles[] = {0.0f, M_PI/6, M_PI/4, M_PI/3, M_PI/2, M_PI, 3*M_PI/2, 2*M_PI};
    size_t num_tests = sizeof(test_angles) / sizeof(test_angles[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_angles[i];
        float expected = sinf(x);
        float actual = et_fast_sin(x);
        float error = relative_error(expected, actual);

        // 특수한 경우 (2π 근처)에서는 더 관대한 허용 오차 적용
        float tolerance = (fabsf(x - 2*M_PI) < 0.001f) ? TOLERANCE_MED : TOLERANCE_HIGH;

        char msg[256];
        snprintf(msg, sizeof(msg),
                "sin(%.4f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < tolerance, msg);
    }
}

// 코사인 함수 테스트
void test_cosine_function(void) {
    float test_angles[] = {0.0f, M_PI/6, M_PI/4, M_PI/3, M_PI/2, M_PI, 3*M_PI/2, 2*M_PI};
    size_t num_tests = sizeof(test_angles) / sizeof(test_angles[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_angles[i];
        float expected = cosf(x);
        float actual = et_fast_cos(x);
        float error = relative_error(expected, actual);

        float tolerance = (fabsf(x - 2*M_PI) < 0.001f) ? TOLERANCE_MED : TOLERANCE_HIGH;

        char msg[256];
        snprintf(msg, sizeof(msg),
                "cos(%.4f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < tolerance, msg);
    }
}

// 삼각함수 항등식 테스트
void test_trigonometric_identities(void) {
    float x = M_PI / 4;
    float sin_val = et_fast_sin(x);
    float cos_val = et_fast_cos(x);

    // sin²(x) + cos²(x) = 1
    float identity_result = sin_val * sin_val + cos_val * cos_val;
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_HIGH, 1.0f, identity_result);
}

// 하이퍼볼릭 탄젠트 테스트
void test_hyperbolic_tangent(void) {
    float test_values[] = {-3.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 3.0f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = tanhf(x);
        float actual = et_fast_tanh(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "tanh(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 시그모이드 함수 테스트
void test_sigmoid_function(void) {
    float test_values[] = {-3.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 3.0f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = 1.0f / (1.0f + expf(-x));
        float actual = et_fast_sigmoid(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "sigmoid(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 활성화 함수 속성 테스트
void test_activation_function_properties(void) {
    // Sigmoid는 항상 (0, 1) 범위
    TEST_ASSERT_TRUE(et_fast_sigmoid(-10.0f) > 0.0f);
    TEST_ASSERT_TRUE(et_fast_sigmoid(-10.0f) < 1.0f);
    TEST_ASSERT_TRUE(et_fast_sigmoid(10.0f) > 0.0f);
    TEST_ASSERT_TRUE(et_fast_sigmoid(10.0f) < 1.0f);

    // Tanh는 항상 (-1, 1) 범위
    TEST_ASSERT_TRUE(et_fast_tanh(-10.0f) > -1.0f);
    TEST_ASSERT_TRUE(et_fast_tanh(-10.0f) < 1.0f);
    TEST_ASSERT_TRUE(et_fast_tanh(10.0f) > -1.0f);
    TEST_ASSERT_TRUE(et_fast_tanh(10.0f) < 1.0f);
}

// 벡터화된 지수 함수 테스트
void test_vectorized_exponential(void) {
    const size_t size = 10;
    float input[10] = {-2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f};
    float output[10];

    et_fast_exp_vec(input, output, size);

    for (size_t i = 0; i < size; i++) {
        float expected = et_fast_exp(input[i]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, output[i]);
    }
}

// 벡터화된 로그 함수 테스트
void test_vectorized_logarithm(void) {
    float positive_input[5] = {0.1f, 0.5f, 1.0f, 2.0f, 10.0f};
    float output[5];

    et_fast_log_vec(positive_input, output, 5);

    for (size_t i = 0; i < 5; i++) {
        float expected = et_fast_log(positive_input[i]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, output[i]);
    }
}

// 벡터화된 하이퍼볼릭 탄젠트 테스트
void test_vectorized_hyperbolic_tangent(void) {
    const size_t size = 10;
    float input[10] = {-2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f};
    float output[10];

    et_fast_tanh_vec(input, output, size);

    for (size_t i = 0; i < size; i++) {
        float expected = et_fast_tanh(input[i]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, output[i]);
    }
}

// 성능 특성 테스트
void test_performance_characteristics(void) {
    const int iterations = 1000;
    float test_value = 1.5f;

    // 단순히 함수가 정상적으로 호출되는지 확인
    for (int i = 0; i < iterations; i++) {
        volatile float result = et_fast_exp(test_value);
        (void)result; // 컴파일러 최적화 방지
    }

    // 크래시 없이 완료되면 성공
    TEST_PASS();
}

// 정리 안전성 테스트
void test_cleanup_safety(void) {
    // 여러 번 호출해도 안전해야 함
    et_fast_math_cleanup();
    et_fast_math_cleanup();
    et_fast_math_cleanup();

    // 크래시 없이 완료되면 성공
    TEST_PASS();
}

// Unity 테스트 러너
int main(void) {
    UNITY_BEGIN();

    // 초기화 테스트
    RUN_TEST(test_fast_math_initialization);

    // 지수 함수 테스트
    RUN_TEST(test_exponential_basic_values);
    RUN_TEST(test_exponential_extreme_values);
    RUN_TEST(test_exponential_edge_cases);

    // 로그 함수 테스트
    RUN_TEST(test_logarithm_basic_values);
    RUN_TEST(test_logarithm_special_values);

    // 삼각함수 테스트
    RUN_TEST(test_sine_function);
    RUN_TEST(test_cosine_function);
    RUN_TEST(test_trigonometric_identities);

    // 활성화 함수 테스트
    RUN_TEST(test_hyperbolic_tangent);
    RUN_TEST(test_sigmoid_function);
    RUN_TEST(test_activation_function_properties);

    // 벡터화된 함수 테스트
    RUN_TEST(test_vectorized_exponential);
    RUN_TEST(test_vectorized_logarithm);
    RUN_TEST(test_vectorized_hyperbolic_tangent);

    // 성능 및 안전성 테스트
    RUN_TEST(test_performance_characteristics);
    RUN_TEST(test_cleanup_safety);

    return UNITY_END();
}