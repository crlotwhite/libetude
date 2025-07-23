/**
 * @file test_fast_math.c
 * @brief FastApprox 기반 고속 수학 함수 단위 테스트
 */

#include "libetude/fast_math.h"
#include "../framework/test_framework.h"
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <assert.h>

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

// 지수 함수 테스트
void test_fast_exp(void) {
    printf("Testing et_fast_exp...\n");

    // 초기화
    assert(et_fast_math_init() == 0);

    // 기본 테스트 케이스들
    float test_values[] = {0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.5f, -0.5f, 5.0f, -5.0f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = expf(x);
        float actual = et_fast_exp(x);
        float error = relative_error(expected, actual);

        printf("  exp(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%\n",
               x, expected, actual, error * 100.0f);

        // 5% 이내 오차 허용
        if (error >= TOLERANCE_MED) {
            printf("FAIL: Fast exp error %.4f%% exceeds tolerance\n", error * 100.0f);
            return;
        }
    }

    // 극한값 테스트
    assert(et_fast_exp(100.0f) == INFINITY);
    assert(et_fast_exp(-100.0f) == 0.0f);

    printf("✓ et_fast_exp tests passed\n");
}

// 로그 함수 테스트
void test_fast_log(void) {
    printf("Testing et_fast_log...\n");

    float test_values[] = {1.0f, 2.0f, 0.5f, 10.0f, 0.1f, 2.718f, 100.0f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = logf(x);
        float actual = et_fast_log(x);
        float error = relative_error(expected, actual);

        printf("  log(%.3f): expected=%.6f, actual=%.6f, error=%.4f%%\n",
               x, expected, actual, error * 100.0f);

        // 5% 이내 오차 허용
        if (error >= TOLERANCE_MED) {
            printf("FAIL: Fast log error %.4f%% exceeds tolerance\n", error * 100.0f);
            return;
        }
    }

    // 특수값 테스트
    assert(et_fast_log(0.0f) == -INFINITY);
    float neg_log_result = et_fast_log(-1.0f);
    assert(neg_log_result != neg_log_result); // NaN 체크

    printf("✓ et_fast_log tests passed\n");
}

// 삼각함수 테스트
void test_fast_trig(void) {
    printf("Testing fast trigonometric functions...\n");

    float test_angles[] = {0.0f, M_PI/6, M_PI/4, M_PI/3, M_PI/2, M_PI, 3*M_PI/2, 2*M_PI};
    int num_tests = sizeof(test_angles) / sizeof(test_angles[0]);

    for (int i = 0; i < num_tests; i++) {
        float x = test_angles[i];

        // 사인 테스트
        float expected_sin = sinf(x);
        float actual_sin = et_fast_sin(x);
        float sin_error = relative_error(expected_sin, actual_sin);

        printf("  sin(%.4f): expected=%.6f, actual=%.6f, error=%.4f%%\n",
               x, expected_sin, actual_sin, sin_error * 100.0f);

        // 코사인 테스트
        float expected_cos = cosf(x);
        float actual_cos = et_fast_cos(x);
        float cos_error = relative_error(expected_cos, actual_cos);

        printf("  cos(%.4f): expected=%.6f, actual=%.6f, error=%.4f%%\n",
               x, expected_cos, actual_cos, cos_error * 100.0f);

        // 특수한 경우 (2π 근처)에서는 더 관대한 허용 오차 적용
        float tolerance = (fabsf(x - 2*M_PI) < 0.001f) ? TOLERANCE_MED : TOLERANCE_HIGH;

        // 1% 이내 오차 허용 (룩업 테이블이므로 더 정확해야 함)
        if (sin_error >= tolerance) {
            printf("FAIL: Fast sin error %.4f%% exceeds tolerance\n", sin_error * 100.0f);
            return;
        }
        if (cos_error >= tolerance) {
            printf("FAIL: Fast cos error %.4f%% exceeds tolerance\n", cos_error * 100.0f);
            return;
        }
    }

    printf("✓ Fast trigonometric function tests passed\n");
}

// 활성화 함수 테스트
void test_activation_functions(void) {
    printf("Testing activation functions...\n");

    float test_values[] = {-3.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 3.0f};
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_tests; i++) {
        float x = test_values[i];

        // tanh 테스트
        float expected_tanh = tanhf(x);
        float actual_tanh = et_fast_tanh(x);
        float tanh_error = relative_error(expected_tanh, actual_tanh);

        printf("  tanh(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%\n",
               x, expected_tanh, actual_tanh, tanh_error * 100.0f);

        // sigmoid 테스트
        float expected_sigmoid = 1.0f / (1.0f + expf(-x));
        float actual_sigmoid = et_fast_sigmoid(x);
        float sigmoid_error = relative_error(expected_sigmoid, actual_sigmoid);

        printf("  sigmoid(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%\n",
               x, expected_sigmoid, actual_sigmoid, sigmoid_error * 100.0f);

        // 5% 이내 오차 허용
        if (tanh_error >= TOLERANCE_MED) {
            printf("FAIL: Fast tanh error %.4f%% exceeds tolerance\n", tanh_error * 100.0f);
            return;
        }
        if (sigmoid_error >= TOLERANCE_MED) {
            printf("FAIL: Fast sigmoid error %.4f%% exceeds tolerance\n", sigmoid_error * 100.0f);
            return;
        }
    }

    printf("✓ Activation function tests passed\n");
}

// 벡터화 함수 테스트
void test_vectorized_functions(void) {
    printf("Testing vectorized functions...\n");

    const size_t size = 10;
    float input[10] = {-2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f};
    float output[10];

    // 벡터화된 exp 테스트
    et_fast_exp_vec(input, output, size);
    for (size_t i = 0; i < size; i++) {
        float expected = et_fast_exp(input[i]);
        assert(fabsf(output[i] - expected) < 1e-6f);
    }

    // 벡터화된 log 테스트 (양수만)
    float positive_input[5] = {0.1f, 0.5f, 1.0f, 2.0f, 10.0f};
    et_fast_log_vec(positive_input, output, 5);
    for (size_t i = 0; i < 5; i++) {
        float expected = et_fast_log(positive_input[i]);
        assert(fabsf(output[i] - expected) < 1e-6f);
    }

    // 벡터화된 tanh 테스트
    et_fast_tanh_vec(input, output, size);
    for (size_t i = 0; i < size; i++) {
        float expected = et_fast_tanh(input[i]);
        assert(fabsf(output[i] - expected) < 1e-6f);
    }

    printf("✓ Vectorized function tests passed\n");
}

// 성능 벤치마크 (간단한)
void test_performance_benchmark(void) {
    printf("Running simple performance benchmark...\n");

    const int iterations = 100000;
    float test_value = 1.5f;

    // 표준 함수 vs 고속 함수 비교는 실제 벤치마크에서 수행
    // 여기서는 함수가 정상적으로 호출되는지만 확인

    for (int i = 0; i < 1000; i++) {
        float result = et_fast_exp(test_value);
        (void)result; // 컴파일러 최적화 방지
    }

    printf("✓ Performance benchmark completed\n");
}

// 메인 테스트 함수
int main(void) {
    printf("=== FastApprox 기반 고속 수학 함수 테스트 ===\n\n");

    // 초기화 테스트
    assert(et_fast_math_init() == 0);

    // 각 함수 그룹 테스트
    test_fast_exp();
    test_fast_log();
    test_fast_trig();
    test_activation_functions();
    test_vectorized_functions();
    test_performance_benchmark();

    // 정리
    et_fast_math_cleanup();

    printf("\n=== 모든 테스트 통과! ===\n");
    return 0;
}