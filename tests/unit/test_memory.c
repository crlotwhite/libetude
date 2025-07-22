/**
 * @file test_memory.c
 * @brief LibEtude 메모리 풀 단위 테스트
 */

#include "libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 테스트 상수
#define TEST_POOL_SIZE (1024 * 1024)  // 1MB
#define TEST_BLOCK_SIZE 256

// 간단한 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return; \
        } \
    } while(0)

// 테스트 함수들
static void test_memory_pool_creation(void) {
    printf("Testing memory pool creation...\n");

    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");
    TEST_ASSERT(pool->total_size >= TEST_POOL_SIZE, "Pool size mismatch");
    TEST_ASSERT(pool->used_size == 0, "Initial used size should be 0");

    et_destroy_memory_pool(pool);
    printf("✓ Memory pool creation tests passed\n");
}

static void test_dynamic_memory_pool(void) {
    printf("Testing dynamic memory pool...\n");

    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Dynamic pool creation failed");

    void* ptr1 = et_alloc_from_pool(pool, 128);
    TEST_ASSERT(ptr1 != NULL, "128 byte allocation failed");

    void* ptr2 = et_alloc_from_pool(pool, 256);
    TEST_ASSERT(ptr2 != NULL, "256 byte allocation failed");

    et_free_to_pool(pool, ptr1);
    et_free_to_pool(pool, ptr2);

    et_destroy_memory_pool(pool);
    printf("✓ Dynamic memory pool tests passed\n");
}

static void test_utility_functions(void) {
    printf("Testing utility functions...\n");

    TEST_ASSERT(et_align_size(100, 32) == 128, "Align size calculation failed");
    TEST_ASSERT(et_align_size(128, 32) == 128, "Already aligned size failed");

    void* aligned_ptr = aligned_alloc(64, 256);
    TEST_ASSERT(aligned_ptr != NULL, "Aligned allocation failed");
    TEST_ASSERT(et_is_aligned(aligned_ptr, 64) == true, "Aligned pointer check failed");

    free(aligned_ptr);
    printf("✓ Utility function tests passed\n");
}

int main(void) {
    printf("=== LibEtude Memory Pool Tests ===\n");

    test_memory_pool_creation();
    test_dynamic_memory_pool();
    test_utility_functions();

    printf("All memory pool tests completed!\n");
    return 0;
}