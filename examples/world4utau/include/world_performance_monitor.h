/**
 * @file world_performance_monitor.h
 * @brief WORLD 파이프라인 성능 모니터링 인터페이스
 *
 * 파이프라인 전체의 성능 프로파일링과 각 처리 단계별 시간 및 메모리 사용량 측정을 제공합니다.
 */

#ifndef WORLD4UTAU_WORLD_PERFORMANCE_MONITOR_H
#define WORLD4UTAU_WORLD_PERFORMANCE_MONITOR_H

#include <libetude/types.h>
#include <libetude/profiler.h>
#include <libetude/memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 타입 정의
// =============================================================================

/**
 * @brief 성능 측정 단계
 */
typedef enum {
    WORLD_PERF_STAGE_INITIALIZATION,    /**< 초기화 단계 */
    WORLD_PERF_STAGE_PARAMETER_PARSING,  /**< 파라미터 파싱 단계 */
    WORLD_PERF_STAGE_AUDIO_INPUT,       /**< 오디오 입력 단계 */
    WORLD_PERF_STAGE_F0_EXTRACTION,     /**< F0 추출 단계 */
    WORLD_PERF_STAGE_SPECTRUM_ANALYSIS,  /**< 스펙트럼 분석 단계 */
    WORLD_PERF_STAGE_APERIODICITY_ANALYSIS, /**< 비주기성 분석 단계 */
    WORLD_PERF_STAGE_PARAMETER_MAPPING,  /**< 파라미터 매핑 단계 */
    WORLD_PERF_STAGE_SYNTHESIS,         /**< 음성 합성 단계 */
    WORLD_PERF_STAGE_AUDIO_OUTPUT,      /**< 오디오 출력 단계 */
    WORLD_PERF_STAGE_CLEANUP,           /**< 정리 단계 */
    WORLD_PERF_STAGE_TOTAL,             /**< 전체 처리 */
    WORLD_PERF_STAGE_COUNT              /**< 단계 수 */
} WorldPerfStage;

/**
 * @brief 성능 메트릭 타입
 */
typedef enum {
    WORLD_PERF_METRIC_TIME,             /**< 시간 메트릭 */
    WORLD_PERF_METRIC_MEMORY,           /**< 메모리 메트릭 */
    WORLD_PERF_METRIC_CPU,              /**< CPU 사용률 메트릭 */
    WORLD_PERF_METRIC_THROUGHPUT,       /**< 처리량 메트릭 */
    WORLD_PERF_METRIC_LATENCY,          /**< 지연 시간 메트릭 */
    WORLD_PERF_METRIC_QUALITY,          /**< 품질 메트릭 */
    WORLD_PERF_METRIC_COUNT             /**< 메트릭 수 */
} WorldPerfMetricType;

/**
 * @brief 단일 성능 측정값
 */
typedef struct {
    double value;                       /**< 측정값 */
    double timestamp;                   /**< 측정 시간 */
    uint64_t sample_count;              /**< 샘플 수 */
    const char* unit;                   /**< 단위 */
    const char* description;            /**< 설명 */
} WorldPerfMeasurement;

/**
 * @brief 성능 통계
 */
typedef struct {
    double min_value;                   /**< 최소값 */
    double max_value;                   /**< 최대값 */
    double avg_value;                   /**< 평균값 */
    double std_deviation;               /**< 표준편차 */
    double median_value;                /**< 중간값 */
    double percentile_95;               /**< 95 퍼센타일 */
    double percentile_99;               /**< 99 퍼센타일 */
    uint64_t sample_count;              /**< 샘플 수 */
    double total_value;                 /**< 총합 */
} WorldPerfStats;

/**
 * @brief 단계별 성능 정보
 */
typedef struct {
    WorldPerfStage stage;               /**< 처리 단계 */
    const char* stage_name;             /**< 단계 이름 */

    // 시간 메트릭
    WorldPerfStats time_stats;          /**< 시간 통계 */
    double last_execution_time;         /**< 마지막 실행 시간 */
    double total_execution_time;        /**< 총 실행 시간 */
    uint64_t execution_count;           /**< 실행 횟수 */

    // 메모리 메트릭
    WorldPerfStats memory_stats;        /**< 메모리 통계 */
    size_t current_memory_usage;        /**< 현재 메모리 사용량 */
    size_t peak_memory_usage;           /**< 최대 메모리 사용량 */
    size_t total_memory_allocated;      /**< 총 할당된 메모리 */

    // CPU 메트릭
    WorldPerfStats cpu_stats;           /**< CPU 통계 */
    double current_cpu_usage;           /**< 현재 CPU 사용률 */
    double peak_cpu_usage;              /**< 최대 CPU 사용률 */

    // 처리량 메트릭
    double frames_per_second;           /**< 초당 프레임 수 */
    double samples_per_second;          /**< 초당 샘플 수 */
    double realtime_factor;             /**< 실시간 배율 */

    // 오류 메트릭
    uint64_t error_count;               /**< 오류 횟수 */
    uint64_t warning_count;             /**< 경고 횟수 */
    double error_rate;                  /**< 오류율 */
} WorldStagePerformance;

