/* LibEtude Windows 특화 오류 처리 시스템 구현 */
/* Copyright (c) 2025 LibEtude Project */

#include "libetude/platform/windows_error.h"
#include "libetude/platform/windows.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* 최대 등록 가능한 폴백 콜백 수 */
#define MAX_FALLBACK_CALLBACKS 64

/* 폴백 콜백 등록 정보 */
typedef struct {
    ETWindowsErrorCode error_code;
    ETWindowsFallbackCallback callback;
    void* context;
    ETWindowsFallbackStrategy strategy;
} ETWindowsFallbackEntry;

/* Windows 오류 처리 시스템 전역 상태 */
static struct {
    bool initialized;
    ETWindowsErrorCallback error_callback;
    void* error_callback_user_data;
    ETWindowsErrorInfo last_error;
    ETWindowsErrorStatistics statistics;
    ETWindowsDegradationState degradation_state;

    /* 폴백 콜백 관리 */
    ETWindowsFallbackEntry fallback_entries[MAX_FALLBACK_CALLBACKS];
    int fallback_count;

    /* 로깅 설정 */
    bool logging_enabled;
    FILE* log_file;
    char log_file_path[MAX_PATH];

    /* 동기화 */
    CRITICAL_SECTION error_lock;
} g_windows_error = { false };

/* 오류 메시지 테이블 (영어) */
static const char* g_error_messages_en[] = {
    /* 오디오 관련 오류 */
    [ET_WINDOWS_ERROR_WASAPI_INIT_FAILED & 0xFFF] = "Failed to initialize WASAPI audio system",
    [ET_WINDOWS_ERROR_WASAPI_DEVICE_NOT_FOUND & 0xFFF] = "WASAPI audio device not found",
    [ET_WINDOWS_ERROR_WASAPI_FORMAT_NOT_SUPPORTED & 0xFFF] = "Audio format not supported by WASAPI device",
    [ET_WINDOWS_ERROR_WASAPI_EXCLUSIVE_MODE_FAILED & 0xFFF] = "Failed to enable WASAPI exclusive mode",
    [ET_WINDOWS_ERROR_WASAPI_BUFFER_UNDERRUN & 0xFFF] = "WASAPI audio buffer underrun detected",
    [ET_WINDOWS_ERROR_WASAPI_DEVICE_DISCONNECTED & 0xFFF] = "WASAPI audio device disconnected",
    [ET_WINDOWS_ERROR_DIRECTSOUND_INIT_FAILED & 0xFFF] = "Failed to initialize DirectSound",
    [ET_WINDOWS_ERROR_DIRECTSOUND_BUFFER_LOST & 0xFFF] = "DirectSound buffer lost",
    [ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED & 0xFFF] = "DirectSound fallback failed",
    [ET_WINDOWS_ERROR_AUDIO_SESSION_EXPIRED & 0xFFF] = "Audio session expired",
    [ET_WINDOWS_ERROR_AUDIO_DEVICE_CHANGED & 0xFFF] = "Audio device changed",

    /* 보안 관련 오류 */
    [ET_WINDOWS_ERROR_DEP_NOT_SUPPORTED & 0xFFF] = "Data Execution Prevention (DEP) not supported",
    [ET_WINDOWS_ERROR_DEP_VIOLATION & 0xFFF] = "Data Execution Prevention (DEP) violation",
    [ET_WINDOWS_ERROR_ASLR_NOT_SUPPORTED & 0xFFF] = "Address Space Layout Randomization (ASLR) not supported",
    [ET_WINDOWS_ERROR_ASLR_ALLOCATION_FAILED & 0xFFF] = "ASLR-compatible memory allocation failed",
    [ET_WINDOWS_ERROR_UAC_INSUFFICIENT_PRIVILEGES & 0xFFF] = "Insufficient UAC privileges",
    [ET_WINDOWS_ERROR_UAC_ELEVATION_REQUIRED & 0xFFF] = "UAC elevation required",
    [ET_WINDOWS_ERROR_SECURITY_CHECK_FAILED & 0xFFF] = "Security check failed",
    [ET_WINDOWS_ERROR_PRIVILEGE_NOT_HELD & 0xFFF] = "Required privilege not held",

    /* 성능 관련 오류 */
    [ET_WINDOWS_ERROR_SIMD_NOT_SUPPORTED & 0xFFF] = "SIMD instructions not supported",
    [ET_WINDOWS_ERROR_AVX_NOT_AVAILABLE & 0xFFF] = "AVX instructions not available",
    [ET_WINDOWS_ERROR_AVX2_NOT_AVAILABLE & 0xFFF] = "AVX2 instructions not available",
    [ET_WINDOWS_ERROR_AVX512_NOT_AVAILABLE & 0xFFF] = "AVX-512 instructions not available",
    [ET_WINDOWS_ERROR_THREAD_POOL_CREATION_FAILED & 0xFFF] = "Thread pool creation failed",
    [ET_WINDOWS_ERROR_THREAD_POOL_SUBMISSION_FAILED & 0xFFF] = "Thread pool task submission failed",
    [ET_WINDOWS_ERROR_LARGE_PAGE_PRIVILEGE_DENIED & 0xFFF] = "Large page privilege denied",
    [ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED & 0xFFF] = "Large page allocation failed",
    [ET_WINDOWS_ERROR_PERFORMANCE_COUNTER_FAILED & 0xFFF] = "Performance counter access failed",

    /* 개발 도구 관련 오류 */
    [ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED & 0xFFF] = "ETW provider registration failed",
    [ET_WINDOWS_ERROR_ETW_EVENT_WRITE_FAILED & 0xFFF] = "ETW event write failed",
    [ET_WINDOWS_ERROR_PDB_GENERATION_FAILED & 0xFFF] = "PDB file generation failed",
    [ET_WINDOWS_ERROR_DEBUG_INFO_UNAVAILABLE & 0xFFF] = "Debug information unavailable",
    [ET_WINDOWS_ERROR_PROFILER_INIT_FAILED & 0xFFF] = "Profiler initialization failed",

    /* 플랫폼 관련 오류 */
    [ET_WINDOWS_ERROR_UNSUPPORTED_WINDOWS_VERSION & 0xFFF] = "Unsupported Windows version",
    [ET_WINDOWS_ERROR_REQUIRED_DLL_NOT_FOUND & 0xFFF] = "Required DLL not found",
    [ET_WINDOWS_ERROR_COM_INIT_FAILED & 0xFFF] = "COM initialization failed",
    [ET_WINDOWS_ERROR_REGISTRY_ACCESS_DENIED & 0xFFF] = "Registry access denied",
    [ET_WINDOWS_ERROR_SERVICE_UNAVAILABLE & 0xFFF] = "Windows service unavailable"
};

