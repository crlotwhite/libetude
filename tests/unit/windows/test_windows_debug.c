#ifdef _WIN32

#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include "libetude/platform/windows_debug.h"
#include "libetude/types.h"

// 테스트 결과 출력 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __FUNCTION__, message); \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    printf("PASS: %s - %s\n", __FUNCTION__, message)

// 디버깅 시스템 초기화 및 정리 테스트
static bool test_debug_initialization(void)
{
    // 기본 설정으로 초기화
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    // PDB 활성화 상태 확인
    bool pdb_enabled = et_windows_debug_is_pdb_enabled();
    printf("PDB 생성 활성화: %s\n", pdb_enabled ? "예" : "아니오");

    // 중복 초기화 테스트
    result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "중복 초기화 처리 실패");

    // 디버깅 시스템 정리
    et_windows_debug_shutdown();

    // 중복 정리 테스트 (크래시하지 않아야 함)
    et_windows_debug_shutdown();

    TEST_PASS("디버깅 시스템 초기화 및 정리 테스트 완료");
    return true;
}

// 사용자 정의 설정으로 초기화 테스트
static bool test_debug_custom_configuration(void)
{
    ETWindowsDebugConfig config = {
        .pdb_generation_enabled = true,
        .event_logging_enabled = true,
        .console_output_enabled = true,
        .file_logging_enabled = true,
        .log_file_path = "test_debug.log",
        .max_log_file_size = 1024 * 1024, // 1MB
        .detailed_stack_trace = true
    };

    ETResult result = et_windows_debug_init(&config);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "사용자 정의 설정 초기화 실패");

    // PDB 설정 변경 테스트
    result = et_windows_debug_configure_pdb(false);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "PDB 설정 변경 실패");

    bool pdb_enabled = et_windows_debug_is_pdb_enabled();
    TEST_ASSERT(!pdb_enabled, "PDB 비활성화 설정이 적용되지 않음");

    // PDB 다시 활성화
    result = et_windows_debug_configure_pdb(true);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "PDB 재활성화 실패");

    et_windows_debug_shutdown();

    TEST_PASS("사용자 정의 설정 테스트 완료");
    return true;
}

// Windows 이벤트 로그 테스트
static bool test_windows_event_logging(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    // 이벤트 소스 등록 테스트
    result = et_windows_debug_register_event_source();
    // 관리자 권한이 없으면 실패할 수 있음
    if (result != ET_RESULT_SUCCESS) {
        printf("WARNING: 이벤트 소스 등록 실패 (관리자 권한 필요할 수 있음)\n");
    }

    // 다양한 타입의 이벤트 로그 작성 테스트
    et_windows_debug_write_event_log(
        ET_EVENT_TYPE_INFORMATION,
        ET_EVENT_CATEGORY_GENERAL,
        1001,
        "테스트 정보 메시지"
    );

    et_windows_debug_write_event_log(
        ET_EVENT_TYPE_WARNING,
        ET_EVENT_CATEGORY_PERFORMANCE,
        1002,
        "테스트 경고 메시지"
    );

    et_windows_debug_write_event_log(
        ET_EVENT_TYPE_ERROR,
        ET_EVENT_CATEGORY_AUDIO,
        1003,
        "테스트 오류 메시지"
    );

    et_windows_debug_unregister_event_source();
    et_windows_debug_shutdown();

    TEST_PASS("Windows 이벤트 로그 테스트 완료");
    return true;
}

