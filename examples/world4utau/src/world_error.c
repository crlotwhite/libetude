/**
 * @file world_error.c
 * @brief WORLD4UTAU 전용 에러 처리 및 로깅 시스템 구현
 *
 * world4utau 예제 프로젝트의 에러 처리와 로깅 시스템을 구현합니다.
 * libetude의 기본 에러 시스템을 확장하여 WORLD 알고리즘 및 UTAU 인터페이스
 * 관련 에러 코드와 처리 기능을 제공합니다.
 */

#include "world_error.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

// =============================================================================
// 전역 변수
// =============================================================================

static bool g_world_logging_initialized = false;
static ETErrorCallback g_world_error_callback = NULL;
static void* g_world_error_callback_data = NULL;

// 로깅 시스템 통합을 위한 추가 변수들
static ETLogCallback g_world_log_callback = NULL;
static void* g_world_log_callback_data = NULL;
static ETLogLevel g_world_min_log_level = ET_LOG_INFO;
static bool g_world_log_timestamps = true;
static bool g_world_log_thread_info = false;

// 로그 필터링을 위한 카테고리별 활성화 상태
static bool g_world_category_enabled[7] = {
    true,  // WORLD_LOG_UTAU_INTERFACE
    true,  // WORLD_LOG_ANALYSIS
    true,  // WORLD_LOG_SYNTHESIS
    true,  // WORLD_LOG_AUDIO_IO
    true,  // WORLD_LOG_CACHE
    true,  // WORLD_LOG_MEMORY
    true   // WORLD_LOG_PERFORMANCE
};

// =============================================================================
// 전방 선언
// =============================================================================

static bool get_current_time_string(char* buffer, size_t buffer_size);
static bool is_world_error_code(int error_code);
static int get_error_severity(WorldErrorCode error_code);

// =============================================================================
// 에러 문자열 테이블
// =============================================================================

/**
 * @brief WORLD 에러 코드에 대한 한국어 메시지 테이블
 */
