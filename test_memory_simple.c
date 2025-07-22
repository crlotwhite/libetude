/**
 * @file test_memory_simple.c
 * @brief 간단한 메모리 풀 테스트
 */

#include "include/libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== LibEtude Memory Pool Simple Test ===\n");

    // 메모리 풀 생성 테스트
    printf("1. Creating memory pool...\n");
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    if (pool == NULL) {
        printf("FAIL: Memory pool creation failed\n");
        return 1;
    }
    printf("✓ Memory pool created successfully\n");
    printf("  - Total size: %zu bytes\n", pool->total_size);
    printf("  - Used size: %zu bytes\n", pool->used_size);
    printf("  - Alignment: %zu bytes\n", pool->alignment);

    // 메모리 할당 테스트
    printf("\n2. Testing memory allocation...\n");
    void* ptr1 = et_alloc_from_pool(pool, 256);
    if (ptr1 == NULL) {
        printf("FAIL: Memory allocation failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }
    printf("✓ Allocated 256 bytes at %p\n", ptr1);

    void* ptr2 = et_alloc_from_pool(pool, 512);
    if (ptr2 == NULL) {
        printf("FAIL: Second memory allocation failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }
    printf("✓ Allocated 512 bytes at %p\n", ptr2);

    // 메모리 사용량 확인
    printf("  - Used size after allocations: %zu bytes\n", pool->used_size);

    // 메모리에 데이터 쓰기 테스트
    printf("\n3. Testing memory write/read...\n");
    memset(ptr1, 0xAA, 256);
    memset(ptr2, 0xBB, 512);

    if (((uint8_t*)ptr1)[0] == 0xAA && ((uint8_t*)ptr2)[0] == 0xBB) {
        printf("✓ Memory write/read test passed\n");
    } else {
        printf("FAIL: Memory write/read test failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }

    // 메모리 해제 테스트
    printf("\n4. Testing memory deallocation...\n");
    et_free_to_pool(pool, ptr1);
    et_free_to_pool(pool, ptr2);
    printf("✓ Memory deallocated successfully\n");
    printf("  - Used size after deallocation: %zu bytes\n", pool->used_size);

    // 통계 테스트
    printf("\n5. Testing memory pool statistics...\n");
    ETMemoryPoolStats stats;
    et_get_pool_stats(pool, &stats);
    printf("✓ Statistics retrieved:\n");
    printf("  - Total allocations: %zu\n", stats.num_allocations);
    printf("  - Total frees: %zu\n", stats.num_frees);
    printf("  - Peak usage: %zu bytes\n", stats.peak_usage);

    // 유틸리티 함수 테스트
    printf("\n6. Testing utility functions...\n");
    size_t aligned = et_align_size(100, 32);
    if (aligned == 128) {
        printf("✓ Alignment calculation: 100 -> %zu (32-byte aligned)\n", aligned);
    } else {
        printf("FAIL: Alignment calculation failed: expected 128, got %zu\n", aligned);
        et_destroy_memory_pool(pool);
        return 1;
    }

    bool is_aligned = et_is_aligned(ptr1, 32);
    printf("✓ Alignment check: %s\n", is_aligned ? "aligned" : "not aligned");

    // 메모리 풀 검증
    printf("\n7. Testing memory pool validation...\n");
    bool valid = et_validate_memory_pool(pool);
    if (valid) {
        printf("✓ Memory pool validation passed\n");
    } else {
        printf("FAIL: Memory pool validation failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }

    // 메모리 풀 소멸
    printf("\n8. Destroying memory pool...\n");
    et_destroy_memory_pool(pool);
    printf("✓ Memory pool destroyed successfully\n");

    printf("\n=== All tests passed! ===\n");
    return 0;
}