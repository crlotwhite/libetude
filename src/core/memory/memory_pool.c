/**
 * @file memory_pool.c
 * @brief LibEtude 메모리 풀 구현
 *
 * 고정 크기 및 동적 크기 메모리 풀을 구현합니다.
 * 스레드 안전성을 보장하며, 메모리 단편화를 최소화합니다.
 */

#include "libetude/memory.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>

// 내부 상수
#define ET_MEMORY_BLOCK_HEADER_SIZE sizeof(ETMemoryBlock)
#define ET_MIN_BLOCK_SIZE 32
#define ET_BITMAP_BITS_PER_BYTE 8
#define ET_MEMORY_MAGIC 0xDEADBEEF
#define ET_INITIAL_ACTIVE_BLOCKS_CAPACITY 1024

// 내부 함수 선언
static void* et_alloc_fixed_block(ETMemoryPool* pool);
static void et_free_fixed_block(ETMemoryPool* pool, void* ptr);
static void* et_alloc_dynamic_block(ETMemoryPool* pool, size_t size);
static void et_free_dynamic_block(ETMemoryPool* pool, void* ptr);
static ETMemoryBlock* et_find_free_block(ETMemoryPool* pool, size_t size);
static ETMemoryBlock* et_split_block(ETMemoryBlock* block, size_t size);
static void et_merge_free_blocks(ETMemoryPool* pool);
static void et_init_fixed_pool(ETMemoryPool* pool, size_t block_size, size_t num_blocks);
static void et_init_dynamic_pool(ETMemoryPool* pool, size_t min_block_size);
static void et_lock_pool(ETMemoryPool* pool);
static void et_unlock_pool(ETMemoryPool* pool);
static uint64_t et_get_current_time_ms(void);
static void et_add_active_block(ETMemoryPool* pool, ETMemoryBlock* block);
static void et_remove_active_block(ETMemoryPool* pool, ETMemoryBlock* block);
static void et_init_leak_detection(ETMemoryPool* pool);
static void et_cleanup_leak_detection(ETMemoryPool* pool);

// =============================================================================
// 메모리 풀 생성 및 초기화
// =============================================================================

ETMemoryPool* et_create_memory_pool(size_t size, size_t alignment) {
    ETMemoryPoolOptions options = {
        .pool_type = ET_POOL_DYNAMIC,
        .mem_type = ET_MEM_CPU,
        .alignment = alignment > 0 ? alignment : ET_DEFAULT_ALIGNMENT,
        .block_size = 0,
        .min_block_size = ET_MIN_BLOCK_SIZE,
        .thread_safe = true,
        .device_context = NULL
    };

    return et_create_memory_pool_with_options(size, &options);
}

ETMemoryPool* et_create_memory_pool_with_options(size_t size, const ETMemoryPoolOptions* options) {
    if (size == 0 || options == NULL) {
        return NULL;
    }

    // 메모리 풀 구조체 할당
    ETMemoryPool* pool = (ETMemoryPool*)calloc(1, sizeof(ETMemoryPool));
    if (pool == NULL) {
        return NULL;
    }

    // 정렬된 크기 계산
    size_t aligned_size = et_align_size(size, options->alignment);

    // 메모리 할당
    pool->base = aligned_alloc(options->alignment, aligned_size);
    if (pool->base == NULL) {
        free(pool);
        return NULL;
    }

    // 기본 정보 설정
    pool->total_size = aligned_size;
    pool->used_size = 0;
    pool->peak_usage = 0;
    pool->alignment = options->alignment;
    pool->pool_type = options->pool_type;
    pool->mem_type = options->mem_type;
    pool->external = false;
    pool->device_context = options->device_context;
    pool->thread_safe = options->thread_safe;

    // 통계 초기화
    pool->num_allocations = 0;
    pool->num_frees = 0;
    pool->num_resets = 0;

    // 메모리 누수 감지 초기화
    pool->leak_detection_enabled = options->enable_leak_detection;
    if (pool->leak_detection_enabled) {
        et_init_leak_detection(pool);
    }

    // 스레드 안전성 초기화
    if (pool->thread_safe) {
        if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
            if (pool->leak_detection_enabled) {
                et_cleanup_leak_detection(pool);
            }
            free(pool->base);
            free(pool);
            return NULL;
        }
    }

    // 풀 타입에 따른 초기화
    if (pool->pool_type == ET_POOL_FIXED) {
        if (options->block_size == 0) {
            et_destroy_memory_pool(pool);
            return NULL;
        }
        size_t num_blocks = aligned_size / options->block_size;
        et_init_fixed_pool(pool, options->block_size, num_blocks);
    } else {
        et_init_dynamic_pool(pool, options->min_block_size);
    }

    return pool;
}

