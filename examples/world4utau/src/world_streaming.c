/**
 * @file world_streaming.c
 * @brief WORLD 파이프라인 스트리밍 처리 구현
 */

#include "world_streaming.h"
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
static ETResult world_stream_set_state(WorldStreamContext* context, WorldStreamState new_state);
static ETResult world_stream_update_stats(WorldStreamContext* context);
static ETResult world_stream_check_quality_adaptation(WorldStreamContext* context);
static void* world_stream_processing_thread(void* arg);
static ETResult world_stream_process_chunk(WorldStreamContext* context, WorldAudioChunk* chunk);
static ETResult world_stream_allocate_buffers(WorldStreamContext* context);
static void world_stream_deallocate_buffers(WorldStreamContext* context);

// =============================================================================
// 기본 설정 및 생성 함수들
// =============================================================================

WorldStreamConfig world_stream_config_default(void) {
    WorldStreamConfig config = {0};

    // 기본 설정
    config.mode = WORLD_STREAM_MODE_REALTIME;
    config.chunk_size = 256;
    config.buffer_count = 8;
    config.sample_rate = 44100;
    config.channel_count = 1;

    // 지연 시간 설정
    config.target_latency_ms = 10.0;
    config.max_latency_ms = 50.0;

    // 품질 설정
    config.enable_quality_adaptation = true;
    config.quality_threshold = 0.8f;

    // 버퍼링 설정
    config.min_buffer_size = 2;
    config.max_buffer_size = 16;
    config.buffer_timeout_ms = 100.0;

    // 스레드 설정
    config.processing_thread_count = 2;
    config.enable_thread_affinity = false;

    // 콜백 초기화
    config.audio_callback = NULL;
    config.progress_callback = NULL;
    config.error_callback = NULL;
    config.state_callback = NULL;
    config.callback_user_data = NULL;

    return config;
}

WorldStreamContext* world_stream_context_create(const WorldStreamConfig* config) {
    if (!config) {
        WorldStreamConfig default_config = world_stream_config_default();
        config = &default_config;
    }

    // 설정 검증
    if (!world_stream_config_validate(config)) {
        return NULL;
    }

    // 컨텍스트 할당
    WorldStreamContext* context = (WorldStreamContext*)calloc(1, sizeof(WorldStreamContext));
    if (!context) {
        return NULL;
    }

    // 설정 복사
    if (world_stream_config_copy(config, &context->config) != ET_SUCCESS) {
        free(context);
        return NULL;
    }

    // 초기 상태 설정
    context->state = WORLD_STREAM_STATE_IDLE;
    context->is_active = false;
    context->should_stop = false;
    context->buffer_read_index = 0;
    context->buffer_write_index = 0;
    context->buffer_count = 0;
    context->current_quality_level = 1.0f;
    context->start_time = 0.0;
    context->last_chunk_time = 0.0;
    context->last_quality_check_time = 0.0;
    context->last_error = ET_SUCCESS;

    // 메모리 풀 생성
    size_t pool_size = config->buffer_count * config->chunk_size * config->channel_count * sizeof(float) * 4;
    context->mem_pool = et_memory_pool_create(pool_size);
    if (!context->mem_pool) {
        world_stream_context_destroy(context);
        return NULL;
    }

    // 동기화 객체 생성
    context->buffer_mutex = malloc(sizeof(pthread_mutex_t));
    context->state_mutex = malloc(sizeof(pthread_mutex_t));
    context->condition_var = malloc(sizeof(pthread_cond_t));

    if (!context->buffer_mutex || !context->state_mutex || !context->condition_var) {
        world_stream_context_destroy(context);
        return NULL;
    }

    pthread_mutex_init((pthread_mutex_t*)context->buffer_mutex, NULL);
    pthread_mutex_init((pthread_mutex_t*)context->state_mutex, NULL);
    pthread_cond_init((pthread_cond_t*)context->condition_var, NULL);

    // 작업 스케줄러 생성
    context->task_scheduler = et_task_scheduler_create(config->processing_thread_count);
    if (!context->task_scheduler) {
        world_stream_context_destroy(context);
        return NULL;
    }

    // 버퍼 할당
    if (world_stream_allocate_buffers(context) != ET_SUCCESS) {
        world_stream_context_destroy(context);
        return NULL;
    }

    return context;
}