// 오류 로깅 테스트
static bool test_error_logging(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    // 간단한 오류 로깅 테스트
    et_windows_debug_log_error_simple(
        ET_RESULT_ERROR_INVALID_PARAMETER,
        "테스트 오류 메시지",
        "test_error_logging",
        __LINE__
    );

    // 상세한 오류 정보 구조체 생성
    ETWindowsErrorInfo error_info = {0};
    error_info.error_code = ET_RESULT_ERROR_OUT_OF_MEMORY;
    error_info.windows_error_code = ERROR_NOT_ENOUGH_MEMORY;
    error_info.error_message = "메모리 부족으로 인한 할당 실패";
    error_info.function_name = "test_error_logging";
    error_info.file_name = __FILE__;
    error_info.line_number = __LINE__;
    error_info.thread_id = GetCurrentThreadId();
    GetSystemTime(&error_info.timestamp);

    // 스택 트레이스 캡처
    et_windows_debug_capture_stack_trace(&error_info.stack_trace);

    // 상세한 오류 로깅
    et_windows_debug_log_error_detailed(&error_info);

    et_windows_debug_shutdown();

    TEST_PASS("오류 로깅 테스트 완료");
    return true;
}

// 스택 트레이스 테스트
static bool test_stack_trace_capture(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    ETWindowsStackTrace stack_trace;
    result = et_windows_debug_capture_stack_trace(&stack_trace);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "스택 트레이스 캡처 실패");

    printf("캡처된 스택 프레임 수: %lu\n", stack_trace.frame_count);
    TEST_ASSERT(stack_trace.frame_count > 0, "스택 프레임이 캡처되지 않음");

    // 스택 트레이스 출력 테스트
    et_windows_debug_print_stack_trace(&stack_trace);

    // 스택 트레이스 포맷팅 테스트
    char* formatted_trace = et_windows_debug_format_stack_trace(&stack_trace);
    if (formatted_trace) {
        printf("포맷된 스택 트레이스:\n%s\n", formatted_trace);
        free(formatted_trace);
    }

    et_windows_debug_shutdown();

    TEST_PASS("스택 트레이스 캡처 테스트 완료");
    return true;
}

// 성능 타이머 테스트
static bool test_performance_timer(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    ETWindowsPerformanceTimer timer;

    // 성능 측정 시작
    et_windows_debug_timer_start(&timer, "test_operation");

    // 짧은 작업 시뮬레이션
    Sleep(50);

    // 성능 측정 종료
    double duration = et_windows_debug_timer_end(&timer);
    printf("측정된 작업 시간: %.3f ms\n", duration);
    TEST_ASSERT(duration >= 40.0 && duration <= 100.0, "성능 측정 시간이 예상 범위를 벗어남");

    // 직접 성능 로깅 테스트
    et_windows_debug_log_performance("direct_test_operation", 25.5);

    et_windows_debug_shutdown();

    TEST_PASS("성능 타이머 테스트 완료");
    return true;
}

// 메모리 사용량 모니터링 테스트
static bool test_memory_monitoring(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    // 메모리 정보 가져오기
    ETWindowsMemoryInfo memory_info;
    result = et_windows_debug_get_memory_info(&memory_info);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "메모리 정보 가져오기 실패");

    printf("메모리 사용량 정보:\n");
    printf("  작업 집합: %.2f MB\n", memory_info.working_set_size / (1024.0 * 1024.0));
    printf("  최대 작업 집합: %.2f MB\n", memory_info.peak_working_set_size / (1024.0 * 1024.0));
    printf("  전용 메모리: %.2f MB\n", memory_info.private_usage / (1024.0 * 1024.0));
    printf("  가상 메모리: %.2f MB\n", memory_info.virtual_size / (1024.0 * 1024.0));
    printf("  페이지 폴트: %lu\n", memory_info.page_faults);

    TEST_ASSERT(memory_info.working_set_size > 0, "작업 집합 크기가 0");
    TEST_ASSERT(memory_info.private_usage > 0, "전용 메모리 사용량이 0");

    // 메모리 사용량 로깅 테스트
    et_windows_debug_log_memory_usage();

    et_windows_debug_shutdown();

    TEST_PASS("메모리 모니터링 테스트 완료");
    return true;
}

