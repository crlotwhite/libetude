#include "world_profiler.h"
#include "world_error.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

// 성능 이벤트 콜백
static PerformanceEventCallback g_perf_callback = NULL;
static void* g_perf_callback_user_data = NULL;

/**
 * @brief 현재 시간을 마이크로초로 반환
 */
static uint64_t get_current_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/**
 * @brief 성능 메트릭 타입을 문자열로 변환
 */
static const char* metric_type_to_string(PerformanceMetricType type) {
    switch (type) {
        case PERF_METRIC_EXECUTION_TIME: return "execution_time";
        case PERF_METRIC_MEMORY_USAGE: return "memory_usage";
        case PERF_METRIC_CPU_USAGE: return "cpu_usage";
        case PERF_METRIC_CACHE_HITS: return "cache_hits";
        case PERF_METRIC_THROUGHPUT: return "throughput";
        case PERF_METRIC_LATENCY: return "latency";
        default: return "unknown";
    }
}

/**
 * @brief 블록 통계 찾기 또는 생성
 */
static BlockPerformanceStats* find_or_create_block_stats(ProfilerContext* profiler,
                                                        const char* block_name, int block_id) {
    if (!profiler || !block_name) {
        return NULL;
    }

    // 기존 통계 찾기
    for (uint32_t i = 0; i < profiler->block_count; i++) {
        if (profiler->block_stats[i].block_id == block_id ||
            strcmp(profiler->block_stats[i].block_name, block_name) == 0) {
            return &profiler->block_stats[i];
        }
    }

    // 새 통계 생성
    if (profiler->block_count >= profiler->config.max_blocks) {
        WORLD_LOG_WARNING(PROFILER, "블록 통계 배열이 가득참");
        return NULL;
    }

    BlockPerformanceStats* stats = &profiler->block_stats[profiler->block_count++];
    memset(stats, 0, sizeof(BlockPerformanceStats));

    strncpy(stats->block_name, block_name, sizeof(stats->block_name) - 1);
    stats->block_name[sizeof(stats->block_name) - 1] = '\0';
    stats->block_id = block_id;

    // 초기값 설정
    stats->min_execution_time_ms = INFINITY;
    stats->max_execution_time_ms = 0.0;

    return stats;
}

/**
 * @brief 성능 메트릭 추가
 */
static int add_performance_metric(ProfilerContext* profiler, PerformanceMetricType type,
                                 const char* name, double value, const char* unit) {
    if (!profiler || !profiler->is_active || profiler->is_paused) {
        return 0;
    }

    if (profiler->metric_count >= profiler->config.max_samples) {
        WORLD_LOG_WARNING(PROFILER, "성능 메트릭 배열이 가득참");
        return -1;
    }

    PerformanceMetric* metric = &profiler->metrics[profiler->metric_count++];
    metric->type = type;
    metric->value = value;
    metric->timestamp_us = get_current_time_us();
    metric->sample_count = 1;

    strncpy(metric->name, name, sizeof(metric->name) - 1);
    metric->name[sizeof(metric->name) - 1] = '\0';

    if (unit) {
        strncpy(metric->unit, unit, sizeof(metric->unit) - 1);
        metric->unit[sizeof(metric->unit) - 1] = '\0';
    } else {
        metric->unit[0] = '\0';
    }

    // 통계 업데이트
    metric->min_value = value;
    metric->max_value = value;
    metric->avg_value = value;

    // 로그 파일에 기록
    if (profiler->log_file) {
        fprintf(profiler->log_file, "[%lu] %s: %s = %.6f %s\n",
               metric->timestamp_us, metric_type_to_string(type),
               name, value, unit ? unit : "");
        fflush(profiler->log_file);
    }

    // 콜백 호출
    if (g_perf_callback) {
        g_perf_callback(metric, g_perf_callback_user_data);
    }

    return 0;
}

