/**
 * @file memory_optimization.c
 * @brief LibEtude 메모리 최적화 전략 구현
 *
 * 인플레이스 연산, 메모리 재사용, 단편화 방지 등의 메모리 최적화 기능을 구현합니다.
 */

#include "libetude/memory_optimization.h"
#include "libetude/memory.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>

// 내부 상수
#define ET_REUSE_POOL_DEFAULT_BUCKETS 16
#define ET_HISTOGRAM_BUCKETS 32
#define ET_CLEANUP_INTERVAL_MS 30000  // 30초
#define ET_MAX_IDLE_TIME_MS 60000     // 60초

// 내부 함수 선언
static void et_lock_inplace_context(ETInPlaceContext* ctx);
static void et_unlock_inplace_context(ETInPlaceContext* ctx);
static void et_lock_reuse_pool(ETMemoryReusePool* pool);
static void et_unlock_reuse_pool(ETMemoryReusePool* pool);
static void et_lock_smart_manager(ETSmartMemoryManager* manager);
static void et_unlock_smart_manager(ETSmartMemoryManager* manager);
static uint64_t et_get_current_time_ms(void);
static size_t et_get_size_class(size_t size);
static ETMemoryReuseBucket* et_find_or_create_bucket(ETMemoryReusePool* pool, size_t size_class);
static void et_cleanup_bucket(ETMemoryReuseBucket* bucket, uint64_t current_time, size_t max_idle_time);
static ETMemoryBlock* et_find_best_fit_block(ETMemoryPool* pool, size_t size);
static ETMemoryBlock* et_find_worst_fit_block(ETMemoryPool* pool, size_t size);

// =============================================================================
// 인플레이스 연산 지원 구현
// =============================================================================

ETInPlaceContext* et_create_inplace_context(size_t buffer_size, size_t alignment, bool thread_safe) {
    if (buffer_size == 0) {
        return NULL;
    }

    ETInPlaceContext* ctx = (ETInPlaceContext*)calloc(1, sizeof(ETInPlaceContext));
    if (ctx == NULL) {
        return NULL;
    }

    // 정렬된 버퍼 할당
    size_t aligned_size = et_align_size(buffer_size, alignment);
    ctx->buffer = aligned_alloc(alignment, aligned_size);
    if (ctx->buffer == NULL) {
        free(ctx);
        return NULL;
    }

    ctx->buffer_size = aligned_size;
    ctx->alignment = alignment;
    ctx->is_external = false;
    ctx->operation_count = 0;
    ctx->bytes_saved = 0;
    ctx->thread_safe = thread_safe;

    // 스레드 안전성 초기화
    if (thread_safe) {
        if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
            free(ctx->buffer);
            free(ctx);
            return NULL;
        }
    }

    return ctx;
}

ETInPlaceContext* et_create_inplace_context_from_buffer(void* buffer, size_t buffer_size,
                                                       size_t alignment, bool thread_safe) {
    if (buffer == NULL || buffer_size == 0) {
        return NULL;
    }

    // 정렬 확인
    if (!et_is_aligned(buffer, alignment)) {
        return NULL;
    }

    ETInPlaceContext* ctx = (ETInPlaceContext*)calloc(1, sizeof(ETInPlaceContext));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->buffer = buffer;
    ctx->buffer_size = buffer_size;
    ctx->alignment = alignment;
    ctx->is_external = true;
    ctx->operation_count = 0;
    ctx->bytes_saved = 0;
    ctx->thread_safe = thread_safe;

    // 스레드 안전성 초기화
    if (thread_safe) {
        if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
            free(ctx);
            return NULL;
        }
    }

    return ctx;
}

