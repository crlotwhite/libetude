/**
 * @file allocator.c
 * @brief LibEtude 메모리 할당자 구현
 *
 * 런타임 시스템에서 사용하는 고성능 메모리 할당자를 구현합니다.
 * 메모리 풀을 기반으로 하며, 통계 수집 및 스레드 안전성을 제공합니다.
 */

#include "libetude/memory.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>

// 런타임 할당자 구조체 (설계 문서의 RTAllocator와 호환)
struct RTAllocator {
    void* base_address;         // 기본 메모리 주소
    size_t total_size;          // 총 크기
    size_t used_size;           // 사용된 크기
    size_t peak_usage;          // 최대 사용량

    // 메모리 블록 관리
    ETMemoryPool* memory_pool;  // 내부 메모리 풀
    size_t block_alignment;     // 블록 정렬

    // 통계
    size_t num_allocations;     // 할당 횟수
    size_t num_frees;           // 해제 횟수

    // 스레드 안전성
    pthread_mutex_t mutex;      // 뮤텍스
    bool thread_safe;           // 스레드 안전성 활성화 여부
};

// 내부 함수 선언
static void rt_lock_allocator(RTAllocator* allocator);
static void rt_unlock_allocator(RTAllocator* allocator);
static void rt_update_stats(RTAllocator* allocator, size_t size, bool is_alloc);

// 함수 전방 선언
void rt_free(RTAllocator* allocator, void* ptr);

// =============================================================================
// 런타임 할당자 생성 및 관리
// =============================================================================

RTAllocator* rt_create_allocator(size_t size, size_t alignment) {
    if (size == 0) {
        return NULL;
    }

    // 할당자 구조체 생성
    RTAllocator* allocator = (RTAllocator*)calloc(1, sizeof(RTAllocator));
    if (allocator == NULL) {
        return NULL;
    }

    // 메모리 풀 옵션 설정
    ETMemoryPoolOptions pool_options = {
        .pool_type = ET_POOL_DYNAMIC,
        .mem_type = ET_MEM_CPU,
        .alignment = alignment > 0 ? alignment : ET_DEFAULT_ALIGNMENT,
        .block_size = 0,
        .min_block_size = 64,  // 최소 64바이트 블록
        .thread_safe = false,  // 할당자 레벨에서 동기화 처리
        .device_context = NULL
    };

    // 내부 메모리 풀 생성
    allocator->memory_pool = et_create_memory_pool_with_options(size, &pool_options);
    if (allocator->memory_pool == NULL) {
        free(allocator);
        return NULL;
    }

    // 할당자 정보 설정
    allocator->base_address = allocator->memory_pool->base;
    allocator->total_size = size;
    allocator->used_size = 0;
    allocator->peak_usage = 0;
    allocator->block_alignment = pool_options.alignment;
    allocator->num_allocations = 0;
    allocator->num_frees = 0;
    allocator->thread_safe = true;

    // 스레드 안전성 초기화
    if (pthread_mutex_init(&allocator->mutex, NULL) != 0) {
        et_destroy_memory_pool(allocator->memory_pool);
        free(allocator);
        return NULL;
    }

    return allocator;
}

void* rt_alloc(RTAllocator* allocator, size_t size) {
    if (allocator == NULL || size == 0) {
        return NULL;
    }

    rt_lock_allocator(allocator);

    void* ptr = et_alloc_from_pool(allocator->memory_pool, size);

    if (ptr != NULL) {
        rt_update_stats(allocator, size, true);
    }

    rt_unlock_allocator(allocator);

    return ptr;
}

void* rt_alloc_aligned(RTAllocator* allocator, size_t size, size_t alignment) {
    if (allocator == NULL || size == 0 || alignment == 0) {
        return NULL;
    }

    // 정렬이 2의 거듭제곱인지 확인
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    rt_lock_allocator(allocator);

    void* ptr = et_alloc_aligned_from_pool(allocator->memory_pool, size, alignment);

    if (ptr != NULL) {
        rt_update_stats(allocator, size, true);
    }

    rt_unlock_allocator(allocator);

    return ptr;
}

void* rt_calloc(RTAllocator* allocator, size_t num, size_t size) {
    if (allocator == NULL || num == 0 || size == 0) {
        return NULL;
    }

    size_t total_size = num * size;

    // 오버플로우 검사
    if (total_size / num != size) {
        return NULL;
    }

    void* ptr = rt_alloc(allocator, total_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }

    return ptr;
}