/* 오류 메시지 테이블 (한국어) */
static const char* g_error_messages_ko[] = {
    /* 오디오 관련 오류 */
    [ET_WINDOWS_ERROR_WASAPI_INIT_FAILED & 0xFFF] = "WASAPI 오디오 시스템 초기화 실패",
    [ET_WINDOWS_ERROR_WASAPI_DEVICE_NOT_FOUND & 0xFFF] = "WASAPI 오디오 장치를 찾을 수 없음",
    [ET_WINDOWS_ERROR_WASAPI_FORMAT_NOT_SUPPORTED & 0xFFF] = "WASAPI 장치에서 지원하지 않는 오디오 형식",
    [ET_WINDOWS_ERROR_WASAPI_EXCLUSIVE_MODE_FAILED & 0xFFF] = "WASAPI 독점 모드 활성화 실패",
    [ET_WINDOWS_ERROR_WASAPI_BUFFER_UNDERRUN & 0xFFF] = "WASAPI 오디오 버퍼 언더런 감지",
    [ET_WINDOWS_ERROR_WASAPI_DEVICE_DISCONNECTED & 0xFFF] = "WASAPI 오디오 장치 연결 해제됨",
    [ET_WINDOWS_ERROR_DIRECTSOUND_INIT_FAILED & 0xFFF] = "DirectSound 초기화 실패",
    [ET_WINDOWS_ERROR_DIRECTSOUND_BUFFER_LOST & 0xFFF] = "DirectSound 버퍼 손실",
    [ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED & 0xFFF] = "DirectSound 폴백 실패",
    [ET_WINDOWS_ERROR_AUDIO_SESSION_EXPIRED & 0xFFF] = "오디오 세션 만료",
    [ET_WINDOWS_ERROR_AUDIO_DEVICE_CHANGED & 0xFFF] = "오디오 장치 변경됨",

    /* 보안 관련 오류 */
    [ET_WINDOWS_ERROR_DEP_NOT_SUPPORTED & 0xFFF] = "데이터 실행 방지(DEP) 지원되지 않음",
    [ET_WINDOWS_ERROR_DEP_VIOLATION & 0xFFF] = "데이터 실행 방지(DEP) 위반",
    [ET_WINDOWS_ERROR_ASLR_NOT_SUPPORTED & 0xFFF] = "주소 공간 배치 임의화(ASLR) 지원되지 않음",
    [ET_WINDOWS_ERROR_ASLR_ALLOCATION_FAILED & 0xFFF] = "ASLR 호환 메모리 할당 실패",
    [ET_WINDOWS_ERROR_UAC_INSUFFICIENT_PRIVILEGES & 0xFFF] = "UAC 권한 부족",
    [ET_WINDOWS_ERROR_UAC_ELEVATION_REQUIRED & 0xFFF] = "UAC 권한 상승 필요",
    [ET_WINDOWS_ERROR_SECURITY_CHECK_FAILED & 0xFFF] = "보안 검사 실패",
    [ET_WINDOWS_ERROR_PRIVILEGE_NOT_HELD & 0xFFF] = "필요한 권한이 없음",

    /* 성능 관련 오류 */
    [ET_WINDOWS_ERROR_SIMD_NOT_SUPPORTED & 0xFFF] = "SIMD 명령어 지원되지 않음",
    [ET_WINDOWS_ERROR_AVX_NOT_AVAILABLE & 0xFFF] = "AVX 명령어 사용 불가",
    [ET_WINDOWS_ERROR_AVX2_NOT_AVAILABLE & 0xFFF] = "AVX2 명령어 사용 불가",
    [ET_WINDOWS_ERROR_AVX512_NOT_AVAILABLE & 0xFFF] = "AVX-512 명령어 사용 불가",
    [ET_WINDOWS_ERROR_THREAD_POOL_CREATION_FAILED & 0xFFF] = "스레드 풀 생성 실패",
    [ET_WINDOWS_ERROR_THREAD_POOL_SUBMISSION_FAILED & 0xFFF] = "스레드 풀 작업 제출 실패",
    [ET_WINDOWS_ERROR_LARGE_PAGE_PRIVILEGE_DENIED & 0xFFF] = "Large Page 권한 거부됨",
    [ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED & 0xFFF] = "Large Page 할당 실패",
    [ET_WINDOWS_ERROR_PERFORMANCE_COUNTER_FAILED & 0xFFF] = "성능 카운터 접근 실패",

    /* 개발 도구 관련 오류 */
    [ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED & 0xFFF] = "ETW 프로바이더 등록 실패",
    [ET_WINDOWS_ERROR_ETW_EVENT_WRITE_FAILED & 0xFFF] = "ETW 이벤트 쓰기 실패",
    [ET_WINDOWS_ERROR_PDB_GENERATION_FAILED & 0xFFF] = "PDB 파일 생성 실패",
    [ET_WINDOWS_ERROR_DEBUG_INFO_UNAVAILABLE & 0xFFF] = "디버그 정보 사용 불가",
    [ET_WINDOWS_ERROR_PROFILER_INIT_FAILED & 0xFFF] = "프로파일러 초기화 실패",

    /* 플랫폼 관련 오류 */
    [ET_WINDOWS_ERROR_UNSUPPORTED_WINDOWS_VERSION & 0xFFF] = "지원되지 않는 Windows 버전",
    [ET_WINDOWS_ERROR_REQUIRED_DLL_NOT_FOUND & 0xFFF] = "필수 DLL을 찾을 수 없음",
    [ET_WINDOWS_ERROR_COM_INIT_FAILED & 0xFFF] = "COM 초기화 실패",
    [ET_WINDOWS_ERROR_REGISTRY_ACCESS_DENIED & 0xFFF] = "레지스트리 접근 거부됨",
    [ET_WINDOWS_ERROR_SERVICE_UNAVAILABLE & 0xFFF] = "Windows 서비스 사용 불가"
};

