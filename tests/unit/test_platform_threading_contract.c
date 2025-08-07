/**
 * @file test_platform_threading_contract.c
 * @brief 스레딩 인터페이스 계약 검증 테스트
 */

#include "test_platform_abstraction.h"

// 테스트용 스레드 함수
static void* test_thread_func(void* arg) {
    int* counter = (int*)arg;
    (*counter)++;
    return NULL;
}

static void* test_thread_sleep_func(void* arg) {
    int* sleep_ms = (int*)arg;

    // 플랫폼 인터페이스를 통한 슬립
    ETPlatformInterface* platform = et_platform_get_interface();
    if (platform && platform->system) {
        platform->system->sleep(*sleep_ms);
    }

    return NULL;
}

/**
 * @brief 스레딩 인터페이스 계약 검증 테스트
 * @return 테스트 결과
 */
ETResult test_threading_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->threading);

    ETThreadInterface* threading = platform->threading;

    // 필수 함수 포인터 검증
    TEST_ASSERT_NOT_NULL(threading->create_thread);
    TEST_ASSERT_NOT_NULL(threading->join_thread);
    TEST_ASSERT_NOT_NULL(threading->detach_thread);
    TEST_ASSERT_NOT_NULL(threading->destroy_thread);
    TEST_ASSERT_NOT_NULL(threading->set_thread_priority);
    TEST_ASSERT_NOT_NULL(threading->set_thread_affinity);
    TEST_ASSERT_NOT_NULL(threading->get_current_thread_id);
    TEST_ASSERT_NOT_NULL(threading->create_mutex);
    TEST_ASSERT_NOT_NULL(threading->lock_mutex);
    TEST_ASSERT_NOT_NULL(threading->unlock_mutex);
    TEST_ASSERT_NOT_NULL(threading->try_lock_mutex);
    TEST_ASSERT_NOT_NULL(threading->destroy_mutex);
    TEST_ASSERT_NOT_NULL(threading->create_semaphore);
    TEST_ASSERT_NOT_NULL(threading->wait_semaphore);
    TEST_ASSERT_NOT_NULL(threading->post_semaphore);
    TEST_ASSERT_NOT_NULL(threading->destroy_semaphore);
    TEST_ASSERT_NOT_NULL(threading->create_condition);
    TEST_ASSERT_NOT_NULL(threading->wait_condition);
    TEST_ASSERT_NOT_NULL(threading->signal_condition);
    TEST_ASSERT_NOT_NULL(threading->broadcast_condition);
    TEST_ASSERT_NOT_NULL(threading->destroy_condition);

    // 기본 스레드 생성/조인 테스트
    int counter = 0;
    ETThread* thread = NULL;

    ETResult result = threading->create_thread(&thread, test_thread_func, &counter);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(thread);

    void* thread_result = NULL;
    result = threading->join_thread(thread, &thread_result);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT_EQUAL(1, counter); // 스레드에서 증가시킨 값

    threading->destroy_thread(thread);

    return ET_SUCCESS;
}

/**
 * @brief 뮤텍스 기능 테스트
 * @return 테스트 결과
 */
ETResult test_threading_mutex(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->threading);

    ETThreadInterface* threading = platform->threading;

    // 뮤텍스 생성
    ETMutex* mutex = NULL;
    ETResult result = threading->create_mutex(&mutex);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(mutex);

    // 뮤텍스 잠금
    result = threading->lock_mutex(mutex);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // try_lock 테스트 (이미 잠겨있으므로 실패해야 함)
    result = threading->try_lock_mutex(mutex);
    TEST_ASSERT(result == ET_ERROR_WOULD_BLOCK || result == ET_ERROR_BUSY);

    // 뮤텍스 해제
    result = threading->unlock_mutex(mutex);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 다시 try_lock (성공해야 함)
    result = threading->try_lock_mutex(mutex);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 해제
    result = threading->unlock_mutex(mutex);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 뮤텍스 파괴
    threading->destroy_mutex(mutex);

    return ET_SUCCESS;
}

/**
 * @brief 세마포어 기능 테스트
 * @return 테스트 결과
 */
