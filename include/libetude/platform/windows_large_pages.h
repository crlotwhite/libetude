/**
 * @file windows_large_pages.h
 * @brief Windows Large Page 메모리 지원 헤더
 *
 * Windows에서 Large Page 메모리 할당을 지원하여 성능을 향상시킵니다.
 */

#ifndef LIBETUDE_PLATFORM_WINDOWS_LARGE_PAGES_H
#define LIBETUDE_PLATFORM_WINDOWS_LARGE_PAGES_H

#ifdef _WIN32

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Large Page 상태 정보 구조체
 */
typedef struct {
    bool is_supported;             ///< 시스템 Large Page 지원 여부
    bool privilege_enabled;        ///< SeLockMemoryPrivilege 권한 활성화 여부
    SIZE_T large_page_size;        ///< Large Page 크기 (바이트)
    SIZE_T total_allocated;        ///< 총 할당된 Large Page 메모리 크기
    SIZE_T fallback_allocated;     ///< 폴백으로 할당된 일반 메모리 크기
    LONG allocation_count;         ///< 총 할당 횟수
    LONG fallback_count;           ///< 폴백 할당 횟수
} ETLargePageInfo;

/**
 * @brief 메모리 할당 정보 구조체
 */
typedef struct {
    void* address;                 ///< 할당된 메모리 주소
    SIZE_T size;                   ///< 할당된 크기 (바이트)
    bool is_large_page;            ///< Large Page 할당 여부
    DWORD allocation_type;         ///< 할당 타입 (MEM_COMMIT | MEM_RESERVE 등)
    DWORD timestamp;               ///< 할당 시간 (GetTickCount)
} ETMemoryAllocation;

// 초기화 및 정리 함수들

/**
 * @brief Large Page 지원 초기화
 *
 * Large Page 지원을 초기화하고 필요한 권한을 활성화합니다.
 *
 * @return ETResult 초기화 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_SYSTEM_ERROR: 시스템 오류
 */
ETResult et_windows_large_pages_init(void);

/**
 * @brief Large Page 지원 정리
 *
 * Large Page 관련 리소스를 정리합니다.
 */
void et_windows_large_pages_finalize(void);

// 권한 관리 함수들

/**
 * @brief SeLockMemoryPrivilege 권한 활성화
 *
 * Large Page 메모리를 할당하기 위해 필요한 권한을 활성화합니다.
 * 관리자 권한이 필요할 수 있습니다.
 *
 * @return bool 권한 활성화 성공 여부
 */
bool et_windows_enable_large_page_privilege(void);

// 메모리 할당 함수들

/**
 * @brief Large Page 메모리 할당
 *
 * 가능한 경우 Large Page 메모리를 할당하고, 실패 시 일반 메모리로 폴백합니다.
 *
 * @param size 할당할 메모리 크기 (바이트)
 * @return void* 할당된 메모리 주소 (실패 시 NULL)
 *
 * @note 할당된 메모리는 et_windows_free_large_pages()로 해제해야 합니다.
 * @note Large Page 크기로 자동 정렬됩니다.
 */
void* et_windows_alloc_large_pages(size_t size);

/**
 * @brief Large Page 메모리 해제
 *
 * et_windows_alloc_large_pages()로 할당된 메모리를 해제합니다.
 *
 * @param memory 해제할 메모리 주소
 * @param size 메모리 크기 (통계 업데이트용)
 */
void et_windows_free_large_pages(void* memory, size_t size);

/**
 * @brief Large Page 메모리 재할당
 *
 * 기존 메모리를 새로운 크기로 재할당합니다.
 *
 * @param memory 기존 메모리 주소 (NULL 가능)
 * @param old_size 기존 메모리 크기
 * @param new_size 새로운 메모리 크기
 * @return void* 재할당된 메모리 주소 (실패 시 NULL)
 *
 * @note memory가 NULL이면 새로 할당합니다.
 * @note new_size가 0이면 메모리를 해제하고 NULL을 반환합니다.
 */
void* et_windows_realloc_large_pages(void* memory, size_t old_size, size_t new_size);

/**
 * @brief 메모리 정렬된 할당 (Large Page 고려)
 *
 * 지정된 정렬 요구사항을 만족하는 메모리를 할당합니다.
 *
 * @param size 할당할 크기
 * @param alignment 정렬 크기 (2의 거듭제곱이어야 함)
 * @return void* 정렬된 메모리 주소 (실패 시 NULL)
 */
void* et_windows_alloc_aligned_large_pages(size_t size, size_t alignment);

// 상태 조회 함수들

/**
 * @brief Large Page 상태 정보 조회
 *
 * 현재 Large Page 사용 상태와 통계 정보를 조회합니다.
 *
 * @param info 상태 정보를 저장할 구조체 포인터
 * @return ETResult 조회 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_INVALID_PARAMETER: 잘못된 매개변수
 */
