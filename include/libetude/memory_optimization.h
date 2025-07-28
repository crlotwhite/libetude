/**
 * @file memory_optimization.h
 * @brief 모바일 메모리 최적화 시스템
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 모바일 환경에서의 메모리 사용량 최소화와 효율적인 메모리 관리를 위한 시스템입니다.
 */

#ifndef LIBETUDE_MEMORY_OPTIMIZATION_H
#define LIBETUDE_MEMORY_OPTIMIZATION_H

#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 메모리 압박 레벨
 */
typedef enum {
    MEMORY_PRESSURE_NONE = 0,       ///< 압박 없음
    MEMORY_PRESSURE_LOW = 1,        ///< 낮은 압박
    MEMORY_PRESSURE_MEDIUM = 2,     ///< 중간 압박
    MEMORY_PRESSURE_HIGH = 3,       ///< 높은 압박
    MEMORY_PRESSURE_CRITICAL = 4    ///< 임계 압박
} MemoryPressureLevel;

/**
 * 메모리 최적화 전략
 */
typedef enum {
    MEMORY_STRATEGY_NONE = 0,           ///< 최적화 없음
    MEMORY_STRATEGY_CONSERVATIVE = 1,   ///< 보수적 최적화
    MEMORY_STRATEGY_BALANCED = 2,       ///< 균형 최적화
    MEMORY_STRATEGY_AGGRESSIVE = 3      ///< 적극적 최적화
} MemoryOptimizationStrategy;

/**
 * 메모리 압축 타입
 */
typedef enum {
    MEMORY_COMPRESSION_NONE = 0,    ///< 압축 없음
    MEMORY_COMPRESSION_LZ4 = 1,     ///< LZ4 압축
    MEMORY_COMPRESSION_ZSTD = 2,    ///< ZSTD 압축
    MEMORY_COMPRESSION_CUSTOM = 3   ///< 커스텀 압축
} MemoryCompressionType;

/**
 * 메모리 풀 타입
 */
typedef enum {
    MEMORY_POOL_FIXED = 0,      ///< 고정 크기 풀
    MEMORY_POOL_DYNAMIC = 1,    ///< 동적 크기 풀
    MEMORY_POOL_CIRCULAR = 2    ///< 순환 풀
} MemoryPoolType;

/**
 * 메모리 최적화 설정
 */
typedef struct {
    MemoryOptimizationStrategy strategy;    ///< 최적화 전략
    MemoryCompressionType compression_type; ///< 압축 타입

    // 메모리 제한 설정
    size_t max_memory_mb;                   ///< 최대 메모리 사용량 (MB)
    size_t warning_threshold_mb;            ///< 경고 임계값 (MB)
    size_t critical_threshold_mb;           ///< 임계 임계값 (MB)

    // 메모리 풀 설정
    MemoryPoolType pool_type;               ///< 메모리 풀 타입
    size_t pool_size_mb;                    ///< 메모리 풀 크기 (MB)
    size_t pool_alignment;                  ///< 메모리 정렬 크기

    // 압축 설정
    bool enable_compression;                ///< 압축 활성화
    float compression_threshold;            ///< 압축 임계값 (0.0-1.0)
    int compression_level;                  ///< 압축 레벨 (1-9)

    // 가비지 컬렉션 설정
    bool enable_gc;                         ///< 가비지 컬렉션 활성화
    int gc_interval_ms;                     ///< GC 간격 (ms)
    float gc_threshold;                     ///< GC 임계값 (0.0-1.0)

    // 스왑 설정
    bool enable_swap;                       ///< 스왑 활성화
    size_t swap_size_mb;                    ///< 스왑 크기 (MB)

    // 캐시 설정
    bool enable_cache_optimization;         ///< 캐시 최적화 활성화
    size_t l1_cache_size_kb;               ///< L1 캐시 크기 (KB)
    size_t l2_cache_size_kb;               ///< L2 캐시 크기 (KB)
} MemoryOptimizationConfig;

/**
 * 메모리 사용량 통계
 */