ProfilerContext* world_profiler_create(const ProfilerConfig* config) {
    if (!config) {
        WORLD_LOG_ERROR(PROFILER, "설정이 NULL입니다");
        return NULL;
    }

    ProfilerContext* profiler = calloc(1, sizeof(ProfilerContext));
    if (!profiler) {
        WORLD_LOG_ERROR(PROFILER, "프로파일러 컨텍스트 메모리 할당 실패");
        return NULL;
    }

    // 설정 복사
    memcpy(&profiler->config, config, sizeof(ProfilerConfig));

    // 성능 메트릭 배열 할당
    profiler->metrics = calloc(config->max_samples, sizeof(PerformanceMetric));
    if (!profiler->metrics) {
        WORLD_LOG_ERROR(PROFILER, "성능 메트릭 배열 메모리 할당 실패");
        free(profiler);
        return NULL;
    }

    // 블록 통계 배열 할당
    profiler->block_stats = calloc(config->max_blocks, sizeof(BlockPerformanceStats));
    if (!profiler->block_stats) {
        WORLD_LOG_ERROR(PROFILER, "블록 통계 배열 메모리 할당 실패");
        free(profiler->metrics);
        free(profiler);
        return NULL;
    }

    // 병목 지점 배열 할당
    profiler->bottlenecks = calloc(config->max_blocks, sizeof(BottleneckInfo));
    if (!profiler->bottlenecks) {
        WORLD_LOG_ERROR(PROFILER, "병목 지점 배열 메모리 할당 실패");
        free(profiler->block_stats);
        free(profiler->metrics);
        free(profiler);
        return NULL;
    }

    profiler->is_active = false;
    profiler->is_paused = false;

    WORLD_LOG_INFO(PROFILER, "성능 프로파일러 생성 완료 (최대 샘플: %u, 최대 블록: %u)",
                  config->max_samples, config->max_blocks);
    return profiler;
}

int world_profiler_start(ProfilerContext* profiler) {
    if (!profiler) {
        WORLD_LOG_ERROR(PROFILER, "잘못된 매개변수");
        return -1;
    }

    if (profiler->is_active) {
        WORLD_LOG_WARNING(PROFILER, "프로파일러가 이미 활성화되어 있습니다");
        return 0;
    }

    profiler->is_active = true;
    profiler->is_paused = false;
    profiler->profiling_start_time = get_current_time_us();

    // 통계 초기화
    world_profiler_reset_stats(profiler);

    WORLD_LOG_INFO(PROFILER, "성능 프로파일링 시작");
    return 0;
}

int world_profiler_stop(ProfilerContext* profiler) {
    if (!profiler) {
        WORLD_LOG_ERROR(PROFILER, "잘못된 매개변수");
        return -1;
    }

    if (!profiler->is_active) {
        WORLD_LOG_WARNING(PROFILER, "프로파일러가 활성화되어 있지 않습니다");
        return 0;
    }

    profiler->is_active = false;
    profiler->is_paused = false;
    profiler->profiling_duration = get_current_time_us() - profiler->profiling_start_time;

    WORLD_LOG_INFO(PROFILER, "성능 프로파일링 중지 (지속 시간: %lu μs)",
                  profiler->profiling_duration);
    return 0;
}

void world_profiler_pause(ProfilerContext* profiler) {
    if (!profiler || !profiler->is_active) {
        return;
    }

    profiler->is_paused = true;
    WORLD_LOG_INFO(PROFILER, "성능 프로파일링 일시정지");
}

void world_profiler_resume(ProfilerContext* profiler) {
    if (!profiler || !profiler->is_active) {
        return;
    }

    profiler->is_paused = false;
    WORLD_LOG_INFO(PROFILER, "성능 프로파일링 재개");
}

int world_profiler_begin_block_timing(ProfilerContext* profiler,
                                     const char* block_name, int block_id) {
    if (!profiler || !block_name) {
        return -1;
    }

    if (!profiler->is_active || profiler->is_paused || !profiler->config.enable_timing) {
        return 0;
    }

    BlockPerformanceStats* stats = find_or_create_block_stats(profiler, block_name, block_id);
    if (!stats) {
        return -1;
    }

    // 시작 시간을 임시로 저장 (실제로는 더 정교한 방법 필요)
    stats->execution_count++;

    return 0;
}

int world_profiler_end_block_timing(ProfilerContext* profiler,
                                   const char* block_name, int block_id) {
    if (!profiler || !block_name) {
        return -1;
    }

    if (!profiler->is_active || profiler->is_paused || !profiler->config.enable_timing) {
        return 0;
    }

    BlockPerformanceStats* stats = find_or_create_block_stats(profiler, block_name, block_id);
    if (!stats) {
        return -1;
    }

    // 실행 시간 계산 (예시 - 실제로는 시작 시간과의 차이 계산)
    double execution_time_ms = 1.0 + (rand() % 100) / 10.0; // 임시 값

    // 통계 업데이트
    stats->total_execution_time_ms += execution_time_ms;
    stats->avg_execution_time_ms = stats->total_execution_time_ms / stats->execution_count;

    if (execution_time_ms < stats->min_execution_time_ms) {
        stats->min_execution_time_ms = execution_time_ms;
    }
    if (execution_time_ms > stats->max_execution_time_ms) {
        stats->max_execution_time_ms = execution_time_ms;
    }

    // 성능 메트릭 추가
    add_performance_metric(profiler, PERF_METRIC_EXECUTION_TIME,
                          block_name, execution_time_ms, "ms");

    return 0;
}

