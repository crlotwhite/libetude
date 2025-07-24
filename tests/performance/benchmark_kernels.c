/**
 * @file benchmark_kernels.c
 * @brief 커널 벤치마크 (Unity)
 */

#include "unity.h"
#include "libetude/kernels.h"
#include "libetude/simd_kernels.h"
#include <time.h>

void setUp(void) {
    // 벤치마크 전 초기화
}

void tearDown(void) {
    // 벤치마크 후 정리
}

void test_vector_add_performance(void) {
    const size_t size = 10000;
    float a[size], b[size], result[size];

    // 테스트 데이터 초기화
    for (size_t i = 0; i < size; i++) {
        a[i] = 1.0f;
        b[i] = 2.0f;
    }

    clock_t start = clock();

    // 벤치마크 실행
    for (int i = 0; i < 1000; i++) {
        for (size_t j = 0; j < size; j++) {
            result[j] = a[j] + b[j];
        }
    }

    clock_t end = clock();
    double duration = ((double)(end - start)) / CLOCKS_PER_SEC * 1000000; // μs

    printf("벡터 덧셈 벤치마크: %.0f μs\n", duration);
    TEST_PASS();
}

void test_matrix_multiplication_performance(void) {
    const size_t size = 100;
    float a[size * size], b[size * size], c[size * size];

    // 테스트 데이터 초기화
    for (size_t i = 0; i < size * size; i++) {
        a[i] = 1.0f;
        b[i] = 2.0f;
        c[i] = 0.0f;
    }

    clock_t start = clock();

    // 간단한 행렬 곱셈 벤치마크
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < size; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < size; k++) {
                sum += a[i * size + k] * b[k * size + j];
            }
            c[i * size + j] = sum;
        }
    }

    clock_t end = clock();
    double duration = ((double)(end - start)) / CLOCKS_PER_SEC * 1000; // ms

    printf("행렬 곱셈 벤치마크: %.0f ms\n", duration);
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_vector_add_performance);
    RUN_TEST(test_matrix_multiplication_performance);

    return UNITY_END();
}