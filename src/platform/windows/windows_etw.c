#ifdef _WIN32

#include "libetude/platform/windows_etw.h"
#include <stdio.h>
#include <string.h>

// ETW 프로바이더 핸들
static REGHANDLE g_etw_provider_handle = 0;
static bool g_etw_initialized = false;

// ETW 프로바이더 콜백 함수
static void NTAPI etw_provider_callback(
    LPCGUID source_id,
    ULONG is_enabled,
    UCHAR level,
    ULONGLONG match_any_keyword,
    ULONGLONG match_all_keyword,
    PEVENT_FILTER_DESCRIPTOR filter_data,
    PVOID callback_context)
{
    // ETW 프로바이더 상태 변경 시 호출됨
    // 현재는 단순히 상태만 기록
    (void)source_id;
    (void)level;
    (void)match_any_keyword;
    (void)match_all_keyword;
    (void)filter_data;
    (void)callback_context;

    // 프로바이더가 활성화/비활성화될 때 내부 상태 업데이트 가능
}

ETResult et_windows_etw_init(void)
{
    if (g_etw_initialized) {
        return ET_RESULT_SUCCESS;
    }

    // ETW 프로바이더 등록
    ULONG status = EventRegister(
        &LIBETUDE_ETW_PROVIDER_GUID,
        etw_provider_callback,
        NULL,
        &g_etw_provider_handle
    );

    if (status != ERROR_SUCCESS) {
        return ET_RESULT_ERROR_PLATFORM_SPECIFIC;
    }

    g_etw_initialized = true;

    // 라이브러리 초기화 이벤트 로깅
    et_windows_etw_log_library_init("1.0.0");

    return ET_RESULT_SUCCESS;
}

void et_windows_etw_shutdown(void)
{
    if (!g_etw_initialized) {
        return;
    }

    // 라이브러리 종료 이벤트 로깅
    et_windows_etw_log_library_shutdown();

    // ETW 프로바이더 등록 해제
    if (g_etw_provider_handle != 0) {
        EventUnregister(g_etw_provider_handle);
        g_etw_provider_handle = 0;
    }

    g_etw_initialized = false;
}

bool et_windows_etw_is_enabled(void)
{
    return g_etw_initialized && (g_etw_provider_handle != 0) &&
           EventProviderEnabled(g_etw_provider_handle, 0, 0);
}

bool et_windows_etw_is_level_enabled(ETETWLevel level)
{
    return g_etw_initialized && (g_etw_provider_handle != 0) &&
           EventProviderEnabled(g_etw_provider_handle, (UCHAR)level, 0);
}

bool et_windows_etw_is_keyword_enabled(ULONGLONG keyword)
{
    return g_etw_initialized && (g_etw_provider_handle != 0) &&
           EventProviderEnabled(g_etw_provider_handle, 0, keyword);
}

void et_windows_etw_log_performance_start(const char* operation_name, ULONGLONG* start_time)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_PERFORMANCE)) {
        return;
    }

    *start_time = GetTickCount64();

    EVENT_DATA_DESCRIPTOR event_data[3];
    EventDataDescCreate(&event_data[0], operation_name,
                       (ULONG)(strlen(operation_name) + 1));
    EventDataDescCreate(&event_data[1], start_time, sizeof(ULONGLONG));

    DWORD thread_id = GetCurrentThreadId();
    EventDataDescCreate(&event_data[2], &thread_id, sizeof(DWORD));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_PERFORMANCE_COUNTER, 0, 0,
                                  ET_ETW_LEVEL_VERBOSE, 0, 0, ET_ETW_KEYWORD_PERFORMANCE},
               3, event_data);
}

void et_windows_etw_log_performance_end(const char* operation_name, ULONGLONG start_time)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_PERFORMANCE)) {
        return;
    }

    ULONGLONG end_time = GetTickCount64();
    double duration_ms = (double)(end_time - start_time);

    ETETWPerformanceEvent event = {
        .operation_name = operation_name,
        .duration_ms = duration_ms,
        .thread_id = GetCurrentThreadId(),
        .timestamp = end_time
    };

    et_windows_etw_log_performance_event(&event);
}

void et_windows_etw_log_performance_event(const ETETWPerformanceEvent* event)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_PERFORMANCE)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[4];
    EventDataDescCreate(&event_data[0], event->operation_name,
                       (ULONG)(strlen(event->operation_name) + 1));
    EventDataDescCreate(&event_data[1], &event->duration_ms, sizeof(double));
    EventDataDescCreate(&event_data[2], &event->thread_id, sizeof(DWORD));
    EventDataDescCreate(&event_data[3], &event->timestamp, sizeof(ULONGLONG));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_PERFORMANCE_COUNTER, 0, 0,
                                  ET_ETW_LEVEL_INFO, 0, 0, ET_ETW_KEYWORD_PERFORMANCE},
               4, event_data);
}

void et_windows_etw_log_error(ETErrorCode error_code, const char* message,
                             const char* function, DWORD line)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_ERROR)) {
        return;
    }

    ETETWErrorEvent event = {
        .error_code = error_code,
        .error_message = message,
        .function_name = function,
        .line_number = line,
        .thread_id = GetCurrentThreadId()
    };

    et_windows_etw_log_error_event(&event);
}

