#ifndef LIBETUDE_PLATFORM_WINDOWS_DEBUG_H
#define LIBETUDE_PLATFORM_WINDOWS_DEBUG_H

#ifdef _WIN32

#include <windows.h>
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Windows 이벤트 로그 소스 이름
#define ET_WINDOWS_EVENT_SOURCE_NAME L"LibEtude"

// Windows 이벤트 로그 카테고리
typedef enum {
    ET_EVENT_CATEGORY_GENERAL = 1,
    ET_EVENT_CATEGORY_AUDIO = 2,
    ET_EVENT_CATEGORY_MEMORY = 3,
    ET_EVENT_CATEGORY_PERFORMANCE = 4,
    ET_EVENT_CATEGORY_SECURITY = 5
} ETWindowsEventCategory;

// Windows 이벤트 로그 타입
typedef enum {
    ET_EVENT_TYPE_SUCCESS = EVENTLOG_SUCCESS,
    ET_EVENT_TYPE_ERROR = EVENTLOG_ERROR_TYPE,
    ET_EVENT_TYPE_WARNING = EVENTLOG_WARNING_TYPE,
    ET_EVENT_TYPE_INFORMATION = EVENTLOG_INFORMATION_TYPE,
    ET_EVENT_TYPE_AUDIT_SUCCESS = EVENTLOG_AUDIT_SUCCESS,
    ET_EVENT_TYPE_AUDIT_FAILURE = EVENTLOG_AUDIT_FAILURE
} ETWindowsEventType;

// 디버그 정보 구조체
typedef struct {
    bool pdb_generation_enabled;      // PDB 파일 생성 활성화
    bool event_logging_enabled;       // Windows 이벤트 로그 활성화
    bool console_output_enabled;      // 콘솔 출력 활성화
    bool file_logging_enabled;        // 파일 로깅 활성화
    const char* log_file_path;        // 로그 파일 경로
    DWORD max_log_file_size;          // 최대 로그 파일 크기 (바이트)
    bool detailed_stack_trace;        // 상세 스택 트레이스 활성화
} ETWindowsDebugConfig;

// 스택 트레이스 정보
typedef struct {
    void* addresses[64];              // 스택 주소 배열
    DWORD frame_count;                // 프레임 수
    char symbols[64][256];            // 심볼 이름 배열
    char modules[64][MAX_PATH];       // 모듈 이름 배열
    DWORD line_numbers[64];           // 라인 번호 배열
} ETWindowsStackTrace;

// 오류 정보 구조체
typedef struct {
    ETErrorCode error_code;           // LibEtude 오류 코드
    DWORD windows_error_code;         // Windows 시스템 오류 코드
    const char* error_message;        // 오류 메시지
    const char* function_name;        // 함수 이름
    const char* file_name;            // 파일 이름
    DWORD line_number;                // 라인 번호
    DWORD thread_id;                  // 스레드 ID
    SYSTEMTIME timestamp;             // 타임스탬프
    ETWindowsStackTrace stack_trace;  // 스택 트레이스
} ETWindowsErrorInfo;

// Windows 디버깅 시스템 초기화 및 정리
ETResult et_windows_debug_init(const ETWindowsDebugConfig* config);
void et_windows_debug_shutdown(void);

// PDB 파일 생성 관련
bool et_windows_debug_is_pdb_enabled(void);
ETResult et_windows_debug_configure_pdb(bool enable_full_debug_info);

// Windows 이벤트 로그 관련
ETResult et_windows_debug_register_event_source(void);
void et_windows_debug_unregister_event_source(void);
ETResult et_windows_debug_write_event_log(ETWindowsEventType type,
                                         ETWindowsEventCategory category,
                                         DWORD event_id,
                                         const char* message);

// 상세 오류 정보 기록
void et_windows_debug_log_error_detailed(const ETWindowsErrorInfo* error_info);
void et_windows_debug_log_error_simple(ETErrorCode error_code,
                                      const char* message,
                                      const char* function,
                                      DWORD line);

// 스택 트레이스 생성
ETResult et_windows_debug_capture_stack_trace(ETWindowsStackTrace* stack_trace);
void et_windows_debug_print_stack_trace(const ETWindowsStackTrace* stack_trace);
char* et_windows_debug_format_stack_trace(const ETWindowsStackTrace* stack_trace);

// 디버그 출력 함수들
void et_windows_debug_output_console(const char* format, ...);
void et_windows_debug_output_debugger(const char* format, ...);
void et_windows_debug_output_file(const char* format, ...);

// 메모리 덤프 생성
ETResult et_windows_debug_create_minidump(const char* dump_file_path,
                                         EXCEPTION_POINTERS* exception_info);

// 성능 카운터 및 프로파일링
typedef struct {
    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;
    LARGE_INTEGER frequency;
    const char* operation_name;
} ETWindowsPerformanceTimer;

void et_windows_debug_timer_start(ETWindowsPerformanceTimer* timer, const char* operation_name);
double et_windows_debug_timer_end(ETWindowsPerformanceTimer* timer);
void et_windows_debug_log_performance(const char* operation_name, double duration_ms);

// 메모리 사용량 모니터링
typedef struct {
    SIZE_T working_set_size;          // 작업 집합 크기
    SIZE_T peak_working_set_size;     // 최대 작업 집합 크기
    SIZE_T private_usage;             // 전용 메모리 사용량
    SIZE_T virtual_size;              // 가상 메모리 크기
    DWORD page_faults;                // 페이지 폴트 수
} ETWindowsMemoryInfo;

ETResult et_windows_debug_get_memory_info(ETWindowsMemoryInfo* memory_info);
void et_windows_debug_log_memory_usage(void);

// 시스템 정보 로깅
typedef struct {
    char os_version[256];             // OS 버전
    char cpu_info[256];               // CPU 정보
    DWORD total_memory_mb;            // 총 메모리 (MB)
    DWORD available_memory_mb;        // 사용 가능 메모리 (MB)
    DWORD processor_count;            // 프로세서 수
} ETWindowsSystemInfo;

ETResult et_windows_debug_get_system_info(ETWindowsSystemInfo* system_info);
void et_windows_debug_log_system_info(void);

// 편의 매크로들
#ifdef _DEBUG
    #define ET_DEBUG_LOG(format, ...) \
        et_windows_debug_output_console("[DEBUG] " format "\n", ##__VA_ARGS__)

    #define ET_DEBUG_LOG_ERROR(code, msg) \
        et_windows_debug_log_error_simple((code), (msg), __FUNCTION__, __LINE__)

    #define ET_DEBUG_TIMER_START(timer, name) \
        et_windows_debug_timer_start(&(timer), (name))

    #define ET_DEBUG_TIMER_END(timer) \
        et_windows_debug_timer_end(&(timer))

    #define ET_DEBUG_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                ET_DEBUG_LOG("ASSERTION FAILED: %s - %s", #condition, message); \
                if (IsDebuggerPresent()) { \
                    __debugbreak(); \
                } \
            } \
        } while(0)
#else
    #define ET_DEBUG_LOG(format, ...) ((void)0)
    #define ET_DEBUG_LOG_ERROR(code, msg) ((void)0)
    #define ET_DEBUG_TIMER_START(timer, name) ((void)0)
    #define ET_DEBUG_TIMER_END(timer) (0.0)
    #define ET_DEBUG_ASSERT(condition, message) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif // _WIN32

#endif // LIBETUDE_PLATFORM_WINDOWS_DEBUG_H