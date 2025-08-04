/* LibEtude Windows 특화 오류 처리 시스템 */
/* Copyright (c) 2025 LibEtude Project */

#ifndef LIBETUDE_PLATFORM_WINDOWS_ERROR_H
#define LIBETUDE_PLATFORM_WINDOWS_ERROR_H

#ifdef _WIN32

#include "libetude/error.h"
#include "libetude/types.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Windows 특화 오류 코드 확장 */
typedef enum {
    /* 오디오 관련 오류 (0x1000-0x1FFF) */
    ET_WINDOWS_ERROR_WASAPI_INIT_FAILED = 0x1000,
    ET_WINDOWS_ERROR_WASAPI_DEVICE_NOT_FOUND,
    ET_WINDOWS_ERROR_WASAPI_FORMAT_NOT_SUPPORTED,
    ET_WINDOWS_ERROR_WASAPI_EXCLUSIVE_MODE_FAILED,
    ET_WINDOWS_ERROR_WASAPI_BUFFER_UNDERRUN,
    ET_WINDOWS_ERROR_WASAPI_DEVICE_DISCONNECTED,
    ET_WINDOWS_ERROR_DIRECTSOUND_INIT_FAILED,
    ET_WINDOWS_ERROR_DIRECTSOUND_BUFFER_LOST,
    ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
    ET_WINDOWS_ERROR_AUDIO_SESSION_EXPIRED,
    ET_WINDOWS_ERROR_AUDIO_DEVICE_CHANGED,

    /* 보안 관련 오류 (0x2000-0x2FFF) */
    ET_WINDOWS_ERROR_DEP_NOT_SUPPORTED = 0x2000,
    ET_WINDOWS_ERROR_DEP_VIOLATION,
    ET_WINDOWS_ERROR_ASLR_NOT_SUPPORTED,
    ET_WINDOWS_ERROR_ASLR_ALLOCATION_FAILED,
    ET_WINDOWS_ERROR_UAC_INSUFFICIENT_PRIVILEGES,
    ET_WINDOWS_ERROR_UAC_ELEVATION_REQUIRED,
    ET_WINDOWS_ERROR_SECURITY_CHECK_FAILED,
    ET_WINDOWS_ERROR_PRIVILEGE_NOT_HELD,

    /* 성능 관련 오류 (0x3000-0x3FFF) */
    ET_WINDOWS_ERROR_SIMD_NOT_SUPPORTED = 0x3000,
    ET_WINDOWS_ERROR_AVX_NOT_AVAILABLE,
    ET_WINDOWS_ERROR_AVX2_NOT_AVAILABLE,
    ET_WINDOWS_ERROR_AVX512_NOT_AVAILABLE,
    ET_WINDOWS_ERROR_THREAD_POOL_CREATION_FAILED,
    ET_WINDOWS_ERROR_THREAD_POOL_SUBMISSION_FAILED,
    ET_WINDOWS_ERROR_LARGE_PAGE_PRIVILEGE_DENIED,
    ET_WINDOWS_ERROR_LARGE_PAGE_ALLOCATION_FAILED,
    ET_WINDOWS_ERROR_PERFORMANCE_COUNTER_FAILED,

    /* 개발 도구 관련 오류 (0x4000-0x4FFF) */
    ET_WINDOWS_ERROR_ETW_PROVIDER_REGISTRATION_FAILED = 0x4000,
    ET_WINDOWS_ERROR_ETW_EVENT_WRITE_FAILED,
    ET_WINDOWS_ERROR_PDB_GENERATION_FAILED,
    ET_WINDOWS_ERROR_DEBUG_INFO_UNAVAILABLE,
    ET_WINDOWS_ERROR_PROFILER_INIT_FAILED,

    /* 플랫폼 관련 오류 (0x5000-0x5FFF) */
    ET_WINDOWS_ERROR_UNSUPPORTED_WINDOWS_VERSION = 0x5000,
    ET_WINDOWS_ERROR_REQUIRED_DLL_NOT_FOUND,
    ET_WINDOWS_ERROR_COM_INIT_FAILED,
    ET_WINDOWS_ERROR_REGISTRY_ACCESS_DENIED,
    ET_WINDOWS_ERROR_SERVICE_UNAVAILABLE
} ETWindowsErrorCode;

/* 오류 심각도 레벨 */
typedef enum {
    ET_WINDOWS_ERROR_SEVERITY_INFO = 0,      /* 정보성 메시지 */
    ET_WINDOWS_ERROR_SEVERITY_WARNING = 1,   /* 경고 (기능 제한) */
    ET_WINDOWS_ERROR_SEVERITY_ERROR = 2,     /* 오류 (복구 가능) */
    ET_WINDOWS_ERROR_SEVERITY_CRITICAL = 3   /* 치명적 오류 (복구 불가) */
} ETWindowsErrorSeverity;

/* 폴백 전략 타입 */
typedef enum {
    ET_WINDOWS_FALLBACK_NONE = 0,           /* 폴백 없음 */
    ET_WINDOWS_FALLBACK_ALTERNATIVE,        /* 대안 구현 사용 */
    ET_WINDOWS_FALLBACK_DEGRADED,          /* 성능 저하 모드 */
    ET_WINDOWS_FALLBACK_DISABLE_FEATURE    /* 기능 비활성화 */
} ETWindowsFallbackStrategy;

