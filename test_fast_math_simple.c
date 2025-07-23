/**
 * @file test_fast_math_simple.c
 * @brief FastApprox 기반 고속 수학 함수 간단 테스트
 */

#include "include/libetude/fast_math.h"
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <float.h>

// 상대 오차 계산 함수
static float relative_error(float expected, float actual) {
    if (fabsf(expected) < FLT_EPSILON) {
        return fabsf(actual);
    }
    return fabsf((actual - expected) / expected);
}

int main(void) {
    printf("=== FastApprox 기반 고속 수학 함수 간단 테스트 ===\n\n");

    // 초기화
    if (et_fast_math_init() != 0) {
        printf("ERROR: 고속 수학 함수 초기화 실패\n");
        return 1;
    }

    printf("1. 지수 함수 테스트\n");
    float exp_test_values[] = {0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.5f, -0.5f};
    int num_exp_tests = sizeof(exp_test_values) / sizeof(exp_test_values[0]);

    for (int i = 0; i < num_exp_tests; i++) {
        float x = exp_test_values[i];
        float expected = expf(x);
        float actual = et_fast_exp(x);
        float error = relative_error(expected, actual);

        printf("  exp(%.2f): 표준=%.6f, 고속=%.6f, 오차=%.2f%%\n",
               x, expected, actual, error * 100.0f);

        if (error > 0.05f) { // 5% 이상 오차
            printf("  WARNING: 오차가 큽니다!\n");
        }
    }

    printf("\n2. 로그 함수 테스트\n");
    float log_test_values[] = {1.0f, 2.0f, 0.5f, 10.0f, 0.1f, 2.718f};
    int num_log_tests = sizeof(log_test_values) / sizeof(log_test_values[0]);

    for (int i = 0; i < num_log_tests; i++) {
        float x = log_test_values[i];
        float expected = logf(x);
        float actual = et_fast_log(x);
        float error = relative_error(expected, actual);

        printf("  log(%.3f): 표준=%.6f, 고속=%.6f, 오차=%.2f%%\n",
               x, expected, actual, error * 100.0f);

        if (error > 0.05f) { // 5% 이상 오차
            printf("  WARNING: 오차가 큽니다!\n");
        }
    }

    printf("\n3. 삼각함수 테스트\n");
    float trig_test_values[] = {0.0f, M_PI/6, M_PI/4, M_PI/3, M_PI/2, M_PI};
    int num_trig_tests = sizeof(trig_test_values) / sizeof(trig_test_values[0]);

    for (int i = 0; i < num_trig_tests; i++) {
        float x = trig_test_values[i];

        // 사인 테스트
        float expected_sin = sinf(x);
        float actual_sin = et_fast_sin(x);
        float sin_error = relative_error(expected_sin, actual_sin);

        printf("  sin(%.4f): 표준=%.6f, 고속=%.6f, 오차=%.2f%%\n",
               x, expected_sin, actual_sin, sin_error * 100.0f);

        // 코사인 테스트
        float expected_cos = cosf(x);
        float actual_cos = et_fast_cos(x);
        float cos_error = relative_error(expected_cos, actual_cos);

        printf("  cos(%.4f): 표준=%.6f, 고속=%.6f, 오차=%.2f%%\n",
               x, expected_cos, actual_cos, cos_error * 100.0f);
    }

    printf("\n4. 활성화 함수 테스트\n");
    float activation_test_values[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    int num_activation_tests = sizeof(activation_test_values) / sizeof(activation_test_values[0]);

    for (int i = 0; i < num_activation_tests; i++) {
        float x = activation_test_values[i];

        // tanh 테스트
        float expected_tanh = tanhf(x);
        float actual_tanh = et_fast_tanh(x);
        float tanh_error = relative_error(expected_tanh, actual_tanh);

        printf("  tanh(%.1f): 표준=%.6f, 고속=%.6f, 오차=%.2f%%\n",
               x, expected_tanh, actual_tanh, tanh_error * 100.0f);

        // sigmoid 테스트
        float expected_sigmoid = 1.0f / (1.0f + expf(-x));
        float actual_sigmoid = et_fast_sigmoid(x);
        float sigmoid_error = relative_error(expected_sigmoid, actual_sigmoid);

        printf("  sigmoid(%.1f): 표준=%.6f, 고속=%.6f, 오차=%.2f%%\n",
               x, expected_sigmoid, actual_sigmoid, sigmoid_error * 100.0f);

        // GELU 테스트
        float actual_gelu = et_fast_gelu(x);
        printf("  gelu(%.1f): 고속=%.6f\n", x, actual_gelu);
    }

    printf("\n5. 벡터화 함수 테스트\n");
    const size_t vec_size = 5;
    float input[5] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    float output[5];

    // 벡터화된 exp 테스트
    et_fast_exp_vec(input, output, vec_size);
    printf("  벡터화된 exp:\n");
    for (size_t i = 0; i < vec_size; i++) {
        printf("    exp(%.1f) = %.6f\n", input[i], output[i]);
    }

    // 벡터화된 tanh 테스트
    et_fast_tanh_vec(input, output, vec_size);
    printf("  벡터화된 tanh:\n");
    for (size_t i = 0; i < vec_size; i++) {
        printf("    tanh(%.1f) = %.6f\n", input[i], output[i]);
    }

    // 정리
    et_fast_math_cleanup();

    printf("\n=== 테스트 완료! ===\n");
    return 0;
}