int et_inplace_memcpy(ETInPlaceContext* ctx, void* dest, const void* src, size_t size) {
    if (ctx == NULL || dest == NULL || src == NULL || size == 0) {
        return -1;
    }

    et_lock_inplace_context(ctx);

    // 겹치는 영역 확인
    uintptr_t dest_addr = (uintptr_t)dest;
    uintptr_t src_addr = (uintptr_t)src;

    if (dest_addr == src_addr) {
        // 동일한 주소면 복사 불필요
        ctx->bytes_saved += size;
        ctx->operation_count++;
        et_unlock_inplace_context(ctx);
        return 0;
    }

    // 겹치는 영역이 있으면 임시 버퍼 사용
    if ((dest_addr < src_addr + size && dest_addr + size > src_addr)) {
        if (size > ctx->buffer_size) {
            et_unlock_inplace_context(ctx);
            return -1; // 버퍼 크기 부족
        }

        // 임시 버퍼를 통한 안전한 복사
        memcpy(ctx->buffer, src, size);
        memcpy(dest, ctx->buffer, size);

        // 임시 버퍼 사용으로 메모리 절약 효과는 제한적
        ctx->operation_count++;
    } else {
        // 겹치지 않으면 직접 복사
        memcpy(dest, src, size);
        ctx->bytes_saved += size; // 추가 할당 없이 처리
        ctx->operation_count++;
    }

    et_unlock_inplace_context(ctx);
    return 0;
}

int et_inplace_memmove(ETInPlaceContext* ctx, void* dest, const void* src, size_t size) {
    if (ctx == NULL || dest == NULL || src == NULL || size == 0) {
        return -1;
    }

    et_lock_inplace_context(ctx);

    // memmove는 겹치는 영역을 안전하게 처리
    memmove(dest, src, size);

    // 인플레이스 이동으로 추가 메모리 할당 방지
    ctx->bytes_saved += size;
    ctx->operation_count++;

    et_unlock_inplace_context(ctx);
    return 0;
}

int et_inplace_swap(ETInPlaceContext* ctx, void* ptr1, void* ptr2, size_t size) {
    if (ctx == NULL || ptr1 == NULL || ptr2 == NULL || size == 0) {
        return -1;
    }

    if (size > ctx->buffer_size) {
        return -1; // 버퍼 크기 부족
    }

    et_lock_inplace_context(ctx);

    // 임시 버퍼를 사용한 스왑
    memcpy(ctx->buffer, ptr1, size);
    memcpy(ptr1, ptr2, size);
    memcpy(ptr2, ctx->buffer, size);

    // 추가 메모리 할당 없이 스왑 수행
    ctx->bytes_saved += size * 2; // 두 개의 임시 할당 방지
    ctx->operation_count++;

    et_unlock_inplace_context(ctx);
    return 0;
}

void et_destroy_inplace_context(ETInPlaceContext* ctx) {
    if (ctx == NULL) {
        return;
    }

    // 스레드 안전성 해제
    if (ctx->thread_safe) {
        pthread_mutex_destroy(&ctx->mutex);
    }

    // 내부 버퍼 해제 (외부 버퍼가 아닌 경우만)
    if (!ctx->is_external && ctx->buffer != NULL) {
        free(ctx->buffer);
    }

    free(ctx);
}

// =============================================================================
// 메모리 재사용 메커니즘 구현
// =============================================================================

ETMemoryReusePool* et_create_reuse_pool(size_t min_size, size_t max_size,
                                       size_t max_buffers_per_class, bool thread_safe) {
    if (min_size == 0 || max_size < min_size || max_buffers_per_class == 0) {
        return NULL;
    }

    ETMemoryReusePool* pool = (ETMemoryReusePool*)calloc(1, sizeof(ETMemoryReusePool));
    if (pool == NULL) {
        return NULL;
    }

    pool->buckets = NULL;
    pool->min_size = min_size;
    pool->max_size = max_size;
    pool->total_memory = 0;
    pool->peak_memory = 0;
    pool->total_requests = 0;
    pool->reuse_hits = 0;
    pool->cache_misses = 0;
    pool->last_cleanup_time = et_get_current_time_ms();
    pool->cleanup_interval_ms = ET_CLEANUP_INTERVAL_MS;
    pool->max_idle_time_ms = ET_MAX_IDLE_TIME_MS;
    pool->thread_safe = thread_safe;

    // 스레드 안전성 초기화
    if (thread_safe) {
        if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
            free(pool);
            return NULL;
        }
    }

    return pool;
}

