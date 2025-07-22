/**
 * @file memory_optimization.h
 * @brief LibEtude 메모리 최적화 전략 헤더
 *
 * 인플레이스 연산, 메모리 재사용, 단편화 방지 등의 메모리 최적화 기능을 제공합니다.
 */

#ifndef LIBETUDE_MEMORY_OPTIMIZATION_H
#define LIBETUDE_MEMORY_OPTIMIZATION_H

#include "libetude/memory.h"
#include "libetude/types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 인플레이스 연산 지원
// =============================================================================

/**
 * @brief 인플레이스 연산 컨텍스트
 *
 * 메모리 복사 없이 동일한 메모리 위치에서 연산을 수행하기 위한 컨텍스트입니다.
 */
typedef struct {
    void* buffer;                   // 작업 버퍼
    size_t buffer_size;             // 버퍼 크기
    size_t alignment;               // 메모리 정렬
    bool is_external;               // 외부 버퍼 여부

    // 연산 추적
    size_t operation_count;         // 수행된 연산 수
    size_t bytes_saved;             // 절약된 메모리 바이트

    // 스레드 안전성
    pthread_mutex_t mutex;
    bool thread_safe;
} ETInPlaceContext;

/**
 * @brief 인플레이스 연산 컨텍스트 생성
 * @param buffer_size 작업 버퍼 크기
 * @param alignment 메모리 정렬 요구사항
 * @param thread_safe 스레드 안전성 활성화 여부
 * @return 생성된 컨텍스트 포인터, 실패시 NULL
 */
ETInPlaceContext* et_create_inplace_context(size_t buffer_size, size_t alignment, bool thread_safe);

/**
 * @brief 외부 버퍼를 사용한 인플레이스 컨텍스트 생성
 * @param buffer 외부 버퍼
 * @param buffer_size 버퍼 크기
 * @param alignment 메모리 정렬
 * @param thread_safe 스레드 안전성 활성화 여부
 * @return 생성된 컨텍스트 포인터, 실패시 NULL
 */
ETInPlaceContext* et_create_inplace_context_from_buffer(void* buffer, size_t buffer_size,
                                                       size_t alignment, bool thread_safe);

/**
 * @brief 인플레이스 메모리 복사 (겹치는 영역 안전)
 * @param ctx 인플레이스 컨텍스트
 * @param dest 목적지 주소
 * @param src 소스 주소
 * @param size 복사할 크기
 * @return 성공시 0, 실패시 음수
 */
int et_inplace_memcpy(ETInPlaceContext* ctx, void* dest, const void* src, size_t size);

/**
 * @brief 인플레이스 메모리 이동 (겹치는 영역 처리)
 * @param ctx 인플레이스 컨텍스트
 * @param dest 목적지 주소
 * @param src 소스 주소
 * @param size 이동할 크기
 * @return 성공시 0, 실패시 음수
 */
int et_inplace_memmove(ETInPlaceContext* ctx, void* dest, const void* src, size_t size);

/**
 * @brief 인플레이스 버퍼 스왑
 * @param ctx 인플레이스 컨텍스트
 * @param ptr1 첫 번째 버퍼
 * @param ptr2 두 번째 버퍼
 * @param size 스왑할 크기
 * @return 성공시 0, 실패시 음수
 */
int et_inplace_swap(ETInPlaceContext* ctx, void* ptr1, void* ptr2, size_t size);

/**
 * @brief 인플레이스 컨텍스트 소멸
 * @param ctx 인플레이스 컨텍스트
 */
void et_destroy_inplace_context(ETInPlaceContext* ctx);

// =============================================================================
// 메모리 재사용 메커니즘
// =============================================================================

/**
 * @brief 메모리 재사용 풀
 *
 * 임시 버퍼들을 재사용하여 동적 할당을 최소화하는 풀입니다.
 */
typedef struct ETMemoryReuseBucket ETMemoryReuseBucket;

struct ETMemoryReuseBucket {
    size_t size_class;              // 크기 클래스 (2의 거듭제곱)
    void** buffers;                 // 재사용 가능한 버퍼 배열
    size_t buffer_count;            // 현재 버퍼 수
    size_t max_buffers;             // 최대 버퍼 수
    size_t total_allocations;       // 총 할당 횟수
    size_t reuse_hits;              // 재사용 성공 횟수
    ETMemoryReuseBucket* next;      // 다음 버킷
};

