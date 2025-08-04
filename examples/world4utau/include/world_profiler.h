#ifndef WORLD_PROFILER_H
#define WORLD_PROFILER_H

#include "dsp_block_diagram.h"
#include "world_graph_node.h"
#include "world_graph_context.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 성능 측정 타입
 */
typedef enum {
    PERF_METRIC_EXECUTION_TIME,     // 실행 시간
    PERF_METRIC_MEMORY_USAGE,       // 메모리 사용량
    PERF_METRIC_CPU_USAGE,          // CPU 사용률
    PERF_METRIC_CACHE_HITS,         // 캐시 히트율
    PERF_METRIC_THROUGHPUT,         // 처리량
    PERF_METRIC_LATENCY            // 지연 시간
} PerformanceMetricType;

/**
 * @brief 성능 측정 데이터
 */
typedef struct {
    PerformanceMetricType type;     // 측정 타입
    char name[64];                  // 측정 이름
    double value;                   // 측정 값
    double min_value;               // 최소값
    double max_value;               // 최대값
    double avg_value;               // 평균값
    uint64_t sample_count;          // 샘플 수
    uint64_t timestamp_us;          // 측정 시간 (마이크로초)
    char unit[16];                  // 단위
} PerformanceMetric;

/**
 * @brief 블록별 성능 통계
 */
typedef struct {
    char block_name[64];            // 블록 이름
    int block_id;                   // 블록 ID

    // 실행 시간 통계
    double total_execution_time_ms; // 총 실행 시간
    double avg_execution_time_ms;   // 평균 실행 시간
    double min_execution_time_ms;   // 최소 실행 시간
    double max_execution_time_ms;   // 최대 실행 시간

    // 메모리 사용량 통계
    size_t total_memory_allocated;  // 총 할당 메모리
    size_t peak_memory_usage;       // 최대 메모리 사용량
    size_t avg_memory_usage;        // 평균 메모리 사용량

    // 처리량 통계
    uint64_t total_samples_processed; // 총 처리 샘플 수
    double samples_per_second;      // 초당 처리 샘플 수

    // 실행 횟수
    uint64_t execution_count;       // 실행 횟수
    uint64_t error_count;           // 오류 횟수

    // 효율성 지표
    double cpu_efficiency;          // CPU 효율성 (0.0-1.0)
    double memory_efficiency;       // 메모리 효율성 (0.0-1.0)
} BlockPerformanceStats;

/**
 * @brief 병목 지점 정보
 */
typedef struct {
    char block_name[64];            // 병목 블록 이름
    double bottleneck_score;        // 병목 점수 (높을수록 심각)
    double execution_time_ratio;    // 전체 실행 시간 대비 비율
    size_t memory_usage_ratio;      // 전체 메모리 사용량 대비 비율
    char recommendation[256];       // 최적화 권장사항
} BottleneckInfo;

/**
 * @brief 성능 프로파일러 설정
 */
typedef struct {
    bool enable_timing;             // 시간 측정 활성화
    bool enable_memory_tracking;    // 메모리 추적 활성화
    bool enable_cpu_monitoring;     // CPU 모니터링 활성화
    bool enable_cache_analysis;     // 캐시 분석 활성화
    bool enable_realtime_monitoring; // 실시간 모니터링 활성화

    uint32_t sampling_interval_ms;  // 샘플링 간격 (밀리초)
    uint32_t max_samples;           // 최대 샘플 수
    uint32_t max_blocks;            // 최대 블록 수

    char output_format[16];         // 출력 형식 (json, csv, xml)
    bool generate_charts;           // 차트 생성 여부
} ProfilerConfig;

/**
 * @brief 성능 프로파일러 컨텍스트
 */
typedef struct {
    ProfilerConfig config;          // 프로파일러 설정

    PerformanceMetric* metrics;     // 성능 측정 데이터 배열
    uint32_t metric_count;          // 측정 데이터 수

    BlockPerformanceStats* block_stats; // 블록별 성능 통계
    uint32_t block_count;           // 블록 수

    BottleneckInfo* bottlenecks;    // 병목 지점 정보
    uint32_t bottleneck_count;      // 병목 지점 수

    uint64_t profiling_start_time;  // 프로파일링 시작 시간
    uint64_t profiling_duration;    // 프로파일링 지속 시간

    bool is_active;                 // 프로파일링 활성 상태
    bool is_paused;                 // 프로파일링 일시정지 상태

    FILE* log_file;                 // 로그 파일
    char log_file_path[256];        // 로그 파일 경로
} ProfilerContext;

/**
 * @brief 성능 이벤트 콜백 함수 타입
 */
typedef void (*PerformanceEventCallback)(const PerformanceMetric* metric, void* user_data);

/**
 * @brief 성능 프로파일러 생성
 * @param config 프로파일러 설정
 * @return 생성된 프로파일러 컨텍스트 (실패 시 NULL)
 */
