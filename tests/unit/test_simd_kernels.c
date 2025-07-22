/**
 * @file test_simd_kernels.c
 * @brief SIMD 커널 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/simd_kernels.h"
#include "libetude/kernel_registry.h"
#include "../framework/test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

// 테스트 허용 오차
#define TEST_EPSILON 1e-5f
#define TEST_LARGE_EPSILON 1e-3f // 근사 함수용

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * @brief 두 부동소수점 값이 거의 같은지 확인
 */
static bool float_equals(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

/**
 * @brief 두 벡터가 거의 같은지 확인
 */
static bool vector_equals(const float* a, const float* b, size_t size, float epsilon) {
    for (size_t i = 0; i < size; i++) {
        if (!float_equals(a[i], b[i], epsilon)) {
            printf("Mismatch at index %zu: %f vs %f (diff: %f)\n",
                   i, a[i], b[i], fabsf(a[i] - b[i]));
            return false;
        }
    }
    return true;
}

/**
 * @brief 테스트 벡터 생성
 */
static void generate_test_vector(float* vec, size_t size, float min_val, float max_val) {
    for (size_t i = 0; i < size; i++) {
        float t = (float)i / (float)(size - 1);
        vec[i] = min_val + t * (max_val - min_val);
    }
}

// ============================================================================
// 벡터 연산 테스트
// ============================================================================

/**
 * @brief 벡터 덧셈 테스트
 */
static void test_vector_add(void) {
    printf("Testing vector addition...\n");

    const size_t sizes[] = {16, 64, 128, 1000, 1023}; // 다양한 크기 테스트
    const size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; s++) {
        size_t size = sizes[s];

        float* a = (float*)malloc(size * sizeof(float));
        float* b = (float*)malloc(size * sizeof(float));
        float* result_simd = (float*)malloc(size * sizeof(float));
        float* result_ref = (float*)malloc(size * sizeof(float));

        // 테스트 데이터 생성
        generate_test_vector(a, size, -10.0f, 10.0f);
        generate_test_vector(b, size, -5.0f, 15.0f);

        // SIMD 버전 실행
        simd_vector_add_optimal(a, b, result_simd, size);

        // 참조 구현 (CPU)
        for (size_t i = 0; i < size; i++) {
            result_ref[i] = a[i] + b[i];
        }

        // 결과 비교
        bool passed = vector_equals(result_simd, result_ref, size, TEST_EPSILON);
        printf("  Size %zu: %s\n", size, passed ? "PASS" : "FAIL");

        if (!passed) {
            TEST_FAIL("Vector addition test failed for size %zu", size);
        }

        free(a);
        free(b);
        free(result_simd);
        free(result_ref);
    }
}

/**
 * @brief 벡터 곱셈 테스트
 */
static void test_vector_mul(void) {
    printf("Testing vector multiplication...\n");

    const size_t size = 1000;
    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));
    float* result_simd = (float*)malloc(size * sizeof(float));
    float* result_ref = (float*)malloc(size * sizeof(float));

    generate_test_vector(a, size, -2.0f, 2.0f);
    generate_test_vector(b, size, -3.0f, 3.0f);

    // SIMD 버전
    simd_vector_mul_optimal(a, b, result_simd, size);

    // 참조 구현
    for (size_t i = 0; i < size; i++) {
        result_ref[i] = a[i] * b[i];
    }

    bool passed = vector_equals(result_simd, result_ref, size, TEST_EPSILON);
    printf("  Result: %s\n", passed ? "PASS" : "FAIL");

    if (!passed) {
        TEST_FAIL("Vector multiplication test failed");
    }

    free(a);
    free(b);
    free(result_simd);
    free(result_ref);
}

/**
 * @brief 벡터 내적 테스트
 */
