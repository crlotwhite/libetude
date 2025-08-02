/**
 * @file world_error.h
 * @brief WORLD4UTAU 전용 에러 처리 및 로깅 시스템
 *
 * 이 헤더는 world4utau 예제 프로젝트의 에러 처리와 로깅 시스템을 정의합니다.
 * libetude의 기본 에러 시스템을 확장하여 WORLD 알고리즘 및 UTAU 인터페이스
 * 관련 에러 코드와 처리 기능을 제공합니다.
 */

#ifndef WORLD4UTAU_ERROR_H
#define WORLD4UTAU_ERROR_H

#include "libetude/error.h"
#include "libetude/types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// WORLD4UTAU 전용 에러 코드 정의
// =============================================================================

/**
 * @brief WORLD4UTAU 전용 에러 코드
 *
 * libetude의 기본 에러 코드를 확장하여 WORLD 알고리즘과 UTAU 인터페이스
 * 관련 에러를 정의합니다. -2000번대를 사용하여 충돌을 방지합니다.
 */
typedef enum {
    // UTAU 인터페이스 관련 에러 (-2000 ~ -2099)
    WORLD_ERROR_UTAU_INVALID_PARAMS = -2000,     ///< 잘못된 UTAU 파라미터
    WORLD_ERROR_UTAU_PARSE_FAILED = -2001,       ///< UTAU 파라미터 파싱 실패
    WORLD_ERROR_UTAU_FILE_NOT_FOUND = -2002,     ///< UTAU 파일을 찾을 수 없음
    WORLD_ERROR_UTAU_INVALID_FORMAT = -2003,     ///< 잘못된 UTAU 파일 형식
    WORLD_ERROR_UTAU_PITCH_RANGE = -2004,        ///< 피치 범위 초과
    WORLD_ERROR_UTAU_VELOCITY_RANGE = -2005,     ///< 벨로시티 범위 초과
    WORLD_ERROR_UTAU_TIMING_INVALID = -2006,     ///< 잘못된 타이밍 파라미터

    // WORLD 분석 관련 에러 (-2100 ~ -2199)
    WORLD_ERROR_ANALYSIS_FAILED = -2100,         ///< WORLD 분석 실패
    WORLD_ERROR_F0_EXTRACTION_FAILED = -2101,    ///< F0 추출 실패
    WORLD_ERROR_SPECTRUM_ANALYSIS_FAILED = -2102, ///< 스펙트럼 분석 실패
    WORLD_ERROR_APERIODICITY_FAILED = -2103,     ///< 비주기성 분석 실패
    WORLD_ERROR_INVALID_AUDIO_DATA = -2104,      ///< 잘못된 오디오 데이터
    WORLD_ERROR_AUDIO_TOO_SHORT = -2105,         ///< 오디오가 너무 짧음
    WORLD_ERROR_AUDIO_TOO_LONG = -2106,          ///< 오디오가 너무 김
    WORLD_ERROR_INVALID_SAMPLE_RATE = -2107,     ///< 잘못된 샘플링 레이트

    // WORLD 합성 관련 에러 (-2200 ~ -2299)
    WORLD_ERROR_SYNTHESIS_FAILED = -2200,        ///< WORLD 합성 실패
    WORLD_ERROR_INVALID_F0_DATA = -2201,         ///< 잘못된 F0 데이터
    WORLD_ERROR_INVALID_SPECTRUM_DATA = -2202,   ///< 잘못된 스펙트럼 데이터
    WORLD_ERROR_INVALID_APERIODICITY_DATA = -2203, ///< 잘못된 비주기성 데이터
    WORLD_ERROR_PARAMETER_MISMATCH = -2204,      ///< 파라미터 불일치
    WORLD_ERROR_SYNTHESIS_BUFFER_OVERFLOW = -2205, ///< 합성 버퍼 오버플로우

    // 오디오 I/O 관련 에러 (-2300 ~ -2399)
    WORLD_ERROR_AUDIO_FILE_READ = -2300,         ///< 오디오 파일 읽기 실패
    WORLD_ERROR_AUDIO_FILE_WRITE = -2301,        ///< 오디오 파일 쓰기 실패
    WORLD_ERROR_UNSUPPORTED_AUDIO_FORMAT = -2302, ///< 지원되지 않는 오디오 형식
    WORLD_ERROR_AUDIO_FILE_CORRUPT = -2303,      ///< 손상된 오디오 파일
    WORLD_ERROR_AUDIO_BUFFER_UNDERRUN = -2304,   ///< 오디오 버퍼 언더런
    WORLD_ERROR_AUDIO_DEVICE_ERROR = -2305,      ///< 오디오 장치 에러

    // 캐시 관련 에러 (-2400 ~ -2499)
    WORLD_ERROR_CACHE_READ_FAILED = -2400,       ///< 캐시 읽기 실패
    WORLD_ERROR_CACHE_WRITE_FAILED = -2401,      ///< 캐시 쓰기 실패
    WORLD_ERROR_CACHE_INVALID_DATA = -2402,      ///< 잘못된 캐시 데이터
    WORLD_ERROR_CACHE_VERSION_MISMATCH = -2403,  ///< 캐시 버전 불일치
    WORLD_ERROR_CACHE_CORRUPTION = -2404,        ///< 캐시 데이터 손상

    // 메모리 관리 관련 에러 (-2500 ~ -2599)
    WORLD_ERROR_MEMORY_POOL_EXHAUSTED = -2500,   ///< 메모리 풀 고갈
    WORLD_ERROR_MEMORY_ALIGNMENT = -2501,        ///< 메모리 정렬 오류
    WORLD_ERROR_MEMORY_LEAK_DETECTED = -2502,    ///< 메모리 누수 감지

    // 성능 관련 에러 (-2600 ~ -2699)
    WORLD_ERROR_PERFORMANCE_TIMEOUT = -2600,     ///< 성능 타임아웃
    WORLD_ERROR_REALTIME_CONSTRAINT = -2601,     ///< 실시간 제약 위반
    WORLD_ERROR_RESOURCE_EXHAUSTED = -2602       ///< 리소스 고갈
} WorldErrorCode;

