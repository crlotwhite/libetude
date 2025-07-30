#ifndef LIBETUDE_PERFORMANCE_ANALYZER_H
#define LIBETUDE_PERFORMANCE_ANALYZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 성능 카운터 타입
typedef enum {
    ET_PERF_COUNTER_CPU_CYCLES = 0,
    ET_PERF_COUNTER_INSTRUCTIONS,
    ET_PERF_COUNTER_CACHE_MISSES,
    ET_PERF_COUNTER_CACHE_REFERENCES,
    ET_PERF_COUNTER_BRANCH_MISSES,
    ET_PERF_COUNTER_BRANCH_INSTRUCTIONS,
    ET_PERF_COUNTER_PAGE_FAULTS,
    ET_PERF_COUNTER_CONTEXT_SWITCHES,
    ET_PERF_COUNTER_MAX
} ETPerformanceCounterType;

// 성능 카운터 값
typedef struct {
    uint64_t cpu_cycles;
    uint64_t instructions;
    uint64_t cache_misses;
    uint64_t cache_references;
    uint64_t branch_misses;
    uint64_t branch_instructions;
    uint64_t page_faults;
    uint64_t context_switches;
    uint64_t timestamp_us;
} ETPerformanceCounters;

// 핫스팟 정보
typedef struct {
    const char* function_name;
    const char* file_name;
    int line_number;
    uint64_t total_time_us;
    uint64_t call_count;
    double avg_time_us;
    double percentage;
    uint64_t cpu_cycles;
    uint64_t cache_misses;
    double cache_miss_rate;
} ETHotspot;

// 캐시 분석 정보
typedef struct {
    uint64_t l1_cache_misses;
    uint64_t l1_cache_references;
    uint64_t l2_cache_misses;
    uint64_t l2_cache_references;
    uint64_t l3_cache_misses;
    uint64_t l3_cache_references;
    double l1_miss_rate;
    double l2_miss_rate;
    double l3_miss_rate;
    uint64_t memory_bandwidth_used;
    uint64_t memory_bandwidth_available;
} ETCacheAnalysis;

// 병목 분석 결과
typedef struct {
    const char* bottleneck_type;     // "CPU", "Memory", "Cache", "I/O"
    const char* description;
    double severity_score;           // 0.0 ~ 1.0
    const char* recommendation;
    ETHotspot* hotspots;
    int num_hotspots;
    ETCacheAnalysis cache_analysis;
} ETBottleneckAnalysis;

// 성능 분석기 컨텍스트
typedef struct {
    bool profiling_enabled;
    bool cache_analysis_enabled;
    bool hotspot_detection_enabled;
    int max_hotspots;
    double hotspot_threshold_percent;

    // 내부 상태
    ETPerformanceCounters start_counters;
    ETPerformanceCounters current_counters;
    ETHotspot* hotspots;
    int hotspot_count;

    // 프로파일링 데이터
    void* profiling_data;
    size_t profiling_data_size;
} ETPerformanceAnalyzer;

// 성능 분석기 생성/해제
ETPerformanceAnalyzer* et_create_performance_analyzer();
void et_destroy_performance_analyzer(ETPerformanceAnalyzer* analyzer);

// 프로파일링 시작/중지
int et_start_profiling(ETPerformanceAnalyzer* analyzer);
int et_stop_profiling(ETPerformanceAnalyzer* analyzer);

// 성능 카운터 읽기
int et_read_performance_counters(ETPerformanceCounters* counters);

// 핫스팟 감지
int et_detect_hotspots(ETPerformanceAnalyzer* analyzer);
const ETHotspot* et_get_hotspots(ETPerformanceAnalyzer* analyzer, int* count);

// 캐시 분석
int et_analyze_cache_performance(ETPerformanceAnalyzer* analyzer, ETCacheAnalysis* analysis);

// 병목 분석
int et_analyze_bottlenecks(ETPerformanceAnalyzer* analyzer, ETBottleneckAnalysis* analysis);

// 최적화 제안
typedef struct {
    const char* optimization_type;   // "SIMD", "Cache", "Memory", "Algorithm"
    const char* description;
    const char* code_location;
    double expected_improvement;     // 예상 성능 향상 (배수)
    int implementation_difficulty;   // 1-5 (쉬움-어려움)
    const char* implementation_hint;
} ETOptimizationSuggestion;

int et_suggest_optimizations(ETPerformanceAnalyzer* analyzer,
                             ETOptimizationSuggestion** suggestions,
                             int* count);

// 캐시 최적화 도구
typedef struct {
    size_t cache_line_size;
    size_t l1_cache_size;
    size_t l2_cache_size;
    size_t l3_cache_size;
    int associativity;
} ETCacheInfo;

int et_get_cache_info(ETCacheInfo* info);

// 메모리 접근 패턴 분석
typedef enum {
    ET_ACCESS_PATTERN_SEQUENTIAL = 0,
    ET_ACCESS_PATTERN_RANDOM,
    ET_ACCESS_PATTERN_STRIDED,
    ET_ACCESS_PATTERN_BLOCKED
} ETMemoryAccessPattern;

typedef struct {
    ETMemoryAccessPattern pattern;
    size_t stride_size;
    size_t block_size;
    double locality_score;         // 0.0 ~ 1.0
    double cache_efficiency;       // 0.0 ~ 1.0
} ETMemoryAccessAnalysis;

int et_analyze_memory_access(const void* data, size_t size,
                             const size_t* access_sequence, int access_count,
                             ETMemoryAccessAnalysis* analysis);

// 캐시 친화적 데이터 구조 최적화
int et_optimize_data_layout(void* data, size_t element_size, int element_count,
                            size_t cache_line_size);

// 루프 최적화 분석
typedef struct {
    bool vectorizable;
    bool parallelizable;
    int unroll_factor;
    bool has_dependencies;
    const char* optimization_hint;
} ETLoopAnalysis;

int et_analyze_loop_optimization(const char* loop_code, ETLoopAnalysis* analysis);

// 성능 리포트 생성
typedef struct {
    const char* title;
    ETBottleneckAnalysis bottleneck_analysis;
    ETOptimizationSuggestion* suggestions;
    int suggestion_count;
    ETCacheAnalysis cache_analysis;
    double overall_performance_score;  // 0.0 ~ 100.0
    const char* summary;
} ETPerformanceReport;

int et_generate_performance_report(ETPerformanceAnalyzer* analyzer,
                                   ETPerformanceReport* report);

int et_save_performance_report(const ETPerformanceReport* report,
                               const char* filename,
                               const char* format); // "html", "json", "text"

// 실시간 성능 모니터링
typedef void (*ETPerformanceCallback)(const ETPerformanceCounters* counters, void* user_data);

int et_start_performance_monitoring(ETPerformanceAnalyzer* analyzer,
                                    ETPerformanceCallback callback,
                                    void* user_data,
                                    int interval_ms);

int et_stop_performance_monitoring(ETPerformanceAnalyzer* analyzer);

// 성능 비교
typedef struct {
    const char* baseline_name;
    const char* optimized_name;
    double speedup;
    double memory_reduction;
    double cache_improvement;
    double energy_efficiency;
    bool is_improvement;
    const char* analysis;
} ETPerformanceComparison;

int et_compare_performance(const ETPerformanceCounters* baseline,
                           const ETPerformanceCounters* optimized,
                           ETPerformanceComparison* comparison);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PERFORMANCE_ANALYZER_H