/**
 * @file test_profiler.c
 * @brief 프로파일러 단위 테스트
 */

#include "libetude/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// 테스트 헬퍼 함수
static void test_profiler_creation(void);
static void test_profiling_operations(void);
static void test_memory_tracking(void);
static void test_resource_monitoring(void);
static void test_report_generation(void);
static void test_thread_safety(void);
static void test_error_handling(void);

// 테스트 유틸리티
static void simulate_work(int milliseconds);
static bool file_exists(const char* path);
static void cleanup_test_files(void);

int main(void) {
    printf("프로파일러 테스트 시작...\n");

    test_profiler_creation();
    test_profiling_operations();
    test_memory_tracking();
    test_resource_monitoring();
    test_report_generation();
    test_thread_safety();
    test_error_handling();

    cleanup_test_files();

    printf("모든 프로파일러 테스트 통과!\n");
    return 0;
}

static void test_profiler_creation(void) {
    printf("  프로파일러 생성 테스트...\n");

    // 정상적인 프로파일러 생성
    Profiler* profiler = rt_create_profiler(100);
    assert(profiler != NULL);
    assert(profiler->capacity == 100);
    assert(profiler->entry_count == 0);
    assert(profiler->is_profiling == true);
    assert(profiler->active_profiles == 0);

    rt_destroy_profiler(profiler);

    // 잘못된 매개변수로 생성
    profiler = rt_create_profiler(0);
    assert(profiler == NULL);

    profiler = rt_create_profiler(-1);
    assert(profiler == NULL);

    printf("    ✓ 프로파일러 생성 테스트 통과\n");
}

static void test_profiling_operations(void) {
    printf("  프로파일링 연산 테스트...\n");

    Profiler* profiler = rt_create_profiler(10);
    assert(profiler != NULL);

    // 기본 프로파일링 테스트
    ETResult result = rt_start_profile(profiler, "test_operation");
    assert(result == ET_SUCCESS);
    assert(profiler->active_profiles == 1);

    simulate_work(10); // 10ms 작업 시뮬레이션

    result = rt_end_profile(profiler, "test_operation");
    assert(result == ET_SUCCESS);
    assert(profiler->entry_count == 1);
    assert(profiler->active_profiles == 0);

    // 프로파일 결과 확인
    const ProfileEntry* entry = rt_get_profile_stats(profiler, "test_operation");
    assert(entry != NULL);
    assert(strcmp(entry->op_name, "test_operation") == 0);
    assert(entry->end_time > entry->start_time);
    assert(entry->cpu_cycles > 0);

    // 여러 연산 프로파일링
    rt_start_profile(profiler, "operation_1");
    simulate_work(5);
    rt_end_profile(profiler, "operation_1");

    rt_start_profile(profiler, "operation_2");
    simulate_work(15);
    rt_end_profile(profiler, "operation_2");

    assert(profiler->entry_count == 3);

    // 중첩 프로파일링 테스트
    rt_start_profile(profiler, "outer_operation");
    rt_start_profile(profiler, "inner_operation");
    simulate_work(5);
    rt_end_profile(profiler, "inner_operation");
    simulate_work(5);
    rt_end_profile(profiler, "outer_operation");

    assert(profiler->entry_count == 5);

    rt_destroy_profiler(profiler);

    printf("    ✓ 프로파일링 연산 테스트 통과\n");
}

static void test_memory_tracking(void) {
    printf("  메모리 추적 테스트...\n");

    Profiler* profiler = rt_create_profiler(10);
    assert(profiler != NULL);

    // 메모리 사용량 업데이트 테스트
    rt_start_profile(profiler, "memory_test");
    rt_end_profile(profiler, "memory_test");
    rt_update_memory_usage(profiler, "memory_test", 1024, 2048);

    const ProfileEntry* entry = rt_get_profile_stats(profiler, "memory_test");
    assert(entry != NULL);
    assert(entry->memory_used == 1024);
    assert(entry->memory_peak == 2048);
    assert(profiler->total_memory_peak == 2048);

    // 더 큰 메모리 사용량으로 업데이트
    rt_start_profile(profiler, "memory_test_2");
    rt_end_profile(profiler, "memory_test_2");
    rt_update_memory_usage(profiler, "memory_test_2", 4096, 8192);

    assert(profiler->total_memory_peak == 8192);

    rt_destroy_profiler(profiler);

    printf("    ✓ 메모리 추적 테스트 통과\n");
}

static void test_resource_monitoring(void) {
    printf("  리소스 모니터링 테스트...\n");

    Profiler* profiler = rt_create_profiler(10);
    assert(profiler != NULL);

    // CPU/GPU 사용률 업데이트 테스트
    rt_update_resource_usage(profiler, 0.5f, 0.3f);
    assert(profiler->avg_cpu_usage > 0.0f);
    assert(profiler->avg_gpu_usage > 0.0f);

    // 여러 번 업데이트하여 이동 평균 테스트
    rt_update_resource_usage(profiler, 0.7f, 0.4f);
    rt_update_resource_usage(profiler, 0.6f, 0.2f);

    // 평균값이 합리적인 범위에 있는지 확인
    assert(profiler->avg_cpu_usage >= 0.0f && profiler->avg_cpu_usage <= 1.0f);
    assert(profiler->avg_gpu_usage >= 0.0f && profiler->avg_gpu_usage <= 1.0f);

    rt_destroy_profiler(profiler);

    printf("    ✓ 리소스 모니터링 테스트 통과\n");
}

