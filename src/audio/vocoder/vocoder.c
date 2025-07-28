/**
 * @file vocoder.c
 * @brief LibEtude 보코더 인터페이스 구현
 *
 * 그래프 기반 보코더 통합, 실시간 최적화, 품질/성능 트레이드오프 조정을 제공합니다.
 */

#include "libetude/vocoder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>

// 내부 상수
#define ET_VOCODER_MAGIC 0x564F434F  // "VOCO"
#define ET_VOCODER_MAX_CHUNK_SIZE 8192
#define ET_VOCODER_MIN_CHUNK_SIZE 64
#define ET_VOCODER_DEFAULT_CHUNK_SIZE 512
#define ET_VOCODER_MAX_LOOKAHEAD 128
#define ET_VOCODER_DEFAULT_LOOKAHEAD 16

// 내부 함수 선언
static int et_vocoder_initialize_graph(ETVocoderContext* ctx);
static int et_vocoder_setup_buffers(ETVocoderContext* ctx);
static int et_vocoder_optimize_for_realtime(ETVocoderContext* ctx);
static int et_vocoder_apply_quality_settings(ETVocoderContext* ctx);
static uint64_t et_get_current_time_us(void);
static void et_vocoder_update_stats(ETVocoderContext* ctx, uint64_t processing_time_us,
                                   float quality_score);

/*
 * =============================================================================
 * 보코더 설정 및 생성 함수
 * =============================================================================
 */

ETVocoderConfig et_vocoder_default_config(void) {
    ETVocoderConfig config = {0};

    // 기본 오디오 설정
    config.sample_rate = 22050;
    config.mel_channels = 80;
    config.hop_length = 256;
    config.win_length = 1024;

    // 품질 및 성능 설정
    config.quality = ET_VOCODER_QUALITY_NORMAL;
    config.mode = ET_VOCODER_MODE_BATCH;
    config.opt_flags = ET_VOCODER_OPT_MEMORY | ET_VOCODER_OPT_SPEED | ET_VOCODER_OPT_SIMD;

    // 실시간 처리 설정
    config.chunk_size = ET_VOCODER_DEFAULT_CHUNK_SIZE;
    config.lookahead_frames = ET_VOCODER_DEFAULT_LOOKAHEAD;
    config.max_latency_ms = 100;

    // 메모리 설정
    config.buffer_size = 1024 * 1024; // 1MB
    config.use_memory_pool = true;

    // GPU 설정
    config.enable_gpu = false;
    config.gpu_device_id = 0;

    // 고급 설정
    config.quality_scale = 1.0f;
    config.speed_scale = 1.0f;
    config.enable_postfilter = true;
    config.enable_noise_shaping = false;

    return config;
}

ETVocoderContext* et_create_vocoder(const char* model_path, const ETVocoderConfig* config) {
    if (!model_path) {
        return NULL;
    }

    // 설정 검증
    ETVocoderConfig final_config = config ? *config : et_vocoder_default_config();
    if (!et_vocoder_validate_config(&final_config)) {
        return NULL;
    }

    // 컨텍스트 할당
    ETVocoderContext* ctx = (ETVocoderContext*)calloc(1, sizeof(ETVocoderContext));
    if (!ctx) {
        return NULL;
    }

    // 설정 복사
    ctx->config = final_config;

    // 뮤텍스 초기화
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        free(ctx);
        return NULL;
    }

    // 메모리 풀 생성
    if (ctx->config.use_memory_pool) {
        ctx->mem_pool = et_create_memory_pool(ctx->config.buffer_size, ET_DEFAULT_ALIGNMENT);
        if (!ctx->mem_pool) {
            pthread_mutex_destroy(&ctx->mutex);
            free(ctx);
            return NULL;
        }
    }

    // 모델 로드 (실제 구현에서는 LEF 모델 로드)
    // ctx->vocoder_model = lef_load_model(model_path);
    // 현재는 더미 구현
    ctx->vocoder_model = NULL;

    // 그래프 초기화
    if (et_vocoder_initialize_graph(ctx) != 0) {
        if (ctx->mem_pool) et_destroy_memory_pool(ctx->mem_pool);
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return NULL;
    }

    // 버퍼 설정
    if (et_vocoder_setup_buffers(ctx) != 0) {
        if (ctx->vocoder_graph) et_destroy_graph(ctx->vocoder_graph);
        if (ctx->mem_pool) et_destroy_memory_pool(ctx->mem_pool);
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return NULL;
    }

    // 품질 설정 적용
    et_vocoder_apply_quality_settings(ctx);

    // 실시간 모드 최적화
    if (ctx->config.mode == ET_VOCODER_MODE_REALTIME) {
        et_vocoder_optimize_for_realtime(ctx);
    }

    ctx->initialized = true;
    return ctx;
}

