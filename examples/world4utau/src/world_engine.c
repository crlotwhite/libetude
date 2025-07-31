/**
 * @file world_engine.c
 * @brief WORLD 알고리즘 엔진 기본 구현
 */

#include "world_engine.h"
#include <libetude/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 기본값 정의
#define DEFAULT_FRAME_PERIOD 5.0
#define DEFAULT_F0_FLOOR 71.0
#define DEFAULT_F0_CEIL 800.0
#define DEFAULT_CHANNELS_IN_OCTAVE 2.0
#define DEFAULT_SPEED 1.0
#define DEFAULT_ALLOWED_RANGE 0.1
#define DEFAULT_Q1 -0.15
#define DEFAULT_THRESHOLD 0.85
#define DEFAULT_MEMORY_POOL_SIZE (1024 * 1024)  // 1MB

// ============================================================================
// WorldParameters 관리 함수들
// ============================================================================

WorldParameters* world_parameters_create(int f0_length, int fft_size, ETMemoryPool* pool) {
    if (f0_length <= 0 || fft_size <= 0) {
        return NULL;
    }

    WorldParameters* params = NULL;

    if (pool) {
        params = (WorldParameters*)et_memory_pool_alloc(pool, sizeof(WorldParameters));
    } else {
        params = (WorldParameters*)malloc(sizeof(WorldParameters));
    }

    if (!params) {
        return NULL;
    }

    memset(params, 0, sizeof(WorldParameters));

    // 기본 정보 설정
    params->f0_length = f0_length;
    params->fft_size = fft_size;
    params->mem_pool = pool;
    params->owns_memory = true;

    // 메모리 할당
    size_t f0_size = f0_length * sizeof(double);
    size_t spectrum_size = f0_length * (fft_size / 2 + 1) * sizeof(double);

    if (pool) {
        params->f0 = (double*)et_memory_pool_alloc(pool, f0_size);
        params->time_axis = (double*)et_memory_pool_alloc(pool, f0_size);

        // 2D 배열 할당
        params->spectrogram = (double**)et_memory_pool_alloc(pool, f0_length * sizeof(double*));
        params->aperiodicity = (double**)et_memory_pool_alloc(pool, f0_length * sizeof(double*));

        if (params->spectrogram && params->aperiodicity) {
            double* spectrum_data = (double*)et_memory_pool_alloc(pool, spectrum_size);
            double* aperiodicity_data = (double*)et_memory_pool_alloc(pool, spectrum_size);

            if (spectrum_data && aperiodicity_data) {
                for (int i = 0; i < f0_length; i++) {
                    params->spectrogram[i] = spectrum_data + i * (fft_size / 2 + 1);
                    params->aperiodicity[i] = aperiodicity_data + i * (fft_size / 2 + 1);
                }
            }
        }
    } else {
        params->f0 = (double*)malloc(f0_size);
        params->time_axis = (double*)malloc(f0_size);

        // 2D 배열 할당
        params->spectrogram = (double**)malloc(f0_length * sizeof(double*));
        params->aperiodicity = (double**)malloc(f0_length * sizeof(double*));

        if (params->spectrogram && params->aperiodicity) {
            double* spectrum_data = (double*)malloc(spectrum_size);
            double* aperiodicity_data = (double*)malloc(spectrum_size);

            if (spectrum_data && aperiodicity_data) {
                for (int i = 0; i < f0_length; i++) {
                    params->spectrogram[i] = spectrum_data + i * (fft_size / 2 + 1);
                    params->aperiodicity[i] = aperiodicity_data + i * (fft_size / 2 + 1);
                }
            }
        }
    }

    // 할당 실패 확인
    if (!params->f0 || !params->time_axis || !params->spectrogram || !params->aperiodicity) {
        world_parameters_destroy(params);
        return NULL;
    }

    return params;
}

void world_parameters_destroy(WorldParameters* params) {
    if (!params) {
        return;
    }

    if (params->owns_memory && !params->mem_pool) {
        // 일반 malloc으로 할당된 경우
        free(params->f0);
        free(params->time_axis);

        if (params->spectrogram) {
            free(params->spectrogram[0]);  // 연속 할당된 데이터
            free(params->spectrogram);
        }

        if (params->aperiodicity) {
            free(params->aperiodicity[0]);  // 연속 할당된 데이터
            free(params->aperiodicity);
        }

        free(params);
    }
    // 메모리 풀로 할당된 경우는 풀 해제시 자동으로 해제됨
}

