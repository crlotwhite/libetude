/**
 * @file test_threading.c
 * @brief 스레딩 추상화 레이어 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/threading.h"
#include "libetude/platform/factory.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// 테스트 헬퍼 함수들
// ============================================================================

static int g_test_count = 0;
static int g_test_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        g_test_count++; \
        if (condition) { \
            g_test_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

// ============================================================================
// 테스트용 스레드 함수들
// ============================================================================

static void* simple_thread_func(void* arg) {
    int* value = (int*)arg;
    *value = 42;
    return value;
}

static void* counter_thread_func(void* arg) {
    int* counter = (int*)arg;
    for (int i = 0; i < 1000; i++) {
        (*counter)++;
    }
    return NULL;
}

// ============================================================================
// 스레드 기본 기능 테스트
// ============================================================================

static void test_thread_creation_and_join(ETThreadInterface* interface) {
    printf("\n=== 스레드 생성 및 조인 테스트 ===\n");

    ETThread* thread = NULL;
    int test_value = 0;

    // 스레드 생성
    ETResult result = interface->create_thread(&thread, simple_thread_func, &test_value);
    TEST_ASSERT(result == ET_SUCCESS, "스레드 생성 성공");
    TEST_ASSERT(thread != NULL, "스레드 핸들이 유효함");

    // 스레드 조인
    void* thread_result = NULL;
    result = interface->join_thread(thread, &thread_result);
    TEST_ASSERT(result == ET_SUCCESS, "스레드 조인 성공");
    TEST_ASSERT(test_value == 42, "스레드 함수가 올바르게 실행됨");
    TEST_ASSERT(thread_result == &test_value, "스레드 반환값이 올바름");

    // 스레드 해제
    interface->destroy_thread(thread);
}

static void test_thread_attributes(ETThreadInterface* interface) {
    printf("\n=== 스레드 속성 테스트 ===\n");

    ETThreadAttributes attributes;
    et_thread_attributes_init(&attributes);

    // 속성 설정
    attributes.priority = ET_THREAD_PRIORITY_HIGH;
    attributes.stack_size = 65536;  // 64KB
    strncpy(attributes.name, "TestThread", sizeof(attributes.name) - 1);

    ETThread* thread = NULL;
    int test_value = 0;

    // 속성을 사용한 스레드 생성
    ETResult result = interface->create_thread_with_attributes(&thread, simple_thread_func,
                                                               &test_value, &attributes);
    TEST_ASSERT(result == ET_SUCCESS, "속성을 사용한 스레드 생성 성공");

    // 우선순위 확인
    ETThreadPriority priority;
    result = interface->get_thread_priority(thread, &priority);
    if (result == ET_SUCCESS) {
        TEST_ASSERT(priority == ET_THREAD_PRIORITY_HIGH, "스레드 우선순위가 올바르게 설정됨");
    }

    // 스레드 조인 및 해제
    interface->join_thread(thread, NULL);
    interface->destroy_thread(thread);
}

static void test_thread_detach(ETThreadInterface* interface) {
    printf("\n=== 스레드 분리 테스트 ===\n");

    ETThread* thread = NULL;
    int test_value = 0;

    // 스레드 생성
    ETResult result = interface->create_thread(&thread, simple_thread_func, &test_value);
    TEST_ASSERT(result == ET_SUCCESS, "스레드 생성 성공");

    // 스레드 분리
    result = interface->detach_thread(thread);
    TEST_ASSERT(result == ET_SUCCESS, "스레드 분리 성공");

    // 분리된 스레드는 조인할 수 없음
    result = interface->join_thread(thread, NULL);
    TEST_ASSERT(result != ET_SUCCESS, "분리된 스레드는 조인할 수 없음");

    // 스레드 해제
    interface->destroy_thread(thread);
}

// ============================================================================
// 뮤텍스 테스트
// ============================================================================

static void test_mutex_basic(ETThreadInterface* interface) {
    printf("\n=== 뮤텍스 기본 기능 테스트 ===\n");

    ETMutex* mutex = NULL;

    // 뮤텍스 생성
    ETResult result = interface->create_mutex(&mutex);
    TEST_ASSERT(result == ET_SUCCESS, "뮤텍스 생성 성공");
    TEST_ASSERT(mutex != NULL, "뮤텍스 핸들이 유효함");

    // 뮤텍스 잠금
    result = interface->lock_mutex(mutex);
    TEST_ASSERT(result == ET_SUCCESS, "뮤텍스 잠금 성공");

    // 뮤텍스 잠금 해제
    result = interface->unlock_mutex(mutex);
    TEST_ASSERT(result == ET_SUCCESS, "뮤텍스 잠금 해제 성공");

    // 뮤텍스 해제
    interface->destroy_mutex(mutex);
}

static void test_mutex_try_lock(ETThreadInterface* interface) {
    printf("\n=== 뮤텍스 논블로킹 잠금 테스트 ===\n");

    ETMutex* mutex = NULL;

    // 뮤텍스 생성
    ETResult result = interface->create_mutex(&mutex);
    TEST_ASSERT(result == ET_SUCCESS, "뮤텍스 생성 성공");

    // 논블로킹 잠금 시도 (성공해야 함)
    result = interface->try_lock_mutex(mutex);
    TEST_ASSERT(result == ET_SUCCESS, "첫 번째 논블로킹 잠금 성공");

    // 다시 논블로킹 잠금 시도 (실패해야 함)
    result = interface->try_lock_mutex(mutex);
    TEST_ASSERT(result == ET_ERROR_BUSY, "이미 잠긴 뮤텍스에 대한 논블로킹 잠금 실패");

    // 잠금 해제
    interface->unlock_mutex(mutex);

    // 뮤텍스 해제
    interface->destroy_mutex(mutex);
}

// ============================================================================
// 세마포어 테스트
// ============================================================================

static void test_semaphore_basic(ETThreadInterface* interface) {
    printf("\n=== 세마포어 기본 기능 테스트 ===\n");

    ETSemaphore* semaphore = NULL;

    // 세마포어 생성 (초기 카운트 2)
    ETResult result = interface->create_semaphore(&semaphore, 2);
    TEST_ASSERT(result == ET_SUCCESS, "세마포어 생성 성공");
    TEST_ASSERT(semaphore != NULL, "세마포어 핸들이 유효함");

    // 세마포어 대기 (성공해야 함)
    result = interface->wait_semaphore(semaphore);
    TEST_ASSERT(result == ET_SUCCESS, "첫 번째 세마포어 대기 성공");

    // 세마포어 대기 (성공해야 함)
    result = interface->wait_semaphore(semaphore);
    TEST_ASSERT(result == ET_SUCCESS, "두 번째 세마포어 대기 성공");

    // 논블로킹 대기 (실패해야 함)
    result = interface->try_wait_semaphore(semaphore);
    TEST_ASSERT(result == ET_ERROR_BUSY, "카운트가 0인 세마포어에 대한 논블로킹 대기 실패");

    // 세마포어 신호
    result = interface->post_semaphore(semaphore);
    TEST_ASSERT(result == ET_SUCCESS, "세마포어 신호 성공");

    // 논블로킹 대기 (성공해야 함)
    result = interface->try_wait_semaphore(semaphore);
    TEST_ASSERT(result == ET_SUCCESS, "신호 후 논블로킹 대기 성공");

    // 세마포어 해제
    interface->destroy_semaphore(semaphore);
}

// ============================================================================
// 조건변수 테스트
// ============================================================================

static void test_condition_basic(ETThreadInterface* interface) {
    printf("\n=== 조건변수 기본 기능 테스트 ===\n");

    ETCondition* condition = NULL;
    ETMutex* mutex = NULL;

    // 조건변수와 뮤텍스 생성
    ETResult result = interface->create_condition(&condition);
    TEST_ASSERT(result == ET_SUCCESS, "조건변수 생성 성공");

    result = interface->create_mutex(&mutex);
    TEST_ASSERT(result == ET_SUCCESS, "뮤텍스 생성 성공");

    // 조건변수 신호 (대기 중인 스레드가 없으므로 무시됨)
    result = interface->signal_condition(condition);
    TEST_ASSERT(result == ET_SUCCESS, "조건변수 신호 성공");

    // 조건변수 브로드캐스트
    result = interface->broadcast_condition(condition);
    TEST_ASSERT(result == ET_SUCCESS, "조건변수 브로드캐스트 성공");

    // 정리
    interface->destroy_condition(condition);
    interface->destroy_mutex(mutex);
}

// ============================================================================
// 유틸리티 함수 테스트
// ============================================================================

static void test_utility_functions(ETThreadInterface* interface) {
    printf("\n=== 유틸리티 함수 테스트 ===\n");

    // 현재 스레드 ID 가져오기
    ETThreadID thread_id;
    ETResult result = interface->get_current_thread_id(&thread_id);
    TEST_ASSERT(result == ET_SUCCESS, "현재 스레드 ID 가져오기 성공");
    TEST_ASSERT(thread_id != 0, "스레드 ID가 유효함");

    // 스레드 양보
    result = interface->yield();
    TEST_ASSERT(result == ET_SUCCESS, "스레드 양보 성공");

    // 짧은 대기
    result = interface->sleep(10);  // 10ms
    TEST_ASSERT(result == ET_SUCCESS, "스레드 대기 성공");
}

// ============================================================================
// 속성 초기화 함수 테스트
// ============================================================================

static void test_attribute_initialization(void) {
    printf("\n=== 속성 초기화 함수 테스트 ===\n");

    // 스레드 속성 초기화
    ETThreadAttributes thread_attr;
    et_thread_attributes_init(&thread_attr);
    TEST_ASSERT(thread_attr.priority == ET_THREAD_PRIORITY_NORMAL, "스레드 속성 기본 우선순위");
    TEST_ASSERT(thread_attr.stack_size == 0, "스레드 속성 기본 스택 크기");
    TEST_ASSERT(thread_attr.cpu_affinity == -1, "스레드 속성 기본 CPU 친화성");
    TEST_ASSERT(thread_attr.detached == false, "스레드 속성 기본 분리 상태");

    // 뮤텍스 속성 초기화
    ETMutexAttributes mutex_attr;
    et_mutex_attributes_init(&mutex_attr);
    TEST_ASSERT(mutex_attr.type == ET_MUTEX_NORMAL, "뮤텍스 속성 기본 타입");
    TEST_ASSERT(mutex_attr.shared == false, "뮤텍스 속성 기본 공유 상태");

    // 세마포어 속성 초기화
    ETSemaphoreAttributes sem_attr;
    et_semaphore_attributes_init(&sem_attr);
    TEST_ASSERT(sem_attr.max_count == INT32_MAX, "세마포어 속성 기본 최대 카운트");
    TEST_ASSERT(sem_attr.shared == false, "세마포어 속성 기본 공유 상태");

    // 조건변수 속성 초기화
    ETConditionAttributes cond_attr;
    et_condition_attributes_init(&cond_attr);
    TEST_ASSERT(cond_attr.shared == false, "조건변수 속성 기본 공유 상태");
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main(void) {
    printf("스레딩 추상화 레이어 단위 테스트 시작\n");
    printf("=====================================\n");

    // 플랫폼 팩토리 초기화
    ETResult result = et_platform_factory_init();
    if (result != ET_SUCCESS) {
        printf("플랫폼 팩토리 초기화 실패: %d\n", result);
        return 1;
    }

    // 스레딩 인터페이스 생성
    ETThreadInterface* interface = NULL;
    result = et_create_thread_interface(&interface);
    if (result != ET_SUCCESS) {
        printf("스레딩 인터페이스 생성 실패: %d\n", result);
        et_platform_factory_cleanup();
        return 1;
    }

    if (!interface) {
        printf("스레딩 인터페이스가 NULL입니다\n");
        et_platform_factory_cleanup();
        return 1;
    }

    // 속성 초기화 함수 테스트
    test_attribute_initialization();

    // 스레드 테스트
    test_thread_creation_and_join(interface);
    test_thread_attributes(interface);
    test_thread_detach(interface);

    // 뮤텍스 테스트
    test_mutex_basic(interface);
    test_mutex_try_lock(interface);

    // 세마포어 테스트
    test_semaphore_basic(interface);

    // 조건변수 테스트
    test_condition_basic(interface);

    // 유틸리티 함수 테스트
    test_utility_functions(interface);

    // 정리
    et_destroy_thread_interface(interface);
    et_platform_factory_cleanup();

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 완료: %d/%d 통과\n", g_test_passed, g_test_count);

    if (g_test_passed == g_test_count) {
        printf("모든 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("%d개의 테스트가 실패했습니다.\n", g_test_count - g_test_passed);
        return 1;
    }
}