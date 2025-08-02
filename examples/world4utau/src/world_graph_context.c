#include "world_graph_context.h"
#include "world_error.h"
#include <libetude/memory.h>
#include <libetude/error.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// 기본 설정값
#define DEFAULT_MEMORY_POOL_SIZE (2 * 1024 * 1024) // 2MB
#define DEFAULT_THREAD_COUNT 4
#define DEFAULT_SHARED_DATA_COUNT 16
#define PROGRESS_UPDATE_INTERVAL 0.1 // 0.1초마다 진행 상황 업데이트

/**
 * @brief 현재 시간을 초 단위로 반환
 */
static double get_current_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

/**
 * @brief 그래프 컨텍스트 생성
 */
WorldGraphContext* world_graph_context_create(const UTAUParameters* utau_params) {
    WorldGraphExecutionConfig default_config = {
        .thread_count = DEFAULT_THREAD_COUNT,
        .enable_profiling = true,
        .enable_caching = true,
        .enable_optimization = true,
        .memory_pool_size = DEFAULT_MEMORY_POOL_SIZE,
        .timeout_seconds = 0.0 // 무제한
    };

    return world_graph_context_create_with_config(utau_params, &default_config);
}

/**
 * @brief 설정을 지정하여 그래프 컨텍스트 생성
 */
WorldGraphContext* world_graph_context_create_with_config(const UTAUParameters* utau_params,
                                                          const WorldGraphExecutionConfig* config) {
    if (!utau_params || !config) {
        return NULL;
    }

    // 메모리 풀 생성
    ETMemoryPool* pool = et_memory_pool_create(config->memory_pool_size);
    if (!pool) {
        return NULL;
    }

    // 컨텍스트 구조체 할당
    WorldGraphContext* context = (WorldGraphContext*)et_memory_pool_alloc(pool, sizeof(WorldGraphContext));
    if (!context) {
        et_memory_pool_destroy(pool);
        return NULL;
    }

    memset(context, 0, sizeof(WorldGraphContext));

    // 기본 설정
    context->mem_pool = pool;
    context->state = WORLD_GRAPH_STATE_IDLE;
    context->is_analysis_complete = false;
    context->is_synthesis_complete = false;
    context->is_async = false;
    context->thread_count = config->thread_count;
    context->enable_caching = config->enable_caching;
    context->should_stop = false;
    context->is_paused = false;
    context->last_error = ET_SUCCESS;

    // UTAU 파라미터 복사
    context->utau_params = (UTAUParameters*)et_memory_pool_alloc(pool, sizeof(UTAUParameters));
    if (!context->utau_params) {
        et_memory_pool_destroy(pool);
        return NULL;
    }
    memcpy(context->utau_params, utau_params, sizeof(UTAUParameters));

    // WORLD 파라미터 초기화 (나중에 설정)
    context->world_params = NULL;

    // 통계 초기화
    memset(&context->stats, 0, sizeof(WorldGraphStats));

    // 공유 데이터 배열 할당
    context->shared_data_count = DEFAULT_SHARED_DATA_COUNT;
    context->shared_data = (void**)et_memory_pool_alloc(pool,
                                                        DEFAULT_SHARED_DATA_COUNT * sizeof(void*));
    if (!context->shared_data) {
        et_memory_pool_destroy(pool);
        return NULL;
    }
    memset(context->shared_data, 0, DEFAULT_SHARED_DATA_COUNT * sizeof(void*));

    // libetude 그래프 컨텍스트 생성 (실제 구현에서는 libetude API 사용)
    context->base_context = (ETGraphContext*)0x1; // 임시 더미 포인터

    // 작업 스케줄러 생성 (실제 구현에서는 libetude API 사용)
    context->task_scheduler = (ETTaskScheduler*)0x1; // 임시 더미 포인터

    return context;
}

/**
 * @brief 그래프 컨텍스트 해제
 */
void world_graph_context_destroy(WorldGraphContext* context) {
    if (!context) {
        return;
    }

    // 실행 중인 작업 중지
    if (world_graph_context_is_running(context)) {
        world_graph_context_stop(context);
    }

    // 작업 스케줄러 해제 (실제 구현에서는 libetude API 사용)
    if (context->task_scheduler) {
        // et_task_scheduler_destroy(context->task_scheduler);
        context->task_scheduler = NULL;
    }

    // libetude 그래프 컨텍스트 해제 (실제 구현에서는 libetude API 사용)
    if (context->base_context) {
        // et_graph_context_destroy(context->base_context);
        context->base_context = NULL;
    }

    // 메모리 풀 해제 (모든 할당된 메모리 자동 해제)
    if (context->mem_pool) {
        et_memory_pool_destroy(context->mem_pool);
    }
}

