#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include "libetude/task_scheduler.h"

// 테스트 결과 추적
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            printf("✗ %s\n", message); \
        } \
    } while(0)

// 테스트용 작업 함수들
static volatile int test_counter = 0;
static volatile int completion_callback_count = 0;

static void simple_task(void* data) {
    int* value = (int*)data;
    (*value)++;
    test_counter++;
}

static void sleep_task(void* data) {
    int sleep_ms = *(int*)data;
    usleep(sleep_ms * 1000);
    test_counter++;
}

static void completion_callback(uint32_t task_id, void* user_data) {
    (void)task_id;    // 사용하지 않는 매개변수 경고 제거
    (void)user_data;  // 사용하지 않는 매개변수 경고 제거
    completion_callback_count++;
}

// 기본 스케줄러 생성/해제 테스트
static void test_scheduler_creation() {
    printf("\n=== 스케줄러 생성/해제 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(4);
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (scheduler) {
        et_destroy_task_scheduler(scheduler);
        printf("✓ 스케줄러 해제 성공\n");
    }
}

// 기본 작업 제출 및 실행 테스트
static void test_basic_task_submission() {
    printf("\n=== 기본 작업 제출 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(2);
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (!scheduler) return;

    test_counter = 0;
    int task_data = 0;

    uint32_t task_id = et_submit_task(scheduler, ET_TASK_PRIORITY_NORMAL,
                                     simple_task, &task_data, 0);
    TEST_ASSERT(task_id != 0, "작업 제출 성공");

    // 작업 완료 대기 (더 긴 시간)
    for (int i = 0; i < 50; i++) {
        usleep(10000); // 10ms씩 대기
        if (test_counter == 1) break;
    }

    TEST_ASSERT(test_counter == 1, "작업 실행 완료");
    TEST_ASSERT(task_data == 1, "작업 데이터 처리 완료");

    et_destroy_task_scheduler(scheduler);
}

// 우선순위 테스트
static void test_priority_scheduling() {
    printf("\n=== 우선순위 스케줄링 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(1); // 단일 워커로 순서 보장
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (!scheduler) return;

    test_counter = 0;
    int low_data = 10, normal_data = 20, high_data = 30;

    // 낮은 우선순위 작업 먼저 제출
    et_submit_task(scheduler, ET_TASK_PRIORITY_LOW, simple_task, &low_data, 0);
    et_submit_task(scheduler, ET_TASK_PRIORITY_HIGH, simple_task, &high_data, 0);
    et_submit_task(scheduler, ET_TASK_PRIORITY_NORMAL, simple_task, &normal_data, 0);

    // 작업 완료 대기
    usleep(200000); // 200ms 대기

    TEST_ASSERT(test_counter == 3, "모든 작업 실행 완료");

    // 우선순위 순서대로 실행되었는지는 단일 워커에서만 보장됨
    // 실제로는 더 정교한 테스트가 필요

    et_destroy_task_scheduler(scheduler);
}

// 콜백 테스트
static void test_completion_callback() {
    printf("\n=== 완료 콜백 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(2);
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (!scheduler) return;

    completion_callback_count = 0;
    int task_data = 0;

    uint32_t task_id = et_submit_task_with_callback(scheduler, ET_TASK_PRIORITY_NORMAL,
                                                   simple_task, &task_data, 0,
                                                   completion_callback, NULL);
    TEST_ASSERT(task_id != 0, "콜백과 함께 작업 제출 성공");

    // 작업 완료 대기
    usleep(100000); // 100ms 대기

    TEST_ASSERT(completion_callback_count == 1, "완료 콜백 호출됨");

    et_destroy_task_scheduler(scheduler);
}

// 실시간 모드 테스트
static void test_realtime_mode() {
    printf("\n=== 실시간 모드 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(2);
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (!scheduler) return;

    // 실시간 모드 활성화
    et_set_realtime_mode(scheduler, true);
    et_set_audio_buffer_deadline(scheduler, 5000); // 5ms

    test_counter = 0;
    int task_data = 0;

    // 현재 시간 + 10ms 데드라인으로 실시간 작업 제출
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t deadline = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec + 10000;

    uint32_t task_id = et_submit_task(scheduler, ET_TASK_PRIORITY_REALTIME,
                                     simple_task, &task_data, deadline);
    TEST_ASSERT(task_id != 0, "실시간 작업 제출 성공");

    // 작업 완료 대기
    usleep(50000); // 50ms 대기

    TEST_ASSERT(test_counter == 1, "실시간 작업 실행 완료");

    et_destroy_task_scheduler(scheduler);
}

// 스케줄러 통계 테스트
static void test_scheduler_stats() {
    printf("\n=== 스케줄러 통계 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(2);
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (!scheduler) return;

    ETSchedulerStats stats;
    et_get_scheduler_stats(scheduler, &stats);

    TEST_ASSERT(stats.total_submitted == 0, "초기 제출 작업 수 0");
    TEST_ASSERT(stats.total_completed == 0, "초기 완료 작업 수 0");

    // 몇 개 작업 제출
    int task_data1 = 1, task_data2 = 2;
    et_submit_task(scheduler, ET_TASK_PRIORITY_NORMAL, simple_task, &task_data1, 0);
    et_submit_task(scheduler, ET_TASK_PRIORITY_HIGH, simple_task, &task_data2, 0);

    // 작업 완료 대기
    usleep(100000); // 100ms 대기

    et_get_scheduler_stats(scheduler, &stats);
    TEST_ASSERT(stats.total_submitted == 2, "제출된 작업 수 2");
    TEST_ASSERT(stats.total_completed == 2, "완료된 작업 수 2");

    et_destroy_task_scheduler(scheduler);
}

// 다중 워커 테스트
static void test_multiple_workers() {
    printf("\n=== 다중 워커 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(4);
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (!scheduler) return;

    test_counter = 0;
    const int num_tasks = 10;
    int task_data[num_tasks];

    // 여러 작업 동시 제출
    for (int i = 0; i < num_tasks; i++) {
        task_data[i] = i;
        et_submit_task(scheduler, ET_TASK_PRIORITY_NORMAL, simple_task, &task_data[i], 0);
    }

    // 모든 작업 완료 대기
    usleep(200000); // 200ms 대기

    TEST_ASSERT(test_counter == num_tasks, "모든 작업 병렬 실행 완료");

    et_destroy_task_scheduler(scheduler);
}

// 스케줄러 일시정지/재개 테스트
static void test_pause_resume() {
    printf("\n=== 스케줄러 일시정지/재개 테스트 ===\n");

    ETTaskScheduler* scheduler = et_create_task_scheduler(2);
    TEST_ASSERT(scheduler != NULL, "스케줄러 생성 성공");

    if (!scheduler) return;

    test_counter = 0;

    // 스케줄러 일시정지
    et_pause_scheduler(scheduler);

    int task_data = 0;
    et_submit_task(scheduler, ET_TASK_PRIORITY_NORMAL, simple_task, &task_data, 0);

    // 일시정지 상태에서는 작업이 실행되지 않아야 함
    usleep(100000); // 100ms 대기
    TEST_ASSERT(test_counter == 0, "일시정지 상태에서 작업 실행 안됨");

    // 스케줄러 재개
    et_resume_scheduler(scheduler);

    // 재개 후 작업 실행 확인
    usleep(100000); // 100ms 대기
    TEST_ASSERT(test_counter == 1, "재개 후 작업 실행됨");

    et_destroy_task_scheduler(scheduler);
}

int main() {
    printf("LibEtude 작업 스케줄러 테스트 시작\n");
    printf("=====================================\n");

    test_scheduler_creation();
    test_basic_task_submission();
    test_priority_scheduling();
    test_completion_callback();
    test_realtime_mode();
    test_scheduler_stats();
    test_multiple_workers();
    test_pause_resume();

    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("✓ 모든 테스트 통과!\n");
        return 0;
    } else {
        printf("✗ %d개 테스트 실패\n", tests_run - tests_passed);
        return 1;
    }
}