// 시스템 정보 테스트
static bool test_system_info(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    // 시스템 정보 가져오기
    ETWindowsSystemInfo system_info;
    result = et_windows_debug_get_system_info(&system_info);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "시스템 정보 가져오기 실패");

    printf("시스템 정보:\n");
    printf("  OS 버전: %s\n", system_info.os_version);
    printf("  CPU 정보: %s\n", system_info.cpu_info);
    printf("  총 메모리: %lu MB\n", system_info.total_memory_mb);
    printf("  사용 가능 메모리: %lu MB\n", system_info.available_memory_mb);
    printf("  프로세서 수: %lu\n", system_info.processor_count);

    TEST_ASSERT(strlen(system_info.os_version) > 0, "OS 버전 정보가 비어있음");
    TEST_ASSERT(system_info.processor_count > 0, "프로세서 수가 0");
    TEST_ASSERT(system_info.total_memory_mb > 0, "총 메모리가 0");

    // 시스템 정보 로깅 테스트
    et_windows_debug_log_system_info();

    et_windows_debug_shutdown();

    TEST_PASS("시스템 정보 테스트 완료");
    return true;
}

// 디버그 출력 함수 테스트
static bool test_debug_output_functions(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

    // 콘솔 출력 테스트
    et_windows_debug_output_console("콘솔 출력 테스트: %s %d\n", "문자열", 123);

    // 디버거 출력 테스트
    et_windows_debug_output_debugger("디버거 출력 테스트: %s %d\n", "문자열", 456);

    // 파일 출력 테스트
    et_windows_debug_output_file("파일 출력 테스트: %s %d\n", "문자열", 789);

    et_windows_debug_shutdown();

    TEST_PASS("디버그 출력 함수 테스트 완료");
    return true;
}

// 디버그 매크로 테스트
static bool test_debug_macros(void)
{
    ETResult result = et_windows_debug_init(NULL);
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "디버깅 시스템 초기화 실패");

#ifdef _DEBUG
    // 디버그 로그 매크로 테스트
    ET_DEBUG_LOG("디버그 로그 매크로 테스트: %s", "성공");

    // 디버그 오류 로그 매크로 테스트
    ET_DEBUG_LOG_ERROR(ET_RESULT_ERROR_INVALID_PARAMETER, "매크로를 통한 오류 로깅");

    // 성능 타이머 매크로 테스트
    ETWindowsPerformanceTimer macro_timer;
    ET_DEBUG_TIMER_START(macro_timer, "macro_test_operation");
    Sleep(10);
    double macro_duration = ET_DEBUG_TIMER_END(macro_timer);
    printf("매크로 타이머 측정 시간: %.3f ms\n", macro_duration);

    // 어서션 매크로 테스트
    ET_DEBUG_ASSERT(1 == 1, "이 어서션은 통과해야 함");
    // ET_DEBUG_ASSERT(1 == 0, "이 어서션은 실패해야 함"); // 주석 처리 (실제 실패 방지)

    printf("디버그 매크로들이 정상적으로 작동함\n");
#else
    printf("릴리즈 모드에서는 디버그 매크로가 비활성화됨\n");
#endif

    et_windows_debug_shutdown();

    TEST_PASS("디버그 매크로 테스트 완료");
    return true;
}

int main(void)
{
    printf("=== Windows 디버깅 지원 테스트 시작 ===\n\n");

    bool all_tests_passed = true;

    // 모든 테스트 실행
    all_tests_passed &= test_debug_initialization();
    all_tests_passed &= test_debug_custom_configuration();
    all_tests_passed &= test_windows_event_logging();
    all_tests_passed &= test_error_logging();
    all_tests_passed &= test_stack_trace_capture();
    all_tests_passed &= test_performance_timer();
    all_tests_passed &= test_memory_monitoring();
    all_tests_passed &= test_system_info();
    all_tests_passed &= test_debug_output_functions();
    all_tests_passed &= test_debug_macros();

    printf("\n=== 테스트 결과 ===\n");
    if (all_tests_passed) {
        printf("모든 Windows 디버깅 지원 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 Windows 디버깅 지원 테스트가 실패했습니다.\n");
        return 1;
    }
}

#else

int main(void)
{
    printf("Windows 디버깅 지원 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif // _WIN32