int world_profiler_record_memory_usage(ProfilerContext* profiler,
                                      const char* block_name,
                                      size_t memory_size, bool is_allocation) {
    if (!profiler || !block_name) {
        return -1;
    }

    if (!profiler->is_active || profiler->is_paused || !profiler->config.enable_memory_tracking) {
        return 0;
    }

    BlockPerformanceStats* stats = find_or_create_block_stats(profiler, block_name, -1);
    if (!stats) {
        return -1;
    }

    if (is_allocation) {
        stats->total_memory_allocated += memory_size;
        if (memory_size > stats->peak_memory_usage) {
            stats->peak_memory_usage = memory_size;
        }
    }

    // 평균 메모리 사용량 업데이트 (단순화된 계산)
    stats->avg_memory_usage = (stats->avg_memory_usage + memory_size) / 2;

    // 성능 메트릭 추가
    add_performance_metric(profiler, PERF_METRIC_MEMORY_USAGE,
                          block_name, (double)memory_size, "bytes");

    return 0;
}

int world_profiler_record_throughput(ProfilerContext* profiler,
                                    const char* block_name,
                                    uint64_t samples_processed,
                                    double processing_time_ms) {
    if (!profiler || !block_name || processing_time_ms <= 0.0) {
        return -1;
    }

    if (!profiler->is_active || profiler->is_paused) {
        return 0;
    }

    BlockPerformanceStats* stats = find_or_create_block_stats(profiler, block_name, -1);
    if (!stats) {
        return -1;
    }

    stats->total_samples_processed += samples_processed;
    stats->samples_per_second = samples_processed / (processing_time_ms / 1000.0);

    // 성능 메트릭 추가
    add_performance_metric(profiler, PERF_METRIC_THROUGHPUT,
                          block_name, stats->samples_per_second, "samples/sec");

    return 0;
}

int world_profiler_add_custom_metric(ProfilerContext* profiler,
                                    const char* metric_name,
                                    double value, const char* unit) {
    if (!profiler || !metric_name) {
        return -1;
    }

    return add_performance_metric(profiler, PERF_METRIC_EXECUTION_TIME,
                                 metric_name, value, unit);
}

int world_profiler_analyze_bottlenecks(ProfilerContext* profiler) {
    if (!profiler) {
        WORLD_LOG_ERROR(PROFILER, "잘못된 매개변수");
        return -1;
    }

    profiler->bottleneck_count = 0;

    // 총 실행 시간 계산
    double total_execution_time = 0.0;
    for (uint32_t i = 0; i < profiler->block_count; i++) {
        total_execution_time += profiler->block_stats[i].total_execution_time_ms;
    }

    if (total_execution_time <= 0.0) {
        WORLD_LOG_WARNING(PROFILER, "총 실행 시간이 0입니다");
        return 0;
    }

    // 각 블록의 병목 점수 계산
    for (uint32_t i = 0; i < profiler->block_count; i++) {
        const BlockPerformanceStats* stats = &profiler->block_stats[i];

        if (profiler->bottleneck_count >= profiler->config.max_blocks) {
            break;
        }

        BottleneckInfo* bottleneck = &profiler->bottlenecks[profiler->bottleneck_count];

        strncpy(bottleneck->block_name, stats->block_name, sizeof(bottleneck->block_name) - 1);
        bottleneck->block_name[sizeof(bottleneck->block_name) - 1] = '\0';

        bottleneck->execution_time_ratio = stats->total_execution_time_ms / total_execution_time;
        bottleneck->memory_usage_ratio = stats->peak_memory_usage;

        // 병목 점수 계산 (실행 시간 비율과 메모리 사용량을 고려)
        bottleneck->bottleneck_score = bottleneck->execution_time_ratio * 0.7 +
                                      (stats->peak_memory_usage / 1024.0 / 1024.0) * 0.3;

        // 최적화 권장사항 생성
        if (bottleneck->execution_time_ratio > 0.3) {
            snprintf(bottleneck->recommendation, sizeof(bottleneck->recommendation),
                    "실행 시간이 전체의 %.1f%%를 차지합니다. 알고리즘 최적화를 고려하세요.",
                    bottleneck->execution_time_ratio * 100.0);
        } else if (stats->peak_memory_usage > 10 * 1024 * 1024) { // 10MB 이상
            snprintf(bottleneck->recommendation, sizeof(bottleneck->recommendation),
                    "메모리 사용량이 %.1fMB입니다. 메모리 최적화를 고려하세요.",
                    stats->peak_memory_usage / 1024.0 / 1024.0);
        } else {
            snprintf(bottleneck->recommendation, sizeof(bottleneck->recommendation),
                    "성능이 양호합니다.");
        }

        profiler->bottleneck_count++;
    }

    WORLD_LOG_INFO(PROFILER, "병목 지점 분석 완료 (%u개 블록 분석)", profiler->bottleneck_count);
    return 0;
}