ETMemoryPool* et_create_memory_pool_from_buffer(void* base, size_t size, const ETMemoryPoolOptions* options) {
    if (base == NULL || size == 0 || options == NULL) {
        return NULL;
    }

    // 정렬 확인
    if (!et_is_aligned(base, options->alignment)) {
        return NULL;
    }

    // 메모리 풀 구조체 할당
    ETMemoryPool* pool = (ETMemoryPool*)calloc(1, sizeof(ETMemoryPool));
    if (pool == NULL) {
        return NULL;
    }

    // 기본 정보 설정
    pool->base = base;
    pool->total_size = size;
    pool->used_size = 0;
    pool->peak_usage = 0;
    pool->alignment = options->alignment;
    pool->pool_type = options->pool_type;
    pool->mem_type = options->mem_type;
    pool->external = true;  // 외부 메모리 사용
    pool->device_context = options->device_context;
    pool->thread_safe = options->thread_safe;

    // 통계 초기화
    pool->num_allocations = 0;
    pool->num_frees = 0;
    pool->num_resets = 0;

    // 메모리 누수 감지 초기화
    pool->leak_detection_enabled = options->enable_leak_detection;
    if (pool->leak_detection_enabled) {
        et_init_leak_detection(pool);
    }

    // 스레드 안전성 초기화
    if (pool->thread_safe) {
        if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
            if (pool->leak_detection_enabled) {
                et_cleanup_leak_detection(pool);
            }
            free(pool);
            return NULL;
        }
    }

    // 풀 타입에 따른 초기화
    if (pool->pool_type == ET_POOL_FIXED) {
        if (options->block_size == 0) {
            et_destroy_memory_pool(pool);
            return NULL;
        }
        size_t num_blocks = size / options->block_size;
        et_init_fixed_pool(pool, options->block_size, num_blocks);
    } else {
        et_init_dynamic_pool(pool, options->min_block_size);
    }

    return pool;
}

// =============================================================================
// 메모리 할당 및 해제
// =============================================================================

void* et_alloc_from_pool(ETMemoryPool* pool, size_t size) {
    if (pool == NULL || size == 0) {
        return NULL;
    }

    et_lock_pool(pool);

    void* ptr = NULL;

    if (pool->pool_type == ET_POOL_FIXED) {
        // 고정 크기 풀에서는 요청 크기가 블록 크기보다 작거나 같아야 함
        if (size <= pool->block_size) {
            ptr = et_alloc_fixed_block(pool);
        }
    } else {
        ptr = et_alloc_dynamic_block(pool, size);
    }

    if (ptr != NULL) {
        pool->num_allocations++;
        if (pool->used_size > pool->peak_usage) {
            pool->peak_usage = pool->used_size;
        }
    }

    et_unlock_pool(pool);

    return ptr;
}

