#ifdef _WIN32

#include "libetude/platform/windows_debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <psapi.h>
#include <dbghelp.h>
#include <versionhelpers.h>

// MiniDumpWriteDump 함수를 위한 추가 헤더
#pragma comment(lib, "dbghelp.lib")

// 전역 디버그 설정
static ETWindowsDebugConfig g_debug_config = {0};
static bool g_debug_initialized = false;
static HANDLE g_event_log_handle = NULL;
static FILE* g_log_file = NULL;
static CRITICAL_SECTION g_debug_lock;

// 내부 함수 선언
static void debug_lock_init(void);
static void debug_lock_cleanup(void);
static void debug_lock_acquire(void);
static void debug_lock_release(void);
static const char* get_error_code_string(ETErrorCode error_code);

ETResult et_windows_debug_init(const ETWindowsDebugConfig* config)
{
    if (g_debug_initialized) {
        return ET_RESULT_SUCCESS;
    }

    // 크리티컬 섹션 초기화
    debug_lock_init();

    // 설정 복사
    if (config) {
        g_debug_config = *config;
    } else {
        // 기본 설정
        g_debug_config.pdb_generation_enabled = true;
        g_debug_config.event_logging_enabled = true;
        g_debug_config.console_output_enabled = true;
        g_debug_config.file_logging_enabled = false;
        g_debug_config.log_file_path = NULL;
        g_debug_config.max_log_file_size = 10 * 1024 * 1024; // 10MB
        g_debug_config.detailed_stack_trace = true;
    }

    // Windows 이벤트 로그 등록
    if (g_debug_config.event_logging_enabled) {
        ETResult result = et_windows_debug_register_event_source();
        if (result != ET_RESULT_SUCCESS) {
            // 이벤트 로그 등록 실패는 치명적이지 않음
            g_debug_config.event_logging_enabled = false;
        }
    }

    // 파일 로깅 초기화
    if (g_debug_config.file_logging_enabled && g_debug_config.log_file_path) {
        g_log_file = fopen(g_debug_config.log_file_path, "a");
        if (!g_log_file) {
            g_debug_config.file_logging_enabled = false;
        }
    }

    // 심볼 핸들러 초기화 (스택 트레이스용)
    if (g_debug_config.detailed_stack_trace) {
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    }

    g_debug_initialized = true;

    // 시스템 정보 로깅
    et_windows_debug_log_system_info();

    return ET_RESULT_SUCCESS;
}

void et_windows_debug_shutdown(void)
{
    if (!g_debug_initialized) {
        return;
    }

    debug_lock_acquire();

    // 파일 로그 닫기
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    // Windows 이벤트 로그 등록 해제
    et_windows_debug_unregister_event_source();

    // 심볼 핸들러 정리
    if (g_debug_config.detailed_stack_trace) {
        SymCleanup(GetCurrentProcess());
    }

    g_debug_initialized = false;

    debug_lock_release();
    debug_lock_cleanup();
}

bool et_windows_debug_is_pdb_enabled(void)
{
    return g_debug_initialized && g_debug_config.pdb_generation_enabled;
}

ETResult et_windows_debug_configure_pdb(bool enable_full_debug_info)
{
    if (!g_debug_initialized) {
        return ET_RESULT_ERROR_NOT_INITIALIZED;
    }

    g_debug_config.pdb_generation_enabled = enable_full_debug_info;
    return ET_RESULT_SUCCESS;
}

ETResult et_windows_debug_register_event_source(void)
{
    if (g_event_log_handle) {
        return ET_RESULT_SUCCESS;
    }

    g_event_log_handle = RegisterEventSourceW(NULL, ET_WINDOWS_EVENT_SOURCE_NAME);
    if (!g_event_log_handle) {
        return ET_RESULT_ERROR_PLATFORM_SPECIFIC;
    }

    return ET_RESULT_SUCCESS;
}

void et_windows_debug_unregister_event_source(void)
{
    if (g_event_log_handle) {
        DeregisterEventSource(g_event_log_handle);
        g_event_log_handle = NULL;
    }
}