/* 내부 함수 선언 */
static void _log_error_to_file(const ETWindowsErrorInfo* error_info);
static void _update_error_statistics(ETWindowsErrorCode error_code, ETWindowsErrorSeverity severity);
static ETWindowsErrorSeverity _determine_error_severity(ETWindowsErrorCode error_code);
static ETWindowsFallbackStrategy _get_default_fallback_strategy(ETWindowsErrorCode error_code);

/* 오류 처리 시스템 초기화 */
ETResult et_windows_error_init(void) {
    if (g_windows_error.initialized) {
        return ET_ERROR_ALREADY_INITIALIZED;
    }

    /* Critical Section 초기화 */
    InitializeCriticalSection(&g_windows_error.error_lock);

    /* 전역 상태 초기화 */
    memset(&g_windows_error.last_error, 0, sizeof(g_windows_error.last_error));
    memset(&g_windows_error.statistics, 0, sizeof(g_windows_error.statistics));
    memset(&g_windows_error.degradation_state, 0, sizeof(g_windows_error.degradation_state));

    g_windows_error.error_callback = NULL;
    g_windows_error.error_callback_user_data = NULL;
    g_windows_error.fallback_count = 0;
    g_windows_error.logging_enabled = false;
    g_windows_error.log_file = NULL;

    /* 성능 스케일 팩터 초기값 설정 */
    g_windows_error.degradation_state.performance_scale_factor = 1.0f;

    g_windows_error.initialized = true;

    return ET_SUCCESS;
}

/* 오류 처리 시스템 정리 */
void et_windows_error_finalize(void) {
    if (!g_windows_error.initialized) {
        return;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 로그 파일 닫기 */
    if (g_windows_error.log_file) {
        fclose(g_windows_error.log_file);
        g_windows_error.log_file = NULL;
    }

    /* 상태 초기화 */
    g_windows_error.initialized = false;
    g_windows_error.error_callback = NULL;
    g_windows_error.error_callback_user_data = NULL;
    g_windows_error.fallback_count = 0;
    g_windows_error.logging_enabled = false;

    LeaveCriticalSection(&g_windows_error.error_lock);
    DeleteCriticalSection(&g_windows_error.error_lock);
}

/* 오류 보고 및 처리 */
ETResult et_windows_report_error(ETWindowsErrorCode error_code,
                                DWORD win32_error,
                                HRESULT hresult,
                                const char* module_name,
                                const char* function_name,
                                int line_number,
                                const char* format, ...) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 오류 정보 구성 */
    ETWindowsErrorInfo* error_info = &g_windows_error.last_error;
    memset(error_info, 0, sizeof(ETWindowsErrorInfo));

    error_info->error_code = error_code;
    error_info->win32_error = win32_error;
    error_info->hresult = hresult;
    error_info->severity = _determine_error_severity(error_code);
    error_info->fallback = _get_default_fallback_strategy(error_code);
    error_info->module_name = module_name;
    error_info->function_name = function_name;
    error_info->line_number = line_number;

    /* 현재 시간 기록 */
    GetSystemTime(&error_info->timestamp);

    /* 사용자 정의 메시지 포맷팅 */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(error_info->technical_details, sizeof(error_info->technical_details), format, args);
        va_end(args);
    }

    /* 기본 오류 메시지 설정 */
    const char* base_message = et_windows_get_error_message(error_code);
    if (base_message) {
        strncpy(error_info->message, base_message, sizeof(error_info->message) - 1);
        error_info->message[sizeof(error_info->message) - 1] = '\0';
    }

    /* Win32 오류 정보 추가 */
    if (win32_error != 0) {
        char win32_msg[256];
        DWORD result = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, win32_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            win32_msg, sizeof(win32_msg), NULL);

        if (result > 0) {
            strncat(error_info->technical_details, " Win32 Error: ",
                   sizeof(error_info->technical_details) - strlen(error_info->technical_details) - 1);
            strncat(error_info->technical_details, win32_msg,
                   sizeof(error_info->technical_details) - strlen(error_info->technical_details) - 1);
        }
    }

    /* 통계 업데이트 */
    _update_error_statistics(error_code, error_info->severity);

    /* 로그 파일에 기록 */
    if (g_windows_error.logging_enabled) {
        _log_error_to_file(error_info);
    }

    /* 오류 콜백 호출 */
    if (g_windows_error.error_callback) {
        g_windows_error.error_callback(error_info, g_windows_error.error_callback_user_data);
    }

    /* 폴백 실행 */
    if (error_info->fallback != ET_WINDOWS_FALLBACK_NONE) {
        et_windows_execute_fallback(error_code);
    }

    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 마지막 오류 정보 조회 */
