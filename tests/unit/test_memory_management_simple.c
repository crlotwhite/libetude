/**
 * @file test_memory_management_simple.c
 * @brief WORLD 메모리 관리 및 캐싱 시스템 간단한 테스트
 *
 * 기본적인 메모리 관리 기능만 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <math.h>

// libetude 헤더 포함
#include "../../include/libetude/memory.h"
#include "../../include/libetude/types.h"
#include "../../include/libetude/error.h"

// 테스트 유틸리티 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return 0; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        printf("PASS: %s - %s\n", __func__, message); \
        return 1; \
    } while(0)

// 테스트 상수
#define TEST_POOL_SIZE (1024 * 1024)    // 1MB

/**
 * @brief 기본 메모리 풀 생성 및 해제 테스트
 */
static int test_basic_memory_pool() {
    printf("Testing basic memory pool creation and destruction...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 기본 정보 확인
    ETMemoryPoolStats stats;
    et_get_pool_stats(pool, &stats);
    TEST_ASSERT(stats.total_size > 0, "Pool total size should be positive");
    TEST_ASSERT(stats.used_size == 0, "Initial used size should be 0");

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Basic memory pool operations");
}

/**
 * @brief 메모리 할당 및 해제 테스트
 */
static int test_memory_allocation() {
    printf("Testing memory allocation and deallocation...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 메모리 할당
    void* ptr1 = et_alloc_from_pool(pool, 1024);
    TEST_ASSERT(ptr1 != NULL, "Memory allocation failed");

    void* ptr2 = et_alloc_from_pool(pool, 512);
    TEST_ASSERT(ptr2 != NULL, "Second memory allocation failed");

    // 통계 확인
    ETMemoryPoolStats stats;
    et_get_pool_stats(pool, &stats);
    TEST_ASSERT(stats.used_size > 0, "Used size should be positive after allocation");

    // 메모리 해제
    et_free_to_pool(pool, ptr1);
    et_free_to_pool(pool, ptr2);

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Memory allocation and deallocation");
}

/**
 * @brief 정렬된 메모리 할당 테스트
 */
static int test_aligned_memory_allocation() {
    printf("Testing aligned memory allocation...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 32바이트 정렬 메모리 할당
    void* aligned_ptr = et_alloc_aligned_from_pool(pool, 1024, 32);
    TEST_ASSERT(aligned_ptr != NULL, "Aligned memory allocation failed");

    // 정렬 확인
    uintptr_t addr = (uintptr_t)aligned_ptr;
    TEST_ASSERT((addr & 31) == 0, "Memory not properly aligned to 32 bytes");

    // 메모리 해제
    et_free_to_pool(pool, aligned_ptr);

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Aligned memory allocation");
}

/**
 * @brief 메모리 풀 리셋 테스트
 */
static int test_memory_pool_reset() {
    printf("Testing memory pool reset...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 메모리 할당
    void* ptr1 = et_alloc_from_pool(pool, 1024);
    void* ptr2 = et_alloc_from_pool(pool, 512);
    TEST_ASSERT(ptr1 != NULL && ptr2 != NULL, "Memory allocation failed");

    // 풀 리셋
    et_reset_pool(pool);

    // 리셋 후 통계 확인
    ETMemoryPoolStats stats;
    et_get_pool_stats(pool, &stats);
    TEST_ASSERT(stats.used_size == 0, "Used size should be 0 after reset");

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Memory pool reset");
}

/**
 * @brief 메모리 풀 통계 테스트
 */
static int test_memory_pool_statistics() {
    printf("Testing memory pool statistics...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 초기 통계 확인
    ETMemoryPoolStats stats;
    et_get_pool_stats(pool, &stats);
    TEST_ASSERT(stats.total_size > 0, "Total size should be positive");
    TEST_ASSERT(stats.used_size == 0, "Initial used size should be 0");
    TEST_ASSERT(stats.num_allocations == 0, "Initial allocation count should be 0");

    // 메모리 할당
    void* ptr = et_alloc_from_pool(pool, 1024);
    TEST_ASSERT(ptr != NULL, "Memory allocation failed");

    // 할당 후 통계 확인
    et_get_pool_stats(pool, &stats);
    TEST_ASSERT(stats.used_size > 0, "Used size should be positive after allocation");
    TEST_ASSERT(stats.num_allocations > 0, "Allocation count should be positive");

    // 메모리 해제
    et_free_to_pool(pool, ptr);

    // 해제 후 통계 확인
    et_get_pool_stats(pool, &stats);
    TEST_ASSERT(stats.num_frees > 0, "Free count should be positive after deallocation");

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Memory pool statistics");
}

/**
 * @brief 메모리 풀 유효성 검사 테스트
 */
static int test_memory_pool_validation() {
    printf("Testing memory pool validation...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 유효성 검사
    bool is_valid = et_validate_memory_pool(pool);
    TEST_ASSERT(is_valid == true, "Memory pool should be valid");

    // NULL 포인터 검사
    is_valid = et_validate_memory_pool(NULL);
    TEST_ASSERT(is_valid == false, "NULL pool should be invalid");

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Memory pool validation");
}

/**
 * @brief 메모리 사용량 최적화 테스트 (요구사항 6.2)
 */
static int test_memory_optimization() {
    printf("Testing memory usage optimization...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 다양한 크기의 메모리 할당으로 단편화 유발
    void* ptrs[20];
    for (int i = 0; i < 20; i++) {
        ptrs[i] = et_alloc_from_pool(pool, 64 * (i + 1));
        TEST_ASSERT(ptrs[i] != NULL, "Memory allocation failed");
    }

    // 일부 메모리 해제 (단편화 증가)
    for (int i = 0; i < 20; i += 2) {
        et_free_to_pool(pool, ptrs[i]);
    }

    // 메모리 풀 압축 테스트
    ETResult result = et_memory_pool_compact(pool);
    TEST_ASSERT(result == ET_SUCCESS, "Memory pool compaction failed");

    // 통계 확인
    ETMemoryPoolStats stats;
    et_get_pool_stats(pool, &stats);
    TEST_ASSERT(stats.used_size > 0, "Used size should be positive");

    // 나머지 메모리 해제
    for (int i = 1; i < 20; i += 2) {
        et_free_to_pool(pool, ptrs[i]);
    }

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Memory usage optimization");
}

/**
 * @brief 메모리 누수 방지 테스트
 */
static int test_memory_leak_prevention() {
    printf("Testing memory leak prevention...\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(TEST_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 메모리 할당
    void* ptr1 = et_alloc_from_pool(pool, 1024);
    void* ptr2 = et_alloc_from_pool(pool, 2048);
    TEST_ASSERT(ptr1 != NULL && ptr2 != NULL, "Memory allocation failed");

    // 통계로 할당 상태 확인
    ETMemoryPoolStats stats_before;
    et_get_pool_stats(pool, &stats_before);
    TEST_ASSERT(stats_before.num_allocations >= 2, "Allocation count should be at least 2");

    // 메모리 해제
    et_free_to_pool(pool, ptr1);
    et_free_to_pool(pool, ptr2);

    // 해제 후 통계 확인
    ETMemoryPoolStats stats_after;
    et_get_pool_stats(pool, &stats_after);
    TEST_ASSERT(stats_after.num_frees >= 2, "Free count should be at least 2");

    // 메모리 풀 리셋으로 완전 정리
    et_reset_pool(pool);

    // 리셋 후 통계 확인
    ETMemoryPoolStats stats_reset;
    et_get_pool_stats(pool, &stats_reset);
    TEST_ASSERT(stats_reset.used_size == 0, "Used size should be 0 after reset");

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Memory leak prevention");
}

/**
 * @brief 메모리 풀 효율성 테스트
 */
static int test_memory_pool_efficiency() {
    printf("Testing memory pool efficiency...\n");

    // 작은 메모리 풀 생성 (효율성 테스트용)
    const size_t small_pool_size = 64 * 1024; // 64KB
    ETMemoryPool* pool = et_create_memory_pool(small_pool_size, ET_DEFAULT_ALIGNMENT);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 초기 통계
    ETMemoryPoolStats initial_stats;
    et_get_pool_stats(pool, &initial_stats);
    TEST_ASSERT(initial_stats.total_size <= small_pool_size, "Pool size should not exceed limit");

    // 작은 블록들을 많이 할당
    const int num_small_blocks = 100;
    const size_t small_block_size = 256;
    void* small_ptrs[num_small_blocks];
    int successful_allocations = 0;

    for (int i = 0; i < num_small_blocks; i++) {
        small_ptrs[i] = et_alloc_from_pool(pool, small_block_size);
        if (small_ptrs[i] != NULL) {
            successful_allocations++;
        } else {
            break; // 풀이 가득 참
        }
    }

    TEST_ASSERT(successful_allocations > 0, "Should be able to allocate at least some blocks");

    // 할당 효율성 계산
    ETMemoryPoolStats alloc_stats;
    et_get_pool_stats(pool, &alloc_stats);
    double efficiency = (double)alloc_stats.used_size / alloc_stats.total_size;
    TEST_ASSERT(efficiency > 0.0, "Memory efficiency should be positive");

    printf("Memory efficiency: %.2f%% (%zu/%zu bytes)\n",
           efficiency * 100.0, alloc_stats.used_size, alloc_stats.total_size);

    // 할당된 메모리 해제
    for (int i = 0; i < successful_allocations; i++) {
        et_free_to_pool(pool, small_ptrs[i]);
    }

    // 메모리 풀 해제
    et_destroy_memory_pool(pool);

    TEST_PASS("Memory pool efficiency");
}

/**
 * @brief 모든 테스트 실행
 */
int main() {
    printf("=== WORLD Memory Management Simple Tests ===\n\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 기본 메모리 풀 테스트
    printf("--- Basic Memory Pool Tests ---\n");
    total_tests++; if (test_basic_memory_pool()) passed_tests++;
    total_tests++; if (test_memory_allocation()) passed_tests++;
    total_tests++; if (test_aligned_memory_allocation()) passed_tests++;
    total_tests++; if (test_memory_pool_reset()) passed_tests++;
    total_tests++; if (test_memory_pool_statistics()) passed_tests++;
    total_tests++; if (test_memory_pool_validation()) passed_tests++;

    // 메모리 최적화 테스트 (요구사항 6.2)
    printf("\n--- Memory Optimization Tests ---\n");
    total_tests++; if (test_memory_optimization()) passed_tests++;
    total_tests++; if (test_memory_leak_prevention()) passed_tests++;
    total_tests++; if (test_memory_pool_efficiency()) passed_tests++;

    // 결과 출력
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed tests: %d\n", passed_tests);
    printf("Failed tests: %d\n", total_tests - passed_tests);
    printf("Success rate: %.1f%%\n", (double)passed_tests / total_tests * 100.0);

    if (passed_tests == total_tests) {
        printf("\nAll tests PASSED! ✓\n");
        return 0;
    } else {
        printf("\nSome tests FAILED! ✗\n");
        return 1;
    }
}