typedef struct {
    ETMemoryReuseBucket* buckets;   // 크기별 버킷 리스트
    size_t min_size;                // 최소 관리 크기
    size_t max_size;                // 최대 관리 크기
    size_t total_memory;            // 총 관리 메모리
    size_t peak_memory;             // 최대 메모리 사용량

    // 통계
    size_t total_requests;          // 총 요청 수
    size_t reuse_hits;              // 재사용 성공 수
    size_t cache_misses;            // 캐시 미스 수

    // 정리 정책
    uint64_t last_cleanup_time;     // 마지막 정리 시간
    uint64_t cleanup_interval_ms;   // 정리 간격 (밀리초)
    size_t max_idle_time_ms;        // 최대 유휴 시간

    // 스레드 안전성
    pthread_mutex_t mutex;
    bool thread_safe;
} ETMemoryReusePool;

/**
 * @brief 메모리 재사용 풀 생성
 * @param min_size 최소 관리 크기
 * @param max_size 최대 관리 크기
 * @param max_buffers_per_class 크기 클래스당 최대 버퍼 수
 * @param thread_safe 스레드 안전성 활성화 여부
 * @return 생성된 재사용 풀 포인터, 실패시 NULL
 */
ETMemoryReusePool* et_create_reuse_pool(size_t min_size, size_t max_size,
                                       size_t max_buffers_per_class, bool thread_safe);

/**
 * @brief 재사용 풀에서 메모리 할당
 * @param pool 재사용 풀
 * @param size 요청 크기
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* et_reuse_alloc(ETMemoryReusePool* pool, size_t size);

/**
 * @brief 재사용 풀에 메모리 반환
 * @param pool 재사용 풀
 * @param ptr 반환할 메모리 포인터
 * @param size 메모리 크기
 */
void et_reuse_free(ETMemoryReusePool* pool, void* ptr, size_t size);

/**
 * @brief 재사용 풀 정리 (오래된 버퍼 해제)
 * @param pool 재사용 풀
 * @param force_cleanup 강제 정리 여부
 * @return 해제된 버퍼 수
 */
size_t et_cleanup_reuse_pool(ETMemoryReusePool* pool, bool force_cleanup);

/**
 * @brief 재사용 풀 통계 조회
 * @param pool 재사용 풀
 * @param total_requests 총 요청 수 (출력)
 * @param reuse_hits 재사용 성공 수 (출력)
 * @param hit_rate 재사용 성공률 (출력)
 */
void et_get_reuse_pool_stats(ETMemoryReusePool* pool, size_t* total_requests,
                            size_t* reuse_hits, float* hit_rate);

/**
 * @brief 재사용 풀 소멸
 * @param pool 재사용 풀
 */
void et_destroy_reuse_pool(ETMemoryReusePool* pool);

// =============================================================================
// 메모리 단편화 방지
// =============================================================================

/**
 * @brief 메모리 단편화 분석 결과
 */
typedef struct {
    size_t total_free_space;        // 총 자유 공간
    size_t largest_free_block;      // 최대 자유 블록 크기
    size_t num_free_blocks;         // 자유 블록 수
    float fragmentation_ratio;      // 단편화 비율 (0.0 ~ 1.0)
    float external_fragmentation;   // 외부 단편화 비율
    size_t wasted_space;            // 낭비된 공간
} ETFragmentationInfo;

/**
 * @brief 메모리 풀의 단편화 분석
 * @param pool 메모리 풀
 * @param frag_info 단편화 정보 (출력)
 * @return 성공시 0, 실패시 음수
 */
int et_analyze_fragmentation(ETMemoryPool* pool, ETFragmentationInfo* frag_info);

/**
 * @brief 메모리 풀 압축 (단편화 해소)
 * @param pool 메모리 풀
 * @param aggressive 적극적 압축 여부
 * @return 압축된 바이트 수
 */
size_t et_compact_memory_pool(ETMemoryPool* pool, bool aggressive);

/**
 * @brief 메모리 풀 최적화 (블록 재배치)
 * @param pool 메모리 풀
 * @return 최적화된 블록 수
 */
size_t et_optimize_memory_layout(ETMemoryPool* pool);

/**
 * @brief 메모리 할당 전략 설정
 */
typedef enum {
    ET_ALLOC_FIRST_FIT = 0,         // First-fit (빠름)
    ET_ALLOC_BEST_FIT = 1,          // Best-fit (공간 효율적)
    ET_ALLOC_WORST_FIT = 2,         // Worst-fit (단편화 방지)
    ET_ALLOC_NEXT_FIT = 3           // Next-fit (캐시 친화적)
} ETAllocationStrategy;

/**
 * @brief 메모리 풀의 할당 전략 설정
 * @param pool 메모리 풀
 * @param strategy 할당 전략
 * @return 성공시 0, 실패시 음수
 */
int et_set_allocation_strategy(ETMemoryPool* pool, ETAllocationStrategy strategy);