ETResult et_windows_get_last_error_info(ETWindowsErrorInfo* error_info) {
    if (!error_info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);
    *error_info = g_windows_error.last_error;
    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 오류 메시지 조회 (영어) */
const char* et_windows_get_error_message(ETWindowsErrorCode error_code) {
    int index = error_code & 0xFFF;
    if (index < 0 || index >= sizeof(g_error_messages_en) / sizeof(g_error_messages_en[0])) {
        return "Unknown Windows error";
    }

    const char* message = g_error_messages_en[index];
    return message ? message : "Unknown Windows error";
}

/* 오류 메시지 조회 (한국어) */
const char* et_windows_get_error_message_korean(ETWindowsErrorCode error_code) {
    int index = error_code & 0xFFF;
    if (index < 0 || index >= sizeof(g_error_messages_ko) / sizeof(g_error_messages_ko[0])) {
        return "알 수 없는 Windows 오류";
    }

    const char* message = g_error_messages_ko[index];
    return message ? message : "알 수 없는 Windows 오류";
}

/* 오류 콜백 등록 */
ETResult et_windows_set_error_callback(ETWindowsErrorCallback callback, void* user_data) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);
    g_windows_error.error_callback = callback;
    g_windows_error.error_callback_user_data = user_data;
    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 오류 콜백 제거 */
ETResult et_windows_remove_error_callback(void) {
    return et_windows_set_error_callback(NULL, NULL);
}

/* 폴백 콜백 등록 */
ETResult et_windows_register_fallback(ETWindowsErrorCode error_code,
                                    ETWindowsFallbackCallback callback,
                                    void* context) {
    if (!callback) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    if (g_windows_error.fallback_count >= MAX_FALLBACK_CALLBACKS) {
        LeaveCriticalSection(&g_windows_error.error_lock);
        return ET_ERROR_RESOURCE_EXHAUSTED;
    }

    /* 기존 등록 확인 */
    for (int i = 0; i < g_windows_error.fallback_count; i++) {
        if (g_windows_error.fallback_entries[i].error_code == error_code) {
            /* 기존 등록 업데이트 */
            g_windows_error.fallback_entries[i].callback = callback;
            g_windows_error.fallback_entries[i].context = context;
            LeaveCriticalSection(&g_windows_error.error_lock);
            return ET_SUCCESS;
        }
    }

    /* 새 등록 추가 */
    ETWindowsFallbackEntry* entry = &g_windows_error.fallback_entries[g_windows_error.fallback_count];
    entry->error_code = error_code;
    entry->callback = callback;
    entry->context = context;
    entry->strategy = _get_default_fallback_strategy(error_code);

    g_windows_error.fallback_count++;

    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 폴백 실행 */
ETResult et_windows_execute_fallback(ETWindowsErrorCode error_code) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 등록된 폴백 콜백 찾기 */
    for (int i = 0; i < g_windows_error.fallback_count; i++) {
        if (g_windows_error.fallback_entries[i].error_code == error_code) {
            ETWindowsFallbackEntry* entry = &g_windows_error.fallback_entries[i];

            /* 폴백 실행 */
            ETResult result = entry->callback(error_code, entry->context);

            /* 통계 업데이트 */
            g_windows_error.statistics.fallback_executions++;
            if (result == ET_SUCCESS) {
                g_windows_error.statistics.successful_recoveries++;
            }

            LeaveCriticalSection(&g_windows_error.error_lock);
            return result;
        }
    }

    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_ERROR_NOT_FOUND;
}

/* 폴백 전략 설정 */
ETResult et_windows_set_fallback_strategy(ETWindowsErrorCode error_code,
                                         ETWindowsFallbackStrategy strategy) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 등록된 폴백 찾기 */
    for (int i = 0; i < g_windows_error.fallback_count; i++) {
        if (g_windows_error.fallback_entries[i].error_code == error_code) {
            g_windows_error.fallback_entries[i].strategy = strategy;
            LeaveCriticalSection(&g_windows_error.error_lock);
            return ET_SUCCESS;
        }
    }

    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_ERROR_NOT_FOUND;
}

