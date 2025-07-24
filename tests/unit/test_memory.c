/**
 * @file test_memory.c
 * @brief LibEtude 메모리 풀 단위 테스트 (Unity)
 */

#include "unity.h"
#include "libetude/memory.h"
#include <string.h>

// 테스트 상수
#define TEST_POOL_SIZE (1024 * 1024)  // 1MB
#define TEST_BLOCK_SIZE 256

static ETMemoryPool* test_pool = NULL;

void setUp(void) {
    test_pool = NULL;
}

void tearDown(void) {
    if (test_pool) {
        et_destroy_memory_pool(test_pool);
        test_pool = NULL;
    }
}

void test_memory_pool_creation(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);
    TEST_ASSERT_GREATER_OR_EQUAL(TEST_POOL_SIZE, test_pool->total_size);
}

void test_invalid_pool_creation(void) {
    // 크기가 0인 풀
    ETMemoryPool* pool1 = et_create_memory_pool(0, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NULL(pool1);

    // 잘못된 정렬
    ETMemoryPool* pool2 = et_create_memory_pool(TEST_POOL_SIZE, 0);
    TEST_ASSERT_NULL(pool2);
}

void test_basic_allocation(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    // 단일 할당
    void* ptr = et_pool_alloc(test_pool, 128);
    TEST_ASSERT_NOT_NULL(ptr);

    // 메모리 쓰기 테스트
    memset(ptr, 0xAA, 128);

    et_pool_free(test_pool, ptr);
}

void test_multiple_allocations(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    void* ptrs[100];
    const int num_allocs = 100;

    // 여러 할당 수행
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

void test_alignment(void) {
    const size_t alignment = 64;
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, alignment);
    TEST_ASSERT_NOT_NULL(test_pool);

    // 여러 크기로 할당하여 정렬 확인
    size_t sizes[] = {1, 7, 15, 31, 63, 127, 255};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t i = 0; i < num_sizes; i++) {
        void* ptr = et_pool_alloc(test_pool, sizes[i]);
        TEST_ASSERT_NOT_NULL(ptr);

        // 정렬 확인
        char msg[64];
        snprintf(msg, sizeof(msg), "Allocation of size %zu not properly aligned", sizes[i]);
        TEST_ASSERT_EQUAL_MESSAGE(0, (uintptr_t)ptr % alignment, msg);

        et_pool_free(test_pool, ptr);
    }
}

void test_memory_statistics(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    ETMemoryStats stats_initial;
    et_get_memory_stats(test_pool, &stats_initial);

    TEST_ASSERT_EQUAL(0, stats_initial.total_allocated);
    TEST_ASSERT_EQUAL(0, stats_initial.active_allocations);

    // 할당 수행
    void* ptr1 = et_pool_alloc(test_pool, 128);
    void* ptr2 = et_pool_alloc(test_pool, 256);

    ETMemoryStats stats_after_alloc;
    et_get_memory_stats(test_pool, &stats_after_alloc);

    TEST_ASSERT_GREATER_THAN(stats_initial.total_allocated, stats_after_alloc.total_allocated);
    TEST_ASSERT_EQUAL(2, stats_after_alloc.active_allocations);

    // 해제 후 통계
    et_pool_free(test_pool, ptr1);
    et_pool_free(test_pool, ptr2);

    ETMemoryStats stats_after_free;
    et_get_memory_stats(test_pool, &stats_after_free);

    TEST_ASSERT_EQUAL(0, stats_after_free.active_allocations);
}

void test_pool_reset(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
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

void test_error_handling(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    // null 포인터 해제 (안전해야 함)
    et_pool_free(test_pool, NULL);
    TEST_PASS();

    // 0 크기 할당
    void* ptr = et_pool_alloc(test_pool, 0);
    if (ptr != NULL) {
        et_pool_free(test_pool, ptr);
    }
    TEST_PASS();

    // 매우 큰 할당 요청
    void* large_ptr = et_pool_alloc(test_pool, TEST_POOL_SIZE * 2);
    TEST_ASSERT_NULL(large_ptr);
}

void test_leak_detection(void) {
    test_pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT_NOT_NULL(test_pool);

    // 누수 없는 시나리오
    void* ptr1 = et_pool_alloc(test_pool, 128);
    void* ptr2 = et_pool_alloc(test_pool, 256);

    et_pool_free(test_pool, ptr1);
    et_pool_free(test_pool, ptr2);

    bool has_leaks = et_check_memory_leaks(test_pool);
    TEST_ASSERT_FALSE(has_leaks);

    // 의도적 누수 시나리오
    void* leak_ptr = et_pool_alloc(test_pool, 64);
    TEST_ASSERT_NOT_NULL(leak_ptr);

    has_leaks = et_check_memory_leaks(test_pool);
    TEST_ASSERT_TRUE(has_leaks);

    // 정리
    et_pool_free(test_pool, leak_ptr);
}

// Unity 테스트 러너
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_memory_pool_creation);
    RUN_TEST(test_invalid_pool_creation);
    RUN_TEST(test_basic_allocation);
    RUN_TEST(test_multiple_allocations);
    RUN_TEST(test_alignment);
    RUN_TEST(test_memory_statistics);
    RUN_TEST(test_pool_reset);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_leak_detection);

    return UNITY_END();
}