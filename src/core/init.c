/**
 * @file init.c
 * @brief LibEtude 초기화 및 정리 함수
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

// 로그 함수 전방 선언
LIBETUDE_API void libetude_log(LibEtudeLogLevel level, const char* format, ...);

// 전역 초기화 상태
static bool g_libetude_initialized = false;
static LibEtudeLogCallback g_log_callback = NULL;
static void* g_log_user_data = NULL;
static LibEtudeLogLevel g_log_level = LIBETUDE_LOG_INFO;

/**
 * @brief 기본 로그 출력 함수
 */
static void default_log_callback(LibEtudeLogLevel level, const char* message, void* user_data) {
    (void)user_data; // 사용하지 않는 매개변수

    const char* level_str;
    switch (level) {
        case LIBETUDE_LOG_DEBUG:   level_str = "DEBUG"; break;
        case LIBETUDE_LOG_INFO:    level_str = "INFO"; break;
        case LIBETUDE_LOG_WARNING: level_str = "WARNING"; break;
        case LIBETUDE_LOG_ERROR:   level_str = "ERROR"; break;
        case LIBETUDE_LOG_FATAL:   level_str = "FATAL"; break;
        default:                   level_str = "UNKNOWN"; break;
    }

    fprintf(stderr, "[LibEtude %s] %s\n", level_str, message);
}

/**
 * @brief LibEtude 라이브러리를 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API int libetude_initialize() {
    if (g_libetude_initialized) {
        return LIBETUDE_SUCCESS; // 이미 초기화됨
    }

    // 기본 로그 콜백 설정
    if (!g_log_callback) {
        g_log_callback = default_log_callback;
    }

    // TODO: 전역 리소스 초기화
    // - 메모리 풀 초기화
    // - 커널 레지스트리 초기화
    // - 하드웨어 기능 감지
    // - 플랫폼별 초기화

    g_libetude_initialized = true;

    libetude_log(LIBETUDE_LOG_INFO, "LibEtude %s initialized successfully", LIBETUDE_VERSION_STRING);

    return LIBETUDE_SUCCESS;
}

/**
 * @brief LibEtude 라이브러리를 정리합니다
 */
LIBETUDE_API void libetude_finalize() {
    if (!g_libetude_initialized) {
        return; // 초기화되지 않음
    }

    libetude_log(LIBETUDE_LOG_INFO, "Finalizing LibEtude");

    // TODO: 전역 리소스 정리
    // - 메모리 풀 정리
    // - 커널 레지스트리 정리
    // - 플랫폼별 정리

    g_libetude_initialized = false;
    g_log_callback = NULL;
    g_log_user_data = NULL;
}

/**
 * @brief LibEtude가 초기화되었는지 확인합니다
 *
 * @return 초기화되었으면 true, 아니면 false
 */
LIBETUDE_API bool libetude_is_initialized() {
    return g_libetude_initialized;
}

/**
 * @brief 로그 메시지를 출력합니다
 *
 * @param level 로그 레벨
 * @param format 포맷 문자열
 * @param ... 가변 인수
 */
LIBETUDE_API void libetude_log(LibEtudeLogLevel level, const char* format, ...) {
    if (level < g_log_level || !g_log_callback) {
        return;
    }

    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    g_log_callback(level, message, g_log_user_data);
}

/**
 * @brief 로그 레벨을 설정합니다
 *
 * @param level 로그 레벨
 */
LIBETUDE_API void libetude_set_log_level(LibEtudeLogLevel level) {
    g_log_level = level;
}

/**
 * @brief 로그 콜백 함수를 설정합니다
 *
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 */
LIBETUDE_API void libetude_set_log_callback(LibEtudeLogCallback callback, void* user_data) {
    g_log_callback = callback ? callback : default_log_callback;
    g_log_user_data = user_data;
}

/**
 * @brief 현재 로그 레벨을 반환합니다
 *
 * @return 현재 로그 레벨
 */
LIBETUDE_API LibEtudeLogLevel libetude_get_log_level() {
    return g_log_level;
}