void world_stream_context_destroy(WorldStreamContext* context) {
    if (!context) return;

    // 스트리밍 중지
    if (context->is_active) {
        world_stream_stop(context);
    }

    // 정리
    world_stream_cleanup(context);

    // 버퍼 해제
    world_stream_deallocate_buffers(context);

    // 작업 스케줄러 해제
    if (context->task_scheduler) {
        et_task_scheduler_destroy(context->task_scheduler);
    }

    // 동기화 객체 해제
    if (context->buffer_mutex) {
        pthread_mutex_destroy((pthread_mutex_t*)context->buffer_mutex);
        free(context->buffer_mutex);
    }
    if (context->state_mutex) {
        pthread_mutex_destroy((pthread_mutex_t*)context->state_mutex);
        free(context->state_mutex);
    }
    if (context->condition_var) {
        pthread_cond_destroy((pthread_cond_t*)context->condition_var);
        free(context->condition_var);
    }

    // 메모리 풀 해제
    if (context->mem_pool) {
        et_memory_pool_destroy(context->mem_pool);
    }

    free(context);
}

ETResult world_stream_initialize(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    if (context->state != WORLD_STREAM_STATE_IDLE) {
        return ET_ERROR_INVALID_STATE;
    }

    world_stream_set_state(context, WORLD_STREAM_STATE_INITIALIZING);

    // 통계 초기화
    memset(&context->stats, 0, sizeof(WorldStreamStats));
    context->start_time = get_current_time();

    // 버퍼 초기화
    context->buffer_read_index = 0;
    context->buffer_write_index = 0;
    context->buffer_count = 0;

    world_stream_set_state(context, WORLD_STREAM_STATE_IDLE);
    return ET_SUCCESS;
}

void world_stream_cleanup(WorldStreamContext* context) {
    if (!context) return;

    // 모든 처리 중지
    context->should_stop = true;
    context->is_active = false;

    // 버퍼 플러시
    world_stream_flush_buffers(context);

    // 상태 초기화
    world_stream_set_state(context, WORLD_STREAM_STATE_IDLE);
}

// =============================================================================
// 스트리밍 제어 함수들
// =============================================================================