static const struct {
    WorldErrorCode code;
    const char* message;
} world_error_messages[] = {
    // UTAU 인터페이스 관련 에러
    {WORLD_ERROR_UTAU_INVALID_PARAMS, "잘못된 UTAU 파라미터입니다"},
    {WORLD_ERROR_UTAU_PARSE_FAILED, "UTAU 파라미터 파싱에 실패했습니다"},
    {WORLD_ERROR_UTAU_FILE_NOT_FOUND, "UTAU 파일을 찾을 수 없습니다"},
    {WORLD_ERROR_UTAU_INVALID_FORMAT, "잘못된 UTAU 파일 형식입니다"},
    {WORLD_ERROR_UTAU_PITCH_RANGE, "피치 값이 허용 범위를 벗어났습니다"},
    {WORLD_ERROR_UTAU_VELOCITY_RANGE, "벨로시티 값이 허용 범위를 벗어났습니다"},
    {WORLD_ERROR_UTAU_TIMING_INVALID, "잘못된 타이밍 파라미터입니다"},

    // WORLD 분석 관련 에러
    {WORLD_ERROR_ANALYSIS_FAILED, "WORLD 분석에 실패했습니다"},
    {WORLD_ERROR_F0_EXTRACTION_FAILED, "F0 추출에 실패했습니다"},
    {WORLD_ERROR_SPECTRUM_ANALYSIS_FAILED, "스펙트럼 분석에 실패했습니다"},
    {WORLD_ERROR_APERIODICITY_FAILED, "비주기성 분석에 실패했습니다"},
    {WORLD_ERROR_INVALID_AUDIO_DATA, "잘못된 오디오 데이터입니다"},
    {WORLD_ERROR_AUDIO_TOO_SHORT, "오디오가 너무 짧습니다"},
    {WORLD_ERROR_AUDIO_TOO_LONG, "오디오가 너무 깁니다"},
    {WORLD_ERROR_INVALID_SAMPLE_RATE, "지원되지 않는 샘플링 레이트입니다"},

    // WORLD 합성 관련 에러
    {WORLD_ERROR_SYNTHESIS_FAILED, "WORLD 합성에 실패했습니다"},
    {WORLD_ERROR_INVALID_F0_DATA, "잘못된 F0 데이터입니다"},
    {WORLD_ERROR_INVALID_SPECTRUM_DATA, "잘못된 스펙트럼 데이터입니다"},
    {WORLD_ERROR_INVALID_APERIODICITY_DATA, "잘못된 비주기성 데이터입니다"},
    {WORLD_ERROR_PARAMETER_MISMATCH, "파라미터가 일치하지 않습니다"},
    {WORLD_ERROR_SYNTHESIS_BUFFER_OVERFLOW, "합성 버퍼 오버플로우가 발생했습니다"},

    // 오디오 I/O 관련 에러
    {WORLD_ERROR_AUDIO_FILE_READ, "오디오 파일 읽기에 실패했습니다"},
    {WORLD_ERROR_AUDIO_FILE_WRITE, "오디오 파일 쓰기에 실패했습니다"},
    {WORLD_ERROR_UNSUPPORTED_AUDIO_FORMAT, "지원되지 않는 오디오 형식입니다"},
    {WORLD_ERROR_AUDIO_FILE_CORRUPT, "손상된 오디오 파일입니다"},
    {WORLD_ERROR_AUDIO_BUFFER_UNDERRUN, "오디오 버퍼 언더런이 발생했습니다"},
    {WORLD_ERROR_AUDIO_DEVICE_ERROR, "오디오 장치 에러가 발생했습니다"},

    // 캐시 관련 에러
    {WORLD_ERROR_CACHE_READ_FAILED, "캐시 읽기에 실패했습니다"},
    {WORLD_ERROR_CACHE_WRITE_FAILED, "캐시 쓰기에 실패했습니다"},
    {WORLD_ERROR_CACHE_INVALID_DATA, "잘못된 캐시 데이터입니다"},
    {WORLD_ERROR_CACHE_VERSION_MISMATCH, "캐시 버전이 일치하지 않습니다"},
    {WORLD_ERROR_CACHE_CORRUPTION, "캐시 데이터가 손상되었습니다"},

    // 메모리 관리 관련 에러
    {WORLD_ERROR_MEMORY_POOL_EXHAUSTED, "메모리 풀이 고갈되었습니다"},
    {WORLD_ERROR_MEMORY_ALIGNMENT, "메모리 정렬 오류가 발생했습니다"},
    {WORLD_ERROR_MEMORY_LEAK_DETECTED, "메모리 누수가 감지되었습니다"},

    // 성능 관련 에러
    {WORLD_ERROR_PERFORMANCE_TIMEOUT, "성능 타임아웃이 발생했습니다"},
    {WORLD_ERROR_REALTIME_CONSTRAINT, "실시간 제약을 위반했습니다"},
    {WORLD_ERROR_RESOURCE_EXHAUSTED, "리소스가 고갈되었습니다"}
};

/**
 * @brief 로그 카테고리 문자열 테이블
 */
static const char* world_log_category_strings[] = {
    "UTAU_INTERFACE",
    "ANALYSIS",
    "SYNTHESIS",
    "AUDIO_IO",
    "CACHE",
    "MEMORY",
    "PERFORMANCE"
};

// =============================================================================
// 에러 처리 함수 구현
// =============================================================================

const char* world_get_error_string(WorldErrorCode error_code)
{
    // WORLD 전용 에러 코드 검색
    for (size_t i = 0; i < sizeof(world_error_messages) / sizeof(world_error_messages[0]); i++) {
        if (world_error_messages[i].code == error_code) {
            return world_error_messages[i].message;
        }
    }

    // libetude 기본 에러 코드로 폴백
    return et_error_string((ETErrorCode)error_code);
}

