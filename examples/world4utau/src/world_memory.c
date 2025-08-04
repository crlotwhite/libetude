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

// ============================================================================
// 메모리 사용량 최적화 함수들 (요구사항 6.2)
// ============================================================================

/**
 * @brief 메모리 풀 효율성 개선
 */
ETResult world_memory_optimize_pools(WorldMemoryManager* manager) {
    if (!manager || !manager->is_initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETResult result = ET_SUCCESS;

    // 각 풀의 사용 패턴 분석
    size_t analysis_usage = manager->peak_analysis_usage;
    size_t synthesis_usage = manager->peak_synthesis_usage;
    size_t cache_usage = manager->peak_cache_usage;

    // 풀 크기 동적 조정 (사용량의 120%로 설정하여 여유분 확보)
    size_t optimal_analysis_size = analysis_usage * 12 / 10;
    size_t optimal_synthesis_size = synthesis_usage * 12 / 10;
    size_t optimal_cache_size = cache_usage * 12 / 10;

    // 최소 크기 보장
    if (optimal_analysis_size < 64 * 1024) optimal_analysis_size = 64 * 1024;  // 64KB
    if (optimal_synthesis_size < 128 * 1024) optimal_synthesis_size = 128 * 1024;  // 128KB
    if (optimal_cache_size < 32 * 1024) optimal_cache_size = 32 * 1024;  // 32KB

    // 풀 크기가 현재보다 작아질 수 있는 경우에만 재생성
    if (optimal_analysis_size < manager->analysis_pool_size ||
        optimal_synthesis_size < manager->synthesis_pool_size ||
        optimal_cache_size < manager->cache_pool_size) {

        // 기존 풀 해제
        et_destroy_memory_pool(manager->analysis_pool);
        et_destroy_memory_pool(manager->synthesis_pool);
        et_destroy_memory_pool(manager->cache_pool);

        // 최적화된 크기로 새 풀 생성
        manager->analysis_pool = et_create_memory_pool(optimal_analysis_size, manager->alignment_size);
        manager->synthesis_pool = et_create_memory_pool(optimal_synthesis_size, manager->alignment_size);
        manager->cache_pool = et_create_memory_pool(optimal_cache_size, manager->alignment_size);

        if (!manager->analysis_pool || !manager->synthesis_pool || !manager->cache_pool) {
            return ET_ERROR_OUT_OF_MEMORY;
        }

        // 새로운 풀 크기 저장
        manager->analysis_pool_size = optimal_analysis_size;
        manager->synthesis_pool_size = optimal_synthesis_size;
        manager->cache_pool_size = optimal_cache_size;

        // 사용량 통계 리셋
        manager->analysis_allocated = 0;
        manager->synthesis_allocated = 0;
        manager->cache_allocated = 0;
    }

    return result;
}

/**
 * @brief 메모리 풀 압축 (단편화 해결)
 */
ETResult world_memory_compact_pools(WorldMemoryManager* manager) {
    if (!manager || !manager->is_initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // libetude 메모리 풀 압축 기능 사용
    ETResult result1 = et_memory_pool_compact(manager->analysis_pool);
    ETResult result2 = et_memory_pool_compact(manager->synthesis_pool);
    ETResult result3 = et_memory_pool_compact(manager->cache_pool);

    // 하나라도 실패하면 실패로 간주
    if (result1 != ET_SUCCESS || result2 != ET_SUCCESS || result3 != ET_SUCCESS) {
        return ET_ERROR_OPERATION_FAILED;
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 사용량 모니터링 및 경고
 */
ETResult world_memory_monitor_usage(WorldMemoryManager* manager, double warning_threshold) {
    if (!manager || !manager->is_initialized || warning_threshold <= 0.0 || warning_threshold > 1.0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 각 풀의 사용률 계산
    double analysis_usage_ratio = (double)manager->analysis_allocated / manager->analysis_pool_size;
    double synthesis_usage_ratio = (double)manager->synthesis_allocated / manager->synthesis_pool_size;
    double cache_usage_ratio = (double)manager->cache_allocated / manager->cache_pool_size;

    // 경고 임계값 초과 검사
    if (analysis_usage_ratio > warning_threshold) {
        printf("경고: 분석 메모리 풀 사용률 높음: %.1f%% (임계값: %.1f%%)\n",
               analysis_usage_ratio * 100.0, warning_threshold * 100.0);
    }

    if (synthesis_usage_ratio > warning_threshold) {
        printf("경고: 합성 메모리 풀 사용률 높음: %.1f%% (임계값: %.1f%%)\n",
               synthesis_usage_ratio * 100.0, warning_threshold * 100.0);
    }

    if (cache_usage_ratio > warning_threshold) {
        printf("경고: 캐시 메모리 풀 사용률 높음: %.1f%% (임계값: %.1f%%)\n",
               cache_usage_ratio * 100.0, warning_threshold * 100.0);
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 누수 방지를 위한 자동 정리
 */
ETResult world_memory_auto_cleanup(WorldMemoryManager* manager, int max_idle_time_ms) {
    if (!manager || !manager->is_initialized || max_idle_time_ms < 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    static int last_cleanup_time = 0;
    static int last_allocation_count = 0;

    // 현재 시간 (간단한 구현을 위해 할당 카운터 사용)
    int current_time = manager->total_allocations;

    // 일정 시간 동안 새로운 할당이 없으면 정리 수행
    if (current_time - last_cleanup_time > max_idle_time_ms / 10 &&  // 대략적인 시간 계산
        manager->total_allocations == last_allocation_count) {

        // 메모리 풀 압축
        world_memory_compact_pools(manager);

        // 사용하지 않는 캐시 정리
        if (manager->cache_allocated > 0) {
            et_reset_pool(manager->cache_pool);
            manager->cache_allocated = 0;
        }

        last_cleanup_time = current_time;
    }

    last_allocation_count = manager->total_allocations;
    return ET_SUCCESS;
}

/**
 * @brief 메모리 사용량 프로파일링
 */
typedef struct {
    size_t total_allocated;
    size_t peak_usage;
    size_t current_usage;
    double fragmentation_ratio;
    int allocation_count;
    int deallocation_count;
    double average_allocation_size;
} WorldMemoryProfile;

ETResult world_memory_get_profile(WorldMemoryManager* manager, WorldMemoryProfile* profile) {
    if (!manager || !manager->is_initialized || !profile) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(profile, 0, sizeof(WorldMemoryProfile));

    // 전체 통계 계산
    profile->total_allocated = manager->analysis_pool_size + manager->synthesis_pool_size + manager->cache_pool_size;
    profile->peak_usage = manager->peak_analysis_usage + manager->peak_synthesis_usage + manager->peak_cache_usage;
    profile->current_usage = manager->analysis_allocated + manager->synthesis_allocated + manager->cache_allocated;
    profile->allocation_count = manager->total_allocations;
    profile->deallocation_count = manager->total_deallocations;

    // 평균 할당 크기 계산
    if (manager->total_allocations > 0) {
        profile->average_allocation_size = (double)profile->peak_usage / manager->total_allocations;
    }

    // 단편화 비율 추정 (사용률 기반)
    if (profile->total_allocated > 0) {
        double usage_ratio = (double)profile->current_usage / profile->total_allocated;
        profile->fragmentation_ratio = 1.0 - usage_ratio;
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 최적화 설정 조정
 */
ETResult world_memory_set_optimization_settings(WorldMemoryManager* manager,
                                               bool enable_alignment,
                                               bool enable_preallocation,
                                               int alignment_size) {
    if (!manager || !manager->is_initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (alignment_size <= 0 || (alignment_size & (alignment_size - 1)) != 0) {
        return ET_ERROR_INVALID_PARAMETER;  // alignment_size는 2의 거듭제곱이어야 함
    }

    manager->enable_memory_alignment = enable_alignment;
    manager->enable_pool_preallocation = enable_preallocation;
    manager->alignment_size = alignment_size;

    return ET_SUCCESS;
}

/**
 * @brief 메모리 풀 사전 할당 (성능 최적화)
 */
ETResult world_memory_preallocate_pools(WorldMemoryManager* manager) {
    if (!manager || !manager->is_initialized) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!manager->enable_pool_preallocation) {
        return ET_SUCCESS;  // 사전 할당이 비활성화됨
    }

    // 각 풀에서 작은 블록들을 미리 할당하여 초기화
    const size_t prealloc_sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
    const int num_sizes = sizeof(prealloc_sizes) / sizeof(prealloc_sizes[0]);

    for (int i = 0; i < num_sizes; i++) {
        // 분석 풀 사전 할당
        void* analysis_ptr = et_alloc_from_pool(manager->analysis_pool, prealloc_sizes[i]);
        if (analysis_ptr) {
            et_free_to_pool(manager->analysis_pool, analysis_ptr);
        }

        // 합성 풀 사전 할당
        void* synthesis_ptr = et_alloc_from_pool(manager->synthesis_pool, prealloc_sizes[i]);
        if (synthesis_ptr) {
            et_free_to_pool(manager->synthesis_pool, synthesis_ptr);
        }

        // 캐시 풀 사전 할당
        void* cache_ptr = et_alloc_from_pool(manager->cache_pool, prealloc_sizes[i]);
        if (cache_ptr) {
            et_free_to_pool(manager->cache_pool, cache_ptr);
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 사용량 리포트 생성
 */
void world_memory_print_report(WorldMemoryManager* manager) {
    if (!manager || !manager->is_initialized) {
        printf("메모리 관리자가 초기화되지 않음\n");
        return;
    }

    printf("\n=== WORLD 메모리 사용량 리포트 ===\n");

    // 풀별 사용량
    printf("분석 풀: %zu/%zu bytes (%.1f%%), 피크: %zu bytes\n",
           manager->analysis_allocated, manager->analysis_pool_size,
           (double)manager->analysis_allocated / manager->analysis_pool_size * 100.0,
           manager->peak_analysis_usage);

    printf("합성 풀: %zu/%zu bytes (%.1f%%), 피크: %zu bytes\n",
           manager->synthesis_allocated, manager->synthesis_pool_size,
           (double)manager->synthesis_allocated / manager->synthesis_pool_size * 100.0,
           manager->peak_synthesis_usage);

    printf("캐시 풀: %zu/%zu bytes (%.1f%%), 피크: %zu bytes\n",
           manager->cache_allocated, manager->cache_pool_size,
           (double)manager->cache_allocated / manager->cache_pool_size * 100.0,
           manager->peak_cache_usage);

    // 전체 통계
    size_t total_allocated = manager->analysis_allocated + manager->synthesis_allocated + manager->cache_allocated;
    size_t total_pool_size = manager->analysis_pool_size + manager->synthesis_pool_size + manager->cache_pool_size;
    size_t total_peak = manager->peak_analysis_usage + manager->peak_synthesis_usage + manager->peak_cache_usage;

    printf("\n전체 사용량: %zu/%zu bytes (%.1f%%)\n",
           total_allocated, total_pool_size,
           (double)total_allocated / total_pool_size * 100.0);

    printf("전체 피크 사용량: %zu bytes\n", total_peak);

    // 할당 통계
    printf("\n할당 통계:\n");
    printf("  총 할당: %d회\n", manager->total_allocations);
    printf("  총 해제: %d회\n", manager->total_deallocations);
    printf("  활성 할당: %d개\n", manager->active_allocations);

    if (manager->total_allocations > 0) {
        printf("  평균 할당 크기: %.1f bytes\n",
               (double)total_peak / manager->total_allocations);
    }

    // 최적화 설정
    printf("\n최적화 설정:\n");
    printf("  메모리 정렬: %s (%d bytes)\n",
           manager->enable_memory_alignment ? "활성화" : "비활성화",
           manager->alignment_size);
    printf("  풀 사전 할당: %s\n",
           manager->enable_pool_preallocation ? "활성화" : "비활성화");
    printf("  통계 수집: %s\n",
           manager->enable_statistics ? "활성화" : "비활성화");

    printf("================================\n\n");
}