ETResult test_threading_semaphore(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->threading);

    ETThreadInterface* threading = platform->threading;

    // 세마포어 생성 (초기값 2)
    ETSemaphore* semaphore = NULL;
    ETResult result = threading->create_semaphore(&semaphore, 2);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(semaphore);

    // 첫 번째 대기 (성공해야 함)
    result = threading->wait_semaphore(semaphore);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 두 번째 대기 (성공해야 함)
    result = threading->wait_semaphore(semaphore);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 신호 전송
    result = threading->post_semaphore(semaphore);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 다시 대기 (성공해야 함)
    result = threading->wait_semaphore(semaphore);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 세마포어 파괴
    threading->destroy_semaphore(semaphore);

    return ET_SUCCESS;
}

/**
 * @brief 조건 변수 기능 테스트
 * @return 테스트 결과
 */
ETResult test_threading_condition(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->threading);

    ETThreadInterface* threading = platform->threading;

    // 뮤텍스와 조건 변수 생성
    ETMutex* mutex = NULL;
    ETCondition* condition = NULL;

    ETResult result = threading->create_mutex(&mutex);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(mutex);

    result = threading->create_condition(&condition);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(condition);

    // 신호 전송 테스트
    result = threading->signal_condition(condition);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 브로드캐스트 테스트
    result = threading->broadcast_condition(condition);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 정리
    threading->destroy_condition(condition);
    threading->destroy_mutex(mutex);

    return ET_SUCCESS;
}

/**
 * @brief 스레드 속성 테스트
 * @return 테스트 결과
 */
ETResult test_threading_attributes(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->threading);

    ETThreadInterface* threading = platform->threading;

    // 현재 스레드 ID 조회
    ETThreadID current_id;
    ETResult result = threading->get_current_thread_id(&current_id);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT(current_id != 0);

    // 테스트 스레드 생성
    int sleep_time = 10;
    ETThread* thread = NULL;

    result = threading->create_thread(&thread, test_thread_sleep_func, &sleep_time);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT_NOT_NULL(thread);

    // 스레드 우선순위 설정 테스트
    result = threading->set_thread_priority(thread, ET_THREAD_PRIORITY_NORMAL);
    TEST_ASSERT(result == ET_SUCCESS || result == ET_ERROR_NOT_SUPPORTED);

    // CPU 친화성 설정 테스트 (첫 번째 CPU)
    result = threading->set_thread_affinity(thread, 0);
    TEST_ASSERT(result == ET_SUCCESS || result == ET_ERROR_NOT_SUPPORTED);

    // 스레드 조인
    void* thread_result = NULL;
    result = threading->join_thread(thread, &thread_result);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    threading->destroy_thread(thread);

    return ET_SUCCESS;
}

/**
 * @brief 스레딩 오류 조건 테스트
 * @return 테스트 결과
 */
ETResult test_threading_error_conditions(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->threading);

    ETThreadInterface* threading = platform->threading;

    // NULL 포인터 테스트
    ETResult result = threading->create_thread(NULL, test_thread_func, NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->create_thread(NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->join_thread(NULL, NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->detach_thread(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    threading->destroy_thread(NULL); // 일반적으로 안전해야 함

    result = threading->set_thread_priority(NULL, ET_THREAD_PRIORITY_NORMAL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->set_thread_affinity(NULL, 0);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->get_current_thread_id(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    // 뮤텍스 NULL 포인터 테스트
    result = threading->create_mutex(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->lock_mutex(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->unlock_mutex(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->try_lock_mutex(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    threading->destroy_mutex(NULL); // 일반적으로 안전해야 함

    // 세마포어 NULL 포인터 테스트
    result = threading->create_semaphore(NULL, 1);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->wait_semaphore(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->post_semaphore(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    threading->destroy_semaphore(NULL); // 일반적으로 안전해야 함

    // 조건 변수 NULL 포인터 테스트
    result = threading->create_condition(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->wait_condition(NULL, NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->signal_condition(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = threading->broadcast_condition(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    threading->destroy_condition(NULL); // 일반적으로 안전해야 함

    return ET_SUCCESS;
}