/**
 * @brief WORLD 로그 카테고리
 *
 * world4utau 전용 로그 카테고리를 정의합니다.
 */
typedef enum {
    WORLD_LOG_UTAU_INTERFACE,    ///< UTAU 인터페이스 관련 로그
    WORLD_LOG_ANALYSIS,          ///< WORLD 분석 관련 로그
    WORLD_LOG_SYNTHESIS,         ///< WORLD 합성 관련 로그
    WORLD_LOG_AUDIO_IO,          ///< 오디오 I/O 관련 로그
    WORLD_LOG_CACHE,             ///< 캐시 관련 로그
    WORLD_LOG_MEMORY,            ///< 메모리 관리 관련 로그
    WORLD_LOG_PERFORMANCE        ///< 성능 관련 로그
} WorldLogCategory;

// =============================================================================
// 에러 처리 함수들
// =============================================================================

/**
 * @brief WORLD 에러 코드를 문자열로 변환합니다.
 *
 * @param error_code WORLD 에러 코드
 * @return 에러 코드에 해당하는 문자열 (한국어)
 */
const char* world_get_error_string(WorldErrorCode error_code);

/**
 * @brief WORLD 에러를 설정합니다.
 *
 * @param error_code WORLD 에러 코드
 * @param file 파일명
 * @param line 라인 번호
 * @param function 함수명
 * @param format 메시지 포맷 문자열
 * @param ... 포맷 인수들
 */
void world_set_error(WorldErrorCode error_code, const char* file, int line,
                     const char* function, const char* format, ...);

/**
 * @brief 마지막 WORLD 에러 정보를 가져옵니다.
 *
 * @return 마지막 에러 정보 포인터 (에러가 없으면 NULL)
 */
const ETError* world_get_last_error(void);

/**
 * @brief WORLD 에러 정보를 지웁니다.
 */
void world_clear_error(void);

/**
 * @brief WORLD 에러 콜백을 설정합니다.
 *
 * @param callback 에러 콜백 함수
 * @param user_data 사용자 데이터
 */
void world_set_error_callback(ETErrorCallback callback, void* user_data);

// =============================================================================
// 로깅 함수들
// =============================================================================

/**
 * @brief WORLD 로그 메시지를 출력합니다.
 *
 * @param category 로그 카테고리
 * @param level 로그 레벨
 * @param format 메시지 포맷 문자열
 * @param ... 포맷 인수들
 */
void world_log(WorldLogCategory category, ETLogLevel level,
               const char* format, ...);

/**
 * @brief WORLD 로그 카테고리를 문자열로 변환합니다.
 *
 * @param category 로그 카테고리
 * @return 카테고리에 해당하는 문자열
 */
const char* world_log_category_string(WorldLogCategory category);

/**
 * @brief WORLD 로깅 시스템을 초기화합니다.
 *
 * @return 성공 시 ET_SUCCESS, 실패 시 에러 코드
 */