/* 오류 통계 조회 */
ETResult et_windows_get_error_statistics(ETWindowsErrorStatistics* stats) {
    if (!stats) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);
    *stats = g_windows_error.statistics;
    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 오류 통계 초기화 */
ETResult et_windows_reset_error_statistics(void) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);
    memset(&g_windows_error.statistics, 0, sizeof(g_windows_error.statistics));
    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 오류 로깅 활성화 */
ETResult et_windows_enable_error_logging(const char* log_file_path) {
    if (!log_file_path) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 기존 로그 파일 닫기 */
    if (g_windows_error.log_file) {
        fclose(g_windows_error.log_file);
        g_windows_error.log_file = NULL;
    }

    /* 새 로그 파일 열기 */
    g_windows_error.log_file = fopen(log_file_path, "a");
    if (!g_windows_error.log_file) {
        LeaveCriticalSection(&g_windows_error.error_lock);
        return ET_ERROR_FILE_IO;
    }

    strncpy(g_windows_error.log_file_path, log_file_path, sizeof(g_windows_error.log_file_path) - 1);
    g_windows_error.log_file_path[sizeof(g_windows_error.log_file_path) - 1] = '\0';
    g_windows_error.logging_enabled = true;

    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 오류 로깅 비활성화 */
ETResult et_windows_disable_error_logging(void) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    if (g_windows_error.log_file) {
        fclose(g_windows_error.log_file);
        g_windows_error.log_file = NULL;
    }

    g_windows_error.logging_enabled = false;
    memset(g_windows_error.log_file_path, 0, sizeof(g_windows_error.log_file_path));

    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 내부 함수 구현 */

/* 로그 파일에 오류 기록 */
static void _log_error_to_file(const ETWindowsErrorInfo* error_info) {
    if (!g_windows_error.log_file || !error_info) {
        return;
    }

    /* 시간 문자열 생성 */
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             error_info->timestamp.wYear, error_info->timestamp.wMonth, error_info->timestamp.wDay,
             error_info->timestamp.wHour, error_info->timestamp.wMinute, error_info->timestamp.wSecond,
             error_info->timestamp.wMilliseconds);

    /* 심각도 문자열 */
    const char* severity_str;
    switch (error_info->severity) {
        case ET_WINDOWS_ERROR_SEVERITY_INFO: severity_str = "INFO"; break;
        case ET_WINDOWS_ERROR_SEVERITY_WARNING: severity_str = "WARNING"; break;
        case ET_WINDOWS_ERROR_SEVERITY_ERROR: severity_str = "ERROR"; break;
        case ET_WINDOWS_ERROR_SEVERITY_CRITICAL: severity_str = "CRITICAL"; break;
        default: severity_str = "UNKNOWN"; break;
    }

    /* 로그 항목 작성 */
    fprintf(g_windows_error.log_file,
            "[%s] %s - Code: 0x%X, Win32: %lu, HRESULT: 0x%08X\n"
            "  Module: %s, Function: %s, Line: %d\n"
            "  Message: %s\n"
            "  Details: %s\n\n",
            time_str, severity_str, error_info->error_code,
            error_info->win32_error, error_info->hresult,
            error_info->module_name ? error_info->module_name : "Unknown",
            error_info->function_name ? error_info->function_name : "Unknown",
            error_info->line_number,
            error_info->message,
            error_info->technical_details);

    fflush(g_windows_error.log_file);
}

/* 오류 통계 업데이트 */
static void _update_error_statistics(ETWindowsErrorCode error_code, ETWindowsErrorSeverity severity) {
    g_windows_error.statistics.total_errors++;

    if (severity == ET_WINDOWS_ERROR_SEVERITY_CRITICAL) {
        g_windows_error.statistics.critical_errors++;
    }

    /* 가장 빈번한 오류 추적 (간단한 구현) */
    g_windows_error.statistics.most_frequent_error = error_code;

    /* 마지막 오류 시간 업데이트 */
    GetSystemTime(&g_windows_error.statistics.last_error_time);
}

/* 오류 심각도 결정 */
static ETWindowsErrorSeverity _determine_error_severity(ETWindowsErrorCode error_code) {
    /* 오류 코드에 따른 기본 심각도 설정 */
    switch (error_code) {
        /* 치명적 오류 */
        case ET_WINDOWS_ERROR_DEP_VIOLATION:
        case ET_WINDOWS_ERROR_SECURITY_CHECK_FAILED:
        case ET_WINDOWS_ERROR_UNSUPPORTED_WINDOWS_VERSION:
            return ET_WINDOWS_ERROR_SEVERITY_CRITICAL;

        /* 일반 오류 */
        case ET_WINDOWS_ERROR_WASAPI_INIT_FAILED:
        case ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED:
        case ET_WINDOWS_ERROR_THREAD_POOL_CREATION_FAILED:
            return ET_WINDOWS_ERROR_SEVERITY_ERROR;

        /* 경고 */
        case ET_WINDOWS_ERROR_LARGE_PAGE_PRIVILEGE_DENIED:
        case ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED:
        case ET_WINDOWS_ERROR_AVX2_NOT_AVAILABLE:
            return ET_WINDOWS_ERROR_SEVERITY_WARNING;

        /* 기본값은 정보 */
        default:
            return ET_WINDOWS_ERROR_SEVERITY_INFO;
    }
}