void* et_reuse_alloc(ETMemoryReusePool* pool, size_t size) {
    if (pool == NULL || size == 0) {
        return NULL;
    }

    // 관리 범위 확인
    if (size < pool->min_size || size > pool->max_size) {
        pool->cache_misses++;
        return malloc(size); // 범위 밖은 일반 할당
    }

    et_lock_reuse_pool(pool);

    pool->total_requests++;

    // 크기 클래스 계산
    size_t size_class = et_get_size_class(size);

    // 해당 크기 클래스의 버킷 찾기
    ETMemoryReuseBucket* bucket = et_find_or_create_bucket(pool, size_class);
    if (bucket == NULL) {
        et_unlock_reuse_pool(pool);
        pool->cache_misses++;
        return malloc(size);
    }

    void* ptr = NULL;

    // 재사용 가능한 버퍼가 있는지 확인
    if (bucket->buffer_count > 0) {
        ptr = bucket->buffers[--bucket->buffer_count];
        bucket->reuse_hits++;
        pool->reuse_hits++;
        pool->total_memory -= size_class;
    } else {
        // 새로 할당
        ptr = malloc(size_class);
        if (ptr != NULL) {
            bucket->total_allocations++;
            pool->cache_misses++;
        }
    }

    et_unlock_reuse_pool(pool);
    return ptr;
}

void et_reuse_free(ETMemoryReusePool* pool, void* ptr, size_t size) {
    if (pool == NULL || ptr == NULL || size == 0) {
        return;
    }

    // 관리 범위 확인
    if (size < pool->min_size || size > pool->max_size) {
        free(ptr); // 범위 밖은 일반 해제
        return;
    }

    et_lock_reuse_pool(pool);

    // 크기 클래스 계산
    size_t size_class = et_get_size_class(size);

    // 해당 크기 클래스의 버킷 찾기
    ETMemoryReuseBucket* bucket = et_find_or_create_bucket(pool, size_class);
    if (bucket == NULL) {
        et_unlock_reuse_pool(pool);
        free(ptr);
        return;
    }

    // 버킷이 가득 찬 경우 즉시 해제
    if (bucket->buffer_count >= bucket->max_buffers) {
        et_unlock_reuse_pool(pool);
        free(ptr);
        return;
    }

    // 재사용 풀에 추가
    bucket->buffers[bucket->buffer_count++] = ptr;
    pool->total_memory += size_class;

    if (pool->total_memory > pool->peak_memory) {
        pool->peak_memory = pool->total_memory;
    }

    et_unlock_reuse_pool(pool);
}

size_t et_cleanup_reuse_pool(ETMemoryReusePool* pool, bool force_cleanup) {
    if (pool == NULL) {
        return 0;
    }

    et_lock_reuse_pool(pool);

    uint64_t current_time = et_get_current_time_ms();

    // 정기 정리 시간 확인
    if (!force_cleanup &&
        (current_time - pool->last_cleanup_time) < pool->cleanup_interval_ms) {
        et_unlock_reuse_pool(pool);
        return 0;
    }

    size_t freed_buffers = 0;
    ETMemoryReuseBucket* bucket = pool->buckets;

    while (bucket != NULL) {
        size_t initial_count = bucket->buffer_count;
        et_cleanup_bucket(bucket, current_time, pool->max_idle_time_ms);
        freed_buffers += (initial_count - bucket->buffer_count);
        bucket = bucket->next;
    }

    pool->last_cleanup_time = current_time;

    et_unlock_reuse_pool(pool);
    return freed_buffers;
}

void et_get_reuse_pool_stats(ETMemoryReusePool* pool, size_t* total_requests,
                            size_t* reuse_hits, float* hit_rate) {
    if (pool == NULL) {
        return;
    }

    et_lock_reuse_pool(pool);

    if (total_requests) *total_requests = pool->total_requests;
    if (reuse_hits) *reuse_hits = pool->reuse_hits;
    if (hit_rate) {
        *hit_rate = pool->total_requests > 0 ?
                   (float)pool->reuse_hits / (float)pool->total_requests : 0.0f;
    }

    et_unlock_reuse_pool(pool);
}