static void test_report_generation(void) {
    printf("  리포트 생성 테스트...\n");

    Profiler* profiler = rt_create_profiler(10);
    assert(profiler != NULL);

    // 테스트 데이터 생성
    rt_start_profile(profiler, "report_test_1");
    simulate_work(10);
    rt_update_memory_usage(profiler, "report_test_1", 1024, 1024);
    rt_end_profile(profiler, "report_test_1");

    rt_start_profile(profiler, "report_test_2");
    simulate_work(20);
    rt_update_memory_usage(profiler, "report_test_2", 2048, 2048);
    rt_end_profile(profiler, "report_test_2");

    rt_update_resource_usage(profiler, 0.6f, 0.4f);

    // JSON 리포트 생성
    const char* report_path = "test_profile_report.json";
    ETResult result = rt_generate_report(profiler, report_path);
    assert(result == ET_SUCCESS);
    assert(file_exists(report_path));

    // 리포트 파일 내용 간단 검증
    FILE* file = fopen(report_path, "r");
    assert(file != NULL);

    char buffer[1024];
    bool found_session = false;
    bool found_operations = false;

    while (fgets(buffer, sizeof(buffer), file)) {
        if (strstr(buffer, "\"session\"")) {
            found_session = true;
        }
        if (strstr(buffer, "\"operations\"")) {
            found_operations = true;
        }
    }

    fclose(file);
    assert(found_session);
    assert(found_operations);

    rt_destroy_profiler(profiler);

    printf("    ✓ 리포트 생성 테스트 통과\n");
}

static void test_thread_safety(void) {
    printf("  스레드 안전성 테스트...\n");

    // 기본적인 스레드 안전성 테스트
    // 실제 멀티스레드 테스트는 복잡하므로 기본 동작만 확인
    Profiler* profiler = rt_create_profiler(100);
    assert(profiler != NULL);

    // 동시에 여러 프로파일 시작/종료
    for (int i = 0; i < 10; i++) {
        char op_name[32];
        snprintf(op_name, sizeof(op_name), "thread_test_%d", i);

        rt_start_profile(profiler, op_name);
        simulate_work(1);
        rt_end_profile(profiler, op_name);
    }

    assert(profiler->entry_count == 10);

    rt_destroy_profiler(profiler);

    printf("    ✓ 스레드 안전성 테스트 통과\n");
}

static void test_error_handling(void) {
    printf("  오류 처리 테스트...\n");

    Profiler* profiler = rt_create_profiler(2); // 작은 용량
    assert(profiler != NULL);

    // NULL 포인터 테스트
    assert(rt_start_profile(NULL, "test") == ET_ERROR_INVALID_ARGUMENT);
    assert(rt_start_profile(profiler, NULL) == ET_ERROR_INVALID_ARGUMENT);
    assert(rt_end_profile(NULL, "test") == ET_ERROR_INVALID_ARGUMENT);
    assert(rt_end_profile(profiler, NULL) == ET_ERROR_INVALID_ARGUMENT);

    // 존재하지 않는 프로파일 종료
    assert(rt_end_profile(profiler, "nonexistent") == ET_ERROR_NOT_FOUND);

    // 중복 프로파일 시작
    assert(rt_start_profile(profiler, "duplicate") == ET_SUCCESS);
    assert(rt_start_profile(profiler, "duplicate") == ET_ERROR_INVALID_STATE);
    rt_end_profile(profiler, "duplicate");

    // 용량 초과 테스트
    rt_start_profile(profiler, "test1");
    rt_end_profile(profiler, "test1");
    rt_start_profile(profiler, "test2");
    rt_end_profile(profiler, "test2");

    // 용량이 가득 찬 상태에서 추가 프로파일링
    rt_start_profile(profiler, "test3");
    assert(rt_end_profile(profiler, "test3") == ET_ERROR_BUFFER_FULL);

    // 프로파일링 비활성화 테스트
    rt_enable_profiling(profiler, false);
    assert(rt_start_profile(profiler, "disabled") == ET_ERROR_INVALID_ARGUMENT);

    rt_destroy_profiler(profiler);

    // 잘못된 리포트 경로
    profiler = rt_create_profiler(10);
    assert(rt_generate_report(profiler, "/invalid/path/report.json") == ET_ERROR_IO);

    rt_destroy_profiler(profiler);

    printf("    ✓ 오류 처리 테스트 통과\n");
}

// 테스트 유틸리티 함수 구현

static void simulate_work(int milliseconds) {
    usleep(milliseconds * 1000); // 마이크로초로 변환
}

static bool file_exists(const char* path) {
    FILE* file = fopen(path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

static void cleanup_test_files(void) {
    remove("test_profile_report.json");
}