/**
 * @brief 전체 파이프라인 성능 정보
 */
typedef struct {
    // 전체 통계
    double total_processing_time;       /**< 총 처리 시간 */
    double average_processing_time;     /**< 평균 처리 시간 */
    uint64_t total_processed_samples;   /**< 총 처리된 샘플 수 */
    uint64_t total_processed_frames;    /**< 총 처리된 프레임 수 */

    // 메모리 통계
    size_t current_total_memory;        /**< 현재 총 메모리 사용량 */
    size_t peak_total_memory;           /**< 최대 총 메모리 사용량 */
    size_t memory_pool_usage;           /**< 메모리 풀 사용량 */
    double memory_fragmentation;        /**< 메모리 단편화율 */

    // 처리량 통계
    double overall_throughput;          /**< 전체 처리량 */
    double realtime_performance;        /**< 실시간 성능 */
    double efficiency_ratio;            /**< 효율성 비율 */

    // 품질 통계
    double average_quality_score;       /**< 평균 품질 점수 */
    double quality_consistency;         /**< 품질 일관성 */

    // 단계별 성능
    WorldStagePerformance stages[WORLD_PERF_STAGE_COUNT]; /**< 단계별 성능 */

    // 시간 정보
    double monitoring_start_time;       /**< 모니터링 시작 시간 */
    double last_update_time;            /**< 마지막 업데이트 시간 */
    double monitoring_duration;         /**< 모니터링 지속 시간 */
} WorldPipelinePerformance;

/**
 * @brief 성능 모니터 설정
 */
typedef struct {
    // 모니터링 활성화 플래그
    bool enable_time_monitoring;        /**< 시간 모니터링 활성화 */
    bool enable_memory_monitoring;      /**< 메모리 모니터링 활성화 */
    bool enable_cpu_monitoring;         /**< CPU 모니터링 활성화 */
    bool enable_quality_monitoring;     /**< 품질 모니터링 활성화 */
    bool enable_realtime_monitoring;    /**< 실시간 모니터링 활성화 */

    // 샘플링 설정
    int sampling_interval_ms;           /**< 샘플링 간격 (ms) */
    int max_samples_per_stage;          /**< 단계별 최대 샘플 수 */
    bool enable_statistical_analysis;   /**< 통계 분석 활성화 */

    // 출력 설정
    bool enable_console_output;         /**< 콘솔 출력 활성화 */
    bool enable_file_output;            /**< 파일 출력 활성화 */
    char output_file_path[256];         /**< 출력 파일 경로 */
    bool enable_csv_export;             /**< CSV 내보내기 활성화 */

    // 알림 설정
    double performance_threshold;       /**< 성능 임계값 */
    double memory_threshold;            /**< 메모리 임계값 */
    bool enable_alerts;                 /**< 알림 활성화 */

    // 히스토리 설정
    int history_buffer_size;            /**< 히스토리 버퍼 크기 */
    bool enable_trend_analysis;         /**< 트렌드 분석 활성화 */
} WorldPerfMonitorConfig;

/**
 * @brief 성능 모니터 컨텍스트
 */
typedef struct {
    // 설정
    WorldPerfMonitorConfig config;      /**< 모니터 설정 */

    // 성능 데이터
    WorldPipelinePerformance performance; /**< 파이프라인 성능 */

    // 측정 데이터 버퍼
    WorldPerfMeasurement** measurement_buffers; /**< 측정값 버퍼 배열 */
    int* buffer_indices;                /**< 버퍼 인덱스 배열 */

    // libetude 프로파일러 통합
    ETProfiler* profiler;               /**< libetude 프로파일러 */

    // 상태
    bool is_monitoring;                 /**< 모니터링 활성 상태 */
    bool is_paused;                     /**< 일시 정지 상태 */

    // 스레드 안전성
    void* mutex;                        /**< 뮤텍스 */

    // 메모리 관리
    ETMemoryPool* mem_pool;             /**< 메모리 풀 */

    // 출력 파일
    FILE* output_file;                  /**< 출력 파일 핸들 */
    FILE* csv_file;                     /**< CSV 파일 핸들 */
} WorldPerfMonitor;

