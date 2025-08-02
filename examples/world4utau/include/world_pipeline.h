/**
 * @file world_pipeline.h
 * @brief 통합 WORLD 처리 파이프라인 관리 인터페이스
 *
 * DSP 블록 다이어그램과 그래프 엔진을 통합한 완전한 파이프라인을 제공합니다.
 * 설정 기반 파이프라인 생성 및 관리 기능을 포함합니다.
 */

#ifndef WORLD4UTAU_WORLD_PIPELINE_H
#define WORLD4UTAU_WORLD_PIPELINE_H

#include <libetude/types.h>
#include <libetude/memory.h>
#include <libetude/graph.h>
#include <libetude/profiler.h>
#include <libetude/audio_io.h>
#include "dsp_block_diagram.h"
#include "world_graph_builder.h"
#include "world_graph_context.h"
#include "world_engine.h"
#include "utau_interface.h"
#include "world_pipeline_config.h"
#include "world_performance_monitor.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 타입 정의
// =============================================================================

/**
 * @brief 파이프라인 상태
 */
typedef enum {
    WORLD_PIPELINE_STATE_UNINITIALIZED, /**< 초기화되지 않음 */
    WORLD_PIPELINE_STATE_INITIALIZED,   /**< 초기화됨 */
    WORLD_PIPELINE_STATE_READY,         /**< 실행 준비됨 */
    WORLD_PIPELINE_STATE_RUNNING,       /**< 실행 중 */
    WORLD_PIPELINE_STATE_PAUSED,        /**< 일시 정지 */
    WORLD_PIPELINE_STATE_COMPLETED,     /**< 완료 */
    WORLD_PIPELINE_STATE_ERROR          /**< 오류 */
} WorldPipelineState;

/**
 * @brief 스트리밍 오디오 콜백 타입
 */
typedef void (*AudioStreamCallback)(const float* audio_data, int frame_count, void* user_data);

/**
 * @brief 파이프라인 진행 상황 콜백 타입
 */
typedef void (*WorldPipelineProgressCallback)(void* user_data, float progress, const char* stage);

/**
 * @brief 파이프라인 완료 콜백 타입
 */
typedef void (*WorldPipelineCompletionCallback)(void* user_data, ETResult result, const char* message);

// 파이프라인 설정은 world_pipeline_config.h에서 WorldPipelineConfiguration으로 정의됨
typedef WorldPipelineConfiguration WorldPipelineConfig;

/**
 * @brief WORLD 처리 파이프라인 구조체
 */
typedef struct {
    // 핵심 컴포넌트
    DSPBlockDiagram* block_diagram;  /**< DSP 블록 다이어그램 */
    WorldGraphBuilder* graph_builder; /**< 그래프 빌더 */
    ETGraph* execution_graph;        /**< 실행 그래프 */
    WorldGraphContext* context;      /**< 실행 컨텍스트 */

    // 설정
    WorldPipelineConfig config;      /**< 파이프라인 설정 */

    // 상태 관리
    WorldPipelineState state;        /**< 현재 상태 */
    bool is_initialized;             /**< 초기화 여부 */
    bool is_running;                 /**< 실행 중 여부 */

    // 콜백
    WorldPipelineProgressCallback progress_callback; /**< 진행 상황 콜백 */
    WorldPipelineCompletionCallback completion_callback; /**< 완료 콜백 */
    void* callback_user_data;        /**< 콜백 사용자 데이터 */

    // 성능 모니터링
    ETProfiler* profiler;            /**< 프로파일러 */
    WorldPerfMonitor* perf_monitor;  /**< 성능 모니터 */
    double creation_time;            /**< 생성 시간 */
    double last_execution_time;      /**< 마지막 실행 시간 */

    // 메모리 관리
    ETMemoryPool* mem_pool;          /**< 메모리 풀 */

    // 오류 처리
    ETResult last_error;             /**< 마지막 오류 코드 */
    char error_message[512];         /**< 오류 메시지 */

    // 스트리밍 지원
    bool is_streaming_active;        /**< 스트리밍 활성 상태 */
    WorldStreamContext* stream_context; /**< 스트리밍 컨텍스트 */
    AudioStreamCallback stream_callback; /**< 스트리밍 콜백 */
    void* stream_user_data;          /**< 스트리밍 사용자 데이터 */

    // 디버깅 지원
    bool debug_enabled;              /**< 디버깅 활성화 */
    FILE* debug_log_file;            /**< 디버그 로그 파일 */
} WorldPipeline;

