#include "libetude/performance_analyzer.h"
#include "libetude/error.h"
#include "libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <unistd.h>
#include <sys/ioctl.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

// 고해상도 타이머
static uint64_t get_timestamp_us() {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart * 1000000.0 / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
#endif
}

// 성능 분석기 생성
ETPerformanceAnalyzer* et_create_performance_analyzer() {
    ETPerformanceAnalyzer* analyzer = malloc(sizeof(ETPerformanceAnalyzer));
    if (!analyzer) {
        return NULL;
    }

    memset(analyzer, 0, sizeof(ETPerformanceAnalyzer));

    // 기본 설정
    analyzer->profiling_enabled = true;
    analyzer->cache_analysis_enabled = true;
    analyzer->hotspot_detection_enabled = true;
    analyzer->max_hotspots = 10;
    analyzer->hotspot_threshold_percent = 5.0;

    // 핫스팟 배열 할당
    analyzer->hotspots = malloc(analyzer->max_hotspots * sizeof(ETHotspot));
    if (!analyzer->hotspots) {
        free(analyzer);
        return NULL;
    }

    return analyzer;
}

// 성능 분석기 해제
void et_destroy_performance_analyzer(ETPerformanceAnalyzer* analyzer) {
    if (!analyzer) {
        return;
    }

    free(analyzer->hotspots);
    free(analyzer->profiling_data);
    free(analyzer);
}

// 성능 카운터 읽기 (플랫폼별 구현)
int et_read_performance_counters(ETPerformanceCounters* counters) {
    if (!counters) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(counters, 0, sizeof(ETPerformanceCounters));
    counters->timestamp_us = get_timestamp_us();

#ifdef __linux__
    // Linux perf_event 시스템 콜 사용
    // 실제 구현에서는 perf_event_open을 사용하여 하드웨어 카운터 읽기
    // 여기서는 간단한 예시만 제공

    FILE* stat = fopen("/proc/self/stat", "r");
    if (stat) {
        // CPU 시간 정보 읽기 (간단한 구현)
        unsigned long utime, stime;
        if (fscanf(stat, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                   &utime, &stime) == 2) {
            counters->cpu_cycles = (utime + stime) * 100; // 대략적인 값
        }
        fclose(stat);
    }

#elif defined(_WIN32)
    // Windows 성능 카운터 사용
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
                        &kernel_time, &user_time)) {
        ULARGE_INTEGER kt, ut;
        kt.LowPart = kernel_time.dwLowDateTime;
        kt.HighPart = kernel_time.dwHighDateTime;
        ut.LowPart = user_time.dwLowDateTime;
        ut.HighPart = user_time.dwHighDateTime;

        counters->cpu_cycles = (kt.QuadPart + ut.QuadPart) / 10; // 100ns 단위를 us로 변환
    }

#elif defined(__APPLE__)
    // macOS Mach 시스템 콜 사용
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
        counters->cpu_cycles = info.user_time.seconds * 1000000 + info.user_time.microseconds;
        counters->cpu_cycles += info.system_time.seconds * 1000000 + info.system_time.microseconds;
    }
#endif

    return ET_SUCCESS;
}

