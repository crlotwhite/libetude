/**
 * @file error_handling.c
 * @brief LibEtude 오류 처리 및 로깅 시스템 구현
 *
 * 이 파일은 LibEtude의 오류 처리와 로깅 시스템을 구현합니다.
 * 스레드 안전한 오류 저장, 콜백 기반 오류 처리, 로그 레벨 필터링을 제공합니다.
 */

#include "libetude/error.h"
#include "libetude/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

// =============================================================================
// 내부 구조체 및 전역 변수
// =============================================================================

/**
 * @brief 스레드별 오류 정보 저장소
 */
typedef struct {
    ETError error;
    bool has_error;
} ThreadErrorInfo;

/**
 * @brief 전역 오류 및 로깅 상태
 */
typedef struct {
    // 오류 처리
    pthread_key_t error_key;
    bool error_key_created;
    ETErrorCallback error_callback;
    void* error_callback_data;
    pthread_mutex_t error_mutex;

    // 로깅
    ETLogLevel log_level;
    ETLogCallback log_callback;
    void* log_callback_data;
    pthread_mutex_t log_mutex;
    bool initialized;
} ErrorSystem;

static ErrorSystem g_error_system = {0};

// =============================================================================
// 내부 함수들
// =============================================================================

/**
 * @brief 현재 시간을 마이크로초로 가져옵니다.
 */
static uint64_t get_current_time_us(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER ui;
    GetSystemTimeAsFileTime(&ft);
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    return (ui.QuadPart - 116444736000000000ULL) / 10;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

/**
 * @brief 스레드별 오류 정보를 정리합니다.
 */
static void cleanup_thread_error(void* data) {
    ThreadErrorInfo* info = (ThreadErrorInfo*)data;
    if (info) {
        free(info);
    }
}

/**
 * @brief 현재 스레드의 오류 정보를 가져옵니다.
 */
static ThreadErrorInfo* get_thread_error_info(void) {
    if (!g_error_system.error_key_created) {
        return NULL;
    }

    ThreadErrorInfo* info = (ThreadErrorInfo*)pthread_getspecific(g_error_system.error_key);
    if (!info) {
        info = (ThreadErrorInfo*)calloc(1, sizeof(ThreadErrorInfo));
        if (info) {
            pthread_setspecific(g_error_system.error_key, info);
        }
    }
    return info;
}

/**
 * @brief 기본 로그 출력 함수
 */
static void default_log_output(ETLogLevel level, const char* message, void* user_data) {
    (void)user_data; // 사용하지 않는 매개변수

    FILE* output = (level >= ET_LOG_ERROR) ? stderr : stdout;

    // 시간 정보 추가
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(output, "[%s] [%s] %s\n", time_str, et_log_level_string(level), message);
    fflush(output);
}

// =============================================================================
// 오류 처리 함수 구현
// =============================================================================

const ETError* et_get_last_error(void) {
    ThreadErrorInfo* info = get_thread_error_info();
    if (!info || !info->has_error) {
        return NULL;
    }
    return &info->error;
}

const char* et_error_string(ETErrorCode code) {
    switch (code) {
        case LIBETUDE_SUCCESS: return "성공";
        case LIBETUDE_ERROR_INVALID_ARGUMENT: return "잘못된 인수";
        case LIBETUDE_ERROR_OUT_OF_MEMORY: return "메모리 부족";
        case LIBETUDE_ERROR_IO: return "입출력 오류";
        case LIBETUDE_ERROR_NOT_IMPLEMENTED: return "구현되지 않음";
        case LIBETUDE_ERROR_RUNTIME: return "런타임 오류";
        case LIBETUDE_ERROR_HARDWARE: return "하드웨어 오류";
        case LIBETUDE_ERROR_MODEL: return "모델 관련 오류";
        case LIBETUDE_ERROR_TIMEOUT: return "타임아웃";
        case LIBETUDE_ERROR_NOT_INITIALIZED: return "초기화되지 않음";
        case LIBETUDE_ERROR_ALREADY_INITIALIZED: return "이미 초기화됨";
        case LIBETUDE_ERROR_UNSUPPORTED: return "지원되지 않음";
        case LIBETUDE_ERROR_NOT_FOUND: return "찾을 수 없음";
        case LIBETUDE_ERROR_INVALID_STATE: return "잘못된 상태";
        case LIBETUDE_ERROR_BUFFER_FULL: return "버퍼 가득 참";
        case ET_ERROR_THREAD: return "스레드 관련 오류";
        case ET_ERROR_AUDIO: return "오디오 관련 오류";
        case ET_ERROR_COMPRESSION: return "압축 관련 오류";
        case ET_ERROR_QUANTIZATION: return "양자화 관련 오류";
        case ET_ERROR_GRAPH: return "그래프 관련 오류";
        case ET_ERROR_KERNEL: return "커널 관련 오류";
        case ET_ERROR_UNKNOWN: return "알 수 없는 오류";
        default: return "정의되지 않은 오류";
    }
}

void et_clear_error(void) {
    ThreadErrorInfo* info = get_thread_error_info();
    if (info) {
        info->has_error = false;
        memset(&info->error, 0, sizeof(ETError));
    }
}

void et_set_error(ETErrorCode code, const char* file, int line,
                  const char* function, const char* format, ...) {
    ThreadErrorInfo* info = get_thread_error_info();
    if (!info) {
        return; // 스레드 로컬 저장소 생성 실패
    }

    // 오류 정보 설정
    info->error.code = code;
    info->error.file = file;
    info->error.line = line;
    info->error.function = function;
    info->error.timestamp = get_current_time_us();
    info->has_error = true;

    // 메시지 포맷팅
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(info->error.message, sizeof(info->error.message), format, args);
        va_end(args);
    } else {
        strncpy(info->error.message, et_error_string(code), sizeof(info->error.message) - 1);
        info->error.message[sizeof(info->error.message) - 1] = '\0';
    }

    // 오류 콜백 호출
    pthread_mutex_lock(&g_error_system.error_mutex);
    if (g_error_system.error_callback) {
        g_error_system.error_callback(&info->error, g_error_system.error_callback_data);
    }
    pthread_mutex_unlock(&g_error_system.error_mutex);

    // 오류 로그 출력
    et_log(ET_LOG_ERROR, "오류 발생: %s (%s:%d in %s)",
           info->error.message, file, line, function);
}