/**
 * @brief 메모리 풀의 자동 압축 설정
 * @param pool 메모리 풀
 * @param enable 자동 압축 활성화 여부
 * @param threshold 압축 임계값 (단편화 비율)
 * @return 성공시 0, 실패시 음수
 */
int et_set_auto_compaction(ETMemoryPool* pool, bool enable, float threshold);

// =============================================================================
// 스마트 메모리 관리
// =============================================================================

/**
 * @brief 스마트 메모리 매니저
 *
 * 메모리 사용 패턴을 학습하고 최적화하는 매니저입니다.
 */
typedef struct {
    ETMemoryPool* primary_pool;     // 주 메모리 풀
    ETMemoryReusePool* reuse_pool;  // 재사용 풀
    ETInPlaceContext* inplace_ctx;  // 인플레이스 컨텍스트

    // 사용 패턴 추적
    size_t* size_histogram;         // 크기별 할당 히스토그램
    size_t histogram_buckets;       // 히스토그램 버킷 수
    uint64_t* access_timestamps;    // 접근 시간 추적

    // 적응적 설정
    ETAllocationStrategy current_strategy;  // 현재 할당 전략
    float compaction_threshold;     // 압축 임계값
    bool auto_optimization;         // 자동 최적화 활성화

    // 성능 메트릭
    uint64_t total_allocations;     // 총 할당 수
    uint64_t total_frees;           // 총 해제 수
    uint64_t bytes_saved;           // 절약된 바이트
    uint64_t optimization_count;    // 최적화 수행 횟수

    // 스레드 안전성
    pthread_mutex_t mutex;
    bool thread_safe;
} ETSmartMemoryManager;

/**
 * @brief 스마트 메모리 매니저 생성
 * @param pool_size 주 메모리 풀 크기
 * @param reuse_pool_config 재사용 풀 설정
 * @param inplace_buffer_size 인플레이스 버퍼 크기
 * @param thread_safe 스레드 안전성 활성화 여부
 * @return 생성된 매니저 포인터, 실패시 NULL
 */
ETSmartMemoryManager* et_create_smart_memory_manager(size_t pool_size,
                                                    size_t reuse_pool_config,
                                                    size_t inplace_buffer_size,
                                                    bool thread_safe);

/**
 * @brief 스마트 할당 (최적 전략 자동 선택)
 * @param manager 스마트 메모리 매니저
 * @param size 할당 크기
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* et_smart_alloc(ETSmartMemoryManager* manager, size_t size);

/**
 * @brief 스마트 해제 (재사용 풀 고려)
 * @param manager 스마트 메모리 매니저
 * @param ptr 해제할 메모리 포인터
 * @param size 메모리 크기
 */
void et_smart_free(ETSmartMemoryManager* manager, void* ptr, size_t size);

/**
 * @brief 메모리 사용 패턴 분석 및 최적화
 * @param manager 스마트 메모리 매니저
 * @return 수행된 최적화 수
 */
size_t et_optimize_memory_usage(ETSmartMemoryManager* manager);

/**
 * @brief 스마트 메모리 매니저 통계 조회
 * @param manager 스마트 메모리 매니저
 * @param total_allocations 총 할당 수 (출력)
 * @param bytes_saved 절약된 바이트 (출력)
 * @param optimization_count 최적화 수행 횟수 (출력)
 */
void et_get_smart_manager_stats(ETSmartMemoryManager* manager,
                               uint64_t* total_allocations,
                               uint64_t* bytes_saved,
                               uint64_t* optimization_count);

/**
 * @brief 스마트 메모리 매니저 소멸
 * @param manager 스마트 메모리 매니저
 */
void et_destroy_smart_memory_manager(ETSmartMemoryManager* manager);

// =============================================================================
// 유틸리티 함수
// =============================================================================

/**
 * @brief 크기를 2의 거듭제곱으로 올림
 * @param size 입력 크기
 * @return 2의 거듭제곱으로 올림된 크기
 */
size_t et_round_up_to_power_of_2(size_t size);

/**
 * @brief 메모리 사용량 최적화 권장사항 생성
 * @param pool 메모리 풀
 * @param recommendations 권장사항 문자열 버퍼
 * @param buffer_size 버퍼 크기
 * @return 권장사항 수
 */
int et_generate_memory_recommendations(ETMemoryPool* pool, char* recommendations, size_t buffer_size);

/**
 * @brief 메모리 최적화 리포트 출력
 * @param manager 스마트 메모리 매니저 (NULL 가능)
 * @param pool 메모리 풀
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void et_print_memory_optimization_report(ETSmartMemoryManager* manager,
                                        ETMemoryPool* pool,
                                        const char* output_file);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_MEMORY_OPTIMIZATION_H