// =============================================================================
// 성능 모니터 생성 및 관리
// =============================================================================

/**
 * @brief 기본 성능 모니터 설정 생성
 *
 * @return WorldPerfMonitorConfig 기본 설정
 */
WorldPerfMonitorConfig world_perf_monitor_config_default(void);

/**
 * @brief 성능 모니터 생성
 *
 * @param config 모니터 설정
 * @return WorldPerfMonitor* 생성된 모니터 (실패 시 NULL)
 */
WorldPerfMonitor* world_perf_monitor_create(const WorldPerfMonitorConfig* config);

/**
 * @brief 성능 모니터 해제
 *
 * @param monitor 해제할 모니터
 */
void world_perf_monitor_destroy(WorldPerfMonitor* monitor);

/**
 * @brief 성능 모니터 초기화
 *
 * @param monitor 성능 모니터
 * @return ETResult 초기화 결과
 */
ETResult world_perf_monitor_initialize(WorldPerfMonitor* monitor);

/**
 * @brief 성능 모니터 정리
 *
 * @param monitor 성능 모니터
 */
void world_perf_monitor_cleanup(WorldPerfMonitor* monitor);

// =============================================================================
// 모니터링 제어
// =============================================================================

/**
 * @brief 모니터링 시작
 *
 * @param monitor 성능 모니터
 * @return ETResult 시작 결과
 */
ETResult world_perf_monitor_start(WorldPerfMonitor* monitor);

/**
 * @brief 모니터링 중지
 *
 * @param monitor 성능 모니터
 * @return ETResult 중지 결과
 */
ETResult world_perf_monitor_stop(WorldPerfMonitor* monitor);

/**
 * @brief 모니터링 일시 정지
 *
 * @param monitor 성능 모니터
 * @return ETResult 일시 정지 결과
 */
ETResult world_perf_monitor_pause(WorldPerfMonitor* monitor);

/**
 * @brief 모니터링 재개
 *
 * @param monitor 성능 모니터
 * @return ETResult 재개 결과
 */
ETResult world_perf_monitor_resume(WorldPerfMonitor* monitor);

/**
 * @brief 모니터링 데이터 초기화
 *
 * @param monitor 성능 모니터
 * @return ETResult 초기화 결과
 */
ETResult world_perf_monitor_reset(WorldPerfMonitor* monitor);

// =============================================================================
// 성능 측정
// =============================================================================

/**
 * @brief 단계 시작 측정
 *
 * @param monitor 성능 모니터
 * @param stage 처리 단계
 * @return ETResult 측정 시작 결과
 */
ETResult world_perf_monitor_stage_begin(WorldPerfMonitor* monitor, WorldPerfStage stage);

/**
 * @brief 단계 종료 측정
 *
 * @param monitor 성능 모니터
 * @param stage 처리 단계
 * @return ETResult 측정 종료 결과
 */
ETResult world_perf_monitor_stage_end(WorldPerfMonitor* monitor, WorldPerfStage stage);

/**
 * @brief 메모리 사용량 측정
 *
 * @param monitor 성능 모니터
 * @param stage 처리 단계
 * @param memory_usage 메모리 사용량 (바이트)
 * @return ETResult 측정 결과
 */
ETResult world_perf_monitor_record_memory(WorldPerfMonitor* monitor,
                                         WorldPerfStage stage,
                                         size_t memory_usage);

/**
 * @brief CPU 사용률 측정
 *
 * @param monitor 성능 모니터
 * @param stage 처리 단계
 * @param cpu_usage CPU 사용률 (0.0-1.0)
 * @return ETResult 측정 결과
 */
ETResult world_perf_monitor_record_cpu(WorldPerfMonitor* monitor,
                                      WorldPerfStage stage,
                                      double cpu_usage);

/**
 * @brief 처리량 측정
 *
 * @param monitor 성능 모니터
 * @param stage 처리 단계
 * @param samples_processed 처리된 샘플 수
 * @param processing_time 처리 시간 (초)
 * @return ETResult 측정 결과
 */
ETResult world_perf_monitor_record_throughput(WorldPerfMonitor* monitor,
                                             WorldPerfStage stage,
                                             uint64_t samples_processed,
                                             double processing_time);

/**
 * @brief 품질 점수 측정
 *
 * @param monitor 성능 모니터
 * @param stage 처리 단계
 * @param quality_score 품질 점수 (0.0-1.0)
 * @return ETResult 측정 결과
 */
ETResult world_perf_monitor_record_quality(WorldPerfMonitor* monitor,
                                          WorldPerfStage stage,
                                          double quality_score);

// =============================================================================
// 성능 데이터 조회
// =============================================================================