int world_profiler_generate_optimization_recommendations(ProfilerContext* profiler,
                                                        const char* output_path) {
    if (!profiler || !output_path) {
        WORLD_LOG_ERROR(PROFILER, "잘못된 매개변수");
        return -1;
    }

    // 병목 지점 분석 먼저 수행
    world_profiler_analyze_bottlenecks(profiler);

    FILE* report_file = fopen(output_path, "w");
    if (!report_file) {
        WORLD_LOG_ERROR(PROFILER, "최적화 권장사항 파일 열기 실패: %s", output_path);
        return -1;
    }

    fprintf(report_file, "# WORLD 성능 최적화 권장사항\n");
    fprintf(report_file, "생성 시간: %lu\n\n", get_current_time_us());

    fprintf(report_file, "## 병목 지점 분석\n");
    for (uint32_t i = 0; i < profiler->bottleneck_count; i++) {
        const BottleneckInfo* bottleneck = &profiler->bottlenecks[i];
        fprintf(report_file, "### %s\n", bottleneck->block_name);
        fprintf(report_file, "- 병목 점수: %.3f\n", bottleneck->bottleneck_score);
        fprintf(report_file, "- 실행 시간 비율: %.1f%%\n", bottleneck->execution_time_ratio * 100.0);
        fprintf(report_file, "- 권장사항: %s\n\n", bottleneck->recommendation);
    }

    fprintf(report_file, "## 일반적인 최적화 권장사항\n");
    fprintf(report_file, "1. SIMD 최적화 활용\n");
    fprintf(report_file, "2. 메모리 풀 사용으로 할당/해제 오버헤드 감소\n");
    fprintf(report_file, "3. 캐시 친화적인 데이터 구조 사용\n");
    fprintf(report_file, "4. 불필요한 복사 연산 제거\n");
    fprintf(report_file, "5. 병렬 처리 가능한 부분 식별 및 멀티스레딩 적용\n");

    fclose(report_file);

    WORLD_LOG_INFO(PROFILER, "최적화 권장사항 생성 완료: %s", output_path);
    return 0;
}

int world_profiler_generate_report(ProfilerContext* profiler,
                                  const char* output_path, const char* format) {
    if (!profiler || !output_path || !format) {
        WORLD_LOG_ERROR(PROFILER, "잘못된 매개변수");
        return -1;
    }

    FILE* report_file = fopen(output_path, "w");
    if (!report_file) {
        WORLD_LOG_ERROR(PROFILER, "성능 보고서 파일 열기 실패: %s", output_path);
        return -1;
    }

    if (strcmp(format, "json") == 0) {
        // JSON 형식 보고서
        fprintf(report_file, "{\n");
        fprintf(report_file, "  \"profiling_info\": {\n");
        fprintf(report_file, "    \"start_time\": %lu,\n", profiler->profiling_start_time);
        fprintf(report_file, "    \"duration_us\": %lu,\n", profiler->profiling_duration);
        fprintf(report_file, "    \"block_count\": %u,\n", profiler->block_count);
        fprintf(report_file, "    \"metric_count\": %u\n", profiler->metric_count);
        fprintf(report_file, "  },\n");

        fprintf(report_file, "  \"block_stats\": [\n");
        for (uint32_t i = 0; i < profiler->block_count; i++) {
            const BlockPerformanceStats* stats = &profiler->block_stats[i];
            fprintf(report_file, "    {\n");
            fprintf(report_file, "      \"name\": \"%s\",\n", stats->block_name);
            fprintf(report_file, "      \"execution_count\": %lu,\n", stats->execution_count);
            fprintf(report_file, "      \"total_time_ms\": %.3f,\n", stats->total_execution_time_ms);
            fprintf(report_file, "      \"avg_time_ms\": %.3f,\n", stats->avg_execution_time_ms);
            fprintf(report_file, "      \"peak_memory_bytes\": %zu\n", stats->peak_memory_usage);
            fprintf(report_file, "    }%s\n", (i < profiler->block_count - 1) ? "," : "");
        }
        fprintf(report_file, "  ]\n");
        fprintf(report_file, "}\n");
    } else {
        // 기본 텍스트 형식
        world_profiler_print_summary(profiler, report_file);
    }

    fclose(report_file);

    WORLD_LOG_INFO(PROFILER, "성능 보고서 생성 완료: %s (%s 형식)", output_path, format);
    return 0;
}