ETResult world_parameters_copy(const WorldParameters* src, WorldParameters* dst) {
    if (!src || !dst) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (dst->f0_length != src->f0_length || dst->fft_size != src->fft_size) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 기본 정보 복사
    dst->sample_rate = src->sample_rate;
    dst->audio_length = src->audio_length;
    dst->frame_period = src->frame_period;

    // 배열 데이터 복사
    memcpy(dst->f0, src->f0, src->f0_length * sizeof(double));
    memcpy(dst->time_axis, src->time_axis, src->f0_length * sizeof(double));

    size_t spectrum_row_size = (src->fft_size / 2 + 1) * sizeof(double);
    for (int i = 0; i < src->f0_length; i++) {
        memcpy(dst->spectrogram[i], src->spectrogram[i], spectrum_row_size);
        memcpy(dst->aperiodicity[i], src->aperiodicity[i], spectrum_row_size);
    }

    return ET_SUCCESS;
}

ETResult world_parameters_init(WorldParameters* params, int sample_rate,
                              int audio_length, double frame_period) {
    if (!params) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    params->sample_rate = sample_rate;
    params->audio_length = audio_length;
    params->frame_period = frame_period;

    // 시간축 초기화
    for (int i = 0; i < params->f0_length; i++) {
        params->time_axis[i] = i * frame_period / 1000.0;  // ms를 초로 변환
    }

    return ET_SUCCESS;
}

// ============================================================================
// WORLD 분석 엔진 함수들
// ============================================================================

WorldAnalysisEngine* world_analysis_create(const WorldAnalysisConfig* config) {
    if (!config) {
        return NULL;
    }

    WorldAnalysisEngine* engine = (WorldAnalysisEngine*)malloc(sizeof(WorldAnalysisEngine));
    if (!engine) {
        return NULL;
    }

    memset(engine, 0, sizeof(WorldAnalysisEngine));

    // 설정 복사
    engine->config = *config;

    // 메모리 풀 생성
    engine->mem_pool = et_memory_pool_create(config->memory_pool_size);
    if (!engine->mem_pool) {
        free(engine);
        return NULL;
    }

    // STFT 컨텍스트 생성 (libetude 통합)
    ETSTFTConfig stft_config = {0};
    stft_config.fft_size = config->spectrum_config.fft_size;
    stft_config.hop_size = stft_config.fft_size / 4;  // 기본값
    stft_config.window_type = ET_WINDOW_HANN;

    engine->stft_ctx = et_stft_create(&stft_config);
    if (!engine->stft_ctx) {
        et_memory_pool_destroy(engine->mem_pool);
        free(engine);
        return NULL;
    }

    // 작업용 버퍼 할당
    engine->work_buffer_size = 1024 * 1024;  // 1MB
    engine->work_buffer = (double*)et_memory_pool_alloc(engine->mem_pool, engine->work_buffer_size);
    if (!engine->work_buffer) {
        et_stft_destroy(engine->stft_ctx);
        et_memory_pool_destroy(engine->mem_pool);
        free(engine);
        return NULL;
    }

    engine->is_initialized = true;
    return engine;
}

void world_analysis_destroy(WorldAnalysisEngine* engine) {
    if (!engine) {
        return;
    }

    if (engine->stft_ctx) {
        et_stft_destroy(engine->stft_ctx);
    }

    if (engine->mem_pool) {
        et_memory_pool_destroy(engine->mem_pool);
    }

    free(engine);
}