typedef struct {
    // 전체 메모리 정보
    size_t total_memory_mb;                 ///< 총 메모리 (MB)
    size_t available_memory_mb;             ///< 사용 가능한 메모리 (MB)
    size_t used_memory_mb;                  ///< 사용 중인 메모리 (MB)
    size_t free_memory_mb;                  ///< 여유 메모리 (MB)

    // LibEtude 메모리 사용량
    size_t libetude_memory_mb;              ///< LibEtude 메모리 사용량 (MB)
    size_t model_memory_mb;                 ///< 모델 메모리 사용량 (MB)
    size_t tensor_memory_mb;                ///< 텐서 메모리 사용량 (MB)
    size_t audio_buffer_memory_mb;          ///< 오디오 버퍼 메모리 (MB)

    // 메모리 풀 통계
    size_t pool_allocated_mb;               ///< 풀에서 할당된 메모리 (MB)
    size_t pool_free_mb;                    ///< 풀의 여유 메모리 (MB)
    float pool_fragmentation;               ///< 풀 단편화 비율 (0.0-1.0)

    // 압축 통계
    size_t compressed_memory_mb;            ///< 압축된 메모리 (MB)
    size_t uncompressed_memory_mb;          ///< 압축 해제된 메모리 (MB)
    float compression_ratio;                ///< 압축 비율

    // 성능 지표
    MemoryPressureLevel pressure_level;     ///< 메모리 압박 레벨
    float memory_efficiency;                ///< 메모리 효율성 (0.0-1.0)
    int gc_count;                          ///< GC 실행 횟수
    int64_t total_gc_time_ms;              ///< 총 GC 시간 (ms)

    // 캐시 통계
    int cache_hits;                        ///< 캐시 히트 수
    int cache_misses;                      ///< 캐시 미스 수
    float cache_hit_ratio;                 ///< 캐시 히트 비율
} MemoryUsageStats;

/**
 * 메모리 블록 정보
 */
typedef struct {
    void* address;                         ///< 메모리 주소
    size_t size;                           ///< 블록 크기
    bool is_compressed;                    ///< 압축 여부
    bool is_cached;                        ///< 캐시 여부
    int64_t last_access_time;              ///< 마지막 접근 시간
    int reference_count;                   ///< 참조 카운트
} MemoryBlockInfo;

/**
 * 메모리 이벤트 콜백 타입
 */
typedef void (*MemoryEventCallback)(MemoryPressureLevel old_level, MemoryPressureLevel new_level,
                                   const MemoryUsageStats* stats, void* user_data);

// ============================================================================
// 메모리 최적화 초기화 및 정리 함수들
// ============================================================================

/**
 * 메모리 최적화 시스템을 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_optimization_init();

/**
 * 메모리 최적화 시스템을 정리합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_optimization_cleanup();

/**
 * 메모리 최적화 설정을 적용합니다
 *
 * @param config 메모리 최적화 설정
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_set_optimization_config(const MemoryOptimizationConfig* config);

/**
 * 현재 메모리 최적화 설정을 가져옵니다
 *
 * @param config 설정을 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_get_optimization_config(MemoryOptimizationConfig* config);

// ============================================================================
// 메모리 사용량 모니터링 함수들
// ============================================================================

/**
 * 현재 메모리 사용량 통계를 가져옵니다
 *
 * @param stats 통계를 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_get_usage_stats(MemoryUsageStats* stats);

/**
 * 메모리 사용량을 업데이트합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_update_usage_stats();

/**
 * 메모리 압박 레벨을 결정합니다
 *
 * @param used_memory_mb 사용 중인 메모리 (MB)
 * @param total_memory_mb 총 메모리 (MB)
 * @param config 최적화 설정
 * @return 메모리 압박 레벨
 */
MemoryPressureLevel memory_determine_pressure_level(size_t used_memory_mb, size_t total_memory_mb,
                                                   const MemoryOptimizationConfig* config);

// ============================================================================
// 메모리 압박 처리 함수들
// ============================================================================

/**
 * 메모리 압박 상황을 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param pressure_level 압박 레벨
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_handle_pressure(void* engine, MemoryPressureLevel pressure_level);

/**
 * 메모리를 해제합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param target_mb 목표 해제 메모리 (MB)
 * @return 실제 해제된 메모리 (MB)
 */
size_t memory_free_memory(void* engine, size_t target_mb);

/**
 * 사용하지 않는 메모리를 정리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 정리된 메모리 크기 (MB)
 */
size_t memory_cleanup_unused(void* engine);

/**
 * 메모리 단편화를 해소합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_defragment();

// ============================================================================
// 메모리 압축 함수들
// ============================================================================

/**
 * 메모리 압축을 활성화합니다
 *
 * @param compression_type 압축 타입
 * @param compression_level 압축 레벨 (1-9)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_enable_compression(MemoryCompressionType compression_type, int compression_level);

/**
 * 메모리 압축을 비활성화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_disable_compression();

/**
 * 메모리 블록을 압축합니다
 *
 * @param data 압축할 데이터
 * @param size 데이터 크기
 * @param compressed_data 압축된 데이터를 저장할 포인터
 * @param compressed_size 압축된 크기를 저장할 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_compress_block(const void* data, size_t size, void** compressed_data, size_t* compressed_size);

/**
 * 압축된 메모리 블록을 해제합니다
 *
 * @param compressed_data 압축된 데이터
 * @param compressed_size 압축된 크기
 * @param data 해제된 데이터를 저장할 포인터
 * @param size 해제된 크기를 저장할 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_decompress_block(const void* compressed_data, size_t compressed_size, void** data, size_t* size);

// ============================================================================
// 메모리 풀 관리 함수들
// ============================================================================

/**
 * 메모리 풀을 생성합니다
 *
 * @param pool_type 풀 타입
 * @param size_mb 풀 크기 (MB)
 * @param alignment 정렬 크기
 * @return 메모리 풀 핸들
 */
