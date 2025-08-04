/**
 * @file memory_optimization.c
 * @brief 모바일 메모리 최적화 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/memory_optimization.h"
#include "libetude/api.h"
#include "libetude/error.h"
#include "libetude/memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#ifdef ANDROID_PLATFORM
#include <android/log.h>
#include <sys/sysinfo.h>
#endif

#ifdef IOS_PLATFORM
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#endif

// 최대 메모리 블록 수
#define MAX_MEMORY_BLOCKS 1024

// 메모리 히스토리 크기
#define MEMORY_HISTORY_SIZE 60

// 전역 메모리 최적화 상태
static struct {
    bool initialized;
    MemoryOptimizationConfig config;
    MemoryUsageStats stats;

    // 메모리 블록 추적
    MemoryBlockInfo blocks[MAX_MEMORY_BLOCKS];
    int block_count;

    // 메모리 사용량 히스토리
    size_t memory_history[MEMORY_HISTORY_SIZE];
    int history_index;

    // 모니터링 스레드
    pthread_t monitoring_thread;
    bool monitoring_active;
    MemoryEventCallback event_callback;
    void* callback_user_data;
    int monitoring_interval_ms;

    // 가비지 컬렉션
    pthread_t gc_thread;
    bool gc_active;
    bool auto_gc_enabled;
    int gc_interval_ms;
    float gc_threshold;

    // 압축 설정
    bool compression_enabled;
    MemoryCompressionType compression_type;
    int compression_level;

    // 캐시 통계
    int cache_hits;
    int cache_misses;

    // 동기화
    pthread_mutex_t mutex;

    // 통계
    int64_t start_time_ms;
    int gc_count;
    int64_t total_gc_time_ms;
} g_memory_state = {0};

// 내부 함수 선언
static void* memory_monitoring_thread(void* arg);
static void* memory_gc_thread(void* arg);
static int update_system_memory_info();
static int update_libetude_memory_info();
static size_t perform_garbage_collection();
static int compress_memory_block(const void* data, size_t size, void** compressed_data, size_t* compressed_size);
static int decompress_memory_block(const void* compressed_data, size_t compressed_size, void** data, size_t* size);
static int64_t get_current_time_ms();

// ============================================================================
// 초기화 및 정리 함수들
// ============================================================================

int memory_optimization_init() {
    pthread_mutex_lock(&g_memory_state.mutex);

    if (g_memory_state.initialized) {
        pthread_mutex_unlock(&g_memory_state.mutex);
        return LIBETUDE_SUCCESS;
    }

    // 기본 설정 초기화
    memset(&g_memory_state.config, 0, sizeof(MemoryOptimizationConfig));
    memset(&g_memory_state.stats, 0, sizeof(MemoryUsageStats));

    // 기본 설정값
    g_memory_state.config.strategy = MEMORY_STRATEGY_BALANCED;
    g_memory_state.config.compression_type = MEMORY_COMPRESSION_LZ4;
    g_memory_state.config.max_memory_mb = 256;
    g_memory_state.config.warning_threshold_mb = 192;
    g_memory_state.config.critical_threshold_mb = 224;
    g_memory_state.config.pool_type = MEMORY_POOL_DYNAMIC;
    g_memory_state.config.pool_size_mb = 64;
    g_memory_state.config.pool_alignment = 16;
    g_memory_state.config.enable_compression = true;
    g_memory_state.config.compression_threshold = 0.7f;
    g_memory_state.config.compression_level = 3;
    g_memory_state.config.enable_gc = true;
    g_memory_state.config.gc_interval_ms = 30000;
    g_memory_state.config.gc_threshold = 0.8f;
    g_memory_state.config.enable_swap = false;
    g_memory_state.config.swap_size_mb = 128;
    g_memory_state.config.enable_cache_optimization = true;
    g_memory_state.config.l1_cache_size_kb = 32;
    g_memory_state.config.l2_cache_size_kb = 256;

    // 초기 상태 설정
    g_memory_state.stats.pressure_level = MEMORY_PRESSURE_NONE;
    g_memory_state.stats.memory_efficiency = 1.0f;

    // 압축 설정
    g_memory_state.compression_enabled = g_memory_state.config.enable_compression;
    g_memory_state.compression_type = g_memory_state.config.compression_type;
    g_memory_state.compression_level = g_memory_state.config.compression_level;

    // 자동 GC 설정
    g_memory_state.auto_gc_enabled = g_memory_state.config.enable_gc;
    g_memory_state.gc_interval_ms = g_memory_state.config.gc_interval_ms;
    g_memory_state.gc_threshold = g_memory_state.config.gc_threshold;

    // 시작 시간 기록
    g_memory_state.start_time_ms = get_current_time_ms();

    g_memory_state.initialized = true;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_optimization_cleanup() {
    pthread_mutex_lock(&g_memory_state.mutex);

    if (!g_memory_state.initialized) {
        pthread_mutex_unlock(&g_memory_state.mutex);
        return LIBETUDE_SUCCESS;
    }

    // 모니터링 중지
    if (g_memory_state.monitoring_active) {
        g_memory_state.monitoring_active = false;
        pthread_mutex_unlock(&g_memory_state.mutex);
        pthread_join(g_memory_state.monitoring_thread, NULL);
        pthread_mutex_lock(&g_memory_state.mutex);
    }

    // GC 스레드 중지
    if (g_memory_state.gc_active) {
        g_memory_state.gc_active = false;
        pthread_mutex_unlock(&g_memory_state.mutex);
        pthread_join(g_memory_state.gc_thread, NULL);
        pthread_mutex_lock(&g_memory_state.mutex);
    }

    g_memory_state.initialized = false;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_set_optimization_config(const MemoryOptimizationConfig* config) {
    if (!config) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_memory_state.initialized) {
        memory_optimization_init();
    }

    pthread_mutex_lock(&g_memory_state.mutex);
    g_memory_state.config = *config;

    // 압축 설정 업데이트
    g_memory_state.compression_enabled = config->enable_compression;
    g_memory_state.compression_type = config->compression_type;
    g_memory_state.compression_level = config->compression_level;

    // 자동 GC 설정 업데이트
    g_memory_state.auto_gc_enabled = config->enable_gc;
    g_memory_state.gc_interval_ms = config->gc_interval_ms;
    g_memory_state.gc_threshold = config->gc_threshold;

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_get_optimization_config(MemoryOptimizationConfig* config) {
    if (!config) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_memory_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_memory_state.mutex);
    *config = g_memory_state.config;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 메모리 사용량 모니터링 함수들
// ============================================================================

int memory_get_usage_stats(MemoryUsageStats* stats) {
    if (!stats) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_memory_state.initialized) {
        memory_optimization_init();
    }

    // 최신 통계 업데이트
    memory_update_usage_stats();

    pthread_mutex_lock(&g_memory_state.mutex);
    *stats = g_memory_state.stats;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_update_usage_stats() {
    if (!g_memory_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    // 시스템 메모리 정보 업데이트
    update_system_memory_info();

    // LibEtude 메모리 정보 업데이트
    update_libetude_memory_info();

    pthread_mutex_lock(&g_memory_state.mutex);

    // 메모리 압박 레벨 결정
    MemoryPressureLevel old_level = g_memory_state.stats.pressure_level;
    MemoryPressureLevel new_level = memory_determine_pressure_level(
        g_memory_state.stats.used_memory_mb,
        g_memory_state.stats.total_memory_mb,
        &g_memory_state.config
    );

    g_memory_state.stats.pressure_level = new_level;

    // 메모리 효율성 계산
    if (g_memory_state.stats.total_memory_mb > 0) {
        float usage_ratio = (float)g_memory_state.stats.used_memory_mb / g_memory_state.stats.total_memory_mb;
        g_memory_state.stats.memory_efficiency = 1.0f - usage_ratio;
    }

    // 압박 레벨 변경 시 이벤트 콜백 호출
    if (old_level != new_level && g_memory_state.event_callback) {
        g_memory_state.event_callback(old_level, new_level, &g_memory_state.stats,
                                     g_memory_state.callback_user_data);
    }

    // 메모리 히스토리 업데이트
    g_memory_state.memory_history[g_memory_state.history_index] = g_memory_state.stats.used_memory_mb;
    g_memory_state.history_index = (g_memory_state.history_index + 1) % MEMORY_HISTORY_SIZE;

    // 캐시 히트 비율 계산
    int total_accesses = g_memory_state.cache_hits + g_memory_state.cache_misses;
    if (total_accesses > 0) {
        g_memory_state.stats.cache_hit_ratio = (float)g_memory_state.cache_hits / total_accesses;
    }

    g_memory_state.stats.cache_hits = g_memory_state.cache_hits;
    g_memory_state.stats.cache_misses = g_memory_state.cache_misses;
    g_memory_state.stats.gc_count = g_memory_state.gc_count;
    g_memory_state.stats.total_gc_time_ms = g_memory_state.total_gc_time_ms;

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

MemoryPressureLevel memory_determine_pressure_level(size_t used_memory_mb, size_t total_memory_mb,
                                                   const MemoryOptimizationConfig* config) {
    if (!config || total_memory_mb == 0) {
        return MEMORY_PRESSURE_NONE;
    }

    float usage_ratio = (float)used_memory_mb / total_memory_mb;

    if (used_memory_mb >= config->critical_threshold_mb || usage_ratio >= 0.95f) {
        return MEMORY_PRESSURE_CRITICAL;
    } else if (used_memory_mb >= config->warning_threshold_mb || usage_ratio >= 0.85f) {
        return MEMORY_PRESSURE_HIGH;
    } else if (usage_ratio >= 0.70f) {
        return MEMORY_PRESSURE_MEDIUM;
    } else if (usage_ratio >= 0.50f) {
        return MEMORY_PRESSURE_LOW;
    } else {
        return MEMORY_PRESSURE_NONE;
    }
}

// ============================================================================
// 메모리 압박 처리 함수들
// ============================================================================

int memory_handle_pressure(void* engine, MemoryPressureLevel pressure_level) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    size_t freed_memory = 0;

    switch (pressure_level) {
        case MEMORY_PRESSURE_NONE:
            // 압박 없음 - 아무것도 하지 않음
            break;

        case MEMORY_PRESSURE_LOW:
            // 낮은 압박 - 가벼운 정리
            freed_memory = memory_cleanup_unused(engine);
            break;

        case MEMORY_PRESSURE_MEDIUM:
            // 중간 압박 - 압축 및 캐시 정리
            if (g_memory_state.compression_enabled) {
                // 압축 가능한 메모리 블록 압축
            }
            memory_flush_cache();
            freed_memory = memory_cleanup_unused(engine);
            break;

        case MEMORY_PRESSURE_HIGH:
            // 높은 압박 - 적극적 메모리 해제
            freed_memory = memory_free_memory(engine, 64);  // 64MB 해제 시도
            memory_garbage_collect(engine);
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
            break;

        case MEMORY_PRESSURE_CRITICAL:
            // 임계 압박 - 최대한 메모리 해제
            freed_memory = memory_free_memory(engine, 128);  // 128MB 해제 시도
            memory_garbage_collect(engine);
            memory_defragment();
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
            break;
    }

    return LIBETUDE_SUCCESS;
}

size_t memory_free_memory(void* engine, size_t target_mb) {
    if (!engine) {
        return 0;
    }

    size_t freed_mb = 0;

    // 1. 사용하지 않는 메모리 정리
    freed_mb += memory_cleanup_unused(engine);

    // 2. 캐시 플러시
    memory_flush_cache();
    freed_mb += 16;  // 추정값

    // 3. 가비지 컬렉션
    freed_mb += memory_garbage_collect(engine);

    // 4. 메모리 단편화 해소
    if (freed_mb < target_mb) {
        memory_defragment();
        freed_mb += 8;  // 추정값
    }

    // 5. 압축된 데이터 일부 해제
    if (freed_mb < target_mb && g_memory_state.compression_enabled) {
        // 압축된 블록 중 일부 해제
        freed_mb += 32;  // 추정값
    }

    return freed_mb;
}

size_t memory_cleanup_unused(void* engine) {
    if (!engine) {
        return 0;
    }

    size_t freed_mb = 0;

    pthread_mutex_lock(&g_memory_state.mutex);

    // 참조되지 않는 메모리 블록 찾기 및 해제
    for (int i = 0; i < g_memory_state.block_count; i++) {
        MemoryBlockInfo* block = &g_memory_state.blocks[i];

        if (block->reference_count == 0) {
            // 마지막 접근 시간이 오래된 블록 해제
            int64_t current_time = get_current_time_ms();
            if (current_time - block->last_access_time > 60000) {  // 1분 이상
                if (block->address) {
                    free(block->address);
                    freed_mb += block->size / (1024 * 1024);

                    // 블록 정보 제거
                    memmove(&g_memory_state.blocks[i], &g_memory_state.blocks[i + 1],
                           (g_memory_state.block_count - i - 1) * sizeof(MemoryBlockInfo));
                    g_memory_state.block_count--;
                    i--;  // 인덱스 조정
                }
            }
        }
    }

    pthread_mutex_unlock(&g_memory_state.mutex);

    return freed_mb;
}

int memory_defragment() {
    // 메모리 단편화 해소 (실제로는 복잡한 구현 필요)
    // 여기서는 시뮬레이션

    pthread_mutex_lock(&g_memory_state.mutex);

    // 메모리 블록들을 주소 순으로 정렬
    for (int i = 0; i < g_memory_state.block_count - 1; i++) {
        for (int j = i + 1; j < g_memory_state.block_count; j++) {
            if (g_memory_state.blocks[i].address > g_memory_state.blocks[j].address) {
                MemoryBlockInfo temp = g_memory_state.blocks[i];
                g_memory_state.blocks[i] = g_memory_state.blocks[j];
                g_memory_state.blocks[j] = temp;
            }
        }
    }

    // 단편화 비율 재계산
    size_t total_size = 0;
    size_t largest_free_block = 0;

    for (int i = 0; i < g_memory_state.block_count; i++) {
        total_size += g_memory_state.blocks[i].size;
    }

    if (total_size > 0) {
        g_memory_state.stats.pool_fragmentation = 1.0f - ((float)largest_free_block / total_size);
    }

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 메모리 압축 함수들
// ============================================================================

int memory_enable_compression(MemoryCompressionType compression_type, int compression_level) {
    if (compression_level < 1 || compression_level > 9) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_memory_state.mutex);
    g_memory_state.compression_enabled = true;
    g_memory_state.compression_type = compression_type;
    g_memory_state.compression_level = compression_level;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_disable_compression() {
    pthread_mutex_lock(&g_memory_state.mutex);
    g_memory_state.compression_enabled = false;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_compress_block(const void* data, size_t size, void** compressed_data, size_t* compressed_size) {
    if (!data || !compressed_data || !compressed_size || size == 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_memory_state.compression_enabled) {
        return LIBETUDE_ERROR_NOT_IMPLEMENTED;
    }

    return compress_memory_block(data, size, compressed_data, compressed_size);
}

int memory_decompress_block(const void* compressed_data, size_t compressed_size, void** data, size_t* size) {
    if (!compressed_data || !data || !size || compressed_size == 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_memory_state.compression_enabled) {
        return LIBETUDE_ERROR_NOT_IMPLEMENTED;
    }

    return decompress_memory_block(compressed_data, compressed_size, data, size);
}

// ============================================================================
// 가비지 컬렉션 함수들
// ============================================================================

size_t memory_garbage_collect(void* engine) {
    if (!engine) {
        return 0;
    }

    int64_t start_time = get_current_time_ms();
    size_t freed_mb = perform_garbage_collection();
    int64_t end_time = get_current_time_ms();

    pthread_mutex_lock(&g_memory_state.mutex);
    g_memory_state.gc_count++;
    g_memory_state.total_gc_time_ms += (end_time - start_time);
    pthread_mutex_unlock(&g_memory_state.mutex);

    return freed_mb;
}

int memory_enable_auto_gc(int interval_ms, float threshold) {
    if (interval_ms <= 0 || threshold < 0.0f || threshold > 1.0f) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_memory_state.mutex);

    g_memory_state.auto_gc_enabled = true;
    g_memory_state.gc_interval_ms = interval_ms;
    g_memory_state.gc_threshold = threshold;

    // GC 스레드가 실행 중이 아니면 시작
    if (!g_memory_state.gc_active) {
        g_memory_state.gc_active = true;
        int result = pthread_create(&g_memory_state.gc_thread, NULL, memory_gc_thread, NULL);
        if (result != 0) {
            g_memory_state.gc_active = false;
            g_memory_state.auto_gc_enabled = false;
            pthread_mutex_unlock(&g_memory_state.mutex);
            return LIBETUDE_ERROR_RUNTIME;
        }
    }

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_disable_auto_gc() {
    pthread_mutex_lock(&g_memory_state.mutex);
    g_memory_state.auto_gc_enabled = false;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 캐시 최적화 함수들
// ============================================================================

int memory_enable_cache_optimization(size_t l1_cache_size_kb, size_t l2_cache_size_kb) {
    pthread_mutex_lock(&g_memory_state.mutex);
    g_memory_state.config.enable_cache_optimization = true;
    g_memory_state.config.l1_cache_size_kb = l1_cache_size_kb;
    g_memory_state.config.l2_cache_size_kb = l2_cache_size_kb;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_flush_cache() {
    pthread_mutex_lock(&g_memory_state.mutex);

    // 캐시된 메모리 블록들의 캐시 플래그 해제
    for (int i = 0; i < g_memory_state.block_count; i++) {
        g_memory_state.blocks[i].is_cached = false;
    }

    // 캐시 통계 리셋
    g_memory_state.cache_hits = 0;
    g_memory_state.cache_misses = 0;

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_get_cache_stats(int* hits, int* misses, float* hit_ratio) {
    if (!hits || !misses || !hit_ratio) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_memory_state.mutex);
    *hits = g_memory_state.cache_hits;
    *misses = g_memory_state.cache_misses;

    int total = *hits + *misses;
    *hit_ratio = (total > 0) ? ((float)*hits / total) : 0.0f;

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 모니터링 및 이벤트 함수들
// ============================================================================

int memory_start_monitoring(MemoryEventCallback callback, void* user_data, int interval_ms) {
    if (!callback || interval_ms <= 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_memory_state.initialized) {
        memory_optimization_init();
    }

    pthread_mutex_lock(&g_memory_state.mutex);

    if (g_memory_state.monitoring_active) {
        pthread_mutex_unlock(&g_memory_state.mutex);
        return LIBETUDE_ERROR_RUNTIME;  // 이미 모니터링 중
    }

    g_memory_state.event_callback = callback;
    g_memory_state.callback_user_data = user_data;
    g_memory_state.monitoring_interval_ms = interval_ms;
    g_memory_state.monitoring_active = true;

    int result = pthread_create(&g_memory_state.monitoring_thread, NULL, memory_monitoring_thread, NULL);
    if (result != 0) {
        g_memory_state.monitoring_active = false;
        pthread_mutex_unlock(&g_memory_state.mutex);
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_unlock(&g_memory_state.mutex);
    return LIBETUDE_SUCCESS;
}

int memory_stop_monitoring() {
    pthread_mutex_lock(&g_memory_state.mutex);

    if (!g_memory_state.monitoring_active) {
        pthread_mutex_unlock(&g_memory_state.mutex);
        return LIBETUDE_SUCCESS;
    }

    g_memory_state.monitoring_active = false;
    pthread_mutex_unlock(&g_memory_state.mutex);

    pthread_join(g_memory_state.monitoring_thread, NULL);

    return LIBETUDE_SUCCESS;
}

int memory_set_event_callback(MemoryEventCallback callback, void* user_data) {
    if (!g_memory_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_memory_state.mutex);
    g_memory_state.event_callback = callback;
    g_memory_state.callback_user_data = user_data;
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 통계 및 리포트 함수들
// ============================================================================

char* memory_generate_optimization_report() {
    if (!g_memory_state.initialized) {
        return NULL;
    }

    char* report = malloc(3072);
    if (!report) {
        return NULL;
    }

    memory_update_usage_stats();

    pthread_mutex_lock(&g_memory_state.mutex);

    snprintf(report, 3072,
        "=== LibEtude Memory Optimization Report ===\n\n"
        "Strategy: %s\n"
        "Pressure Level: %s\n"
        "Memory Efficiency: %.2f\n\n"

        "System Memory:\n"
        "  Total: %zu MB\n"
        "  Used: %zu MB (%.1f%%)\n"
        "  Available: %zu MB\n"
        "  Free: %zu MB\n\n"

        "LibEtude Memory:\n"
        "  Total: %zu MB\n"
        "  Model: %zu MB\n"
        "  Tensor: %zu MB\n"
        "  Audio Buffer: %zu MB\n\n"

        "Memory Pool:\n"
        "  Allocated: %zu MB\n"
        "  Free: %zu MB\n"
        "  Fragmentation: %.2f%%\n\n"

        "Compression:\n"
        "  Enabled: %s\n"
        "  Type: %s\n"
        "  Compressed: %zu MB\n"
        "  Uncompressed: %zu MB\n"
        "  Ratio: %.2f\n\n"

        "Cache:\n"
        "  Hits: %d\n"
        "  Misses: %d\n"
        "  Hit Ratio: %.2f%%\n\n"

        "Garbage Collection:\n"
        "  Count: %d\n"
        "  Total Time: %.1f seconds\n"
        "  Auto GC: %s\n\n"

        "Thresholds:\n"
        "  Warning: %zu MB\n"
        "  Critical: %zu MB\n",

        // Strategy
        (g_memory_state.config.strategy == MEMORY_STRATEGY_NONE) ? "None" :
        (g_memory_state.config.strategy == MEMORY_STRATEGY_CONSERVATIVE) ? "Conservative" :
        (g_memory_state.config.strategy == MEMORY_STRATEGY_BALANCED) ? "Balanced" : "Aggressive",

        // Pressure level
        (g_memory_state.stats.pressure_level == MEMORY_PRESSURE_NONE) ? "None" :
        (g_memory_state.stats.pressure_level == MEMORY_PRESSURE_LOW) ? "Low" :
        (g_memory_state.stats.pressure_level == MEMORY_PRESSURE_MEDIUM) ? "Medium" :
        (g_memory_state.stats.pressure_level == MEMORY_PRESSURE_HIGH) ? "High" : "Critical",

        g_memory_state.stats.memory_efficiency,

        // System memory
        g_memory_state.stats.total_memory_mb,
        g_memory_state.stats.used_memory_mb,
        (g_memory_state.stats.total_memory_mb > 0) ?
            ((float)g_memory_state.stats.used_memory_mb / g_memory_state.stats.total_memory_mb * 100.0f) : 0.0f,
        g_memory_state.stats.available_memory_mb,
        g_memory_state.stats.free_memory_mb,

        // LibEtude memory
        g_memory_state.stats.libetude_memory_mb,
        g_memory_state.stats.model_memory_mb,
        g_memory_state.stats.tensor_memory_mb,
        g_memory_state.stats.audio_buffer_memory_mb,

        // Memory pool
        g_memory_state.stats.pool_allocated_mb,
        g_memory_state.stats.pool_free_mb,
        g_memory_state.stats.pool_fragmentation * 100.0f,

        // Compression
        g_memory_state.compression_enabled ? "Yes" : "No",
        (g_memory_state.compression_type == MEMORY_COMPRESSION_NONE) ? "None" :
        (g_memory_state.compression_type == MEMORY_COMPRESSION_LZ4) ? "LZ4" :
        (g_memory_state.compression_type == MEMORY_COMPRESSION_ZSTD) ? "ZSTD" : "Custom",
        g_memory_state.stats.compressed_memory_mb,
        g_memory_state.stats.uncompressed_memory_mb,
        g_memory_state.stats.compression_ratio,

        // Cache
        g_memory_state.stats.cache_hits,
        g_memory_state.stats.cache_misses,
        g_memory_state.stats.cache_hit_ratio * 100.0f,

        // GC
        g_memory_state.stats.gc_count,
        g_memory_state.stats.total_gc_time_ms / 1000.0f,
        g_memory_state.auto_gc_enabled ? "Enabled" : "Disabled",

        // Thresholds
        g_memory_state.config.warning_threshold_mb,
        g_memory_state.config.critical_threshold_mb
    );

    pthread_mutex_unlock(&g_memory_state.mutex);

    return report;
}

int memory_reset_usage_history() {
    if (!g_memory_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_memory_state.mutex);
    memset(g_memory_state.memory_history, 0, sizeof(g_memory_state.memory_history));
    g_memory_state.history_index = 0;
    g_memory_state.gc_count = 0;
    g_memory_state.total_gc_time_ms = 0;
    g_memory_state.cache_hits = 0;
    g_memory_state.cache_misses = 0;
    g_memory_state.start_time_ms = get_current_time_ms();
    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

int memory_get_block_info(MemoryBlockInfo* blocks, int max_blocks, int* actual_count) {
    if (!blocks || !actual_count || max_blocks <= 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_memory_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_memory_state.mutex);

    int count = (g_memory_state.block_count < max_blocks) ?
                g_memory_state.block_count : max_blocks;

    memcpy(blocks, g_memory_state.blocks, count * sizeof(MemoryBlockInfo));
    *actual_count = count;

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static void* memory_monitoring_thread(void* arg) {
    (void)arg;

    while (g_memory_state.monitoring_active) {
        memory_update_usage_stats();

        pthread_mutex_lock(&g_memory_state.mutex);
        int interval_ms = g_memory_state.monitoring_interval_ms;
        pthread_mutex_unlock(&g_memory_state.mutex);

        usleep(interval_ms * 1000);
    }

    return NULL;
}

static void* memory_gc_thread(void* arg) {
    (void)arg;

    while (g_memory_state.gc_active) {
        pthread_mutex_lock(&g_memory_state.mutex);
        bool auto_gc_enabled = g_memory_state.auto_gc_enabled;
        int interval_ms = g_memory_state.gc_interval_ms;
        float threshold = g_memory_state.gc_threshold;
        float current_usage = (g_memory_state.stats.total_memory_mb > 0) ?
            ((float)g_memory_state.stats.used_memory_mb / g_memory_state.stats.total_memory_mb) : 0.0f;
        pthread_mutex_unlock(&g_memory_state.mutex);

        if (auto_gc_enabled && current_usage >= threshold) {
            perform_garbage_collection();
        }

        usleep(interval_ms * 1000);
    }

    return NULL;
}

static int update_system_memory_info() {
    pthread_mutex_lock(&g_memory_state.mutex);

#ifdef ANDROID_PLATFORM
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        g_memory_state.stats.total_memory_mb = si.totalram / (1024 * 1024);
        g_memory_state.stats.free_memory_mb = si.freeram / (1024 * 1024);
        g_memory_state.stats.available_memory_mb = (si.freeram + si.bufferram) / (1024 * 1024);
        g_memory_state.stats.used_memory_mb = g_memory_state.stats.total_memory_mb - g_memory_state.stats.free_memory_mb;
    }

#elif defined(IOS_PLATFORM)
    // iOS 메모리 정보
    size_t size = sizeof(uint64_t);
    uint64_t memory_bytes;
    if (sysctlbyname("hw.memsize", &memory_bytes, &size, NULL, 0) == 0) {
        g_memory_state.stats.total_memory_mb = memory_bytes / (1024 * 1024);
    }

    // 사용 중인 메모리 정보
    struct mach_task_basic_info info;
    mach_msg_type_number_t info_count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &info_count) == KERN_SUCCESS) {
        g_memory_state.stats.used_memory_mb = info.resident_size / (1024 * 1024);
        g_memory_state.stats.available_memory_mb = g_memory_state.stats.total_memory_mb - g_memory_state.stats.used_memory_mb;
        g_memory_state.stats.free_memory_mb = g_memory_state.stats.available_memory_mb;
    }

#else
    // 기본 구현 (시뮬레이션)
    g_memory_state.stats.total_memory_mb = 2048;  // 2GB
    g_memory_state.stats.used_memory_mb = 1024;   // 1GB
    g_memory_state.stats.available_memory_mb = 1024;  // 1GB
    g_memory_state.stats.free_memory_mb = 512;    // 512MB
#endif

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

static int update_libetude_memory_info() {
    pthread_mutex_lock(&g_memory_state.mutex);

    // LibEtude 메모리 사용량 추정 (실제로는 메모리 추적 시스템 필요)
    g_memory_state.stats.libetude_memory_mb = 128;      // 128MB
    g_memory_state.stats.model_memory_mb = 64;          // 64MB
    g_memory_state.stats.tensor_memory_mb = 32;         // 32MB
    g_memory_state.stats.audio_buffer_memory_mb = 16;   // 16MB

    // 메모리 풀 통계
    g_memory_state.stats.pool_allocated_mb = 48;        // 48MB
    g_memory_state.stats.pool_free_mb = 16;             // 16MB
    g_memory_state.stats.pool_fragmentation = 0.1f;    // 10%

    // 압축 통계
    if (g_memory_state.compression_enabled) {
        g_memory_state.stats.compressed_memory_mb = 32;     // 32MB
        g_memory_state.stats.uncompressed_memory_mb = 48;   // 48MB
        g_memory_state.stats.compression_ratio = 0.67f;    // 67%
    } else {
        g_memory_state.stats.compressed_memory_mb = 0;
        g_memory_state.stats.uncompressed_memory_mb = 0;
        g_memory_state.stats.compression_ratio = 1.0f;
    }

    pthread_mutex_unlock(&g_memory_state.mutex);

    return LIBETUDE_SUCCESS;
}

static size_t perform_garbage_collection() {
    // 가비지 컬렉션 수행 (실제로는 복잡한 구현 필요)
    size_t freed_mb = 0;

    pthread_mutex_lock(&g_memory_state.mutex);

    // 참조되지 않는 메모리 블록 해제
    for (int i = 0; i < g_memory_state.block_count; i++) {
        MemoryBlockInfo* block = &g_memory_state.blocks[i];

        if (block->reference_count == 0) {
            if (block->address) {
                free(block->address);
                freed_mb += block->size / (1024 * 1024);

                // 블록 정보 제거
                memmove(&g_memory_state.blocks[i], &g_memory_state.blocks[i + 1],
                       (g_memory_state.block_count - i - 1) * sizeof(MemoryBlockInfo));
                g_memory_state.block_count--;
                i--;
            }
        }
    }

    pthread_mutex_unlock(&g_memory_state.mutex);

    return freed_mb;
}

static int compress_memory_block(const void* data, size_t size, void** compressed_data, size_t* compressed_size) {
    // 간단한 압축 시뮬레이션 (실제로는 LZ4, ZSTD 등 사용)
    *compressed_size = size * 0.7f;  // 30% 압축률 가정
    *compressed_data = malloc(*compressed_size);

    if (!*compressed_data) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    // 실제로는 압축 알고리즘 적용
    memcpy(*compressed_data, data, (*compressed_size < size) ? *compressed_size : size);

    return LIBETUDE_SUCCESS;
}

static int decompress_memory_block(const void* compressed_data, size_t compressed_size, void** data, size_t* size) {
    // 간단한 압축 해제 시뮬레이션
    *size = compressed_size / 0.7f;  // 압축률 역산
    *data = malloc(*size);

    if (!*data) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    // 실제로는 압축 해제 알고리즘 적용
    memcpy(*data, compressed_data, (compressed_size < *size) ? compressed_size : *size);

    return LIBETUDE_SUCCESS;
}

static int64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ============================================================================
// 플랫폼별 특화 함수들
// ============================================================================

#ifdef ANDROID_PLATFORM
int memory_android_handle_trim(void* engine, int trim_level) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // Android trim level에 따른 메모리 압박 레벨 매핑
    MemoryPressureLevel pressure_level;

    switch (trim_level) {
        case 80:  // TRIM_MEMORY_COMPLETE
            pressure_level = MEMORY_PRESSURE_CRITICAL;
            break;
        case 60:  // TRIM_MEMORY_MODERATE
            pressure_level = MEMORY_PRESSURE_HIGH;
            break;
        case 40:  // TRIM_MEMORY_BACKGROUND
            pressure_level = MEMORY_PRESSURE_MEDIUM;
            break;
        case 20:  // TRIM_MEMORY_UI_HIDDEN
            pressure_level = MEMORY_PRESSURE_LOW;
            break;
        default:
            pressure_level = MEMORY_PRESSURE_NONE;
            break;
    }

    return memory_handle_pressure(engine, pressure_level);
}

int memory_android_optimize_for_lmk(void* engine) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // Android Low Memory Killer 대응 최적화
    // - 메모리 사용량 최소화
    // - 중요하지 않은 데이터 해제
    // - 압축 활성화

    memory_enable_compression(MEMORY_COMPRESSION_LZ4, 6);
    memory_free_memory(engine, 64);  // 64MB 해제
    memory_garbage_collect(engine);

    return LIBETUDE_SUCCESS;
}
#endif

#ifdef IOS_PLATFORM
int memory_ios_handle_memory_warning(void* engine, int warning_level) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // iOS 메모리 경고 레벨에 따른 처리
    MemoryPressureLevel pressure_level;

    switch (warning_level) {
        case 2:  // Critical memory warning
            pressure_level = MEMORY_PRESSURE_CRITICAL;
            break;
        case 1:  // Memory warning
            pressure_level = MEMORY_PRESSURE_HIGH;
            break;
        default:
            pressure_level = MEMORY_PRESSURE_MEDIUM;
            break;
    }

    return memory_handle_pressure(engine, pressure_level);
}

int memory_ios_handle_memory_pressure_ended(void* engine) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 메모리 압박 종료 - 정상 모드로 복원
    return memory_handle_pressure(engine, MEMORY_PRESSURE_NONE);
}
#endif
// ============================================================================
// 인플레이스 메모리 컨텍스트 구현
// ============================================================================

/**
 * 인플레이스 메모리 컨텍스트를 생성합니다
 */