/* 기본 폴백 전략 결정 */
static ETWindowsFallbackStrategy _get_default_fallback_strategy(ETWindowsErrorCode error_code) {
    switch (error_code) {
        /* 대안 구현 사용 */
        case ET_WINDOWS_ERROR_WASAPI_INIT_FAILED:
        case ET_WINDOWS_ERROR_WASAPI_DEVICE_NOT_FOUND:
            return ET_WINDOWS_FALLBACK_ALTERNATIVE;

        /* 성능 저하 모드 */
        case ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED:
        case ET_WINDOWS_ERROR_AVX2_NOT_AVAILABLE:
        case ET_WINDOWS_ERROR_THREAD_POOL_CREATION_FAILED:
            return ET_WINDOWS_FALLBACK_DEGRADED;

        /* 기능 비활성화 */
        case ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED:
        case ET_WINDOWS_ERROR_UAC_INSUFFICIENT_PRIVILEGES:
            return ET_WINDOWS_FALLBACK_DISABLE_FEATURE;

        /* 기본값은 폴백 없음 */
        default:
            return ET_WINDOWS_FALLBACK_NONE;
    }
}/* 우
아한 성능 저하 (Graceful Degradation) 구현 */

/* 성능 저하 상태 조회 */
ETResult et_windows_get_degradation_state(ETWindowsDegradationState* state) {
    if (!state) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);
    *state = g_windows_error.degradation_state;
    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 성능 저하 적용 */
ETResult et_windows_apply_degradation(const ETWindowsDegradationState* state) {
    if (!state) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 성능 저하 상태 업데이트 */
    g_windows_error.degradation_state = *state;

    /* 성능 스케일 팩터 검증 */
    if (g_windows_error.degradation_state.performance_scale_factor < 0.0f) {
        g_windows_error.degradation_state.performance_scale_factor = 0.0f;
    } else if (g_windows_error.degradation_state.performance_scale_factor > 1.0f) {
        g_windows_error.degradation_state.performance_scale_factor = 1.0f;
    }

    LeaveCriticalSection(&g_windows_error.error_lock);

    /* 로그 기록 */
    if (g_windows_error.logging_enabled) {
        char degradation_log[512];
        snprintf(degradation_log, sizeof(degradation_log),
                "Performance degradation applied: "
                "Audio Quality Reduced: %s, "
                "SIMD Disabled: %s, "
                "Threading Limited: %s, "
                "Large Pages Disabled: %s, "
                "ETW Disabled: %s, "
                "Performance Scale: %.2f",
                state->audio_quality_reduced ? "Yes" : "No",
                state->simd_optimization_disabled ? "Yes" : "No",
                state->threading_limited ? "Yes" : "No",
                state->large_pages_disabled ? "Yes" : "No",
                state->etw_logging_disabled ? "Yes" : "No",
                state->performance_scale_factor);

        ET_WINDOWS_REPORT_ERROR(ET_WINDOWS_ERROR_PERFORMANCE_COUNTER_FAILED, 0, S_OK,
                               "Performance degradation applied: %s", degradation_log);
    }

    return ET_SUCCESS;
}

/* 복구 시도 */
ETResult et_windows_attempt_recovery(void) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    g_windows_error.statistics.recovery_attempts++;

    /* 성능 저하 상태 점진적 복구 */
    ETWindowsDegradationState* state = &g_windows_error.degradation_state;
    bool recovery_attempted = false;

    /* 성능 스케일 팩터 점진적 증가 */
    if (state->performance_scale_factor < 1.0f) {
        state->performance_scale_factor += 0.1f;
        if (state->performance_scale_factor > 1.0f) {
            state->performance_scale_factor = 1.0f;
        }
        recovery_attempted = true;
    }

    /* ETW 로깅 복구 시도 */
    if (state->etw_logging_disabled) {
        ETResult etw_result = et_windows_register_etw_provider();
        if (etw_result == ET_SUCCESS) {
            state->etw_logging_disabled = false;
            recovery_attempted = true;
        }
    }

    /* Large Page 복구 시도 */
    if (state->large_pages_disabled) {
        if (et_windows_enable_large_page_privilege()) {
            state->large_pages_disabled = false;
            recovery_attempted = true;
        }
    }

    /* SIMD 최적화 복구 시도 */
    if (state->simd_optimization_disabled) {
        ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
        if (features.has_avx2 || features.has_avx) {
            state->simd_optimization_disabled = false;
            recovery_attempted = true;
        }
    }

    if (recovery_attempted) {
        g_windows_error.statistics.successful_recoveries++;
    }

    LeaveCriticalSection(&g_windows_error.error_lock);

    return recovery_attempted ? ET_SUCCESS : ET_ERROR_OPERATION_FAILED;
}