void et_set_error_callback(ETErrorCallback callback, void* user_data) {
    pthread_mutex_lock(&g_error_system.error_mutex);
    g_error_system.error_callback = callback;
    g_error_system.error_callback_data = user_data;
    pthread_mutex_unlock(&g_error_system.error_mutex);
}

void et_clear_error_callback(void) {
    pthread_mutex_lock(&g_error_system.error_mutex);
    g_error_system.error_callback = NULL;
    g_error_system.error_callback_data = NULL;
    pthread_mutex_unlock(&g_error_system.error_mutex);
}

// =============================================================================
// 로깅 함수 구현
// =============================================================================

void et_log(ETLogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    et_log_va(level, format, args);
    va_end(args);
}

void et_log_va(ETLogLevel level, const char* format, va_list args) {
    // 로그 레벨 필터링
    if (level < g_error_system.log_level) {
        return;
    }

    // 메시지 포맷팅
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    message[sizeof(message) - 1] = '\0';

    // 로그 콜백 호출
    pthread_mutex_lock(&g_error_system.log_mutex);
    if (g_error_system.log_callback) {
        g_error_system.log_callback(level, message, g_error_system.log_callback_data);
    } else {
        // 기본 출력
        default_log_output(level, message, NULL);
    }
    pthread_mutex_unlock(&g_error_system.log_mutex);
}

void et_set_log_level(ETLogLevel level) {
    pthread_mutex_lock(&g_error_system.log_mutex);
    g_error_system.log_level = level;
    pthread_mutex_unlock(&g_error_system.log_mutex);
}

ETLogLevel et_get_log_level(void) {
    pthread_mutex_lock(&g_error_system.log_mutex);
    ETLogLevel level = g_error_system.log_level;
    pthread_mutex_unlock(&g_error_system.log_mutex);
    return level;
}

void et_set_log_callback(ETLogCallback callback, void* user_data) {
    pthread_mutex_lock(&g_error_system.log_mutex);
    g_error_system.log_callback = callback;
    g_error_system.log_callback_data = user_data;
    pthread_mutex_unlock(&g_error_system.log_mutex);
}

void et_clear_log_callback(void) {
    pthread_mutex_lock(&g_error_system.log_mutex);
    g_error_system.log_callback = NULL;
    g_error_system.log_callback_data = NULL;
    pthread_mutex_unlock(&g_error_system.log_mutex);
}

const char* et_log_level_string(ETLogLevel level) {
    switch (level) {
        case ET_LOG_DEBUG: return "DEBUG";
        case ET_LOG_INFO: return "INFO";
        case ET_LOG_WARNING: return "WARNING";
        case ET_LOG_ERROR: return "ERROR";
        case ET_LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

ETErrorCode et_init_logging(void) {
    if (g_error_system.initialized) {
        return LIBETUDE_SUCCESS;
    }

    // 뮤텍스 초기화
    if (pthread_mutex_init(&g_error_system.error_mutex, NULL) != 0) {
        return ET_ERROR_THREAD;
    }

    if (pthread_mutex_init(&g_error_system.log_mutex, NULL) != 0) {
        pthread_mutex_destroy(&g_error_system.error_mutex);
        return ET_ERROR_THREAD;
    }

    // 스레드 로컬 키 생성
    if (pthread_key_create(&g_error_system.error_key, cleanup_thread_error) != 0) {
        pthread_mutex_destroy(&g_error_system.error_mutex);
        pthread_mutex_destroy(&g_error_system.log_mutex);
        return ET_ERROR_THREAD;
    }
    g_error_system.error_key_created = true;

    // 기본값 설정
    g_error_system.log_level = ET_LOG_INFO;
    g_error_system.error_callback = NULL;
    g_error_system.error_callback_data = NULL;
    g_error_system.log_callback = NULL;
    g_error_system.log_callback_data = NULL;
    g_error_system.initialized = true;

    ET_LOG_INFO("LibEtude 오류 처리 및 로깅 시스템이 초기화되었습니다");

    return LIBETUDE_SUCCESS;
}

void et_cleanup_logging(void) {
    if (!g_error_system.initialized) {
        return;
    }

    ET_LOG_INFO("LibEtude 오류 처리 및 로깅 시스템을 정리합니다");

    // 스레드 로컬 키 삭제
    if (g_error_system.error_key_created) {
        pthread_key_delete(g_error_system.error_key);
        g_error_system.error_key_created = false;
    }

    // 뮤텍스 정리
    pthread_mutex_destroy(&g_error_system.error_mutex);
    pthread_mutex_destroy(&g_error_system.log_mutex);

    // 상태 초기화
    memset(&g_error_system, 0, sizeof(ErrorSystem));
}