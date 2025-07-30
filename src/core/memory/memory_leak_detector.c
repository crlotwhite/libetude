#include "libetude/memory_optimization.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// 메모리 누수 추적을 위한 구조체
typedef struct MemoryAllocation {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* function;
    time_t timestamp;
    struct MemoryAllocation* next;
} MemoryAllocation;

// 메모리 블록 헤더 (단편화 추적용)
typedef struct MemoryBlock {
    size_t size;
    bool is_free;
    struct MemoryBlock* next;
    struct MemoryBlock* prev;
} MemoryBlock;

// 전역 메모리 누수 추적 상태
static struct {
    MemoryAllocation* allocation_list;
    size_t total_allocations;
    size_t total_allocated_bytes;
    size_t peak_allocated_bytes;
    pthread_mutex_t mutex;
    bool tracking_enabled;
} g_leak_detector = {0};

// 메모리 풀 관리
static struct {
    MemoryBlock* pool_head;
    size_t pool_total_size;
    size_t pool_used_size;
    pthread_mutex_t mutex;
} g_memory_pool = {0};

// 메모리 누수 감지기 초기화
int memory_leak_detector_init() {
    if (pthread_mutex_init(&g_leak_detector.mutex, NULL) != 0) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    if (pthread_mutex_init(&g_memory_pool.mutex, NULL) != 0) {
        pthread_mutex_destroy(&g_leak_detector.mutex);
        return LIBETUDE_ERROR_RUNTIME;
    }

    g_leak_detector.tracking_enabled = true;

    printf("메모리 누수 감지기가 초기화되었습니다.\n");
    return LIBETUDE_SUCCESS;
}

// 메모리 누수 감지기 정리
int memory_leak_detector_cleanup() {
    // 메모리 누수 검사
    memory_check_leaks();

    // 할당 목록 정리
    pthread_mutex_lock(&g_leak_detector.mutex);
    MemoryAllocation* current = g_leak_detector.allocation_list;
    while (current) {
        MemoryAllocation* next = current->next;
        free(current);
        current = next;
    }
    g_leak_detector.allocation_list = NULL;
    pthread_mutex_unlock(&g_leak_detector.mutex);

    pthread_mutex_destroy(&g_leak_detector.mutex);
    pthread_mutex_destroy(&g_memory_pool.mutex);

    printf("메모리 누수 감지기가 정리되었습니다.\n");
    return LIBETUDE_SUCCESS;
}

// 메모리 할당 추적 (디버그용)
void* memory_tracked_malloc(size_t size, const char* file, int line, const char* function) {
    void* ptr = malloc(size);
    if (!ptr) {
        return NULL;
    }

    if (!g_leak_detector.tracking_enabled) {
        return ptr;
    }

    pthread_mutex_lock(&g_leak_detector.mutex);

    // 할당 정보 기록
    MemoryAllocation* alloc = malloc(sizeof(MemoryAllocation));
    if (alloc) {
        alloc->ptr = ptr;
        alloc->size = size;
        alloc->file = file;
        alloc->line = line;
        alloc->function = function;
        alloc->timestamp = time(NULL);
        alloc->next = g_leak_detector.allocation_list;
        g_leak_detector.allocation_list = alloc;

        g_leak_detector.total_allocations++;
        g_leak_detector.total_allocated_bytes += size;

        if (g_leak_detector.total_allocated_bytes > g_leak_detector.peak_allocated_bytes) {
            g_leak_detector.peak_allocated_bytes = g_leak_detector.total_allocated_bytes;
        }
    }

    pthread_mutex_unlock(&g_leak_detector.mutex);

    return ptr;
}

// 메모리 해제 추적 (디버그용)
void memory_tracked_free(void* ptr, const char* file, int line, const char* function) {
    if (!ptr) {
        return;
    }

    if (g_leak_detector.tracking_enabled) {
        pthread_mutex_lock(&g_leak_detector.mutex);

        // 할당 목록에서 제거
        MemoryAllocation* current = g_leak_detector.allocation_list;
        MemoryAllocation* prev = NULL;

        while (current) {
            if (current->ptr == ptr) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    g_leak_detector.allocation_list = current->next;
                }

                g_leak_detector.total_allocated_bytes -= current->size;
                free(current);
                break;
            }
            prev = current;
            current = current->next;
        }

        pthread_mutex_unlock(&g_leak_detector.mutex);
    }

    free(ptr);
}