/**
 * @brief 전체 파이프라인 성능 조회
 *
 * @param monitor 성능 모니터
 * @return const WorldPipelinePerformance* 성능 정보 (NULL 시 실패)
 */
const WorldPipelinePerformance* world_perf_monitor_get_performance(WorldPerfMonitor* monitor);

/**
 * @brief 특정 단계 성능 조회
 *
 * @param monitor 성능 모니터
 * @param stage 처리 단계
 * @return const WorldStagePerformance* 단계 성능 정보 (NULL 시 실패)
 */
const WorldStagePerformance* world_perf_monitor_get_stage_performance(WorldPerfMonitor* monitor,
                                                                     WorldPerfStage stage);

/**
 * @brief 실시간 성능 지표 조회
 *
 * @param monitor 성능 모니터
 * @param realtime_factor 실시간 배율 (출력)
 * @param current_latency 현재 지연 시간 (출력)
 * @param throughput 처리량 (출력)
 * @return ETResult 조회 결과
 */
ETResult world_perf_monitor_get_realtime_metrics(WorldPerfMonitor* monitor,
                                                double* realtime_factor,
                                                double* current_latency,
                                                double* throughput);

// =============================================================================
// 통계 분석
// =============================================================================

/**
 * @brief 성능 통계 업데이트
 *
 * @param monitor 성능 모니터
 * @return ETResult 업데이트 결과
 */
ETResult world_perf_monitor_update_stats(WorldPerfMonitor* monitor);

/**
 * @brief 트렌드 분석 수행
 *
 * @param monitor 성능 모니터
 * @param stage 분석할 단계
 * @param trend_slope 트렌드 기울기 (출력)
 * @param trend_confidence 트렌드 신뢰도 (출력)
 * @return ETResult 분석 결과
 */
ETResult world_perf_monitor_analyze_trend(WorldPerfMonitor* monitor,
                                         WorldPerfStage stage,
                                         double* trend_slope,
                                         double* trend_confidence);

/**
 * @brief 성능 병목 지점 식별
 *
 * @param monitor 성능 모니터
 * @param bottleneck_stage 병목 단계 (출력)
 * @param bottleneck_severity 병목 심각도 (출력)
 * @return ETResult 식별 결과
 */
ETResult world_perf_monitor_identify_bottlenecks(WorldPerfMonitor* monitor,
                                                WorldPerfStage* bottleneck_stage,
                                                double* bottleneck_severity);

// =============================================================================
// 출력 및 보고
// =============================================================================

/**
 * @brief 성능 보고서 생성
 *
 * @param monitor 성능 모니터
 * @param filename 출력 파일명
 * @return ETResult 생성 결과
 */
ETResult world_perf_monitor_generate_report(WorldPerfMonitor* monitor, const char* filename);

/**
 * @brief CSV 형식으로 데이터 내보내기
 *
 * @param monitor 성능 모니터
 * @param filename CSV 파일명
 * @return ETResult 내보내기 결과
 */
ETResult world_perf_monitor_export_csv(WorldPerfMonitor* monitor, const char* filename);

/**
 * @brief 실시간 성능 정보 출력
 *
 * @param monitor 성능 모니터
 */
void world_perf_monitor_print_realtime(WorldPerfMonitor* monitor);

/**
 * @brief 성능 요약 정보 출력
 *
 * @param monitor 성능 모니터
 */
void world_perf_monitor_print_summary(WorldPerfMonitor* monitor);

/**
 * @brief 상세 성능 정보 출력
 *
 * @param monitor 성능 모니터
 */
void world_perf_monitor_print_detailed(WorldPerfMonitor* monitor);

// =============================================================================
// 유틸리티 함수
// =============================================================================

/**
 * @brief 단계 이름 조회
 *
 * @param stage 처리 단계
 * @return const char* 단계 이름
 */
const char* world_perf_stage_get_name(WorldPerfStage stage);

/**
 * @brief 메트릭 타입 이름 조회
 *
 * @param metric_type 메트릭 타입
 * @return const char* 메트릭 타입 이름
 */
const char* world_perf_metric_get_name(WorldPerfMetricType metric_type);

/**
 * @brief 성능 점수 계산
 *
 * @param monitor 성능 모니터
 * @return double 전체 성능 점수 (0.0-1.0)
 */
double world_perf_monitor_calculate_score(WorldPerfMonitor* monitor);

/**
 * @brief 설정 검증
 *
 * @param config 검증할 설정
 * @return bool 검증 결과
 */
bool world_perf_monitor_config_validate(const WorldPerfMonitorConfig* config);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_WORLD_PERFORMANCE_MONITOR_H