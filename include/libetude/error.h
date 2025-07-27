/**
 * @file error.h
 * @brief LibEtude 오류 처리 및 로깅 시스템
 *
 * 이 헤더는 LibEtude의 오류 처리와 로깅 시스템을 정의합니다.
 * 오류 코드, 메시지 시스템, 콜백 기반 오류 처리, 로그 레벨 및 필터링을 제공합니다.
 */

#ifndef LIBETUDE_ERROR_H
#define LIBETUDE_ERROR_H

#include "libetude/types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// types.h에서 정의된 오류 코드를 사용
typedef LibEtudeErrorCode ETErrorCode;

// 추가 오류 코드 정의 (types.h에 없는 것들)
#define ET_ERROR_THREAD -15             ///< 스레드 관련 오류
#define ET_ERROR_AUDIO -16              ///< 오디오 관련 오류
#define ET_ERROR_COMPRESSION -17        ///< 압축 관련 오류
#define ET_ERROR_QUANTIZATION -18       ///< 양자화 관련 오류
#define ET_ERROR_GRAPH -19              ///< 그래프 관련 오류
#define ET_ERROR_KERNEL -20             ///< 커널 관련 오류
#define ET_ERROR_UNKNOWN -999           ///< 알 수 없는 오류

/**
 * @brief 로그 레벨 열거형
 *
 * 로그 메시지의 중요도를 나타냅니다.
 */
typedef enum {
    ET_LOG_DEBUG = 0,      ///< 디버그 정보
    ET_LOG_INFO = 1,       ///< 일반 정보
    ET_LOG_WARNING = 2,    ///< 경고
    ET_LOG_ERROR = 3,      ///< 오류
    ET_LOG_FATAL = 4       ///< 치명적 오류
} ETLogLevel;

/**
 * @brief 오류 정보 구조체
 *
 * 발생한 오류에 대한 상세 정보를 담습니다.
 */
typedef struct {
    ETErrorCode code;          ///< 오류 코드
    char message[256];         ///< 오류 메시지
    const char* file;          ///< 오류 발생 파일명
    int line;                  ///< 오류 발생 라인 번호
    const char* function;      ///< 오류 발생 함수명
    uint64_t timestamp;        ///< 오류 발생 시간 (마이크로초)
} ETError;

/**
 * @brief 오류 콜백 함수 타입
 *
 * @param error 발생한 오류 정보
 * @param user_data 사용자 데이터
 */
typedef void (*ETErrorCallback)(const ETError* error, void* user_data);

/**
 * @brief 로그 콜백 함수 타입
 *
 * @param level 로그 레벨
 * @param message 로그 메시지
 * @param user_data 사용자 데이터
 */
typedef void (*ETLogCallback)(ETLogLevel level, const char* message, void* user_data);

// =============================================================================
// 오류 처리 함수들
// =============================================================================

/**
 * @brief 마지막 오류 정보를 가져옵니다.
 *
 * @return 마지막 오류 정보 포인터 (오류가 없으면 NULL)
 */
const ETError* et_get_last_error(void);

/**
 * @brief 오류 코드를 문자열로 변환합니다.
 *
 * @param code 오류 코드
 * @return 오류 코드에 해당하는 문자열
 */
const char* et_error_string(ETErrorCode code);

/**
 * @brief 저장된 오류 정보를 지웁니다.
 */
void et_clear_error(void);

/**
 * @brief 오류를 설정합니다.
 *
 * @param code 오류 코드
 * @param file 파일명
 * @param line 라인 번호
 * @param function 함수명
 * @param format 메시지 포맷 문자열
 * @param ... 포맷 인수들
 */
void et_set_error(ETErrorCode code, const char* file, int line,
                  const char* function, const char* format, ...);

/**
 * @brief 오류 콜백을 설정합니다.
 *
 * @param callback 오류 콜백 함수
 * @param user_data 사용자 데이터
 */
void et_set_error_callback(ETErrorCallback callback, void* user_data);

/**
 * @brief 오류 콜백을 제거합니다.
 */
void et_clear_error_callback(void);

// =============================================================================
// 로깅 함수들
// =============================================================================

/**
 * @brief 로그 메시지를 출력합니다.
 *
 * @param level 로그 레벨
 * @param format 메시지 포맷 문자열
 * @param ... 포맷 인수들
 */
void et_log(ETLogLevel level, const char* format, ...);

/**
 * @brief 로그 메시지를 출력합니다 (va_list 버전).
 *
 * @param level 로그 레벨
 * @param format 메시지 포맷 문자열
 * @param args 포맷 인수들
 */
void et_log_va(ETLogLevel level, const char* format, va_list args);

/**
 * @brief 현재 로그 레벨을 설정합니다.
 *
 * @param level 최소 로그 레벨 (이 레벨 이상만 출력됨)
 */
void et_set_log_level(ETLogLevel level);

/**
 * @brief 현재 로그 레벨을 가져옵니다.
 *
 * @return 현재 로그 레벨
 */
ETLogLevel et_get_log_level(void);

/**
 * @brief 로그 콜백을 설정합니다.
 *
 * @param callback 로그 콜백 함수
 * @param user_data 사용자 데이터
 */
void et_set_log_callback(ETLogCallback callback, void* user_data);

/**
 * @brief 로그 콜백을 제거합니다.
 */
void et_clear_log_callback(void);

/**
 * @brief 로그 레벨을 문자열로 변환합니다.
 *
 * @param level 로그 레벨
 * @return 로그 레벨에 해당하는 문자열
 */
const char* et_log_level_string(ETLogLevel level);

/**
 * @brief 로깅 시스템을 초기화합니다.
 *
 * @return 성공 시 ET_SUCCESS, 실패 시 오류 코드
 */
ETErrorCode et_init_logging(void);

/**
 * @brief 로깅 시스템을 정리합니다.
 */
void et_cleanup_logging(void);

// =============================================================================
// 편의 매크로들
// =============================================================================

/**
 * @brief 오류 설정 매크로
 */
#define ET_SET_ERROR(code, format, ...) \
    et_set_error(code, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/**
 * @brief 조건부 오류 설정 매크로
 */
#define ET_CHECK_ERROR(condition, code, format, ...) \
    do { \
        if (!(condition)) { \
            ET_SET_ERROR(code, format, ##__VA_ARGS__); \
            return code; \
        } \
    } while(0)

/**
 * @brief 포인터 NULL 체크 매크로
 */
#define ET_CHECK_NULL(ptr, format, ...) \
    ET_CHECK_ERROR((ptr) != NULL, ET_ERROR_INVALID_ARGUMENT, format, ##__VA_ARGS__)

/**
 * @brief 메모리 할당 체크 매크로
 */
#define ET_CHECK_ALLOC(ptr) \
    ET_CHECK_ERROR((ptr) != NULL, ET_ERROR_OUT_OF_MEMORY, "메모리 할당 실패")

/**
 * @brief 로그 매크로들
 */
#define ET_LOG_DEBUG(format, ...) et_log(ET_LOG_DEBUG, format, ##__VA_ARGS__)
#define ET_LOG_INFO(format, ...) et_log(ET_LOG_INFO, format, ##__VA_ARGS__)
#define ET_LOG_WARNING(format, ...) et_log(ET_LOG_WARNING, format, ##__VA_ARGS__)
#define ET_LOG_ERROR(format, ...) et_log(ET_LOG_ERROR, format, ##__VA_ARGS__)
#define ET_LOG_FATAL(format, ...) et_log(ET_LOG_FATAL, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_ERROR_H