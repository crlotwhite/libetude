/**
 * @file world_performance_monitor.c
 * @brief WORLD 파이프라인 성능 모니터링 구현
 */

#include "world_performance_monitor.h"
#include "world_error.h"
#include <libetude/api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

// =============================================================================
// 내부 헬퍼 함수 선언
// =============================================================================

static double get_current_time(void);
static ETResult world_perf_calculate_stats(WorldPerfMeasurement* measurements,
                                          int count, WorldPerfStats* stats);
static ETResult world_perf_update_stage_stats(WorldPerfMonitor* monitor, WorldPerfStage stage);
static const char* world_perf_get_stage_name_internal(WorldPerfStage stage);
static ETResult world_perf_write_csv_header(FILE* file);
static ETResult world_perf_write_csv_data(FILE* file, WorldPerfMonitor* monitor);

// =============================================================================
// 단계 이름 매핑
// =============================================================================

static const char* stage_names[WORLD_PERF_STAGE_COUNT] = {
    "Initialization",
    "Parameter Parsing",
    "Audio Input",
    "F0 Extraction",
    "Spectrum Analysis",
    "Aperiodicity Analysis",
    "Parameter Mapping",
    "Synthesis",
    "Audio Output",
    "Cleanup",
    "Total"
};

static const char* metric_names[WORLD_PERF_METRIC_COUNT] = {
    "Time",
    "Memory",
    "CPU",
    "Throughput",
    "Latency",
    "Quality"
};

// =============================================================================
// 기본 설정 및 생성 함수들
// =============================================================================

WorldPerfMonitorConfig world_perf_monitor_config_default(void) {
    WorldPerfMonitorConfig config = {0};

    // 모니터링 활성화 플래그
    config.enable_time_monitoring = true;
    config.enable_memory_monitoring = true;
    config.enable_cpu_monitoring = false; // CPU 모니터링은 기본적으로 비활성화
    config.enable_quality_monitoring = false;
    config.enable_realtime_monitoring = false;

    // 샘플링 설정
    config.sampling_interval_ms = 100;
    config.max_samples_per_stage = 1000;
    config.enable_statistical_analysis = true;

    // 출력 설정
    config.enable_console_output = false;
    config.enable_file_output = false;
    strcpy(config.output_file_path, "./performance.log");
    config.enable_csv_export = false;

    // 알림 설정
    config.performance_threshold = 0.1; // 100ms
    config.memory_threshold = 100 * 1024 * 1024; // 100MB
    config.enable_alerts = false;

    // 히스토리 설정
    config.history_buffer_size = 1000;
    config.enable_trend_analysis = false;

    return config;
}