// 메모리 누수 검사
int memory_check_leaks() {
    pthread_mutex_lock(&g_leak_detector.mutex);

    if (!g_leak_detector.allocation_list) {
        printf("메모리 누수가 발견되지 않았습니다.\n");
        pthread_mutex_unlock(&g_leak_detector.mutex);
        return LIBETUDE_SUCCESS;
    }

    printf("=== 메모리 누수 검사 결과 ===\n");
    printf("총 %zu개의 메모리 누수 발견 (%zu bytes)\n",
           g_leak_detector.total_allocations, g_leak_detector.total_allocated_bytes);
    printf("피크 메모리 사용량: %zu bytes\n", g_leak_detector.peak_allocated_bytes);

    MemoryAllocation* current = g_leak_detector.allocation_list;
    int leak_count = 0;

    while (current && leak_count < 10) { // 최대 10개만 출력
        printf("누수 #%d:\n", leak_count + 1);
        printf("  주소: %p\n", current->ptr);
        printf("  크기: %zu bytes\n", current->size);
        printf("  위치: %s:%d (%s)\n", current->file, current->line, current->function);
        printf("  시간: %s", ctime(&current->timestamp));
        printf("\n");

        current = current->next;
        leak_count++;
    }

    if (current) {
        printf("... 그리고 %zu개의 추가 누수\n", g_leak_detector.total_allocations - leak_count);
    }

    pthread_mutex_unlock(&g_leak_detector.mutex);

    return LIBETUDE_ERROR_RUNTIME;
}

// 메모리 사용량 통계
int memory_get_leak_stats(size_t* total_allocs, size_t* total_bytes, size_t* peak_bytes) {
    if (!total_allocs || !total_bytes || !peak_bytes) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_leak_detector.mutex);
    *total_allocs = g_leak_detector.total_allocations;
    *total_bytes = g_leak_detector.total_allocated_bytes;
    *peak_bytes = g_leak_detector.peak_allocated_bytes;
    pthread_mutex_unlock(&g_leak_detector.mutex);

    return LIBETUDE_SUCCESS;
}

// 메모리 단편화 분석
float memory_analyze_fragmentation() {
    pthread_mutex_lock(&g_memory_pool.mutex);

    if (!g_memory_pool.pool_head) {
        pthread_mutex_unlock(&g_memory_pool.mutex);
        return 0.0f;
    }

    size_t free_blocks = 0;
    size_t total_free_size = 0;
    size_t largest_free_block = 0;

    MemoryBlock* current = g_memory_pool.pool_head;
    while (current) {
        if (current->is_free) {
            free_blocks++;
            total_free_size += current->size;
            if (current->size > largest_free_block) {
                largest_free_block = current->size;
            }
        }
        current = current->next;
    }

    float fragmentation = 0.0f;
    if (total_free_size > 0) {
        // 단편화 비율 = 1 - (최대 자유 블록 / 총 자유 메모리)
        fragmentation = 1.0f - ((float)largest_free_block / total_free_size);
    }

    printf("메모리 단편화 분석:\n");
    printf("  자유 블록 수: %zu\n", free_blocks);
    printf("  총 자유 메모리: %zu bytes\n", total_free_size);
    printf("  최대 자유 블록: %zu bytes\n", largest_free_block);
    printf("  단편화 비율: %.2f%%\n", fragmentation * 100.0f);

    pthread_mutex_unlock(&g_memory_pool.mutex);

    return fragmentation;
}

// 메모리 압축 (단편화 해소)
int memory_compact() {
    pthread_mutex_lock(&g_memory_pool.mutex);

    if (!g_memory_pool.pool_head) {
        pthread_mutex_unlock(&g_memory_pool.mutex);
        return LIBETUDE_SUCCESS;
    }

    printf("메모리 압축 시작...\n");

    // 인접한 자유 블록들을 병합
    MemoryBlock* current = g_memory_pool.pool_head;
    int merged_blocks = 0;

    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            // 두 자유 블록을 병합
            current->size += current->next->size + sizeof(MemoryBlock);
            MemoryBlock* next_block = current->next;
            current->next = next_block->next;
            if (next_block->next) {
                next_block->next->prev = current;
            }
            free(next_block);
            merged_blocks++;
        } else {
            current = current->next;
        }
    }

    printf("메모리 압축 완료: %d개 블록 병합\n", merged_blocks);

    pthread_mutex_unlock(&g_memory_pool.mutex);

    return LIBETUDE_SUCCESS;
}

// 메모리 사용 패턴 분석
typedef struct {
    size_t small_allocs;    // < 1KB
    size_t medium_allocs;   // 1KB - 1MB
    size_t large_allocs;    // > 1MB
    size_t total_allocs;
    double avg_alloc_size;
    double alloc_frequency; // 초당 할당 횟수
} MemoryUsagePattern;

