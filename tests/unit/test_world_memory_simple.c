/**
 * @file test_world_memory_simple.c
 * @brief WORLD 메모리 관리자 간단한 테스트
 *
 * WorldMemoryManager의 기본 기능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// libetude 헤더 포함
#include "../../include/libetude/memory.h"
#include "../../include/libetude/types.h"
#include "../../include/libetude/error.h"

// WORLD 엔진 헤더 포함 (메모리 관리 부분만)
// 전체 world_engine.h를 포함하면 컴파일 오류가 발생할 수 있으므로 필요한 부분만 정의
typedef enum {
    WORLD_MEMORY_POOL_ANALYSIS = 0,
    WORLD_MEMORY_POOL_SYNTHESIS = 1,
    WORLD_MEMORY_POOL_CACHE = 2,
    WORLD_MEMORY_POOL_COUNT = 3
} WorldMemoryPoolType;

typedef struct WorldMemoryManager {
    ETMemoryPool* analysis_pool;
    ETMemoryPool* synthesis_pool;
    ETMemoryPool* cache_pool;

    size_t analysis_pool_size;
    size_t synthesis_pool_size;
    size_t cache_pool_size;

    size_t analysis_allocated;
    size_t synthesis_allocated;
    size_t cache_allocated;
    size_t peak_analysis_usage;
    size_t peak_synthesis_usage;
    size_t peak_cache_usage;

    int total_allocations;
    int total_deallocations;
    int active_allocations;

    bool enable_memory_alignment;
    bool enable_pool_preallocation;
    int alignment_size;

    bool is_initialized;
    bool enable_statistics;
} WorldMemoryManager;

// WORLD 메모리 관리 함수 선언
WorldMemoryManager* world_memory_manager_create(size_t analysis_size, size_t synthesis_size, size_t cache_size);
void world_memory_manager_destroy(WorldMemoryManager* manager);
void* world_memory_alloc(WorldMemoryManager* manager, size_t size, WorldMemoryPoolType pool_type);
void* world_memory_alloc_aligned(WorldMemoryManager* manager, size_t size, size_t alignment, WorldMemoryPoolType pool_type);
void world_memory_free(WorldMemoryManager* manager, void* ptr, WorldMemoryPoolType pool_type);
ETResult world_memory_pool_reset(WorldMemoryManager* manager, WorldMemoryPoolType pool_type);

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
#define TEST_ANALYSIS_POOL_SIZE (1024 * 1024)    // 1MB
#define TEST_SYNTHESIS_POOL_SIZE (512 * 1024)    // 512KB
#define TEST_CACHE_POOL_SIZE (256 * 1024)        // 256KB

/**
 * @brief WORLD 메모리 관리자 생성 및 해제 테스트
 */
static int test_world_memory_manager_create_destroy() {
    printf("Testing WORLD memory manager creation and destruction...\n");

    // 정상적인 생성 테스트
    WorldMemoryManager* manager = world_memory_manager_create(
        TEST_ANALYSIS_POOL_SIZE, TEST_SYNTHESIS_POOL_SIZE, TEST_CACHE_POOL_SIZE);

    TEST_ASSERT(manager != NULL, "Memory manager creation failed");
    TEST_ASSERT(manager->is_initialized == true, "Memory manager not initialized");
    TEST_ASSERT(manager->analysis_pool != NULL, "Analysis pool not created");
    TEST_ASSERT(manager->synthesis_pool != NULL, "Synthesis pool not created");
    TEST_ASSERT(manager->cache_pool != NULL, "Cache pool not created");

    // 해제 테스트
    world_memory_manager_destroy(manager);

    // 잘못된 파라미터 테스트
    WorldMemoryManager* invalid_manager = world_memory_manager_create(0, 1024, 1024);
    TEST_ASSERT(invalid_manager == NULL, "Invalid parameter should return NULL");

    TEST_PASS("WORLD memory manager creation and destruction");
}

/**
 * @brief WORLD 메모리 할당 및 해제 테스트
 */
