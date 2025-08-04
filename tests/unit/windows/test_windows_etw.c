#ifdef _WIN32

#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include "libetude/platform/windows_etw.h"
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

// ETW 초기화 및 정리 테스트
static bool test_etw_initialization(void)
{
    // ETW 초기화
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    // 초기화 상태 확인
    bool is_enabled = et_windows_etw_is_enabled();
    // ETW가 활성화되지 않을 수도 있음 (트레이싱 세션이 없는 경우)
    printf("ETW 활성화 상태: %s\n", is_enabled ? "활성화됨" : "비활성화됨");

    // 중복 초기화 테스트
    result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "중복 초기화 처리 실패");

    // ETW 정리
    et_windows_etw_shutdown();

    // 정리 후 상태 확인
    is_enabled = et_windows_etw_is_enabled();
    TEST_ASSERT(!is_enabled, "ETW 정리 후에도 활성화 상태");

    // 중복 정리 테스트 (크래시하지 않아야 함)
    et_windows_etw_shutdown();

    TEST_PASS("ETW 초기화 및 정리 테스트 완료");
    return true;
}

// ETW 레벨 및 키워드 확인 테스트
static bool test_etw_level_keyword_check(void)
{
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    // 다양한 레벨 확인
    bool level_enabled = et_windows_etw_is_level_enabled(ET_ETW_LEVEL_INFO);
    printf("INFO 레벨 활성화: %s\n", level_enabled ? "예" : "아니오");

    level_enabled = et_windows_etw_is_level_enabled(ET_ETW_LEVEL_ERROR);
    printf("ERROR 레벨 활성화: %s\n", level_enabled ? "예" : "아니오");

    // 다양한 키워드 확인
    bool keyword_enabled = et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_PERFORMANCE);
    printf("PERFORMANCE 키워드 활성화: %s\n", keyword_enabled ? "예" : "아니오");

    keyword_enabled = et_windows_etw_is_keyword_enabled(ET_ETW_KEYWORD_AUDIO);
    printf("AUDIO 키워드 활성화: %s\n", keyword_enabled ? "예" : "아니오");

    et_windows_etw_shutdown();

    TEST_PASS("ETW 레벨 및 키워드 확인 테스트 완료");
    return true;
}

// 성능 이벤트 로깅 테스트
static bool test_etw_performance_logging(void)
{
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    // 성능 측정 시작/종료 테스트
    ULONGLONG start_time;
    et_windows_etw_log_performance_start("test_operation", &start_time);

    // 짧은 작업 시뮬레이션
    Sleep(10);

    et_windows_etw_log_performance_end("test_operation", start_time);

    // 성능 이벤트 직접 로깅 테스트
    ETETWPerformanceEvent perf_event = {
        .operation_name = "direct_test_operation",
        .duration_ms = 15.5,
        .thread_id = GetCurrentThreadId(),
        .timestamp = GetTickCount64()
    };
    et_windows_etw_log_performance_event(&perf_event);

    et_windows_etw_shutdown();

    TEST_PASS("성능 이벤트 로깅 테스트 완료");
    return true;
}

// 오류 이벤트 로깅 테스트
static bool test_etw_error_logging(void)
{
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    // 매크로를 통한 오류 로깅
    ET_ETW_LOG_ERROR(ET_RESULT_ERROR_INVALID_PARAMETER, "테스트 오류 메시지");

    // 직접 오류 로깅
    et_windows_etw_log_error(ET_RESULT_ERROR_OUT_OF_MEMORY,
                            "메모리 부족 오류",
                            "test_function",
                            123);

    // 오류 이벤트 구조체를 통한 로깅
    ETETWErrorEvent error_event = {
        .error_code = ET_RESULT_ERROR_PLATFORM_SPECIFIC,
        .error_message = "플랫폼 특화 오류",
        .function_name = "test_etw_error_logging",
        .line_number = __LINE__,
        .thread_id = GetCurrentThreadId()
    };
    et_windows_etw_log_error_event(&error_event);

    et_windows_etw_shutdown();

    TEST_PASS("오류 이벤트 로깅 테스트 완료");
    return true;
}

// 메모리 이벤트 로깅 테스트
static bool test_etw_memory_logging(void)
{
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    // 메모리 할당 시뮬레이션
    void* test_ptr = malloc(1024);
    et_windows_etw_log_memory_alloc(test_ptr, 1024, "malloc");

    // 메모리 해제 시뮬레이션
    et_windows_etw_log_memory_free(test_ptr, 1024);
    free(test_ptr);

    // 메모리 이벤트 직접 로깅
    ETETWMemoryEvent mem_event = {
        .address = (void*)0x12345678,
        .size = 2048,
        .allocation_type = "custom_allocator",
        .thread_id = GetCurrentThreadId()
    };
    et_windows_etw_log_memory_event(&mem_event, true);
    et_windows_etw_log_memory_event(&mem_event, false);

    et_windows_etw_shutdown();

    TEST_PASS("메모리 이벤트 로깅 테스트 완료");
    return true;
}

// 오디오 이벤트 로깅 테스트
static bool test_etw_audio_logging(void)
{
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    // 오디오 초기화 이벤트
    et_windows_etw_log_audio_init("WASAPI", true);
    et_windows_etw_log_audio_init("DirectSound", false);

    // 오디오 렌더링 이벤트
    et_windows_etw_log_audio_render_start(1024, 44100);
    et_windows_etw_log_audio_render_end(1024, 23.5);

    et_windows_etw_shutdown();

    TEST_PASS("오디오 이벤트 로깅 테스트 완료");
    return true;
}

// 스레딩 이벤트 로깅 테스트
static bool test_etw_threading_logging(void)
{
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    DWORD current_thread_id = GetCurrentThreadId();

    // 스레드 생성/소멸 이벤트
    et_windows_etw_log_thread_created(current_thread_id, "main_thread");
    et_windows_etw_log_thread_destroyed(current_thread_id);

    et_windows_etw_shutdown();

    TEST_PASS("스레딩 이벤트 로깅 테스트 완료");
    return true;
}

// 라이브러리 생명주기 이벤트 테스트
static bool test_etw_library_lifecycle(void)
{
    ETResult result = et_windows_etw_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "ETW 초기화 실패");

    // 라이브러리 초기화 이벤트는 et_windows_etw_init()에서 자동 호출됨
    // 추가 테스트를 위해 직접 호출
    et_windows_etw_log_library_init("1.0.0-test");

    // 라이브러리 종료 이벤트는 et_windows_etw_shutdown()에서 자동 호출됨
    et_windows_etw_shutdown();

    TEST_PASS("라이브러리 생명주기 이벤트 테스트 완료");
    return true;
}

int main(void)
{
    printf("=== Windows ETW 지원 테스트 시작 ===\n\n");

    bool all_tests_passed = true;

    // 모든 테스트 실행
    all_tests_passed &= test_etw_initialization();
    all_tests_passed &= test_etw_level_keyword_check();
    all_tests_passed &= test_etw_performance_logging();
    all_tests_passed &= test_etw_error_logging();
    all_tests_passed &= test_etw_memory_logging();
    all_tests_passed &= test_etw_audio_logging();
    all_tests_passed &= test_etw_threading_logging();
    all_tests_passed &= test_etw_library_lifecycle();

    printf("\n=== 테스트 결과 ===\n");
    if (all_tests_passed) {
        printf("모든 ETW 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 ETW 테스트가 실패했습니다.\n");
        return 1;
    }
}

#else

int main(void)
{
    printf("Windows ETW 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif // _WIN32