ETResult world_stream_start(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    if (context->state != WORLD_STREAM_STATE_IDLE) {
        return ET_ERROR_INVALID_STATE;
    }

    world_stream_set_state(context, WORLD_STREAM_STATE_BUFFERING);

    context->is_active = true;
    context->should_stop = false;
    context->start_time = get_current_time();
    context->last_chunk_time = context->start_time;

    // 처리 스레드 시작
    context->processing_threads = malloc(context->config.processing_thread_count * sizeof(void*));
    if (!context->processing_threads) {
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    for (int i = 0; i < context->config.processing_thread_count; i++) {
        pthread_t* thread = malloc(sizeof(pthread_t));
        if (!thread) {
            return ET_ERROR_MEMORY_ALLOCATION;
        }

        if (pthread_create(thread, NULL, world_stream_processing_thread, context) != 0) {
            free(thread);
            return ET_ERROR_THREAD_CREATION;
        }

        context->processing_threads[i] = thread;
    }

    world_stream_set_state(context, WORLD_STREAM_STATE_STREAMING);
    return ET_SUCCESS;
}

ETResult world_stream_stop(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    if (!context->is_active) {
        return ET_SUCCESS; // 이미 중지됨
    }

    world_stream_set_state(context, WORLD_STREAM_STATE_STOPPING);

    // 중지 플래그 설정
    context->should_stop = true;

    // 조건 변수 신호로 대기 중인 스레드들 깨우기
    pthread_cond_broadcast((pthread_cond_t*)context->condition_var);

    // 처리 스레드들 종료 대기
    if (context->processing_threads) {
        for (int i = 0; i < context->config.processing_thread_count; i++) {
            if (context->processing_threads[i]) {
                pthread_t* thread = (pthread_t*)context->processing_threads[i];
                pthread_join(*thread, NULL);
                free(thread);
            }
        }
        free(context->processing_threads);
        context->processing_threads = NULL;
    }

    context->is_active = false;
    world_stream_set_state(context, WORLD_STREAM_STATE_IDLE);

    return ET_SUCCESS;
}

ETResult world_stream_pause(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    if (context->state != WORLD_STREAM_STATE_STREAMING) {
        return ET_ERROR_INVALID_STATE;
    }

    world_stream_set_state(context, WORLD_STREAM_STATE_PAUSED);
    return ET_SUCCESS;
}

ETResult world_stream_resume(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    if (context->state != WORLD_STREAM_STATE_PAUSED) {
        return ET_ERROR_INVALID_STATE;
    }

    world_stream_set_state(context, WORLD_STREAM_STATE_STREAMING);

    // 조건 변수 신호로 대기 중인 스레드들 깨우기
    pthread_cond_broadcast((pthread_cond_t*)context->condition_var);

    return ET_SUCCESS;
}

ETResult world_stream_restart(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    ETResult result = world_stream_stop(context);
    if (result != ET_SUCCESS) return result;

    world_stream_cleanup(context);

    result = world_stream_initialize(context);
    if (result != ET_SUCCESS) return result;

    return world_stream_start(context);
}

// =============================================================================
// 청크 처리 함수들
// =============================================================================

WorldAudioChunk* world_audio_chunk_create(int frame_count, int channel_count,
                                          int sample_rate, ETMemoryPool* mem_pool) {
    if (frame_count <= 0 || channel_count <= 0 || sample_rate <= 0) {
        return NULL;
    }

    WorldAudioChunk* chunk = (WorldAudioChunk*)calloc(1, sizeof(WorldAudioChunk));
    if (!chunk) {
        return NULL;
    }

    size_t audio_size = frame_count * channel_count * sizeof(float);
    chunk->audio_data = (float*)malloc(audio_size);
    if (!chunk->audio_data) {
        free(chunk);
        return NULL;
    }

    chunk->frame_count = frame_count;
    chunk->channel_count = channel_count;
    chunk->sample_rate = sample_rate;
    chunk->timestamp = get_current_time();
    chunk->sequence_number = 0;
    chunk->is_final = false;

    memset(chunk->audio_data, 0, audio_size);

    return chunk;
}

void world_audio_chunk_destroy(WorldAudioChunk* chunk) {
    if (!chunk) return;

    if (chunk->audio_data) {
        free(chunk->audio_data);
    }

    free(chunk);
}

ETResult world_audio_chunk_copy(const WorldAudioChunk* src, WorldAudioChunk* dst) {
    if (!src || !dst) return ET_ERROR_INVALID_PARAMETER;

    if (dst->frame_count < src->frame_count || dst->channel_count < src->channel_count) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    size_t copy_size = src->frame_count * src->channel_count * sizeof(float);
    memcpy(dst->audio_data, src->audio_data, copy_size);

    dst->frame_count = src->frame_count;
    dst->channel_count = src->channel_count;
    dst->sample_rate = src->sample_rate;
    dst->timestamp = src->timestamp;
    dst->sequence_number = src->sequence_number;
    dst->is_final = src->is_final;

    return ET_SUCCESS;
}

ETResult world_stream_push_audio(WorldStreamContext* context,
                                const float* input_audio, int input_length) {
    if (!context || !input_audio || input_length <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!context->is_active) {
        return ET_ERROR_INVALID_STATE;
    }

    pthread_mutex_lock((pthread_mutex_t*)context->buffer_mutex);

    // 입력 오디오를 청크 크기로 분할
    int processed_frames = 0;
    while (processed_frames < input_length) {
        int remaining_frames = input_length - processed_frames;
        int chunk_frames = (remaining_frames < context->config.chunk_size) ?
                          remaining_frames : context->config.chunk_size;

        // 버퍼 공간 확인
        if (context->buffer_count >= context->config.buffer_count) {
            context->stats.buffer_overruns++;
            pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);
            return ET_ERROR_BUFFER_FULL;
        }

        // 새 청크 생성
        WorldAudioChunk* chunk = world_audio_chunk_create(chunk_frames,
                                                         context->config.channel_count,
                                                         context->config.sample_rate,
                                                         context->mem_pool);
        if (!chunk) {
            pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);
            return ET_ERROR_MEMORY_ALLOCATION;
        }

        // 오디오 데이터 복사
        memcpy(chunk->audio_data,
               input_audio + processed_frames * context->config.channel_count,
               chunk_frames * context->config.channel_count * sizeof(float));

        chunk->timestamp = get_current_time();
        chunk->sequence_number = context->stats.total_chunks_processed;
        chunk->is_final = (processed_frames + chunk_frames >= input_length);

        // 버퍼에 추가
        context->chunk_buffers[context->buffer_write_index] = chunk;
        context->buffer_write_index = (context->buffer_write_index + 1) % context->config.buffer_count;
        context->buffer_count++;

        processed_frames += chunk_frames;
    }

    // 대기 중인 처리 스레드들에게 신호
    pthread_cond_broadcast((pthread_cond_t*)context->condition_var);

    pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);

    return ET_SUCCESS;
}

