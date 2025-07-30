#ifndef LIBETUDE_BENCHMARK_H
#define LIBETUDE_BENCHMARK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 벤치마크 결과 구조체
typedef struct {
    const char* name;                    // 벤치마크 이름
    double execution_time_ms;            // 실행 시간 (밀리초)
    double memory_usage_mb;              // 메모리 사용량 (MB)
    double cpu_usage_percent;            // CPU 사용률 (%)
    double gpu_usage_percent;            // GPU 사용률 (%)
    uint64_t operations_per_second;      // 초당 연산 수
    uint64_t memory_bandwidth_mbps;      // 메모리 대역폭 (MB/s)
    uint64_t cache_misses;               // 캐시 미스 횟수
    uint64_t instructions_retired;       // 실행된 명령어 수
    double energy_consumption_mj;        // 에너지 소비량 (밀리줄)
    bool success;                        // 벤치마크 성공 여부
    char error_message[256];             // 오류 메시지
} ETBenchmarkResult;

// 벤치마크 설정 구조체
typedef struct {
    int warmup_iterations;               // 워밍업 반복 횟수
    int measurement_iterations;          // 측정 반복 횟수
    double timeout_seconds;              // 타임아웃 (초)
    bool measure_memory;                 // 메모리 측정 여부
    bool measure_cpu;                    // CPU 측정 여부
    bool measure_gpu;                    // GPU 측정 여부
    bool measure_energy;                 // 에너지 측정 여부
    bool measure_cache;                  // 캐시 측정 여부
    const char* output_format;           // 출력 형식 ("json", "csv", "text")
} ETBenchmarkConfig;

// 벤치마크 함수 타입
typedef void (*ETBenchmarkFunc)(void* user_data);

// 벤치마크 스위트 구조체
typedef struct {
    const char* name;                    // 스위트 이름
    ETBenchmarkFunc* benchmarks;         // 벤치마크 함수 배열
    const char** benchmark_names;        // 벤치마크 이름 배열
    void** user_data;                    // 사용자 데이터 배열
    int num_benchmarks;                  // 벤치마크 수
    ETBenchmarkConfig config;            // 설정
    ETBenchmarkResult* results;          // 결과 배열
} ETBenchmarkSuite;

// 비교 분석 결과 구조체
typedef struct {
    const char* baseline_name;           // 기준 벤치마크 이름
    const char* comparison_name;         // 비교 벤치마크 이름
    double speedup_ratio;                // 속도 향상 비율
    double memory_ratio;                 // 메모리 사용량 비율
    double energy_ratio;                 // 에너지 효율성 비율
    bool is_improvement;                 // 개선 여부
    char analysis[512];                  // 분석 결과
} ETBenchmarkComparison;

// 벤치마크 프레임워크 초기화/해제
int et_benchmark_init();
void et_benchmark_cleanup();

// 단일 벤치마크 실행
int et_run_benchmark(const char* name,
                     ETBenchmarkFunc benchmark_func,
                     void* user_data,
                     const ETBenchmarkConfig* config,
                     ETBenchmarkResult* result);

// 벤치마크 스위트 생성/해제
ETBenchmarkSuite* et_create_benchmark_suite(const char* name,
                                             const ETBenchmarkConfig* config);
void et_destroy_benchmark_suite(ETBenchmarkSuite* suite);

// 벤치마크 스위트에 벤치마크 추가
int et_add_benchmark(ETBenchmarkSuite* suite,
                     const char* name,
                     ETBenchmarkFunc benchmark_func,
                     void* user_data);

// 벤치마크 스위트 실행
int et_run_benchmark_suite(ETBenchmarkSuite* suite);

// 결과 출력 및 저장
void et_print_benchmark_results(const ETBenchmarkResult* results, int num_results);
int et_save_benchmark_results(const ETBenchmarkResult* results,
                              int num_results,
                              const char* filename,
                              const char* format);

// 비교 분석
int et_compare_benchmarks(const ETBenchmarkResult* baseline,
                          const ETBenchmarkResult* comparison,
                          ETBenchmarkComparison* result);

// 통계 분석
double et_calculate_mean(const double* values, int count);
double et_calculate_stddev(const double* values, int count);
double et_calculate_percentile(const double* values, int count, double percentile);

// 시스템 정보 수집
typedef struct {
    char cpu_name[128];                  // CPU 이름
    int cpu_cores;                       // CPU 코어 수
    int cpu_threads;                     // CPU 스레드 수
    uint64_t memory_total_mb;            // 총 메모리 (MB)
    uint64_t memory_available_mb;        // 사용 가능한 메모리 (MB)
    char gpu_name[128];                  // GPU 이름
    uint64_t gpu_memory_mb;              // GPU 메모리 (MB)
    char os_name[64];                    // 운영체제 이름
    char compiler_version[64];           // 컴파일러 버전
} ETSystemInfo;

int et_get_system_info(ETSystemInfo* info);

// 기본 벤치마크 설정
extern const ETBenchmarkConfig ET_BENCHMARK_CONFIG_DEFAULT;
extern const ETBenchmarkConfig ET_BENCHMARK_CONFIG_QUICK;
extern const ETBenchmarkConfig ET_BENCHMARK_CONFIG_THOROUGH;

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_BENCHMARK_H