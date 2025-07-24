/**
 * @file test_simd_math_functions.c
 * @brief SIMD 벡터화된 수학 함수 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "libetude/simd_kernels.h"
#include "libetude/kernel_registry.h"
#include "libetude/types.h"

// 테스트 허용 오차
#define TEST_EPSILON 1e-5f
#define TEST_LARGE_EPSILON 1e-3f  // 근사 함수용

// 테스트 유틸리티 함수들
static int float_equals(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

static void print_test_result(const char* test_name, int passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

// 테스트 데이터 생성
static void generate_test_data(float* data, size_t size, float min_val, float max_val) {
    for (size_t i = 0; i < size; i++) {
        float t = (float)i / (float)(size - 1);
        data[i] = min_val + t * (max_val - min_val);
    }
}

// 활성화 함수 테스트들
static int test_simd_sigmoid() {
    const size_t size = 128;
    float input[128];
    float output[128];
    float expected[128];

    // 테스트 데이터 생성 (-10 ~ 10 범위)
    generate_test_data(input, size, -10.0f, 10.0f);

    // 예상 결과 계산 (표준 라이브러리 사용)
    for (size_t i = 0; i < size; i++) {
        expected[i] = 1.0f / (1.0f + expf(-input[i]));
    }

    // SIMD 최적화된 sigmoid 함수 테스트
    simd_sigmoid_optimal(input, output, size);

    // 결과 비교
    int passed = 1;
    for (size_t i = 0; i < size; i++) {
        if (!float_equals(output[i], expected[i], TEST_LARGE_EPSILON)) {
            printf("Sigmoid mismatch at index %zu: got %f, expected %f\n",
                   i, output[i], expected[i]);
            passed = 0;
            break;
        }
    }

    return passed;
}

static int test_simd_tanh() {
    const size_t size = 128;
    float input[128];
    float output[128];
    float expected[128];

    // 테스트 데이터 생성 (-5 ~ 5 범위)
    generate_test_data(input, size, -5.0f, 5.0f);

    // 예상 결과 계산
    for (size_t i = 0; i < size; i++) {
        expected[i] = tanhf(input[i]);
    }

    // SIMD 최적화된 tanh 함수 테스트
    simd_tanh_optimal(input, output, size);

    // 결과 비교
    int passed = 1;
    for (size_t i = 0; i < size; i++) {
        if (!float_equals(output[i], expected[i], TEST_LARGE_EPSILON)) {
            printf("Tanh mismatch at index %zu: got %f, expected %f\n",
                   i, output[i], expected[i]);
            passed = 0;
            break;
        }
    }

    return passed;
}

static int test_simd_gelu() {
    const size_t size = 128;
    float input[128];
    float output[128];
    float expected[128];

    // 테스트 데이터 생성 (-3 ~ 3 범위)
    generate_test_data(input, size, -3.0f, 3.0f);

    // 예상 결과 계산 (GELU 공식)
    const float sqrt_2_over_pi = 0.7978845608f;
    const float coeff = 0.044715f;

    for (size_t i = 0; i < size; i++) {
        float x = input[i];
        float x3 = x * x * x;
        float inner = sqrt_2_over_pi * (x + coeff * x3);
        expected[i] = 0.5f * x * (1.0f + tanhf(inner));
    }

    // SIMD 최적화된 GELU 함수 테스트
    simd_gelu_optimal(input, output, size);

    // 결과 비교
    int passed = 1;
    for (size_t i = 0; i < size; i++) {
        if (!float_equals(output[i], expected[i], TEST_LARGE_EPSILON)) {
            printf("GELU mismatch at index %zu: got %f, expected %f\n",
                   i, output[i], expected[i]);
            passed = 0;
            break;
        }
    }

    return passed;
}

static int test_simd_softmax() {
    const size_t size = 64;
    float input[64];
    float output[64];
    float expected[64];

    // 테스트 데이터 생성 (-5 ~ 5 범위)
    generate_test_data(input, size, -5.0f, 5.0f);

    // 예상 결과 계산 (수치적으로 안정한 소프트맥스)
    // 1. 최댓값 찾기
    float max_val = input[0];
    for (size_t i = 1; i < size; i++) {
        if (input[i] > max_val) {
            max_val = input[i];
        }
    }

    // 2. exp(x - max) 계산 및 합계
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        expected[i] = expf(input[i] - max_val);
        sum += expected[i];
    }

    // 3. 정규화
    for (size_t i = 0; i < size; i++) {
        expected[i] /= sum;
    }

    // SIMD 최적화된 소프트맥스 함수 테스트
    simd_softmax_optimal(input, output, size);

    // 결과 비교
    int passed = 1;
    float output_sum = 0.0f;

    for (size_t i = 0; i < size; i++) {
        output_sum += output[i];
        if (!float_equals(output[i], expected[i], TEST_LARGE_EPSILON)) {
            printf("Softmax mismatch at index %zu: got %f, expected %f\n",
                   i, output[i], expected[i]);
            passed = 0;
            break;
        }
    }

    // 소프트맥스 출력의 합이 1인지 확인
    if (!float_equals(output_sum, 1.0f, TEST_EPSILON)) {
        printf("Softmax sum is not 1.0: got %f\n", output_sum);
        passed = 0;
    }

    return passed;
}

static int test_simd_layer_norm() {
    const size_t size = 128;
    float input[128];
    float output[128];
    float expected[128];
    const float epsilon = 1e-5f;

    // 테스트 데이터 생성 (다양한 값)
    for (size_t i = 0; i < size; i++) {
        input[i] = (float)i * 0.1f - 6.4f; // -6.4 ~ 6.3 범위
    }

    // 예상 결과 계산
    // 1. 평균 계산
    float mean = 0.0f;
    for (size_t i = 0; i < size; i++) {
        mean += input[i];
    }
    mean /= (float)size;

    // 2. 분산 계산
    float variance = 0.0f;
    for (size_t i = 0; i < size; i++) {
        float diff = input[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)size;

    // 3. 정규화
    float inv_std = 1.0f / sqrtf(variance + epsilon);
    for (size_t i = 0; i < size; i++) {
        expected[i] = (input[i] - mean) * inv_std;
    }

    // SIMD 최적화된 레이어 정규화 함수 테스트
    simd_layer_norm_optimal(input, output, size, epsilon);

    // 결과 비교
    int passed = 1;
    for (size_t i = 0; i < size; i++) {
        if (!float_equals(output[i], expected[i], TEST_EPSILON)) {
            printf("Layer norm mismatch at index %zu: got %f, expected %f\n",
                   i, output[i], expected[i]);
            passed = 0;
            break;
        }
    }

    // 정규화된 출력의 평균이 0에 가까운지 확인
    float output_mean = 0.0f;
    for (size_t i = 0; i < size; i++) {
        output_mean += output[i];
    }
    output_mean /= (float)size;

    if (!float_equals(output_mean, 0.0f, TEST_EPSILON)) {
        printf("Layer norm output mean is not close to 0: got %f\n", output_mean);
        passed = 0;
    }

    return passed;
}

static int test_simd_batch_norm() {
    const size_t size = 128;
    float input[128];
    float output[128];
    float expected[128];

    const float mean = 2.0f;
    const float variance = 4.0f;
    const float gamma = 1.5f;
    const float beta = 0.5f;
    const float epsilon = 1e-5f;

    // 테스트 데이터 생성
    generate_test_data(input, size, -5.0f, 10.0f);

    // 예상 결과 계산
    float inv_std = 1.0f / sqrtf(variance + epsilon);
    for (size_t i = 0; i < size; i++) {
        expected[i] = gamma * (input[i] - mean) * inv_std + beta;
    }

    // SIMD 최적화된 배치 정규화 함수 테스트
    simd_batch_norm_optimal(input, output, size, mean, variance, gamma, beta, epsilon);

    // 결과 비교
    int passed = 1;
    for (size_t i = 0; i < size; i++) {
        if (!float_equals(output[i], expected[i], TEST_EPSILON)) {
            printf("Batch norm mismatch at index %zu: got %f, expected %f\n",
                   i, output[i], expected[i]);
            passed = 0;
            break;
        }
    }

    return passed;
}

// 성능 테스트
static void performance_test_sigmoid() {
    const size_t size = 10000;
    float* input = (float*)malloc(size * sizeof(float));
    float* output = (float*)malloc(size * sizeof(float));

    if (!input || !output) {
        printf("Memory allocation failed for performance test\n");
        free(input);
        free(output);
        return;
    }

    // 테스트 데이터 생성
    generate_test_data(input, size, -10.0f, 10.0f);

    // 성능 측정 (간단한 반복 테스트)
    const int iterations = 1000;

    printf("Performance test: Sigmoid with %zu elements, %d iterations\n", size, iterations);

    for (int i = 0; i < iterations; i++) {
        simd_sigmoid_optimal(input, output, size);
    }

    printf("SIMD Sigmoid performance test completed\n");

    free(input);
    free(output);
}

// 메인 테스트 함수
int main() {
    printf("=== SIMD 벡터화된 수학 함수 테스트 ===\n\n");

    // 커널 시스템 초기화
    if (simd_kernels_init() != LIBETUDE_SUCCESS) {
        printf("Failed to initialize SIMD kernels\n");
        return 1;
    }

    // 현재 사용 가능한 SIMD 기능 출력
    uint32_t features = simd_kernels_get_features();
    printf("Available SIMD features: 0x%08X\n", features);
    simd_kernels_print_info();
    printf("\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 활성화 함수 테스트
    printf("=== 활성화 함수 테스트 ===\n");

    total_tests++;
    if (test_simd_sigmoid()) {
        passed_tests++;
    }
    print_test_result("SIMD Sigmoid", test_simd_sigmoid());

    total_tests++;
    if (test_simd_tanh()) {
        passed_tests++;
    }
    print_test_result("SIMD Tanh", test_simd_tanh());

    total_tests++;
    if (test_simd_gelu()) {
        passed_tests++;
    }
    print_test_result("SIMD GELU", test_simd_gelu());

    // 정규화 함수 테스트
    printf("\n=== 정규화 함수 테스트 ===\n");

    total_tests++;
    if (test_simd_softmax()) {
        passed_tests++;
    }
    print_test_result("SIMD Softmax", test_simd_softmax());

    total_tests++;
    if (test_simd_layer_norm()) {
        passed_tests++;
    }
    print_test_result("SIMD Layer Normalization", test_simd_layer_norm());

    total_tests++;
    if (test_simd_batch_norm()) {
        passed_tests++;
    }
    print_test_result("SIMD Batch Normalization", test_simd_batch_norm());

    // 성능 테스트
    printf("\n=== 성능 테스트 ===\n");
    performance_test_sigmoid();

    // 결과 요약
    printf("\n=== 테스트 결과 요약 ===\n");
    printf("총 테스트: %d\n", total_tests);
    printf("통과: %d\n", passed_tests);
    printf("실패: %d\n", total_tests - passed_tests);
    printf("성공률: %.1f%%\n", (float)passed_tests / total_tests * 100.0f);

    // 정리
    simd_kernels_finalize();

    return (passed_tests == total_tests) ? 0 : 1;
}