WorldPerfMonitor* world_perf_monitor_create(const WorldPerfMonitorConfig* config) {
    if (!config) {
        WorldPerfMonitorConfig default_config = world_perf_monitor_config_default();
        config = &default_config;
    }

    // 설정 검증
    if (!world_perf_monitor_config_validate(config)) {
        return NULL;
    }

    // 모니터 할당
    WorldPerfMonitor* monitor = (WorldPerfMonitor*)calloc(1, sizeof(WorldPerfMonitor));
    if (!monitor) {
        return NULL;
    }

    // 설정 복사
    monitor->config = *config;

    // 초기 상태 설정
    monitor->is_monitoring = false;
    monitor->is_paused = false;

    // 메모리 풀 생성
    size_t pool_size = config->history_buffer_size * WORLD_PERF_STAGE_COUNT *
                      sizeof(WorldPerfMeasurement) * 2;
    monitor->mem_pool = et_memory_pool_create(pool_size);
    if (!monitor->mem_pool) {
        world_perf_monitor_destroy(monitor);
        return NULL;
    }

    // 뮤텍스 생성
    monitor->mutex = malloc(sizeof(pthread_mutex_t));
    if (!monitor->mutex) {
        world_perf_monitor_destroy(monitor);
        return NULL;
    }
    pthread_mutex_init((pthread_mutex_t*)monitor->mutex, NULL);

    // libetude 프로파일러 생성
    monitor->profiler = et_profiler_create();
    if (!monitor->profiler) {
        world_perf_monitor_destroy(monitor);
        return NULL;
    }

    // 측정값 버퍼 할당
    monitor->measurement_buffers = (WorldPerfMeasurement**)calloc(WORLD_PERF_STAGE_COUNT,
                                                                 sizeof(WorldPerfMeasurement*));
    monitor->buffer_indices = (int*)calloc(WORLD_PERF_STAGE_COUNT, sizeof(int));

    if (!monitor->measurement_buffers || !monitor->buffer_indices) {
        world_perf_monitor_destroy(monitor);
        return NULL;
    }

    // 각 단계별 측정값 버퍼 할당
    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        monitor->measurement_buffers[i] = (WorldPerfMeasurement*)calloc(
            config->history_buffer_size, sizeof(WorldPerfMeasurement));
        if (!monitor->measurement_buffers[i]) {
            world_perf_monitor_destroy(monitor);
            return NULL;
        }
        monitor->buffer_indices[i] = 0;
    }

    // 성능 데이터 초기화
    memset(&monitor->performance, 0, sizeof(WorldPipelinePerformance));
    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        monitor->performance.stages[i].stage = (WorldPerfStage)i;
        monitor->performance.stages[i].stage_name = stage_names[i];
    }

    return monitor;
}

void world_perf_monitor_destroy(WorldPerfMonitor* monitor) {
    if (!monitor) return;

    // 모니터링 중지
    if (monitor->is_monitoring) {
        world_perf_monitor_stop(monitor);
    }

    // 출력 파일 닫기
    if (monitor->output_file) {
        fclose(monitor->output_file);
    }
    if (monitor->csv_file) {
        fclose(monitor->csv_file);
    }

    // 측정값 버퍼 해제
    if (monitor->measurement_buffers) {
        for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
            if (monitor->measurement_buffers[i]) {
                free(monitor->measurement_buffers[i]);
            }
        }
        free(monitor->measurement_buffers);
    }

    if (monitor->buffer_indices) {
        free(monitor->buffer_indices);
    }

    // libetude 프로파일러 해제
    if (monitor->profiler) {
        et_profiler_destroy(monitor->profiler);
    }

    // 뮤텍스 해제
    if (monitor->mutex) {
        pthread_mutex_destroy((pthread_mutex_t*)monitor->mutex);
        free(monitor->mutex);
    }

    // 메모리 풀 해제
    if (monitor->mem_pool) {
        et_memory_pool_destroy(monitor->mem_pool);
    }

    free(monitor);
}

