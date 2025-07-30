#include "libetude/benchmark.h"
#include "libetude/error.h"
#include "libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

// 기본 벤치마크 설정
const ETBenchmarkConfig ET_BENCHMARK_CONFIG_DEFAULT = {
    .warmup_iterations = 3,
    .measurement_iterations = 10,
    .timeout_seconds = 30.0,
    .measure_memory = true,
    .measure_cpu = true,
    .measure_gpu = false,
    .measure_energy = false,
    .measure_cache = false,
    .output_format = "text"
};

const ETBenchmarkConfig ET_BENCHMARK_CONFIG_QUICK = {
    .warmup_iterations = 1,
    .measurement_iterations = 3,
    .timeout_seconds = 10.0,
    .measure_memory = true,
    .measure_cpu = false,
    .measure_gpu = false,
    .measure_energy = false,
    .measure_cache = false,
    .output_format = "text"
};

const ETBenchmarkConfig ET_BENCHMARK_CONFIG_THOROUGH = {
    .warmup_iterations = 5,
    .measurement_iterations = 20,
    .timeout_seconds = 120.0,
    .measure_memory = true,
    .measure_cpu = true,
    .measure_gpu = true,
    .measure_energy = true,
    .measure_cache = true,
    .output_format = "json"
};

// 전역 벤치마크 상태
static bool g_benchmark_initialized = false;

// 고해상도 타이머 함수
static uint64_t get_high_resolution_time() {
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

// 메모리 사용량 측정
static uint64_t get_memory_usage_mb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024 * 1024);
    }
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
        return info.resident_size / (1024 * 1024);
    }
#elif defined(__linux__)
    FILE* file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                uint64_t kb;
                if (sscanf(line, "VmRSS: %lu kB", &kb) == 1) {
                    fclose(file);
                    return kb / 1024;
                }
            }
        }
        fclose(file);
    }
#endif
    return 0;
}

// CPU 사용률 측정 (간단한 구현)
static double get_cpu_usage_percent() {
    // 실제 구현에서는 더 정확한 CPU 사용률 측정이 필요
    // 여기서는 간단한 예시만 제공
    return 0.0;
}

// 벤치마크 프레임워크 초기화
int et_benchmark_init() {
    if (g_benchmark_initialized) {
        return ET_SUCCESS;
    }

    g_benchmark_initialized = true;
    return ET_SUCCESS;
}

// 벤치마크 프레임워크 해제
void et_benchmark_cleanup() {
    g_benchmark_initialized = false;
}

// 단일 벤치마크 실행
int et_run_benchmark(const char* name,
                     ETBenchmarkFunc benchmark_func,
                     void* user_data,
                     const ETBenchmarkConfig* config,
                     ETBenchmarkResult* result) {
    if (!name || !benchmark_func || !result) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!g_benchmark_initialized) {
        return ET_ERROR_RUNTIME;
    }

    // 결과 초기화
    memset(result, 0, sizeof(ETBenchmarkResult));
    result->name = name;
    result->success = false;

    const ETBenchmarkConfig* cfg = config ? config : &ET_BENCHMARK_CONFIG_DEFAULT;

    // 워밍업 실행
    for (int i = 0; i < cfg->warmup_iterations; i++) {
        benchmark_func(user_data);
    }

    // 측정 배열
    double* execution_times = malloc(cfg->measurement_iterations * sizeof(double));
    uint64_t* memory_usages = malloc(cfg->measurement_iterations * sizeof(uint64_t));

    if (!execution_times || !memory_usages) {
        free(execution_times);
        free(memory_usages);
        strcpy(result->error_message, "메모리 할당 실패");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 측정 실행
    for (int i = 0; i < cfg->measurement_iterations; i++) {
        uint64_t start_memory = cfg->measure_memory ? get_memory_usage_mb() : 0;
        uint64_t start_time = get_high_resolution_time();

        benchmark_func(user_data);

        uint64_t end_time = get_high_resolution_time();
        uint64_t end_memory = cfg->measure_memory ? get_memory_usage_mb() : 0;

        execution_times[i] = (end_time - start_time) / 1000.0; // 마이크로초를 밀리초로 변환
        memory_usages[i] = end_memory > start_memory ? end_memory - start_memory : 0;
    }

    // 통계 계산
    result->execution_time_ms = et_calculate_mean(execution_times, cfg->measurement_iterations);

    if (cfg->measure_memory) {
        double avg_memory = 0.0;
        for (int i = 0; i < cfg->measurement_iterations; i++) {
            avg_memory += memory_usages[i];
        }
        result->memory_usage_mb = avg_memory / cfg->measurement_iterations;
    }

    if (cfg->measure_cpu) {
        result->cpu_usage_percent = get_cpu_usage_percent();
    }

    // 연산 수 계산 (예시)
    if (result->execution_time_ms > 0) {
        result->operations_per_second = (uint64_t)(1000.0 / result->execution_time_ms);
    }

    result->success = true;

    free(execution_times);
    free(memory_usages);

    return ET_SUCCESS;
}