void et_windows_etw_log_error_event(const ETETWErrorEvent* event)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_ERROR)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[5];
    EventDataDescCreate(&event_data[0], &event->error_code, sizeof(ETErrorCode));
    EventDataDescCreate(&event_data[1], event->error_message,
                       (ULONG)(strlen(event->error_message) + 1));
    EventDataDescCreate(&event_data[2], event->function_name,
                       (ULONG)(strlen(event->function_name) + 1));
    EventDataDescCreate(&event_data[3], &event->line_number, sizeof(DWORD));
    EventDataDescCreate(&event_data[4], &event->thread_id, sizeof(DWORD));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_ERROR_OCCURRED, 0, 0,
                                  ET_ETW_LEVEL_ERROR, 0, 0, ET_ETW_KEYWORD_ERROR},
               5, event_data);
}

void et_windows_etw_log_memory_alloc(void* address, size_t size, const char* type)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_MEMORY)) {
        return;
    }

    ETETWMemoryEvent event = {
        .address = address,
        .size = size,
        .allocation_type = type,
        .thread_id = GetCurrentThreadId()
    };

    et_windows_etw_log_memory_event(&event, true);
}

void et_windows_etw_log_memory_free(void* address, size_t size)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_MEMORY)) {
        return;
    }

    ETETWMemoryEvent event = {
        .address = address,
        .size = size,
        .allocation_type = "free",
        .thread_id = GetCurrentThreadId()
    };

    et_windows_etw_log_memory_event(&event, false);
}

void et_windows_etw_log_memory_event(const ETETWMemoryEvent* event, bool is_allocation)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_MEMORY)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[4];
    EventDataDescCreate(&event_data[0], &event->address, sizeof(void*));
    EventDataDescCreate(&event_data[1], &event->size, sizeof(size_t));
    EventDataDescCreate(&event_data[2], event->allocation_type,
                       (ULONG)(strlen(event->allocation_type) + 1));
    EventDataDescCreate(&event_data[3], &event->thread_id, sizeof(DWORD));

    USHORT event_id = is_allocation ? ET_ETW_EVENT_MEMORY_ALLOCATION :
                                     ET_ETW_EVENT_MEMORY_DEALLOCATION;

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){event_id, 0, 0,
                                  ET_ETW_LEVEL_VERBOSE, 0, 0, ET_ETW_KEYWORD_MEMORY},
               4, event_data);
}

void et_windows_etw_log_audio_init(const char* backend_name, bool success)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_AUDIO)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[2];
    EventDataDescCreate(&event_data[0], backend_name,
                       (ULONG)(strlen(backend_name) + 1));
    EventDataDescCreate(&event_data[1], &success, sizeof(bool));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_AUDIO_INIT, 0, 0,
                                  ET_ETW_LEVEL_INFO, 0, 0, ET_ETW_KEYWORD_AUDIO},
               2, event_data);
}

void et_windows_etw_log_audio_render_start(DWORD buffer_size, DWORD sample_rate)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_AUDIO)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[2];
    EventDataDescCreate(&event_data[0], &buffer_size, sizeof(DWORD));
    EventDataDescCreate(&event_data[1], &sample_rate, sizeof(DWORD));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_AUDIO_RENDER_START, 0, 0,
                                  ET_ETW_LEVEL_VERBOSE, 0, 0, ET_ETW_KEYWORD_AUDIO},
               2, event_data);
}

void et_windows_etw_log_audio_render_end(DWORD samples_rendered, double latency_ms)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_AUDIO)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[2];
    EventDataDescCreate(&event_data[0], &samples_rendered, sizeof(DWORD));
    EventDataDescCreate(&event_data[1], &latency_ms, sizeof(double));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_AUDIO_RENDER_END, 0, 0,
                                  ET_ETW_LEVEL_VERBOSE, 0, 0, ET_ETW_KEYWORD_AUDIO},
               2, event_data);
}

void et_windows_etw_log_thread_created(DWORD thread_id, const char* thread_name)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_THREADING)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[2];
    EventDataDescCreate(&event_data[0], &thread_id, sizeof(DWORD));
    EventDataDescCreate(&event_data[1], thread_name,
                       (ULONG)(strlen(thread_name) + 1));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_THREAD_CREATED, 0, 0,
                                  ET_ETW_LEVEL_INFO, 0, 0, ET_ETW_KEYWORD_THREADING},
               2, event_data);
}

void et_windows_etw_log_thread_destroyed(DWORD thread_id)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_THREADING)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[1];
    EventDataDescCreate(&event_data[0], &thread_id, sizeof(DWORD));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_THREAD_DESTROYED, 0, 0,
                                  ET_ETW_LEVEL_INFO, 0, 0, ET_ETW_KEYWORD_THREADING},
               1, event_data);
}

void et_windows_etw_log_library_init(const char* version)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_INITIALIZATION)) {
        return;
    }

    EVENT_DATA_DESCRIPTOR event_data[1];
    EventDataDescCreate(&event_data[0], version, (ULONG)(strlen(version) + 1));

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_LIBRARY_INIT, 0, 0,
                                  ET_ETW_LEVEL_INFO, 0, 0, ET_ETW_KEYWORD_INITIALIZATION},
               1, event_data);
}

void et_windows_etw_log_library_shutdown(void)
{
    if (!et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_INITIALIZATION)) {
        return;
    }

    EventWrite(g_etw_provider_handle,
               &(EVENT_DESCRIPTOR){ET_ETW_EVENT_LIBRARY_SHUTDOWN, 0, 0,
                                  ET_ETW_LEVEL_INFO, 0, 0, ET_ETW_KEYWORD_INITIALIZATION},
               0, NULL);
}

#endif // _WIN32