ETResult world_perf_monitor_initialize(WorldPerfMonitor* monitor) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    // 성능 데이터 초기화
    memset(&monitor->performance, 0, sizeof(WorldPipelinePerformance));

    // 단계별 정보 초기화
    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        monitor->performance.stages[i].stage = (WorldPerfStage)i;
        monitor->performance.stages[i].stage_name = stage_names[i];
        monitor->buffer_indices[i] = 0;

        // 측정값 버퍼 초기화
        memset(monitor->measurement_buffers[i], 0,
               monitor->config.history_buffer_size * sizeof(WorldPerfMeasurement));
    }

    // 시간 정보 설정
    double current_time = get_current_time();
    monitor->performance.monitoring_start_time = current_time;
    monitor->performance.last_update_time = current_time;

    // 출력 파일 열기
    if (monitor->config.enable_file_output) {
        monitor->output_file = fopen(monitor->config.output_file_path, "w");
        if (monitor->output_file) {
            fprintf(monitor->output_file, "WORLD Performance Monitor Log\n");
            fprintf(monitor->output_file, "Started at: %.3f\n\n", current_time);
            fflush(monitor->output_file);
        }
    }

    if (monitor->config.enable_csv_export) {
        char csv_filename[512];
        snprintf(csv_filename, sizeof(csv_filename), "%s.csv", monitor->config.output_file_path);
        monitor->csv_file = fopen(csv_filename, "w");
        if (monitor->csv_file) {
            world_perf_write_csv_header(monitor->csv_file);
        }
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

void world_perf_monitor_cleanup(WorldPerfMonitor* monitor) {
    if (!monitor) return;

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    // 최종 통계 업데이트
    world_perf_monitor_update_stats(monitor);

    // 출력 파일에 최종 보고서 작성
    if (monitor->output_file) {
        fprintf(monitor->output_file, "\nFinal Performance Report\n");
        fprintf(monitor->output_file, "========================\n");
        fprintf(monitor->output_file, "Total Processing Time: %.6f seconds\n",
               monitor->performance.total_processing_time);
        fprintf(monitor->output_file, "Total Processed Samples: %llu\n",
               monitor->performance.total_processed_samples);
        fprintf(monitor->output_file, "Overall Throughput: %.2f samples/sec\n",
               monitor->performance.overall_throughput);
        fflush(monitor->output_file);
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);
}

// =============================================================================
// 모니터링 제어 함수들
// =============================================================================

ETResult world_perf_monitor_start(WorldPerfMonitor* monitor) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    if (monitor->is_monitoring) {
        return ET_SUCCESS; // 이미 시작됨
    }

    ETResult result = world_perf_monitor_initialize(monitor);
    if (result != ET_SUCCESS) {
        return result;
    }

    monitor->is_monitoring = true;
    monitor->is_paused = false;

    // libetude 프로파일러 시작
    et_profiler_start(monitor->profiler);

    return ET_SUCCESS;
}

ETResult world_perf_monitor_stop(WorldPerfMonitor* monitor) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    if (!monitor->is_monitoring) {
        return ET_SUCCESS; // 이미 중지됨
    }

    // libetude 프로파일러 중지
    et_profiler_stop(monitor->profiler);

    monitor->is_monitoring = false;
    monitor->is_paused = false;

    // 정리 작업
    world_perf_monitor_cleanup(monitor);

    return ET_SUCCESS;
}

ETResult world_perf_monitor_pause(WorldPerfMonitor* monitor) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    if (!monitor->is_monitoring) {
        return ET_ERROR_INVALID_STATE;
    }

    monitor->is_paused = true;
    return ET_SUCCESS;
}

ETResult world_perf_monitor_resume(WorldPerfMonitor* monitor) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    if (!monitor->is_monitoring) {
        return ET_ERROR_INVALID_STATE;
    }

    monitor->is_paused = false;
    return ET_SUCCESS;
}

ETResult world_perf_monitor_reset(WorldPerfMonitor* monitor) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    bool was_monitoring = monitor->is_monitoring;

    if (was_monitoring) {
        world_perf_monitor_stop(monitor);
    }

    // 모든 데이터 초기화
    memset(&monitor->performance, 0, sizeof(WorldPipelinePerformance));

    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        monitor->buffer_indices[i] = 0;
        memset(monitor->measurement_buffers[i], 0,
               monitor->config.history_buffer_size * sizeof(WorldPerfMeasurement));
    }

    if (was_monitoring) {
        return world_perf_monitor_start(monitor);
    }

    return ET_SUCCESS;
}

// =============================================================================
// 성능 측정 함수들
// =============================================================================

ETResult world_perf_monitor_stage_begin(WorldPerfMonitor* monitor, WorldPerfStage stage) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!monitor->is_monitoring || monitor->is_paused) {
        return ET_SUCCESS; // 모니터링 비활성화 시 무시
    }

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    // 시작 시간 기록
    double start_time = get_current_time();

    // libetude 프로파일러에도 기록
    char stage_name[64];
    snprintf(stage_name, sizeof(stage_name), "world_%s", stage_names[stage]);
    et_profiler_begin(monitor->profiler, stage_name);

    // 임시로 시작 시간을 저장 (실제로는 더 정교한 방법 필요)
    monitor->performance.stages[stage].last_execution_time = start_time;

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