/**
 * @brief UTAU 파라미터 설정
 */
ETResult world_graph_context_set_utau_parameters(WorldGraphContext* context,
                                                 const UTAUParameters* params) {
    if (!context || !params) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (world_graph_context_is_running(context)) {
        return ET_ERROR_INVALID_STATE;
    }

    // 파라미터 복사
    memcpy(context->utau_params, params, sizeof(UTAUParameters));

    return ET_SUCCESS;
}

/**
 * @brief WORLD 파라미터 설정
 */
ETResult world_graph_context_set_world_parameters(WorldGraphContext* context,
                                                  const WorldParameters* params) {
    if (!context || !params) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!context->world_params) {
        context->world_params = (WorldParameters*)et_memory_pool_alloc(context->mem_pool,
                                                                       sizeof(WorldParameters));
        if (!context->world_params) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
    }

    // 파라미터 복사 (실제 구현에서는 world_parameters_copy 사용)
    memcpy(context->world_params, params, sizeof(WorldParameters));

    return ET_SUCCESS;
}

/**
 * @brief UTAU 파라미터 조회
 */
const UTAUParameters* world_graph_context_get_utau_parameters(WorldGraphContext* context) {
    if (!context) {
        return NULL;
    }

    return context->utau_params;
}

/**
 * @brief WORLD 파라미터 조회
 */
const WorldParameters* world_graph_context_get_world_parameters(WorldGraphContext* context) {
    if (!context) {
        return NULL;
    }

    return context->world_params;
}

/**
 * @brief 완료 콜백 설정
 */
ETResult world_graph_context_set_completion_callback(WorldGraphContext* context,
                                                     WorldGraphCallback callback,
                                                     void* user_data) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    context->completion_callback = callback;
    context->callback_user_data = user_data;

    return ET_SUCCESS;
}

/**
 * @brief 진행 상황 콜백 설정
 */
ETResult world_graph_context_set_progress_callback(WorldGraphContext* context,
                                                   WorldGraphProgressCallback callback,
                                                   void* user_data) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    context->progress_callback = callback;
    context->callback_user_data = user_data;

    return ET_SUCCESS;
}

/**
 * @brief 그래프 실행 (동기)
 */