// 벤치마크 스위트 생성
ETBenchmarkSuite* et_create_benchmark_suite(const char* name,
                                             const ETBenchmarkConfig* config) {
    if (!name) {
        return NULL;
    }

    ETBenchmarkSuite* suite = malloc(sizeof(ETBenchmarkSuite));
    if (!suite) {
        return NULL;
    }

    memset(suite, 0, sizeof(ETBenchmarkSuite));
    suite->name = strdup(name);
    suite->config = config ? *config : ET_BENCHMARK_CONFIG_DEFAULT;

    return suite;
}

// 벤치마크 스위트 해제
void et_destroy_benchmark_suite(ETBenchmarkSuite* suite) {
    if (!suite) {
        return;
    }

    free((void*)suite->name);
    free(suite->benchmarks);

    if (suite->benchmark_names) {
        for (int i = 0; i < suite->num_benchmarks; i++) {
            free((void*)suite->benchmark_names[i]);
        }
        free(suite->benchmark_names);
    }

    free(suite->user_data);
    free(suite->results);
    free(suite);
}

// 벤치마크 스위트에 벤치마크 추가
int et_add_benchmark(ETBenchmarkSuite* suite,
                     const char* name,
                     ETBenchmarkFunc benchmark_func,
                     void* user_data) {
    if (!suite || !name || !benchmark_func) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 배열 크기 확장
    suite->benchmarks = realloc(suite->benchmarks,
                                (suite->num_benchmarks + 1) * sizeof(ETBenchmarkFunc));
    suite->benchmark_names = realloc(suite->benchmark_names,
                                     (suite->num_benchmarks + 1) * sizeof(const char*));
    suite->user_data = realloc(suite->user_data,
                               (suite->num_benchmarks + 1) * sizeof(void*));

    if (!suite->benchmarks || !suite->benchmark_names || !suite->user_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    suite->benchmarks[suite->num_benchmarks] = benchmark_func;
    suite->benchmark_names[suite->num_benchmarks] = strdup(name);
    suite->user_data[suite->num_benchmarks] = user_data;
    suite->num_benchmarks++;

    return ET_SUCCESS;
}

// 벤치마크 스위트 실행
int et_run_benchmark_suite(ETBenchmarkSuite* suite) {
    if (!suite) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 결과 배열 할당
    suite->results = malloc(suite->num_benchmarks * sizeof(ETBenchmarkResult));
    if (!suite->results) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    printf("벤치마크 스위트 실행: %s\n", suite->name);
    printf("========================================\n");

    // 각 벤치마크 실행
    for (int i = 0; i < suite->num_benchmarks; i++) {
        printf("실행 중: %s... ", suite->benchmark_names[i]);
        fflush(stdout);

        int result = et_run_benchmark(suite->benchmark_names[i],
                                      suite->benchmarks[i],
                                      suite->user_data[i],
                                      &suite->config,
                                      &suite->results[i]);

        if (result == ET_SUCCESS && suite->results[i].success) {
            printf("완료 (%.2f ms)\n", suite->results[i].execution_time_ms);
        } else {
            printf("실패: %s\n", suite->results[i].error_message);
        }
    }

    printf("========================================\n");
    et_print_benchmark_results(suite->results, suite->num_benchmarks);

    return ET_SUCCESS;
}

// 결과 출력
void et_print_benchmark_results(const ETBenchmarkResult* results, int num_results) {
    if (!results || num_results <= 0) {
        return;
    }

    printf("\n벤치마크 결과:\n");
    printf("%-30s %12s %12s %12s %15s\n",
           "이름", "시간(ms)", "메모리(MB)", "CPU(%)", "연산/초");
    printf("--------------------------------------------------------------------------------\n");

    for (int i = 0; i < num_results; i++) {
        if (results[i].success) {
            printf("%-30s %12.3f %12.1f %12.1f %15lu\n",
                   results[i].name,
                   results[i].execution_time_ms,
                   results[i].memory_usage_mb,
                   results[i].cpu_usage_percent,
                   results[i].operations_per_second);
        } else {
            printf("%-30s %12s %12s %12s %15s\n",
                   results[i].name, "실패", "-", "-", "-");
        }
    }
    printf("\n");
}

// 결과 저장
int et_save_benchmark_results(const ETBenchmarkResult* results,
                              int num_results,
                              const char* filename,
                              const char* format) {
    if (!results || !filename || !format) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(filename, "w");
    if (!file) {
        return ET_ERROR_IO;
    }

    if (strcmp(format, "json") == 0) {
        fprintf(file, "{\n  \"benchmarks\": [\n");
        for (int i = 0; i < num_results; i++) {
            fprintf(file, "    {\n");
            fprintf(file, "      \"name\": \"%s\",\n", results[i].name);
            fprintf(file, "      \"execution_time_ms\": %.3f,\n", results[i].execution_time_ms);
            fprintf(file, "      \"memory_usage_mb\": %.1f,\n", results[i].memory_usage_mb);
            fprintf(file, "      \"cpu_usage_percent\": %.1f,\n", results[i].cpu_usage_percent);
            fprintf(file, "      \"operations_per_second\": %lu,\n", results[i].operations_per_second);
            fprintf(file, "      \"success\": %s\n", results[i].success ? "true" : "false");
            fprintf(file, "    }%s\n", i < num_results - 1 ? "," : "");
        }
        fprintf(file, "  ]\n}\n");
    } else if (strcmp(format, "csv") == 0) {
        fprintf(file, "Name,ExecutionTime(ms),Memory(MB),CPU(%%),Operations/sec,Success\n");
        for (int i = 0; i < num_results; i++) {
            fprintf(file, "%s,%.3f,%.1f,%.1f,%lu,%s\n",
                    results[i].name,
                    results[i].execution_time_ms,
                    results[i].memory_usage_mb,
                    results[i].cpu_usage_percent,
                    results[i].operations_per_second,
                    results[i].success ? "true" : "false");
        }
    }

    fclose(file);
    return ET_SUCCESS;
}

// 비교 분석
int et_compare_benchmarks(const ETBenchmarkResult* baseline,
                          const ETBenchmarkResult* comparison,
                          ETBenchmarkComparison* result) {
    if (!baseline || !comparison || !result) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(result, 0, sizeof(ETBenchmarkComparison));
    result->baseline_name = baseline->name;
    result->comparison_name = comparison->name;

    if (baseline->execution_time_ms > 0) {
        result->speedup_ratio = baseline->execution_time_ms / comparison->execution_time_ms;
    }

    if (baseline->memory_usage_mb > 0) {
        result->memory_ratio = comparison->memory_usage_mb / baseline->memory_usage_mb;
    }

    if (baseline->energy_consumption_mj > 0) {
        result->energy_ratio = comparison->energy_consumption_mj / baseline->energy_consumption_mj;
    }

    result->is_improvement = (result->speedup_ratio > 1.0) && (result->memory_ratio <= 1.1);

    snprintf(result->analysis, sizeof(result->analysis),
             "%s는 %s 대비 %.2fx %s, 메모리 사용량 %.1f%% %s",
             comparison->name, baseline->name,
             result->speedup_ratio > 1.0 ? result->speedup_ratio : 1.0 / result->speedup_ratio,
             result->speedup_ratio > 1.0 ? "빠름" : "느림",
             result->memory_ratio * 100.0,
             result->memory_ratio > 1.0 ? "증가" : "감소");

    return ET_SUCCESS;
}

// 통계 함수들
double et_calculate_mean(const double* values, int count) {
    if (!values || count <= 0) {
        return 0.0;
    }

    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}

double et_calculate_stddev(const double* values, int count) {
    if (!values || count <= 1) {
        return 0.0;
    }

    double mean = et_calculate_mean(values, count);
    double sum_sq_diff = 0.0;

    for (int i = 0; i < count; i++) {
        double diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }

    return sqrt(sum_sq_diff / (count - 1));
}

double et_calculate_percentile(const double* values, int count, double percentile) {
    if (!values || count <= 0 || percentile < 0.0 || percentile > 100.0) {
        return 0.0;
    }

    // 간단한 구현 - 실제로는 정렬이 필요
    double* sorted = malloc(count * sizeof(double));
    memcpy(sorted, values, count * sizeof(double));

    // 버블 정렬 (간단한 구현)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                double temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }

    int index = (int)((percentile / 100.0) * (count - 1));
    double result = sorted[index];

    free(sorted);
    return result;
}

// 시스템 정보 수집
int et_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETSystemInfo));