ETResult world_perf_monitor_stage_end(WorldPerfMonitor* monitor, WorldPerfStage stage) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!monitor->is_monitoring || monitor->is_paused) {
        return ET_SUCCESS; // 모니터링 비활성화 시 무시
    }

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    double end_time = get_current_time();
    double start_time = monitor->performance.stages[stage].last_execution_time;
    double execution_time = end_time - start_time;

    // libetude 프로파일러 종료
    char stage_name[64];
    snprintf(stage_name, sizeof(stage_name), "world_%s", stage_names[stage]);
    et_profiler_end(monitor->profiler, stage_name);

    // 측정값 기록
    if (monitor->config.enable_time_monitoring) {
        int buffer_index = monitor->buffer_indices[stage];
        WorldPerfMeasurement* measurement = &monitor->measurement_buffers[stage][buffer_index];

        measurement->value = execution_time;
        measurement->timestamp = end_time;
        measurement->sample_count = 1;
        measurement->unit = "seconds";
        measurement->description = "Execution time";

        // 버퍼 인덱스 업데이트 (순환 버퍼)
        monitor->buffer_indices[stage] = (buffer_index + 1) % monitor->config.history_buffer_size;
    }

    // 단계별 통계 업데이트
    WorldStagePerformance* stage_perf = &monitor->performance.stages[stage];
    stage_perf->last_execution_time = execution_time;
    stage_perf->total_execution_time += execution_time;
    stage_perf->execution_count++;

    // 전체 통계 업데이트
    monitor->performance.total_processing_time += execution_time;
    monitor->performance.last_update_time = end_time;
    monitor->performance.monitoring_duration = end_time - monitor->performance.monitoring_start_time;

    // 통계 재계산
    world_perf_update_stage_stats(monitor, stage);

    // 실시간 출력
    if (monitor->config.enable_console_output) {
        printf("[PERF] %s: %.6f seconds\n", stage_names[stage], execution_time);
    }

    // 파일 출력
    if (monitor->output_file) {
        fprintf(monitor->output_file, "%.6f,%s,%.6f\n",
               end_time, stage_names[stage], execution_time);
        fflush(monitor->output_file);
    }

    // CSV 출력
    if (monitor->csv_file) {
        fprintf(monitor->csv_file, "%.6f,%d,%s,%.6f,%.0f,%.2f\n",
               end_time, stage, stage_names[stage], execution_time,
               (double)stage_perf->current_memory_usage, stage_perf->current_cpu_usage);
        fflush(monitor->csv_file);
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

ETResult world_perf_monitor_record_memory(WorldPerfMonitor* monitor,
                                         WorldPerfStage stage,
                                         size_t memory_usage) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!monitor->is_monitoring || monitor->is_paused || !monitor->config.enable_memory_monitoring) {
        return ET_SUCCESS;
    }

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    WorldStagePerformance* stage_perf = &monitor->performance.stages[stage];
    stage_perf->current_memory_usage = memory_usage;

    if (memory_usage > stage_perf->peak_memory_usage) {
        stage_perf->peak_memory_usage = memory_usage;
    }

    stage_perf->total_memory_allocated += memory_usage;

    // 전체 메모리 통계 업데이트
    monitor->performance.current_total_memory += memory_usage;
    if (monitor->performance.current_total_memory > monitor->performance.peak_total_memory) {
        monitor->performance.peak_total_memory = monitor->performance.current_total_memory;
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

ETResult world_perf_monitor_record_cpu(WorldPerfMonitor* monitor,
                                      WorldPerfStage stage,
                                      double cpu_usage) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!monitor->is_monitoring || monitor->is_paused || !monitor->config.enable_cpu_monitoring) {
        return ET_SUCCESS;
    }

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    WorldStagePerformance* stage_perf = &monitor->performance.stages[stage];
    stage_perf->current_cpu_usage = cpu_usage;

    if (cpu_usage > stage_perf->peak_cpu_usage) {
        stage_perf->peak_cpu_usage = cpu_usage;
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

ETResult world_perf_monitor_record_throughput(WorldPerfMonitor* monitor,
                                             WorldPerfStage stage,
                                             uint64_t samples_processed,
                                             double processing_time) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT || processing_time <= 0.0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!monitor->is_monitoring || monitor->is_paused) {
        return ET_SUCCESS;
    }

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    WorldStagePerformance* stage_perf = &monitor->performance.stages[stage];

    // 처리량 계산
    double throughput = samples_processed / processing_time;
    stage_perf->samples_per_second = throughput;

    // 실시간 배율 계산 (44.1kHz 기준)
    double expected_time = samples_processed / 44100.0;
    stage_perf->realtime_factor = expected_time / processing_time;

    // 전체 통계 업데이트
    monitor->performance.total_processed_samples += samples_processed;
    monitor->performance.total_processed_frames += samples_processed; // 단순화

    if (monitor->performance.total_processing_time > 0.0) {
        monitor->performance.overall_throughput =
            monitor->performance.total_processed_samples / monitor->performance.total_processing_time;
        monitor->performance.realtime_performance =
            (monitor->performance.total_processed_samples / 44100.0) / monitor->performance.total_processing_time;
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

ETResult world_perf_monitor_record_quality(WorldPerfMonitor* monitor,
                                          WorldPerfStage stage,
                                          double quality_score) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!monitor->is_monitoring || monitor->is_paused || !monitor->config.enable_quality_monitoring) {
        return ET_SUCCESS;
    }

    if (quality_score < 0.0 || quality_score > 1.0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    // 품질 점수 기록 (간단한 이동 평균)
    static double quality_sum = 0.0;
    static int quality_count = 0;

    quality_sum += quality_score;
    quality_count++;

    monitor->performance.average_quality_score = quality_sum / quality_count;

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

// =============================================================================
// 성능 데이터 조회 함수들
// =============================================================================

const WorldPipelinePerformance* world_perf_monitor_get_performance(WorldPerfMonitor* monitor) {
    if (!monitor) return NULL;

    world_perf_monitor_update_stats(monitor);
    return &monitor->performance;
}

const WorldStagePerformance* world_perf_monitor_get_stage_performance(WorldPerfMonitor* monitor,
                                                                     WorldPerfStage stage) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT) return NULL;

    return &monitor->performance.stages[stage];
}

