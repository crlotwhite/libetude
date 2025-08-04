#ifdef _WIN32

#include <stdio.h>
#include <windows.h>
#include "libetude/platform/windows_etw.h"
#include "libetude/platform/windows_debug.h"

// 예제 함수: 성능 측정이 포함된 작업
static void example_performance_operation(void)
{
    // ETW 성능 이벤트 시작
    ULONGLONG start_time;
    et_windows_etw_log_performance_start("example_operation", &start_time);

    // 디버그 타이머 시작
    ETWindowsPerformanceTimer debug_timer;
    et_windows_debug_timer_start(&debug_timer, "example_debug_operation");

    // 시뮬레이션된 작업 (100ms 대기)
    printf("예제 작업 실행 중...\n");
    Sleep(100);

    // 성능 측정 종료
    et_windows_etw_log_performance_end("example_operation", start_time);
    double duration = et_windows_debug_timer_end(&debug_timer);

    printf("작업 완료 - 측정된 시간: %.3f ms\n", duration);
}

// 예제 함수: 오류 처리 및 로깅
static void example_error_handling(void)
{
    printf("오류 처리 예제 실행 중...\n");

    // 의도적인 오류 상황 시뮬레이션
    ETErrorCode error_code = ET_RESULT_ERROR_INVALID_PARAMETER;
    const char* error_message = "예제 오류: 잘못된 매개변수가 전달됨";

    // ETW 오류 이벤트 로깅
    et_windows_etw_log_error(error_code, error_message, __FUNCTION__, __LINE__);

    // Windows 디버그 시스템을 통한 상세 오류 로깅
    et_windows_debug_log_error_simple(error_code, error_message, __FUNCTION__, __LINE__);

    printf("오류 로깅 완료\n");
}

// 예제 함수: 메모리 할당 추적
static void example_memory_tracking(void)
{
    printf("메모리 추적 예제 실행 중...\n");

    // 메모리 할당
    size_t allocation_size = 1024;
    void* memory_ptr = malloc(allocation_size);

    if (memory_ptr) {
        // ETW 메모리 할당 이벤트
        et_windows_etw_log_memory_alloc(memory_ptr, allocation_size, "malloc");

        printf("메모리 할당됨: %p (%zu bytes)\n", memory_ptr, allocation_size);

        // 메모리 사용량 로깅
        et_windows_debug_log_memory_usage();

        // 메모리 해제
        et_windows_etw_log_memory_free(memory_ptr, allocation_size);
        free(memory_ptr);

        printf("메모리 해제됨\n");
    } else {
        et_windows_etw_log_error(ET_RESULT_ERROR_OUT_OF_MEMORY,
                                "메모리 할당 실패", __FUNCTION__, __LINE__);
    }
}

// 예제 함수: 오디오 시스템 시뮬레이션
static void example_audio_simulation(void)
{
    printf("오디오 시스템 시뮬레이션 실행 중...\n");

    // 오디오 초기화 시뮬레이션
    et_windows_etw_log_audio_init("WASAPI", true);

    // 오디오 렌더링 시뮬레이션
    DWORD buffer_size = 1024;
    DWORD sample_rate = 44100;

    et_windows_etw_log_audio_render_start(buffer_size, sample_rate);

    // 렌더링 작업 시뮬레이션
    Sleep(20);

    et_windows_etw_log_audio_render_end(buffer_size, 18.5);

    printf("오디오 시뮬레이션 완료\n");
}

// 예제 함수: 스레드 생성 및 추적
static DWORD WINAPI example_worker_thread(LPVOID param)
{
    DWORD thread_id = GetCurrentThreadId();
    const char* thread_name = (const char*)param;

    // 스레드 생성 이벤트
    et_windows_etw_log_thread_created(thread_id, thread_name);

    printf("작업 스레드 시작: %s (ID: %lu)\n", thread_name, thread_id);

    // 작업 시뮬레이션
    Sleep(500);

    printf("작업 스레드 완료: %s\n", thread_name);

    // 스레드 소멸 이벤트
    et_windows_etw_log_thread_destroyed(thread_id);

    return 0;
}

static void example_threading(void)
{
    printf("스레딩 예제 실행 중...\n");

    const char* thread_name = "ExampleWorkerThread";
    HANDLE thread_handle = CreateThread(
        NULL,
        0,
        example_worker_thread,
        (LPVOID)thread_name,
        0,
        NULL
    );

    if (thread_handle) {
        // 스레드 완료 대기
        WaitForSingleObject(thread_handle, INFINITE);
        CloseHandle(thread_handle);
        printf("스레드 완료\n");
    } else {
        et_windows_etw_log_error(ET_RESULT_ERROR_PLATFORM_SPECIFIC,
                                "스레드 생성 실패", __FUNCTION__, __LINE__);
    }
}

int main(void)
{
    printf("=== LibEtude Windows 개발 도구 통합 예제 ===\n\n");

    // ETW 시스템 초기화
    printf("ETW 시스템 초기화 중...\n");
    ETResult etw_result = et_windows_etw_init();
    if (etw_result != ET_RESULT_SUCCESS) {
        printf("ETW 초기화 실패: %d\n", etw_result);
        return 1;
    }

    // Windows 디버깅 시스템 초기화
    printf("Windows 디버깅 시스템 초기화 중...\n");
    ETWindowsDebugConfig debug_config = {
        .pdb_generation_enabled = true,
        .event_logging_enabled = true,
        .console_output_enabled = true,
        .file_logging_enabled = true,
        .log_file_path = "libetude_example.log",
        .max_log_file_size = 10 * 1024 * 1024, // 10MB
        .detailed_stack_trace = true
    };

    ETResult debug_result = et_windows_debug_init(&debug_config);
    if (debug_result != ET_RESULT_SUCCESS) {
        printf("디버깅 시스템 초기화 실패: %d\n", debug_result);
        et_windows_etw_shutdown();
        return 1;
    }

    printf("초기화 완료!\n\n");

    // 라이브러리 초기화 이벤트 (이미 ETW 초기화에서 호출됨)
    printf("1. 성능 측정 예제\n");
    example_performance_operation();
    printf("\n");

    printf("2. 오류 처리 예제\n");
    example_error_handling();
    printf("\n");

    printf("3. 메모리 추적 예제\n");
    example_memory_tracking();
    printf("\n");

    printf("4. 오디오 시스템 시뮬레이션\n");
    example_audio_simulation();
    printf("\n");

    printf("5. 스레딩 예제\n");
    example_threading();
    printf("\n");

    // 시스템 정보 출력
    printf("6. 시스템 정보\n");
    et_windows_debug_log_system_info();
    printf("\n");

    // 최종 메모리 사용량 출력
    printf("7. 최종 메모리 사용량\n");
    et_windows_debug_log_memory_usage();
    printf("\n");

    // 정리
    printf("시스템 정리 중...\n");
    et_windows_debug_shutdown();
    et_windows_etw_shutdown();

    printf("=== 예제 완료 ===\n");
    return 0;
}

#else

int main(void)
{
    printf("이 예제는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif // _WIN32