ETInPlaceContext* et_create_inplace_context(size_t buffer_size, size_t alignment, bool initialize) {
    if (buffer_size == 0 || alignment == 0) {
        return NULL;
    }

    ETInPlaceContext* context = (ETInPlaceContext*)malloc(sizeof(ETInPlaceContext));
    if (!context) {
        return NULL;
    }

    // 정렬된 메모리 할당
#ifdef _WIN32
    context->buffer = _aligned_malloc(buffer_size, alignment);
#else
    context->buffer = aligned_alloc(alignment, buffer_size);
#endif
    if (!context->buffer) {
        free(context);
        return NULL;
    }

    context->buffer_size = buffer_size;
    context->alignment = alignment;
    context->is_initialized = initialize;
    context->used_size = 0;
    context->current_ptr = context->buffer;

    if (initialize) {
        memset(context->buffer, 0, buffer_size);
    }

    return context;
}

/**
 * 인플레이스 메모리 컨텍스트를 해제합니다
 */
int et_destroy_inplace_context(ETInPlaceContext* context) {
    if (!context) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (context->buffer) {
#ifdef _WIN32
        _aligned_free(context->buffer);
#else
        free(context->buffer);
#endif
    }

    free(context);
    return LIBETUDE_SUCCESS;
}

/**
 * 인플레이스 컨텍스트에서 메모리를 할당합니다
 */
void* et_inplace_alloc(ETInPlaceContext* context, size_t size) {
    if (!context || size == 0) {
        return NULL;
    }

    // 정렬 조정
    size_t aligned_size = (size + context->alignment - 1) & ~(context->alignment - 1);

    if (context->used_size + aligned_size > context->buffer_size) {
        return NULL; // 공간 부족
    }

    void* ptr = context->current_ptr;
    context->current_ptr = (char*)context->current_ptr + aligned_size;
    context->used_size += aligned_size;

    return ptr;
}

/**
 * 인플레이스 컨텍스트를 리셋합니다
 */
int et_inplace_reset(ETInPlaceContext* context) {
    if (!context) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    context->used_size = 0;
    context->current_ptr = context->buffer;

    if (context->is_initialized) {
        memset(context->buffer, 0, context->buffer_size);
    }

    return LIBETUDE_SUCCESS;
}