int memory_analyze_usage_pattern(MemoryUsagePattern* pattern) {
    if (!pattern) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    memset(pattern, 0, sizeof(MemoryUsagePattern));

    pthread_mutex_lock(&g_leak_detector.mutex);

    MemoryAllocation* current = g_leak_detector.allocation_list;
    size_t total_size = 0;
    time_t earliest_time = time(NULL);
    time_t latest_time = 0;

    while (current) {
        pattern->total_allocs++;
        total_size += current->size;

        if (current->size < 1024) {
            pattern->small_allocs++;
        } else if (current->size < 1024 * 1024) {
            pattern->medium_allocs++;
        } else {
            pattern->large_allocs++;
        }

        if (current->timestamp < earliest_time) {
            earliest_time = current->timestamp;
        }
        if (current->timestamp > latest_time) {
            latest_time = current->timestamp;
        }

        current = current->next;
    }

    if (pattern->total_allocs > 0) {
        pattern->avg_alloc_size = (double)total_size / pattern->total_allocs;

        time_t duration = latest_time - earliest_time;
        if (duration > 0) {
            pattern->alloc_frequency = (double)pattern->total_allocs / duration;
        }
    }

    pthread_mutex_unlock(&g_leak_detector.mutex);

    printf("메모리 사용 패턴 분석:\n");
    printf("  소형 할당 (< 1KB): %zu개\n", pattern->small_allocs);
    printf("  중형 할당 (1KB-1MB): %zu개\n", pattern->medium_allocs);
    printf("  대형 할당 (> 1MB): %zu개\n", pattern->large_allocs);
    printf("  평균 할당 크기: %.1f bytes\n", pattern->avg_alloc_size);
    printf("  할당 빈도: %.2f 회/초\n", pattern->alloc_frequency);

    return LIBETUDE_SUCCESS;
}

// 메모리 최적화 제안 생성
typedef struct {
    const char* suggestion;
    float expected_improvement;
    int priority; // 1-5 (높음-낮음)
} MemoryOptimizationSuggestion;

int memory_generate_optimization_suggestions(MemoryOptimizationSuggestion** suggestions, int* count) {
    if (!suggestions || !count) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    static MemoryOptimizationSuggestion opt_suggestions[10];
    int suggestion_count = 0;

    // 단편화 분석
    float fragmentation = memory_analyze_fragmentation();
    if (fragmentation > 0.3f) {
        opt_suggestions[suggestion_count++] = (MemoryOptimizationSuggestion){
            .suggestion = "메모리 단편화가 높습니다. 정기적인 메모리 압축을 권장합니다.",
            .expected_improvement = fragmentation * 0.5f,
            .priority = 2
        };
    }

    // 사용 패턴 분석
    MemoryUsagePattern pattern;
    memory_analyze_usage_pattern(&pattern);

    if (pattern.small_allocs > pattern.total_allocs * 0.7f) {
        opt_suggestions[suggestion_count++] = (MemoryOptimizationSuggestion){
            .suggestion = "소형 할당이 많습니다. 메모리 풀 사용을 권장합니다.",
            .expected_improvement = 0.3f,
            .priority = 1
        };
    }

    if (pattern.large_allocs > 0) {
        opt_suggestions[suggestion_count++] = (MemoryOptimizationSuggestion){
            .suggestion = "대형 할당이 있습니다. 메모리 압축을 고려하세요.",
            .expected_improvement = 0.4f,
            .priority = 2
        };
    }

    // 메모리 누수 검사
    if (g_leak_detector.total_allocations > 0) {
        opt_suggestions[suggestion_count++] = (MemoryOptimizationSuggestion){
            .suggestion = "메모리 누수가 감지되었습니다. 메모리 해제를 확인하세요.",
            .expected_improvement = 0.8f,
            .priority = 1
        };
    }

    // 피크 메모리 사용량 분석
    if (g_leak_detector.peak_allocated_bytes > g_leak_detector.total_allocated_bytes * 2) {
        opt_suggestions[suggestion_count++] = (MemoryOptimizationSuggestion){
            .suggestion = "피크 메모리 사용량이 높습니다. 메모리 사용량을 평준화하세요.",
            .expected_improvement = 0.25f,
            .priority = 3
        };
    }

    *suggestions = opt_suggestions;
    *count = suggestion_count;

    return LIBETUDE_SUCCESS;
}