ETResult world_stream_pop_chunk(WorldStreamContext* context, WorldAudioChunk** chunk) {
    if (!context || !chunk) return ET_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock((pthread_mutex_t*)context->buffer_mutex);

    if (context->buffer_count == 0) {
        *chunk = NULL;
        pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);
        return ET_ERROR_BUFFER_EMPTY;
    }

    *chunk = context->chunk_buffers[context->buffer_read_index];
    context->chunk_buffers[context->buffer_read_index] = NULL;
    context->buffer_read_index = (context->buffer_read_index + 1) % context->config.buffer_count;
    context->buffer_count--;

    pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);

    return ET_SUCCESS;
}

// =============================================================================
// 버퍼 관리 함수들
// =============================================================================

int world_stream_get_buffer_level(WorldStreamContext* context) {
    if (!context) return 0;

    pthread_mutex_lock((pthread_mutex_t*)context->buffer_mutex);
    int level = context->buffer_count;
    pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);

    return level;
}

int world_stream_get_buffer_space(WorldStreamContext* context) {
    if (!context) return 0;

    pthread_mutex_lock((pthread_mutex_t*)context->buffer_mutex);
    int space = context->config.buffer_count - context->buffer_count;
    pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);

    return space;
}

ETResult world_stream_flush_buffers(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock((pthread_mutex_t*)context->buffer_mutex);

    // 모든 버퍼의 청크들 해제
    for (int i = 0; i < context->config.buffer_count; i++) {
        if (context->chunk_buffers[i]) {
            world_audio_chunk_destroy(context->chunk_buffers[i]);
            context->chunk_buffers[i] = NULL;
        }
    }

    context->buffer_read_index = 0;
    context->buffer_write_index = 0;
    context->buffer_count = 0;

    pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);

    return ET_SUCCESS;
}

