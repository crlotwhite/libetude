#include "libetude/performance_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// 성능 분석기 생성/해제 테스트
void test_analyzer_creation() {
    printf("성능 분석기 생성/해제 테스트... ");

    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    assert(analyzer != NULL);
    assert(analyzer->profiling_enabled == true);
    assert(analyzer->cache_analysis_enabled == true);
    assert(analyzer->hotspot_detection_enabled == true);
    assert(analyzer->max_hotspots == 10);

    et_destroy_performance_analyzer(analyzer);

    printf("통과\n");
}

// 성능 카운터 읽기 테스트
void test_performance_counters() {
    printf("성능 카운터 읽기 테스트... ");

    ETPerformanceCounters counters;
    int result = et_read_performance_counters(&counters);

    assert(result == ET_SUCCESS);
    assert(counters.timestamp_us > 0);

    printf("통과\n");
}

// 프로파일링 테스트
void test_profiling() {
    printf("프로파일링 테스트... ");

    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    assert(analyzer != NULL);

    // 프로파일링 시작
    int result = et_start_profiling(analyzer);
    assert(result == ET_SUCCESS);
    assert(analyzer->profiling_enabled == true);

    // 간단한 작업 수행
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }

    // 프로파일링 중지
    result = et_stop_profiling(analyzer);
    assert(result == ET_SUCCESS);
    assert(analyzer->profiling_enabled == false);

    et_destroy_performance_analyzer(analyzer);

    printf("통과\n");
}

// 핫스팟 감지 테스트
void test_hotspot_detection() {
    printf("핫스팟 감지 테스트... ");

    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    assert(analyzer != NULL);

    int result = et_detect_hotspots(analyzer);
    assert(result == ET_SUCCESS);

    int hotspot_count;
    const ETHotspot* hotspots = et_get_hotspots(analyzer, &hotspot_count);
    assert(hotspots != NULL);
    assert(hotspot_count > 0);
    assert(hotspot_count <= analyzer->max_hotspots);

    // 첫 번째 핫스팟 검증
    assert(hotspots[0].function_name != NULL);
    assert(hotspots[0].total_time_us > 0);
    assert(hotspots[0].call_count > 0);
    assert(hotspots[0].percentage > 0.0);

    et_destroy_performance_analyzer(analyzer);

    printf("통과\n");
}

// 캐시 정보 테스트
void test_cache_info() {
    printf("캐시 정보 테스트... ");

    ETCacheInfo info;
    int result = et_get_cache_info(&info);

    assert(result == ET_SUCCESS);
    assert(info.cache_line_size > 0);
    assert(info.l1_cache_size > 0);
    assert(info.l2_cache_size > 0);
    assert(info.l3_cache_size > 0);
    assert(info.associativity > 0);

    printf("통과\n");
}

// 캐시 분석 테스트
void test_cache_analysis() {
    printf("캐시 분석 테스트... ");

    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    assert(analyzer != NULL);

    ETCacheAnalysis analysis;
    int result = et_analyze_cache_performance(analyzer, &analysis);

    assert(result == ET_SUCCESS);
    assert(analysis.l1_cache_references > 0);
    assert(analysis.l1_miss_rate >= 0.0 && analysis.l1_miss_rate <= 1.0);
    assert(analysis.l2_miss_rate >= 0.0 && analysis.l2_miss_rate <= 1.0);
    assert(analysis.l3_miss_rate >= 0.0 && analysis.l3_miss_rate <= 1.0);

    et_destroy_performance_analyzer(analyzer);

    printf("통과\n");
}

// 병목 분석 테스트
void test_bottleneck_analysis() {
    printf("병목 분석 테스트... ");

    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    assert(analyzer != NULL);

    ETBottleneckAnalysis analysis;
    int result = et_analyze_bottlenecks(analyzer, &analysis);

    assert(result == ET_SUCCESS);
    assert(analysis.bottleneck_type != NULL);
    assert(analysis.description != NULL);
    assert(analysis.recommendation != NULL);
    assert(analysis.severity_score >= 0.0 && analysis.severity_score <= 1.0);
    assert(analysis.hotspots != NULL);
    assert(analysis.num_hotspots > 0);

    et_destroy_performance_analyzer(analyzer);

    printf("통과\n");
}

// 최적화 제안 테스트
void test_optimization_suggestions() {
    printf("최적화 제안 테스트... ");

    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    assert(analyzer != NULL);

    ETOptimizationSuggestion* suggestions;
    int count;
    int result = et_suggest_optimizations(analyzer, &suggestions, &count);

    assert(result == ET_SUCCESS);
    assert(suggestions != NULL);
    assert(count > 0);

    // 첫 번째 제안 검증
    assert(suggestions[0].optimization_type != NULL);
    assert(suggestions[0].description != NULL);
    assert(suggestions[0].expected_improvement > 1.0);
    assert(suggestions[0].implementation_difficulty >= 1 &&
           suggestions[0].implementation_difficulty <= 5);

    et_destroy_performance_analyzer(analyzer);

    printf("통과\n");
}