void* et_alloc_aligned_from_pool(ETMemoryPool* pool, size_t size, size_t alignment) {
    if (pool == NULL || size == 0 || alignment == 0) {
        return NULL;
    }

    // 정렬이 2의 거듭제곱인지 확인
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    et_lock_pool(pool);

    void* ptr = NULL;

    if (pool->pool_type == ET_POOL_FIXED) {
        // 고정 크기 풀에서는 요청 크기가 블록 크기보다 작거나 같아야 함
        if (size <= pool->block_size) {
            ptr = et_alloc_fixed_block(pool);

            // 정렬 확인 및 조정
            if (ptr != NULL && !et_is_aligned(ptr, alignment)) {
                // 고정 풀에서는 정렬 조정이 어려우므로 실패
                et_free_fixed_block(pool, ptr);
                ptr = NULL;
            }
        }
    } else {
        // 동적 풀에서 정렬된 할당
        size_t aligned_size = et_align_size(size, alignment);
        size_t total_size = aligned_size + ET_MEMORY_BLOCK_HEADER_SIZE;

        // 추가 정렬 공간이 필요한 경우
        if (alignment > pool->alignment) {
            total_size += alignment - 1;
        }

        ETMemoryBlock* block = et_find_free_block(pool, total_size);
        if (block != NULL) {
            // 블록이 충분히 크면 분할
            if (block->size >= total_size + pool->min_block_size + ET_MEMORY_BLOCK_HEADER_SIZE) {
                block = et_split_block(block, total_size);
            }

            // 블록을 사용 중으로 표시
            block->is_free = false;

            // 누수 감지 정보 설정
            if (pool->leak_detection_enabled) {
                block->file = NULL;
                block->line = 0;
                block->function = NULL;
                block->timestamp = et_get_current_time_ms();
                block->magic = ET_MEMORY_MAGIC;
                et_add_active_block(pool, block);
            }

            // 자유 리스트에서 제거
            if (block->prev) {
                block->prev->next = block->next;
            } else {
                pool->free_list = block->next;
            }

            if (block->next) {
                block->next->prev = block->prev;
            }

            // 사용 중인 리스트에 추가
            block->next = pool->used_list;
            block->prev = NULL;
            if (pool->used_list) {
                pool->used_list->prev = block;
            }
            pool->used_list = block;

            pool->used_size += block->size;

            // 정렬된 주소 계산
            uintptr_t raw_addr = (uintptr_t)block + ET_MEMORY_BLOCK_HEADER_SIZE;
            uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
            ptr = (void*)aligned_addr;
        }
    }

    if (ptr != NULL) {
        pool->num_allocations++;
        if (pool->used_size > pool->peak_usage) {
            pool->peak_usage = pool->used_size;
        }
    }

    et_unlock_pool(pool);

    return ptr;
}

void et_free_to_pool(ETMemoryPool* pool, void* ptr) {
    if (pool == NULL || ptr == NULL) {
        return;
    }

    et_lock_pool(pool);

    if (pool->pool_type == ET_POOL_FIXED) {
        et_free_fixed_block(pool, ptr);
    } else {
        et_free_dynamic_block(pool, ptr);
    }

    pool->num_frees++;

    et_unlock_pool(pool);
}

// =============================================================================
// 메모리 풀 관리
// =============================================================================

void et_reset_pool(ETMemoryPool* pool) {
    if (pool == NULL) {
        return;
    }

    et_lock_pool(pool);

    pool->used_size = 0;
    pool->num_resets++;

    if (pool->pool_type == ET_POOL_FIXED) {
        // 고정 풀 리셋
        pool->free_blocks = pool->num_blocks;
        memset(pool->block_bitmap, 0, (pool->num_blocks + 7) / 8);
    } else {
        // 동적 풀 리셋
        pool->free_list = NULL;
        pool->used_list = NULL;

        // 전체 메모리를 하나의 자유 블록으로 초기화
        ETMemoryBlock* initial_block = (ETMemoryBlock*)pool->base;
        initial_block->size = pool->total_size - ET_MEMORY_BLOCK_HEADER_SIZE;
        initial_block->is_free = true;
        initial_block->next = NULL;
        initial_block->prev = NULL;

        pool->free_list = initial_block;
    }

    et_unlock_pool(pool);
}

void et_get_pool_stats(ETMemoryPool* pool, ETMemoryPoolStats* stats) {
    if (pool == NULL || stats == NULL) {
        return;
    }

    et_lock_pool(pool);

    stats->total_size = pool->total_size;
    stats->used_size = pool->used_size;
    stats->peak_usage = pool->peak_usage;
    stats->free_size = pool->total_size - pool->used_size;
    stats->num_allocations = pool->num_allocations;
    stats->num_frees = pool->num_frees;
    stats->num_resets = pool->num_resets;

    // 단편화 비율 계산
    if (pool->total_size > 0) {
        stats->fragmentation_ratio = (float)pool->used_size / (float)pool->total_size;
    } else {
        stats->fragmentation_ratio = 0.0f;
    }

    // 메모리 누수 감지 통계
    if (pool->leak_detection_enabled) {
        stats->num_active_blocks = pool->active_block_count;

        // 누수 의심 블록 계산 (5초 이상 할당된 블록)
        uint64_t current_time = et_get_current_time_ms();
        size_t leaked_blocks = 0;
        size_t leaked_bytes = 0;

        for (size_t i = 0; i < pool->active_block_count; i++) {
            ETMemoryBlock* block = pool->active_blocks[i];
            if (block != NULL && !block->is_free) {
                uint64_t age = current_time - block->timestamp;
                if (age > 5000) { // 5초 임계값
                    leaked_blocks++;
                    leaked_bytes += block->size;
                }
            }
        }

        stats->num_leaked_blocks = leaked_blocks;
        stats->leaked_bytes = leaked_bytes;
    } else {
        stats->num_active_blocks = 0;
        stats->num_leaked_blocks = 0;
        stats->leaked_bytes = 0;
    }

    et_unlock_pool(pool);
}