ETResult world_perf_monitor_get_realtime_metrics(WorldPerfMonitor* monitor,
                                                double* realtime_factor,
                                                double* current_latency,
                                                double* throughput) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    if (realtime_factor) {
        *realtime_factor = monitor->performance.realtime_performance;
    }

    if (current_latency) {
        // 현재 지연 시간 추정 (마지막 처리 시간 기반)
        *current_latency = monitor->performance.stages[WORLD_PERF_STAGE_TOTAL].last_execution_time * 1000.0; // ms
    }

    if (throughput) {
        *throughput = monitor->performance.overall_throughput;
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

// =============================================================================
// 내부 헬퍼 함수 구현
// =============================================================================

static double get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

static ETResult world_perf_calculate_stats(WorldPerfMeasurement* measurements,
                                          int count, WorldPerfStats* stats) {
    if (!measurements || count <= 0 || !stats) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(stats, 0, sizeof(WorldPerfStats));

    if (count == 0) return ET_SUCCESS;

    // 기본 통계 계산
    double sum = 0.0;
    double min_val = measurements[0].value;
    double max_val = measurements[0].value;

    for (int i = 0; i < count; i++) {
        double val = measurements[i].value;
        sum += val;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    stats->min_value = min_val;
    stats->max_value = max_val;
    stats->avg_value = sum / count;
    stats->total_value = sum;
    stats->sample_count = count;

    // 표준편차 계산
    double variance_sum = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = measurements[i].value - stats->avg_value;
        variance_sum += diff * diff;
    }
    stats->std_deviation = sqrt(variance_sum / count);

    // 중간값 계산 (간단한 근사)
    stats->median_value = stats->avg_value; // 실제로는 정렬 필요

    // 퍼센타일 계산 (간단한 근사)
    stats->percentile_95 = stats->avg_value + 1.645 * stats->std_deviation;
    stats->percentile_99 = stats->avg_value + 2.326 * stats->std_deviation;

    return ET_SUCCESS;
}

static ETResult world_perf_update_stage_stats(WorldPerfMonitor* monitor, WorldPerfStage stage) {
    if (!monitor || stage >= WORLD_PERF_STAGE_COUNT) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 측정값 버퍼에서 통계 계산
    int buffer_index = monitor->buffer_indices[stage];
    int sample_count = (buffer_index == 0) ? monitor->config.history_buffer_size : buffer_index;

    if (sample_count > 0) {
        world_perf_calculate_stats(monitor->measurement_buffers[stage],
                                  sample_count,
                                  &monitor->performance.stages[stage].time_stats);
    }

    return ET_SUCCESS;
}

static const char* world_perf_get_stage_name_internal(WorldPerfStage stage) {
    if (stage >= WORLD_PERF_STAGE_COUNT) return "Unknown";
    return stage_names[stage];
}

static ETResult world_perf_write_csv_header(FILE* file) {
    if (!file) return ET_ERROR_INVALID_PARAMETER;

    fprintf(file, "Timestamp,Stage_ID,Stage_Name,Execution_Time,Memory_Usage,CPU_Usage\n");
    return ET_SUCCESS;
}

static ETResult world_perf_write_csv_data(FILE* file, WorldPerfMonitor* monitor) {
    if (!file || !monitor) return ET_ERROR_INVALID_PARAMETER;

    double current_time = get_current_time();

    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        WorldStagePerformance* stage = &monitor->performance.stages[i];
        fprintf(file, "%.6f,%d,%s,%.6f,%zu,%.2f\n",
               current_time, i, stage->stage_name,
               stage->last_execution_time,
               stage->current_memory_usage,
               stage->current_cpu_usage);
    }

    return ET_SUCCESS;
}