ETResult world_init_logging(void);

/**
 * @brief WORLD 로깅 시스템을 정리합니다.
 */
void world_cleanup_logging(void);

/**
 * @brief WORLD 로그 콜백을 설정합니다.
 *
 * @param callback 로그 콜백 함수
 * @param user_data 사용자 데이터
 */
void world_set_log_callback(ETLogCallback callback, void* user_data);

/**
 * @brief WORLD 로그 콜백을 제거합니다.
 */
void world_clear_log_callback(void);

/**
 * @brief WORLD 로그 레벨을 설정합니다.
 *
 * @param level 최소 로그 레벨
 */
void world_set_log_level(ETLogLevel level);

/**
 * @brief 현재 WORLD 로그 레벨을 가져옵니다.
 *
 * @return 현재 로그 레벨
 */
ETLogLevel world_get_log_level(void);

/**
 * @brief 특정 카테고리의 로그를 활성화/비활성화합니다.
 *
 * @param category 로그 카테고리
 * @param enabled 활성화 여부
 */
void world_set_log_category_enabled(WorldLogCategory category, bool enabled);

/**
 * @brief 특정 카테고리의 로그 활성화 상태를 확인합니다.
 *
 * @param category 로그 카테고리
 * @return 활성화 상태
 */
bool world_is_log_category_enabled(WorldLogCategory category);

/**
 * @brief 로그에 타임스탬프 포함 여부를 설정합니다.
 *
 * @param enabled 타임스탬프 포함 여부
 */
void world_set_log_timestamps(bool enabled);

/**
 * @brief 로그에 스레드 정보 포함 여부를 설정합니다.
 *
 * @param enabled 스레드 정보 포함 여부
 */
void world_set_log_thread_info(bool enabled);

/**
 * @brief 향상된 WORLD 로그 함수 (필터링 및 포맷팅 포함)
 *
 * @param category 로그 카테고리
 * @param level 로그 레벨
 * @param format 메시지 포맷 문자열
 * @param ... 포맷 인수들
 */
void world_log_enhanced(WorldLogCategory category, ETLogLevel level,
                       const char* format, ...);

/**
 * @brief 성능 측정을 위한 로그 함수
 *
 * @param category 로그 카테고리
 * @param operation_name 작업 이름
 * @param duration_ms 소요 시간 (밀리초)
 * @param additional_info 추가 정보 (NULL 가능)
 */
void world_log_performance(WorldLogCategory category, const char* operation_name,
                          double duration_ms, const char* additional_info);

/**
 * @brief 메모리 사용량 로그 함수
 *
 * @param category 로그 카테고리
 * @param operation_name 작업 이름
 * @param memory_bytes 메모리 사용량 (바이트)
 * @param is_allocation 할당이면 true, 해제면 false
 */
void world_log_memory(WorldLogCategory category, const char* operation_name,
                     size_t memory_bytes, bool is_allocation);

// =============================================================================
// 편의 매크로들
// =============================================================================

/**
 * @brief WORLD 에러 설정 매크로
 */