void et_destroy_memory_pool(ETMemoryPool* pool) {
    if (pool == NULL) {
        return;
    }

    // 스레드 안전성 해제
    if (pool->thread_safe) {
        pthread_mutex_destroy(&pool->mutex);
    }

    // 고정 풀 관련 메모리 해제
    if (pool->pool_type == ET_POOL_FIXED) {
        free(pool->fixed_blocks);
        free(pool->block_bitmap);
    }

    // 기본 메모리 해제 (외부 메모리가 아닌 경우만)
    if (!pool->external && pool->base != NULL) {
        free(pool->base);
    }

    // 풀 구조체 해제
    free(pool);
}

// =============================================================================
// 내부 함수 구현
// =============================================================================

static void et_init_fixed_pool(ETMemoryPool* pool, size_t block_size, size_t num_blocks) {
    pool->block_size = et_align_size(block_size, pool->alignment);
    pool->num_blocks = num_blocks;
    pool->free_blocks = num_blocks;
    pool->min_block_size = pool->block_size;

    // 블록 포인터 배열 할당
    pool->fixed_blocks = (void**)calloc(num_blocks, sizeof(void*));

    // 비트맵 할당 (각 블록의 사용 상태 추적)
    size_t bitmap_size = (num_blocks + ET_BITMAP_BITS_PER_BYTE - 1) / ET_BITMAP_BITS_PER_BYTE;
    pool->block_bitmap = (uint8_t*)calloc(bitmap_size, sizeof(uint8_t));

    // 블록 포인터 초기화
    uint8_t* base_ptr = (uint8_t*)pool->base;
    for (size_t i = 0; i < num_blocks; i++) {
        pool->fixed_blocks[i] = base_ptr + (i * pool->block_size);
    }
}

static void et_init_dynamic_pool(ETMemoryPool* pool, size_t min_block_size) {
    pool->min_block_size = et_align_size(min_block_size, pool->alignment);
    pool->free_list = NULL;
    pool->used_list = NULL;

    // 전체 메모리를 하나의 자유 블록으로 초기화
    ETMemoryBlock* initial_block = (ETMemoryBlock*)pool->base;
    initial_block->size = pool->total_size - ET_MEMORY_BLOCK_HEADER_SIZE;
    initial_block->is_free = true;
    initial_block->next = NULL;
    initial_block->prev = NULL;

    pool->free_list = initial_block;
}

static void* et_alloc_fixed_block(ETMemoryPool* pool) {
    if (pool->free_blocks == 0) {
        return NULL;
    }

    // 첫 번째 사용 가능한 블록 찾기
    for (size_t i = 0; i < pool->num_blocks; i++) {
        size_t byte_index = i / ET_BITMAP_BITS_PER_BYTE;
        size_t bit_index = i % ET_BITMAP_BITS_PER_BYTE;

        if ((pool->block_bitmap[byte_index] & (1 << bit_index)) == 0) {
            // 블록 사용으로 표시
            pool->block_bitmap[byte_index] |= (1 << bit_index);
            pool->free_blocks--;
            pool->used_size += pool->block_size;

            return pool->fixed_blocks[i];
        }
    }

    return NULL;
}

