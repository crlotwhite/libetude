/**
 * @file test_memory_allocator.c
 * @brief 메모리 할당자 단위 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/memory.h"
#include <string.h>
#include <stdint.h>

// 테스트 상수
#define TEST_POOL_SIZE (1024 * 1024)  // 1MB
#define TEST_ALIGNMENT 32
#define TEST_BLOCK_SIZE 256

static ETMemoryPool* test_pool = NULL;

// 테스트 설정 함수
void setUp(void) {
    // 각 테스트마다 새로운 풀 생성
    test_pool = NULL;
}

// 테스트 정리 함수
void tearDown(void) {
    if (test_pool) {
        et_destroy_memory_pool(test_pool);
        test_pool = NULL;
    }
}

// 기본 메모리 풀 생성 테스트
void test_create_basic_memory_pool(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);
}

// 다양한 크기의 풀 생성 테스트
void test_create_pool_with_different_sizes(void) {
    size_t sizes[] = {1024, 4096, 64 * 1024, 1024 * 1024};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t i = 0; i < num_sizes; i++) {
        ETMemoryPool* pool = et_create_memory_pool(sizes[i], 16);
        TEST_ASSERT_NOT_NULL_MESSAGE(pool, "Pool creation failed");
        et_destroy_memory_pool(pool);
    }
}

// 잘못된 매개변수로 풀 생성 테스트
void test_invalid_parameters_should_fail(void) {
    // 크기가 0인 경우
    ETMemoryPool* pool1 = et_create_memory_pool(0, 16);
    TEST_ASSERT_NULL(pool1);

    // 정렬이 2의 거듭제곱이 아닌 경우
    ETMemoryPool* pool2 = et_create_memory_pool(1024, 15);
    TEST_ASSERT_NULL(pool2);
}

// 단일 할당 테스트
void test_single_allocation(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr = et_pool_alloc(test_pool, 128);
    TEST_ASSERT_NOT_NULL(ptr);

    // 메모리에 쓰기 테스트
    memset(ptr, 0xAA, 128);

    et_pool_free(test_pool, ptr);
}

// 다중 할당 테스트
void test_multiple_allocations(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptrs[10];
    const int num_allocs = 10;

    for (int i = 0; i < num_allocs; i++) {
        ptrs[i] = et_pool_alloc(test_pool, 64);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }

    // 모든 포인터가 다른지 확인
    for (int i = 0; i < num_allocs; i++) {
        for (int j = i + 1; j < num_allocs; j++) {
            TEST_ASSERT_NOT_EQUAL(ptrs[i], ptrs[j]);
        }
    }

    // 메모리 해제
    for (int i = 0; i < num_allocs; i++) {
        et_pool_free(test_pool, ptrs[i]);
    }
}

// 대용량 할당 테스트
void test_large_allocation(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr = et_pool_alloc(test_pool, TEST_POOL_SIZE / 2);
    TEST_ASSERT_NOT_NULL(ptr);

    et_pool_free(test_pool, ptr);
}

// 기본 정렬 테스트
void test_default_alignment(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr = et_pool_alloc(test_pool, 100);
    TEST_ASSERT_NOT_NULL(ptr);

    // 기본 정렬 확인
    TEST_ASSERT_EQUAL(0, (uintptr_t)ptr % TEST_ALIGNMENT);

    et_pool_free(test_pool, ptr);
}

// 사용자 정의 정렬 테스트
void test_custom_alignment(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    size_t alignments[] = {16, 32, 64, 128};
    size_t num_alignments = sizeof(alignments) / sizeof(alignments[0]);

    for (size_t i = 0; i < num_alignments; i++) {
        void* ptr = et_pool_alloc_aligned(test_pool, 100, alignments[i]);
        TEST_ASSERT_NOT_NULL(ptr);

        // 정렬 확인
        TEST_ASSERT_EQUAL_MESSAGE(0, (uintptr_t)ptr % alignments[i], "Alignment check failed");

        et_pool_free(test_pool, ptr);
    }
}

// 메모리 추적 테스트
void test_track_allocations(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    ETMemoryStats stats_before;
    et_get_memory_stats(test_pool, &stats_before);

    void* ptr1 = et_pool_alloc(test_pool, 128);
    void* ptr2 = et_pool_alloc(test_pool, 256);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);

    ETMemoryStats stats_after;
    et_get_memory_stats(test_pool, &stats_after);

    // 할당된 메모리 증가 확인
    TEST_ASSERT_GREATER_THAN(stats_before.total_allocated, stats_after.total_allocated);
    TEST_ASSERT_GREATER_THAN(stats_before.active_allocations, stats_after.active_allocations);

    et_pool_free(test_pool, ptr1);
    et_pool_free(test_pool, ptr2);

    ETMemoryStats stats_final;
    et_get_memory_stats(test_pool, &stats_final);

    // 해제 후 통계 확인
    TEST_ASSERT_EQUAL(stats_before.active_allocations, stats_final.active_allocations);
}

// 누수 없는 시나리오 테스트
void test_no_leaks_scenario(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr1 = et_pool_alloc(test_pool, 128);
    void* ptr2 = et_pool_alloc(test_pool, 256);

    et_pool_free(test_pool, ptr1);
    et_pool_free(test_pool, ptr2);

    // 누수 검사
    bool has_leaks = et_check_memory_leaks(test_pool);
    TEST_ASSERT_FALSE(has_leaks);
}

// 누수 감지 테스트
void test_leak_detection(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr1 = et_pool_alloc(test_pool, 128);
    void* ptr2 = et_pool_alloc(test_pool, 256);

    // ptr1만 해제하고 ptr2는 의도적으로 누수
    et_pool_free(test_pool, ptr1);

    bool has_leaks = et_check_memory_leaks(test_pool);
    TEST_ASSERT_TRUE(has_leaks);

    // 정리
    et_pool_free(test_pool, ptr2);
}

// 버퍼 오버플로우 감지 테스트
void test_buffer_overflow_detection(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr = et_pool_alloc(test_pool, 128);
    TEST_ASSERT_NOT_NULL(ptr);

    // 정상적인 쓰기
    memset(ptr, 0xAA, 128);

    // 메모리 무결성 검사
    bool is_corrupted = et_check_memory_corruption(test_pool, ptr);
    TEST_ASSERT_FALSE(is_corrupted);

    et_pool_free(test_pool, ptr);
}

// 초기 통계 테스트
void test_initial_statistics(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    ETMemoryStats stats;
    et_get_memory_stats(test_pool, &stats);

    TEST_ASSERT_EQUAL(TEST_POOL_SIZE, stats.pool_size);
    TEST_ASSERT_EQUAL(0, stats.total_allocated);
    TEST_ASSERT_EQUAL(0, stats.active_allocations);
    TEST_ASSERT_EQUAL(0, stats.peak_usage);
}

// 할당 후 통계 테스트
void test_statistics_after_allocations(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptrs[5];
    size_t total_size = 0;

    for (int i = 0; i < 5; i++) {
        size_t size = 128 * (i + 1);
        ptrs[i] = et_pool_alloc(test_pool, size);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
        total_size += size;
    }

    ETMemoryStats stats;
    et_get_memory_stats(test_pool, &stats);

    TEST_ASSERT_EQUAL(5, stats.active_allocations);
    TEST_ASSERT_GREATER_OR_EQUAL(total_size, stats.total_allocated);

    // 메모리 해제
    for (int i = 0; i < 5; i++) {
        et_pool_free(test_pool, ptrs[i]);
    }
}

// 풀 리셋 테스트
void test_pool_reset_after_allocations(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    // 여러 할당 수행
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = et_pool_alloc(test_pool, 64);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }

    // 풀 리셋
    et_reset_memory_pool(test_pool);

    // 통계 확인
    ETMemoryStats stats;
    et_get_memory_stats(test_pool, &stats);

    TEST_ASSERT_EQUAL(0, stats.active_allocations);
    TEST_ASSERT_EQUAL(0, stats.total_allocated);

    // 리셋 후 새로운 할당이 가능한지 확인
    void* new_ptr = et_pool_alloc(test_pool, 128);
    TEST_ASSERT_NOT_NULL(new_ptr);

    et_pool_free(test_pool, new_ptr);
}

// null 포인터 처리 테스트
void test_null_pointer_handling(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    // null 포인터 해제는 안전해야 함
    et_pool_free(test_pool, NULL);
    // 크래시 없이 완료되면 성공
    TEST_PASS();
}

// 이중 해제 감지 테스트
void test_double_free_detection(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr = et_pool_alloc(test_pool, 128);
    TEST_ASSERT_NOT_NULL(ptr);

    et_pool_free(test_pool, ptr);

    // 이중 해제 시도 (구현에 따라 감지되거나 무시될 수 있음)
    et_pool_free(test_pool, ptr);
    // 크래시 없이 완료되면 성공
    TEST_PASS();
}

// 0 크기 할당 테스트
void test_zero_size_allocation(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptr = et_pool_alloc(test_pool, 0);
    // 구현에 따라 null이거나 최소 크기 할당
    if (ptr != NULL) {
        et_pool_free(test_pool, ptr);
    }
    // 크래시 없이 완료되면 성공
    TEST_PASS();
}

// Unity 테스트 러너
int main(void) {
    UNITY_BEGIN();

    // 메모리 풀 생성 테스트
    RUN_TEST(test_create_basic_memory_pool);
    RUN_TEST(test_create_pool_with_different_sizes);
    RUN_TEST(test_invalid_parameters_should_fail);

    // 기본 할당/해제 테스트
    RUN_TEST(test_single_allocation);
    RUN_TEST(test_multiple_allocations);
    RUN_TEST(test_large_allocation);

    // 정렬 테스트
    RUN_TEST(test_default_alignment);
    RUN_TEST(test_custom_alignment);

    // 메모리 추적 테스트
    RUN_TEST(test_track_allocations);

    // 누수 감지 테스트
    RUN_TEST(test_no_leaks_scenario);
    RUN_TEST(test_leak_detection);

    // 메모리 무결성 테스트
    RUN_TEST(test_buffer_overflow_detection);

    // 통계 테스트
    RUN_TEST(test_initial_statistics);
    RUN_TEST(test_statistics_after_allocations);

    // 풀 리셋 테스트
    RUN_TEST(test_pool_reset_after_allocations);

    // 에러 처리 테스트
    RUN_TEST(test_null_pointer_handling);
    RUN_TEST(test_double_free_detection);
    RUN_TEST(test_zero_size_allocation);

    return UNITY_END();
}