ETResult world_stream_resize_buffers(WorldStreamContext* context, int new_buffer_count) {
    if (!context || new_buffer_count <= 0) return ET_ERROR_INVALID_PARAMETER;

    if (context->is_active) {
        return ET_ERROR_INVALID_STATE; // 스트리밍 중에는 크기 조정 불가
    }

    // 기존 버퍼 해제
    world_stream_deallocate_buffers(context);

    // 새 설정 적용
    context->config.buffer_count = new_buffer_count;

    // 새 버퍼 할당
    return world_stream_allocate_buffers(context);
}

// =============================================================================
// 상태 및 통계 함수들
// =============================================================================

WorldStreamState world_stream_get_state(WorldStreamContext* context) {
    if (!context) return WORLD_STREAM_STATE_ERROR;

    pthread_mutex_lock((pthread_mutex_t*)context->state_mutex);
    WorldStreamState state = context->state;
    pthread_mutex_unlock((pthread_mutex_t*)context->state_mutex);

    return state;
}

bool world_stream_is_active(WorldStreamContext* context) {
    return context ? context->is_active : false;
}

double world_stream_get_current_latency(WorldStreamContext* context) {
    if (!context) return 0.0;

    double current_time = get_current_time();
    double latency = (current_time - context->last_chunk_time) * 1000.0; // ms로 변환

    return latency;
}

const WorldStreamStats* world_stream_get_stats(WorldStreamContext* context) {
    if (!context) return NULL;

    world_stream_update_stats(context);
    return &context->stats;
}

ETResult world_stream_reset_stats(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    memset(&context->stats, 0, sizeof(WorldStreamStats));
    context->start_time = get_current_time();

    return ET_SUCCESS;
}

// =============================================================================
// 품질 적응 함수들
// =============================================================================

ETResult world_stream_set_quality_level(WorldStreamContext* context, float quality_level) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    if (quality_level < 0.0f || quality_level > 1.0f) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    context->current_quality_level = quality_level;
    return ET_SUCCESS;
}

float world_stream_get_quality_level(WorldStreamContext* context) {
    return context ? context->current_quality_level : 0.0f;
}

ETResult world_stream_enable_quality_adaptation(WorldStreamContext* context, bool enable) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    context->config.enable_quality_adaptation = enable;
    return ET_SUCCESS;
}

// =============================================================================
// 콜백 관리 함수들
// =============================================================================

ETResult world_stream_set_audio_callback(WorldStreamContext* context,
                                        WorldStreamAudioCallback callback,
                                        void* user_data) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    context->config.audio_callback = callback;
    context->config.callback_user_data = user_data;
    return ET_SUCCESS;
}

ETResult world_stream_set_progress_callback(WorldStreamContext* context,
                                           WorldStreamProgressCallback callback,
                                           void* user_data) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    context->config.progress_callback = callback;
    context->config.callback_user_data = user_data;
    return ET_SUCCESS;
}

ETResult world_stream_set_error_callback(WorldStreamContext* context,
                                        WorldStreamErrorCallback callback,
                                        void* user_data) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    context->config.error_callback = callback;
    context->config.callback_user_data = user_data;
    return ET_SUCCESS;
}

ETResult world_stream_set_state_callback(WorldStreamContext* context,
                                        WorldStreamStateCallback callback,
                                        void* user_data) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    context->config.state_callback = callback;
    context->config.callback_user_data = user_data;
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

static ETResult world_stream_set_state(WorldStreamContext* context, WorldStreamState new_state) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    pthread_mutex_lock((pthread_mutex_t*)context->state_mutex);

    WorldStreamState old_state = context->state;
    context->state = new_state;

    // 상태 변경 콜백 호출
    if (context->config.state_callback && old_state != new_state) {
        context->config.state_callback(old_state, new_state, context->config.callback_user_data);
    }

    pthread_mutex_unlock((pthread_mutex_t*)context->state_mutex);

    return ET_SUCCESS;
}