void* memory_create_pool(MemoryPoolType pool_type, size_t size_mb, size_t alignment);

/**
 * 메모리 풀을 해제합니다
 *
 * @param pool 메모리 풀 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_destroy_pool(void* pool);

/**
 * 메모리 풀에서 메모리를 할당합니다
 *
 * @param pool 메모리 풀 핸들
 * @param size 할당할 크기
 * @return 할당된 메모리 포인터
 */
void* memory_pool_alloc(void* pool, size_t size);

/**
 * 메모리 풀에 메모리를 반환합니다
 *
 * @param pool 메모리 풀 핸들
 * @param ptr 반환할 메모리 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_pool_free(void* pool, void* ptr);

/**
 * 메모리 풀을 리셋합니다
 *
 * @param pool 메모리 풀 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_pool_reset(void* pool);

/**
 * 메모리 풀 통계를 가져옵니다
 *
 * @param pool 메모리 풀 핸들
 * @param allocated_mb 할당된 메모리를 저장할 포인터 (MB)
 * @param free_mb 여유 메모리를 저장할 포인터 (MB)
 * @param fragmentation 단편화 비율을 저장할 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_pool_get_stats(void* pool, size_t* allocated_mb, size_t* free_mb, float* fragmentation);

// ============================================================================
// 가비지 컬렉션 함수들
// ============================================================================

/**
 * 가비지 컬렉션을 수행합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 회수된 메모리 크기 (MB)
 */
size_t memory_garbage_collect(void* engine);

/**
 * 자동 가비지 컬렉션을 활성화합니다
 *
 * @param interval_ms GC 간격 (ms)
 * @param threshold GC 임계값 (0.0-1.0)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_enable_auto_gc(int interval_ms, float threshold);

/**
 * 자동 가비지 컬렉션을 비활성화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_disable_auto_gc();

// ============================================================================
// 캐시 최적화 함수들
// ============================================================================

/**
 * 캐시 최적화를 활성화합니다
 *
 * @param l1_cache_size_kb L1 캐시 크기 (KB)
 * @param l2_cache_size_kb L2 캐시 크기 (KB)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_enable_cache_optimization(size_t l1_cache_size_kb, size_t l2_cache_size_kb);

/**
 * 캐시를 플러시합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_flush_cache();

/**
 * 캐시 통계를 가져옵니다
 *
 * @param hits 캐시 히트 수를 저장할 포인터
 * @param misses 캐시 미스 수를 저장할 포인터
 * @param hit_ratio 히트 비율을 저장할 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_get_cache_stats(int* hits, int* misses, float* hit_ratio);

// ============================================================================
// 모니터링 및 이벤트 함수들
// ============================================================================

/**
 * 메모리 모니터링을 시작합니다
 *
 * @param callback 이벤트 콜백
 * @param user_data 사용자 데이터
 * @param interval_ms 모니터링 간격 (ms)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_start_monitoring(MemoryEventCallback callback, void* user_data, int interval_ms);

/**
 * 메모리 모니터링을 중지합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_stop_monitoring();

/**
 * 메모리 이벤트 콜백을 설정합니다
 *
 * @param callback 이벤트 콜백
 * @param user_data 사용자 데이터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_set_event_callback(MemoryEventCallback callback, void* user_data);

// ============================================================================
// 통계 및 리포트 함수들
// ============================================================================

/**
 * 메모리 최적화 리포트를 생성합니다
 *
 * @return 리포트 문자열 (호출자가 해제해야 함)
 */
char* memory_generate_optimization_report();

/**
 * 메모리 사용량 히스토리를 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_reset_usage_history();

/**
 * 메모리 블록 정보를 가져옵니다
 *
 * @param blocks 블록 정보 배열
 * @param max_blocks 최대 블록 수
 * @param actual_count 실제 블록 수를 저장할 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_get_block_info(MemoryBlockInfo* blocks, int max_blocks, int* actual_count);

// ============================================================================
// 플랫폼별 메모리 최적화 함수들
// ============================================================================

#ifdef ANDROID_PLATFORM
/**
 * Android 메모리 트림을 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param trim_level 트림 레벨
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_android_handle_trim(LibEtudeEngine* engine, int trim_level);

/**
 * Android 저메모리 킬러를 위한 최적화를 수행합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_android_optimize_for_lmk(LibEtudeEngine* engine);
#endif

#ifdef IOS_PLATFORM
/**
 * iOS 메모리 경고를 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param warning_level 경고 레벨
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_ios_handle_memory_warning(LibEtudeEngine* engine, int warning_level);

/**
 * iOS 메모리 압박 종료를 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int memory_ios_handle_memory_pressure_ended(LibEtudeEngine* engine);
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_MEMORY_OPTIMIZATION_H