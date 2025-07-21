/**
 * @file test_kernel_registry.c
 * @brief 커널 레지스트리 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "tests/framework/test_framework.h"
#include "libetude/kernel_registry.h"
#include "libetude/hardware.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// 테스트 함수들
// ============================================================================

void test_kernel_registry_init(void) {
    // 커널 레지스트리 초기화
    LibEtudeErrorCode result = kernel_registry_init();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);

    // 하드웨어 기능 확인
    uint32_t features = kernel_registry_get_hardware_features();
    TEST_ASSERT_NOT_EQUAL(0, features);

    // 등록된 커널 수 확인
    size_t kernel_count = kernel_registry_get_kernel_count();
    TEST_ASSERT_TRUE(kernel_count > 0);

    printf("커널 레지스트리 초기화 성공\n");
    printf("하드웨어 기능: 0x%08X\n", features);
    printf("등록된 커널 수: %zu\n", kernel_count);

    // 커널 정보 출력
    kernel_registry_print_info();
}

void test_kernel_registry_select_optimal(void) {
    // 벡터 덧셈 커널 선택
    void* kernel_func = kernel_registry_select_optimal("vector_add", 1000);
    TEST_ASSERT_NOT_NULL(kernel_func);

    // 벡터 곱셈 커널 선택
    kernel_func = kernel_registry_select_optimal("vector_mul", 1000);
    TEST_ASSERT_NOT_NULL(kernel_func);

    // 행렬 곱셈 커널 선택
    kernel_func = kernel_registry_select_optimal("matmul", 1000);
    TEST_ASSERT_NOT_NULL(kernel_func);

    // 활성화 함수 커널 선택
    kernel_func = kernel_registry_select_optimal("activation_relu", 1000);
    TEST_ASSERT_NOT_NULL(kernel_func);

    printf("커널 선택 테스트 성공\n");
}

void test_kernel_registry_benchmarks(void) {
    // 벤치마크 실행
    LibEtudeErrorCode result = kernel_registry_run_benchmarks();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);

    // 벤치마크 후 커널 정보 출력
    kernel_registry_print_info();
}

void test_kernel_registry_vector_add(void) {
    // 테스트 데이터 준비
    const size_t size = 1000;
    float a[size], b[size], result[size];

    for (size_t i = 0; i < size; i++) {
        a[i] = (float)i;
        b[i] = (float)(size - i);
    }

    // 최적 커널 선택
    VectorAddKernel kernel_func = (VectorAddKernel)kernel_registry_select_optimal("vector_add", size);
    TEST_ASSERT_NOT_NULL(kernel_func);

    // 커널 실행
    kernel_func(a, b, result, size);

    // 결과 검증
    for (size_t i = 0; i < size; i++) {
        TEST_ASSERT_EQUAL_FLOAT(a[i] + b[i], result[i], 0.0001f);
    }

    printf("벡터 덧셈 커널 테스트 성공\n");
}

void test_kernel_registry_vector_mul(void) {
    // 테스트 데이터 준비
    const size_t size = 1000;
    float a[size], b[size], result[size];

    for (size_t i = 0; i < size; i++) {
        a[i] = (float)i / 100.0f;
        b[i] = (float)(size - i) / 100.0f;
    }

    // 최적 커널 선택
    VectorMulKernel kernel_func = (VectorMulKernel)kernel_registry_select_optimal("vector_mul", size);
    TEST_ASSERT_NOT_NULL(kernel_func);

    // 커널 실행
    kernel_func(a, b, result, size);

    // 결과 검증
    for (size_t i = 0; i < size; i++) {
        TEST_ASSERT_EQUAL_FLOAT(a[i] * b[i], result[i], 0.0001f);
    }

    printf("벡터 곱셈 커널 테스트 성공\n");
}

void test_kernel_registry_matmul(void) {
    // 테스트 데이터 준비
    const size_t m = 10, n = 10, k = 10;
    float a[m * k], b[k * n], c[m * n], expected[m * n];

    for (size_t i = 0; i < m * k; i++) {
        a[i] = (float)i / 100.0f;
    }
    for (size_t i = 0; i < k * n; i++) {
        b[i] = (float)i / 100.0f;
    }

    // 예상 결과 계산 (CPU 구현)
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (size_t l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            expected[i * n + j] = sum;
        }
    }

    // 최적 커널 선택
    MatMulKernel kernel_func = (MatMulKernel)kernel_registry_select_optimal("matmul", m);
    TEST_ASSERT_NOT_NULL(kernel_func);

    // 커널 실행
    kernel_func(a, b, c, m, n, k);

    // 결과 검증
    for (size_t i = 0; i < m * n; i++) {
        TEST_ASSERT_EQUAL_FLOAT(expected[i], c[i], 0.001f);
    }

    printf("행렬 곱셈 커널 테스트 성공\n");
}

void test_kernel_registry_activation(void) {
    // 테스트 데이터 준비
    const size_t size = 1000;
    float input[size], output[size], expected[size];

    for (size_t i = 0; i < size; i++) {
        input[i] = ((float)i / size) * 2.0f - 1.0f; // -1 ~ 1 범위
        expected[i] = input[i] > 0.0f ? input[i] : 0.0f; // ReLU
    }

    // 최적 커널 선택
    ActivationKernel kernel_func = (ActivationKernel)kernel_registry_select_optimal("activation_relu", size);
    TEST_ASSERT_NOT_NULL(kernel_func);

    // 커널 실행
    kernel_func(input, output, size);

    // 결과 검증
    for (size_t i = 0; i < size; i++) {
        TEST_ASSERT_EQUAL_FLOAT(expected[i], output[i], 0.0001f);
    }

    printf("활성화 함수 커널 테스트 성공\n");
}

void test_kernel_registry_finalize(void) {
    // 커널 레지스트리 정리
    kernel_registry_finalize();

    // 정리 후 커널 수 확인 (0이어야 함)
    size_t kernel_count = kernel_registry_get_kernel_count();
    TEST_ASSERT_EQUAL(0, kernel_count);

    printf("커널 레지스트리 정리 성공\n");
}

// ============================================================================
// 테스트 스위트 실행
// ============================================================================

int main(void) {
    printf("LibEtude 커널 레지스트리 테스트 시작\n");
    printf("=====================================\n\n");

    // 커널 레지스트리 초기화 및 정보 출력
    RUN_TEST(test_kernel_registry_init);

    // 커널 선택 테스트
    RUN_TEST(test_kernel_registry_select_optimal);

    // 벤치마크 테스트
    RUN_TEST(test_kernel_registry_benchmarks);

    // 개별 커널 테스트
    RUN_TEST(test_kernel_registry_vector_add);
    RUN_TEST(test_kernel_registry_vector_mul);
    RUN_TEST(test_kernel_registry_matmul);
    RUN_TEST(test_kernel_registry_activation);

    // 커널 레지스트리 정리
    RUN_TEST(test_kernel_registry_finalize);

    printf("\n=====================================\n");
    printf("테스트 완료: %d개 성공, %d개 실패\n",
           test_framework_get_passed_count(),
           test_framework_get_failed_count());

    return test_framework_get_failed_count() == 0 ? 0 : 1;
}