/**
 * @file test_memory_fixed.c
 * @brief 고정 크기 메모리 풀 테스트
 */

#include "include/libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== LibEtude Fixed Memory Pool Test ===\n");

    // 고정 크기 메모리 풀 생성
    printf("1. Creating fixed memory pool...\n");
    ETMemoryPoolOptions options = {
        .pool_type = ET_POOL_FIXED,
        .mem_type = ET_MEM_CPU,
        .alignment = 32,
        .block_size = 256,
        .min_block_size = 0,
        .thread_safe = true,
        .device_context = NULL
    };

    ETMemoryPool* pool = et_create_memory_pool_with_options(64 * 1024, &options);
    if (pool == NULL) {
        printf("FAIL: Fixed memory pool creation failed\n");
        return 1;
    }

    printf("✓ Fixed memory pool created successfully\n");
    printf("  - Total size: %zu bytes\n", pool->total_size);
    printf("  - Block size: %zu bytes\n", pool->block_size);
    printf("  - Number of blocks: %zu\n", pool->num_blocks);
    printf("  - Free blocks: %zu\n", pool->free_blocks);

    // 블록 할당 테스트
    printf("\n2. Testing block allocation...\n");
    void* blocks[10];

    for (int i = 0; i < 10; i++) {
        blocks[i] = et_alloc_from_pool(pool, 256);
        if (blocks[i] == NULL) {
            printf("FAIL: Block allocation %d failed\n", i);
            et_destroy_memory_pool(pool);
            return 1;
        }
        printf("✓ Allocated block %d at %p\n", i, blocks[i]);
    }

    printf("  - Free blocks after allocation: %zu\n", pool->free_blocks);
    printf("  - Used size: %zu bytes\n", pool->used_size);

    // 작은 크기 할당 테스트 (블록 크기보다 작음)
    printf("\n3. Testing small allocation...\n");
    void* small_ptr = et_alloc_from_pool(pool, 128);  // 256바이트 블록에 128바이트 할당
    if (small_ptr == NULL) {
        printf("FAIL: Small allocation failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }
    printf("✓ Small allocation (128 bytes) successful at %p\n", small_ptr);

    // 큰 크기 할당 테스트 (블록 크기보다 큼) - 실패해야 함
    printf("\n4. Testing oversized allocation (should fail)...\n");
    void* large_ptr = et_alloc_from_pool(pool, 512);  // 256바이트 블록에 512바이트 할당 시도
    if (large_ptr == NULL) {
        printf("✓ Oversized allocation correctly failed\n");
    } else {
        printf("FAIL: Oversized allocation should have failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }

    // 메모리에 데이터 쓰기/읽기 테스트
    printf("\n5. Testing memory write/read...\n");
    for (int i = 0; i < 10; i++) {
        memset(blocks[i], 0x10 + i, 256);
    }

    bool write_read_ok = true;
    for (int i = 0; i < 10; i++) {
        if (((uint8_t*)blocks[i])[0] != (0x10 + i)) {
            write_read_ok = false;
            break;
        }
    }

    if (write_read_ok) {
        printf("✓ Memory write/read test passed\n");
    } else {
        printf("FAIL: Memory write/read test failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }

    // 블록 해제 테스트
    printf("\n6. Testing block deallocation...\n");
    for (int i = 0; i < 5; i++) {
        et_free_to_pool(pool, blocks[i]);
        printf("✓ Freed block %d\n", i);
    }

    printf("  - Free blocks after partial deallocation: %zu\n", pool->free_blocks);

    // 해제된 블록 재할당 테스트
    printf("\n7. Testing block reallocation...\n");
    void* realloc_ptr = et_alloc_from_pool(pool, 200);
    if (realloc_ptr == NULL) {
        printf("FAIL: Block reallocation failed\n");
        et_destroy_memory_pool(pool);
        return 1;
    }
    printf("✓ Block reallocation successful at %p\n", realloc_ptr);

    // 나머지 블록들 해제
    printf("\n8. Cleaning up remaining blocks...\n");
    for (int i = 5; i < 10; i++) {
        et_free_to_pool(pool, blocks[i]);
    }
    et_free_to_pool(pool, small_ptr);
    et_free_to_pool(pool, realloc_ptr);

    printf("  - Free blocks after cleanup: %zu\n", pool->free_blocks);
    printf("  - Used size after cleanup: %zu bytes\n", pool->used_size);

    // 통계 확인
    printf("\n9. Testing statistics...\n");
    ETMemoryPoolStats stats;
    et_get_pool_stats(pool, &stats);
    printf("✓ Statistics:\n");
    printf("  - Total allocations: %zu\n", stats.num_allocations);
    printf("  - Total frees: %zu\n", stats.num_frees);
    printf("  - Peak usage: %zu bytes\n", stats.peak_usage);

    // 메모리 풀 리셋 테스트
    printf("\n10. Testing pool reset...\n");
    et_reset_pool(pool);
    printf("✓ Pool reset completed\n");
    printf("  - Free blocks after reset: %zu\n", pool->free_blocks);
    printf("  - Used size after reset: %zu bytes\n", pool->used_size);

    // 메모리 풀 소멸
    printf("\n11. Destroying memory pool...\n");
    et_destroy_memory_pool(pool);
    printf("✓ Memory pool destroyed successfully\n");

    printf("\n=== All fixed memory pool tests passed! ===\n");
    return 0;
}