ETResult et_windows_debug_write_event_log(ETWindowsEventType type,
                                         ETWindowsEventCategory category,
                                         DWORD event_id,
                                         const char* message)
{
    if (!g_debug_config.event_logging_enabled || !g_event_log_handle) {
        return ET_RESULT_ERROR_NOT_INITIALIZED;
    }

    // ANSI 문자열을 유니코드로 변환
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, message, -1, NULL, 0);
    if (wide_len <= 0) {
        return ET_RESULT_ERROR_INVALID_PARAMETER;
    }

    wchar_t* wide_message = (wchar_t*)malloc(wide_len * sizeof(wchar_t));
    if (!wide_message) {
        return ET_RESULT_ERROR_OUT_OF_MEMORY;
    }

    MultiByteToWideChar(CP_UTF8, 0, message, -1, wide_message, wide_len);

    LPCWSTR strings[] = { wide_message };
    BOOL success = ReportEventW(
        g_event_log_handle,
        (WORD)type,
        (WORD)category,
        event_id,
        NULL,
        1,
        0,
        strings,
        NULL
    );

    free(wide_message);

    return success ? ET_RESULT_SUCCESS : ET_RESULT_ERROR_PLATFORM_SPECIFIC;
}

void et_windows_debug_log_error_detailed(const ETWindowsErrorInfo* error_info)
{
    if (!g_debug_initialized || !error_info) {
        return;
    }

    debug_lock_acquire();

    char detailed_message[2048];
    snprintf(detailed_message, sizeof(detailed_message),
        "LibEtude Error Details:\n"
        "  Error Code: %d (%s)\n"
        "  Windows Error: %lu\n"
        "  Message: %s\n"
        "  Function: %s\n"
        "  File: %s\n"
        "  Line: %lu\n"
        "  Thread ID: %lu\n"
        "  Timestamp: %04d-%02d-%02d %02d:%02d:%02d.%03d",
        error_info->error_code,
        get_error_code_string(error_info->error_code),
        error_info->windows_error_code,
        error_info->error_message ? error_info->error_message : "Unknown",
        error_info->function_name ? error_info->function_name : "Unknown",
        error_info->file_name ? error_info->file_name : "Unknown",
        error_info->line_number,
        error_info->thread_id,
        error_info->timestamp.wYear,
        error_info->timestamp.wMonth,
        error_info->timestamp.wDay,
        error_info->timestamp.wHour,
        error_info->timestamp.wMinute,
        error_info->timestamp.wSecond,
        error_info->timestamp.wMilliseconds);

    // 콘솔 출력
    if (g_debug_config.console_output_enabled) {
        printf("%s\n", detailed_message);
    }

    // 디버거 출력
    if (IsDebuggerPresent()) {
        OutputDebugStringA(detailed_message);
        OutputDebugStringA("\n");
    }

    // 파일 출력
    if (g_debug_config.file_logging_enabled && g_log_file) {
        fprintf(g_log_file, "%s\n", detailed_message);
        fflush(g_log_file);
    }

    // Windows 이벤트 로그
    if (g_debug_config.event_logging_enabled) {
        et_windows_debug_write_event_log(
            ET_EVENT_TYPE_ERROR,
            ET_EVENT_CATEGORY_GENERAL,
            1000 + error_info->error_code,
            detailed_message
        );
    }

    // 스택 트레이스 출력
    if (error_info->stack_trace.frame_count > 0) {
        et_windows_debug_print_stack_trace(&error_info->stack_trace);
    }

    debug_lock_release();
}

void et_windows_debug_log_error_simple(ETErrorCode error_code,
                                      const char* message,
                                      const char* function,
                                      DWORD line)
{
    ETWindowsErrorInfo error_info = {0};
    error_info.error_code = error_code;
    error_info.windows_error_code = GetLastError();
    error_info.error_message = message;
    error_info.function_name = function;
    error_info.file_name = __FILE__;
    error_info.line_number = line;
    error_info.thread_id = GetCurrentThreadId();
    GetSystemTime(&error_info.timestamp);

    // 스택 트레이스 캡처
    if (g_debug_config.detailed_stack_trace) {
        et_windows_debug_capture_stack_trace(&error_info.stack_trace);
    }

    et_windows_debug_log_error_detailed(&error_info);
}