ETVocoderContext* et_create_vocoder_from_memory(const void* model_data, size_t model_size,
                                               const ETVocoderConfig* config) {
    if (!model_data || model_size == 0) {
        return NULL;
    }

    // 현재는 파일 기반과 동일한 구현
    // 실제로는 메모리에서 모델을 로드해야 함
    return et_create_vocoder("dummy_path", config);
}

void et_destroy_vocoder(ETVocoderContext* ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->mutex);

    // 스트리밍 중이면 중지
    if (ctx->is_streaming) {
        ctx->is_streaming = false;
    }

    // 버퍼 해제
    if (ctx->input_buffer) et_destroy_tensor(ctx->input_buffer);
    if (ctx->output_buffer) et_destroy_tensor(ctx->output_buffer);
    for (int i = 0; i < 4; i++) {
        if (ctx->temp_buffers[i]) et_destroy_tensor(ctx->temp_buffers[i]);
    }
    if (ctx->overlap_buffer) free(ctx->overlap_buffer);

    // 그래프 해제
    if (ctx->vocoder_graph) et_destroy_graph(ctx->vocoder_graph);

    // 모델 해제 (실제 구현에서는 LEF 모델 해제)
    // if (ctx->vocoder_model) lef_unload_model(ctx->vocoder_model);

    // 메모리 풀 해제
    if (ctx->mem_pool) et_destroy_memory_pool(ctx->mem_pool);

    pthread_mutex_unlock(&ctx->mutex);
    pthread_mutex_destroy(&ctx->mutex);

    free(ctx);
}

/*
 * =============================================================================
 * 보코더 추론 함수
 * =============================================================================
 */