static ETResult world_stream_update_stats(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    double current_time = get_current_time();
    context->stats.total_processing_time = current_time - context->start_time;

    if (context->stats.total_chunks_processed > 0) {
        context->stats.average_chunk_time = context->stats.total_processing_time /
                                           context->stats.total_chunks_processed;
    }

    context->stats.current_latency_ms = world_stream_get_current_latency(context);
    context->stats.current_buffer_level = world_stream_get_buffer_level(context);

    if (context->stats.current_buffer_level > context->stats.max_buffer_level) {
        context->stats.max_buffer_level = context->stats.current_buffer_level;
    }

    if (context->stats.current_latency_ms > context->stats.max_latency_ms) {
        context->stats.max_latency_ms = context->stats.current_latency_ms;
    }

    return ET_SUCCESS;
}

static ETResult world_stream_check_quality_adaptation(WorldStreamContext* context) {
    if (!context || !context->config.enable_quality_adaptation) {
        return ET_SUCCESS;
    }

    double current_time = get_current_time();
    if (current_time - context->last_quality_check_time < 1.0) { // 1초마다 체크
        return ET_SUCCESS;
    }

    context->last_quality_check_time = current_time;

    // 지연 시간 기반 품질 적응
    double current_latency = world_stream_get_current_latency(context);
    if (current_latency > context->config.max_latency_ms) {
        // 지연 시간이 너무 높으면 품질 낮춤
        context->current_quality_level = fmax(0.1f, context->current_quality_level - 0.1f);
        context->stats.quality_adaptations++;
    } else if (current_latency < context->config.target_latency_ms) {
        // 지연 시간이 목표보다 낮으면 품질 높임
        context->current_quality_level = fmin(1.0f, context->current_quality_level + 0.05f);
        context->stats.quality_adaptations++;
    }

    context->stats.current_quality = context->current_quality_level;

    return ET_SUCCESS;
}

static void* world_stream_processing_thread(void* arg) {
    WorldStreamContext* context = (WorldStreamContext*)arg;
    if (!context) return NULL;

    while (!context->should_stop) {
        // 일시 정지 상태 처리
        if (context->state == WORLD_STREAM_STATE_PAUSED) {
            pthread_mutex_lock((pthread_mutex_t*)context->buffer_mutex);
            pthread_cond_wait((pthread_cond_t*)context->condition_var,
                             (pthread_mutex_t*)context->buffer_mutex);
            pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);
            continue;
        }

        // 청크 가져오기
        WorldAudioChunk* chunk = NULL;
        ETResult result = world_stream_pop_chunk(context, &chunk);

        if (result == ET_ERROR_BUFFER_EMPTY) {
            // 버퍼가 비어있으면 잠시 대기
            context->stats.buffer_underruns++;

            pthread_mutex_lock((pthread_mutex_t*)context->buffer_mutex);
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_nsec += (long)(context->config.buffer_timeout_ms * 1000000);
            if (timeout.tv_nsec >= 1000000000) {
                timeout.tv_sec += 1;
                timeout.tv_nsec -= 1000000000;
            }

            pthread_cond_timedwait((pthread_cond_t*)context->condition_var,
                                  (pthread_mutex_t*)context->buffer_mutex,
                                  &timeout);
            pthread_mutex_unlock((pthread_mutex_t*)context->buffer_mutex);
            continue;
        }

        if (result != ET_SUCCESS || !chunk) {
            continue;
        }

        // 청크 처리
        result = world_stream_process_chunk(context, chunk);
        if (result != ET_SUCCESS) {
            context->stats.total_errors++;
            if (context->config.error_callback) {
                context->config.error_callback(result, "청크 처리 실패",
                                             context->config.callback_user_data);
            }
        }

        // 통계 업데이트
        context->stats.total_chunks_processed++;
        context->stats.total_frames_processed += chunk->frame_count;
        context->last_chunk_time = get_current_time();

        // 품질 적응 체크
        world_stream_check_quality_adaptation(context);

        // 청크 해제
        world_audio_chunk_destroy(chunk);
    }

    return NULL;
}