#define WORLD_SET_ERROR(code, format, ...) \
    world_set_error(code, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/**
 * @brief 조건부 WORLD 에러 설정 매크로
 */
#define WORLD_CHECK_ERROR(condition, code, format, ...) \
    do { \
        if (!(condition)) { \
            WORLD_SET_ERROR(code, format, ##__VA_ARGS__); \
            return code; \
        } \
    } while(0)

/**
 * @brief WORLD 포인터 NULL 체크 매크로
 */
#define WORLD_CHECK_NULL(ptr, format, ...) \
    WORLD_CHECK_ERROR((ptr) != NULL, WORLD_ERROR_UTAU_INVALID_PARAMS, format, ##__VA_ARGS__)

/**
 * @brief WORLD 메모리 할당 체크 매크로
 */
#define WORLD_CHECK_ALLOC(ptr) \
    WORLD_CHECK_ERROR((ptr) != NULL, WORLD_ERROR_MEMORY_POOL_EXHAUSTED, "메모리 할당 실패")

/**
 * @brief WORLD 로그 매크로들
 */
#define WORLD_LOG_DEBUG(category, format, ...) \
    world_log(category, ET_LOG_DEBUG, "[WORLD:" #category "] " format, ##__VA_ARGS__)

#define WORLD_LOG_INFO(category, format, ...) \
    world_log(category, ET_LOG_INFO, "[WORLD:" #category "] " format, ##__VA_ARGS__)

#define WORLD_LOG_WARNING(category, format, ...) \
    world_log(category, ET_LOG_WARNING, "[WORLD:" #category "] " format, ##__VA_ARGS__)

#define WORLD_LOG_ERROR(category, format, ...) \
    world_log(category, ET_LOG_ERROR, "[WORLD:" #category "] " format, ##__VA_ARGS__)

#define WORLD_LOG_FATAL(category, format, ...) \
    world_log(category, ET_LOG_FATAL, "[WORLD:" #category "] " format, ##__VA_ARGS__)

/**
 * @brief 카테고리별 로그 매크로들
 */
#define WORLD_LOG_UTAU_DEBUG(format, ...) \
    WORLD_LOG_DEBUG(WORLD_LOG_UTAU_INTERFACE, format, ##__VA_ARGS__)
#define WORLD_LOG_UTAU_INFO(format, ...) \
    WORLD_LOG_INFO(WORLD_LOG_UTAU_INTERFACE, format, ##__VA_ARGS__)
#define WORLD_LOG_UTAU_WARNING(format, ...) \
    WORLD_LOG_WARNING(WORLD_LOG_UTAU_INTERFACE, format, ##__VA_ARGS__)
#define WORLD_LOG_UTAU_ERROR(format, ...) \
    WORLD_LOG_ERROR(WORLD_LOG_UTAU_INTERFACE, format, ##__VA_ARGS__)

#define WORLD_LOG_ANALYSIS_DEBUG(format, ...) \
    WORLD_LOG_DEBUG(WORLD_LOG_ANALYSIS, format, ##__VA_ARGS__)
#define WORLD_LOG_ANALYSIS_INFO(format, ...) \
    WORLD_LOG_INFO(WORLD_LOG_ANALYSIS, format, ##__VA_ARGS__)
#define WORLD_LOG_ANALYSIS_WARNING(format, ...) \
    WORLD_LOG_WARNING(WORLD_LOG_ANALYSIS, format, ##__VA_ARGS__)
#define WORLD_LOG_ANALYSIS_ERROR(format, ...) \
    WORLD_LOG_ERROR(WORLD_LOG_ANALYSIS, format, ##__VA_ARGS__)

#define WORLD_LOG_SYNTHESIS_DEBUG(format, ...) \
    WORLD_LOG_DEBUG(WORLD_LOG_SYNTHESIS, format, ##__VA_ARGS__)
#define WORLD_LOG_SYNTHESIS_INFO(format, ...) \
    WORLD_LOG_INFO(WORLD_LOG_SYNTHESIS, format, ##__VA_ARGS__)
#define WORLD_LOG_SYNTHESIS_WARNING(format, ...) \
    WORLD_LOG_WARNING(WORLD_LOG_SYNTHESIS, format, ##__VA_ARGS__)
#define WORLD_LOG_SYNTHESIS_ERROR(format, ...) \
    WORLD_LOG_ERROR(WORLD_LOG_SYNTHESIS, format, ##__VA_ARGS__)

#define WORLD_LOG_AUDIO_DEBUG(format, ...) \
    WORLD_LOG_DEBUG(WORLD_LOG_AUDIO_IO, format, ##__VA_ARGS__)
#define WORLD_LOG_AUDIO_INFO(format, ...) \
    WORLD_LOG_INFO(WORLD_LOG_AUDIO_IO, format, ##__VA_ARGS__)
#define WORLD_LOG_AUDIO_WARNING(format, ...) \
    WORLD_LOG_WARNING(WORLD_LOG_AUDIO_IO, format, ##__VA_ARGS__)
#define WORLD_LOG_AUDIO_ERROR(format, ...) \
    WORLD_LOG_ERROR(WORLD_LOG_AUDIO_IO, format, ##__VA_ARGS__)

#define WORLD_LOG_CACHE_DEBUG(format, ...) \
    WORLD_LOG_DEBUG(WORLD_LOG_CACHE, format, ##__VA_ARGS__)
#define WORLD_LOG_CACHE_INFO(format, ...) \
    WORLD_LOG_INFO(WORLD_LOG_CACHE, format, ##__VA_ARGS__)
#define WORLD_LOG_CACHE_WARNING(format, ...) \
    WORLD_LOG_WARNING(WORLD_LOG_CACHE, format, ##__VA_ARGS__)
#define WORLD_LOG_CACHE_ERROR(format, ...) \
    WORLD_LOG_ERROR(WORLD_LOG_CACHE, format, ##__VA_ARGS__)

#define WORLD_LOG_MEMORY_DEBUG(format, ...) \
    WORLD_LOG_DEBUG(WORLD_LOG_MEMORY, format, ##__VA_ARGS__)
#define WORLD_LOG_MEMORY_INFO(format, ...) \
    WORLD_LOG_INFO(WORLD_LOG_MEMORY, format, ##__VA_ARGS__)
#define WORLD_LOG_MEMORY_WARNING(format, ...) \
    WORLD_LOG_WARNING(WORLD_LOG_MEMORY, format, ##__VA_ARGS__)
#define WORLD_LOG_MEMORY_ERROR(format, ...) \
    WORLD_LOG_ERROR(WORLD_LOG_MEMORY, format, ##__VA_ARGS__)

#define WORLD_LOG_PERF_DEBUG(format, ...) \
    WORLD_LOG_DEBUG(WORLD_LOG_PERFORMANCE, format, ##__VA_ARGS__)
#define WORLD_LOG_PERF_INFO(format, ...) \
    WORLD_LOG_INFO(WORLD_LOG_PERFORMANCE, format, ##__VA_ARGS__)
#define WORLD_LOG_PERF_WARNING(format, ...) \
    WORLD_LOG_WARNING(WORLD_LOG_PERFORMANCE, format, ##__VA_ARGS__)
#define WORLD_LOG_PERF_ERROR(format, ...) \
    WORLD_LOG_ERROR(WORLD_LOG_PERFORMANCE, format, ##__VA_ARGS__)

/**
 * @brief 향상된 로깅 매크로들 (필터링 및 포맷팅 포함)
 */
#define WORLD_LOG_ENHANCED_DEBUG(category, format, ...) \
    world_log_enhanced(category, ET_LOG_DEBUG, format, ##__VA_ARGS__)

#define WORLD_LOG_ENHANCED_INFO(category, format, ...) \
    world_log_enhanced(category, ET_LOG_INFO, format, ##__VA_ARGS__)

#define WORLD_LOG_ENHANCED_WARNING(category, format, ...) \
    world_log_enhanced(category, ET_LOG_WARNING, format, ##__VA_ARGS__)

#define WORLD_LOG_ENHANCED_ERROR(category, format, ...) \
    world_log_enhanced(category, ET_LOG_ERROR, format, ##__VA_ARGS__)

#define WORLD_LOG_ENHANCED_FATAL(category, format, ...) \
    world_log_enhanced(category, ET_LOG_FATAL, format, ##__VA_ARGS__)

/**
 * @brief 성능 로깅 매크로
 */
#define WORLD_LOG_PERFORMANCE_TIMING(category, operation, duration) \
    world_log_performance(category, operation, duration, NULL)

#define WORLD_LOG_PERFORMANCE_TIMING_INFO(category, operation, duration, info) \
    world_log_performance(category, operation, duration, info)

/**
 * @brief 메모리 로깅 매크로
 */
#define WORLD_LOG_MEMORY_ALLOC(category, operation, bytes) \
    world_log_memory(category, operation, bytes, true)

#define WORLD_LOG_MEMORY_FREE(category, operation, bytes) \
    world_log_memory(category, operation, bytes, false)

/**
 * @brief 조건부 로깅 매크로
 */
#define WORLD_LOG_IF(condition, category, level, format, ...) \
    do { \
        if (condition) { \
            world_log_enhanced(category, level, format, ##__VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 함수 진입/종료 로깅 매크로 (디버그용)
 */
#ifdef DEBUG
#define WORLD_LOG_FUNCTION_ENTER(category) \
    world_log_enhanced(category, ET_LOG_DEBUG, "함수 진입: %s", __func__)

#define WORLD_LOG_FUNCTION_EXIT(category) \
    world_log_enhanced(category, ET_LOG_DEBUG, "함수 종료: %s", __func__)

#define WORLD_LOG_FUNCTION_EXIT_WITH_RESULT(category, result) \
    world_log_enhanced(category, ET_LOG_DEBUG, "함수 종료: %s (결과: %d)", __func__, result)
#else
#define WORLD_LOG_FUNCTION_ENTER(category)
#define WORLD_LOG_FUNCTION_EXIT(category)
#define WORLD_LOG_FUNCTION_EXIT_WITH_RESULT(category, result)
#endif

/**
 * @brief 에러와 함께 로깅하는 매크로
 */
#define WORLD_LOG_ERROR_WITH_CODE(category, error_code, format, ...) \
    do { \
        world_log_enhanced(category, ET_LOG_ERROR, format " (에러 코드: %d - %s)", \
                          ##__VA_ARGS__, error_code, world_get_error_string(error_code)); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_ERROR_H