// =============================================================================
// 통계 분석 함수들
// =============================================================================

ETResult world_perf_monitor_update_stats(WorldPerfMonitor* monitor) {
    if (!monitor) return ET_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock((pthread_mutex_t*)monitor->mutex);

    // 모든 단계의 통계 업데이트
    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        world_perf_update_stage_stats(monitor, (WorldPerfStage)i);
    }

    // 전체 성능 지표 계산
    if (monitor->performance.monitoring_duration > 0.0) {
        monitor->performance.average_processing_time =
            monitor->performance.total_processing_time / monitor->performance.monitoring_duration;

        monitor->performance.efficiency_ratio =
            monitor->performance.realtime_performance;
    }

    pthread_mutex_unlock((pthread_mutex_t*)monitor->mutex);

    return ET_SUCCESS;
}

// =============================================================================
// 출력 및 보고 함수들
// =============================================================================

ETResult world_perf_monitor_generate_report(WorldPerfMonitor* monitor, const char* filename) {
    if (!monitor || !filename) return ET_ERROR_INVALID_PARAMETER;

    FILE* file = fopen(filename, "w");
    if (!file) return ET_ERROR_FILE_IO;

    world_perf_monitor_update_stats(monitor);

    fprintf(file, "WORLD Performance Monitor Report\n");
    fprintf(file, "================================\n\n");

    fprintf(file, "Monitoring Period: %.3f seconds\n", monitor->performance.monitoring_duration);
    fprintf(file, "Total Processing Time: %.6f seconds\n", monitor->performance.total_processing_time);
    fprintf(file, "Total Processed Samples: %llu\n", monitor->performance.total_processed_samples);
    fprintf(file, "Overall Throughput: %.2f samples/sec\n", monitor->performance.overall_throughput);
    fprintf(file, "Realtime Performance: %.2fx\n", monitor->performance.realtime_performance);
    fprintf(file, "Peak Memory Usage: %.2f MB\n", monitor->performance.peak_total_memory / (1024.0 * 1024.0));

    fprintf(file, "\nStage Performance:\n");
    fprintf(file, "==================\n");

    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        WorldStagePerformance* stage = &monitor->performance.stages[i];
        if (stage->execution_count > 0) {
            fprintf(file, "\n%s:\n", stage->stage_name);
            fprintf(file, "  Executions: %llu\n", stage->execution_count);
            fprintf(file, "  Total Time: %.6f seconds\n", stage->total_execution_time);
            fprintf(file, "  Average Time: %.6f seconds\n",
                   stage->total_execution_time / stage->execution_count);
            fprintf(file, "  Min Time: %.6f seconds\n", stage->time_stats.min_value);
            fprintf(file, "  Max Time: %.6f seconds\n", stage->time_stats.max_value);
            fprintf(file, "  Peak Memory: %.2f MB\n", stage->peak_memory_usage / (1024.0 * 1024.0));
            fprintf(file, "  Peak CPU: %.1f%%\n", stage->peak_cpu_usage * 100.0);
        }
    }

    fclose(file);
    return ET_SUCCESS;
}