static int test_world_memory_allocation() {
    printf("Testing WORLD memory allocation and deallocation...\n");

    WorldMemoryManager* manager = world_memory_manager_create(
        TEST_ANALYSIS_POOL_SIZE, TEST_SYNTHESIS_POOL_SIZE, TEST_CACHE_POOL_SIZE);
    TEST_ASSERT(manager != NULL, "Memory manager creation failed");

    // 분석용 메모리 할당 테스트
    void* analysis_ptr = world_memory_alloc(manager, 1024, WORLD_MEMORY_POOL_ANALYSIS);
    TEST_ASSERT(analysis_ptr != NULL, "Analysis memory allocation failed");

    // 합성용 메모리 할당 테스트
    void* synthesis_ptr = world_memory_alloc(manager, 512, WORLD_MEMORY_POOL_SYNTHESIS);
    TEST_ASSERT(synthesis_ptr != NULL, "Synthesis memory allocation failed");

    // 캐시용 메모리 할당 테스트
    void* cache_ptr = world_memory_alloc(manager, 256, WORLD_MEMORY_POOL_CACHE);
    TEST_ASSERT(cache_ptr != NULL, "Cache memory allocation failed");

    // 메모리 해제
    world_memory_free(manager, analysis_ptr, WORLD_MEMORY_POOL_ANALYSIS);
    world_memory_free(manager, synthesis_ptr, WORLD_MEMORY_POOL_SYNTHESIS);
    world_memory_free(manager, cache_ptr, WORLD_MEMORY_POOL_CACHE);

    world_memory_manager_destroy(manager);
    TEST_PASS("WORLD memory allocation and deallocation");
}

/**
 * @brief WORLD 정렬된 메모리 할당 테스트
 */
static int test_world_aligned_memory_allocation() {
    printf("Testing WORLD aligned memory allocation...\n");

    WorldMemoryManager* manager = world_memory_manager_create(
        TEST_ANALYSIS_POOL_SIZE, TEST_SYNTHESIS_POOL_SIZE, TEST_CACHE_POOL_SIZE);
    TEST_ASSERT(manager != NULL, "Memory manager creation failed");

    // 32바이트 정렬 메모리 할당
    void* aligned_ptr = world_memory_alloc_aligned(manager, 1024, 32, WORLD_MEMORY_POOL_ANALYSIS);
    TEST_ASSERT(aligned_ptr != NULL, "Aligned memory allocation failed");

    // 정렬 확인
    uintptr_t addr = (uintptr_t)aligned_ptr;
    TEST_ASSERT((addr & 31) == 0, "Memory not properly aligned to 32 bytes");

    world_memory_free(manager, aligned_ptr, WORLD_MEMORY_POOL_ANALYSIS);
    world_memory_manager_destroy(manager);
    TEST_PASS("WORLD aligned memory allocation");
}

/**
 * @brief WORLD 메모리 풀 리셋 테스트
 */
static int test_world_memory_pool_reset() {
    printf("Testing WORLD memory pool reset...\n");

    WorldMemoryManager* manager = world_memory_manager_create(
        TEST_ANALYSIS_POOL_SIZE, TEST_SYNTHESIS_POOL_SIZE, TEST_CACHE_POOL_SIZE);
    TEST_ASSERT(manager != NULL, "Memory manager creation failed");

    // 메모리 할당
    void* ptr1 = world_memory_alloc(manager, 1024, WORLD_MEMORY_POOL_ANALYSIS);
    void* ptr2 = world_memory_alloc(manager, 512, WORLD_MEMORY_POOL_ANALYSIS);
    TEST_ASSERT(ptr1 != NULL && ptr2 != NULL, "Memory allocation failed");

    // 풀 리셋
    ETResult result = world_memory_pool_reset(manager, WORLD_MEMORY_POOL_ANALYSIS);
    TEST_ASSERT(result == ET_SUCCESS, "Memory pool reset failed");

    world_memory_manager_destroy(manager);
    TEST_PASS("WORLD memory pool reset");
}

/**
 * @brief 모든 테스트 실행
 */
int main() {
    printf("=== WORLD Memory Manager Simple Tests ===\n\n");

    int total_tests = 0;
    int passed_tests = 0;

    // WORLD 메모리 관리자 테스트
    printf("--- WORLD Memory Manager Tests ---\n");
    total_tests++; if (test_world_memory_manager_create_destroy()) passed_tests++;
    total_tests++; if (test_world_memory_allocation()) passed_tests++;
    total_tests++; if (test_world_aligned_memory_allocation()) passed_tests++;
    total_tests++; if (test_world_memory_pool_reset()) passed_tests++;

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