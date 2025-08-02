#ifndef WORLD_GRAPH_CONTEXT_H
#define WORLD_GRAPH_CONTEXT_H

#include <libetude/graph.h>
#include <libetude/types.h>
#include <libetude/memory.h>
#include <libetude/task_scheduler.h>
#include "world_graph_node.h"
#include "utau_interface.h"
#include "world_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 그래프 실행 상태
 */
typedef enum {
    WORLD_GRAPH_STATE_IDLE,          /**< 유휴 상태 */
    WORLD_GRAPH_STATE_INITIALIZING,  /**< 초기화 중 */
    WORLD_GRAPH_STATE_RUNNING,       /**< 실행 중 */
    WORLD_GRAPH_STATE_PAUSED,        /**< 일시 정지 */
    WORLD_GRAPH_STATE_COMPLETED,     /**< 완료 */
    WORLD_GRAPH_STATE_ERROR          /**< 오류 */
} WorldGraphState;

/**
 * @brief 비동기 실행 콜백 타입
 */
typedef void (*WorldGraphCallback)(void* user_data, ETResult result, const char* message);

/**
 * @brief 진행 상황 콜백 타입
 */
typedef void (*WorldGraphProgressCallback)(void* user_data, float progress, const char* stage);

/**
 * @brief 그래프 실행 통계
 */
typedef struct {
    double total_execution_time;     /**< 총 실행 시간 (초) */
    double analysis_time;            /**< 분석 시간 (초) */
    double synthesis_time;           /**< 합성 시간 (초) */
    size_t memory_usage;             /**< 메모리 사용량 (바이트) */
    size_t peak_memory_usage;        /**< 최대 메모리 사용량 (바이트) */
    int nodes_executed;              /**< 실행된 노드 수 */
    int total_nodes;                 /**< 총 노드 수 */
} WorldGraphStats;

/**
 * @brief WORLD 그래프 실행 컨텍스트
 */
typedef struct {
    ETGraphContext* base_context;    /**< libetude 그래프 컨텍스트 */

    // 파라미터
    WorldParameters* world_params;   /**< WORLD 파라미터 */
    UTAUParameters* utau_params;     /**< UTAU 파라미터 */

    // 실행 상태
    WorldGraphState state;           /**< 현재 실행 상태 */
    bool is_analysis_complete;       /**< 분석 완료 여부 */
    bool is_synthesis_complete;      /**< 합성 완료 여부 */

    // 비동기 실행 지원
    bool is_async;                   /**< 비동기 실행 여부 */
    WorldGraphCallback completion_callback; /**< 완료 콜백 */
    WorldGraphProgressCallback progress_callback; /**< 진행 상황 콜백 */
    void* callback_user_data;        /**< 콜백 사용자 데이터 */

    // 스레드 관리
    ETTaskScheduler* task_scheduler; /**< 작업 스케줄러 */
    int thread_count;                /**< 사용할 스레드 수 */

    // 성능 모니터링
    WorldGraphStats stats;           /**< 실행 통계 */
    double start_time;               /**< 시작 시간 */
    double last_progress_time;       /**< 마지막 진행 상황 업데이트 시간 */

    // 메모리 관리
    ETMemoryPool* mem_pool;          /**< 메모리 풀 */

    // 오류 처리
    ETResult last_error;             /**< 마지막 오류 코드 */
    char error_message[256];         /**< 오류 메시지 */

    // 실행 제어
    bool should_stop;                /**< 중지 요청 플래그 */
    bool is_paused;                  /**< 일시 정지 플래그 */

    // 데이터 공유 (노드 간 데이터 전달)
    void** shared_data;              /**< 공유 데이터 배열 */
    int shared_data_count;           /**< 공유 데이터 개수 */

    // 캐시 관리
    bool enable_caching;             /**< 캐싱 활성화 */
    void* cache_context;             /**< 캐시 컨텍스트 */
} WorldGraphContext;

/**
 * @brief 그래프 실행 설정
 */
typedef struct {
    int thread_count;                /**< 스레드 수 (0: 자동) */
    bool enable_profiling;           /**< 프로파일링 활성화 */
    bool enable_caching;             /**< 캐싱 활성화 */
    bool enable_optimization;        /**< 최적화 활성화 */
    size_t memory_pool_size;         /**< 메모리 풀 크기 */
    double timeout_seconds;          /**< 타임아웃 (초, 0: 무제한) */
} WorldGraphExecutionConfig;