const BlockPerformanceStats* world_profiler_get_block_stats(const ProfilerContext* profiler,
                                                           const char* block_name) {
    if (!profiler || !block_name) {
        return NULL;
    }

    for (uint32_t i = 0; i < profiler->block_count; i++) {
        if (strcmp(profiler->block_stats[i].block_name, block_name) == 0) {
            return &profiler->block_stats[i];
        }
    }

    return NULL;
}

void world_profiler_print_summary(const ProfilerContext* profiler, FILE* output_file) {
    if (!profiler) {
        return;
    }

    FILE* out = output_file ? output_file : stdout;

    fprintf(out, "\n=== WORLD 성능 프로파일링 요약 ===\n");
    fprintf(out, "프로파일링 지속 시간: %lu μs (%.2f ms)\n",
           profiler->profiling_duration, profiler->profiling_duration / 1000.0);
    fprintf(out, "총 블록 수: %u\n", profiler->block_count);
    fprintf(out, "총 메트릭 수: %u\n\n", profiler->metric_count);

    fprintf(out, "블록별 성능 통계:\n");
    fprintf(out, "%-20s %-10s %-15s %-15s %-15s %-15s\n",
           "블록 이름", "실행 횟수", "총 시간(ms)", "평균 시간(ms)", "최대 메모리(B)", "처리량(sps)");
    fprintf(out, "----------------------------------------------------------------------------------------\n");

    for (uint32_t i = 0; i < profiler->block_count; i++) {
        const BlockPerformanceStats* stats = &profiler->block_stats[i];
        fprintf(out, "%-20s %-10lu %-15.3f %-15.3f %-15zu %-15.0f\n",
               stats->block_name, stats->execution_count,
               stats->total_execution_time_ms, stats->avg_execution_time_ms,
               stats->peak_memory_usage, stats->samples_per_second);
    }
    fprintf(out, "\n");
}

void world_profiler_print_bottlenecks(const ProfilerContext* profiler, FILE* output_file) {
    if (!profiler) {
        return;
    }

    FILE* out = output_file ? output_file : stdout;

    fprintf(out, "\n=== 병목 지점 분석 ===\n");
    for (uint32_t i = 0; i < profiler->bottleneck_count; i++) {
        const BottleneckInfo* bottleneck = &profiler->bottlenecks[i];
        fprintf(out, "%s (점수: %.3f)\n", bottleneck->block_name, bottleneck->bottleneck_score);
        fprintf(out, "  실행 시간 비율: %.1f%%\n", bottleneck->execution_time_ratio * 100.0);
        fprintf(out, "  권장사항: %s\n\n", bottleneck->recommendation);
    }
}

void world_profiler_reset_stats(ProfilerContext* profiler) {
    if (!profiler) {
        return;
    }

    profiler->metric_count = 0;
    profiler->block_count = 0;
    profiler->bottleneck_count = 0;

    memset(profiler->metrics, 0, sizeof(PerformanceMetric) * profiler->config.max_samples);
    memset(profiler->block_stats, 0, sizeof(BlockPerformanceStats) * profiler->config.max_blocks);
    memset(profiler->bottlenecks, 0, sizeof(BottleneckInfo) * profiler->config.max_blocks);

    WORLD_LOG_INFO(PROFILER, "프로파일러 통계 초기화 완료");
}

void world_profiler_destroy(ProfilerContext* profiler) {
    if (!profiler) {
        return;
    }

    if (profiler->log_file) {
        fclose(profiler->log_file);
    }

    free(profiler->metrics);
    free(profiler->block_stats);
    free(profiler->bottlenecks);
    free(profiler);

    WORLD_LOG_INFO(PROFILER, "성능 프로파일러 해제 완료");
}