ETResult et_windows_large_pages_get_info(ETLargePageInfo* info);

/**
 * @brief Large Page 상태를 문자열로 반환
 *
 * Large Page 상태 정보를 사람이 읽기 쉬운 문자열로 변환합니다.
 *
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 * @return ETResult 변환 결과
 *   - ET_RESULT_SUCCESS: 성공
 *   - ET_RESULT_INVALID_PARAMETER: 잘못된 매개변수
 */
ETResult et_windows_large_pages_status_to_string(char* buffer, size_t buffer_size);

/**
 * @brief Large Page 통계 초기화
 *
 * 할당 통계를 초기화합니다. (실제 메모리는 해제하지 않음)
 */
void et_windows_large_pages_reset_stats(void);

/**
 * @brief 메모리 사용량 통계 조회
 *
 * 시스템 전체 메모리 사용량과 Large Page 관련 통계를 조회합니다.
 *
 * @param total_system_memory 전체 시스템 메모리 크기 (바이트)
 * @param available_memory 사용 가능한 메모리 크기 (바이트)
 * @param large_page_total 전체 Large Page 메모리 크기 (바이트)
 * @param large_page_free 사용 가능한 Large Page 메모리 크기 (바이트)
 * @return ETResult 조회 결과
 */
ETResult et_windows_large_pages_get_memory_stats(size_t* total_system_memory,
                                                size_t* available_memory,
                                                size_t* large_page_total,
                                                size_t* large_page_free);

/**
 * @brief 활성 메모리 할당 목록 조회
 *
 * 현재 활성화된 메모리 할당 목록을 조회합니다.
 *
 * @param allocations 할당 정보를 저장할 배열
 * @param max_count 배열의 최대 크기
 * @param actual_count 실제 반환된 할당 개수
 * @return ETResult 조회 결과
 */
ETResult et_windows_large_pages_get_active_allocations(ETMemoryAllocation* allocations,
                                                      size_t max_count,
                                                      size_t* actual_count);

/**
 * @brief Large Page 성능 벤치마크 실행
 *
 * Large Page와 일반 메모리의 성능을 비교 측정합니다.
 *
 * @param test_size 테스트할 메모리 크기 (바이트)
 * @param iterations 반복 횟수
 * @param large_page_time_ms Large Page 할당 시간 (밀리초)
 * @param regular_time_ms 일반 메모리 할당 시간 (밀리초)
 * @return ETResult 벤치마크 결과
 */
ETResult et_windows_large_pages_benchmark(size_t test_size,
                                         int iterations,
                                         double* large_page_time_ms,
                                         double* regular_time_ms);

// 편의 매크로들

/**
 * @brief Large Page 크기로 정렬된 크기 계산
 *
 * @param size 원본 크기
 * @return 정렬된 크기
 */
#define ET_ALIGN_TO_LARGE_PAGE(size) \
    (((size) + ET_LARGE_PAGE_SIZE - 1) & ~(ET_LARGE_PAGE_SIZE - 1))

/**
 * @brief 일반적인 Large Page 크기 (2MB)
 */
#define ET_LARGE_PAGE_SIZE (2 * 1024 * 1024)

/**
 * @brief Large Page 할당에 적합한 최소 크기
 */
#define ET_LARGE_PAGE_THRESHOLD (64 * 1024)  // 64KB 이상일 때 Large Page 사용 권장

// 편의 함수들

/**
 * @brief 크기가 Large Page 사용에 적합한지 확인
 *
 * @param size 확인할 크기
 * @return bool Large Page 사용 권장 여부
 */
static inline bool et_windows_should_use_large_pages(size_t size) {
    return size >= ET_LARGE_PAGE_THRESHOLD;
}

/**
 * @brief 스마트 메모리 할당 (크기에 따라 Large Page 자동 선택)
 *
 * @param size 할당할 크기
 * @return void* 할당된 메모리 주소
 */
static inline void* et_windows_alloc_smart(size_t size) {
    if (et_windows_should_use_large_pages(size)) {
        return et_windows_alloc_large_pages(size);
    } else {
        return malloc(size);
    }
}

/**
 * @brief 스마트 메모리 해제
 *
 * @param memory 해제할 메모리 주소
 * @param size 메모리 크기
 * @param was_large_page Large Page로 할당되었는지 여부
 */
static inline void et_windows_free_smart(void* memory, size_t size, bool was_large_page) {
    if (was_large_page) {
        et_windows_free_large_pages(memory, size);
    } else {
        free(memory);
    }
}

#ifdef __cplusplus
}
#endif

#endif // _WIN32

#endif // LIBETUDE_PLATFORM_WINDOWS_LARGE_PAGES_H