int et_vocoder_mel_to_audio(ETVocoderContext* ctx, const ETTensor* mel_spec,
                           float* audio, int* audio_len) {
    if (!ctx || !ctx->initialized || !mel_spec || !audio || !audio_len) {
        return -1;
    }

    if (!et_validate_tensor(mel_spec)) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    uint64_t start_time = et_get_current_time_us();

    // 입력 텐서 검증
    if (mel_spec->ndim != 2 || mel_spec->shape[1] != (size_t)ctx->config.mel_channels) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    int time_frames = (int)mel_spec->shape[0];
    int expected_audio_len = time_frames * ctx->config.hop_length;

    if (*audio_len < expected_audio_len) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    // 더미 구현: 간단한 사인파 생성
    for (int i = 0; i < expected_audio_len && i < *audio_len; i++) {
        audio[i] = 0.1f * sinf(2.0f * M_PI * 440.0f * i / (float)ctx->config.sample_rate);
    }

    *audio_len = expected_audio_len < *audio_len ? expected_audio_len : *audio_len;

    uint64_t end_time = et_get_current_time_us();
    uint64_t processing_time = end_time - start_time;

    // 품질 점수 계산 (간단한 더미)
    float quality_score = 0.8f;

    // 통계 업데이트
    et_vocoder_update_stats(ctx, processing_time, quality_score);

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

ETTensor* et_vocoder_mel_to_audio_tensor(ETVocoderContext* ctx, const ETTensor* mel_spec,
                                        ETTensor* audio_tensor) {
    if (!ctx || !ctx->initialized || !mel_spec) {
        return NULL;
    }

    pthread_mutex_lock(&ctx->mutex);

    // 출력 텐서 크기 계산
    int time_frames = (int)mel_spec->shape[0];
    int audio_samples = time_frames * ctx->config.hop_length;

    // 출력 텐서 생성 또는 크기 조정
    if (!audio_tensor) {
        size_t audio_shape[] = {(size_t)audio_samples};
        audio_tensor = et_create_tensor(ctx->mem_pool, ET_FLOAT32, 1, audio_shape);
        if (!audio_tensor) {
            pthread_mutex_unlock(&ctx->mutex);
            return NULL;
        }
    } else if (audio_tensor->size < (size_t)audio_samples) {
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }

    // 오디오 변환
    float* audio_data = (float*)audio_tensor->data;
    int audio_len = audio_samples;

    int result = et_vocoder_mel_to_audio(ctx, mel_spec, audio_data, &audio_len);
    if (result != 0) {
        if (!audio_tensor) et_destroy_tensor(audio_tensor);
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }

    pthread_mutex_unlock(&ctx->mutex);
    return audio_tensor;
}

int et_vocoder_start_streaming(ETVocoderContext* ctx) {
    if (!ctx || !ctx->initialized) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    if (ctx->is_streaming) {
        pthread_mutex_unlock(&ctx->mutex);
        return 0; // 이미 스트리밍 중
    }

    // 스트리밍 모드로 설정 변경
    ctx->config.mode = ET_VOCODER_MODE_STREAMING;

    // 오버랩 버퍼 초기화
    if (!ctx->overlap_buffer) {
        ctx->overlap_size = ctx->config.hop_length;
        ctx->overlap_buffer = (float*)calloc(ctx->overlap_size, sizeof(float));
        if (!ctx->overlap_buffer) {
            pthread_mutex_unlock(&ctx->mutex);
            return -1;
        }
    } else {
        memset(ctx->overlap_buffer, 0, ctx->overlap_size * sizeof(float));
    }

    // 스트리밍 상태 초기화
    ctx->current_frame = 0;
    ctx->is_streaming = true;

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int et_vocoder_process_chunk(ETVocoderContext* ctx, const ETTensor* mel_chunk,
                            float* audio_chunk, int* chunk_len) {
    if (!ctx || !ctx->initialized || !mel_chunk || !audio_chunk || !chunk_len) {
        return -1;
    }

    if (!ctx->is_streaming) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    // 청크 크기 검증
    int chunk_frames = (int)mel_chunk->shape[0];
    if (chunk_frames > ctx->config.chunk_size) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    int expected_audio_len = chunk_frames * ctx->config.hop_length;
    if (*chunk_len < expected_audio_len) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
    }

    // 더미 구현: 간단한 사인파 생성
    for (int i = 0; i < expected_audio_len && i < *chunk_len; i++) {
        audio_chunk[i] = 0.1f * sinf(2.0f * M_PI * 440.0f * (ctx->current_frame * ctx->config.hop_length + i) / (float)ctx->config.sample_rate);
    }

    *chunk_len = expected_audio_len < *chunk_len ? expected_audio_len : *chunk_len;
    ctx->current_frame += chunk_frames;

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int et_vocoder_stop_streaming(ETVocoderContext* ctx, float* final_audio, int* final_len) {
    if (!ctx || !ctx->initialized) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    if (!ctx->is_streaming) {
        if (final_len) *final_len = 0;
        pthread_mutex_unlock(&ctx->mutex);
        return 0;
    }

    // 남은 오버랩 데이터 출력
    if (final_audio && final_len && ctx->overlap_buffer) {
        int output_samples = (int)ctx->overlap_size;
        if (output_samples > *final_len) output_samples = *final_len;

        memcpy(final_audio, ctx->overlap_buffer, output_samples * sizeof(float));
        *final_len = output_samples;
    } else if (final_len) {
        *final_len = 0;
    }

    // 스트리밍 상태 리셋
    ctx->is_streaming = false;
    ctx->current_frame = 0;

    if (ctx->overlap_buffer) {
        memset(ctx->overlap_buffer, 0, ctx->overlap_size * sizeof(float));
    }

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

/*
 * =============================================================================
 * 품질/성능 트레이드오프 조정 함수
 * =============================================================================
 */

int et_vocoder_set_quality(ETVocoderContext* ctx, ETVocoderQuality quality) {
    if (!ctx || !ctx->initialized) {
        return -1;
    }

    if (quality < ET_VOCODER_QUALITY_DRAFT || quality > ET_VOCODER_QUALITY_ULTRA) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    ctx->config.quality = quality;
    et_vocoder_apply_quality_settings(ctx);

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int et_vocoder_set_mode(ETVocoderContext* ctx, ETVocoderMode mode) {
    if (!ctx || !ctx->initialized) {
        return -1;
    }

    if (mode < ET_VOCODER_MODE_BATCH || mode > ET_VOCODER_MODE_REALTIME) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    ETVocoderMode old_mode = ctx->config.mode;
    ctx->config.mode = mode;

    // 모드 변경에 따른 최적화
    if (mode == ET_VOCODER_MODE_REALTIME && old_mode != ET_VOCODER_MODE_REALTIME) {
        et_vocoder_optimize_for_realtime(ctx);
    }

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int et_vocoder_set_optimization(ETVocoderContext* ctx, ETVocoderOptFlags opt_flags) {
    if (!ctx || !ctx->initialized) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    ctx->config.opt_flags = opt_flags;

    // 최적화 플래그에 따른 그래프 재최적화
    if (ctx->vocoder_graph) {
        ETOptimizationFlags graph_flags = ET_OPT_NONE;

        if (opt_flags & ET_VOCODER_OPT_MEMORY) {
            graph_flags |= ET_OPT_MEMORY_OPTIMIZATION;
        }
        if (opt_flags & ET_VOCODER_OPT_SPEED) {
            graph_flags |= ET_OPT_OPERATOR_FUSION;
        }

        et_optimize_graph(ctx->vocoder_graph, graph_flags);
    }

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

int et_vocoder_balance_quality_speed(ETVocoderContext* ctx, float quality_weight, float speed_weight) {
    if (!ctx || !ctx->initialized) {
        return -1;
    }

    if (quality_weight < 0.0f || quality_weight > 1.0f ||
        speed_weight < 0.0f || speed_weight > 1.0f) {
        return -1;
    }

    pthread_mutex_lock(&ctx->mutex);

    // 가중치에 따른 품질 및 속도 스케일 조정
    ctx->config.quality_scale = 0.5f + 0.5f * quality_weight;
    ctx->config.speed_scale = 0.5f + 0.5f * speed_weight;

    // 품질 모드 자동 선택
    float total_weight = quality_weight + speed_weight;
    if (total_weight > 0.0f) {
        float quality_ratio = quality_weight / total_weight;

        if (quality_ratio < 0.25f) {
            ctx->config.quality = ET_VOCODER_QUALITY_DRAFT;
        } else if (quality_ratio < 0.5f) {
            ctx->config.quality = ET_VOCODER_QUALITY_NORMAL;
        } else if (quality_ratio < 0.75f) {
            ctx->config.quality = ET_VOCODER_QUALITY_HIGH;
        } else {
            ctx->config.quality = ET_VOCODER_QUALITY_ULTRA;
        }
    }

    et_vocoder_apply_quality_settings(ctx);

    pthread_mutex_unlock(&ctx->mutex);
    return 0;
}

/*
 * =============================================================================
 * 내부 구현 함수
 * =============================================================================
 */

static int et_vocoder_initialize_graph(ETVocoderContext* ctx) {
    // 그래프 생성
    ctx->vocoder_graph = et_create_graph(64); // 초기 용량 64개 노드
    if (!ctx->vocoder_graph) {
        return -1;
    }

    // TODO: 실제 보코더 모델에서 그래프 구성 로드
    // 현재는 간단한 더미 그래프 생성

    return 0;
}

static int et_vocoder_setup_buffers(ETVocoderContext* ctx) {
    // 입력 버퍼 생성 (Mel 스펙트로그램용)
    size_t input_shape[] = {(size_t)ctx->config.chunk_size, (size_t)ctx->config.mel_channels};
    ctx->input_buffer = et_create_tensor(ctx->mem_pool, ET_FLOAT32, 2, input_shape);
    if (!ctx->input_buffer) {
        return -1;
    }

    // 출력 버퍼 생성 (오디오용)
    size_t output_shape[] = {(size_t)(ctx->config.chunk_size * ctx->config.hop_length)};
    ctx->output_buffer = et_create_tensor(ctx->mem_pool, ET_FLOAT32, 1, output_shape);
    if (!ctx->output_buffer) {
        return -1;
    }

    // 임시 버퍼들 생성
    for (int i = 0; i < 4; i++) {
        size_t temp_shape[] = {(size_t)(ctx->config.chunk_size * ctx->config.mel_channels)};
        ctx->temp_buffers[i] = et_create_tensor(ctx->mem_pool, ET_FLOAT32, 1, temp_shape);
        if (!ctx->temp_buffers[i]) {
            return -1;
        }
    }

    return 0;
}

static int et_vocoder_optimize_for_realtime(ETVocoderContext* ctx) {
    // 실시간 처리를 위한 최적화

    // 청크 크기 최적화
    int optimal_chunk = et_vocoder_optimize_chunk_size(ctx, ctx->config.max_latency_ms);
    if (optimal_chunk > 0) {
        ctx->config.chunk_size = optimal_chunk;
    }

    // 미리보기 프레임 최소화
    ctx->config.lookahead_frames = ET_VOCODER_DEFAULT_LOOKAHEAD / 2;

    // 후처리 비활성화 (성능 우선)
    if (ctx->config.opt_flags & ET_VOCODER_OPT_SPEED) {
        ctx->config.enable_postfilter = false;
        ctx->config.enable_noise_shaping = false;
    }

    return 0;
}

static int et_vocoder_apply_quality_settings(ETVocoderContext* ctx) {
    switch (ctx->config.quality) {
        case ET_VOCODER_QUALITY_DRAFT:
            ctx->config.enable_postfilter = false;
            ctx->config.enable_noise_shaping = false;
            ctx->config.quality_scale = 0.7f;
            break;

        case ET_VOCODER_QUALITY_NORMAL:
            ctx->config.enable_postfilter = true;
            ctx->config.enable_noise_shaping = false;
            ctx->config.quality_scale = 1.0f;
            break;

        case ET_VOCODER_QUALITY_HIGH:
            ctx->config.enable_postfilter = true;
            ctx->config.enable_noise_shaping = true;
            ctx->config.quality_scale = 1.3f;
            break;

        case ET_VOCODER_QUALITY_ULTRA:
            ctx->config.enable_postfilter = true;
            ctx->config.enable_noise_shaping = true;
            ctx->config.quality_scale = 1.5f;
            break;
    }

    return 0;
}

static uint64_t et_get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void et_vocoder_update_stats(ETVocoderContext* ctx, uint64_t processing_time_us,
                                   float quality_score) {
    ctx->total_frames_processed++;
    ctx->total_processing_time_us += processing_time_us;

    float processing_time_ms = (float)processing_time_us / 1000.0f;

    // 평균 처리 시간 업데이트
    ctx->avg_processing_time_ms = (ctx->avg_processing_time_ms * (ctx->total_frames_processed - 1) +
                                  processing_time_ms) / ctx->total_frames_processed;

    // 최대 처리 시간 업데이트
    if (processing_time_ms > ctx->peak_processing_time_ms) {
        ctx->peak_processing_time_ms = processing_time_ms;
    }

    // 품질 점수 업데이트
    ctx->current_quality_score = quality_score;
    ctx->avg_quality_score = (ctx->avg_quality_score * (ctx->total_frames_processed - 1) +
                             quality_score) / ctx->total_frames_processed;
}

/*
 * =============================================================================
 * 성능 모니터링 및 통계 함수
 * =============================================================================
 */

void et_vocoder_get_stats(ETVocoderContext* ctx, ETVocoderStats* stats) {
    if (!ctx || !stats) return;

    pthread_mutex_lock(&ctx->mutex);

    memset(stats, 0, sizeof(ETVocoderStats));

    stats->frames_processed = ctx->total_frames_processed;
    stats->total_processing_time_us = ctx->total_processing_time_us;
    stats->avg_processing_time_ms = ctx->avg_processing_time_ms;
    stats->peak_processing_time_ms = ctx->peak_processing_time_ms;
    stats->min_processing_time_ms = 0.0f; // TODO: 최소값 추적 구현

    // 실시간 팩터 계산
    if (ctx->total_frames_processed > 0) {
        float total_audio_duration_ms = (float)(ctx->total_frames_processed * ctx->config.hop_length) /
                                       (float)ctx->config.sample_rate * 1000.0f;
        float total_processing_time_ms = (float)ctx->total_processing_time_us / 1000.0f;
        stats->realtime_factor = total_audio_duration_ms / total_processing_time_ms;
    } else {
        stats->realtime_factor = 0.0f;
    }

    stats->avg_quality_score = ctx->avg_quality_score;
    stats->min_quality_score = 0.0f; // TODO: 최소/최대 품질 점수 추적 구현
    stats->max_quality_score = 1.0f;

    // 메모리 사용량 (메모리 풀에서 조회)
    if (ctx->mem_pool) {
        ETMemoryPoolStats pool_stats;
        et_get_pool_stats(ctx->mem_pool, &pool_stats);
        stats->peak_memory_usage = pool_stats.peak_usage;
        stats->current_memory_usage = pool_stats.used_size;
    }

    stats->num_errors = 0; // TODO: 오류 카운터 구현
    stats->num_warnings = 0; // TODO: 경고 카운터 구현

    pthread_mutex_unlock(&ctx->mutex);
}

float et_vocoder_compute_quality_score(ETVocoderContext* ctx, const float* reference_audio,
                                      const float* generated_audio, int audio_len) {
    if (!ctx || !generated_audio || audio_len <= 0) {
        return 0.0f;
    }

    // 간단한 품질 메트릭 (SNR 기반)
    float signal_power = 0.0f;
    float noise_power = 0.0f;

    for (int i = 0; i < audio_len; i++) {
        float sample = generated_audio[i];
        signal_power += sample * sample;

        // 간단한 노이즈 추정 (고주파 성분)
        if (i > 0) {
            float diff = sample - generated_audio[i-1];
            noise_power += diff * diff;
        }
    }

    signal_power /= (float)audio_len;
    noise_power /= (float)(audio_len - 1);

    if (noise_power > 0.0f) {
        float snr = 10.0f * log10f(signal_power / noise_power);
        // SNR을 0.0 ~ 1.0 범위로 정규화
        return fmaxf(0.0f, fminf(1.0f, (snr + 10.0f) / 50.0f));
    }

    return 0.8f; // 기본값
}

float et_vocoder_get_realtime_factor(ETVocoderContext* ctx) {
    if (!ctx) return 0.0f;

    ETVocoderStats stats;
    et_vocoder_get_stats(ctx, &stats);
    return stats.realtime_factor;
}

void et_vocoder_reset_stats(ETVocoderContext* ctx) {
    if (!ctx) return;

    pthread_mutex_lock(&ctx->mutex);

    ctx->total_frames_processed = 0;
    ctx->total_processing_time_us = 0;
    ctx->avg_processing_time_ms = 0.0f;
    ctx->peak_processing_time_ms = 0.0f;
    ctx->current_quality_score = 0.0f;
    ctx->avg_quality_score = 0.0f;

    pthread_mutex_unlock(&ctx->mutex);
}

/*
 * =============================================================================
 * 유틸리티 함수
 * =============================================================================
 */

bool et_vocoder_validate_config(const ETVocoderConfig* config) {
    if (!config) return false;

    // 기본 오디오 설정 검증
    if (config->sample_rate <= 0 || config->sample_rate > 96000) return false;
    if (config->mel_channels <= 0 || config->mel_channels > 512) return false;
    if (config->hop_length <= 0 || config->hop_length > 2048) return false;
    if (config->win_length <= 0 || config->win_length > 4096) return false;

    // 품질 및 모드 검증
    if (config->quality < ET_VOCODER_QUALITY_DRAFT || config->quality > ET_VOCODER_QUALITY_ULTRA) return false;
    if (config->mode < ET_VOCODER_MODE_BATCH || config->mode > ET_VOCODER_MODE_REALTIME) return false;

    // 실시간 설정 검증
    if (config->chunk_size < ET_VOCODER_MIN_CHUNK_SIZE || config->chunk_size > ET_VOCODER_MAX_CHUNK_SIZE) return false;
    if (config->lookahead_frames < 0 || config->lookahead_frames > ET_VOCODER_MAX_LOOKAHEAD) return false;
    if (config->max_latency_ms <= 0 || config->max_latency_ms > 10000) return false;

    // 메모리 설정 검증
    if (config->buffer_size < 1024 || config->buffer_size > 1024*1024*1024) return false;

    // 스케일 검증
    if (config->quality_scale < 0.1f || config->quality_scale > 2.0f) return false;
    if (config->speed_scale < 0.5f || config->speed_scale > 2.0f) return false;

    return true;
}

bool et_vocoder_validate_context(const ETVocoderContext* ctx) {
    if (!ctx) return false;
    if (!ctx->initialized) return false;
    if (!ctx->vocoder_graph) return false;
    if (!ctx->input_buffer || !ctx->output_buffer) return false;

    return et_vocoder_validate_config(&ctx->config);
}

int et_vocoder_optimize_chunk_size(ETVocoderContext* ctx, int target_latency_ms) {
    if (!ctx || target_latency_ms <= 0) return -1;

    // 목표 지연 시간에 맞는 청크 크기 계산
    float target_latency_sec = (float)target_latency_ms / 1000.0f;
    int target_samples = (int)(target_latency_sec * ctx->config.sample_rate);
    int target_frames = target_samples / ctx->config.hop_length;

    // 최소/최대 범위 내로 제한
    if (target_frames < ET_VOCODER_MIN_CHUNK_SIZE) {
        target_frames = ET_VOCODER_MIN_CHUNK_SIZE;
    } else if (target_frames > ET_VOCODER_MAX_CHUNK_SIZE) {
        target_frames = ET_VOCODER_MAX_CHUNK_SIZE;
    }

    return target_frames;
}

int et_vocoder_compute_recommended_config(int sample_rate, int target_latency_ms,
                                         float quality_preference, ETVocoderConfig* config) {
    if (!config || sample_rate <= 0 || target_latency_ms <= 0) return -1;

    *config = et_vocoder_default_config();
    config->sample_rate = sample_rate;
    config->max_latency_ms = target_latency_ms;

    // 품질 선호도에 따른 설정 조정
    if (quality_preference < 0.3f) {
        config->quality = ET_VOCODER_QUALITY_DRAFT;
        config->mode = ET_VOCODER_MODE_REALTIME;
        config->opt_flags = ET_VOCODER_OPT_SPEED | ET_VOCODER_OPT_MEMORY;
    } else if (quality_preference < 0.7f) {
        config->quality = ET_VOCODER_QUALITY_NORMAL;
        config->mode = ET_VOCODER_MODE_STREAMING;
        config->opt_flags = ET_VOCODER_OPT_SPEED | ET_VOCODER_OPT_MEMORY | ET_VOCODER_OPT_SIMD;
    } else {
        config->quality = ET_VOCODER_QUALITY_HIGH;
        config->mode = ET_VOCODER_MODE_BATCH;
        config->opt_flags = ET_VOCODER_OPT_QUALITY | ET_VOCODER_OPT_SIMD;
    }

    return 0;
}

size_t et_vocoder_estimate_memory_usage(const ETVocoderConfig* config) {
    if (!config) return 0;

    size_t base_memory = sizeof(ETVocoderContext);
    size_t buffer_memory = config->buffer_size;
    size_t tensor_memory = config->chunk_size * config->mel_channels * sizeof(float) * 6; // 입력, 출력, 임시 버퍼들
    size_t overlap_memory = config->hop_length * sizeof(float);

    return base_memory + buffer_memory + tensor_memory + overlap_memory;
}

uint64_t et_vocoder_estimate_processing_time(const ETVocoderConfig* config, int mel_frames) {
    if (!config || mel_frames <= 0) return 0;

    // 간단한 추정 공식 (실제로는 하드웨어와 모델에 따라 달라짐)
    uint64_t base_time_us = 1000; // 기본 오버헤드
    uint64_t per_frame_time_us = 100; // 프레임당 처리 시간

    // 품질 모드에 따른 조정
    float quality_factor = 1.0f;
    switch (config->quality) {
        case ET_VOCODER_QUALITY_DRAFT: quality_factor = 0.7f; break;
        case ET_VOCODER_QUALITY_NORMAL: quality_factor = 1.0f; break;
        case ET_VOCODER_QUALITY_HIGH: quality_factor = 1.5f; break;
        case ET_VOCODER_QUALITY_ULTRA: quality_factor = 2.0f; break;
    }

    return base_time_us + (uint64_t)(per_frame_time_us * mel_frames * quality_factor);
}

void et_vocoder_print_info(const ETVocoderContext* ctx) {
    if (!ctx) return;

    printf("=== LibEtude Vocoder Information ===\n");
    printf("Sample Rate: %d Hz\n", ctx->config.sample_rate);
    printf("Mel Channels: %d\n", ctx->config.mel_channels);
    printf("Hop Length: %d\n", ctx->config.hop_length);
    printf("Quality Mode: %d\n", ctx->config.quality);
    printf("Execution Mode: %d\n", ctx->config.mode);
    printf("Chunk Size: %d frames\n", ctx->config.chunk_size);
    printf("Max Latency: %d ms\n", ctx->config.max_latency_ms);
    printf("Streaming: %s\n", ctx->is_streaming ? "Yes" : "No");
    printf("Frames Processed: %llu\n", (unsigned long long)ctx->total_frames_processed);
    printf("Avg Processing Time: %.2f ms\n", ctx->avg_processing_time_ms);
    printf("Peak Processing Time: %.2f ms\n", ctx->peak_processing_time_ms);
    printf("Avg Quality Score: %.3f\n", ctx->avg_quality_score);
    printf("=====================================\n");
}

void et_vocoder_print_performance_report(const ETVocoderContext* ctx, const char* output_file) {
    if (!ctx) return;

    FILE* file = output_file ? fopen(output_file, "w") : stdout;
    if (!file) return;

    ETVocoderStats stats;
    et_vocoder_get_stats((ETVocoderContext*)ctx, &stats);

    fprintf(file, "=== LibEtude Vocoder Performance Report ===\n");
    fprintf(file, "Frames Processed: %llu\n", (unsigned long long)stats.frames_processed);
    fprintf(file, "Total Processing Time: %.2f ms\n", (float)stats.total_processing_time_us / 1000.0f);
    fprintf(file, "Average Processing Time: %.2f ms\n", stats.avg_processing_time_ms);
    fprintf(file, "Peak Processing Time: %.2f ms\n", stats.peak_processing_time_ms);
    fprintf(file, "Realtime Factor: %.2f\n", stats.realtime_factor);
    fprintf(file, "Average Quality Score: %.3f\n", stats.avg_quality_score);
    fprintf(file, "Peak Memory Usage: %zu bytes\n", stats.peak_memory_usage);
    fprintf(file, "Current Memory Usage: %zu bytes\n", stats.current_memory_usage);
    fprintf(file, "Errors: %u\n", stats.num_errors);
    fprintf(file, "Warnings: %u\n", stats.num_warnings);
    fprintf(file, "==========================================\n");

    if (output_file) fclose(file);
}