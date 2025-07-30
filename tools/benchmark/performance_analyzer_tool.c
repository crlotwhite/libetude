#include "libetude/performance_analyzer.h"
#include "libetude/benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// 테스트용 함수들
void cpu_intensive_task(void* user_data) {
    volatile double result = 0.0;
    for (int i = 0; i < 1000000; i++) {
        result += i * 3.14159;
    }
}

void memory_intensive_task(void* user_data) {
    size_t size = 10 * 1024 * 1024; // 10MB
    char* buffer = malloc(size);
    if (buffer) {
        // 메모리 접근 패턴 테스트
        for (size_t i = 0; i < size; i += 64) { // 캐시 라인 크기만큼 점프
            buffer[i] = (char)(i & 0xFF);
        }
        free(buffer);
    }
}

void cache_unfriendly_task(void* user_data) {
    size_t size = 1024 * 1024; // 1MB
    int* array = malloc(size * sizeof(int));
    if (array) {
        // 랜덤 접근 패턴 (캐시 미스 유발)
        for (int i = 0; i < 10000; i++) {
            int index = (i * 7919) % size; // 의사 랜덤 인덱스
            array[index] = i;
        }
        free(array);
    }
}

void print_usage(const char* program_name) {
    printf("사용법: %s [옵션]\n", program_name);
    printf("옵션:\n");
    printf("  -h, --help              이 도움말 출력\n");
    printf("  -o, --output FILE       리포트를 파일로 저장\n");
    printf("  -f, --format FORMAT     출력 형식 (text, json, html)\n");
    printf("  -t, --task TASK         분석할 작업 (cpu, memory, cache, all)\n");
    printf("  -v, --verbose           상세 출력\n");
    printf("  --hotspots              핫스팟 분석 활성화\n");
    printf("  --cache-analysis        캐시 분석 활성화\n");
    printf("  --suggestions           최적화 제안 출력\n");
}

void print_hotspots(const ETHotspot* hotspots, int count) {
    printf("\n핫스팟 분석 결과:\n");
    printf("=====================================\n");
    printf("%-25s %10s %10s %10s %8s\n",
           "함수명", "총시간(us)", "호출횟수", "평균(us)", "비율(%)");
    printf("-------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%-25s %10lu %10lu %10.1f %8.1f\n",
               hotspots[i].function_name,
               hotspots[i].total_time_us,
               hotspots[i].call_count,
               hotspots[i].avg_time_us,
               hotspots[i].percentage);
    }
    printf("\n");
}

void print_cache_analysis(const ETCacheAnalysis* analysis) {
    printf("캐시 분석 결과:\n");
    printf("=====================================\n");
    printf("L1 캐시:\n");
    printf("  참조: %lu, 미스: %lu, 미스율: %.2f%%\n",
           analysis->l1_cache_references, analysis->l1_cache_misses,
           analysis->l1_miss_rate * 100.0);
    printf("L2 캐시:\n");
    printf("  참조: %lu, 미스: %lu, 미스율: %.2f%%\n",
           analysis->l2_cache_references, analysis->l2_cache_misses,
           analysis->l2_miss_rate * 100.0);
    printf("L3 캐시:\n");
    printf("  참조: %lu, 미스: %lu, 미스율: %.2f%%\n",
           analysis->l3_cache_references, analysis->l3_cache_misses,
           analysis->l3_miss_rate * 100.0);
    printf("메모리 대역폭:\n");
    printf("  사용: %lu MB/s, 가용: %lu MB/s (사용률: %.1f%%)\n",
           analysis->memory_bandwidth_used, analysis->memory_bandwidth_available,
           (double)analysis->memory_bandwidth_used / analysis->memory_bandwidth_available * 100.0);
    printf("\n");
}