void world_set_error(WorldErrorCode error_code, const char* file, int line,
                     const char* function, const char* format, ...)
{
    // libetude 에러 시스템을 사용하여 에러 설정
    va_list args;
    va_start(args, format);

    char message[256];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // libetude 에러 시스템에 에러 설정
    et_set_error((ETErrorCode)error_code, file, line, function, "%s", message);

    // WORLD 전용 에러 콜백 호출
    if (g_world_error_callback) {
        const ETError* error = et_get_last_error();
        if (error) {
            g_world_error_callback(error, g_world_error_callback_data);
        }
    }
}

const ETError* world_get_last_error(void)
{
    return et_get_last_error();
}

void world_clear_error(void)
{
    et_clear_error();
}

void world_set_error_callback(ETErrorCallback callback, void* user_data)
{
    g_world_error_callback = callback;
    g_world_error_callback_data = user_data;

    // libetude 에러 콜백도 설정
    et_set_error_callback(callback, user_data);
}

// =============================================================================
// 로깅 함수 구현
// =============================================================================

void world_log(WorldLogCategory category, ETLogLevel level,
               const char* format, ...)
{
    if (!g_world_logging_initialized) {
        return;
    }

    va_list args;
    va_start(args, format);

    // 카테고리 정보를 포함한 메시지 생성
    char formatted_message[512];
    const char* category_str = world_log_category_string(category);

    snprintf(formatted_message, sizeof(formatted_message),
             "[WORLD:%s] %s", category_str, format);

    // libetude 로깅 시스템 사용
    et_log_va(level, formatted_message, args);

    va_end(args);
}

const char* world_log_category_string(WorldLogCategory category)
{
    if (category >= 0 && category < sizeof(world_log_category_strings) / sizeof(world_log_category_strings[0])) {
        return world_log_category_strings[category];
    }
    return "UNKNOWN";
}

ETResult world_init_logging(void)
{
    if (g_world_logging_initialized) {
        return ET_SUCCESS;
    }

    // libetude 로깅 시스템 초기화
    ETResult result = et_init_logging();
    if (result != ET_SUCCESS) {
        return result;
    }

    g_world_logging_initialized = true;

    // 초기화 완료 로그
    WORLD_LOG_INFO(WORLD_LOG_UTAU_INTERFACE, "WORLD4UTAU 로깅 시스템이 초기화되었습니다");

    return ET_SUCCESS;
}

void world_cleanup_logging(void)
{
    if (!g_world_logging_initialized) {
        return;
    }

    // 정리 시작 로그
    WORLD_LOG_INFO(WORLD_LOG_UTAU_INTERFACE, "WORLD4UTAU 로깅 시스템을 정리합니다");

    // 콜백 정리
    g_world_error_callback = NULL;
    g_world_error_callback_data = NULL;
    g_world_log_callback = NULL;
    g_world_log_callback_data = NULL;

    // libetude 로깅 시스템 정리
    et_cleanup_logging();

    g_world_logging_initialized = false;
}

// =============================================================================
// 로깅 시스템 통합 추가 함수들
// =============================================================================

/**
 * @brief WORLD 로그 콜백을 설정합니다.
 *
 * @param callback 로그 콜백 함수
 * @param user_data 사용자 데이터
 */
void world_set_log_callback(ETLogCallback callback, void* user_data)
{
    g_world_log_callback = callback;
    g_world_log_callback_data = user_data;

    // libetude 로그 콜백도 설정
    et_set_log_callback(callback, user_data);
}

/**
 * @brief WORLD 로그 콜백을 제거합니다.
 */
void world_clear_log_callback(void)
{
    g_world_log_callback = NULL;
    g_world_log_callback_data = NULL;

    // libetude 로그 콜백도 제거
    et_clear_log_callback();
}

/**
 * @brief WORLD 로그 레벨을 설정합니다.
 *
 * @param level 최소 로그 레벨
 */
void world_set_log_level(ETLogLevel level)
{
    g_world_min_log_level = level;

    // libetude 로그 레벨도 설정
    et_set_log_level(level);
}

/**
 * @brief 현재 WORLD 로그 레벨을 가져옵니다.
 *
 * @return 현재 로그 레벨
 */
ETLogLevel world_get_log_level(void)
{
    return g_world_min_log_level;
}

