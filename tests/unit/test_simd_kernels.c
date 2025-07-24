/**
 * @file test_simd_kernels.c
 * @brief SIMD 커널 단위 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/simd_kernels.h"
#include "libetude/kernel_registry.h"
#include <math.h>
#include <string.h>
#include <time.h>

// 테스트 허용 오차
#define TEST_EPSILON 1e-5f
#define TEST_LARGE_EPSILON 1e-3f // 근사 함수용

// 유틸리티 함수
static bool float_equals(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

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

static void generate_test_vector(float* vec, size_t size, float min_val, float max_val) {
    for (size_t i = 0; i < size; i++) {
        vec[i] = min_val + ((float)rand() / RAND_MAX) * (max_val - min_val);
    }
}

void setUp(void) {
    // SIMD 커널 초기화
    et_init_simd_kernels();
    srand((unsigned int)time(NULL));
}

void tearDown(void) {
    // 정리
    et_cleanup_simd_kernels();
}

void test_vector_addition(void) {
    const size_t size = 1024;
    float a[size], b[size], result[size], expected[size];

    // 테스트 데이터 생성
    generate_test_vector(a, size, -10.0f, 10.0f);
    generate_test_vector(b, size, -10.0f, 10.0f);

    // 예상 결과 계산
    for (size_t i = 0; i < size; i++) {
        expected[i] = a[i] + b[i];
    }

    // SIMD 벡터 덧셈 실행
    et_simd_vector_add(a, b, result, size);

    // 결과 검증
    TEST_ASSERT_TRUE(vector_equals(result, expected, size, TEST_EPSILON));
}

void test_vector_multiplication(void) {
    const size_t size = 1024;
    float a[size], b[size], result[size], expected[size];

    generate_test_vector(a, size, -10.0f, 10.0f);
    generate_test_vector(b, size, -10.0f, 10.0f);

    for (size_t i = 0; i < size; i++) {
        expected[i] = a[i] * b[i];
    }

    et_simd_vector_mul(a, b, result, size);

    TEST_ASSERT_TRUE(vector_equals(result, expected, size, TEST_EPSILON));
}

void test_vector_dot_product(void) {
    const size_t size = 1024;
    float a[size], b[size];

    generate_test_vector(a, size, -10.0f, 10.0f);
    generate_test_vector(b, size, -10.0f, 10.0f);

    // 예상 결과 계산
    float expected = 0.0f;
    for (size_t i = 0; i < size; i++) {
        expected += a[i] * b[i];
    }

    // SIMD 내적 계산
    float result = et_simd_dot_product(a, b, size);

    TEST_ASSERT_FLOAT_WITHIN(TEST_EPSILON * size, expected, result);
}

void test_matrix_vector_multiplication(void) {
    const size_t rows = 64;
    const size_t cols = 64;

    float matrix[rows * cols], vector[cols], result[rows], expected[rows];

    generate_test_vector(matrix, rows * cols, -1.0f, 1.0f);
    generate_test_vector(vector, cols, -1.0f, 1.0f);

    // 예상 결과 계산 (일반적인 행렬-벡터 곱셈)
    for (size_t i = 0; i < rows; i++) {
        expected[i] = 0.0f;
        for (size_t j = 0; j < cols; j++) {
            expected[i] += matrix[i * cols + j] * vector[j];
        }
    }

    // SIMD 행렬-벡터 곱셈
    et_simd_matrix_vector_mul(matrix, vector, result, rows, cols);

    TEST_ASSERT_TRUE(vector_equals(result, expected, rows, TEST_EPSILON * cols));
}

void test_relu_activation(void) {
    const size_t size = 1024;
    float input[size], result[size], expected[size];

    generate_test_vector(input, size, -5.0f, 5.0f);

    // 예상 결과 계산
    for (size_t i = 0; i < size; i++) {
        expected[i] = fmaxf(0.0f, input[i]);
    }

    // SIMD ReLU 실행
    et_simd_relu(input, result, size);

    TEST_ASSERT_TRUE(vector_equals(result, expected, size, TEST_EPSILON));
}

void test_sigmoid_activation(void) {
    const size_t size = 16;
    float input[size], result[size], expected[size];

    generate_test_vector(input, size, -3.0f, 3.0f);

    // 예상 결과 계산
    for (size_t i = 0; i < size; i++) {
        expected[i] = 1.0f / (1.0f + expf(-input[i]));
    }

    // SIMD Sigmoid 실행
    et_simd_sigmoid(input, result, size);

    TEST_ASSERT_TRUE(vector_equals(result, expected, size, TEST_LARGE_EPSILON));
}

void test_edge_cases(void) {
    // 빈 벡터
    float* null_ptr = NULL;
    et_simd_vector_add(null_ptr, null_ptr, null_ptr, 0);
    TEST_PASS(); // 크래시하지 않으면 성공

    // 크기가 1인 벡터
    float a = 1.0f, b = 2.0f, result;
    et_simd_vector_add(&a, &b, &result, 1);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPSILON, 3.0f, result);
}

void test_hardware_specific_features(void) {
#ifdef LIBETUDE_HAVE_SSE
    TEST_ASSERT_TRUE(et_has_sse_support());
#endif

#ifdef LIBETUDE_HAVE_AVX
    TEST_ASSERT_TRUE(et_has_avx_support());
#endif

#ifdef LIBETUDE_HAVE_NEON
    TEST_ASSERT_TRUE(et_has_neon_support());
#endif

    // 사용 가능한 SIMD 기능 정보 출력
    printf("Available SIMD features:\n");
    if (et_has_sse_support()) printf("  - SSE\n");
    if (et_has_avx_support()) printf("  - AVX\n");
    if (et_has_neon_support()) printf("  - NEON\n");
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_vector_addition);
    RUN_TEST(test_vector_multiplication);
    RUN_TEST(test_vector_dot_product);
    RUN_TEST(test_matrix_vector_multiplication);
    RUN_TEST(test_relu_activation);
    RUN_TEST(test_sigmoid_activation);
    RUN_TEST(test_edge_cases);
    RUN_TEST(test_hardware_specific_features);

    return UNITY_END();
}