void* rt_realloc(RTAllocator* allocator, void* ptr, size_t new_size) {
    if (allocator == NULL) {
        return NULL;
    }

    if (ptr == NULL) {
        return rt_alloc(allocator, new_size);
    }

    if (new_size == 0) {
        rt_free(allocator, ptr);
        return NULL;
    }

    // 기존 블록 크기 확인 (블록 헤더에서 가져오기)
    // 주의: 이는 동적 풀에서만 작동하며, 실제로는 더 안전한 방법이 필요
    size_t old_size = new_size; // 임시로 새 크기와 동일하게 설정

    // 새로운 메모리 할당
    void* new_ptr = rt_alloc(allocator, new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    // 기존 데이터 복사 (작은 크기만큼만 복사)
    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);

    // 기존 메모리 해제
    rt_free(allocator, ptr);

    return new_ptr;
}

void rt_free(RTAllocator* allocator, void* ptr) {
    if (allocator == NULL || ptr == NULL) {
        return;
    }

    rt_lock_allocator(allocator);

    et_free_to_pool(allocator->memory_pool, ptr);
    rt_update_stats(allocator, 0, false);  // 크기는 풀에서 관리

    rt_unlock_allocator(allocator);
}

void rt_reset_allocator(RTAllocator* allocator) {
    if (allocator == NULL) {
        return;
    }

    rt_lock_allocator(allocator);

    et_reset_pool(allocator->memory_pool);

    allocator->used_size = 0;
    // 통계는 유지 (리셋 횟수는 풀에서 관리)

    rt_unlock_allocator(allocator);
}

void rt_destroy_allocator(RTAllocator* allocator) {
    if (allocator == NULL) {
        return;
    }

    // 메모리 풀 해제
    et_destroy_memory_pool(allocator->memory_pool);

    // 뮤텍스 해제
    pthread_mutex_destroy(&allocator->mutex);

    // 할당자 구조체 해제
    free(allocator);
}

// =============================================================================
// 할당자 정보 및 통계
// =============================================================================

size_t rt_get_total_size(RTAllocator* allocator) {
    if (allocator == NULL) {
        return 0;
    }

    return allocator->total_size;
}

size_t rt_get_used_size(RTAllocator* allocator) {
    if (allocator == NULL) {
        return 0;
    }

    rt_lock_allocator(allocator);
    size_t used_size = allocator->used_size;
    rt_unlock_allocator(allocator);

    return used_size;
}

size_t rt_get_free_size(RTAllocator* allocator) {
    if (allocator == NULL) {
        return 0;
    }

    rt_lock_allocator(allocator);
    size_t free_size = allocator->total_size - allocator->used_size;
    rt_unlock_allocator(allocator);

    return free_size;
}

size_t rt_get_peak_usage(RTAllocator* allocator) {
    if (allocator == NULL) {
        return 0;
    }

    rt_lock_allocator(allocator);
    size_t peak_usage = allocator->peak_usage;
    rt_unlock_allocator(allocator);

    return peak_usage;
}

void rt_get_allocator_stats(RTAllocator* allocator, ETMemoryPoolStats* stats) {
    if (allocator == NULL || stats == NULL) {
        return;
    }

    rt_lock_allocator(allocator);

    // 메모리 풀 통계 가져오기
    et_get_pool_stats(allocator->memory_pool, stats);

    rt_unlock_allocator(allocator);
}

bool rt_validate_allocator(RTAllocator* allocator) {
    if (allocator == NULL) {
        return false;
    }

    rt_lock_allocator(allocator);

    bool valid = et_validate_memory_pool(allocator->memory_pool);

    if (valid) {
        // 추가 할당자 레벨 검증
        if (allocator->used_size > allocator->total_size) {
            valid = false;
        }

        if (allocator->peak_usage > allocator->total_size) {
            valid = false;
        }

        if (allocator->base_address != allocator->memory_pool->base) {
            valid = false;
        }
    }

    rt_unlock_allocator(allocator);

    return valid;
}

// =============================================================================
// 디버깅 및 프로파일링 지원
// =============================================================================

void rt_print_allocator_info(RTAllocator* allocator) {
    if (allocator == NULL) {
        printf("Allocator: NULL\n");
        return;
    }

    rt_lock_allocator(allocator);

    printf("=== Runtime Allocator Info ===\n");
    printf("Base Address: %p\n", allocator->base_address);
    printf("Total Size: %zu bytes (%.2f MB)\n",
           allocator->total_size,
           (double)allocator->total_size / (1024.0 * 1024.0));
    printf("Used Size: %zu bytes (%.2f MB)\n",
           allocator->used_size,
           (double)allocator->used_size / (1024.0 * 1024.0));
    printf("Free Size: %zu bytes (%.2f MB)\n",
           allocator->total_size - allocator->used_size,
           (double)(allocator->total_size - allocator->used_size) / (1024.0 * 1024.0));
    printf("Peak Usage: %zu bytes (%.2f MB)\n",
           allocator->peak_usage,
           (double)allocator->peak_usage / (1024.0 * 1024.0));
    printf("Usage Ratio: %.2f%%\n",
           (double)allocator->used_size / (double)allocator->total_size * 100.0);
    printf("Block Alignment: %zu bytes\n", allocator->block_alignment);
    printf("Allocations: %zu\n", allocator->num_allocations);
    printf("Frees: %zu\n", allocator->num_frees);
    printf("Thread Safe: %s\n", allocator->thread_safe ? "Yes" : "No");

    // 메모리 풀 통계
    ETMemoryPoolStats pool_stats;
    et_get_pool_stats(allocator->memory_pool, &pool_stats);
    printf("Pool Resets: %zu\n", pool_stats.num_resets);
    printf("Fragmentation: %.2f%%\n", pool_stats.fragmentation_ratio * 100.0);

    rt_unlock_allocator(allocator);
}