void print_optimization_suggestions(ETOptimizationSuggestion* suggestions, int count) {
    printf("최적화 제안:\n");
    printf("=====================================\n");

    for (int i = 0; i < count; i++) {
        printf("%d. %s 최적화\n", i + 1, suggestions[i].optimization_type);
        printf("   설명: %s\n", suggestions[i].description);
        printf("   위치: %s\n", suggestions[i].code_location);
        printf("   예상 개선: %.1fx\n", suggestions[i].expected_improvement);
        printf("   난이도: %d/5\n", suggestions[i].implementation_difficulty);
        printf("   힌트: %s\n", suggestions[i].implementation_hint);
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    // 기본 설정
    const char* output_file = NULL;
    const char* output_format = "text";
    const char* task_type = "all";
    bool verbose = false;
    bool show_hotspots = false;
    bool show_cache_analysis = false;
    bool show_suggestions = false;

    // 명령행 옵션 파싱
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"output", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'f'},
        {"task", required_argument, 0, 't'},
        {"verbose", no_argument, 0, 'v'},
        {"hotspots", no_argument, 0, 0},
        {"cache-analysis", no_argument, 0, 0},
        {"suggestions", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "ho:f:t:v", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'o':
                output_file = optarg;
                break;
            case 'f':
                output_format = optarg;
                break;
            case 't':
                task_type = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 0:
                if (strcmp(long_options[option_index].name, "hotspots") == 0) {
                    show_hotspots = true;
                } else if (strcmp(long_options[option_index].name, "cache-analysis") == 0) {
                    show_cache_analysis = true;
                } else if (strcmp(long_options[option_index].name, "suggestions") == 0) {
                    show_suggestions = true;
                }
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    printf("LibEtude 성능 분석 도구\n");
    printf("=====================================\n\n");

    // 성능 분석기 생성
    ETPerformanceAnalyzer* analyzer = et_create_performance_analyzer();
    if (!analyzer) {
        fprintf(stderr, "성능 분석기 생성 실패\n");
        return 1;
    }

    // 벤치마크 프레임워크 초기화
    if (et_benchmark_init() != ET_SUCCESS) {
        fprintf(stderr, "벤치마크 프레임워크 초기화 실패\n");
        et_destroy_performance_analyzer(analyzer);
        return 1;
    }

    // 프로파일링 시작
    printf("프로파일링 시작...\n");
    et_start_profiling(analyzer);

    // 작업 실행
    ETBenchmarkConfig config = ET_BENCHMARK_CONFIG_DEFAULT;

    if (strcmp(task_type, "cpu") == 0 || strcmp(task_type, "all") == 0) {
        printf("CPU 집약적 작업 실행 중...\n");
        ETBenchmarkResult result;
        et_run_benchmark("CPU 집약적 작업", cpu_intensive_task, NULL, &config, &result);
        if (verbose) {
            printf("  실행 시간: %.2f ms\n", result.execution_time_ms);
        }
    }

    if (strcmp(task_type, "memory") == 0 || strcmp(task_type, "all") == 0) {
        printf("메모리 집약적 작업 실행 중...\n");
        ETBenchmarkResult result;
        et_run_benchmark("메모리 집약적 작업", memory_intensive_task, NULL, &config, &result);
        if (verbose) {
            printf("  실행 시간: %.2f ms, 메모리 사용량: %.1f MB\n",
                   result.execution_time_ms, result.memory_usage_mb);
        }
    }

    if (strcmp(task_type, "cache") == 0 || strcmp(task_type, "all") == 0) {
        printf("캐시 비친화적 작업 실행 중...\n");
        ETBenchmarkResult result;
        et_run_benchmark("캐시 비친화적 작업", cache_unfriendly_task, NULL, &config, &result);
        if (verbose) {
            printf("  실행 시간: %.2f ms\n", result.execution_time_ms);
        }
    }

    // 프로파일링 중지
    et_stop_profiling(analyzer);
    printf("프로파일링 완료\n\n");

    // 분석 수행
    printf("성능 분석 중...\n");

    // 핫스팟 분석
    if (show_hotspots || strcmp(task_type, "all") == 0) {
        et_detect_hotspots(analyzer);
        int hotspot_count;
        const ETHotspot* hotspots = et_get_hotspots(analyzer, &hotspot_count);
        if (hotspots && hotspot_count > 0) {
            print_hotspots(hotspots, hotspot_count);
        }
    }

    // 캐시 분석
    if (show_cache_analysis || strcmp(task_type, "all") == 0) {
        ETCacheAnalysis cache_analysis;
        if (et_analyze_cache_performance(analyzer, &cache_analysis) == ET_SUCCESS) {
            print_cache_analysis(&cache_analysis);
        }
    }

    // 병목 분석
    ETBottleneckAnalysis bottleneck_analysis;
    if (et_analyze_bottlenecks(analyzer, &bottleneck_analysis) == ET_SUCCESS) {
        printf("병목 분석 결과:\n");
        printf("=====================================\n");
        printf("병목 유형: %s\n", bottleneck_analysis.bottleneck_type);
        printf("설명: %s\n", bottleneck_analysis.description);
        printf("심각도: %.2f\n", bottleneck_analysis.severity_score);
        printf("권장사항: %s\n\n", bottleneck_analysis.recommendation);
    }

    // 최적화 제안
    if (show_suggestions || strcmp(task_type, "all") == 0) {
        ETOptimizationSuggestion* suggestions;
        int suggestion_count;
        if (et_suggest_optimizations(analyzer, &suggestions, &suggestion_count) == ET_SUCCESS) {
            print_optimization_suggestions(suggestions, suggestion_count);
        }
    }

    // 성능 리포트 생성
    ETPerformanceReport report;
    if (et_generate_performance_report(analyzer, &report) == ET_SUCCESS) {
        printf("전체 성능 평가:\n");
        printf("=====================================\n");
        printf("성능 점수: %.1f/100\n", report.overall_performance_score);
        printf("요약: %s\n\n", report.summary);

        // 리포트 저장
        if (output_file) {
            if (et_save_performance_report(&report, output_file, output_format) == ET_SUCCESS) {
                printf("성능 리포트가 %s 파일로 저장되었습니다.\n", output_file);
            } else {
                fprintf(stderr, "리포트 저장 실패: %s\n", output_file);
            }
        }
    }

    // 정리
    et_destroy_performance_analyzer(analyzer);
    et_benchmark_cleanup();

    printf("성능 분석 완료\n");
    return 0;
}