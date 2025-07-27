/**
 * @file test_error_handling.c
 * @brief 오류 처리 및 로깅 시스템 단위 테스트
 */

#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

// 테스트 결과 카운터
static int tests_passed = 0;
static int tests_failed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

// 콜백 테스트용 전역 변수
static bool error_callback_called = false;
static ETError last_callback_error;
static bool log_callback_called = false;
static ETLogLevel last_log_level;
static char last_log_message[256];

// 테스트용 오류 콜백
static void test_error_callback(const ETError* error, void* user_data) {
    error_callback_called = true;
    last_callback_error = *error;
    int* counter = (int*)user_data;
    if (counter) {
        (*counter)++;
    }
}

// 테스트용 로그 콜백
static void test_log_callback(ETLogLevel level, const char* message, void* user_data) {
    log_callback_called = true;
    last_log_level = level;
    strncpy(last_log_message, message, sizeof(last_log_message) - 1);
    last_log_message[sizeof(last_log_message) - 1] = '\0';

    int* counter = (int*)user_data;
    if (counter) {
        (*counter)++;
    }
}

// 기본 오류 처리 테스트
static void test_basic_error_handling(void) {
    printf("\n=== 기본 오류 처리 테스트 ===\n");

    // 초기 상태 확인
    const ETError* error = et_get_last_error();
    TEST_ASSERT(error == NULL, "초기 상태에서는 오류가 없어야 함");

    // 오류 설정
    ET_SET_ERROR(LIBETUDE_ERROR_INVALID_ARGUMENT, "테스트 오류 메시지: %d", 42);

    // 오류 확인
    error = et_get_last_error();
    TEST_ASSERT(error != NULL, "오류가 설정되어야 함");
    TEST_ASSERT(error->code == LIBETUDE_ERROR_INVALID_ARGUMENT, "오류 코드가 올바르게 설정되어야 함");
    TEST_ASSERT(strstr(error->message, "테스트 오류 메시지: 42") != NULL, "오류 메시지가 올바르게 포맷되어야 함");
    TEST_ASSERT(error->line > 0, "라인 번호가 설정되어야 함");
    TEST_ASSERT(error->file != NULL, "파일명이 설정되어야 함");
    TEST_ASSERT(error->function != NULL, "함수명이 설정되어야 함");
    TEST_ASSERT(error->timestamp > 0, "타임스탬프가 설정되어야 함");

    // 오류 문자열 변환 테스트
    const char* error_str = et_error_string(LIBETUDE_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(error_str != NULL, "오류 문자열이 반환되어야 함");
    TEST_ASSERT(strlen(error_str) > 0, "오류 문자열이 비어있지 않아야 함");

    // 오류 지우기
    et_clear_error();
    error = et_get_last_error();
    TEST_ASSERT(error == NULL, "오류가 지워져야 함");
}

// 오류 콜백 테스트
static void test_error_callback_functionality(void) {
    printf("\n=== 오류 콜백 테스트 ===\n");

    int callback_count = 0;
    error_callback_called = false;

    // 콜백 설정
    et_set_error_callback(test_error_callback, &callback_count);

    // 오류 발생
    ET_SET_ERROR(LIBETUDE_ERROR_OUT_OF_MEMORY, "메모리 부족 테스트");

    // 콜백 호출 확인
    TEST_ASSERT(error_callback_called, "오류 콜백이 호출되어야 함");
    TEST_ASSERT(callback_count == 1, "콜백 카운터가 증가해야 함");
    TEST_ASSERT(last_callback_error.code == LIBETUDE_ERROR_OUT_OF_MEMORY, "콜백에서 올바른 오류 코드를 받아야 함");

    // 콜백 제거
    et_clear_error_callback();
    error_callback_called = false;
    callback_count = 0;

    // 오류 발생 (콜백 없음)
    ET_SET_ERROR(LIBETUDE_ERROR_IO, "IO 오류 테스트");
    TEST_ASSERT(!error_callback_called, "콜백이 제거된 후에는 호출되지 않아야 함");
    TEST_ASSERT(callback_count == 0, "콜백 카운터가 증가하지 않아야 함");
}

// 기본 로깅 테스트
static void test_basic_logging(void) {
    printf("\n=== 기본 로깅 테스트 ===\n");

    // 로그 레벨 설정 및 확인
    et_set_log_level(ET_LOG_WARNING);
    ETLogLevel level = et_get_log_level();
    TEST_ASSERT(level == ET_LOG_WARNING, "로그 레벨이 올바르게 설정되어야 함");

    // 로그 레벨 문자열 테스트
    const char* level_str = et_log_level_string(ET_LOG_ERROR);
    TEST_ASSERT(level_str != NULL, "로그 레벨 문자열이 반환되어야 함");
    TEST_ASSERT(strcmp(level_str, "ERROR") == 0, "로그 레벨 문자열이 올바르게 반환되어야 함");

    // 다양한 로그 레벨 테스트
    printf("다음 로그들이 출력되어야 합니다 (WARNING 이상만):\n");
    ET_LOG_DEBUG("이 디버그 메시지는 출력되지 않아야 합니다");
    ET_LOG_INFO("이 정보 메시지는 출력되지 않아야 합니다");
    ET_LOG_WARNING("이 경고 메시지는 출력되어야 합니다");
    ET_LOG_ERROR("이 오류 메시지는 출력되어야 합니다");
    ET_LOG_FATAL("이 치명적 오류 메시지는 출력되어야 합니다");

    // 로그 레벨을 DEBUG로 변경
    et_set_log_level(ET_LOG_DEBUG);
    printf("로그 레벨을 DEBUG로 변경 후:\n");
    ET_LOG_DEBUG("이 디버그 메시지는 이제 출력되어야 합니다");
}

// 로그 콜백 테스트
static void test_log_callback_functionality(void) {
    printf("\n=== 로그 콜백 테스트 ===\n");

    int callback_count = 0;
    log_callback_called = false;

    // 콜백 설정
    et_set_log_callback(test_log_callback, &callback_count);
    et_set_log_level(ET_LOG_DEBUG);

    // 로그 출력
    ET_LOG_INFO("콜백 테스트 메시지");

    // 콜백 호출 확인
    TEST_ASSERT(log_callback_called, "로그 콜백이 호출되어야 함");
    TEST_ASSERT(callback_count == 1, "콜백 카운터가 증가해야 함");
    TEST_ASSERT(last_log_level == ET_LOG_INFO, "콜백에서 올바른 로그 레벨을 받아야 함");
    TEST_ASSERT(strstr(last_log_message, "콜백 테스트 메시지") != NULL, "콜백에서 올바른 메시지를 받아야 함");

    // 콜백 제거
    et_clear_log_callback();
    log_callback_called = false;
    callback_count = 0;

    // 로그 출력 (콜백 없음)
    printf("콜백 제거 후 기본 출력으로 전환:\n");
    ET_LOG_INFO("기본 출력 테스트 메시지");
    TEST_ASSERT(!log_callback_called, "콜백이 제거된 후에는 호출되지 않아야 함");
}

// 스레드 안전성 테스트용 구조체
typedef struct {
    int thread_id;
    int error_count;
    int log_count;
} ThreadTestData;

// 스레드 테스트 함수
static void* thread_test_func(void* arg) {
    ThreadTestData* data = (ThreadTestData*)arg;

    for (int i = 0; i < 10; i++) {
        // 각 스레드에서 고유한 오류 설정
        ET_SET_ERROR(LIBETUDE_ERROR_RUNTIME, "스레드 %d 오류 %d", data->thread_id, i);

        // 오류 확인
        const ETError* error = et_get_last_error();
        if (error && error->code == LIBETUDE_ERROR_RUNTIME) {
            data->error_count++;
        }

        // 로그 출력
        ET_LOG_INFO("스레드 %d 로그 %d", data->thread_id, i);
        data->log_count++;

        // 오류 지우기
        et_clear_error();

        // 짧은 대기
        usleep(1000); // 1ms
    }

    return NULL;
}

// 스레드 안전성 테스트
static void test_thread_safety(void) {
    printf("\n=== 스레드 안전성 테스트 ===\n");

    const int num_threads = 5;
    pthread_t threads[num_threads];
    ThreadTestData thread_data[num_threads];

    // 스레드 생성 및 실행
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].error_count = 0;
        thread_data[i].log_count = 0;

        int result = pthread_create(&threads[i], NULL, thread_test_func, &thread_data[i]);
        TEST_ASSERT(result == 0, "스레드 생성이 성공해야 함");
    }

    // 스레드 종료 대기
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // 결과 확인
    int total_errors = 0, total_logs = 0;
    for (int i = 0; i < num_threads; i++) {
        TEST_ASSERT(thread_data[i].error_count == 10, "각 스레드에서 모든 오류가 처리되어야 함");
        TEST_ASSERT(thread_data[i].log_count == 10, "각 스레드에서 모든 로그가 출력되어야 함");
        total_errors += thread_data[i].error_count;
        total_logs += thread_data[i].log_count;
    }

    TEST_ASSERT(total_errors == num_threads * 10, "전체 오류 수가 올바르게 계산되어야 함");
    TEST_ASSERT(total_logs == num_threads * 10, "전체 로그 수가 올바르게 계산되어야 함");
}