// =============================================================================
// 내부 함수 구현
// =============================================================================

static void rt_lock_allocator(RTAllocator* allocator) {
    if (allocator->thread_safe) {
        pthread_mutex_lock(&allocator->mutex);
    }
}

static void rt_unlock_allocator(RTAllocator* allocator) {
    if (allocator->thread_safe) {
        pthread_mutex_unlock(&allocator->mutex);
    }
}

static void rt_update_stats(RTAllocator* allocator, size_t size, bool is_alloc) {
    if (is_alloc) {
        allocator->num_allocations++;
        // 실제 사용된 크기는 풀에서 가져오기
        ETMemoryPoolStats stats;
        et_get_pool_stats(allocator->memory_pool, &stats);
        allocator->used_size = stats.used_size;
        if (allocator->used_size > allocator->peak_usage) {
            allocator->peak_usage = allocator->used_size;
        }
    } else {
        allocator->num_frees++;
        // 실제 해제된 크기는 메모리 풀에서 관리되므로 풀의 used_size를 동기화
        ETMemoryPoolStats stats;
        et_get_pool_stats(allocator->memory_pool, &stats);
        allocator->used_size = stats.used_size;
    }
}

// =============================================================================
// 메모리 누수 감지 및 추적 기능
// =============================================================================

void rt_enable_leak_detection(RTAllocator* allocator, bool enable) {
    if (allocator == NULL) {
        return;
    }

    rt_lock_allocator(allocator);
    et_enable_leak_detection(allocator->memory_pool, enable);
    rt_unlock_allocator(allocator);
}

size_t rt_check_memory_leaks(RTAllocator* allocator, uint64_t leak_threshold_ms) {
    if (allocator == NULL) {
        return 0;
    }

    rt_lock_allocator(allocator);
    size_t leak_count = et_check_memory_leaks(allocator->memory_pool, leak_threshold_ms);
    rt_unlock_allocator(allocator);

    return leak_count;
}

size_t rt_get_memory_leaks(RTAllocator* allocator, ETMemoryLeakInfo* leak_infos, size_t max_infos) {
    if (allocator == NULL) {
        return 0;
    }

    rt_lock_allocator(allocator);
    size_t leak_count = et_get_memory_leaks(allocator->memory_pool, leak_infos, max_infos);
    rt_unlock_allocator(allocator);

    return leak_count;
}

void rt_print_memory_leak_report(RTAllocator* allocator, const char* output_file) {
    if (allocator == NULL) {
        return;
    }

    rt_lock_allocator(allocator);
    et_print_memory_leak_report(allocator->memory_pool, output_file);
    rt_unlock_allocator(allocator);
}

size_t rt_check_memory_corruption(RTAllocator* allocator) {
    if (allocator == NULL) {
        return 0;
    }

    rt_lock_allocator(allocator);
    size_t corruption_count = et_check_memory_corruption(allocator->memory_pool);
    rt_unlock_allocator(allocator);

    return corruption_count;
}

// 디버그 모드 할당자 함수
#ifdef ET_DEBUG_MEMORY
void* rt_alloc_debug(RTAllocator* allocator, size_t size, const char* file, int line, const char* function) {
    if (allocator == NULL || size == 0) {
        return NULL;
    }

    rt_lock_allocator(allocator);

    void* ptr = et_alloc_from_pool_debug(allocator->memory_pool, size, file, line, function);

    if (ptr != NULL) {
        rt_update_stats(allocator, size, true);
    }

    rt_unlock_allocator(allocator);

    return ptr;
}

void rt_free_debug(RTAllocator* allocator, void* ptr, const char* file, int line, const char* function) {
    if (allocator == NULL || ptr == NULL) {
        return;
    }

    rt_lock_allocator(allocator);

    et_free_to_pool_debug(allocator->memory_pool, ptr, file, line, function);
    rt_update_stats(allocator, 0, false);

    rt_unlock_allocator(allocator);
}
#endif