/**
 * @brief 특정 카테고리의 로그를 활성화/비활성화합니다.
 *
 * @param category 로그 카테고리
 * @param enabled 활성화 여부
 */
void world_set_log_category_enabled(WorldLogCategory category, bool enabled)
{
    if (category >= 0 && category < 7) {
        g_world_category_enabled[category] = enabled;
    }
}

/**
 * @brief 특정 카테고리의 로그 활성화 상태를 확인합니다.
 *
 * @param category 로그 카테고리
 * @return 활성화 상태
 */
bool world_is_log_category_enabled(WorldLogCategory category)
{
    if (category >= 0 && category < 7) {
        return g_world_category_enabled[category];
    }
    return false;
}

/**
 * @brief 로그에 타임스탬프 포함 여부를 설정합니다.
 *
 * @param enabled 타임스탬프 포함 여부
 */
void world_set_log_timestamps(bool enabled)
{
    g_world_log_timestamps = enabled;
}

/**
 * @brief 로그에 스레드 정보 포함 여부를 설정합니다.
 *
 * @param enabled 스레드 정보 포함 여부
 */
void world_set_log_thread_info(bool enabled)
{
    g_world_log_thread_info = enabled;
}

/**
 * @brief 향상된 WORLD 로그 함수 (필터링 및 포맷팅 포함)
 *
 * @param category 로그 카테고리
 * @param level 로그 레벨
 * @param format 메시지 포맷 문자열
 * @param ... 포맷 인수들
 */
void world_log_enhanced(WorldLogCategory category, ETLogLevel level,
                       const char* format, ...)
{
    // 초기화 확인
    if (!g_world_logging_initialized) {
        return;
    }

    // 레벨 필터링
    if (level < g_world_min_log_level) {
        return;
    }

    // 카테고리 필터링
    if (!world_is_log_category_enabled(category)) {
        return;
    }

    va_list args;
    va_start(args, format);

    // 메시지 버퍼 준비
    char message_buffer[1024];
    char final_message[1200];

    // 기본 메시지 포맷팅
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);

    // 추가 정보 포함한 최종 메시지 생성
    char prefix[200] = "";

    // 타임스탬프 추가
    if (g_world_log_timestamps) {
        char time_str[64];
        if (get_current_time_string(time_str, sizeof(time_str))) {
            snprintf(prefix, sizeof(prefix), "[%s] ", time_str);
        }
    }

    // 스레드 정보 추가 (간단한 구현)
    if (g_world_log_thread_info) {
        char thread_info[32];
        snprintf(thread_info, sizeof(thread_info), "[TID:%p] ", (void*)&category);
        strncat(prefix, thread_info, sizeof(prefix) - strlen(prefix) - 1);
    }

    // 카테고리 정보 추가
    const char* category_str = world_log_category_string(category);
    char category_info[64];
    snprintf(category_info, sizeof(category_info), "[WORLD:%s] ", category_str);
    strncat(prefix, category_info, sizeof(prefix) - strlen(prefix) - 1);

    // 최종 메시지 조합
    snprintf(final_message, sizeof(final_message), "%s%s", prefix, message_buffer);

    // libetude 로깅 시스템으로 출력
    et_log(level, "%s", final_message);

    // WORLD 전용 로그 콜백 호출
    if (g_world_log_callback) {
        g_world_log_callback(level, final_message, g_world_log_callback_data);
    }
}

/**
 * @brief 성능 측정을 위한 로그 함수
 *
 * @param category 로그 카테고리
 * @param operation_name 작업 이름
 * @param duration_ms 소요 시간 (밀리초)
 * @param additional_info 추가 정보
 */
void world_log_performance(WorldLogCategory category, const char* operation_name,
                          double duration_ms, const char* additional_info)
{
    if (additional_info && strlen(additional_info) > 0) {
        world_log_enhanced(category, ET_LOG_INFO,
                          "성능: %s 완료 (%.2fms) - %s",
                          operation_name, duration_ms, additional_info);
    } else {
        world_log_enhanced(category, ET_LOG_INFO,
                          "성능: %s 완료 (%.2fms)",
                          operation_name, duration_ms);
    }
}