static void test_vector_dot(void) {
    printf("Testing vector dot product...\n");

    const size_t size = 1000;
    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));

    generate_test_vector(a, size, -1.0f, 1.0f);
    generate_test_vector(b, size, -1.0f, 1.0f);

    // SIMD 버전
    float result_simd = simd_vector_dot_optimal(a, b, size);

    // 참조 구현
    float result_ref = 0.0f;
    for (size_t i = 0; i < size; i++) {
        result_ref += a[i] * b[i];
    }

    bool passed = float_equals(result_simd, result_ref, TEST_EPSILON);
    printf("  SIMD: %f, Reference: %f, Diff: %f\n",
           result_simd, result_ref, fabsf(result_simd - result_ref));
    printf("  Result: %s\n", passed ? "PASS" : "FAIL");

    if (!passed) {
        TEST_FAIL("Vector dot product test failed");
    }

    free(a);
    free(b);
}

// ============================================================================
// 행렬 연산 테스트
// ============================================================================

/**
 * @brief 행렬 곱셈 테스트
 */
static void test_matrix_mul(void) {
    printf("Testing matrix multiplication...\n");

    const size_t m = 32, n = 32, k = 32;

    float* a = (float*)malloc(m * k * sizeof(float));
    float* b = (float*)malloc(k * n * sizeof(float));
    float* c_simd = (float*)malloc(m * n * sizeof(float));
    float* c_ref = (float*)malloc(m * n * sizeof(float));

    // 테스트 데이터 생성
    for (size_t i = 0; i < m * k; i++) {
        a[i] = (float)(i % 100) / 100.0f - 0.5f;
    }
    for (size_t i = 0; i < k * n; i++) {
        b[i] = (float)(i % 100) / 100.0f - 0.5f;
    }

    // SIMD 버전
    simd_gemm_optimal(a, b, c_simd, m, n, k);

    // 참조 구현
    memset(c_ref, 0, m * n * sizeof(float));
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            for (size_t l = 0; l < k; l++) {
                c_ref[i * n + j] += a[i * k + l] * b[l * n + j];
            }
        }
    }

    bool passed = vector_equals(c_simd, c_ref, m * n, TEST_EPSILON);
    printf("  Matrix size: %zux%zux%zu\n", m, n, k);
    printf("  Result: %s\n", passed ? "PASS" : "FAIL");

    if (!passed) {
        TEST_FAIL("Matrix multiplication test failed");
    }

    free(a);
    free(b);
    free(c_simd);
    free(c_ref);
}

// ============================================================================
// 활성화 함수 테스트
// ============================================================================

/**
 * @brief ReLU 테스트
 */