#ifdef _WIN32
    // Windows 시스템 정보
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->cpu_cores = si.dwNumberOfProcessors;
    info->cpu_threads = si.dwNumberOfProcessors;

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    info->memory_total_mb = memInfo.ullTotalPhys / (1024 * 1024);
    info->memory_available_mb = memInfo.ullAvailPhys / (1024 * 1024);

    strcpy(info->os_name, "Windows");
    strcpy(info->cpu_name, "Unknown CPU");

#elif defined(__APPLE__)
    // macOS 시스템 정보
    size_t size = sizeof(int);
    sysctlbyname("hw.ncpu", &info->cpu_cores, &size, NULL, 0);
    sysctlbyname("hw.logicalcpu", &info->cpu_threads, &size, NULL, 0);

    uint64_t memsize;
    size = sizeof(uint64_t);
    sysctlbyname("hw.memsize", &memsize, &size, NULL, 0);
    info->memory_total_mb = memsize / (1024 * 1024);

    strcpy(info->os_name, "macOS");
    strcpy(info->cpu_name, "Apple CPU");

#elif defined(__linux__)
    // Linux 시스템 정보
    info->cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    info->cpu_threads = info->cpu_cores;

    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info->memory_total_mb = si.totalram * si.mem_unit / (1024 * 1024);
        info->memory_available_mb = si.freeram * si.mem_unit / (1024 * 1024);
    }

    strcpy(info->os_name, "Linux");
    strcpy(info->cpu_name, "Unknown CPU");
#endif

    strcpy(info->compiler_version, __VERSION__);

    return ET_SUCCESS;
}