ETResult world_analyze_audio(WorldAnalysisEngine* engine,
                            const float* audio, int audio_length, int sample_rate,
                            WorldParameters* params) {
    if (!engine || !audio || !params || audio_length <= 0 || sample_rate <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!engine->is_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    // WorldParameters 초기화
    ETResult result = world_parameters_init(params, sample_rate, audio_length,
                                           engine->config.f0_config.frame_period);
    if (result != ET_SUCCESS) {
        return result;
    }

    // F0 추출
    result = world_extract_f0(engine, audio, audio_length, sample_rate,
                             params->f0, params->time_axis, params->f0_length);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 스펙트럼 분석
    result = world_analyze_spectrum(engine, audio, audio_length, sample_rate,
                                   params->f0, params->time_axis, params->f0_length,
                                   params->spectrogram);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 비주기성 분석
    result = world_analyze_aperiodicity(engine, audio, audio_length, sample_rate,
                                       params->f0, params->time_axis, params->f0_length,
                                       params->aperiodicity);
    if (result != ET_SUCCESS) {
        return result;
    }

    engine->last_sample_rate = sample_rate;
    return ET_SUCCESS;
}

// 임시 구현 - 실제 WORLD 알고리즘은 후속 작업에서 구현
ETResult world_extract_f0(WorldAnalysisEngine* engine,
                         const float* audio, int audio_length, int sample_rate,
                         double* f0, double* time_axis, int f0_length) {
    // TODO: 실제 DIO/Harvest 알고리즘 구현
    // 현재는 더미 구현
    for (int i = 0; i < f0_length; i++) {
        f0[i] = 0.0;  // 무음성 구간으로 초기화
        time_axis[i] = i * engine->config.f0_config.frame_period / 1000.0;
    }
    return ET_SUCCESS;
}

ETResult world_analyze_spectrum(WorldAnalysisEngine* engine,
                               const float* audio, int audio_length, int sample_rate,
                               const double* f0, const double* time_axis, int f0_length,
                               double** spectrogram) {
    // TODO: 실제 CheapTrick 알고리즘 구현
    // 현재는 더미 구현
    int fft_size = engine->config.spectrum_config.fft_size;
    for (int i = 0; i < f0_length; i++) {
        for (int j = 0; j < fft_size / 2 + 1; j++) {
            spectrogram[i][j] = 0.0;
        }
    }
    return ET_SUCCESS;
}

ETResult world_analyze_aperiodicity(WorldAnalysisEngine* engine,
                                   const float* audio, int audio_length, int sample_rate,
                                   const double* f0, const double* time_axis, int f0_length,
                                   double** aperiodicity) {
    // TODO: 실제 D4C 알고리즘 구현
    // 현재는 더미 구현
    int fft_size = engine->config.spectrum_config.fft_size;
    for (int i = 0; i < f0_length; i++) {
        for (int j = 0; j < fft_size / 2 + 1; j++) {
            aperiodicity[i][j] = engine->config.aperiodicity_config.threshold;
        }
    }
    return ET_SUCCESS;
}

// ============================================================================
// WORLD 합성 엔진 함수들
// ============================================================================

WorldSynthesisEngine* world_synthesis_create(const WorldSynthesisConfig* config) {
    if (!config) {
        return NULL;
    }

    WorldSynthesisEngine* engine = (WorldSynthesisEngine*)malloc(sizeof(WorldSynthesisEngine));
    if (!engine) {
        return NULL;
    }

    memset(engine, 0, sizeof(WorldSynthesisEngine));

    // 설정 복사
    engine->config = *config;

    // 메모리 풀 생성
    engine->mem_pool = et_memory_pool_create(config->memory_pool_size);
    if (!engine->mem_pool) {
        free(engine);
        return NULL;
    }

    // 보코더 컨텍스트 생성 (libetude 통합)
    ETVocoderConfig vocoder_config = {0};
    vocoder_config.sample_rate = config->sample_rate;
    vocoder_config.frame_size = (int)(config->sample_rate * config->frame_period / 1000.0);

    engine->vocoder_ctx = et_vocoder_create(&vocoder_config);
    if (!engine->vocoder_ctx) {
        et_memory_pool_destroy(engine->mem_pool);
        free(engine);
        return NULL;
    }

    // 출력 버퍼 생성
    engine->output_buffer = et_audio_buffer_create(config->sample_rate, 2, 1024);
    if (!engine->output_buffer) {
        et_vocoder_destroy(engine->vocoder_ctx);
        et_memory_pool_destroy(engine->mem_pool);
        free(engine);
        return NULL;
    }

    // 합성용 버퍼 할당
    engine->synthesis_buffer_size = 1024 * 1024;  // 1MB
    engine->synthesis_buffer = (double*)et_memory_pool_alloc(engine->mem_pool,
                                                            engine->synthesis_buffer_size);
    if (!engine->synthesis_buffer) {
        et_audio_buffer_destroy(engine->output_buffer);
        et_vocoder_destroy(engine->vocoder_ctx);
        et_memory_pool_destroy(engine->mem_pool);
        free(engine);
        return NULL;
    }

    engine->is_initialized = true;
    return engine;
}

void world_synthesis_destroy(WorldSynthesisEngine* engine) {
    if (!engine) {
        return;
    }

    if (engine->output_buffer) {
        et_audio_buffer_destroy(engine->output_buffer);
    }

    if (engine->vocoder_ctx) {
        et_vocoder_destroy(engine->vocoder_ctx);
    }

    if (engine->mem_pool) {
        et_memory_pool_destroy(engine->mem_pool);
    }

    free(engine);
}

ETResult world_synthesize_audio(WorldSynthesisEngine* engine,
                               const WorldParameters* params,
                               float* output_audio, int* output_length) {
    if (!engine || !params || !output_audio || !output_length) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!engine->is_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    // TODO: 실제 WORLD 합성 알고리즘 구현
    // 현재는 더미 구현 (무음 출력)
    int samples_to_generate = params->audio_length;
    if (samples_to_generate > *output_length) {
        samples_to_generate = *output_length;
    }

    memset(output_audio, 0, samples_to_generate * sizeof(float));
    *output_length = samples_to_generate;

    return ET_SUCCESS;
}

ETResult world_synthesize_streaming(WorldSynthesisEngine* engine,
                                   const WorldParameters* params,
                                   AudioStreamCallback callback, void* user_data) {
    if (!engine || !params || !callback) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!engine->is_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    // TODO: 실제 스트리밍 합성 구현
    // 현재는 더미 구현
    const int chunk_size = 1024;
    float chunk_buffer[chunk_size];

    int total_samples = params->audio_length;
    int processed_samples = 0;

    while (processed_samples < total_samples) {
        int samples_to_process = chunk_size;
        if (processed_samples + samples_to_process > total_samples) {
            samples_to_process = total_samples - processed_samples;
        }

        // 더미 데이터 생성 (무음)
        memset(chunk_buffer, 0, samples_to_process * sizeof(float));

        // 콜백 호출
        if (!callback(chunk_buffer, samples_to_process, user_data)) {
            break;  // 사용자가 중단 요청
        }

        processed_samples += samples_to_process;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 유틸리티 함수들
// ============================================================================

void world_get_default_analysis_config(WorldAnalysisConfig* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(WorldAnalysisConfig));

    // F0 설정
    config->f0_config.frame_period = DEFAULT_FRAME_PERIOD;
    config->f0_config.f0_floor = DEFAULT_F0_FLOOR;
    config->f0_config.f0_ceil = DEFAULT_F0_CEIL;
    config->f0_config.algorithm = 0;  // DIO
    config->f0_config.channels_in_octave = DEFAULT_CHANNELS_IN_OCTAVE;
    config->f0_config.speed = DEFAULT_SPEED;
    config->f0_config.allowed_range = DEFAULT_ALLOWED_RANGE;

    // 스펙트럼 설정
    config->spectrum_config.q1 = DEFAULT_Q1;
    config->spectrum_config.fft_size = 0;  // 자동 계산

    // 비주기성 설정
    config->aperiodicity_config.threshold = DEFAULT_THRESHOLD;

    // libetude 통합 설정
    config->enable_simd_optimization = true;
    config->enable_gpu_acceleration = false;  // 기본적으로 비활성화
    config->memory_pool_size = DEFAULT_MEMORY_POOL_SIZE;
}

void world_get_default_synthesis_config(WorldSynthesisConfig* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(WorldSynthesisConfig));

    config->sample_rate = 44100;
    config->frame_period = DEFAULT_FRAME_PERIOD;
    config->enable_postfilter = true;
    config->enable_simd_optimization = true;
    config->enable_gpu_acceleration = false;
    config->memory_pool_size = DEFAULT_MEMORY_POOL_SIZE;
}

int world_get_fft_size_for_cheaptrick(int sample_rate) {
    // WORLD CheapTrick의 권장 FFT 크기 계산
    return (int)pow(2.0, ceil(log2(3.0 * sample_rate / DEFAULT_F0_FLOOR + 1)));
}

int world_get_samples_for_dio(int audio_length, int sample_rate, double frame_period) {
    return (int)((double)audio_length / (double)sample_rate / (frame_period / 1000.0)) + 1;
}