ETResult et_windows_debug_capture_stack_trace(ETWindowsStackTrace* stack_trace)
{
    if (!stack_trace || !g_debug_config.detailed_stack_trace) {
        return ET_RESULT_ERROR_INVALID_PARAMETER;
    }

    memset(stack_trace, 0, sizeof(ETWindowsStackTrace));

    // 스택 트레이스 캡처
    stack_trace->frame_count = CaptureStackBackTrace(
        1, // 현재 함수 제외
        64, // 최대 프레임 수
        stack_trace->addresses,
        NULL
    );

    if (stack_trace->frame_count == 0) {
        return ET_RESULT_ERROR_PLATFORM_SPECIFIC;
    }

    // 심볼 정보 해석
    HANDLE process = GetCurrentProcess();
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256);
    if (!symbol) {
        return ET_RESULT_ERROR_OUT_OF_MEMORY;
    }

    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    for (DWORD i = 0; i < stack_trace->frame_count; i++) {
        // 심볼 이름 가져오기
        if (SymFromAddr(process, (DWORD64)stack_trace->addresses[i], 0, symbol)) {
            strncpy(stack_trace->symbols[i], symbol->Name, sizeof(stack_trace->symbols[i]) - 1);
            stack_trace->symbols[i][sizeof(stack_trace->symbols[i]) - 1] = '\0';
        } else {
            snprintf(stack_trace->symbols[i], sizeof(stack_trace->symbols[i]),
                    "0x%p", stack_trace->addresses[i]);
        }

        // 모듈 이름 가져오기
        HMODULE module;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              (LPCSTR)stack_trace->addresses[i], &module)) {
            GetModuleFileNameA(module, stack_trace->modules[i], MAX_PATH);
        } else {
            strcpy(stack_trace->modules[i], "Unknown");
        }

        // 라인 번호 가져오기
        IMAGEHLP_LINE64 line = {0};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD displacement;
        if (SymGetLineFromAddr64(process, (DWORD64)stack_trace->addresses[i],
                                &displacement, &line)) {
            stack_trace->line_numbers[i] = line.LineNumber;
        } else {
            stack_trace->line_numbers[i] = 0;
        }
    }

    free(symbol);
    return ET_RESULT_SUCCESS;
}

void et_windows_debug_print_stack_trace(const ETWindowsStackTrace* stack_trace)
{
    if (!stack_trace || stack_trace->frame_count == 0) {
        return;
    }

    debug_lock_acquire();

    const char* header = "\nStack Trace:";

    if (g_debug_config.console_output_enabled) {
        printf("%s\n", header);
    }

    if (IsDebuggerPresent()) {
        OutputDebugStringA(header);
        OutputDebugStringA("\n");
    }

    if (g_debug_config.file_logging_enabled && g_log_file) {
        fprintf(g_log_file, "%s\n", header);
    }

    for (DWORD i = 0; i < stack_trace->frame_count; i++) {
        char frame_info[512];
        snprintf(frame_info, sizeof(frame_info),
                "  [%2lu] %s (%s:%lu) - %s",
                i,
                stack_trace->symbols[i],
                strrchr(stack_trace->modules[i], '\\') ?
                    strrchr(stack_trace->modules[i], '\\') + 1 : stack_trace->modules[i],
                stack_trace->line_numbers[i],
                stack_trace->modules[i]);

        if (g_debug_config.console_output_enabled) {
            printf("%s\n", frame_info);
        }

        if (IsDebuggerPresent()) {
            OutputDebugStringA(frame_info);
            OutputDebugStringA("\n");
        }

        if (g_debug_config.file_logging_enabled && g_log_file) {
            fprintf(g_log_file, "%s\n", frame_info);
        }
    }

    if (g_debug_config.file_logging_enabled && g_log_file) {
        fflush(g_log_file);
    }

    debug_lock_release();
}