static void test_relu(void) {
    printf("Testing ReLU activation...\n");

    const size_t size = 1000;
    float* input = (float*)malloc(size * sizeof(float));
    float* output_simd = (float*)malloc(size * sizeof(float));
    float* output_ref = (float*)malloc(size * sizeof(float));

    generate_test_vector(input, size, -5.0f, 5.0f);

    // SIMD 버전
    simd_relu_optimal(input, output_simd, size);

    // 참조 구현
    for (size_t i = 0; i < size; i++) {
        output_ref[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }

    bool passed = vector_equals(output_simd, output_ref, size, TEST_EPSILON);
    printf("  Result: %s\n", passed ? "PASS" : "FAIL");

    if (!passed) {
        TEST_FAIL("ReLU test failed");
    }

    free(input);
    free(output_simd);
    free(output_ref);
}

/**
 * @brief Sigmoid 테스트
 */
static void test_sigmoid(void) {
    printf("Testing Sigmoid activation...\n");

    const size_t size = 1000;
    float* input = (float*)malloc(size * sizeof(float));
    float* output_simd = (float*)malloc(size * sizeof(float));
    float* output_ref = (float*)malloc(size * sizeof(float));

    generate_test_vector(input, size, -5.0f, 5.0f);

    // SIMD 버전
    simd_sigmoid_optimal(input, output_simd, size);

    // 참조 구현
    for (size_t i = 0; i < size; i++) {
        output_ref[i] = 1.0f / (1.0f + expf(-input[i]));
    }

    // Sigmoid는 근사 함수를 사용하므로 더 큰 허용 오차 사용
    bool passed = vector_equals(output_simd, output_ref, size, TEST_LARGE_EPSILON);
    printf("  Result: %s\n", passed ? "PASS" : "FAIL");

    if (!passed) {
        // 몇 개 샘플 출력해서 차이 확인
        printf("  Sample differences:\n");
        for (size_t i = 0; i < 10 && i < size; i++) {
            printf("    [%zu] SIMD: %f, Ref: %f, Diff: %f\n",
                   i, output_simd[i], output_ref[i], fabsf(output_simd[i] - output_ref[i]));
        }
        TEST_FAIL("Sigmoid test failed");
    }

    free(input);
    free(output_simd);
    free(output_ref);
}

/**
 * @brief Tanh 테스트
 */
static void test_tanh(void) {
    printf("Testing Tanh activation...\n");

    const size_t size = 1000;
    float* input = (float*)malloc(size * sizeof(float));
    float* output_simd = (float*)malloc(size * sizeof(float));
    float* output_ref = (float*)malloc(size * sizeof(float));

    generate_test_vector(input, size, -3.0f, 3.0f);

    // SIMD 버전
    simd_tanh_optimal(input, output_simd, size);

    // 참조 구현
    for (size_t i = 0; i < size; i++) {
        output_ref[i] = tanhf(input[i]);
    }

    // Tanh도 근사 함수를 사용하므로 더 큰 허용 오차 사용
    bool passed = vector_equals(output_simd, output_ref, size, TEST_LARGE_EPSILON);
    printf("  Result: %s\n", passed ? "PASS" : "FAIL");

    if (!passed) {
        printf("  Sample differences:\n");
        for (size_t i = 0; i < 10 && i < size; i++) {
            printf("    [%zu] SIMD: %f, Ref: %f, Diff: %f\n",
                   i, output_simd[i], output_ref[i], fabsf(output_simd[i] - output_ref[i]));
        }
        TEST_FAIL("Tanh test failed");
    }

    free(input);
    free(output_simd);
    free(output_ref);
}

// ============================================================================
// 성능 테스트
// ============================================================================

/**
 * @brief 성능 벤치마크 테스트
 */
static void test_performance_benchmark(void) {
    printf("Running performance benchmarks...\n");

    const size_t size = 100000;
    const int iterations = 1000;

    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));
    float* result = (float*)malloc(size * sizeof(float));

    generate_test_vector(a, size, -1.0f, 1.0f);
    generate_test_vector(b, size, -1.0f, 1.0f);

    // 벡터 덧셈 벤치마크
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        simd_vector_add_optimal(a, b, result, size);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    double ops_per_sec = (double)(iterations * size) / elapsed;

    printf("  Vector Add: %.2f M ops/sec\n", ops_per_sec / 1e6);

    // 벡터 곱셈 벤치마크
    start = clock();
    for (int i = 0; i < iterations; i++) {
        simd_vector_mul_optimal(a, b, result, size);
    }
    end = clock();

    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    ops_per_sec = (double)(iterations * size) / elapsed;

    printf("  Vector Mul: %.2f M ops/sec\n", ops_per_sec / 1e6);

    free(a);
    free(b);
    free(result);
}

/**
 * @brief 모바일 특화 함수 테스트
 */