// 그래프 컨텍스트 생성 및 관리
WorldGraphContext* world_graph_context_create(const UTAUParameters* utau_params);
WorldGraphContext* world_graph_context_create_with_config(const UTAUParameters* utau_params,
                                                          const WorldGraphExecutionConfig* config);
void world_graph_context_destroy(WorldGraphContext* context);

// 파라미터 설정
ETResult world_graph_context_set_utau_parameters(WorldGraphContext* context,
                                                 const UTAUParameters* params);
ETResult world_graph_context_set_world_parameters(WorldGraphContext* context,
                                                  const WorldParameters* params);
const UTAUParameters* world_graph_context_get_utau_parameters(WorldGraphContext* context);
const WorldParameters* world_graph_context_get_world_parameters(WorldGraphContext* context);

// 콜백 설정
ETResult world_graph_context_set_completion_callback(WorldGraphContext* context,
                                                     WorldGraphCallback callback,
                                                     void* user_data);
ETResult world_graph_context_set_progress_callback(WorldGraphContext* context,
                                                   WorldGraphProgressCallback callback,
                                                   void* user_data);

// 그래프 실행
ETResult world_graph_execute(ETGraph* graph, WorldGraphContext* context);
ETResult world_graph_execute_async(ETGraph* graph, WorldGraphContext* context);
ETResult world_graph_execute_with_timeout(ETGraph* graph, WorldGraphContext* context,
                                          double timeout_seconds);

// 실행 제어
ETResult world_graph_context_pause(WorldGraphContext* context);
ETResult world_graph_context_resume(WorldGraphContext* context);
ETResult world_graph_context_stop(WorldGraphContext* context);
ETResult world_graph_context_reset(WorldGraphContext* context);

// 상태 조회
WorldGraphState world_graph_context_get_state(WorldGraphContext* context);
bool world_graph_context_is_running(WorldGraphContext* context);
bool world_graph_context_is_complete(WorldGraphContext* context);
float world_graph_context_get_progress(WorldGraphContext* context);

// 통계 및 성능 모니터링
const WorldGraphStats* world_graph_context_get_stats(WorldGraphContext* context);
ETResult world_graph_context_reset_stats(WorldGraphContext* context);
double world_graph_context_get_execution_time(WorldGraphContext* context);
size_t world_graph_context_get_memory_usage(WorldGraphContext* context);

// 오류 처리
ETResult world_graph_context_get_last_error(WorldGraphContext* context);
const char* world_graph_context_get_error_message(WorldGraphContext* context);
ETResult world_graph_context_clear_error(WorldGraphContext* context);

// 공유 데이터 관리
ETResult world_graph_context_set_shared_data(WorldGraphContext* context,
                                             int index, void* data);
void* world_graph_context_get_shared_data(WorldGraphContext* context, int index);
ETResult world_graph_context_allocate_shared_data(WorldGraphContext* context,
                                                  int count);

// 캐시 관리
ETResult world_graph_context_enable_caching(WorldGraphContext* context, bool enable);
ETResult world_graph_context_clear_cache(WorldGraphContext* context);
bool world_graph_context_is_caching_enabled(WorldGraphContext* context);

// 스레드 관리
ETResult world_graph_context_set_thread_count(WorldGraphContext* context, int count);
int world_graph_context_get_thread_count(WorldGraphContext* context);
ETResult world_graph_context_wait_for_completion(WorldGraphContext* context);

// 디버깅 및 진단
ETResult world_graph_context_dump_state(WorldGraphContext* context, const char* filename);
ETResult world_graph_context_print_stats(WorldGraphContext* context);
ETResult world_graph_context_validate_state(WorldGraphContext* context);

// 내부 헬퍼 함수들
ETResult world_graph_context_initialize_internal(WorldGraphContext* context);
ETResult world_graph_context_update_progress(WorldGraphContext* context,
                                             float progress, const char* stage);
ETResult world_graph_context_handle_node_completion(WorldGraphContext* context,
                                                    WorldGraphNode* node, ETResult result);
ETResult world_graph_context_handle_error(WorldGraphContext* context,
                                          ETResult error, const char* message);

#ifdef __cplusplus
}
#endif

#endif // WORLD_GRAPH_CONTEXT_H