// 메모리 풀 생성
void* memory_create_pool(size_t size_mb, size_t alignment) {
    pthread_mutex_lock(&g_memory_pool.mutex);

    size_t pool_size = size_mb * 1024 * 1024;

    // 정렬된 메모리 할당
    void* pool_memory = aligned_alloc(alignment, pool_size);
    if (!pool_memory) {
        pthread_mutex_unlock(&g_memory_pool.mutex);
        return NULL;
    }

    // 첫 번째 블록 생성
    MemoryBlock* first_block = (MemoryBlock*)pool_memory;
    first_block->size = pool_size - sizeof(MemoryBlock);
    first_block->is_free = true;
    first_block->next = NULL;
    first_block->prev = NULL;

    g_memory_pool.pool_head = first_block;
    g_memory_pool.pool_total_size = pool_size;
    g_memory_pool.pool_used_size = sizeof(MemoryBlock);

    pthread_mutex_unlock(&g_memory_pool.mutex);

    printf("메모리 풀 생성: %zu MB\n", size_mb);
    return pool_memory;
}

// 메모리 풀에서 할당
void* memory_pool_alloc(size_t size) {
    pthread_mutex_lock(&g_memory_pool.mutex);

    if (!g_memory_pool.pool_head) {
        pthread_mutex_unlock(&g_memory_pool.mutex);
        return NULL;
    }

    // 적절한 크기의 자유 블록 찾기
    MemoryBlock* current = g_memory_pool.pool_head;
    while (current) {
        if (current->is_free && current->size >= size) {
            // 블록 분할
            if (current->size > size + sizeof(MemoryBlock)) {
                MemoryBlock* new_block = (MemoryBlock*)((char*)current + sizeof(MemoryBlock) + size);
                new_block->size = current->size - size - sizeof(MemoryBlock);
                new_block->is_free = true;
                new_block->next = current->next;
                new_block->prev = current;

                if (current->next) {
                    current->next->prev = new_block;
                }
                current->next = new_block;
                current->size = size;
            }

            current->is_free = false;
            g_memory_pool.pool_used_size += current->size;

            pthread_mutex_unlock(&g_memory_pool.mutex);
            return (char*)current + sizeof(MemoryBlock);
        }
        current = current->next;
    }

    pthread_mutex_unlock(&g_memory_pool.mutex);
    return NULL; // 할당 실패
}

// 메모리 풀에 반환
int memory_pool_free(void* ptr) {
    if (!ptr) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_memory_pool.mutex);

    // 블록 헤더 찾기
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    block->is_free = true;
    g_memory_pool.pool_used_size -= block->size;

    // 인접한 자유 블록과 병합
    if (block->prev && block->prev->is_free) {
        block->prev->size += block->size + sizeof(MemoryBlock);
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        block = block->prev;
    }

    if (block->next && block->next->is_free) {
        block->size += block->next->size + sizeof(MemoryBlock);
        MemoryBlock* next_block = block->next;
        block->next = next_block->next;
        if (next_block->next) {
            next_block->next->prev = block;
        }
    }

    pthread_mutex_unlock(&g_memory_pool.mutex);

    return LIBETUDE_SUCCESS;
}

// 메모리 풀 통계
int memory_pool_get_stats(size_t* total_mb, size_t* used_mb, size_t* free_mb, float* fragmentation) {
    if (!total_mb || !used_mb || !free_mb || !fragmentation) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_memory_pool.mutex);

    *total_mb = g_memory_pool.pool_total_size / (1024 * 1024);
    *used_mb = g_memory_pool.pool_used_size / (1024 * 1024);
    *free_mb = (*total_mb) - (*used_mb);
    *fragmentation = memory_analyze_fragmentation();

    pthread_mutex_unlock(&g_memory_pool.mutex);

    return LIBETUDE_SUCCESS;
}

// 메모리 추적 활성화/비활성화
int memory_set_tracking_enabled(bool enabled) {
    pthread_mutex_lock(&g_leak_detector.mutex);
    g_leak_detector.tracking_enabled = enabled;
    pthread_mutex_unlock(&g_leak_detector.mutex);

    printf("메모리 추적 %s\n", enabled ? "활성화" : "비활성화");
    return LIBETUDE_SUCCESS;
}

// 매크로 정의 (디버그 빌드에서만 활성화)
#ifdef DEBUG
#define TRACKED_MALLOC(size) memory_tracked_malloc(size, __FILE__, __LINE__, __FUNCTION__)
#define TRACKED_FREE(ptr) memory_tracked_free(ptr, __FILE__, __LINE__, __FUNCTION__)
#else
#define TRACKED_MALLOC(size) malloc(size)
#define TRACKED_FREE(ptr) free(ptr)
#endif