/* Windows 오류 정보 구조체 */
typedef struct {
    ETWindowsErrorCode error_code;          /* Windows 특화 오류 코드 */
    DWORD win32_error;                      /* Win32 API 오류 코드 */
    HRESULT hresult;                        /* COM/HRESULT 오류 코드 */
    ETWindowsErrorSeverity severity;        /* 오류 심각도 */
    ETWindowsFallbackStrategy fallback;     /* 폴백 전략 */
    char message[512];                      /* 오류 메시지 (한국어/영어) */
    char technical_details[1024];           /* 기술적 세부사항 */
    const char* module_name;                /* 오류 발생 모듈 */
    const char* function_name;              /* 오류 발생 함수 */
    int line_number;                        /* 오류 발생 라인 */
    SYSTEMTIME timestamp;                   /* 오류 발생 시간 */
} ETWindowsErrorInfo;

/* 오류 처리 콜백 함수 타입 */
typedef void (*ETWindowsErrorCallback)(const ETWindowsErrorInfo* error_info, void* user_data);

/* 폴백 실행 콜백 함수 타입 */
typedef ETResult (*ETWindowsFallbackCallback)(ETWindowsErrorCode error_code, void* context);

/* 오류 처리 시스템 초기화/정리 */
ETResult et_windows_error_init(void);
void et_windows_error_finalize(void);

/* 오류 보고 및 처리 */
ETResult et_windows_report_error(ETWindowsErrorCode error_code,
                                DWORD win32_error,
                                HRESULT hresult,
                                const char* module_name,
                                const char* function_name,
                                int line_number,
                                const char* format, ...);

/* 오류 정보 조회 */
ETResult et_windows_get_last_error_info(ETWindowsErrorInfo* error_info);
const char* et_windows_get_error_message(ETWindowsErrorCode error_code);
const char* et_windows_get_error_message_korean(ETWindowsErrorCode error_code);

/* 오류 콜백 등록 */
ETResult et_windows_set_error_callback(ETWindowsErrorCallback callback, void* user_data);
ETResult et_windows_remove_error_callback(void);

/* 폴백 메커니즘 관리 */
ETResult et_windows_register_fallback(ETWindowsErrorCode error_code,
                                    ETWindowsFallbackCallback callback,
                                    void* context);
ETResult et_windows_execute_fallback(ETWindowsErrorCode error_code);
ETResult et_windows_set_fallback_strategy(ETWindowsErrorCode error_code,
                                         ETWindowsFallbackStrategy strategy);

/* 오류 통계 및 모니터링 */
typedef struct {
    uint32_t total_errors;                  /* 총 오류 발생 횟수 */
    uint32_t critical_errors;               /* 치명적 오류 횟수 */
    uint32_t fallback_executions;          /* 폴백 실행 횟수 */
    uint32_t recovery_attempts;             /* 복구 시도 횟수 */
    uint32_t successful_recoveries;         /* 성공한 복구 횟수 */
    SYSTEMTIME last_error_time;             /* 마지막 오류 발생 시간 */
    ETWindowsErrorCode most_frequent_error; /* 가장 빈번한 오류 */
} ETWindowsErrorStatistics;

ETResult et_windows_get_error_statistics(ETWindowsErrorStatistics* stats);
ETResult et_windows_reset_error_statistics(void);

/* 오류 로깅 및 진단 */
ETResult et_windows_enable_error_logging(const char* log_file_path);
ETResult et_windows_disable_error_logging(void);
ETResult et_windows_log_system_info(void);
ETResult et_windows_generate_error_report(const char* report_file_path);

/* 우아한 성능 저하 (Graceful Degradation) */
typedef struct {
    bool audio_quality_reduced;             /* 오디오 품질 저하 */
    bool simd_optimization_disabled;        /* SIMD 최적화 비활성화 */
    bool threading_limited;                 /* 스레딩 제한 */
    bool large_pages_disabled;              /* Large Page 비활성화 */
    bool etw_logging_disabled;              /* ETW 로깅 비활성화 */
    float performance_scale_factor;         /* 성능 스케일 팩터 (0.0-1.0) */
} ETWindowsDegradationState;

ETResult et_windows_get_degradation_state(ETWindowsDegradationState* state);
ETResult et_windows_apply_degradation(const ETWindowsDegradationState* state);
ETResult et_windows_attempt_recovery(void);

/* 기본 폴백 콜백 등록 */
ETResult et_windows_register_default_fallbacks(void);

/* 매크로 헬퍼 */
#define ET_WINDOWS_REPORT_ERROR(code, win32_err, hr, fmt, ...) \
    et_windows_report_error((code), (win32_err), (hr), __FILE__, __FUNCTION__, __LINE__, (fmt), ##__VA_ARGS__)

#define ET_WINDOWS_REPORT_WIN32_ERROR(code, fmt, ...) \
    ET_WINDOWS_REPORT_ERROR((code), GetLastError(), S_OK, (fmt), ##__VA_ARGS__)

#define ET_WINDOWS_REPORT_HRESULT_ERROR(code, hr, fmt, ...) \
    ET_WINDOWS_REPORT_ERROR((code), 0, (hr), (fmt), ##__VA_ARGS__)

#define ET_WINDOWS_CHECK_AND_REPORT(condition, code, fmt, ...) \
    do { \
        if (!(condition)) { \
            ET_WINDOWS_REPORT_WIN32_ERROR((code), (fmt), ##__VA_ARGS__); \
            return ET_ERROR_PLATFORM_SPECIFIC; \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* LIBETUDE_PLATFORM_WINDOWS_ERROR_H */