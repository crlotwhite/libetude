/**
 * @file test_kernels.c
 * @brief 커널 레지스트리 및 기본 커널 테스트 (Unity)
 *
 * 커널 레지스트리 시스템과 기본 커널 기능을 테스트합니다.
 */

#include "unity.h"
#include "libetude/kernel_registry.h"
#include "libetude/simd_kernels.h"
#include <stdio.h>
#include <stdlib.h>

void setUp(void) {
    // 커널 레지스트리 초기화
    LibEtudeErrorCode result = kernel_registry_init();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);
}

void tearDown(void) {
    // 커널 레지스트리 정리
    kernel_registry_finalize();
}

void test_kernel_registry_initialization(void) {
    printf("Testing kernel registry initialization... ");

    // 이미 setUp에서 초기화되었으므로 추가 초기화 시도
    LibEtudeErrorCode result = kernel_registry_init();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result); // 중복 초기화는 성공해야 함

    printf("PASS\n");
}

void test_kernel_registry_features(void) {
    printf("Testing hardware feature detection... ");

    uint32_t features = kernel_registry_get_hardware_features();

    // 최소한 NONE이거나 실제 기능이 있어야 함
    TEST_ASSERT_TRUE(features == LIBETUDE_SIMD_NONE || features > 0);

    printf("PASS (features: 0x%08X)\n", features);
}

void test_kernel_registry_count(void) {
    printf("Testing kernel count... ");

    size_t count = kernel_registry_get_kernel_count();

    // 최소한 몇 개의 커널은 등록되어 있어야 함
    TEST_ASSERT_TRUE(count > 0);

    printf("PASS (%zu kernels registered)\n", count);
}

void test_kernel_selection(void) {
    printf("Testing kernel selection... ");

    // 벡터 덧셈 커널 선택 테스트
    void* add_kernel = kernel_registry_select_optimal("vector_add", 1024);

    // 커널이 선택되어야 함 (fallback이라도)
    TEST_ASSERT_NOT_NULL(add_kernel);

    printf("PASS\n");
}

void test_kernel_registry_info(void) {
    printf("\n=== Kernel Registry Information ===\n");

    // 커널 레지스트리 정보 출력
    kernel_registry_print_info();

    TEST_PASS();
}

void test_simd_kernels_integration(void) {
    printf("Testing SIMD kernels integration... ");

    // SIMD 커널 시스템 초기화
    LibEtudeErrorCode result = simd_kernels_init();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);

    // SIMD 기능 확인
    uint32_t simd_features = simd_kernels_get_features();
    uint32_t registry_features = kernel_registry_get_hardware_features();

    // 두 시스템의 기능 정보가 일치해야 함
    TEST_ASSERT_EQUAL(registry_features, simd_features);

    // SIMD 커널 시스템 정리
    simd_kernels_finalize();

    printf("PASS\n");
}

void test_kernel_performance_basic(void) {
    printf("Testing basic kernel performance... ");

    const size_t test_size = 1000;
    float* a = (float*)malloc(test_size * sizeof(float));
    float* b = (float*)malloc(test_size * sizeof(float));
    float* result = (float*)malloc(test_size * sizeof(float));

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(result);

    // 테스트 데이터 초기화
    for (size_t i = 0; i < test_size; i++) {
        a[i] = (float)i;
        b[i] = (float)(test_size - i);
    }

    // SIMD 커널 시스템 초기화
    LibEtudeErrorCode init_result = simd_kernels_init();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, init_result);

    // 벡터 덧셈 실행
    simd_vector_add_optimal(a, b, result, test_size);

    // 결과 검증 (몇 개 샘플만)
    for (size_t i = 0; i < 10; i++) {
        float expected = a[i] + b[i];
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, result[i]);
    }

    // SIMD 커널 시스템 정리
    simd_kernels_finalize();

    free(a);
    free(b);
    free(result);

    printf("PASS\n");
}

void test_kernel_error_handling(void) {
    printf("Testing kernel error handling... ");

    // 잘못된 커널 이름으로 선택 시도
    void* invalid_kernel = kernel_registry_select_optimal("nonexistent_kernel", 1024);

    // NULL이 반환되어야 함
    TEST_ASSERT_NULL(invalid_kernel);

    // NULL 포인터로 커널 등록 시도
    KernelInfo invalid_info = {0};
    LibEtudeErrorCode result = kernel_registry_register(&invalid_info);

    // 오류가 반환되어야 함
    TEST_ASSERT_NOT_EQUAL(LIBETUDE_SUCCESS, result);

    printf("PASS\n");
}

int main(void) {
    printf("LibEtude Kernel Registry Test Suite\n");
    printf("===================================\n");

    UNITY_BEGIN();

    // 기본 기능 테스트
    printf("\n>>> BASIC FUNCTIONALITY TESTS <<<\n");
    RUN_TEST(test_kernel_registry_initialization);
    RUN_TEST(test_kernel_registry_features);
    RUN_TEST(test_kernel_registry_count);
    RUN_TEST(test_kernel_selection);

    // 통합 테스트
    printf("\n>>> INTEGRATION TESTS <<<\n");
    RUN_TEST(test_simd_kernels_integration);
    RUN_TEST(test_kernel_performance_basic);

    // 오류 처리 테스트
    printf("\n>>> ERROR HANDLING TESTS <<<\n");
    RUN_TEST(test_kernel_error_handling);

    // 정보 출력 테스트
    printf("\n>>> INFORMATION TESTS <<<\n");
    RUN_TEST(test_kernel_registry_info);

    printf("\n>>> TEST SUMMARY <<<\n");
    return UNITY_END();
}