void et_destroy_reuse_pool(ETMemoryReusePool* pool) {
    if (pool == NULL) {
        return;
    }

    // 모든 버킷과 버퍼 해제
    ETMemoryReuseBucket* bucket = pool->buckets;
    while (bucket != NULL) {
        ETMemoryReuseBucket* next = bucket->next;

        // 버킷의 모든 버퍼 해제
        for (size_t i = 0; i < bucket->buffer_count; i++) {
            free(bucket->buffers[i]);
        }
        free(bucket->buffers);
        free(bucket);

        bucket = next;
    }

    // 스레드 안전성 해제
    if (pool->thread_safe) {
        pthread_mutex_destroy(&pool->mutex);
    }

    free(pool);
}

// =============================================================================
// 메모리 단편화 방지 구현
// =============================================================================

int et_analyze_fragmentation(ETMemoryPool* pool, ETFragmentationInfo* frag_info) {
    if (pool == NULL || frag_info == NULL) {
        return -1;
    }

    if (pool->pool_type != ET_POOL_DYNAMIC) {
        return -1; // 고정 풀은 단편화 분석 불가
    }

    et_lock_pool(pool);

    memset(frag_info, 0, sizeof(ETFragmentationInfo));

    // 자유 블록 분석
    ETMemoryBlock* current = pool->free_list;
    while (current != NULL) {
        if (current->is_free) {
            frag_info->total_free_space += current->size;
            frag_info->num_free_blocks++;

            if (current->size > frag_info->largest_free_block) {
                frag_info->largest_free_block = current->size;
            }
        }
        current = current->next;
    }

    // 단편화 비율 계산
    if (pool->total_size > 0) {
        frag_info->fragmentation_ratio =
            (float)pool->used_size / (float)pool->total_size;
    }

    // 외부 단편화 계산
    if (frag_info->total_free_space > 0) {
        frag_info->external_fragmentation =
            1.0f - ((float)frag_info->largest_free_block / (float)frag_info->total_free_space);
    }

    // 낭비된 공간 추정
    frag_info->wasted_space = frag_info->total_free_space - frag_info->largest_free_block;

    et_unlock_pool(pool);
    return 0;
}