void et_windows_debug_output_console(const char* format, ...)
{
    if (!g_debug_config.console_output_enabled) {
        return;
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void et_windows_debug_output_debugger(const char* format, ...)
{
    if (!IsDebuggerPresent()) {
        return;
    }

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    OutputDebugStringA(buffer);
}

void et_windows_debug_output_file(const char* format, ...)
{
    if (!g_debug_config.file_logging_enabled || !g_log_file) {
        return;
    }

    debug_lock_acquire();

    va_list args;
    va_start(args, format);
    vfprintf(g_log_file, format, args);
    va_end(args);
    fflush(g_log_file);

    debug_lock_release();
}

void et_windows_debug_timer_start(ETWindowsPerformanceTimer* timer, const char* operation_name)
{
    if (!timer) {
        return;
    }

    timer->operation_name = operation_name;
    QueryPerformanceFrequency(&timer->frequency);
    QueryPerformanceCounter(&timer->start_time);
}

double et_windows_debug_timer_end(ETWindowsPerformanceTimer* timer)
{
    if (!timer) {
        return 0.0;
    }

    QueryPerformanceCounter(&timer->end_time);

    double duration_ms = (double)(timer->end_time.QuadPart - timer->start_time.QuadPart) * 1000.0 /
                        (double)timer->frequency.QuadPart;

    if (timer->operation_name) {
        et_windows_debug_log_performance(timer->operation_name, duration_ms);
    }

    return duration_ms;
}

void et_windows_debug_log_performance(const char* operation_name, double duration_ms)
{
    if (!g_debug_initialized) {
        return;
    }

    char perf_message[256];
    snprintf(perf_message, sizeof(perf_message),
            "Performance: %s completed in %.3f ms (Thread: %lu)",
            operation_name ? operation_name : "Unknown",
            duration_ms,
            GetCurrentThreadId());

    if (g_debug_config.console_output_enabled) {
        printf("[PERF] %s\n", perf_message);
    }

    if (IsDebuggerPresent()) {
        OutputDebugStringA("[PERF] ");
        OutputDebugStringA(perf_message);
        OutputDebugStringA("\n");
    }

    if (g_debug_config.file_logging_enabled && g_log_file) {
        debug_lock_acquire();
        fprintf(g_log_file, "[PERF] %s\n", perf_message);
        fflush(g_log_file);
        debug_lock_release();
    }
}

ETResult et_windows_debug_get_memory_info(ETWindowsMemoryInfo* memory_info)
{
    if (!memory_info) {
        return ET_RESULT_ERROR_INVALID_PARAMETER;
    }

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return ET_RESULT_ERROR_PLATFORM_SPECIFIC;
    }

    memory_info->working_set_size = pmc.WorkingSetSize;
    memory_info->peak_working_set_size = pmc.PeakWorkingSetSize;
    memory_info->private_usage = pmc.PrivateUsage;
    memory_info->virtual_size = pmc.PagefileUsage;
    memory_info->page_faults = pmc.PageFaultCount;

    return ET_RESULT_SUCCESS;
}

void et_windows_debug_log_memory_usage(void)
{
    ETWindowsMemoryInfo memory_info;
    if (et_windows_debug_get_memory_info(&memory_info) != ET_RESULT_SUCCESS) {
        return;
    }

    char mem_message[512];
    snprintf(mem_message, sizeof(mem_message),
            "Memory Usage: Working Set: %.2f MB, Peak: %.2f MB, Private: %.2f MB, Virtual: %.2f MB, Page Faults: %lu",
            memory_info.working_set_size / (1024.0 * 1024.0),
            memory_info.peak_working_set_size / (1024.0 * 1024.0),
            memory_info.private_usage / (1024.0 * 1024.0),
            memory_info.virtual_size / (1024.0 * 1024.0),
            memory_info.page_faults);

    if (g_debug_config.console_output_enabled) {
        printf("[MEMORY] %s\n", mem_message);
    }

    if (g_debug_config.file_logging_enabled && g_log_file) {
        debug_lock_acquire();
        fprintf(g_log_file, "[MEMORY] %s\n", mem_message);
        fflush(g_log_file);
        debug_lock_release();
    }
}

ETResult et_windows_debug_get_system_info(ETWindowsSystemInfo* system_info)
{
    if (!system_info) {
        return ET_RESULT_ERROR_INVALID_PARAMETER;
    }

    memset(system_info, 0, sizeof(ETWindowsSystemInfo));

    // OS 버전 정보
    OSVERSIONINFOEXA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);

    if (GetVersionExA((OSVERSIONINFOA*)&osvi)) {
        snprintf(system_info->os_version, sizeof(system_info->os_version),
                "Windows %lu.%lu Build %lu %s",
                osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber,
                osvi.szCSDVersion);
    } else {
        strcpy(system_info->os_version, "Windows (Unknown Version)");
    }

    // CPU 정보
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    system_info->processor_count = si.dwNumberOfProcessors;

    snprintf(system_info->cpu_info, sizeof(system_info->cpu_info),
            "Processors: %lu, Architecture: %u",
            si.dwNumberOfProcessors, si.wProcessorArchitecture);

    // 메모리 정보
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        system_info->total_memory_mb = (DWORD)(memStatus.ullTotalPhys / (1024 * 1024));
        system_info->available_memory_mb = (DWORD)(memStatus.ullAvailPhys / (1024 * 1024));
    }

    return ET_RESULT_SUCCESS;
}

