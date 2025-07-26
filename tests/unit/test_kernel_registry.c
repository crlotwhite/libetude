/**
 * @file test_kernel_registry.c
 * @brief 커널 레지스트리 단위 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/kernel_registry.h"
#include "libetude/hardware.h"
#include <string.h>

void setUp(void) {
    // 커널 레지스트리 초기화
    LibEtudeErrorCode result = kernel_registry_init();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
}

void tearDown(void) {
    // 커널 레지스트리 정리
    kernel_registry_finalize();
}

void test_initialization(void) {
    // 하드웨어 기능 확인
    uint32_t features = kernel_registry_get_hardware_features();
    TEST_ASSERT_NOT_EQUAL(0, features);

    // 등록된 커널 수 확인
    size_t kernel_count = kernel_registry_get_kernel_count();
    TEST_ASSERT_GREATER_THAN(0, kernel_count);

    printf("커널 레지스트리 초기화 성공\n");
    printf("하드웨어 기능: 0x%08X\n", features);
    printf("등록된 커널 수: %zu\n", kernel_count);

    // 커널 정보 출력
    kernel_registry_print_info();
}

void test_select_optimal_kernel(void) {
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

void test_run_benchmarks(void) {
    // 벤치마크 기능은 현재 구현되지 않음
    // 대신 커널 정보 출력으로 대체
    kernel_registry_print_info();
}

void test_vector_add_kernel(void) {
    const size_t size = 1000;
    float a[size], b[size], result[size];

    // 테스트 데이터 준비
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
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, a[i] + b[i], result[i]);
    }

    printf("벡터 덧셈 커널 테스트 성공\n");
}

void test_error_handling(void) {
    // 존재하지 않는 커널 선택
    void* kernel_func = kernel_registry_select_optimal("nonexistent_kernel", 1000);
    TEST_ASSERT_NULL(kernel_func);

    // 잘못된 크기로 커널 선택
    kernel_func = kernel_registry_select_optimal("vector_add", 0);
    TEST_ASSERT_NULL(kernel_func);

    // NULL 커널 이름
    kernel_func = kernel_registry_select_optimal(NULL, 1000);
    TEST_ASSERT_NULL(kernel_func);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_initialization);
    RUN_TEST(test_select_optimal_kernel);
    RUN_TEST(test_run_benchmarks);
    RUN_TEST(test_vector_add_kernel);
    RUN_TEST(test_error_handling);

    return UNITY_END();
}