/**
 * @brief 메모리 사용량 로그 함수
 *
 * @param category 로그 카테고리
 * @param operation_name 작업 이름
 * @param memory_bytes 메모리 사용량 (바이트)
 * @param is_allocation 할당이면 true, 해제면 false
 */
void world_log_memory(WorldLogCategory category, const char* operation_name,
                     size_t memory_bytes, bool is_allocation)
{
    const char* action = is_allocation ? "할당" : "해제";
    double memory_mb = (double)memory_bytes / (1024.0 * 1024.0);

    if (memory_mb >= 1.0) {
        world_log_enhanced(category, ET_LOG_DEBUG,
                          "메모리: %s %s (%.2f MB)",
                          operation_name, action, memory_mb);
    } else {
        double memory_kb = (double)memory_bytes / 1024.0;
        world_log_enhanced(category, ET_LOG_DEBUG,
                          "메모리: %s %s (%.2f KB)",
                          operation_name, action, memory_kb);
    }
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

/**
 * @brief 현재 시간을 문자열로 반환합니다.
 *
 * @param buffer 시간 문자열을 저장할 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공 시 true, 실패 시 false
 */
static bool get_current_time_string(char* buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    if (tm_info == NULL) {
        return false;
    }

    size_t result = strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info);
    return result > 0;
}

/**
 * @brief 에러 코드가 WORLD 전용 에러인지 확인합니다.
 *
 * @param error_code 확인할 에러 코드
 * @return WORLD 전용 에러이면 true, 아니면 false
 */
bool is_world_error_code(int error_code)
{
    return error_code >= -2699 && error_code <= -2000;
}

/**
 * @brief 에러 심각도를 반환합니다.
 *
 * @param error_code 에러 코드
 * @return 에러 심각도 (0: 낮음, 1: 보통, 2: 높음, 3: 치명적)
 */
int get_error_severity(WorldErrorCode error_code)
{
    // 치명적 에러들
    if (error_code == WORLD_ERROR_MEMORY_POOL_EXHAUSTED ||
        error_code == WORLD_ERROR_RESOURCE_EXHAUSTED ||
        error_code == WORLD_ERROR_AUDIO_DEVICE_ERROR) {
        return 3;
    }

    // 높은 심각도 에러들
    if (error_code >= WORLD_ERROR_SYNTHESIS_FAILED && error_code <= WORLD_ERROR_SYNTHESIS_BUFFER_OVERFLOW) {
        return 2;
    }

    // 보통 심각도 에러들
    if (error_code >= WORLD_ERROR_ANALYSIS_FAILED && error_code <= WORLD_ERROR_INVALID_SAMPLE_RATE) {
        return 2;
    }

    // 낮은 심각도 에러들 (주로 파라미터 관련)
    if (error_code >= WORLD_ERROR_UTAU_INVALID_PARAMS && error_code <= WORLD_ERROR_UTAU_TIMING_INVALID) {
        return 1;
    }

    return 0; // 기본값
}

/**
 * @brief 디버그 모드에서 상세한 에러 정보를 출력합니다.
 *
 * @param error 에러 정보
 */
void world_debug_print_error(const ETError* error)
{
    if (!error) {
        return;
    }

    char time_str[64];
    if (!get_current_time_string(time_str, sizeof(time_str))) {
        strcpy(time_str, "Unknown Time");
    }

    printf("=== WORLD4UTAU 에러 정보 ===\n");
    printf("시간: %s\n", time_str);
    printf("에러 코드: %d\n", error->code);
    printf("에러 메시지: %s\n", error->message);
    printf("파일: %s\n", error->file ? error->file : "Unknown");
    printf("라인: %d\n", error->line);
    printf("함수: %s\n", error->function ? error->function : "Unknown");

    if (is_world_error_code(error->code)) {
        printf("WORLD 에러 설명: %s\n", world_get_error_string((WorldErrorCode)error->code));
        printf("심각도: %d\n", get_error_severity((WorldErrorCode)error->code));
    }

    printf("========================\n");
}