/* 시스템 정보 로깅 */
ETResult et_windows_log_system_info(void) {
    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    if (!g_windows_error.logging_enabled || !g_windows_error.log_file) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 시스템 정보 수집 */
    OSVERSIONINFOEX osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((OSVERSIONINFO*)&osvi);

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);

    MEMORYSTATUSEX meminfo = { 0 };
    meminfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&meminfo);

    /* CPU 기능 정보 */
    ETWindowsCPUFeatures cpu_features = et_windows_detect_cpu_features();

    /* 시스템 정보 로깅 */
    fprintf(g_windows_error.log_file,
            "=== LibEtude Windows System Information ===\n"
            "OS Version: %lu.%lu.%lu (Build %lu)\n"
            "Processor Architecture: %s\n"
            "Number of Processors: %lu\n"
            "Total Physical Memory: %llu MB\n"
            "Available Physical Memory: %llu MB\n"
            "CPU Features:\n"
            "  SSE4.1: %s\n"
            "  AVX: %s\n"
            "  AVX2: %s\n"
            "  AVX-512: %s\n"
            "Current Degradation State:\n"
            "  Audio Quality Reduced: %s\n"
            "  SIMD Optimization Disabled: %s\n"
            "  Threading Limited: %s\n"
            "  Large Pages Disabled: %s\n"
            "  ETW Logging Disabled: %s\n"
            "  Performance Scale Factor: %.2f\n"
            "Error Statistics:\n"
            "  Total Errors: %u\n"
            "  Critical Errors: %u\n"
            "  Fallback Executions: %u\n"
            "  Recovery Attempts: %u\n"
            "  Successful Recoveries: %u\n"
            "==========================================\n\n",
            osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, osvi.dwBuildNumber,
            (sysinfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) ? "x64" :
            (sysinfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) ? "x86" : "Unknown",
            sysinfo.dwNumberOfProcessors,
            meminfo.ullTotalPhys / (1024 * 1024),
            meminfo.ullAvailPhys / (1024 * 1024),
            cpu_features.has_sse41 ? "Yes" : "No",
            cpu_features.has_avx ? "Yes" : "No",
            cpu_features.has_avx2 ? "Yes" : "No",
            cpu_features.has_avx512 ? "Yes" : "No",
            g_windows_error.degradation_state.audio_quality_reduced ? "Yes" : "No",
            g_windows_error.degradation_state.simd_optimization_disabled ? "Yes" : "No",
            g_windows_error.degradation_state.threading_limited ? "Yes" : "No",
            g_windows_error.degradation_state.large_pages_disabled ? "Yes" : "No",
            g_windows_error.degradation_state.etw_logging_disabled ? "Yes" : "No",
            g_windows_error.degradation_state.performance_scale_factor,
            g_windows_error.statistics.total_errors,
            g_windows_error.statistics.critical_errors,
            g_windows_error.statistics.fallback_executions,
            g_windows_error.statistics.recovery_attempts,
            g_windows_error.statistics.successful_recoveries);

    fflush(g_windows_error.log_file);

    LeaveCriticalSection(&g_windows_error.error_lock);

    return ET_SUCCESS;
}

/* 오류 보고서 생성 */
ETResult et_windows_generate_error_report(const char* report_file_path) {
    if (!report_file_path) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_windows_error.initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    FILE* report_file = fopen(report_file_path, "w");
    if (!report_file) {
        return ET_ERROR_FILE_IO;
    }

    EnterCriticalSection(&g_windows_error.error_lock);

    /* 보고서 헤더 */
    SYSTEMTIME current_time;
    GetSystemTime(&current_time);

    fprintf(report_file,
            "LibEtude Windows Error Report\n"
            "Generated: %04d-%02d-%02d %02d:%02d:%02d UTC\n"
            "========================================\n\n",
            current_time.wYear, current_time.wMonth, current_time.wDay,
            current_time.wHour, current_time.wMinute, current_time.wSecond);

    /* 마지막 오류 정보 */
    if (g_windows_error.last_error.error_code != 0) {
        fprintf(report_file,
                "Last Error Information:\n"
                "  Error Code: 0x%X (%s)\n"
                "  Win32 Error: %lu\n"
                "  HRESULT: 0x%08X\n"
                "  Severity: %d\n"
                "  Module: %s\n"
                "  Function: %s\n"
                "  Line: %d\n"
                "  Message: %s\n"
                "  Technical Details: %s\n"
                "  Timestamp: %04d-%02d-%02d %02d:%02d:%02d.%03d\n\n",
                g_windows_error.last_error.error_code,
                et_windows_get_error_message(g_windows_error.last_error.error_code),
                g_windows_error.last_error.win32_error,
                g_windows_error.last_error.hresult,
                g_windows_error.last_error.severity,
                g_windows_error.last_error.module_name ? g_windows_error.last_error.module_name : "Unknown",
                g_windows_error.last_error.function_name ? g_windows_error.last_error.function_name : "Unknown",
                g_windows_error.last_error.line_number,
                g_windows_error.last_error.message,
                g_windows_error.last_error.technical_details,
                g_windows_error.last_error.timestamp.wYear,
                g_windows_error.last_error.timestamp.wMonth,
                g_windows_error.last_error.timestamp.wDay,
                g_windows_error.last_error.timestamp.wHour,
                g_windows_error.last_error.timestamp.wMinute,
                g_windows_error.last_error.timestamp.wSecond,
                g_windows_error.last_error.timestamp.wMilliseconds);
    }

    /* 통계 정보 */
    fprintf(report_file,
            "Error Statistics:\n"
            "  Total Errors: %u\n"
            "  Critical Errors: %u\n"
            "  Fallback Executions: %u\n"
            "  Recovery Attempts: %u\n"
            "  Successful Recoveries: %u\n"
            "  Most Frequent Error: 0x%X (%s)\n\n",
            g_windows_error.statistics.total_errors,
            g_windows_error.statistics.critical_errors,
            g_windows_error.statistics.fallback_executions,
            g_windows_error.statistics.recovery_attempts,
            g_windows_error.statistics.successful_recoveries,
            g_windows_error.statistics.most_frequent_error,
            et_windows_get_error_message(g_windows_error.statistics.most_frequent_error));

    /* 현재 성능 저하 상태 */
    fprintf(report_file,
            "Current Degradation State:\n"
            "  Audio Quality Reduced: %s\n"
            "  SIMD Optimization Disabled: %s\n"
            "  Threading Limited: %s\n"
            "  Large Pages Disabled: %s\n"
            "  ETW Logging Disabled: %s\n"
            "  Performance Scale Factor: %.2f\n\n",
            g_windows_error.degradation_state.audio_quality_reduced ? "Yes" : "No",
            g_windows_error.degradation_state.simd_optimization_disabled ? "Yes" : "No",
            g_windows_error.degradation_state.threading_limited ? "Yes" : "No",
            g_windows_error.degradation_state.large_pages_disabled ? "Yes" : "No",
            g_windows_error.degradation_state.etw_logging_disabled ? "Yes" : "No",
            g_windows_error.degradation_state.performance_scale_factor);

    /* 등록된 폴백 콜백 정보 */
    fprintf(report_file,
            "Registered Fallback Callbacks:\n");
    for (int i = 0; i < g_windows_error.fallback_count; i++) {
        fprintf(report_file,
                "  Error Code: 0x%X, Strategy: %d\n",
                g_windows_error.fallback_entries[i].error_code,
                g_windows_error.fallback_entries[i].strategy);
    }

    fprintf(report_file, "\nEnd of Report\n");

    LeaveCriticalSection(&g_windows_error.error_lock);

    fclose(report_file);

    return ET_SUCCESS;
}

