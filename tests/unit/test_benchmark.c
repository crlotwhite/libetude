#include "libetude/benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// 테스트용 간단한 벤치마크 함수
void simple_benchmark_func(void* user_data) {
    int* counter = (int*)user_data;
    if (counter) {
        (*counter)++;
    }

    // 간단한 연산 수행
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
}

void fast_benchmark_func(void* user_data) {
    // 매우 빠른 연산
    volatile int x = 42;
    x = x * 2;
}

void slow_benchmark_func(void* user_data) {
    // 상대적으로 느린 연산
    volatile int sum = 0;
    for (int i = 0; i < 100000; i++) {
        sum += i * i;
    }
}

// 벤치마크 프레임워크 초기화/해제 테스트
void test_benchmark_init_cleanup() {
    printf("벤치마크 초기화/해제 테스트... ");

    // 초기화
    int result = et_benchmark_init();
    assert(result == ET_SUCCESS);

    // 중복 초기화 (성공해야 함)
    result = et_benchmark_init();
    assert(result == ET_SUCCESS);

    // 해제
    et_benchmark_cleanup();

    printf("통과\n");
}

// 단일 벤치마크 실행 테스트
void test_single_benchmark() {
    printf("단일 벤치마크 실행 테스트... ");

    et_benchmark_init();

    int counter = 0;
    ETBenchmarkResult result;
    ETBenchmarkConfig config = ET_BENCHMARK_CONFIG_QUICK;

    int ret = et_run_benchmark("테스트 벤치마크",
                               simple_benchmark_func,
                               &counter,
                               &config,
                               &result);

    assert(ret == ET_SUCCESS);
    assert(result.success);
    assert(strcmp(result.name, "테스트 벤치마크") == 0);
    assert(result.execution_time_ms > 0);
    assert(counter == config.measurement_iterations);

    et_benchmark_cleanup();

    printf("통과\n");
}

// 벤치마크 스위트 테스트
void test_benchmark_suite() {
    printf("벤치마크 스위트 테스트... ");

    et_benchmark_init();

    ETBenchmarkConfig config = ET_BENCHMARK_CONFIG_QUICK;
    ETBenchmarkSuite* suite = et_create_benchmark_suite("테스트 스위트", &config);
    assert(suite != NULL);

    // 벤치마크 추가
    int ret1 = et_add_benchmark(suite, "빠른 테스트", fast_benchmark_func, NULL);
    int ret2 = et_add_benchmark(suite, "느린 테스트", slow_benchmark_func, NULL);

    assert(ret1 == ET_SUCCESS);
    assert(ret2 == ET_SUCCESS);
    assert(suite->num_benchmarks == 2);

    // 스위트 실행
    int ret = et_run_benchmark_suite(suite);
    assert(ret == ET_SUCCESS);

    // 결과 확인
    assert(suite->results != NULL);
    assert(suite->results[0].success);
    assert(suite->results[1].success);

    // 빠른 테스트가 느린 테스트보다 빨라야 함
    assert(suite->results[0].execution_time_ms < suite->results[1].execution_time_ms);

    et_destroy_benchmark_suite(suite);
    et_benchmark_cleanup();

    printf("통과\n");
}

// 비교 분석 테스트
void test_benchmark_comparison() {
    printf("벤치마크 비교 분석 테스트... ");

    ETBenchmarkResult baseline = {
        .name = "기준",
        .execution_time_ms = 100.0,
        .memory_usage_mb = 10.0,
        .energy_consumption_mj = 5.0,
        .success = true
    };

    ETBenchmarkResult comparison = {
        .name = "비교",
        .execution_time_ms = 50.0,  // 2배 빠름
        .memory_usage_mb = 8.0,     // 메모리 20% 절약
        .energy_consumption_mj = 4.0, // 에너지 20% 절약
        .success = true
    };

    ETBenchmarkComparison comp_result;
    int ret = et_compare_benchmarks(&baseline, &comparison, &comp_result);

    assert(ret == ET_SUCCESS);
    assert(comp_result.speedup_ratio == 2.0);
    assert(comp_result.memory_ratio == 0.8);
    assert(comp_result.energy_ratio == 0.8);
    assert(comp_result.is_improvement == true);

    printf("통과\n");
}

// 통계 함수 테스트
void test_statistics() {
    printf("통계 함수 테스트... ");

    double values[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    int count = 5;

    double mean = et_calculate_mean(values, count);
    assert(mean == 3.0);

    double stddev = et_calculate_stddev(values, count);
    assert(stddev > 1.5 && stddev < 1.6); // 약 1.58

    double p50 = et_calculate_percentile(values, count, 50.0);
    assert(p50 == 3.0);

    printf("통과\n");
}

// 시스템 정보 테스트
void test_system_info() {
    printf("시스템 정보 테스트... ");

    ETSystemInfo info;
    int ret = et_get_system_info(&info);

    assert(ret == ET_SUCCESS);
    assert(info.cpu_cores > 0);
    assert(info.cpu_threads > 0);
    assert(info.memory_total_mb > 0);
    assert(strlen(info.os_name) > 0);

    printf("통과\n");
}

// 결과 저장 테스트
void test_save_results() {
    printf("결과 저장 테스트... ");

    ETBenchmarkResult results[] = {
        {
            .name = "테스트1",
            .execution_time_ms = 10.5,
            .memory_usage_mb = 5.2,
            .cpu_usage_percent = 75.0,
            .operations_per_second = 1000,
            .success = true
        },
        {
            .name = "테스트2",
            .execution_time_ms = 20.3,
            .memory_usage_mb = 8.1,
            .cpu_usage_percent = 60.0,
            .operations_per_second = 500,
            .success = true
        }
    };

    // JSON 형식으로 저장
    int ret = et_save_benchmark_results(results, 2, "test_results.json", "json");
    assert(ret == ET_SUCCESS);

    // CSV 형식으로 저장
    ret = et_save_benchmark_results(results, 2, "test_results.csv", "csv");
    assert(ret == ET_SUCCESS);

    // 파일 삭제
    remove("test_results.json");
    remove("test_results.csv");

    printf("통과\n");
}

// 오류 처리 테스트
void test_error_handling() {
    printf("오류 처리 테스트... ");

    // NULL 포인터 테스트
    ETBenchmarkResult result;
    int ret = et_run_benchmark(NULL, simple_benchmark_func, NULL, NULL, &result);
    assert(ret == ET_ERROR_INVALID_ARGUMENT);

    ret = et_run_benchmark("테스트", NULL, NULL, NULL, &result);
    assert(ret == ET_ERROR_INVALID_ARGUMENT);

    ret = et_run_benchmark("테스트", simple_benchmark_func, NULL, NULL, NULL);
    assert(ret == ET_ERROR_INVALID_ARGUMENT);

    // 초기화되지 않은 상태에서 실행
    et_benchmark_cleanup();
    ret = et_run_benchmark("테스트", simple_benchmark_func, NULL, NULL, &result);
    assert(ret == ET_ERROR_RUNTIME);

    printf("통과\n");
}

int main() {
    printf("벤치마크 프레임워크 테스트 시작\n");
    printf("================================\n");

    test_benchmark_init_cleanup();
    test_single_benchmark();
    test_benchmark_suite();
    test_benchmark_comparison();
    test_statistics();
    test_system_info();
    test_save_results();
    test_error_handling();

    printf("================================\n");
    printf("모든 벤치마크 테스트 통과!\n");

    return 0;
}