static void et_free_fixed_block(ETMemoryPool* pool, void* ptr) {
    // 블록 인덱스 찾기
    uint8_t* base_ptr = (uint8_t*)pool->base;
    uint8_t* block_ptr = (uint8_t*)ptr;

    if (block_ptr < base_ptr || block_ptr >= base_ptr + pool->total_size) {
        return; // 잘못된 포인터
    }

    size_t offset = block_ptr - base_ptr;
    if (offset % pool->block_size != 0) {
        return; // 정렬되지 않은 포인터
    }

    size_t block_index = offset / pool->block_size;
    if (block_index >= pool->num_blocks) {
        return; // 범위를 벗어난 인덱스
    }

    // 블록 해제
    size_t byte_index = block_index / ET_BITMAP_BITS_PER_BYTE;
    size_t bit_index = block_index % ET_BITMAP_BITS_PER_BYTE;

    if (pool->block_bitmap[byte_index] & (1 << bit_index)) {
        pool->block_bitmap[byte_index] &= ~(1 << bit_index);
        pool->free_blocks++;
        pool->used_size -= pool->block_size;
    }
}

static void* et_alloc_dynamic_block(ETMemoryPool* pool, size_t size) {
    size_t aligned_size = et_align_size(size, pool->alignment);
    size_t total_size = aligned_size + ET_MEMORY_BLOCK_HEADER_SIZE;

    ETMemoryBlock* block = et_find_free_block(pool, total_size);
    if (block == NULL) {
        return NULL;
    }

    // 블록이 충분히 크면 분할
    if (block->size >= total_size + pool->min_block_size + ET_MEMORY_BLOCK_HEADER_SIZE) {
        block = et_split_block(block, total_size);
    }

    // 블록을 사용 중으로 표시
    block->is_free = false;

    // 누수 감지 정보 설정
    if (pool->leak_detection_enabled) {
        block->file = NULL;  // 기본 할당에서는 파일 정보 없음
        block->line = 0;
        block->function = NULL;
        block->timestamp = et_get_current_time_ms();
        block->magic = ET_MEMORY_MAGIC;
        et_add_active_block(pool, block);
    }

    // 자유 리스트에서 제거
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        pool->free_list = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }

    // 사용 중인 리스트에 추가
    block->next = pool->used_list;
    block->prev = NULL;
    if (pool->used_list) {
        pool->used_list->prev = block;
    }
    pool->used_list = block;

    pool->used_size += block->size;

    return (uint8_t*)block + ET_MEMORY_BLOCK_HEADER_SIZE;
}

static void et_free_dynamic_block(ETMemoryPool* pool, void* ptr) {
    if (ptr == NULL) {
        return;
    }

    ETMemoryBlock* block = (ETMemoryBlock*)((uint8_t*)ptr - ET_MEMORY_BLOCK_HEADER_SIZE);

    // 블록 유효성 검사
    if (block->is_free) {
        return; // 이미 해제된 블록
    }

    // 사용 중인 리스트에서 제거
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        pool->used_list = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }

    // 누수 감지에서 제거
    if (pool->leak_detection_enabled) {
        et_remove_active_block(pool, block);
    }

    // 블록을 자유로 표시
    block->is_free = true;
    pool->used_size -= block->size;

    // 자유 리스트에 추가
    block->next = pool->free_list;
    block->prev = NULL;
    if (pool->free_list) {
        pool->free_list->prev = block;
    }
    pool->free_list = block;

    // 인접한 자유 블록들과 병합
    et_merge_free_blocks(pool);
}