// 편의 매크로 테스트용 함수들
static ETErrorCode test_valid_ptr_func(void) {
    char* valid_ptr = "test";
    ET_CHECK_NULL(valid_ptr, "유효한 포인터 테스트");
    return LIBETUDE_SUCCESS;
}

static ETErrorCode test_null_ptr_func(void) {
    char* null_ptr = NULL;
    ET_CHECK_NULL(null_ptr, "NULL 포인터 테스트");
    return LIBETUDE_SUCCESS;
}

// 편의 매크로 테스트
static void test_convenience_macros(void) {
    printf("\n=== 편의 매크로 테스트 ===\n");

    // 유효한 포인터 테스트
    ETErrorCode result = test_valid_ptr_func();
    TEST_ASSERT(result == LIBETUDE_SUCCESS, "유효한 포인터에 대해 성공해야 함");

    // NULL 포인터 테스트
    et_clear_error(); // 이전 오류 지우기
    result = test_null_ptr_func();
    TEST_ASSERT(result == LIBETUDE_ERROR_INVALID_ARGUMENT, "NULL 포인터에 대해 오류를 반환해야 함");

    const ETError* error = et_get_last_error();
    TEST_ASSERT(error != NULL, "오류가 설정되어야 함");
    TEST_ASSERT(error->code == LIBETUDE_ERROR_INVALID_ARGUMENT, "올바른 오류 코드가 설정되어야 함");
}

// 메인 테스트 함수
int main(void) {
    printf("LibEtude 오류 처리 및 로깅 시스템 테스트 시작\n");
    printf("================================================\n");

    // 시스템 초기화
    ETErrorCode init_result = et_init_logging();
    TEST_ASSERT(init_result == LIBETUDE_SUCCESS, "로깅 시스템 초기화가 성공해야 함");

    // 테스트 실행
    test_basic_error_handling();
    test_error_callback_functionality();
    test_basic_logging();
    test_log_callback_functionality();
    test_thread_safety();
    test_convenience_macros();

    // 시스템 정리
    et_cleanup_logging();

    // 결과 출력
    printf("\n================================================\n");
    printf("테스트 결과: %d개 통과, %d개 실패\n", tests_passed, tests_failed);

    if (tests_failed == 0) {
        printf("모든 테스트가 통과했습니다! ✓\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다. ✗\n");
        return 1;
    }
}