ETResult world_graph_execute(ETGraph* graph, WorldGraphContext* context) {
    if (!graph || !context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (world_graph_context_is_running(context)) {
        return ET_ERROR_INVALID_STATE;
    }

    // 실행 상태 설정
    context->state = WORLD_GRAPH_STATE_INITIALIZING;
    context->is_async = false;
    context->should_stop = false;
    context->is_paused = false;
    context->start_time = get_current_time();
    context->last_progress_time = context->start_time;

    // 통계 초기화
    memset(&context->stats, 0, sizeof(WorldGraphStats));

    // 내부 초기화
    ETResult result = world_graph_context_initialize_internal(context);
    if (result != ET_SUCCESS) {
        context->state = WORLD_GRAPH_STATE_ERROR;
        return result;
    }

    // 실행 시작
    context->state = WORLD_GRAPH_STATE_RUNNING;

    // 진행 상황 업데이트
    world_graph_context_update_progress(context, 0.0f, "Starting execution");

    // 실제 그래프 실행 (실제 구현에서는 libetude 그래프 엔진 사용)
    // 여기서는 시뮬레이션된 실행

    // 분석 단계 시뮬레이션
    world_graph_context_update_progress(context, 0.1f, "F0 extraction");
    // F0 추출 시뮬레이션...

    world_graph_context_update_progress(context, 0.3f, "Spectrum analysis");
    // 스펙트럼 분석 시뮬레이션...

    world_graph_context_update_progress(context, 0.5f, "Aperiodicity analysis");
    // 비주기성 분석 시뮬레이션...

    context->is_analysis_complete = true;
    world_graph_context_update_progress(context, 0.6f, "Analysis complete");

    // 합성 단계 시뮬레이션
    world_graph_context_update_progress(context, 0.8f, "Voice synthesis");
    // 음성 합성 시뮬레이션...

    context->is_synthesis_complete = true;
    world_graph_context_update_progress(context, 1.0f, "Synthesis complete");

    // 실행 완료
    context->state = WORLD_GRAPH_STATE_COMPLETED;

    // 통계 업데이트
    context->stats.total_execution_time = get_current_time() - context->start_time;
    context->stats.analysis_time = context->stats.total_execution_time * 0.6;
    context->stats.synthesis_time = context->stats.total_execution_time * 0.4;

    // 완료 콜백 호출
    if (context->completion_callback) {
        context->completion_callback(context->callback_user_data, ET_SUCCESS, "Execution completed");
    }

    return ET_SUCCESS;
}

/**
 * @brief 그래프 실행 (비동기)
 */
ETResult world_graph_execute_async(ETGraph* graph, WorldGraphContext* context) {
    if (!graph || !context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (world_graph_context_is_running(context)) {
        return ET_ERROR_INVALID_STATE;
    }

    context->is_async = true;

    // 실제 구현에서는 별도 스레드에서 world_graph_execute 호출
    // 여기서는 동기 실행으로 대체
    return world_graph_execute(graph, context);
}

/**
 * @brief 타임아웃을 지정하여 그래프 실행
 */
ETResult world_graph_execute_with_timeout(ETGraph* graph, WorldGraphContext* context,
                                          double timeout_seconds) {
    if (!graph || !context || timeout_seconds <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실제 구현에서는 타임아웃 처리 로직 추가
    return world_graph_execute(graph, context);
}

/**
 * @brief 실행 일시 정지
 */
ETResult world_graph_context_pause(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (context->state != WORLD_GRAPH_STATE_RUNNING) {
        return ET_ERROR_INVALID_STATE;
    }

    context->is_paused = true;
    context->state = WORLD_GRAPH_STATE_PAUSED;

    return ET_SUCCESS;
}

/**
 * @brief 실행 재개
 */
ETResult world_graph_context_resume(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (context->state != WORLD_GRAPH_STATE_PAUSED) {
        return ET_ERROR_INVALID_STATE;
    }

    context->is_paused = false;
    context->state = WORLD_GRAPH_STATE_RUNNING;

    return ET_SUCCESS;
}

/**
 * @brief 실행 중지
 */
ETResult world_graph_context_stop(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!world_graph_context_is_running(context) &&
        context->state != WORLD_GRAPH_STATE_PAUSED) {
        return ET_ERROR_INVALID_STATE;
    }

    context->should_stop = true;
    context->state = WORLD_GRAPH_STATE_IDLE;

    return ET_SUCCESS;
}

/**
 * @brief 컨텍스트 리셋
 */
ETResult world_graph_context_reset(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실행 중인 경우 중지
    if (world_graph_context_is_running(context)) {
        world_graph_context_stop(context);
    }

    // 상태 초기화
    context->state = WORLD_GRAPH_STATE_IDLE;
    context->is_analysis_complete = false;
    context->is_synthesis_complete = false;
    context->should_stop = false;
    context->is_paused = false;
    context->last_error = ET_SUCCESS;
    memset(context->error_message, 0, sizeof(context->error_message));

    // 통계 초기화
    memset(&context->stats, 0, sizeof(WorldGraphStats));

    return ET_SUCCESS;
}

/**
 * @brief 실행 상태 조회
 */
WorldGraphState world_graph_context_get_state(WorldGraphContext* context) {
    if (!context) {
        return WORLD_GRAPH_STATE_ERROR;
    }

    return context->state;
}

/**
 * @brief 실행 중 여부 확인
 */
bool world_graph_context_is_running(WorldGraphContext* context) {
    if (!context) {
        return false;
    }

    return (context->state == WORLD_GRAPH_STATE_RUNNING ||
            context->state == WORLD_GRAPH_STATE_INITIALIZING);
}

/**
 * @brief 완료 여부 확인
 */
bool world_graph_context_is_complete(WorldGraphContext* context) {
    if (!context) {
        return false;
    }

    return (context->state == WORLD_GRAPH_STATE_COMPLETED);
}

/**
 * @brief 진행 상황 조회 (0.0 ~ 1.0)
 */
float world_graph_context_get_progress(WorldGraphContext* context) {
    if (!context) {
        return 0.0f;
    }

    if (context->state == WORLD_GRAPH_STATE_COMPLETED) {
        return 1.0f;
    }

    if (context->state == WORLD_GRAPH_STATE_IDLE ||
        context->state == WORLD_GRAPH_STATE_ERROR) {
        return 0.0f;
    }

    // 간단한 진행 상황 계산
    float progress = 0.0f;

    if (context->is_analysis_complete) {
        progress += 0.6f;
    } else {
        // 분석 단계 진행 상황 추정
        progress += 0.3f; // 임시값
    }

    if (context->is_synthesis_complete) {
        progress += 0.4f;
    } else if (context->is_analysis_complete) {
        // 합성 단계 진행 상황 추정
        progress += 0.2f; // 임시값
    }

    return fminf(progress, 1.0f);
}

/**
 * @brief 통계 조회
 */
const WorldGraphStats* world_graph_context_get_stats(WorldGraphContext* context) {
    if (!context) {
        return NULL;
    }

    return &context->stats;
}

/**
 * @brief 통계 리셋
 */
ETResult world_graph_context_reset_stats(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(&context->stats, 0, sizeof(WorldGraphStats));

    return ET_SUCCESS;
}

/**
 * @brief 실행 시간 조회
 */
double world_graph_context_get_execution_time(WorldGraphContext* context) {
    if (!context) {
        return 0.0;
    }

    if (context->state == WORLD_GRAPH_STATE_COMPLETED) {
        return context->stats.total_execution_time;
    } else if (world_graph_context_is_running(context)) {
        return get_current_time() - context->start_time;
    }

    return 0.0;
}

/**
 * @brief 메모리 사용량 조회
 */
size_t world_graph_context_get_memory_usage(WorldGraphContext* context) {
    if (!context || !context->mem_pool) {
        return 0;
    }

    // 실제 구현에서는 메모리 풀의 사용량 조회
    return context->stats.memory_usage;
}

/**
 * @brief 마지막 오류 조회
 */
ETResult world_graph_context_get_last_error(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return context->last_error;
}

/**
 * @brief 오류 메시지 조회
 */
const char* world_graph_context_get_error_message(WorldGraphContext* context) {
    if (!context) {
        return "Invalid context";
    }

    return context->error_message;
}

/**
 * @brief 오류 상태 초기화
 */
ETResult world_graph_context_clear_error(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    context->last_error = ET_SUCCESS;
    memset(context->error_message, 0, sizeof(context->error_message));

    return ET_SUCCESS;
}

/**
 * @brief 공유 데이터 설정
 */
ETResult world_graph_context_set_shared_data(WorldGraphContext* context,
                                             int index, void* data) {
    if (!context || index < 0 || index >= context->shared_data_count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    context->shared_data[index] = data;

    return ET_SUCCESS;
}

/**
 * @brief 공유 데이터 조회
 */
void* world_graph_context_get_shared_data(WorldGraphContext* context, int index) {
    if (!context || index < 0 || index >= context->shared_data_count) {
        return NULL;
    }

    return context->shared_data[index];
}

/**
 * @brief 스레드 수 설정
 */
ETResult world_graph_context_set_thread_count(WorldGraphContext* context, int count) {
    if (!context || count <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (world_graph_context_is_running(context)) {
        return ET_ERROR_INVALID_STATE;
    }

    context->thread_count = count;

    return ET_SUCCESS;
}

/**
 * @brief 스레드 수 조회
 */
int world_graph_context_get_thread_count(WorldGraphContext* context) {
    if (!context) {
        return 0;
    }

    return context->thread_count;
}

/**
 * @brief 내부 초기화
 */
ETResult world_graph_context_initialize_internal(WorldGraphContext* context) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실제 구현에서는 libetude 그래프 엔진 초기화
    // 작업 스케줄러 설정, 메모리 풀 준비 등

    return ET_SUCCESS;
}

/**
 * @brief 진행 상황 업데이트
 */
ETResult world_graph_context_update_progress(WorldGraphContext* context,
                                             float progress, const char* stage) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    double current_time = get_current_time();

    // 진행 상황 콜백 호출 (일정 간격으로만)
    if (context->progress_callback &&
        (current_time - context->last_progress_time) >= PROGRESS_UPDATE_INTERVAL) {
        context->progress_callback(context->callback_user_data, progress, stage);
        context->last_progress_time = current_time;
    }

    return ET_SUCCESS;
}

/**
 * @brief 오류 처리
 */
ETResult world_graph_context_handle_error(WorldGraphContext* context,
                                          ETResult error, const char* message) {
    if (!context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    context->last_error = error;
    context->state = WORLD_GRAPH_STATE_ERROR;

    if (message) {
        strncpy(context->error_message, message, sizeof(context->error_message) - 1);
        context->error_message[sizeof(context->error_message) - 1] = '\0';
    }

    // 완료 콜백 호출 (오류와 함께)
    if (context->completion_callback) {
        context->completion_callback(context->callback_user_data, error, message);
    }

    return ET_SUCCESS;
}