size_t et_compact_memory_pool(ETMemoryPool* pool, bool aggressive) {
    if (pool == NULL || pool->pool_type != ET_POOL_DYNAMIC) {
        return 0;
    }

    et_lock_pool(pool);

    size_t compacted_bytes = 0;

    // 기본 압축: 인접한 자유 블록 병합
    ETMemoryBlock* current = pool->free_list;
    while (current != NULL) {
        if (current->is_free) {
            ETMemoryBlock* next_block = (ETMemoryBlock*)((uint8_t*)current +
                                       current->size + sizeof(ETMemoryBlock));

            // 다음 블록이 자유 블록인지 확인하고 병합
            ETMemoryBlock* list_block = pool->free_list;
            while (list_block != NULL) {
                if (list_block == next_block && list_block->is_free) {
                    // 병합 수행
                    current->size += next_block->size + sizeof(ETMemoryBlock);
                    compacted_bytes += sizeof(ETMemoryBlock);

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

    // 적극적 압축: 사용 중인 블록 재배치 (구현 복잡도로 인해 기본 구현만 제공)
    if (aggressive) {
        // 실제 구현에서는 사용 중인 블록들을 메모리 앞쪽으로 이동시켜
        // 자유 공간을 하나의 큰 블록으로 통합할 수 있음
        // 여기서는 단순화된 버전만 구현
    }

    et_unlock_pool(pool);
    return compacted_bytes;
}

size_t et_optimize_memory_layout(ETMemoryPool* pool) {
    if (pool == NULL || pool->pool_type != ET_POOL_DYNAMIC) {
        return 0;
    }

    et_lock_pool(pool);

    size_t optimized_blocks = 0;

    // 자유 블록 리스트를 크기 순으로 정렬하여 할당 효율성 향상
    // (실제 구현에서는 더 복잡한 최적화 수행)

    // 기본적인 블록 병합 수행
    optimized_blocks = et_compact_memory_pool(pool, false);

    et_unlock_pool(pool);
    return optimized_blocks;
}

int et_set_allocation_strategy(ETMemoryPool* pool, ETAllocationStrategy strategy) {
    if (pool == NULL || pool->pool_type != ET_POOL_DYNAMIC) {
        return -1;
    }

    // 실제 구현에서는 pool 구조체에 strategy 필드를 추가하고
    // et_find_free_block 함수에서 해당 전략을 사용하도록 수정
    // 여기서는 기본 구현만 제공

    return 0;
}

int et_set_auto_compaction(ETMemoryPool* pool, bool enable, float threshold) {
    if (pool == NULL || threshold < 0.0f || threshold > 1.0f) {
        return -1;
    }

    // 실제 구현에서는 pool 구조체에 auto_compaction 필드를 추가하고
    // 할당/해제 시 단편화 비율을 확인하여 자동 압축 수행
    // 여기서는 기본 구현만 제공

    return 0;
}

// =============================================================================
// 스마트 메모리 관리 구현
// =============================================================================

ETSmartMemoryManager* et_create_smart_memory_manager(size_t pool_size,
                                                    size_t reuse_pool_config,
                                                    size_t inplace_buffer_size,
                                                    bool thread_safe) {
    if (pool_size == 0) {
        return NULL;
    }

    ETSmartMemoryManager* manager = (ETSmartMemoryManager*)calloc(1, sizeof(ETSmartMemoryManager));
    if (manager == NULL) {
        return NULL;
    }

    // 주 메모리 풀 생성
    manager->primary_pool = et_create_memory_pool(pool_size, ET_DEFAULT_ALIGNMENT);
    if (manager->primary_pool == NULL) {
        free(manager);
        return NULL;
    }

    // 재사용 풀 생성
    manager->reuse_pool = et_create_reuse_pool(64, reuse_pool_config, 16, thread_safe);
    if (manager->reuse_pool == NULL) {
        et_destroy_memory_pool(manager->primary_pool);
        free(manager);
        return NULL;
    }

    // 인플레이스 컨텍스트 생성
    if (inplace_buffer_size > 0) {
        manager->inplace_ctx = et_create_inplace_context(inplace_buffer_size,
                                                        ET_DEFAULT_ALIGNMENT, thread_safe);
        if (manager->inplace_ctx == NULL) {
            et_destroy_reuse_pool(manager->reuse_pool);
            et_destroy_memory_pool(manager->primary_pool);
            free(manager);
            return NULL;
        }
    }

    // 히스토그램 초기화
    manager->size_histogram = (size_t*)calloc(ET_HISTOGRAM_BUCKETS, sizeof(size_t));
    manager->histogram_buckets = ET_HISTOGRAM_BUCKETS;
    manager->access_timestamps = (uint64_t*)calloc(ET_HISTOGRAM_BUCKETS, sizeof(uint64_t));

    // 기본 설정
    manager->current_strategy = ET_ALLOC_FIRST_FIT;
    manager->compaction_threshold = 0.7f;
    manager->auto_optimization = true;
    manager->total_allocations = 0;
    manager->total_frees = 0;
    manager->bytes_saved = 0;
    manager->optimization_count = 0;
    manager->thread_safe = thread_safe;

    // 스레드 안전성 초기화
    if (thread_safe) {
        if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
            free(manager->size_histogram);
            free(manager->access_timestamps);
            if (manager->inplace_ctx) et_destroy_inplace_context(manager->inplace_ctx);
            et_destroy_reuse_pool(manager->reuse_pool);
            et_destroy_memory_pool(manager->primary_pool);
            free(manager);
            return NULL;
        }
    }

    return manager;
}

void* et_smart_alloc(ETSmartMemoryManager* manager, size_t size) {
    if (manager == NULL || size == 0) {
        return NULL;
    }

    et_lock_smart_manager(manager);

    manager->total_allocations++;

    // 크기 히스토그램 업데이트
    size_t bucket = (size / 64) % manager->histogram_buckets;
    manager->size_histogram[bucket]++;
    manager->access_timestamps[bucket] = et_get_current_time_ms();

    void* ptr = NULL;

    // 재사용 풀에서 먼저 시도
    ptr = et_reuse_alloc(manager->reuse_pool, size);
    if (ptr != NULL) {
        manager->bytes_saved += size; // 재사용으로 절약
        et_unlock_smart_manager(manager);
        return ptr;
    }

    // 주 메모리 풀에서 할당
    ptr = et_alloc_from_pool(manager->primary_pool, size);

    // 자동 최적화 확인
    if (manager->auto_optimization && (manager->total_allocations % 100) == 0) {
        ETFragmentationInfo frag_info;
        if (et_analyze_fragmentation(manager->primary_pool, &frag_info) == 0) {
            if (frag_info.fragmentation_ratio > manager->compaction_threshold) {
                et_compact_memory_pool(manager->primary_pool, false);
                manager->optimization_count++;
            }
        }
    }

    et_unlock_smart_manager(manager);
    return ptr;
}

void et_smart_free(ETSmartMemoryManager* manager, void* ptr, size_t size) {
    if (manager == NULL || ptr == NULL) {
        return;
    }

    et_lock_smart_manager(manager);

    manager->total_frees++;

    // 재사용 풀에 반환 시도
    et_reuse_free(manager->reuse_pool, ptr, size);

    et_unlock_smart_manager(manager);
}

size_t et_optimize_memory_usage(ETSmartMemoryManager* manager) {
    if (manager == NULL) {
        return 0;
    }

    et_lock_smart_manager(manager);

    size_t optimizations = 0;

    // 메모리 풀 압축
    size_t compacted = et_compact_memory_pool(manager->primary_pool, true);
    if (compacted > 0) {
        optimizations++;
        manager->bytes_saved += compacted;
    }

    // 재사용 풀 정리
    size_t cleaned = et_cleanup_reuse_pool(manager->reuse_pool, false);
    if (cleaned > 0) {
        optimizations++;
    }

    // 사용 패턴 분석 기반 최적화
    // (실제 구현에서는 히스토그램 데이터를 분석하여 최적 전략 선택)

    manager->optimization_count += optimizations;

    et_unlock_smart_manager(manager);
    return optimizations;
}

void et_get_smart_manager_stats(ETSmartMemoryManager* manager,
                               uint64_t* total_allocations,
                               uint64_t* bytes_saved,
                               uint64_t* optimization_count) {
    if (manager == NULL) {
        return;
    }

    et_lock_smart_manager(manager);

    if (total_allocations) *total_allocations = manager->total_allocations;
    if (bytes_saved) *bytes_saved = manager->bytes_saved;
    if (optimization_count) *optimization_count = manager->optimization_count;

    et_unlock_smart_manager(manager);
}

void et_destroy_smart_memory_manager(ETSmartMemoryManager* manager) {
    if (manager == NULL) {
        return;
    }

    // 컴포넌트들 해제
    if (manager->inplace_ctx) {
        et_destroy_inplace_context(manager->inplace_ctx);
    }

    et_destroy_reuse_pool(manager->reuse_pool);
    et_destroy_memory_pool(manager->primary_pool);

    // 히스토그램 해제
    free(manager->size_histogram);
    free(manager->access_timestamps);

    // 스레드 안전성 해제
    if (manager->thread_safe) {
        pthread_mutex_destroy(&manager->mutex);
    }

    free(manager);
}

// =============================================================================
// 내부 함수 구현
// =============================================================================

static void et_lock_inplace_context(ETInPlaceContext* ctx) {
    if (ctx->thread_safe) {
        pthread_mutex_lock(&ctx->mutex);
    }
}

static void et_unlock_inplace_context(ETInPlaceContext* ctx) {
    if (ctx->thread_safe) {
        pthread_mutex_unlock(&ctx->mutex);
    }
}

static void et_lock_reuse_pool(ETMemoryReusePool* pool) {
    if (pool->thread_safe) {
        pthread_mutex_lock(&pool->mutex);
    }
}

static void et_unlock_reuse_pool(ETMemoryReusePool* pool) {
    if (pool->thread_safe) {
        pthread_mutex_unlock(&pool->mutex);
    }
}

static void et_lock_smart_manager(ETSmartMemoryManager* manager) {
    if (manager->thread_safe) {
        pthread_mutex_lock(&manager->mutex);
    }
}

static void et_unlock_smart_manager(ETSmartMemoryManager* manager) {
    if (manager->thread_safe) {
        pthread_mutex_unlock(&manager->mutex);
    }
}

static uint64_t et_get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static size_t et_get_size_class(size_t size) {
    // 2의 거듭제곱으로 올림하여 크기 클래스 결정
    return et_round_up_to_power_of_2(size);
}

static ETMemoryReuseBucket* et_find_or_create_bucket(ETMemoryReusePool* pool, size_t size_class) {
    // 기존 버킷 찾기
    ETMemoryReuseBucket* bucket = pool->buckets;
    while (bucket != NULL) {
        if (bucket->size_class == size_class) {
            return bucket;
        }
        bucket = bucket->next;
    }

    // 새 버킷 생성
    bucket = (ETMemoryReuseBucket*)calloc(1, sizeof(ETMemoryReuseBucket));
    if (bucket == NULL) {
        return NULL;
    }

    bucket->size_class = size_class;
    bucket->max_buffers = 16; // 기본값
    bucket->buffers = (void**)calloc(bucket->max_buffers, sizeof(void*));
    if (bucket->buffers == NULL) {
        free(bucket);
        return NULL;
    }

    // 리스트에 추가
    bucket->next = pool->buckets;
    pool->buckets = bucket;

    return bucket;
}

static void et_cleanup_bucket(ETMemoryReuseBucket* bucket, uint64_t current_time, size_t max_idle_time) {
    // 간단한 정리: 버킷의 절반 정도 해제
    size_t to_free = bucket->buffer_count / 2;

    for (size_t i = 0; i < to_free; i++) {
        free(bucket->buffers[bucket->buffer_count - 1 - i]);
    }

    bucket->buffer_count -= to_free;
}

// =============================================================================
// 유틸리티 함수 구현
// =============================================================================

size_t et_round_up_to_power_of_2(size_t size) {
    if (size == 0) return 1;

    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    if (sizeof(size_t) > 4) {
        size |= size >> 32;
    }
    size++;

    return size;
}

int et_generate_memory_recommendations(ETMemoryPool* pool, char* recommendations, size_t buffer_size) {
    if (pool == NULL || recommendations == NULL || buffer_size == 0) {
        return 0;
    }

    int rec_count = 0;
    size_t offset = 0;

    ETFragmentationInfo frag_info;
    if (et_analyze_fragmentation(pool, &frag_info) == 0) {
        if (frag_info.fragmentation_ratio > 0.8f) {
            offset += snprintf(recommendations + offset, buffer_size - offset,
                             "권장사항 %d: 메모리 사용률이 높습니다 (%.1f%%). 메모리 풀 크기를 늘리거나 압축을 수행하세요.\n",
                             ++rec_count, frag_info.fragmentation_ratio * 100.0f);
        }

        if (frag_info.external_fragmentation > 0.5f) {
            offset += snprintf(recommendations + offset, buffer_size - offset,
                             "권장사항 %d: 외부 단편화가 심합니다 (%.1f%%). 메모리 압축을 수행하세요.\n",
                             ++rec_count, frag_info.external_fragmentation * 100.0f);
        }

        if (frag_info.num_free_blocks > 20) {
            offset += snprintf(recommendations + offset, buffer_size - offset,
                             "권장사항 %d: 자유 블록이 너무 많습니다 (%zu개). 블록 병합을 수행하세요.\n",
                             ++rec_count, frag_info.num_free_blocks);
        }
    }

    return rec_count;
}

void et_print_memory_optimization_report(ETSmartMemoryManager* manager,
                                        ETMemoryPool* pool,
                                        const char* output_file) {
    FILE* fp = output_file ? fopen(output_file, "w") : stdout;
    if (fp == NULL) {
        return;
    }

    fprintf(fp, "=== LibEtude 메모리 최적화 리포트 ===\n\n");

    // 스마트 매니저 통계
    if (manager != NULL) {
        uint64_t total_allocs, bytes_saved, opt_count;
        et_get_smart_manager_stats(manager, &total_allocs, &bytes_saved, &opt_count);

        fprintf(fp, "스마트 메모리 매니저 통계:\n");
        fprintf(fp, "  총 할당 횟수: %llu\n", (unsigned long long)total_allocs);
        fprintf(fp, "  절약된 바이트: %llu (%.2f KB)\n",
               (unsigned long long)bytes_saved, (double)bytes_saved / 1024.0);
        fprintf(fp, "  최적화 수행 횟수: %llu\n", (unsigned long long)opt_count);

        // 재사용 풀 통계
        size_t total_requests, reuse_hits;
        float hit_rate;
        et_get_reuse_pool_stats(manager->reuse_pool, &total_requests, &reuse_hits, &hit_rate);

        fprintf(fp, "\n재사용 풀 통계:\n");
        fprintf(fp, "  총 요청 수: %zu\n", total_requests);
        fprintf(fp, "  재사용 성공 수: %zu\n", reuse_hits);
        fprintf(fp, "  재사용 성공률: %.2f%%\n", hit_rate * 100.0f);
    }

    // 메모리 풀 통계
    if (pool != NULL) {
        ETMemoryPoolStats stats;
        et_get_pool_stats(pool, &stats);

        fprintf(fp, "\n메모리 풀 통계:\n");
        fprintf(fp, "  총 크기: %zu bytes (%.2f MB)\n",
               stats.total_size, (double)stats.total_size / (1024.0 * 1024.0));
        fprintf(fp, "  사용된 크기: %zu bytes (%.2f MB)\n",
               stats.used_size, (double)stats.used_size / (1024.0 * 1024.0));
        fprintf(fp, "  최대 사용량: %zu bytes (%.2f MB)\n",
               stats.peak_usage, (double)stats.peak_usage / (1024.0 * 1024.0));
        fprintf(fp, "  사용률: %.2f%%\n",
               (double)stats.used_size / (double)stats.total_size * 100.0);
        fprintf(fp, "  단편화 비율: %.2f%%\n", stats.fragmentation_ratio * 100.0f);

        // 단편화 분석
        ETFragmentationInfo frag_info;
        if (et_analyze_fragmentation(pool, &frag_info) == 0) {
            fprintf(fp, "\n단편화 분석:\n");
            fprintf(fp, "  총 자유 공간: %zu bytes\n", frag_info.total_free_space);
            fprintf(fp, "  최대 자유 블록: %zu bytes\n", frag_info.largest_free_block);
            fprintf(fp, "  자유 블록 수: %zu\n", frag_info.num_free_blocks);
            fprintf(fp, "  외부 단편화: %.2f%%\n", frag_info.external_fragmentation * 100.0f);
            fprintf(fp, "  낭비된 공간: %zu bytes\n", frag_info.wasted_space);
        }

        // 권장사항
        char recommendations[1024];
        int rec_count = et_generate_memory_recommendations(pool, recommendations, sizeof(recommendations));
        if (rec_count > 0) {
            fprintf(fp, "\n최적화 권장사항:\n");
            fprintf(fp, "%s", recommendations);
        }
    }

    fprintf(fp, "\n=== 리포트 끝 ===\n");

    if (output_file && fp != stdout) {
        fclose(fp);
    }
}

// =============================================================================
// 누락된 함수 구현
// =============================================================================

static ETMemoryBlock* et_find_best_fit_block(ETMemoryPool* pool, size_t size) {
    ETMemoryBlock* current = pool->free_list;
    ETMemoryBlock* best_fit = NULL;
    size_t best_size = SIZE_MAX;

    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size < best_size) {
                best_fit = current;
                best_size = current->size;
            }
        }
        current = current->next;
    }

    return best_fit;
}

static ETMemoryBlock* et_find_worst_fit_block(ETMemoryPool* pool, size_t size) {
    ETMemoryBlock* current = pool->free_list;
    ETMemoryBlock* worst_fit = NULL;
    size_t worst_size = 0;

    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            if (current->size > worst_size) {
                worst_fit = current;
                worst_size = current->size;
            }
        }
        current = current->next;
    }

    return worst_fit;
}