// 메모리 접근 패턴 분석 테스트
void test_memory_access_analysis() {
    printf("메모리 접근 패턴 분석 테스트... ");

    // 순차 접근 패턴
    size_t sequential_access[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    char data[1024];
    ETMemoryAccessAnalysis analysis;

    int result = et_analyze_memory_access(data, sizeof(data),
                                          sequential_access, 10, &analysis);

    assert(result == ET_SUCCESS);
    assert(analysis.pattern == ET_ACCESS_PATTERN_SEQUENTIAL);
    assert(analysis.locality_score == 1.0);
    assert(analysis.cache_efficiency > 0.9);

    // 스트라이드 접근 패턴
    size_t strided_access[] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36};
    result = et_analyze_memory_access(data, sizeof(data),
                                      strided_access, 10, &analysis);

    assert(result == ET_SUCCESS);
    assert(analysis.pattern == ET_ACCESS_PATTERN_STRIDED);
    assert(analysis.stride_size == 4);
    assert(analysis.locality_score > 0.0);

    // 랜덤 접근 패턴
    size_t random_access[] = {100, 50, 200, 25, 150, 75, 300, 10, 250, 125};
    result = et_analyze_memory_access(data, sizeof(data),
                                      random_access, 10, &analysis);

    assert(result == ET_SUCCESS);
    assert(analysis.pattern == ET_ACCESS_PATTERN_RANDOM);
    assert(analysis.locality_score < 0.5);
    assert(analysis.cache_efficiency < 0.5);

    printf("통과\n");
}

// 데이터 레이아웃 최적화 테스트
void test_data_layout_optimization() {
    printf("데이터 레이아웃 최적화 테스트... ");

    int data[1000];
    int result = et_optimize_data_layout(data, sizeof(int), 1000, 64);

    assert(result == ET_SUCCESS);

    printf("통과\n");
}

// 성능 리포트 생성 테스트
void test_performance_report() {
    printf("성능 리포트 생성 테스트... ");

    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    assert(analyzer != NULL);

    ETPerformanceReport report;
    int result = et_generate_performance_report(analyzer, &report);

    assert(result == ET_SUCCESS);
    assert(report.title != NULL);
    assert(report.overall_performance_score >= 0.0 &&
           report.overall_performance_score <= 100.0);
    assert(report.summary != NULL);
    assert(report.suggestions != NULL);
    assert(report.suggestion_count > 0);

    // 리포트 저장 테스트
    result = et_save_performance_report(&report, "test_report.txt", "text");
    assert(result == ET_SUCCESS);

    result = et_save_performance_report(&report, "test_report.json", "json");
    assert(result == ET_SUCCESS);

    // 파일 삭제
    remove("test_report.txt");
    remove("test_report.json");

    et_destroy_performance_analyzer(analyzer);

    printf("통과\n");
}

// 성능 비교 테스트
void test_performance_comparison() {
    printf("성능 비교 테스트... ");

    ETPerformanceCounters baseline = {
        .cpu_cycles = 1000000,
        .instructions = 500000,
        .cache_misses = 10000,
        .cache_references = 100000,
        .timestamp_us = 1000000
    };

    ETPerformanceCounters optimized = {
        .cpu_cycles = 500000,    // 2배 빠름
        .instructions = 400000,
        .cache_misses = 5000,    // 캐시 미스 절반
        .cache_references = 80000,
        .timestamp_us = 500000
    };

    ETPerformanceComparison comparison;
    int result = et_compare_performance(&baseline, &optimized, &comparison);

    assert(result == ET_SUCCESS);
    assert(comparison.speedup == 2.0);
    assert(comparison.is_improvement == true);
    assert(comparison.analysis != NULL);

    printf("통과\n");
}

// 오류 처리 테스트
void test_error_handling() {
    printf("오류 처리 테스트... ");

    // NULL 포인터 테스트
    int result = et_read_performance_counters(NULL);
    assert(result == ET_ERROR_INVALID_ARGUMENT);

    result = et_analyze_cache_performance(NULL, NULL);
    assert(result == ET_ERROR_INVALID_ARGUMENT);

    result = et_analyze_bottlenecks(NULL, NULL);
    assert(result == ET_ERROR_INVALID_ARGUMENT);

    ETMemoryAccessAnalysis analysis;
    result = et_analyze_memory_access(NULL, 0, NULL, 0, &analysis);
    assert(result == ET_ERROR_INVALID_ARGUMENT);

    printf("통과\n");
}

int main() {
    printf("성능 분석기 테스트 시작\n");
    printf("================================\n");

    test_analyzer_creation();
    test_performance_counters();
    test_profiling();
    test_hotspot_detection();
    test_cache_info();
    test_cache_analysis();
    test_bottleneck_analysis();
    test_optimization_suggestions();
    test_memory_access_analysis();
    test_data_layout_optimization();
    test_performance_report();
    test_performance_comparison();
    test_error_handling();

    printf("================================\n");
    printf("모든 성능 분석기 테스트 통과!\n");

    return 0;
}