static ETMemoryBlock* et_find_free_block(ETMemoryPool* pool, size_t size) {
    ETMemoryBlock* current = pool->free_list;
    ETMemoryBlock* best_fit = NULL;

    // First-fit 전략 사용 (성능 우선)
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

static ETMemoryBlock* et_split_block(ETMemoryBlock* block, size_t size) {
    if (block->size <= size + ET_MEMORY_BLOCK_HEADER_SIZE) {
        return block; // 분할할 수 없음
    }

    // 새로운 블록 생성
    ETMemoryBlock* new_block = (ETMemoryBlock*)((uint8_t*)block + size);
    new_block->size = block->size - size;
    new_block->is_free = true;
    new_block->next = block->next;
    new_block->prev = block;

    // 기존 블록 업데이트
    block->size = size - ET_MEMORY_BLOCK_HEADER_SIZE;
    block->next = new_block;

    if (new_block->next) {
        new_block->next->prev = new_block;
    }

    return block;
}

static void et_merge_free_blocks(ETMemoryPool* pool) {
    ETMemoryBlock* current = pool->free_list;

    while (current != NULL) {
        if (!current->is_free) {
            current = current->next;
            continue;
        }

        // 다음 블록과 병합 시도
        ETMemoryBlock* next_block = (ETMemoryBlock*)((uint8_t*)current + current->size + ET_MEMORY_BLOCK_HEADER_SIZE);

        // 메모리 범위 내에 있고 자유 블록인지 확인
        if ((uint8_t*)next_block < (uint8_t*)pool->base + pool->total_size) {
            ETMemoryBlock* list_block = pool->free_list;
            while (list_block != NULL) {
                if (list_block == next_block && list_block->is_free) {
                    // 병합 수행
                    current->size += next_block->size + ET_MEMORY_BLOCK_HEADER_SIZE;

                    // 리스트에서 next_block 제거
                    if (next_block->prev) {
                        next_block->prev->next = next_block->next;
                    } else {
                        pool->free_list = next_block->next;
                    }

                    if (next_block->next) {
                        next_block->next->prev = next_block->prev;
                    }

                    break;
                }
                list_block = list_block->next;
            }
        }

        current = current->next;
    }
}

static void et_lock_pool(ETMemoryPool* pool) {
    if (pool->thread_safe) {
        pthread_mutex_lock(&pool->mutex);
    }
}

static void et_unlock_pool(ETMemoryPool* pool) {
    if (pool->thread_safe) {
        pthread_mutex_unlock(&pool->mutex);
    }
}

// =============================================================================
// 유틸리티 함수
// =============================================================================

size_t et_align_size(size_t size, size_t alignment) {
    if (alignment == 0) {
        return size;
    }
    return (size + alignment - 1) & ~(alignment - 1);
}

bool et_is_aligned(const void* ptr, size_t alignment) {
    if (alignment == 0) {
        return true;
    }
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

bool et_validate_memory_pool(ETMemoryPool* pool) {
    if (pool == NULL) {
        return false;
    }

    if (pool->base == NULL || pool->total_size == 0) {
        return false;
    }

    if (pool->used_size > pool->total_size) {
        return false;
    }

    if (pool->alignment == 0 || (pool->alignment & (pool->alignment - 1)) != 0) {
        return false; // 정렬이 2의 거듭제곱이 아님
    }

    if (!et_is_aligned(pool->base, pool->alignment)) {
        return false;
    }

    return true;
}

// =============================================================================
// 메모리 누수 감지 함수 구현
// =============================================================================

static uint64_t et_get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static void et_init_leak_detection(ETMemoryPool* pool) {
    pool->active_blocks = (ETMemoryBlock**)calloc(ET_INITIAL_ACTIVE_BLOCKS_CAPACITY, sizeof(ETMemoryBlock*));
    pool->active_block_count = 0;
    pool->active_block_capacity = ET_INITIAL_ACTIVE_BLOCKS_CAPACITY;
}

static void et_cleanup_leak_detection(ETMemoryPool* pool) {
    if (pool->active_blocks) {
        free(pool->active_blocks);
        pool->active_blocks = NULL;
    }
    pool->active_block_count = 0;
    pool->active_block_capacity = 0;
}

static void et_add_active_block(ETMemoryPool* pool, ETMemoryBlock* block) {
    if (!pool->leak_detection_enabled || pool->active_blocks == NULL) {
        return;
    }

    // 용량 확장이 필요한 경우
    if (pool->active_block_count >= pool->active_block_capacity) {
        size_t new_capacity = pool->active_block_capacity * 2;
        ETMemoryBlock** new_blocks = (ETMemoryBlock**)realloc(
            pool->active_blocks,
            new_capacity * sizeof(ETMemoryBlock*)
        );

        if (new_blocks == NULL) {
            return; // 메모리 부족, 누수 감지 실패
        }

        pool->active_blocks = new_blocks;
        pool->active_block_capacity = new_capacity;
    }

    pool->active_blocks[pool->active_block_count++] = block;
}

static void et_remove_active_block(ETMemoryPool* pool, ETMemoryBlock* block) {
    if (!pool->leak_detection_enabled || pool->active_blocks == NULL) {
        return;
    }

    // 블록 찾기 및 제거
    for (size_t i = 0; i < pool->active_block_count; i++) {
        if (pool->active_blocks[i] == block) {
            // 마지막 요소로 이동하여 제거
            pool->active_blocks[i] = pool->active_blocks[pool->active_block_count - 1];
            pool->active_block_count--;
            break;
        }
    }
}

void et_enable_leak_detection(ETMemoryPool* pool, bool enable) {
    if (pool == NULL) {
        return;
    }

    et_lock_pool(pool);

    if (enable && !pool->leak_detection_enabled) {
        et_init_leak_detection(pool);
        pool->leak_detection_enabled = true;
    } else if (!enable && pool->leak_detection_enabled) {
        et_cleanup_leak_detection(pool);
        pool->leak_detection_enabled = false;
    }

    et_unlock_pool(pool);
}

size_t et_check_memory_leaks(ETMemoryPool* pool, uint64_t leak_threshold_ms) {
    if (pool == NULL || !pool->leak_detection_enabled) {
        return 0;
    }

    et_lock_pool(pool);

    uint64_t current_time = et_get_current_time_ms();
    size_t leak_count = 0;

    for (size_t i = 0; i < pool->active_block_count; i++) {
        ETMemoryBlock* block = pool->active_blocks[i];
        if (block != NULL && !block->is_free) {
            uint64_t age = current_time - block->timestamp;
            if (age > leak_threshold_ms) {
                leak_count++;
            }
        }
    }

    et_unlock_pool(pool);

    return leak_count;
}

size_t et_get_memory_leaks(ETMemoryPool* pool, ETMemoryLeakInfo* leak_infos, size_t max_infos) {
    if (pool == NULL || !pool->leak_detection_enabled || leak_infos == NULL || max_infos == 0) {
        return 0;
    }

    et_lock_pool(pool);

    size_t leak_count = 0;
    uint64_t current_time = et_get_current_time_ms();

    for (size_t i = 0; i < pool->active_block_count && leak_count < max_infos; i++) {
        ETMemoryBlock* block = pool->active_blocks[i];
        if (block != NULL && !block->is_free) {
            leak_infos[leak_count].ptr = (uint8_t*)block + ET_MEMORY_BLOCK_HEADER_SIZE;
            leak_infos[leak_count].size = block->size;
            leak_infos[leak_count].file = block->file;
            leak_infos[leak_count].line = block->line;
            leak_infos[leak_count].function = block->function;
            leak_infos[leak_count].timestamp = block->timestamp;
            leak_count++;
        }
    }

    et_unlock_pool(pool);

    return leak_count;
}

void et_print_memory_leak_report(ETMemoryPool* pool, const char* output_file) {
    if (pool == NULL || !pool->leak_detection_enabled) {
        return;
    }

    FILE* fp = output_file ? fopen(output_file, "w") : stdout;
    if (fp == NULL) {
        return;
    }

    et_lock_pool(pool);

    fprintf(fp, "=== Memory Leak Report ===\n");
    fprintf(fp, "Pool Address: %p\n", pool);
    fprintf(fp, "Total Size: %zu bytes\n", pool->total_size);
    fprintf(fp, "Used Size: %zu bytes\n", pool->used_size);
    fprintf(fp, "Active Blocks: %zu\n", pool->active_block_count);

    uint64_t current_time = et_get_current_time_ms();
    size_t leak_count = 0;
    size_t total_leaked_bytes = 0;

    for (size_t i = 0; i < pool->active_block_count; i++) {
        ETMemoryBlock* block = pool->active_blocks[i];
        if (block != NULL && !block->is_free) {
            uint64_t age = current_time - block->timestamp;

            fprintf(fp, "\nBlock #%zu:\n", leak_count + 1);
            fprintf(fp, "  Address: %p\n", (uint8_t*)block + ET_MEMORY_BLOCK_HEADER_SIZE);
            fprintf(fp, "  Size: %zu bytes\n", block->size);
            fprintf(fp, "  Age: %llu ms\n", (unsigned long long)age);

            if (block->file) {
                fprintf(fp, "  Allocated at: %s:%d in %s()\n",
                       block->file, block->line,
                       block->function ? block->function : "unknown");
            } else {
                fprintf(fp, "  Allocated at: unknown location\n");
            }

            leak_count++;
            total_leaked_bytes += block->size;
        }
    }

    fprintf(fp, "\n=== Summary ===\n");
    fprintf(fp, "Total Leaked Blocks: %zu\n", leak_count);
    fprintf(fp, "Total Leaked Bytes: %zu (%.2f KB)\n",
           total_leaked_bytes, (double)total_leaked_bytes / 1024.0);

    if (leak_count > 0) {
        fprintf(fp, "Average Block Size: %.2f bytes\n",
               (double)total_leaked_bytes / (double)leak_count);
    }

    et_unlock_pool(pool);

    if (output_file && fp != stdout) {
        fclose(fp);
    }
}

size_t et_check_memory_corruption(ETMemoryPool* pool) {
    if (pool == NULL || !pool->leak_detection_enabled) {
        return 0;
    }

    et_lock_pool(pool);

    size_t corruption_count = 0;

    for (size_t i = 0; i < pool->active_block_count; i++) {
        ETMemoryBlock* block = pool->active_blocks[i];
        if (block != NULL && !block->is_free) {
            // 매직 넘버 검사
            if (block->magic != ET_MEMORY_MAGIC) {
                corruption_count++;
            }
        }
    }

    et_unlock_pool(pool);

    return corruption_count;
}

// 디버그 모드 함수 구현
#ifdef ET_DEBUG_MEMORY
void* et_alloc_from_pool_debug(ETMemoryPool* pool, size_t size, const char* file, int line, const char* function) {
    if (pool == NULL || size == 0) {
        return NULL;
    }

    et_lock_pool(pool);

    void* ptr = NULL;

    if (pool->pool_type == ET_POOL_FIXED) {
        if (size <= pool->block_size) {
            ptr = et_alloc_fixed_block(pool);
        }
    } else {
        size_t aligned_size = et_align_size(size, pool->alignment);
        size_t total_size = aligned_size + ET_MEMORY_BLOCK_HEADER_SIZE;

        ETMemoryBlock* block = et_find_free_block(pool, total_size);
        if (block != NULL) {
            if (block->size >= total_size + pool->min_block_size + ET_MEMORY_BLOCK_HEADER_SIZE) {
                block = et_split_block(block, total_size);
            }

            block->is_free = false;

            // 디버그 정보 설정
            if (pool->leak_detection_enabled) {
                block->file = file;
                block->line = line;
                block->function = function;
                block->timestamp = et_get_current_time_ms();
                block->magic = ET_MEMORY_MAGIC;
                et_add_active_block(pool, block);
            }

            // 리스트 관리
            if (block->prev) {
                block->prev->next = block->next;
            } else {
                pool->free_list = block->next;
            }

            if (block->next) {
                block->next->prev = block->prev;
            }

            block->next = pool->used_list;
            block->prev = NULL;
            if (pool->used_list) {
                pool->used_list->prev = block;
            }
            pool->used_list = block;

            pool->used_size += block->size;
            ptr = (uint8_t*)block + ET_MEMORY_BLOCK_HEADER_SIZE;
        }
    }

    if (ptr != NULL) {
        pool->num_allocations++;
        if (pool->used_size > pool->peak_usage) {
            pool->peak_usage = pool->used_size;
        }
    }

    et_unlock_pool(pool);

    return ptr;
}

void et_free_to_pool_debug(ETMemoryPool* pool, void* ptr, const char* file, int line, const char* function) {
    if (pool == NULL || ptr == NULL) {
        return;
    }

    et_lock_pool(pool);

    if (pool->pool_type == ET_POOL_FIXED) {
        et_free_fixed_block(pool, ptr);
    } else {
        ETMemoryBlock* block = (ETMemoryBlock*)((uint8_t*)ptr - ET_MEMORY_BLOCK_HEADER_SIZE);

        // 디버그 정보로 이중 해제 검사
        if (block->is_free) {
            fprintf(stderr, "Double free detected at %s:%d in %s()\n", file, line, function);
            fprintf(stderr, "Original allocation: %s:%d in %s()\n",
                   block->file ? block->file : "unknown",
                   block->line,
                   block->function ? block->function : "unknown");
            et_unlock_pool(pool);
            return;
        }

        et_free_dynamic_block(pool, ptr);
    }

    pool->num_frees++;

    et_unlock_pool(pool);
}
#endif