// 프로파일링 시작
int et_start_profiling(ETPerformanceAnalyzer* analyzer) {
    if (!analyzer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    analyzer->profiling_enabled = true;
    return et_read_performance_counters(&analyzer->start_counters);
}

// 프로파일링 중지
int et_stop_profiling(ETPerformanceAnalyzer* analyzer) {
    if (!analyzer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int result = et_read_performance_counters(&analyzer->current_counters);
    analyzer->profiling_enabled = false;

    return result;
}

// 핫스팟 감지 (간단한 구현)
int et_detect_hotspots(ETPerformanceAnalyzer* analyzer) {
    if (!analyzer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 실제 구현에서는 프로파일링 데이터를 분석하여 핫스팟 감지
    // 여기서는 예시 데이터 생성

    analyzer->hotspot_count = 3;

    // 예시 핫스팟 1
    analyzer->hotspots[0] = (ETHotspot){
        .function_name = "et_tensor_matmul",
        .file_name = "tensor.c",
        .line_number = 245,
        .total_time_us = 50000,
        .call_count = 100,
        .avg_time_us = 500.0,
        .percentage = 45.2,
        .cpu_cycles = 2500000,
        .cache_misses = 1200,
        .cache_miss_rate = 0.15
    };

    // 예시 핫스팟 2
    analyzer->hotspots[1] = (ETHotspot){
        .function_name = "et_simd_vector_add",
        .file_name = "simd_kernels.c",
        .line_number = 128,
        .total_time_us = 25000,
        .call_count = 500,
        .avg_time_us = 50.0,
        .percentage = 22.6,
        .cpu_cycles = 1250000,
        .cache_misses = 300,
        .cache_miss_rate = 0.05
    };

    // 예시 핫스팟 3
    analyzer->hotspots[2] = (ETHotspot){
        .function_name = "et_fast_exp",
        .file_name = "fast_math.c",
        .line_number = 89,
        .total_time_us = 15000,
        .call_count = 1000,
        .avg_time_us = 15.0,
        .percentage = 13.5,
        .cpu_cycles = 750000,
        .cache_misses = 50,
        .cache_miss_rate = 0.02
    };

    return ET_SUCCESS;
}

// 핫스팟 조회
const ETHotspot* et_get_hotspots(ETPerformanceAnalyzer* analyzer, int* count) {
    if (!analyzer || !count) {
        return NULL;
    }

    *count = analyzer->hotspot_count;
    return analyzer->hotspots;
}

// 캐시 정보 조회
int et_get_cache_info(ETCacheInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETCacheInfo));

    // 기본값 설정 (실제로는 시스템에서 읽어야 함)
    info->cache_line_size = 64;
    info->l1_cache_size = 32 * 1024;      // 32KB
    info->l2_cache_size = 256 * 1024;     // 256KB
    info->l3_cache_size = 8 * 1024 * 1024; // 8MB
    info->associativity = 8;

#ifdef __linux__
    // Linux에서 캐시 정보 읽기
    FILE* cache_info = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if (cache_info) {
        fscanf(cache_info, "%zu", &info->cache_line_size);
        fclose(cache_info);
    }

    cache_info = fopen("/sys/devices/system/cpu/cpu0/cache/index0/size", "r");
    if (cache_info) {
        char size_str[32];
        if (fgets(size_str, sizeof(size_str), cache_info)) {
            info->l1_cache_size = atoi(size_str) * 1024; // KB to bytes
        }
        fclose(cache_info);
    }
#endif

    return ET_SUCCESS;
}

// 캐시 분석
int et_analyze_cache_performance(ETPerformanceAnalyzer* analyzer, ETCacheAnalysis* analysis) {
    if (!analyzer || !analysis) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(analysis, 0, sizeof(ETCacheAnalysis));

    // 예시 캐시 분석 데이터
    analysis->l1_cache_references = 1000000;
    analysis->l1_cache_misses = 50000;
    analysis->l1_miss_rate = (double)analysis->l1_cache_misses / analysis->l1_cache_references;

    analysis->l2_cache_references = 50000;
    analysis->l2_cache_misses = 10000;
    analysis->l2_miss_rate = (double)analysis->l2_cache_misses / analysis->l2_cache_references;

    analysis->l3_cache_references = 10000;
    analysis->l3_cache_misses = 2000;
    analysis->l3_miss_rate = (double)analysis->l3_cache_misses / analysis->l3_cache_references;

    analysis->memory_bandwidth_available = 25600; // MB/s
    analysis->memory_bandwidth_used = 12800;      // MB/s

    return ET_SUCCESS;
}

// 병목 분석
int et_analyze_bottlenecks(ETPerformanceAnalyzer* analyzer, ETBottleneckAnalysis* analysis) {
    if (!analyzer || !analysis) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(analysis, 0, sizeof(ETBottleneckAnalysis));

    // 캐시 분석 수행
    et_analyze_cache_performance(analyzer, &analysis->cache_analysis);

    // 핫스팟 감지
    et_detect_hotspots(analyzer);
    analysis->hotspots = analyzer->hotspots;
    analysis->num_hotspots = analyzer->hotspot_count;

    // 병목 유형 결정
    if (analysis->cache_analysis.l1_miss_rate > 0.1) {
        analysis->bottleneck_type = "Cache";
        analysis->description = "L1 캐시 미스율이 높음 (>10%)";
        analysis->severity_score = analysis->cache_analysis.l1_miss_rate;
        analysis->recommendation = "데이터 지역성 개선, 캐시 친화적 알고리즘 사용";
    } else if (analysis->cache_analysis.memory_bandwidth_used >
               analysis->cache_analysis.memory_bandwidth_available * 0.8) {
        analysis->bottleneck_type = "Memory";
        analysis->description = "메모리 대역폭 사용률이 높음 (>80%)";
        analysis->severity_score = (double)analysis->cache_analysis.memory_bandwidth_used /
                                   analysis->cache_analysis.memory_bandwidth_available;
        analysis->recommendation = "메모리 접근 패턴 최적화, 데이터 압축 고려";
    } else {
        analysis->bottleneck_type = "CPU";
        analysis->description = "CPU 집약적 연산이 주요 병목";
        analysis->severity_score = 0.6;
        analysis->recommendation = "SIMD 최적화, 병렬 처리 적용";
    }

    return ET_SUCCESS;
}

// 최적화 제안
int et_suggest_optimizations(ETPerformanceAnalyzer* analyzer,
                             ETOptimizationSuggestion** suggestions,
                             int* count) {
    if (!analyzer || !suggestions || !count) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 예시 최적화 제안들
    static ETOptimizationSuggestion opt_suggestions[] = {
        {
            .optimization_type = "SIMD",
            .description = "벡터 연산을 SIMD 명령어로 최적화",
            .code_location = "tensor.c:245 (et_tensor_matmul)",
            .expected_improvement = 2.5,
            .implementation_difficulty = 3,
            .implementation_hint = "AVX2 또는 NEON 명령어 사용하여 4개 또는 8개 요소를 동시 처리"
        },
        {
            .optimization_type = "Cache",
            .description = "메모리 접근 패턴을 캐시 친화적으로 변경",
            .code_location = "tensor.c:245 (et_tensor_matmul)",
            .expected_improvement = 1.8,
            .implementation_difficulty = 2,
            .implementation_hint = "행렬 곱셈을 블록 단위로 분할하여 캐시 지역성 향상"
        },
        {
            .optimization_type = "Memory",
            .description = "메모리 할당 패턴 최적화",
            .code_location = "memory.c:128",
            .expected_improvement = 1.3,
            .implementation_difficulty = 2,
            .implementation_hint = "메모리 풀 사용으로 동적 할당 오버헤드 감소"
        },
        {
            .optimization_type = "Algorithm",
            .description = "더 효율적인 알고리즘 적용",
            .code_location = "fast_math.c:89 (et_fast_exp)",
            .expected_improvement = 1.5,
            .implementation_difficulty = 4,
            .implementation_hint = "룩업 테이블과 선형 보간을 조합한 근사 함수 사용"
        }
    };

    *suggestions = opt_suggestions;
    *count = sizeof(opt_suggestions) / sizeof(opt_suggestions[0]);

    return ET_SUCCESS;
}

// 메모리 접근 패턴 분석
int et_analyze_memory_access(const void* data, size_t size,
                             const size_t* access_sequence, int access_count,
                             ETMemoryAccessAnalysis* analysis) {
    if (!data || !access_sequence || !analysis) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(analysis, 0, sizeof(ETMemoryAccessAnalysis));

    if (access_count < 2) {
        analysis->pattern = ET_ACCESS_PATTERN_SEQUENTIAL;
        analysis->locality_score = 1.0;
        analysis->cache_efficiency = 1.0;
        return ET_SUCCESS;
    }

    // 접근 패턴 분석
    bool is_sequential = true;
    bool is_strided = true;
    size_t stride = access_sequence[1] - access_sequence[0];

    for (int i = 1; i < access_count - 1; i++) {
        size_t current_stride = access_sequence[i + 1] - access_sequence[i];

        if (current_stride != 1) {
            is_sequential = false;
        }

        if (current_stride != stride) {
            is_strided = false;
        }
    }

    if (is_sequential) {
        analysis->pattern = ET_ACCESS_PATTERN_SEQUENTIAL;
        analysis->locality_score = 1.0;
        analysis->cache_efficiency = 0.95;
    } else if (is_strided) {
        analysis->pattern = ET_ACCESS_PATTERN_STRIDED;
        analysis->stride_size = stride;
        analysis->locality_score = stride <= 64 ? 0.8 : 0.4; // 캐시 라인 크기 기준
        analysis->cache_efficiency = stride <= 64 ? 0.7 : 0.3;
    } else {
        analysis->pattern = ET_ACCESS_PATTERN_RANDOM;
        analysis->locality_score = 0.2;
        analysis->cache_efficiency = 0.1;
    }

    return ET_SUCCESS;
}

// 데이터 레이아웃 최적화
int et_optimize_data_layout(void* data, size_t element_size, int element_count,
                            size_t cache_line_size) {
    if (!data || element_size == 0 || element_count <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 캐시 라인 정렬
    size_t elements_per_cache_line = cache_line_size / element_size;

    if (elements_per_cache_line > 1) {
        // 구조체 배열을 배열 구조체로 변환 (SoA 변환)
        // 실제 구현에서는 데이터 타입에 따라 다르게 처리
        printf("데이터 레이아웃 최적화: %zu개 요소를 캐시 라인 단위로 정렬\n",
               elements_per_cache_line);
    }

    return ET_SUCCESS;
}

// 성능 리포트 생성
int et_generate_performance_report(ETPerformanceAnalyzer* analyzer,
                                   ETPerformanceReport* report) {
    if (!analyzer || !report) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(report, 0, sizeof(ETPerformanceReport));

    report->title = "LibEtude 성능 분석 리포트";

    // 병목 분석
    et_analyze_bottlenecks(analyzer, &report->bottleneck_analysis);

    // 최적화 제안
    et_suggest_optimizations(analyzer, &report->suggestions, &report->suggestion_count);

    // 캐시 분석
    et_analyze_cache_performance(analyzer, &report->cache_analysis);

    // 전체 성능 점수 계산
    double cache_score = (1.0 - report->cache_analysis.l1_miss_rate) * 40.0;
    double cpu_score = (1.0 - report->bottleneck_analysis.severity_score) * 40.0;
    double memory_score = 20.0; // 기본 점수

    report->overall_performance_score = cache_score + cpu_score + memory_score;

    // 요약
    if (report->overall_performance_score >= 80.0) {
        report->summary = "성능이 우수합니다. 추가 최적화는 선택사항입니다.";
    } else if (report->overall_performance_score >= 60.0) {
        report->summary = "성능이 양호합니다. 일부 최적화를 고려해보세요.";
    } else {
        report->summary = "성능 개선이 필요합니다. 제안된 최적화를 적용하세요.";
    }

    return ET_SUCCESS;
}

// 성능 리포트 저장
int et_save_performance_report(const ETPerformanceReport* report,
                               const char* filename,
                               const char* format) {
    if (!report || !filename || !format) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(filename, "w");
    if (!file) {
        return ET_ERROR_IO;
    }

    if (strcmp(format, "json") == 0) {
        fprintf(file, "{\n");
        fprintf(file, "  \"title\": \"%s\",\n", report->title);
        fprintf(file, "  \"overall_score\": %.1f,\n", report->overall_performance_score);
        fprintf(file, "  \"summary\": \"%s\",\n", report->summary);
        fprintf(file, "  \"bottleneck\": {\n");
        fprintf(file, "    \"type\": \"%s\",\n", report->bottleneck_analysis.bottleneck_type);
        fprintf(file, "    \"description\": \"%s\",\n", report->bottleneck_analysis.description);
        fprintf(file, "    \"severity\": %.2f,\n", report->bottleneck_analysis.severity_score);
        fprintf(file, "    \"recommendation\": \"%s\"\n", report->bottleneck_analysis.recommendation);
        fprintf(file, "  },\n");
        fprintf(file, "  \"cache_analysis\": {\n");
        fprintf(file, "    \"l1_miss_rate\": %.3f,\n", report->cache_analysis.l1_miss_rate);
        fprintf(file, "    \"l2_miss_rate\": %.3f,\n", report->cache_analysis.l2_miss_rate);
        fprintf(file, "    \"l3_miss_rate\": %.3f\n", report->cache_analysis.l3_miss_rate);
        fprintf(file, "  }\n");
        fprintf(file, "}\n");
    } else {
        // 텍스트 형식
        fprintf(file, "%s\n", report->title);
        fprintf(file, "=====================================\n\n");
        fprintf(file, "전체 성능 점수: %.1f/100\n", report->overall_performance_score);
        fprintf(file, "요약: %s\n\n", report->summary);
        fprintf(file, "병목 분석:\n");
        fprintf(file, "  유형: %s\n", report->bottleneck_analysis.bottleneck_type);
        fprintf(file, "  설명: %s\n", report->bottleneck_analysis.description);
        fprintf(file, "  심각도: %.2f\n", report->bottleneck_analysis.severity_score);
        fprintf(file, "  권장사항: %s\n\n", report->bottleneck_analysis.recommendation);
        fprintf(file, "캐시 분석:\n");
        fprintf(file, "  L1 미스율: %.1f%%\n", report->cache_analysis.l1_miss_rate * 100);
        fprintf(file, "  L2 미스율: %.1f%%\n", report->cache_analysis.l2_miss_rate * 100);
        fprintf(file, "  L3 미스율: %.1f%%\n", report->cache_analysis.l3_miss_rate * 100);
    }

    fclose(file);
    return ET_SUCCESS;
}

// 성능 비교
int et_compare_performance(const ETPerformanceCounters* baseline,
                           const ETPerformanceCounters* optimized,
                           ETPerformanceComparison* comparison) {
    if (!baseline || !optimized || !comparison) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(comparison, 0, sizeof(ETPerformanceComparison));

    comparison->baseline_name = "기준";
    comparison->optimized_name = "최적화";

    // 속도 향상 계산
    if (baseline->cpu_cycles > 0) {
        comparison->speedup = (double)baseline->cpu_cycles / optimized->cpu_cycles;
    }

    // 캐시 개선 계산
    if (baseline->cache_references > 0 && optimized->cache_references > 0) {
        double baseline_miss_rate = (double)baseline->cache_misses / baseline->cache_references;
        double optimized_miss_rate = (double)optimized->cache_misses / optimized->cache_references;
        comparison->cache_improvement = baseline_miss_rate / optimized_miss_rate;
    }

    comparison->is_improvement = (comparison->speedup > 1.05); // 5% 이상 개선

    static char analysis_buffer[256];
    snprintf(analysis_buffer, sizeof(analysis_buffer),
             "최적화 결과: %.2fx 속도 향상, %.2fx 캐시 성능 개선",
             comparison->speedup, comparison->cache_improvement);
    comparison->analysis = analysis_buffer;

    return ET_SUCCESS;
}