static void test_mobile_functions(void) {
    printf("Testing mobile-specific functions...\n");

    const size_t size = 1000;
    float* input = (float*)malloc(size * sizeof(float));
    float* output = (float*)malloc(size * sizeof(float));
    float* envelope = (float*)malloc(size * sizeof(float));

    generate_test_vector(input, size, -1.0f, 1.0f);
    generate_test_vector(envelope, size, 0.5f, 1.5f);

    // 배터리 효율적인 벡터 덧셈 테스트
    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));
    float* result_power = (float*)malloc(size * sizeof(float));
    float* result_ref = (float*)malloc(size * sizeof(float));

    generate_test_vector(a, size, -2.0f, 2.0f);
    generate_test_vector(b, size, -1.0f, 1.0f);

    simd_vector_add_power_efficient(a, b, result_power, size);

    // 참조 구현
    for (size_t i = 0; i < size; i++) {
        result_ref[i] = a[i] + b[i];
    }

    bool power_test_passed = vector_equals(result_power, result_ref, size, TEST_EPSILON);
    printf("  Power Efficient Vector Add: %s\n", power_test_passed ? "PASS" : "FAIL");

    // 온도 인식 벡터 덧셈 테스트
    float* result_thermal = (float*)malloc(size * sizeof(float));
    simd_vector_add_thermal_aware(a, b, result_thermal, size);

    bool thermal_test_passed = vector_equals(result_thermal, result_ref, size, TEST_EPSILON);
    printf("  Thermal Aware Vector Add: %s\n", thermal_test_passed ? "PASS" : "FAIL");

    // 피치 시프팅 테스트
    float* pitch_output = (float*)malloc(size * sizeof(float));
    simd_pitch_shift_mobile(input, pitch_output, size, 1.2f); // 20% 높은 피치

    // 기본적인 유효성 검사 (출력이 0이 아닌지 확인)
    bool pitch_test_passed = true;
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        sum += fabsf(pitch_output[i]);
    }
    if (sum < 1e-6f) {
        pitch_test_passed = false;
    }
    printf("  Pitch Shift Mobile: %s\n", pitch_test_passed ? "PASS" : "FAIL");

    // 스펙트럴 엔벨로프 테스트
    float* spectral_output = (float*)malloc(size * sizeof(float));
    simd_spectral_envelope_mobile(input, envelope, spectral_output, size);

    // 참조 구현과 비교
    float* spectral_ref = (float*)malloc(size * sizeof(float));
    for (size_t i = 0; i < size; i++) {
        spectral_ref[i] = input[i] * envelope[i];
    }

    bool spectral_test_passed = vector_equals(spectral_output, spectral_ref, size, TEST_EPSILON);
    printf("  Spectral Envelope Mobile: %s\n", spectral_test_passed ? "PASS" : "FAIL");

    // 노이즈 게이트 테스트
    float* gate_output = (float*)malloc(size * sizeof(float));
    simd_noise_gate_mobile(input, gate_output, size, 0.5f, 0.1f);

    // 기본적인 유효성 검사
    bool gate_test_passed = true;
    for (size_t i = 0; i < size; i++) {
        float abs_input = fabsf(input[i]);
        float abs_output = fabsf(gate_output[i]);

        if (abs_input > 0.5f) {
            // 임계값 이상이면 원본과 같아야 함
            if (!float_equals(gate_output[i], input[i], TEST_EPSILON)) {
                gate_test_passed = false;
                break;
            }
        } else {
            // 임계값 이하면 감쇠되어야 함
            if (abs_output > abs_input) {
                gate_test_passed = false;
                break;
            }
        }
    }
    printf("  Noise Gate Mobile: %s\n", gate_test_passed ? "PASS" : "FAIL");

    // 메모리 정리
    free(input);
    free(output);
    free(envelope);
    free(a);
    free(b);
    free(result_power);
    free(result_ref);
    free(result_thermal);
    free(pitch_output);
    free(spectral_output);
    free(spectral_ref);
    free(gate_output);

    if (!power_test_passed || !thermal_test_passed || !pitch_test_passed ||
        !spectral_test_passed || !gate_test_passed) {
        TEST_FAIL("Mobile-specific function tests failed");
    }
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

/**
 * @brief SIMD 커널 테스트 실행
 */
void run_simd_kernel_tests(void) {
    printf("=== SIMD Kernel Tests ===\n");

    // SIMD 커널 시스템 초기화
    LibEtudeErrorCode result = simd_kernels_init();
    if (result != LIBETUDE_SUCCESS) {
        TEST_FAIL("Failed to initialize SIMD kernel system: %d", result);
        return;
    }

    // 하드웨어 정보 출력
    printf("Available SIMD features: 0x%08X\n", simd_kernels_get_features());
    simd_kernels_print_info();
    printf("\n");

    // 테스트 실행
    test_vector_add();
    test_vector_mul();
    test_vector_dot();
    test_matrix_mul();
    test_relu();
    test_sigmoid();
    test_tanh();
    test_mobile_functions();
    test_performance_benchmark();

    // 정리
    simd_kernels_finalize();

    printf("=== SIMD Kernel Tests Completed ===\n\n");
}

// 독립 실행을 위한 main 함수
#ifdef TEST_SIMD_KERNELS_STANDALONE
int main(void) {
    run_simd_kernel_tests();
    return 0;
}
#endif