void et_windows_debug_log_system_info(void)
{
    ETWindowsSystemInfo system_info;
    if (et_windows_debug_get_system_info(&system_info) != ET_RESULT_SUCCESS) {
        return;
    }

    char sys_message[1024];
    snprintf(sys_message, sizeof(sys_message),
            "System Information:\n"
            "  OS: %s\n"
            "  CPU: %s\n"
            "  Total Memory: %lu MB\n"
            "  Available Memory: %lu MB",
            system_info.os_version,
            system_info.cpu_info,
            system_info.total_memory_mb,
            system_info.available_memory_mb);

    if (g_debug_config.console_output_enabled) {
        printf("[SYSTEM] %s\n", sys_message);
    }

    if (g_debug_config.file_logging_enabled && g_log_file) {
        debug_lock_acquire();
        fprintf(g_log_file, "[SYSTEM] %s\n", sys_message);
        fflush(g_log_file);
        debug_lock_release();
    }

    if (g_debug_config.event_logging_enabled) {
        et_windows_debug_write_event_log(
            ET_EVENT_TYPE_INFORMATION,
            ET_EVENT_CATEGORY_GENERAL,
            2000,
            sys_message
        );
    }
}

// 내부 함수 구현
static void debug_lock_init(void)
{
    InitializeCriticalSection(&g_debug_lock);
}

static void debug_lock_cleanup(void)
{
    DeleteCriticalSection(&g_debug_lock);
}

static void debug_lock_acquire(void)
{
    EnterCriticalSection(&g_debug_lock);
}

static void debug_lock_release(void)
{
    LeaveCriticalSection(&g_debug_lock);
}

ETResult et_windows_debug_create_minidump(const char* dump_file_path,
                                         EXCEPTION_POINTERS* exception_info)
{
    if (!dump_file_path) {
        return ET_RESULT_ERROR_INVALID_PARAMETER;
    }

    HANDLE dump_file = CreateFileA(
        dump_file_path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (dump_file == INVALID_HANDLE_VALUE) {
        return ET_RESULT_ERROR_PLATFORM_SPECIFIC;
    }

    MINIDUMP_EXCEPTION_INFORMATION mdei = {0};
    if (exception_info) {
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = exception_info;
        mdei.ClientPointers = FALSE;
    }

    BOOL success = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        dump_file,
        MiniDumpNormal,
        exception_info ? &mdei : NULL,
        NULL,
        NULL
    );

    CloseHandle(dump_file);

    return success ? ET_RESULT_SUCCESS : ET_RESULT_ERROR_PLATFORM_SPECIFIC;
}

char* et_windows_debug_format_stack_trace(const ETWindowsStackTrace* stack_trace)
{
    if (!stack_trace || stack_trace->frame_count == 0) {
        return NULL;
    }

    // 대략적인 크기 계산 (각 프레임당 약 200바이트)
    size_t buffer_size = stack_trace->frame_count * 200 + 100;
    char* buffer = (char*)malloc(buffer_size);
    if (!buffer) {
        return NULL;
    }

    strcpy(buffer, "Stack Trace:\n");
    size_t offset = strlen(buffer);

    for (DWORD i = 0; i < stack_trace->frame_count; i++) {
        int written = snprintf(
            buffer + offset,
            buffer_size - offset,
            "  [%2lu] %s (%s:%lu) - %s\n",
            i,
            stack_trace->symbols[i],
            strrchr(stack_trace->modules[i], '\\') ?
                strrchr(stack_trace->modules[i], '\\') + 1 : stack_trace->modules[i],
            stack_trace->line_numbers[i],
            stack_trace->modules[i]
        );

        if (written < 0 || (size_t)written >= buffer_size - offset) {
            break; // 버퍼 오버플로우 방지
        }

        offset += written;
    }

    return buffer;
}

static const char* get_error_code_string(ETErrorCode error_code)
{
    switch (error_code) {
        case ET_RESULT_SUCCESS: return "SUCCESS";
        case ET_RESULT_ERROR_INVALID_PARAMETER: return "INVALID_PARAMETER";
        case ET_RESULT_ERROR_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case ET_RESULT_ERROR_NOT_INITIALIZED: return "NOT_INITIALIZED";
        case ET_RESULT_ERROR_PLATFORM_SPECIFIC: return "PLATFORM_SPECIFIC";
        default: return "UNKNOWN";
    }
}

#endif // _WIN32