ProfilerContext* world_profiler_create(const ProfilerConfig* config);

/**
 * @brief 프로파일링 시작
 * @param profiler 프로파일러 컨텍스트
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_start(ProfilerContext* profiler);

/**
 * @brief 프로파일링 중지
 * @param profiler 프로파일러 컨텍스트
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_stop(ProfilerContext* profiler);

/**
 * @brief 프로파일링 일시정지
 * @param profiler 프로파일러 컨텍스트
 */
void world_profiler_pause(ProfilerContext* profiler);

/**
 * @brief 프로파일링 재개
 * @param profiler 프로파일러 컨텍스트
 */
void world_profiler_resume(ProfilerContext* profiler);

/**
 * @brief 블록 실행 시간 측정 시작
 * @param profiler 프로파일러 컨텍스트
 * @param block_name 블록 이름
 * @param block_id 블록 ID
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_begin_block_timing(ProfilerContext* profiler,
                                     const char* block_name, int block_id);

/**
 * @brief 블록 실행 시간 측정 종료
 * @param profiler 프로파일러 컨텍스트
 * @param block_name 블록 이름
 * @param block_id 블록 ID
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_end_block_timing(ProfilerContext* profiler,
                                   const char* block_name, int block_id);

/**
 * @brief 메모리 사용량 기록
 * @param profiler 프로파일러 컨텍스트
 * @param block_name 블록 이름
 * @param memory_size 메모리 크기 (바이트)
 * @param is_allocation 할당 여부 (true: 할당, false: 해제)
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_record_memory_usage(ProfilerContext* profiler,
                                      const char* block_name,
                                      size_t memory_size, bool is_allocation);

/**
 * @brief 처리량 기록
 * @param profiler 프로파일러 컨텍스트
 * @param block_name 블록 이름
 * @param samples_processed 처리된 샘플 수
 * @param processing_time_ms 처리 시간 (밀리초)
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_record_throughput(ProfilerContext* profiler,
                                    const char* block_name,
                                    uint64_t samples_processed,
                                    double processing_time_ms);

/**
 * @brief 사용자 정의 성능 메트릭 추가
 * @param profiler 프로파일러 컨텍스트
 * @param metric_name 메트릭 이름
 * @param value 측정값
 * @param unit 단위
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_add_custom_metric(ProfilerContext* profiler,
                                    const char* metric_name,
                                    double value, const char* unit);

/**
 * @brief 병목 지점 분석
 * @param profiler 프로파일러 컨텍스트
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_analyze_bottlenecks(ProfilerContext* profiler);

/**
 * @brief 최적화 권장사항 생성
 * @param profiler 프로파일러 컨텍스트
 * @param output_path 출력 파일 경로
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_generate_optimization_recommendations(ProfilerContext* profiler,
                                                        const char* output_path);

/**
 * @brief 성능 보고서 생성
 * @param profiler 프로파일러 컨텍스트
 * @param output_path 출력 파일 경로
 * @param format 출력 형식 ("json", "csv", "html")
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_generate_report(ProfilerContext* profiler,
                                  const char* output_path, const char* format);

/**
 * @brief 실시간 성능 모니터링 시작
 * @param profiler 프로파일러 컨텍스트
 * @param callback 성능 이벤트 콜백
 * @param user_data 사용자 데이터
 * @return 성공 시 0, 실패 시 음수
 */
int world_profiler_start_realtime_monitoring(ProfilerContext* profiler,
                                            PerformanceEventCallback callback,
                                            void* user_data);

/**
 * @brief 실시간 성능 모니터링 중지
 * @param profiler 프로파일러 컨텍스트
 */
void world_profiler_stop_realtime_monitoring(ProfilerContext* profiler);

/**
 * @brief 블록별 성능 통계 조회
 * @param profiler 프로파일러 컨텍스트
 * @param block_name 블록 이름
 * @return 블록 성능 통계 (찾지 못하면 NULL)
 */
const BlockPerformanceStats* world_profiler_get_block_stats(const ProfilerContext* profiler,
                                                           const char* block_name);

/**
 * @brief 전체 성능 통계 출력
 * @param profiler 프로파일러 컨텍스트
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void world_profiler_print_summary(const ProfilerContext* profiler, FILE* output_file);

/**
 * @brief 병목 지점 정보 출력
 * @param profiler 프로파일러 컨텍스트
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void world_profiler_print_bottlenecks(const ProfilerContext* profiler, FILE* output_file);

/**
 * @brief 프로파일러 통계 초기화
 * @param profiler 프로파일러 컨텍스트
 */
void world_profiler_reset_stats(ProfilerContext* profiler);

/**
 * @brief 성능 프로파일러 해제
 * @param profiler 프로파일러 컨텍스트
 */
void world_profiler_destroy(ProfilerContext* profiler);

#ifdef __cplusplus
}
#endif

#endif // WORLD_PROFILER_H