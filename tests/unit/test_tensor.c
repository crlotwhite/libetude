/**
 * @file test_tensor.c
 * @brief LibEtude 텐서 시스템 단위 테스트
 *
 * 텐서 생성, 조작, 연산 등의 기능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "libetude/tensor.h"
#include "libetude/memory.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s\n", message); \
    } \
} while(0)

#define FLOAT_EPSILON 1e-6f
#define FLOAT_EQUAL(a, b) (fabsf((a) - (b)) < FLOAT_EPSILON)

// =============================================================================
// 테스트 유틸리티 함수
// =============================================================================

static void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

static void print_test_summary() {
    printf("\n=== 테스트 결과 ===\n");
    printf("총 테스트: %d\n", tests_run);
    printf("통과: %d\n", tests_passed);
    printf("실패: %d\n", tests_run - tests_passed);
    printf("성공률: %.1f%%\n", tests_run > 0 ? (float)tests_passed / tests_run * 100.0f : 0.0f);
}

// =============================================================================
// 데이터 타입 테스트
// =============================================================================

static void test_dtype_functions() {
    print_test_header("데이터 타입 함수 테스트");

    TEST_ASSERT(et_dtype_size(ET_FLOAT32) == sizeof(float), "FLOAT32 크기 확인");
    TEST_ASSERT(et_dtype_size(ET_INT8) == sizeof(int8_t), "INT8 크기 확인");
    TEST_ASSERT(et_dtype_size(ET_INT32) == sizeof(int32_t), "INT32 크기 확인");

    TEST_ASSERT(strcmp(et_dtype_name(ET_FLOAT32), "float32") == 0, "FLOAT32 이름 확인");
    TEST_ASSERT(strcmp(et_dtype_name(ET_INT8), "int8") == 0, "INT8 이름 확인");

    TEST_ASSERT(et_dtype_is_float(ET_FLOAT32) == true, "FLOAT32는 부동소수점");
    TEST_ASSERT(et_dtype_is_float(ET_INT8) == false, "INT8은 부동소수점 아님");

    TEST_ASSERT(et_dtype_is_int(ET_INT8) == true, "INT8은 정수");
    TEST_ASSERT(et_dtype_is_int(ET_FLOAT32) == false, "FLOAT32는 정수 아님");
}

// =============================================================================
// 텐서 생성 및 소멸 테스트
// =============================================================================

static void test_tensor_creation() {
    print_test_header("텐서 생성 및 소멸 테스트");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(4 * 1024 * 1024, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 2D 텐서 생성
    size_t shape[] = {3, 4};
    ETTensor* tensor = et_create_tensor(pool, ET_FLOAT32, 2, shape);
    TEST_ASSERT(tensor != NULL, "2D 텐서 생성");
    TEST_ASSERT(tensor->ndim == 2, "차원 수 확인");
    TEST_ASSERT(tensor->shape[0] == 3 && tensor->shape[1] == 4, "모양 확인");
    TEST_ASSERT(tensor->size == 12, "총 요소 수 확인");
    TEST_ASSERT(tensor->dtype == ET_FLOAT32, "데이터 타입 확인");
    TEST_ASSERT(tensor->is_contiguous == true, "연속 메모리 확인");
    TEST_ASSERT(tensor->ref_count == 1, "참조 카운트 확인");

    // 이름을 가진 텐서 생성
    ETTensor* named_tensor = et_create_tensor_named(pool, ET_FLOAT32, 2, shape, "test_tensor");
    TEST_ASSERT(named_tensor != NULL, "이름을 가진 텐서 생성");
    TEST_ASSERT(strcmp(named_tensor->name, "test_tensor") == 0, "텐서 이름 확인");

    // 0으로 초기화된 텐서 생성
    ETTensor* zeros = et_create_zeros(pool, ET_FLOAT32, 2, shape);
    TEST_ASSERT(zeros != NULL, "0 텐서 생성");
    float val = et_get_float(zeros, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 0.0f), "0 초기화 확인");

    // 1로 초기화된 텐서 생성
    ETTensor* ones = et_create_ones(pool, ET_FLOAT32, 2, shape);
    TEST_ASSERT(ones != NULL, "1 텐서 생성");
    if (ones) {
        val = et_get_float(ones, (size_t[]){0, 0});
        TEST_ASSERT(FLOAT_EQUAL(val, 1.0f), "1 초기화 확인");
    } else {
        printf("DEBUG: et_create_ones returned NULL\n");
        tests_run++;
    }

    // 텐서 복사
    ETTensor* copied = et_copy_tensor(tensor, NULL);
    TEST_ASSERT(copied != NULL, "텐서 복사");
    if (copied) {
        TEST_ASSERT(et_same_shape(tensor, copied), "복사된 텐서 모양 확인");
    } else {
        printf("DEBUG: et_copy_tensor returned NULL\n");
        tests_run++;
    }

    // 참조 카운트 테스트
    ETTensor* retained = et_retain_tensor(tensor);
    TEST_ASSERT(retained == tensor, "텐서 참조 반환값 확인");
    TEST_ASSERT(tensor->ref_count == 2, "참조 카운트 증가 확인");

    // 텐서 소멸
    et_destroy_tensor(retained); // ref_count: 2 -> 1
    TEST_ASSERT(tensor->ref_count == 1, "참조 카운트 감소 확인");

    et_destroy_tensor(tensor);
    et_destroy_tensor(named_tensor);
    et_destroy_tensor(zeros);
    if (ones) et_destroy_tensor(ones);
    if (copied) et_destroy_tensor(copied);

    et_destroy_memory_pool(pool);
}

// =============================================================================
// 텐서 데이터 접근 테스트
// =============================================================================

static void test_tensor_data_access() {
    print_test_header("텐서 데이터 접근 테스트");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);
    size_t shape[] = {2, 3};
    ETTensor* tensor = et_create_tensor(pool, ET_FLOAT32, 2, shape);

    // 값 설정 및 가져오기
    et_set_float(tensor, (size_t[]){0, 0}, 1.5f);
    et_set_float(tensor, (size_t[]){0, 1}, 2.5f);
    et_set_float(tensor, (size_t[]){1, 2}, 3.5f);

    float val1 = et_get_float(tensor, (size_t[]){0, 0});
    float val2 = et_get_float(tensor, (size_t[]){0, 1});
    float val3 = et_get_float(tensor, (size_t[]){1, 2});

    TEST_ASSERT(FLOAT_EQUAL(val1, 1.5f), "값 설정/가져오기 (0,0)");
    TEST_ASSERT(FLOAT_EQUAL(val2, 2.5f), "값 설정/가져오기 (0,1)");
    TEST_ASSERT(FLOAT_EQUAL(val3, 3.5f), "값 설정/가져오기 (1,2)");

    // 포인터 접근
    void* ptr = et_get_ptr(tensor, (size_t[]){0, 0});
    TEST_ASSERT(ptr != NULL, "포인터 접근");
    TEST_ASSERT(FLOAT_EQUAL(*(float*)ptr, 1.5f), "포인터를 통한 값 확인");

    // 텐서 채우기
    et_fill_tensor(tensor, 7.0f);
    val1 = et_get_float(tensor, (size_t[]){0, 0});
    val2 = et_get_float(tensor, (size_t[]){1, 1});
    TEST_ASSERT(FLOAT_EQUAL(val1, 7.0f), "텐서 채우기 확인 (0,0)");
    TEST_ASSERT(FLOAT_EQUAL(val2, 7.0f), "텐서 채우기 확인 (1,1)");

    et_destroy_tensor(tensor);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 텐서 조작 테스트
// =============================================================================

static void test_tensor_manipulation() {
    print_test_header("텐서 조작 테스트");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);

    // 원본 텐서 생성 (2x6)
    size_t shape[] = {2, 6};
    ETTensor* tensor = et_create_tensor(pool, ET_FLOAT32, 2, shape);

    // 데이터 초기화
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < 6; j++) {
            et_set_float(tensor, (size_t[]){i, j}, (float)(i * 6 + j));
        }
    }

    // 리셰이프 테스트 (2x6 -> 3x4)
    size_t new_shape[] = {3, 4};
    ETTensor* reshaped = et_reshape_tensor(tensor, 2, new_shape);
    TEST_ASSERT(reshaped != NULL, "리셰이프 성공");
    TEST_ASSERT(reshaped->shape[0] == 3 && reshaped->shape[1] == 4, "리셰이프 모양 확인");
    TEST_ASSERT(reshaped->size == 12, "리셰이프 크기 확인");

    // 리셰이프된 데이터 확인
    float val = et_get_float(reshaped, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 0.0f), "리셰이프 데이터 확인 (0,0)");
    val = et_get_float(reshaped, (size_t[]){1, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 4.0f), "리셰이프 데이터 확인 (1,0)");

    // 2D 텐서 전치 테스트
    size_t matrix_shape[] = {2, 3};
    ETTensor* matrix = et_create_tensor(pool, ET_FLOAT32, 2, matrix_shape);

    // 행렬 데이터 초기화
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < 3; j++) {
            et_set_float(matrix, (size_t[]){i, j}, (float)(i * 3 + j + 1));
        }
    }

    ETTensor* transposed = et_transpose_tensor(matrix);
    TEST_ASSERT(transposed != NULL, "전치 성공");
    TEST_ASSERT(transposed->shape[0] == 3 && transposed->shape[1] == 2, "전치 모양 확인");

    // 전치된 데이터 확인
    val = et_get_float(transposed, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 1.0f), "전치 데이터 확인 (0,0)");
    val = et_get_float(transposed, (size_t[]){0, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 4.0f), "전치 데이터 확인 (0,1)");
    val = et_get_float(transposed, (size_t[]){1, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 2.0f), "전치 데이터 확인 (1,0)");

    et_destroy_tensor(tensor);
    et_destroy_tensor(reshaped);
    et_destroy_tensor(matrix);
    et_destroy_tensor(transposed);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 텐서 연산 테스트
// =============================================================================

static void test_tensor_operations() {
    print_test_header("텐서 연산 테스트");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);

    // 테스트용 텐서 생성
    size_t shape[] = {2, 2};
    ETTensor* a = et_create_tensor(pool, ET_FLOAT32, 2, shape);
    ETTensor* b = et_create_tensor(pool, ET_FLOAT32, 2, shape);

    // 데이터 초기화
    et_set_float(a, (size_t[]){0, 0}, 1.0f);
    et_set_float(a, (size_t[]){0, 1}, 2.0f);
    et_set_float(a, (size_t[]){1, 0}, 3.0f);
    et_set_float(a, (size_t[]){1, 1}, 4.0f);

    et_set_float(b, (size_t[]){0, 0}, 2.0f);
    et_set_float(b, (size_t[]){0, 1}, 3.0f);
    et_set_float(b, (size_t[]){1, 0}, 4.0f);
    et_set_float(b, (size_t[]){1, 1}, 5.0f);

    // 덧셈 테스트
    ETTensor* sum = et_add(a, b, NULL, NULL);
    TEST_ASSERT(sum != NULL, "텐서 덧셈 성공");
    float val = et_get_float(sum, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 3.0f), "덧셈 결과 확인 (0,0)");
    val = et_get_float(sum, (size_t[]){1, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 9.0f), "덧셈 결과 확인 (1,1)");

    // 곱셈 테스트
    ETTensor* mul = et_mul(a, b, NULL, NULL);
    TEST_ASSERT(mul != NULL, "텐서 곱셈 성공");
    val = et_get_float(mul, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 2.0f), "곱셈 결과 확인 (0,0)");
    val = et_get_float(mul, (size_t[]){1, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 20.0f), "곱셈 결과 확인 (1,1)");

    // 스칼라 연산 테스트
    ETTensor* add_scalar = et_add_scalar(a, 10.0f, NULL, NULL);
    TEST_ASSERT(add_scalar != NULL, "스칼라 덧셈 성공");
    val = et_get_float(add_scalar, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 11.0f), "스칼라 덧셈 결과 확인");

    ETTensor* mul_scalar = et_mul_scalar(a, 2.0f, NULL, NULL);
    TEST_ASSERT(mul_scalar != NULL, "스칼라 곱셈 성공");
    val = et_get_float(mul_scalar, (size_t[]){1, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 8.0f), "스칼라 곱셈 결과 확인");

    // 행렬 곱셈 테스트
    ETTensor* matmul = et_matmul(a, b, NULL, NULL);
    TEST_ASSERT(matmul != NULL, "행렬 곱셈 성공");
    val = et_get_float(matmul, (size_t[]){0, 0}); // 1*2 + 2*4 = 10
    TEST_ASSERT(FLOAT_EQUAL(val, 10.0f), "행렬 곱셈 결과 확인 (0,0)");
    val = et_get_float(matmul, (size_t[]){0, 1}); // 1*3 + 2*5 = 13
    TEST_ASSERT(FLOAT_EQUAL(val, 13.0f), "행렬 곱셈 결과 확인 (0,1)");
    val = et_get_float(matmul, (size_t[]){1, 0}); // 3*2 + 4*4 = 22
    TEST_ASSERT(FLOAT_EQUAL(val, 22.0f), "행렬 곱셈 결과 확인 (1,0)");
    val = et_get_float(matmul, (size_t[]){1, 1}); // 3*3 + 4*5 = 29
    TEST_ASSERT(FLOAT_EQUAL(val, 29.0f), "행렬 곱셈 결과 확인 (1,1)");

    et_destroy_tensor(a);
    et_destroy_tensor(b);
    et_destroy_tensor(sum);
    et_destroy_tensor(mul);
    et_destroy_tensor(add_scalar);
    et_destroy_tensor(mul_scalar);
    et_destroy_tensor(matmul);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 축소 연산 테스트
// =============================================================================

static void test_reduction_operations() {
    print_test_header("축소 연산 테스트");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);

    // 테스트용 텐서 생성 (2x3)
    size_t shape[] = {2, 3};
    ETTensor* tensor = et_create_tensor(pool, ET_FLOAT32, 2, shape);

    // 데이터 초기화: [[1, 2, 3], [4, 5, 6]]
    et_set_float(tensor, (size_t[]){0, 0}, 1.0f);
    et_set_float(tensor, (size_t[]){0, 1}, 2.0f);
    et_set_float(tensor, (size_t[]){0, 2}, 3.0f);
    et_set_float(tensor, (size_t[]){1, 0}, 4.0f);
    et_set_float(tensor, (size_t[]){1, 1}, 5.0f);
    et_set_float(tensor, (size_t[]){1, 2}, 6.0f);

    // 전체 합계 테스트
    ETTensor* total_sum = et_sum(tensor, NULL, -1, false, NULL);
    TEST_ASSERT(total_sum != NULL, "전체 합계 성공");
    float val = et_get_float(total_sum, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 21.0f), "전체 합계 결과 확인"); // 1+2+3+4+5+6 = 21

    // 축 0 방향 합계 테스트 (열 방향)
    ETTensor* sum_axis0 = et_sum(tensor, NULL, 0, false, NULL);
    TEST_ASSERT(sum_axis0 != NULL, "축 0 합계 성공");
    TEST_ASSERT(sum_axis0->ndim == 1 && sum_axis0->shape[0] == 3, "축 0 합계 모양 확인");
    val = et_get_float(sum_axis0, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 5.0f), "축 0 합계 결과 확인 [0]"); // 1+4 = 5
    val = et_get_float(sum_axis0, (size_t[]){1});
    TEST_ASSERT(FLOAT_EQUAL(val, 7.0f), "축 0 합계 결과 확인 [1]"); // 2+5 = 7
    val = et_get_float(sum_axis0, (size_t[]){2});
    TEST_ASSERT(FLOAT_EQUAL(val, 9.0f), "축 0 합계 결과 확인 [2]"); // 3+6 = 9

    // 축 1 방향 합계 테스트 (행 방향)
    ETTensor* sum_axis1 = et_sum(tensor, NULL, 1, false, NULL);
    TEST_ASSERT(sum_axis1 != NULL, "축 1 합계 성공");
    TEST_ASSERT(sum_axis1->ndim == 1 && sum_axis1->shape[0] == 2, "축 1 합계 모양 확인");
    val = et_get_float(sum_axis1, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 6.0f), "축 1 합계 결과 확인 [0]"); // 1+2+3 = 6
    val = et_get_float(sum_axis1, (size_t[]){1});
    TEST_ASSERT(FLOAT_EQUAL(val, 15.0f), "축 1 합계 결과 확인 [1]"); // 4+5+6 = 15

    // 평균 테스트
    ETTensor* mean_total = et_mean(tensor, NULL, -1, false, NULL);
    TEST_ASSERT(mean_total != NULL, "전체 평균 성공");
    val = et_get_float(mean_total, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 3.5f), "전체 평균 결과 확인"); // 21/6 = 3.5

    ETTensor* mean_axis0 = et_mean(tensor, NULL, 0, false, NULL);
    TEST_ASSERT(mean_axis0 != NULL, "축 0 평균 성공");
    val = et_get_float(mean_axis0, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 2.5f), "축 0 평균 결과 확인 [0]"); // 5/2 = 2.5

    et_destroy_tensor(tensor);
    et_destroy_tensor(total_sum);
    et_destroy_tensor(sum_axis0);
    et_destroy_tensor(sum_axis1);
    et_destroy_tensor(mean_total);
    et_destroy_tensor(mean_axis0);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 새로운 축소 연산 테스트 (max, min)
// =============================================================================

static void test_new_reduction_operations() {
    print_test_header("새로운 축소 연산 테스트 (max, min)");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);

    // 테스트용 텐서 생성 (2x3)
    size_t shape[] = {2, 3};
    ETTensor* tensor = et_create_tensor(pool, ET_FLOAT32, 2, shape);

    // 데이터 초기화: [[1, 5, 3], [4, 2, 6]]
    et_set_float(tensor, (size_t[]){0, 0}, 1.0f);
    et_set_float(tensor, (size_t[]){0, 1}, 5.0f);
    et_set_float(tensor, (size_t[]){0, 2}, 3.0f);
    et_set_float(tensor, (size_t[]){1, 0}, 4.0f);
    et_set_float(tensor, (size_t[]){1, 1}, 2.0f);
    et_set_float(tensor, (size_t[]){1, 2}, 6.0f);

    // 전체 최대값 테스트
    ETTensor* total_max = et_max(tensor, NULL, -1, false, NULL);
    TEST_ASSERT(total_max != NULL, "전체 최대값 성공");
    float val = et_get_float(total_max, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 6.0f), "전체 최대값 결과 확인");

    // 전체 최소값 테스트
    ETTensor* total_min = et_min(tensor, NULL, -1, false, NULL);
    TEST_ASSERT(total_min != NULL, "전체 최소값 성공");
    val = et_get_float(total_min, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 1.0f), "전체 최소값 결과 확인");

    // 축 0 방향 최대값 테스트
    ETTensor* max_axis0 = et_max(tensor, NULL, 0, false, NULL);
    TEST_ASSERT(max_axis0 != NULL, "축 0 최대값 성공");
    val = et_get_float(max_axis0, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 4.0f), "축 0 최대값 결과 확인 [0]"); // max(1,4) = 4
    val = et_get_float(max_axis0, (size_t[]){1});
    TEST_ASSERT(FLOAT_EQUAL(val, 5.0f), "축 0 최대값 결과 확인 [1]"); // max(5,2) = 5
    val = et_get_float(max_axis0, (size_t[]){2});
    TEST_ASSERT(FLOAT_EQUAL(val, 6.0f), "축 0 최대값 결과 확인 [2]"); // max(3,6) = 6

    // 축 1 방향 최소값 테스트
    ETTensor* min_axis1 = et_min(tensor, NULL, 1, false, NULL);
    TEST_ASSERT(min_axis1 != NULL, "축 1 최소값 성공");
    val = et_get_float(min_axis1, (size_t[]){0});
    TEST_ASSERT(FLOAT_EQUAL(val, 1.0f), "축 1 최소값 결과 확인 [0]"); // min(1,5,3) = 1
    val = et_get_float(min_axis1, (size_t[]){1});
    TEST_ASSERT(FLOAT_EQUAL(val, 2.0f), "축 1 최소값 결과 확인 [1]"); // min(4,2,6) = 2

    et_destroy_tensor(tensor);
    et_destroy_tensor(total_max);
    et_destroy_tensor(total_min);
    et_destroy_tensor(max_axis0);
    et_destroy_tensor(min_axis1);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 단항 연산 테스트
// =============================================================================

static void test_unary_operations() {
    print_test_header("단항 연산 테스트");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);

    // 테스트용 텐서 생성
    size_t shape[] = {2, 2};
    ETTensor* tensor = et_create_tensor(pool, ET_FLOAT32, 2, shape);

    // 데이터 초기화: [[-2, 4], [9, 1]]
    et_set_float(tensor, (size_t[]){0, 0}, -2.0f);
    et_set_float(tensor, (size_t[]){0, 1}, 4.0f);
    et_set_float(tensor, (size_t[]){1, 0}, 9.0f);
    et_set_float(tensor, (size_t[]){1, 1}, 1.0f);

    // 절댓값 테스트
    ETTensor* abs_result = et_abs(tensor, NULL, NULL);
    TEST_ASSERT(abs_result != NULL, "절댓값 연산 성공");
    float val = et_get_float(abs_result, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 2.0f), "절댓값 결과 확인 (0,0)");
    val = et_get_float(abs_result, (size_t[]){0, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 4.0f), "절댓값 결과 확인 (0,1)");

    // 제곱 테스트
    ETTensor* square_result = et_square(tensor, NULL, NULL);
    TEST_ASSERT(square_result != NULL, "제곱 연산 성공");
    val = et_get_float(square_result, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 4.0f), "제곱 결과 확인 (0,0)"); // (-2)^2 = 4
    val = et_get_float(square_result, (size_t[]){1, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 81.0f), "제곱 결과 확인 (1,0)"); // 9^2 = 81

    // 제곱근 테스트
    ETTensor* sqrt_result = et_sqrt(square_result, NULL, NULL);
    TEST_ASSERT(sqrt_result != NULL, "제곱근 연산 성공");
    val = et_get_float(sqrt_result, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 2.0f), "제곱근 결과 확인 (0,0)"); // sqrt(4) = 2
    val = et_get_float(sqrt_result, (size_t[]){1, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 9.0f), "제곱근 결과 확인 (1,0)"); // sqrt(81) = 9

    // 지수 함수 테스트 (작은 값으로)
    ETTensor* small_tensor = et_create_tensor(pool, ET_FLOAT32, 2, shape);
    et_set_float(small_tensor, (size_t[]){0, 0}, 0.0f);
    et_set_float(small_tensor, (size_t[]){0, 1}, 1.0f);
    et_set_float(small_tensor, (size_t[]){1, 0}, 2.0f);
    et_set_float(small_tensor, (size_t[]){1, 1}, -1.0f);

    ETTensor* exp_result = et_exp(small_tensor, NULL, NULL);
    TEST_ASSERT(exp_result != NULL, "지수 함수 연산 성공");
    val = et_get_float(exp_result, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 1.0f), "지수 함수 결과 확인 (0,0)"); // exp(0) = 1
    val = et_get_float(exp_result, (size_t[]){0, 1});
    TEST_ASSERT(fabsf(val - 2.718282f) < 0.001f, "지수 함수 결과 확인 (0,1)"); // exp(1) ≈ e

    // 자연 로그 테스트
    ETTensor* log_result = et_tensor_log(exp_result, NULL, NULL);
    TEST_ASSERT(log_result != NULL, "자연 로그 연산 성공");
    val = et_get_float(log_result, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 0.0f), "자연 로그 결과 확인 (0,0)"); // log(1) = 0
    val = et_get_float(log_result, (size_t[]){0, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 1.0f), "자연 로그 결과 확인 (0,1)"); // log(e) = 1

    et_destroy_tensor(tensor);
    et_destroy_tensor(abs_result);
    et_destroy_tensor(square_result);
    et_destroy_tensor(sqrt_result);
    et_destroy_tensor(small_tensor);
    et_destroy_tensor(exp_result);
    et_destroy_tensor(log_result);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 고급 변환 연산 테스트
// =============================================================================

static void test_advanced_transformation() {
    print_test_header("고급 변환 연산 테스트");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);

    // 3D 텐서 생성 (2x3x4)
    size_t shape[] = {2, 3, 4};
    ETTensor* tensor = et_create_tensor(pool, ET_FLOAT32, 3, shape);

    // 데이터 초기화
    for (size_t i = 0; i < 2; i++) {
        for (size_t j = 0; j < 3; j++) {
            for (size_t k = 0; k < 4; k++) {
                et_set_float(tensor, (size_t[]){i, j, k}, (float)(i * 12 + j * 4 + k));
            }
        }
    }

    // 차원 순서 변경 테스트 (2x3x4 -> 4x2x3)
    size_t axes[] = {2, 0, 1}; // (dim2, dim0, dim1)
    ETTensor* permuted = et_permute_tensor(tensor, axes);
    TEST_ASSERT(permuted != NULL, "차원 순서 변경 성공");
    TEST_ASSERT(permuted->shape[0] == 4 && permuted->shape[1] == 2 && permuted->shape[2] == 3,
                "차원 순서 변경 모양 확인");

    // 차원 확장 테스트 (2x3x4 -> 2x1x3x4)
    ETTensor* expanded = et_expand_dims(tensor, 1);
    TEST_ASSERT(expanded != NULL, "차원 확장 성공");
    TEST_ASSERT(expanded->ndim == 4, "차원 확장 차원 수 확인");
    TEST_ASSERT(expanded->shape[0] == 2 && expanded->shape[1] == 1 &&
                expanded->shape[2] == 3 && expanded->shape[3] == 4, "차원 확장 모양 확인");

    // 차원 축소 테스트 (2x1x3x4 -> 2x3x4)
    ETTensor* squeezed = et_squeeze_tensor(expanded, 1);
    TEST_ASSERT(squeezed != NULL, "차원 축소 성공");
    TEST_ASSERT(squeezed->ndim == 3, "차원 축소 차원 수 확인");
    TEST_ASSERT(et_same_shape(tensor, squeezed), "차원 축소 모양 확인");

    // 모든 크기 1인 차원 제거 테스트
    size_t shape_with_ones[] = {2, 1, 3, 1, 4};
    ETTensor* tensor_with_ones = et_create_tensor(pool, ET_FLOAT32, 5, shape_with_ones);
    ETTensor* all_squeezed = et_squeeze_tensor(tensor_with_ones, -1);
    TEST_ASSERT(all_squeezed != NULL, "모든 크기 1 차원 제거 성공");
    TEST_ASSERT(all_squeezed->ndim == 3, "모든 크기 1 차원 제거 차원 수 확인");
    TEST_ASSERT(all_squeezed->shape[0] == 2 && all_squeezed->shape[1] == 3 && all_squeezed->shape[2] == 4,
                "모든 크기 1 차원 제거 모양 확인");

    et_destroy_tensor(tensor);
    et_destroy_tensor(permuted);
    et_destroy_tensor(expanded);
    et_destroy_tensor(squeezed);
    et_destroy_tensor(tensor_with_ones);
    et_destroy_tensor(all_squeezed);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 인플레이스 연산 테스트
// =============================================================================

static void test_inplace_operations() {
    print_test_header("인플레이스 연산 테스트");

    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, ET_DEFAULT_ALIGNMENT);

    // 테스트용 텐서 생성
    size_t shape[] = {2, 2};
    ETTensor* a = et_create_tensor(pool, ET_FLOAT32, 2, shape);
    ETTensor* b = et_create_tensor(pool, ET_FLOAT32, 2, shape);

    // 데이터 초기화
    et_set_float(a, (size_t[]){0, 0}, 1.0f);
    et_set_float(a, (size_t[]){0, 1}, 2.0f);
    et_set_float(a, (size_t[]){1, 0}, 3.0f);
    et_set_float(a, (size_t[]){1, 1}, 4.0f);

    et_set_float(b, (size_t[]){0, 0}, 1.0f);
    et_set_float(b, (size_t[]){0, 1}, 1.0f);
    et_set_float(b, (size_t[]){1, 0}, 1.0f);
    et_set_float(b, (size_t[]){1, 1}, 1.0f);

    // 인플레이스 덧셈 테스트
    ETTensor* result = et_add_inplace(a, b);
    TEST_ASSERT(result == a, "인플레이스 덧셈 반환값 확인");
    float val = et_get_float(a, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 2.0f), "인플레이스 덧셈 결과 확인 (0,0)");
    val = et_get_float(a, (size_t[]){1, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 5.0f), "인플레이스 덧셈 결과 확인 (1,1)");

    // 인플레이스 스칼라 곱셈 테스트
    result = et_mul_scalar_inplace(a, 2.0f);
    TEST_ASSERT(result == a, "인플레이스 스칼라 곱셈 반환값 확인");
    val = et_get_float(a, (size_t[]){0, 0});
    TEST_ASSERT(FLOAT_EQUAL(val, 4.0f), "인플레이스 스칼라 곱셈 결과 확인 (0,0)");
    val = et_get_float(a, (size_t[]){1, 1});
    TEST_ASSERT(FLOAT_EQUAL(val, 10.0f), "인플레이스 스칼라 곱셈 결과 확인 (1,1)");

    et_destroy_tensor(a);
    et_destroy_tensor(b);
    et_destroy_memory_pool(pool);
}

// =============================================================================
// 메인 테스트 함수
// =============================================================================

int main() {
    printf("LibEtude 텐서 시스템 단위 테스트\n");
    printf("================================\n");

    test_dtype_functions();
    test_tensor_creation();
    test_tensor_data_access();
    test_tensor_manipulation();
    test_tensor_operations();
    test_reduction_operations();
    test_new_reduction_operations();
    test_unary_operations();
    test_advanced_transformation();
    test_inplace_operations();

    print_test_summary();

    return (tests_run == tests_passed) ? 0 : 1;
}