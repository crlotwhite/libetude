#ifndef LIBETUDE_PLATFORM_WINDOWS_ETW_H
#define LIBETUDE_PLATFORM_WINDOWS_ETW_H

#ifdef _WIN32

#include <windows.h>
#include <evntprov.h>
#include <evntrace.h>
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ETW 프로바이더 GUID - LibEtude 전용
// {12345678-1234-5678-9ABC-123456789ABC}
DEFINE_GUID(LIBETUDE_ETW_PROVIDER_GUID,
    0x12345678, 0x1234, 0x5678, 0x9A, 0xBC, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC);

// ETW 이벤트 레벨 정의
typedef enum {
    ET_ETW_LEVEL_CRITICAL = 1,
    ET_ETW_LEVEL_ERROR = 2,
    ET_ETW_LEVEL_WARNING = 3,
    ET_ETW_LEVEL_INFO = 4,
    ET_ETW_LEVEL_VERBOSE = 5
} ETETWLevel;

// ETW 이벤트 키워드 (카테고리)
#define ET_ETW_KEYWORD_PERFORMANCE  0x0000000000000001ULL
#define ET_ETW_KEYWORD_AUDIO        0x0000000000000002ULL
#define ET_ETW_KEYWORD_MEMORY       0x0000000000000004ULL
#define ET_ETW_KEYWORD_THREADING    0x0000000000000008ULL
#define ET_ETW_KEYWORD_ERROR        0x0000000000000010ULL
#define ET_ETW_KEYWORD_INITIALIZATION 0x0000000000000020ULL

// ETW 이벤트 ID 정의
typedef enum {
    ET_ETW_EVENT_LIBRARY_INIT = 1,
    ET_ETW_EVENT_LIBRARY_SHUTDOWN = 2,
    ET_ETW_EVENT_AUDIO_INIT = 10,
    ET_ETW_EVENT_AUDIO_RENDER_START = 11,
    ET_ETW_EVENT_AUDIO_RENDER_END = 12,
    ET_ETW_EVENT_PERFORMANCE_COUNTER = 20,
    ET_ETW_EVENT_MEMORY_ALLOCATION = 30,
    ET_ETW_EVENT_MEMORY_DEALLOCATION = 31,
    ET_ETW_EVENT_THREAD_CREATED = 40,
    ET_ETW_EVENT_THREAD_DESTROYED = 41,
    ET_ETW_EVENT_ERROR_OCCURRED = 50
} ETETWEventId;

// ETW 성능 이벤트 데이터 구조체
typedef struct {
    const char* operation_name;
    double duration_ms;
    DWORD thread_id;
    ULONGLONG timestamp;
} ETETWPerformanceEvent;

// ETW 오류 이벤트 데이터 구조체
typedef struct {
    ETErrorCode error_code;
    const char* error_message;
    const char* function_name;
    DWORD line_number;
    DWORD thread_id;
} ETETWErrorEvent;

// ETW 메모리 이벤트 데이터 구조체
typedef struct {
    void* address;
    size_t size;
    const char* allocation_type;
    DWORD thread_id;
} ETETWMemoryEvent;

// ETW 프로바이더 초기화 및 정리
ETResult et_windows_etw_init(void);
void et_windows_etw_shutdown(void);

// ETW 프로바이더 상태 확인
bool et_windows_etw_is_enabled(void);
bool et_windows_etw_is_level_enabled(ETETWLevel level);
bool et_windows_etw_is_keyword_enabled(ULONGLONG keyword);

// 성능 이벤트 로깅
void et_windows_etw_log_performance_start(const char* operation_name, ULONGLONG* start_time);
void et_windows_etw_log_performance_end(const char* operation_name, ULONGLONG start_time);
void et_windows_etw_log_performance_event(const ETETWPerformanceEvent* event);

// 오류 이벤트 로깅
void et_windows_etw_log_error(ETErrorCode error_code, const char* message,
                             const char* function, DWORD line);
void et_windows_etw_log_error_event(const ETETWErrorEvent* event);

// 메모리 이벤트 로깅
void et_windows_etw_log_memory_alloc(void* address, size_t size, const char* type);
void et_windows_etw_log_memory_free(void* address, size_t size);
void et_windows_etw_log_memory_event(const ETETWMemoryEvent* event, bool is_allocation);

// 오디오 이벤트 로깅
void et_windows_etw_log_audio_init(const char* backend_name, bool success);
void et_windows_etw_log_audio_render_start(DWORD buffer_size, DWORD sample_rate);
void et_windows_etw_log_audio_render_end(DWORD samples_rendered, double latency_ms);

// 스레딩 이벤트 로깅
void et_windows_etw_log_thread_created(DWORD thread_id, const char* thread_name);
void et_windows_etw_log_thread_destroyed(DWORD thread_id);

// 라이브러리 생명주기 이벤트
void et_windows_etw_log_library_init(const char* version);
void et_windows_etw_log_library_shutdown(void);

// 편의 매크로
#define ET_ETW_LOG_ERROR(code, msg) \
    et_windows_etw_log_error((code), (msg), __FUNCTION__, __LINE__)

#define ET_ETW_PERFORMANCE_SCOPE(name) \
    ULONGLONG _etw_start_time; \
    et_windows_etw_log_performance_start((name), &_etw_start_time); \
    struct _etw_scope_guard { \
        const char* _name; \
        ULONGLONG _start; \
        ~_etw_scope_guard() { et_windows_etw_log_performance_end(_name, _start); } \
    } _etw_guard = {(name), _etw_start_time}

#ifdef __cplusplus
}
#endif

#endif // _WIN32

#endif // LIBETUDE_PLATFORM_WINDOWS_ETW_H