/* 기본 폴백 콜백 구현들 */

/* WASAPI -> DirectSound 폴백 콜백 */
static ETResult _fallback_wasapi_to_directsound(ETWindowsErrorCode error_code, void* context) {
    (void)error_code; /* 미사용 매개변수 */

    ETAudioDevice* device = (ETAudioDevice*)context;
    if (!device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    /* DirectSound로 폴백 시도 */
    ETResult result = et_audio_fallback_to_directsound(device);
    if (result == ET_SUCCESS) {
        /* 오디오 품질 저하 상태 설정 */
        g_windows_error.degradation_state.audio_quality_reduced = true;
        g_windows_error.degradation_state.performance_scale_factor *= 0.9f;
    }

    return result;
}

/* SIMD 최적화 비활성화 폴백 콜백 */
static ETResult _fallback_disable_simd(ETWindowsErrorCode error_code, void* context) {
    (void)error_code; /* 미사용 매개변수 */
    (void)context;    /* 미사용 매개변수 */

    /* SIMD 최적화 비활성화 */
    g_windows_error.degradation_state.simd_optimization_disabled = true;
    g_windows_error.degradation_state.performance_scale_factor *= 0.8f;

    return ET_SUCCESS;
}

/* Large Page 비활성화 폴백 콜백 */
static ETResult _fallback_disable_large_pages(ETWindowsErrorCode error_code, void* context) {
    (void)error_code; /* 미사용 매개변수 */
    (void)context;    /* 미사용 매개변수 */

    /* Large Page 비활성화 */
    g_windows_error.degradation_state.large_pages_disabled = true;
    g_windows_error.degradation_state.performance_scale_factor *= 0.95f;

    return ET_SUCCESS;
}

/* ETW 로깅 비활성화 폴백 콜백 */
static ETResult _fallback_disable_etw(ETWindowsErrorCode error_code, void* context) {
    (void)error_code; /* 미사용 매개변수 */
    (void)context;    /* 미사용 매개변수 */

    /* ETW 로깅 비활성화 */
    g_windows_error.degradation_state.etw_logging_disabled = true;

    return ET_SUCCESS;
}

/* 기본 폴백 콜백 등록 함수 */
ETResult et_windows_register_default_fallbacks(void) {
    ETResult result;

    /* WASAPI 관련 폴백 등록 */
    result = et_windows_register_fallback(ET_WINDOWS_ERROR_WASAPI_INIT_FAILED,
                                        _fallback_wasapi_to_directsound, NULL);
    if (result != ET_SUCCESS) return result;

    result = et_windows_register_fallback(ET_WINDOWS_ERROR_WASAPI_DEVICE_NOT_FOUND,
                                        _fallback_wasapi_to_directsound, NULL);
    if (result != ET_SUCCESS) return result;

    /* SIMD 관련 폴백 등록 */
    result = et_windows_register_fallback(ET_WINDOWS_ERROR_AVX2_NOT_AVAILABLE,
                                        _fallback_disable_simd, NULL);
    if (result != ET_SUCCESS) return result;

    result = et_windows_register_fallback(ET_WINDOWS_ERROR_AVX512_NOT_AVAILABLE,
                                        _fallback_disable_simd, NULL);
    if (result != ET_SUCCESS) return result;

    /* Large Page 관련 폴백 등록 */
    result = et_windows_register_fallback(ET_WINDOWS_ERROR_LARGE_PAGE_PRIVILEGE_DENIED,
                                        _fallback_disable_large_pages, NULL);
    if (result != ET_SUCCESS) return result;

    result = et_windows_register_fallback(ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED,
                                        _fallback_disable_large_pages, NULL);
    if (result != ET_SUCCESS) return result;

    /* ETW 관련 폴백 등록 */
    result = et_windows_register_fallback(ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED,
                                        _fallback_disable_etw, NULL);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}