void world_perf_monitor_print_realtime(WorldPerfMonitor* monitor) {
    if (!monitor) {
        printf("Performance Monitor: NULL\n");
        return;
    }

    double realtime_factor, current_latency, throughput;
    world_perf_monitor_get_realtime_metrics(monitor, &realtime_factor, &current_latency, &throughput);

    printf("\rRealtime: %.2fx | Latency: %.1fms | Throughput: %.0f sps",
           realtime_factor, current_latency, throughput);
    fflush(stdout);
}

void world_perf_monitor_print_summary(WorldPerfMonitor* monitor) {
    if (!monitor) {
        printf("Performance Monitor: NULL\n");
        return;
    }

    world_perf_monitor_update_stats(monitor);

    printf("WORLD Performance Summary\n");
    printf("========================\n");
    printf("Total Processing Time: %.6f seconds\n", monitor->performance.total_processing_time);
    printf("Processed Samples: %llu\n", monitor->performance.total_processed_samples);
    printf("Overall Throughput: %.2f samples/sec\n", monitor->performance.overall_throughput);
    printf("Realtime Performance: %.2fx\n", monitor->performance.realtime_performance);
    printf("Peak Memory Usage: %.2f MB\n", monitor->performance.peak_total_memory / (1024.0 * 1024.0));
    printf("Average Quality Score: %.3f\n", monitor->performance.average_quality_score);
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

const char* world_perf_stage_get_name(WorldPerfStage stage) {
    if (stage >= WORLD_PERF_STAGE_COUNT) return "Unknown";
    return stage_names[stage];
}

const char* world_perf_metric_get_name(WorldPerfMetricType metric_type) {
    if (metric_type >= WORLD_PERF_METRIC_COUNT) return "Unknown";
    return metric_names[metric_type];
}

double world_perf_monitor_calculate_score(WorldPerfMonitor* monitor) {
    if (!monitor) return 0.0;

    world_perf_monitor_update_stats(monitor);

    // 간단한 성능 점수 계산 (0.0-1.0)
    double realtime_score = fmin(1.0, monitor->performance.realtime_performance);
    double efficiency_score = fmin(1.0, monitor->performance.efficiency_ratio);
    double quality_score = monitor->performance.average_quality_score;

    // 가중 평균
    double total_score = (realtime_score * 0.4 + efficiency_score * 0.3 + quality_score * 0.3);

    return fmax(0.0, fmin(1.0, total_score));
}

bool world_perf_monitor_config_validate(const WorldPerfMonitorConfig* config) {
    if (!config) return false;

    if (config->sampling_interval_ms <= 0 || config->sampling_interval_ms > 10000) return false;
    if (config->max_samples_per_stage <= 0 || config->max_samples_per_stage > 100000) return false;
    if (config->performance_threshold < 0.0 || config->performance_threshold > 10.0) return false;
    if (config->memory_threshold <= 0) return false;
    if (config->history_buffer_size <= 0 || config->history_buffer_size > 100000) return false;

    return true;
}