static ETResult world_stream_process_chunk(WorldStreamContext* context, WorldAudioChunk* chunk) {
    if (!context || !chunk) return ET_ERROR_INVALID_PARAMETER;

    // 실제 WORLD 처리는 여기서 수행
    // 현재는 단순히 콜백 호출만 구현

    if (context->config.audio_callback) {
        context->config.audio_callback(chunk, context->config.callback_user_data);
    }

    return ET_SUCCESS;
}

static ETResult world_stream_allocate_buffers(WorldStreamContext* context) {
    if (!context) return ET_ERROR_INVALID_PARAMETER;

    context->chunk_buffers = (WorldAudioChunk**)calloc(context->config.buffer_count,
                                                       sizeof(WorldAudioChunk*));
    if (!context->chunk_buffers) {
        return ET_ERROR_MEMORY_ALLOCATION;
    }

    return ET_SUCCESS;
}

static void world_stream_deallocate_buffers(WorldStreamContext* context) {
    if (!context) return;

    if (context->chunk_buffers) {
        // 남은 청크들 해제
        for (int i = 0; i < context->config.buffer_count; i++) {
            if (context->chunk_buffers[i]) {
                world_audio_chunk_destroy(context->chunk_buffers[i]);
            }
        }

        free(context->chunk_buffers);
        context->chunk_buffers = NULL;
    }
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

bool world_stream_config_validate(const WorldStreamConfig* config) {
    if (!config) return false;

    if (config->chunk_size <= 0 || config->chunk_size > 8192) return false;
    if (config->buffer_count <= 0 || config->buffer_count > 64) return false;
    if (config->sample_rate <= 0 || config->sample_rate > 192000) return false;
    if (config->channel_count <= 0 || config->channel_count > 8) return false;
    if (config->target_latency_ms <= 0.0 || config->target_latency_ms > 1000.0) return false;
    if (config->max_latency_ms <= config->target_latency_ms) return false;
    if (config->quality_threshold < 0.0f || config->quality_threshold > 1.0f) return false;
    if (config->min_buffer_size <= 0 || config->min_buffer_size >= config->max_buffer_size) return false;
    if (config->processing_thread_count <= 0 || config->processing_thread_count > 16) return false;

    return true;
}

ETResult world_stream_config_copy(const WorldStreamConfig* src, WorldStreamConfig* dst) {
    if (!src || !dst) return ET_ERROR_INVALID_PARAMETER;

    memcpy(dst, src, sizeof(WorldStreamConfig));
    return ET_SUCCESS;
}

int world_stream_calculate_optimal_chunk_size(int sample_rate, double target_latency_ms) {
    if (sample_rate <= 0 || target_latency_ms <= 0.0) return 256; // 기본값

    int chunk_size = (int)(sample_rate * target_latency_ms / 1000.0);

    // 2의 거듭제곱으로 조정
    int power = 1;
    while (power < chunk_size) power *= 2;

    // 범위 제한
    if (power < 64) power = 64;
    if (power > 2048) power = 2048;

    return power;
}

int world_stream_calculate_optimal_buffer_count(int chunk_size,
                                               double target_latency_ms,
                                               double max_latency_ms) {
    if (chunk_size <= 0 || target_latency_ms <= 0.0 || max_latency_ms <= target_latency_ms) {
        return 8; // 기본값
    }

    double chunk_duration_ms = (double)chunk_size * 1000.0 / 44100.0; // 44.1kHz 기준
    int buffer_count = (int)(max_latency_ms / chunk_duration_ms);

    // 범위 제한
    if (buffer_count < 4) buffer_count = 4;
    if (buffer_count > 32) buffer_count = 32;

    return buffer_count;
}

// =============================================================================
// 디버깅 및 진단 함수들
// =============================================================================

ETResult world_stream_dump_state(WorldStreamContext* context, const char* filename) {
    if (!context || !filename) return ET_ERROR_INVALID_PARAMETER;

    FILE* file = fopen(filename, "w");
    if (!file) return ET_ERROR_FILE_IO;

    fprintf(file, "WORLD Streaming State Dump\n");
    fprintf(file, "==========================\n\n");

    fprintf(file, "State: %d\n", context->state);
    fprintf(file, "Active: %s\n", context->is_active ? "Yes" : "No");
    fprintf(file, "Should Stop: %s\n", context->should_stop ? "Yes" : "No");
    fprintf(file, "Buffer Level: %d/%d\n", context->buffer_count, context->config.buffer_count);
    fprintf(file, "Quality Level: %.2f\n", context->current_quality_level);

    fprintf(file, "\nConfiguration:\n");
    fprintf(file, "Mode: %d\n", context->config.mode);
    fprintf(file, "Chunk Size: %d\n", context->config.chunk_size);
    fprintf(file, "Sample Rate: %d\n", context->config.sample_rate);
    fprintf(file, "Target Latency: %.2f ms\n", context->config.target_latency_ms);
    fprintf(file, "Max Latency: %.2f ms\n", context->config.max_latency_ms);

    fprintf(file, "\nStatistics:\n");
    fprintf(file, "Total Chunks: %llu\n", context->stats.total_chunks_processed);
    fprintf(file, "Total Frames: %llu\n", context->stats.total_frames_processed);
    fprintf(file, "Current Latency: %.2f ms\n", context->stats.current_latency_ms);
    fprintf(file, "Buffer Underruns: %llu\n", context->stats.buffer_underruns);
    fprintf(file, "Buffer Overruns: %llu\n", context->stats.buffer_overruns);
    fprintf(file, "Total Errors: %llu\n", context->stats.total_errors);

    fclose(file);
    return ET_SUCCESS;
}

void world_stream_print_info(WorldStreamContext* context) {
    if (!context) {
        printf("Stream Context: NULL\n");
        return;
    }

    printf("WORLD Streaming Information\n");
    printf("===========================\n");
    printf("State: %d\n", context->state);
    printf("Active: %s\n", context->is_active ? "Yes" : "No");
    printf("Chunk Size: %d frames\n", context->config.chunk_size);
    printf("Buffer Count: %d\n", context->config.buffer_count);
    printf("Sample Rate: %d Hz\n", context->config.sample_rate);
    printf("Target Latency: %.2f ms\n", context->config.target_latency_ms);
    printf("Current Quality: %.2f\n", context->current_quality_level);
    printf("Buffer Level: %d/%d\n", context->buffer_count, context->config.buffer_count);
}

void world_stream_print_stats(WorldStreamContext* context) {
    if (!context) {
        printf("Stream Context: NULL\n");
        return;
    }

    world_stream_update_stats(context);

    printf("WORLD Streaming Statistics\n");
    printf("==========================\n");
    printf("Total Chunks Processed: %llu\n", context->stats.total_chunks_processed);
    printf("Total Frames Processed: %llu\n", context->stats.total_frames_processed);
    printf("Total Processing Time: %.3f seconds\n", context->stats.total_processing_time);
    printf("Average Chunk Time: %.6f seconds\n", context->stats.average_chunk_time);
    printf("Current Latency: %.2f ms\n", context->stats.current_latency_ms);
    printf("Average Latency: %.2f ms\n", context->stats.average_latency_ms);
    printf("Max Latency: %.2f ms\n", context->stats.max_latency_ms);
    printf("Buffer Underruns: %llu\n", context->stats.buffer_underruns);
    printf("Buffer Overruns: %llu\n", context->stats.buffer_overruns);
    printf("Quality Adaptations: %llu\n", context->stats.quality_adaptations);
    printf("Total Errors: %llu\n", context->stats.total_errors);
    printf("Dropped Chunks: %llu\n", context->stats.dropped_chunks);
}