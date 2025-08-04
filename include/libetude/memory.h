/**
 * @file memory.h
 * @brief LibEtude 메모리 관리 시스템
 *
 * 이 파일은 LibEtude의 메모리 풀과 할당자 시스템을 정의합니다.
 * 고정 크기 및 동적 크기 메모리 풀을 지원하며, 스레드 안전성을 보장합니다.
 */

#ifndef LIBETUDE_MEMORY_H
#define LIBETUDE_MEMORY_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "threading.h"

#ifdef __cplusplus
extern "C" {
#endif

// 메모리 정렬 매크로
#define ET_MEMORY_ALIGN_16  16
#define ET_MEMORY_ALIGN_32  32
#define ET_MEMORY_ALIGN_64  64
#define ET_MEMORY_ALIGN_128 128
#define ET_MEMORY_ALIGN_256 256

// 기본 정렬 크기 (SIMD 최적화를 위해 32바이트)
#define ET_DEFAULT_ALIGNMENT ET_MEMORY_ALIGN_32

// 메모리 풀 타입
typedef enum {
    ET_POOL_FIXED = 0,    // 고정 크기 메모리 풀
    ET_POOL_DYNAMIC = 1   // 동적 크기 메모리 풀
} ETMemoryPoolType;

// 메모리 타입
typedef enum {
    ET_MEM_CPU = 0,       // CPU 메모리
    ET_MEM_GPU = 1,       // GPU 메모리
    ET_MEM_SHARED = 2     // 공유 메모리
} ETMemoryType;

// 메모리 블록 헤더 (내부 사용)
typedef struct ETMemoryBlock {
    size_t size;                    // 블록 크기
    bool is_free;                   // 사용 가능 여부
    struct ETMemoryBlock* next;     // 다음 블록
    struct ETMemoryBlock* prev;     // 이전 블록

    // 메모리 누수 감지용 정보
    const char* file;               // 할당된 파일명
    int line;                       // 할당된 라인 번호
    const char* function;           // 할당된 함수명
    uint64_t timestamp;             // 할당 시간
    uint32_t magic;                 // 매직 넘버 (손상 감지용)
} ETMemoryBlock;

// 메모리 풀 구조체
typedef struct {
    // 기본 정보
    void* base;                     // 기본 메모리 주소
    size_t total_size;              // 총 크기
    size_t used_size;               // 사용된 크기
    size_t peak_usage;              // 최대 사용량
    size_t alignment;               // 정렬 요구사항

    // 풀 타입 및 설정
    ETMemoryPoolType pool_type;     // 풀 타입
    ETMemoryType mem_type;          // 메모리 타입
    bool external;                  // 외부 메모리 여부

    // 블록 관리 (동적 풀용)
    ETMemoryBlock* free_list;       // 자유 블록 리스트
    ETMemoryBlock* used_list;       // 사용 중인 블록 리스트
    size_t min_block_size;          // 최소 블록 크기

    // 고정 크기 풀 관리
    void** fixed_blocks;            // 고정 크기 블록 배열
    size_t block_size;              // 고정 블록 크기
    size_t num_blocks;              // 총 블록 수
    size_t free_blocks;             // 사용 가능한 블록 수
    uint8_t* block_bitmap;          // 블록 사용 비트맵

    // 통계
    size_t num_allocations;         // 할당 횟수
    size_t num_frees;               // 해제 횟수
    size_t num_resets;              // 리셋 횟수

    // GPU 컨텍스트 (필요시)
    void* device_context;           // GPU 컨텍스트

    // 스레드 안전성
    et_mutex_t mutex;               // 뮤텍스
    bool thread_safe;               // 스레드 안전성 활성화 여부

    // 메모리 누수 감지
    bool leak_detection_enabled;    // 누수 감지 활성화 여부
    ETMemoryBlock** active_blocks;  // 활성 블록 추적 배열
    size_t active_block_count;      // 활성 블록 수
    size_t active_block_capacity;   // 활성 블록 배열 용량
} ETMemoryPool;

// 메모리 풀 생성 옵션
typedef struct {
    ETMemoryPoolType pool_type;     // 풀 타입
    ETMemoryType mem_type;          // 메모리 타입
    size_t alignment;               // 정렬 요구사항
    size_t block_size;              // 고정 블록 크기 (고정 풀용)
    size_t min_block_size;          // 최소 블록 크기 (동적 풀용)
    bool thread_safe;               // 스레드 안전성
    bool enable_leak_detection;     // 메모리 누수 감지 활성화
    void* device_context;           // GPU 컨텍스트
} ETMemoryPoolOptions;

// 메모리 누수 정보
typedef struct {
    void* ptr;                      // 메모리 주소
    size_t size;                    // 할당 크기
    const char* file;               // 할당된 파일명
    int line;                       // 할당된 라인 번호
    const char* function;           // 할당된 함수명
    uint64_t timestamp;             // 할당 시간
} ETMemoryLeakInfo;

// 메모리 풀 통계
typedef struct {
    size_t total_size;              // 총 크기
    size_t used_size;               // 사용된 크기
    size_t peak_usage;              // 최대 사용량
    size_t free_size;               // 사용 가능한 크기
    size_t num_allocations;         // 할당 횟수
    size_t num_frees;               // 해제 횟수
    size_t num_resets;              // 리셋 횟수
    float fragmentation_ratio;      // 단편화 비율

    // 메모리 누수 감지 관련
    size_t num_active_blocks;       // 활성 블록 수
    size_t num_leaked_blocks;       // 누수 의심 블록 수
    size_t leaked_bytes;            // 누수된 바이트 수
} ETMemoryPoolStats;

// =============================================================================
// 메모리 풀 관리 함수
// =============================================================================

/**
 * @brief 메모리 풀 생성
 * @param size 총 메모리 크기
 * @param alignment 메모리 정렬 요구사항
 * @return 생성된 메모리 풀 포인터, 실패시 NULL
 */
ETMemoryPool* et_create_memory_pool(size_t size, size_t alignment);

/**
 * @brief 옵션을 사용한 메모리 풀 생성
 * @param size 총 메모리 크기
 * @param options 메모리 풀 옵션
 * @return 생성된 메모리 풀 포인터, 실패시 NULL
 */
ETMemoryPool* et_create_memory_pool_with_options(size_t size, const ETMemoryPoolOptions* options);

/**
 * @brief 외부 메모리를 사용한 메모리 풀 생성
 * @param base 외부 메모리 주소
 * @param size 메모리 크기
 * @param options 메모리 풀 옵션
 * @return 생성된 메모리 풀 포인터, 실패시 NULL
 */
ETMemoryPool* et_create_memory_pool_from_buffer(void* base, size_t size, const ETMemoryPoolOptions* options);

/**
 * @brief 메모리 풀에서 메모리 할당
 * @param pool 메모리 풀
 * @param size 할당할 크기
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* et_alloc_from_pool(ETMemoryPool* pool, size_t size);

/**
 * @brief 정렬된 메모리 할당
 * @param pool 메모리 풀
 * @param size 할당할 크기
 * @param alignment 정렬 요구사항
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* et_alloc_aligned_from_pool(ETMemoryPool* pool, size_t size, size_t alignment);

/**
 * @brief 메모리 풀에 메모리 반환
 * @param pool 메모리 풀
 * @param ptr 반환할 메모리 포인터
 */
void et_free_to_pool(ETMemoryPool* pool, void* ptr);

/**
 * @brief 메모리 풀 리셋 (모든 할당 해제)
 * @param pool 메모리 풀
 */
void et_reset_pool(ETMemoryPool* pool);

/**
 * @brief 메모리 풀 통계 조회
 * @param pool 메모리 풀
 * @param stats 통계 정보를 저장할 구조체
 */
void et_get_pool_stats(ETMemoryPool* pool, ETMemoryPoolStats* stats);

/**
 * @brief 메모리 풀 소멸
 * @param pool 메모리 풀
 */
void et_destroy_memory_pool(ETMemoryPool* pool);

// =============================================================================
// 유틸리티 함수
// =============================================================================

/**
 * @brief 메모리 정렬 계산
 * @param size 크기
 * @param alignment 정렬 요구사항
 * @return 정렬된 크기
 */
size_t et_align_size(size_t size, size_t alignment);

/**
 * @brief 메모리 주소 정렬 확인
 * @param ptr 메모리 주소
 * @param alignment 정렬 요구사항
 * @return 정렬되어 있으면 true, 아니면 false
 */
bool et_is_aligned(const void* ptr, size_t alignment);

/**
 * @brief 메모리 풀 유효성 검사
 * @param pool 메모리 풀
 * @return 유효하면 true, 아니면 false
 */
bool et_validate_memory_pool(ETMemoryPool* pool);

/**
 * @brief 메모리 풀 락 (내부 사용)
 * @param pool 메모리 풀
 */
void et_lock_pool(ETMemoryPool* pool);

/**
 * @brief 메모리 풀 언락 (내부 사용)
 * @param pool 메모리 풀
 */
void et_unlock_pool(ETMemoryPool* pool);

// =============================================================================
// 메모리 누수 감지 함수
// =============================================================================

/**
 * @brief 메모리 누수 감지 활성화/비활성화
 * @param pool 메모리 풀
 * @param enable 활성화 여부
 */
void et_enable_leak_detection(ETMemoryPool* pool, bool enable);

/**
 * @brief 메모리 누수 검사 수행
 * @param pool 메모리 풀
 * @param leak_threshold_ms 누수 판단 임계값 (밀리초)
 * @return 누수된 블록 수
 */
size_t et_check_memory_leaks(ETMemoryPool* pool, uint64_t leak_threshold_ms);

/**
 * @brief 메모리 누수 정보 조회
 * @param pool 메모리 풀
 * @param leak_infos 누수 정보를 저장할 배열
 * @param max_infos 최대 정보 개수
 * @return 실제 누수 정보 개수
 */
size_t et_get_memory_leaks(ETMemoryPool* pool, ETMemoryLeakInfo* leak_infos, size_t max_infos);

/**
 * @brief 메모리 누수 리포트 출력
 * @param pool 메모리 풀
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void et_print_memory_leak_report(ETMemoryPool* pool, const char* output_file);

/**
 * @brief 메모리 블록 손상 검사
 * @param pool 메모리 풀
 * @return 손상된 블록 수
 */
size_t et_check_memory_corruption(ETMemoryPool* pool);

// =============================================================================
// 런타임 할당자 (Runtime Allocator) 인터페이스
// =============================================================================

// 런타임 할당자 구조체 (불투명 타입)
typedef struct RTAllocator RTAllocator;

/**
 * @brief 런타임 할당자 생성
 * @param size 총 메모리 크기
 * @param alignment 메모리 정렬 요구사항
 * @return 생성된 할당자 포인터, 실패시 NULL
 */
RTAllocator* rt_create_allocator(size_t size, size_t alignment);

/**
 * @brief 런타임 할당자에서 메모리 할당
 * @param allocator 할당자
 * @param size 할당할 크기
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* rt_alloc(RTAllocator* allocator, size_t size);

/**
 * @brief 정렬된 메모리 할당
 * @param allocator 할당자
 * @param size 할당할 크기
 * @param alignment 정렬 요구사항
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* rt_alloc_aligned(RTAllocator* allocator, size_t size, size_t alignment);

/**
 * @brief 0으로 초기화된 메모리 할당
 * @param allocator 할당자
 * @param num 요소 개수
 * @param size 각 요소 크기
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* rt_calloc(RTAllocator* allocator, size_t num, size_t size);

/**
 * @brief 메모리 재할당
 * @param allocator 할당자
 * @param ptr 기존 메모리 포인터
 * @param new_size 새로운 크기
 * @return 재할당된 메모리 포인터, 실패시 NULL
 */
void* rt_realloc(RTAllocator* allocator, void* ptr, size_t new_size);

/**
 * @brief 메모리 해제
 * @param allocator 할당자
 * @param ptr 해제할 메모리 포인터
 */
void rt_free(RTAllocator* allocator, void* ptr);

/**
 * @brief 할당자 리셋 (모든 할당 해제)
 * @param allocator 할당자
 */
void rt_reset_allocator(RTAllocator* allocator);

/**
 * @brief 할당자 소멸
 * @param allocator 할당자
 */
void rt_destroy_allocator(RTAllocator* allocator);

/**
 * @brief 총 메모리 크기 조회
 * @param allocator 할당자
 * @return 총 메모리 크기
 */
size_t rt_get_total_size(RTAllocator* allocator);

/**
 * @brief 사용된 메모리 크기 조회
 * @param allocator 할당자
 * @return 사용된 메모리 크기
 */
size_t rt_get_used_size(RTAllocator* allocator);

/**
 * @brief 사용 가능한 메모리 크기 조회
 * @param allocator 할당자
 * @return 사용 가능한 메모리 크기
 */
size_t rt_get_free_size(RTAllocator* allocator);

/**
 * @brief 최대 사용량 조회
 * @param allocator 할당자
 * @return 최대 사용량
 */
size_t rt_get_peak_usage(RTAllocator* allocator);

/**
 * @brief 할당자 통계 조회
 * @param allocator 할당자
 * @param stats 통계 정보를 저장할 구조체
 */
void rt_get_allocator_stats(RTAllocator* allocator, ETMemoryPoolStats* stats);

/**
 * @brief 할당자 유효성 검사
 * @param allocator 할당자
 * @return 유효하면 true, 아니면 false
 */
bool rt_validate_allocator(RTAllocator* allocator);

/**
 * @brief 할당자 정보 출력
 * @param allocator 할당자
 */
void rt_print_allocator_info(RTAllocator* allocator);

// =============================================================================
// 런타임 할당자 메모리 누수 감지 함수
// =============================================================================

/**
 * @brief 할당자의 메모리 누수 감지 활성화/비활성화
 * @param allocator 할당자
 * @param enable 활성화 여부
 */
void rt_enable_leak_detection(RTAllocator* allocator, bool enable);

/**
 * @brief 할당자의 메모리 누수 검사 수행
 * @param allocator 할당자
 * @param leak_threshold_ms 누수 판단 임계값 (밀리초)
 * @return 누수된 블록 수
 */
size_t rt_check_memory_leaks(RTAllocator* allocator, uint64_t leak_threshold_ms);

/**
 * @brief 할당자의 메모리 누수 정보 조회
 * @param allocator 할당자
 * @param leak_infos 누수 정보를 저장할 배열
 * @param max_infos 최대 정보 개수
 * @return 실제 누수 정보 개수
 */
size_t rt_get_memory_leaks(RTAllocator* allocator, ETMemoryLeakInfo* leak_infos, size_t max_infos);

/**
 * @brief 할당자의 메모리 누수 리포트 출력
 * @param allocator 할당자
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void rt_print_memory_leak_report(RTAllocator* allocator, const char* output_file);

/**
 * @brief 할당자의 메모리 블록 손상 검사
 * @param allocator 할당자
 * @return 손상된 블록 수
 */
size_t rt_check_memory_corruption(RTAllocator* allocator);

// 디버그 모드에서 사용할 매크로
#ifdef ET_DEBUG_MEMORY
#define et_alloc_debug(pool, size) et_alloc_from_pool_debug(pool, size, __FILE__, __LINE__, __FUNCTION__)
#define et_free_debug(pool, ptr) et_free_to_pool_debug(pool, ptr, __FILE__, __LINE__, __FUNCTION__)
#define rt_alloc_debug(allocator, size) rt_alloc_debug_impl(allocator, size, __FILE__, __LINE__, __FUNCTION__)
#define rt_free_debug(allocator, ptr) rt_free_debug_impl(allocator, ptr, __FILE__, __LINE__, __FUNCTION__)

void* et_alloc_from_pool_debug(ETMemoryPool* pool, size_t size, const char* file, int line, const char* function);
void et_free_to_pool_debug(ETMemoryPool* pool, void* ptr, const char* file, int line, const char* function);
void* rt_alloc_debug_impl(RTAllocator* allocator, size_t size, const char* file, int line, const char* function);
void rt_free_debug_impl(RTAllocator* allocator, void* ptr, const char* file, int line, const char* function);
#else
#define et_alloc_debug(pool, size) et_alloc_from_pool(pool, size)
#define et_free_debug(pool, ptr) et_free_to_pool(pool, ptr)
#define rt_alloc_debug(allocator, size) rt_alloc(allocator, size)
#define rt_free_debug(allocator, ptr) rt_free(allocator, ptr)
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_MEMORY_H