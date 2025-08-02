/**
 * @file world_memory.c
 * @brief WORLD 메모리 관리 시스템 구현
 *
 * libetude 메모리 풀을 활용한 WORLD 전용 메모리 관리자 구현
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// libetude 헤더 포함
#include "../../../include/libetude/memory.h"
#include "../../../include/libetude/types.h"
#include "../../../include/libetude/error.h"

// WORLD 메모리 관리 타입 정의
typedef enum {
    WORLD_MEMORY_POOL_ANALYSIS = 0,
    WORLD_MEMORY_POOL_SYNTHESIS = 1,
    WORLD_MEMORY_POOL_CACHE = 2,
    WORLD_MEMORY_POOL_COUNT = 3
} WorldMemoryPoolType;

// 에러 코드 정의 (libetude에 없는 경우)
#ifndef ET_ERROR_INVALID_PARAMETER
#define ET_ERROR_INVALID_PARAMETER -1000
#endif

typedef struct WorldMemoryManager {
    // libetude 메모리 풀들
    ETMemoryPool* analysis_pool;
    ETMemoryPool* synthesis_pool;
    ETMemoryPool* cache_pool;

    // 메모리 풀 설정
    size_t analysis_pool_size;
    size_t synthesis_pool_size;
    size_t cache_pool_size;

    // 메모리 사용량 통계
    size_t analysis_allocated;
    size_t synthesis_allocated;
    size_t cache_allocated;
    size_t peak_analysis_usage;
    size_t peak_synthesis_usage;
    size_t peak_cache_usage;

    // 할당 통계
    int total_allocations;
    int total_deallocations;
    int active_allocations;

    // 성능 최적화 설정
    bool enable_memory_alignment;
    bool enable_pool_preallocation;
    int alignment_size;

    // 상태 정보
    bool is_initialized;
    bool enable_statistics;
} WorldMemoryManager;

// ============================================================================
// 함수 선언
// ============================================================================
void world_memory_manager_destroy(WorldMemoryManager* manager);
void* world_memory_alloc_aligned(WorldMemoryManager* manager, size_t size, size_t alignment, WorldMemoryPoolType pool_type);

// ============================================================================
// WorldMemoryManager 구현
// ============================================================================

WorldMemoryManager* world_memory_manager_create(size_t analysis_size,
                                                size_t synthesis_size,
                                                size_t cache_size) {
    if (analysis_size == 0 || synthesis_size == 0 || cache_size == 0) {
        return NULL;
    }

    WorldMemoryManager* manager = (WorldMemoryManager*)malloc(sizeof(WorldMemoryManager));
    if (!manager) {
        return NULL;
    }

    memset(manager, 0, sizeof(WorldMemoryManager));

    // 메모리 풀 크기 설정
    manager->analysis_pool_size = analysis_size;
    manager->synthesis_pool_size = synthesis_size;
    manager->cache_pool_size = cache_size;

    // libetude 메모리 풀 생성
    manager->analysis_pool = et_create_memory_pool(analysis_size, ET_DEFAULT_ALIGNMENT);
    manager->synthesis_pool = et_create_memory_pool(synthesis_size, ET_DEFAULT_ALIGNMENT);
    manager->cache_pool = et_create_memory_pool(cache_size, ET_DEFAULT_ALIGNMENT);

    if (!manager->analysis_pool || !manager->synthesis_pool || !manager->cache_pool) {
        world_memory_manager_destroy(manager);
        return NULL;
    }

    // 기본 설정
    manager->enable_memory_alignment = true;
    manager->enable_pool_preallocation = true;
    manager->alignment_size = 32; // SIMD 최적화를 위한 32바이트 정렬
    manager->enable_statistics = true;
    manager->is_initialized = true;

    return manager;
}

void world_memory_manager_destroy(WorldMemoryManager* manager) {
    if (!manager) {
        return;
    }

    // libetude 메모리 풀 해제
    if (manager->analysis_pool) {
        et_destroy_memory_pool(manager->analysis_pool);
    }
    if (manager->synthesis_pool) {
        et_destroy_memory_pool(manager->synthesis_pool);
    }
    if (manager->cache_pool) {
        et_destroy_memory_pool(manager->cache_pool);
    }

    free(manager);
}

void* world_memory_alloc(WorldMemoryManager* manager, size_t size, WorldMemoryPoolType pool_type) {
    if (!manager || !manager->is_initialized || size == 0) {
        return NULL;
    }

    ETMemoryPool* pool = NULL;
    size_t* allocated_counter = NULL;
    size_t* peak_counter = NULL;

    // 풀 타입에 따라 적절한 풀 선택
    switch (pool_type) {
        case WORLD_MEMORY_POOL_ANALYSIS:
            pool = manager->analysis_pool;
            allocated_counter = &manager->analysis_allocated;
            peak_counter = &manager->peak_analysis_usage;
            break;
        case WORLD_MEMORY_POOL_SYNTHESIS:
            pool = manager->synthesis_pool;
            allocated_counter = &manager->synthesis_allocated;
            peak_counter = &manager->peak_synthesis_usage;
            break;
        case WORLD_MEMORY_POOL_CACHE:
            pool = manager->cache_pool;
            allocated_counter = &manager->cache_allocated;
            peak_counter = &manager->peak_cache_usage;
            break;
        default:
            return NULL;
    }

    if (!pool) {
        return NULL;
    }

    void* ptr = NULL;

    // 메모리 정렬이 활성화된 경우
    if (manager->enable_memory_alignment) {
        ptr = world_memory_alloc_aligned(manager, size, manager->alignment_size, pool_type);
    } else {
        ptr = et_alloc_from_pool(pool, size);
    }

    if (ptr && manager->enable_statistics) {
        // 통계 업데이트
        *allocated_counter += size;
        if (*allocated_counter > *peak_counter) {
            *peak_counter = *allocated_counter;
        }
        manager->total_allocations++;
        manager->active_allocations++;
    }

    return ptr;
}

void world_memory_free(WorldMemoryManager* manager, void* ptr, WorldMemoryPoolType pool_type) {
    if (!manager || !manager->is_initialized || !ptr) {
        return;
    }

    ETMemoryPool* pool = NULL;

    // 풀 타입에 따라 적절한 풀 선택
    switch (pool_type) {
        case WORLD_MEMORY_POOL_ANALYSIS:
            pool = manager->analysis_pool;
            break;
        case WORLD_MEMORY_POOL_SYNTHESIS:
            pool = manager->synthesis_pool;
            break;
        case WORLD_MEMORY_POOL_CACHE:
            pool = manager->cache_pool;
            break;
        default:
            return;
    }

    if (!pool) {
        return;
    }

    // libetude 메모리 풀에서 해제
    et_free_to_pool(pool, ptr);

    if (manager->enable_statistics) {
        // 통계 업데이트 (정확한 크기를 알 수 없으므로 근사치 사용)
        manager->total_deallocations++;
        manager->active_allocations--;
    }
}

void* world_memory_alloc_aligned(WorldMemoryManager* manager, size_t size,
                                size_t alignment, WorldMemoryPoolType pool_type) {
    if (!manager || !manager->is_initialized || size == 0 || alignment == 0) {
        return NULL;
    }

    ETMemoryPool* pool = NULL;

    // 풀 타입에 따라 적절한 풀 선택
    switch (pool_type) {
        case WORLD_MEMORY_POOL_ANALYSIS:
            pool = manager->analysis_pool;
            break;
        case WORLD_MEMORY_POOL_SYNTHESIS:
            pool = manager->synthesis_pool;
            break;
        case WORLD_MEMORY_POOL_CACHE:
            pool = manager->cache_pool;
            break;
        default:
            return NULL;
    }

    if (!pool) {
        return NULL;
    }

    // 정렬된 메모리 할당
    void* aligned_ptr = et_alloc_aligned_from_pool(pool, size, alignment);
    if (!aligned_ptr) {
        return NULL;
    }

    return aligned_ptr;
}

ETResult world_memory_pool_reset(WorldMemoryManager* manager, WorldMemoryPoolType pool_type) {
    if (!manager || !manager->is_initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETMemoryPool* pool = NULL;
    size_t* allocated_counter = NULL;

    // 풀 타입에 따라 적절한 풀 선택
    switch (pool_type) {
        case WORLD_MEMORY_POOL_ANALYSIS:
            pool = manager->analysis_pool;
            allocated_counter = &manager->analysis_allocated;
            break;
        case WORLD_MEMORY_POOL_SYNTHESIS:
            pool = manager->synthesis_pool;
            allocated_counter = &manager->synthesis_allocated;
            break;
        case WORLD_MEMORY_POOL_CACHE:
            pool = manager->cache_pool;
            allocated_counter = &manager->cache_allocated;
            break;
        default:
            return ET_ERROR_INVALID_PARAMETER;
    }

    if (!pool) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 메모리 풀 리셋
    et_reset_pool(pool);
    ETResult result = ET_SUCCESS;

    if (result == ET_SUCCESS && manager->enable_statistics) {
        // 통계 리셋
        *allocated_counter = 0;
        // 피크 사용량은 유지
    }

    return result;
}

ETResult world_memory_get_statistics(WorldMemoryManager* manager, WorldMemoryPoolType pool_type,
                                     size_t* allocated, size_t* peak_usage, int* allocation_count) {
    if (!manager || !manager->is_initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!allocated || !peak_usage || !allocation_count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 풀 타입에 따라 통계 반환
    switch (pool_type) {
        case WORLD_MEMORY_POOL_ANALYSIS:
            *allocated = manager->analysis_allocated;
            *peak_usage = manager->peak_analysis_usage;
            break;
        case WORLD_MEMORY_POOL_SYNTHESIS:
            *allocated = manager->synthesis_allocated;
            *peak_usage = manager->peak_synthesis_usage;
            break;
        case WORLD_MEMORY_POOL_CACHE:
            *allocated = manager->cache_allocated;
            *peak_usage = manager->peak_cache_usage;
            break;
        default:
            return ET_ERROR_INVALID_PARAMETER;
    }

    *allocation_count = manager->active_allocations;

    return ET_SUCCESS;
}

ETResult world_memory_check_leaks(WorldMemoryManager* manager, size_t* leaked_bytes, int* leaked_allocations) {
    if (!manager || !manager->is_initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!leaked_bytes || !leaked_allocations) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 간단한 누수 검사 (활성 할당 개수 기반)
    *leaked_allocations = manager->active_allocations;
    *leaked_bytes = manager->analysis_allocated + manager->synthesis_allocated + manager->cache_allocated;

    return ET_SUCCESS;
}