// =============================================================================
// 파이프라인 생성 및 관리
// =============================================================================

/**
 * @brief 기본 파이프라인 설정 생성
 *
 * @return WorldPipelineConfig 기본 설정
 */
WorldPipelineConfig world_pipeline_config_default(void);

/**
 * @brief 프리셋 기반 파이프라인 설정 생성
 *
 * @param preset 프리셋 타입
 * @return WorldPipelineConfig 프리셋 설정
 */
WorldPipelineConfig world_pipeline_config_create_preset(WorldConfigPreset preset);

/**
 * @brief 파이프라인 생성
 *
 * @param config 파이프라인 설정
 * @return WorldPipeline* 생성된 파이프라인 (실패 시 NULL)
 */
WorldPipeline* world_pipeline_create(const WorldPipelineConfig* config);

/**
 * @brief 파이프라인 해제
 *
 * @param pipeline 해제할 파이프라인
 */
void world_pipeline_destroy(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 초기화
 *
 * @param pipeline 초기화할 파이프라인
 * @return ETResult 초기화 결과
 */
ETResult world_pipeline_initialize(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 정리
 *
 * @param pipeline 정리할 파이프라인
 */
void world_pipeline_cleanup(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 재설정
 *
 * @param pipeline 재설정할 파이프라인
 * @param config 새로운 설정
 * @return ETResult 재설정 결과
 */
ETResult world_pipeline_reconfigure(WorldPipeline* pipeline, const WorldPipelineConfig* config);

// =============================================================================
// 파이프라인 실행
// =============================================================================

/**
 * @brief 파이프라인 처리 (동기)
 *
 * @param pipeline 파이프라인
 * @param utau_params UTAU 파라미터
 * @param output_audio 출력 오디오 버퍼
 * @param output_length 출력 길이 (입출력)
 * @return ETResult 처리 결과
 */
ETResult world_pipeline_process(WorldPipeline* pipeline,
                               const UTAUParameters* utau_params,
                               float* output_audio, int* output_length);

/**
 * @brief 파이프라인 처리 (비동기)
 *
 * @param pipeline 파이프라인
 * @param utau_params UTAU 파라미터
 * @param completion_callback 완료 콜백
 * @param user_data 사용자 데이터
 * @return ETResult 처리 시작 결과
 */
ETResult world_pipeline_process_async(WorldPipeline* pipeline,
                                     const UTAUParameters* utau_params,
                                     WorldPipelineCompletionCallback completion_callback,
                                     void* user_data);

/**
 * @brief 파이프라인 스트리밍 처리
 *
 * @param pipeline 파이프라인
 * @param utau_params UTAU 파라미터
 * @param stream_callback 스트리밍 콜백
 * @param user_data 사용자 데이터
 * @return ETResult 스트리밍 시작 결과
 */
ETResult world_pipeline_process_streaming(WorldPipeline* pipeline,
                                         const UTAUParameters* utau_params,
                                         AudioStreamCallback stream_callback,
                                         void* user_data);

// =============================================================================
// 파이프라인 제어
// =============================================================================

/**
 * @brief 파이프라인 일시 정지
 *
 * @param pipeline 파이프라인
 * @return ETResult 일시 정지 결과
 */
ETResult world_pipeline_pause(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 재개
 *
 * @param pipeline 파이프라인
 * @return ETResult 재개 결과
 */
ETResult world_pipeline_resume(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 중지
 *
 * @param pipeline 파이프라인
 * @return ETResult 중지 결과
 */
ETResult world_pipeline_stop(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 재시작
 *
 * @param pipeline 파이프라인
 * @return ETResult 재시작 결과
 */
ETResult world_pipeline_restart(WorldPipeline* pipeline);

// =============================================================================
// 상태 조회
// =============================================================================

/**
 * @brief 파이프라인 상태 조회
 *
 * @param pipeline 파이프라인
 * @return WorldPipelineState 현재 상태
 */
WorldPipelineState world_pipeline_get_state(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 실행 중 여부 확인
 *
 * @param pipeline 파이프라인
 * @return bool 실행 중 여부
 */
bool world_pipeline_is_running(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 완료 여부 확인
 *
 * @param pipeline 파이프라인
 * @return bool 완료 여부
 */
bool world_pipeline_is_completed(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 진행률 조회
 *
 * @param pipeline 파이프라인
 * @return float 진행률 (0.0-1.0)
 */
float world_pipeline_get_progress(WorldPipeline* pipeline);

// =============================================================================
// 콜백 설정
// =============================================================================

/**
 * @brief 진행 상황 콜백 설정
 *
 * @param pipeline 파이프라인
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return ETResult 설정 결과
 */
ETResult world_pipeline_set_progress_callback(WorldPipeline* pipeline,
                                             WorldPipelineProgressCallback callback,
                                             void* user_data);

/**
 * @brief 완료 콜백 설정
 *
 * @param pipeline 파이프라인
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return ETResult 설정 결과
 */
ETResult world_pipeline_set_completion_callback(WorldPipeline* pipeline,
                                               WorldPipelineCompletionCallback callback,
                                               void* user_data);

// =============================================================================
// 성능 모니터링
// =============================================================================

/**
 * @brief 파이프라인 성능 통계 조회
 *
 * @param pipeline 파이프라인
 * @return const WorldGraphStats* 성능 통계 (NULL 시 실패)
 */
const WorldGraphStats* world_pipeline_get_stats(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 상세 성능 통계 조회
 *
 * @param pipeline 파이프라인
 * @return const WorldPipelinePerformance* 상세 성능 통계 (NULL 시 실패)
 */
const WorldPipelinePerformance* world_pipeline_get_performance_stats(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 실행 시간 조회
 *
 * @param pipeline 파이프라인
 * @return double 실행 시간 (초)
 */
double world_pipeline_get_execution_time(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 메모리 사용량 조회
 *
 * @param pipeline 파이프라인
 * @return size_t 메모리 사용량 (바이트)
 */
size_t world_pipeline_get_memory_usage(WorldPipeline* pipeline);

/**
 * @brief 성능 통계 초기화
 *
 * @param pipeline 파이프라인
 * @return ETResult 초기화 결과
 */
ETResult world_pipeline_reset_stats(WorldPipeline* pipeline);

// =============================================================================
// 오류 처리
// =============================================================================

/**
 * @brief 마지막 오류 코드 조회
 *
 * @param pipeline 파이프라인
 * @return ETResult 오류 코드
 */
ETResult world_pipeline_get_last_error(WorldPipeline* pipeline);

/**
 * @brief 오류 메시지 조회
 *
 * @param pipeline 파이프라인
 * @return const char* 오류 메시지
 */
const char* world_pipeline_get_error_message(WorldPipeline* pipeline);

/**
 * @brief 오류 상태 초기화
 *
 * @param pipeline 파이프라인
 * @return ETResult 초기화 결과
 */
ETResult world_pipeline_clear_error(WorldPipeline* pipeline);

// =============================================================================
// 디버깅 및 진단
// =============================================================================

/**
 * @brief 파이프라인 상태 덤프
 *
 * @param pipeline 파이프라인
 * @param filename 출력 파일명
 * @return ETResult 덤프 결과
 */
ETResult world_pipeline_dump_state(WorldPipeline* pipeline, const char* filename);

/**
 * @brief 파이프라인 구조 시각화 (DOT 형식)
 *
 * @param pipeline 파이프라인
 * @param filename 출력 파일명
 * @return ETResult 시각화 결과
 */
ETResult world_pipeline_export_dot(WorldPipeline* pipeline, const char* filename);

/**
 * @brief 파이프라인 검증
 *
 * @param pipeline 파이프라인
 * @return ETResult 검증 결과
 */
ETResult world_pipeline_validate(WorldPipeline* pipeline);

/**
 * @brief 파이프라인 정보 출력
 *
 * @param pipeline 파이프라인
 */
void world_pipeline_print_info(WorldPipeline* pipeline);

// =============================================================================
// 유틸리티 함수
// =============================================================================

/**
 * @brief 파이프라인 완료 대기
 *
 * @param pipeline 파이프라인
 * @param timeout_seconds 타임아웃 (초, 0: 무제한)
 * @return ETResult 대기 결과
 */
ETResult world_pipeline_wait_for_completion(WorldPipeline* pipeline, double timeout_seconds);

/**
 * @brief 파이프라인 설정 복사
 *
 * @param src 소스 설정
 * @param dst 대상 설정
 * @return ETResult 복사 결과
 */
ETResult world_pipeline_config_copy(const WorldPipelineConfig* src, WorldPipelineConfig* dst);

/**
 * @brief 파이프라인 설정 검증
 *
 * @param config 검증할 설정
 * @return bool 검증 결과
 */
bool world_pipeline_config_validate(const WorldPipelineConfig* config);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_WORLD_PIPELINE_H