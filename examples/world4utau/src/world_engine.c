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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SIMD 최적화를 위한 헤더 포함
#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __AVX__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

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
        params = (WorldParameters*)et_alloc_from_pool(pool, sizeof(WorldParameters));
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
        params->f0 = (double*)et_alloc_from_pool(pool, f0_size);
        params->time_axis = (double*)et_alloc_from_pool(pool, f0_size);

        // 2D 배열 할당
        params->spectrogram = (double**)et_alloc_from_pool(pool, f0_length * sizeof(double*));
        params->aperiodicity = (double**)et_alloc_from_pool(pool, f0_length * sizeof(double*));

        if (params->spectrogram && params->aperiodicity) {
            double* spectrum_data = (double*)et_alloc_from_pool(pool, spectrum_size);
            double* aperiodicity_data = (double*)et_alloc_from_pool(pool, spectrum_size);

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
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (dst->f0_length != src->f0_length || dst->fft_size != src->fft_size) {
        return ET_ERROR_INVALID_ARGUMENT;
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
        return ET_ERROR_INVALID_ARGUMENT;
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
// WORLD F0 추출기 함수들
// ============================================================================

WorldF0Extractor* world_f0_extractor_create(const WorldF0Config* config, ETMemoryPool* mem_pool) {
    if (!config) {
        return NULL;
    }

    WorldF0Extractor* extractor = (WorldF0Extractor*)malloc(sizeof(WorldF0Extractor));
    if (!extractor) {
        return NULL;
    }

    memset(extractor, 0, sizeof(WorldF0Extractor));

    // 설정 복사
    extractor->config = *config;

    // 메모리 풀 설정
    if (mem_pool) {
        extractor->mem_pool = mem_pool;
    } else {
        extractor->mem_pool = et_create_memory_pool(DEFAULT_MEMORY_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
        if (!extractor->mem_pool) {
            free(extractor);
            return NULL;
        }
    }

    // STFT 컨텍스트 생성 (libetude 통합)
    ETSTFTConfig stft_config = {0};
    stft_config.fft_size = 2048;  // F0 추출용 기본 FFT 크기
    stft_config.hop_size = stft_config.fft_size / 4;
    stft_config.window_type = ET_WINDOW_HANN;

    extractor->stft_ctx = et_stft_create_context(&stft_config);
    if (!extractor->stft_ctx) {
        if (!mem_pool) {
            et_destroy_memory_pool(extractor->mem_pool);
        }
        free(extractor);
        return NULL;
    }

    return extractor;
}

void world_f0_extractor_destroy(WorldF0Extractor* extractor) {
    if (!extractor) {
        return;
    }

    if (extractor->stft_ctx) {
        et_stft_destroy_context(extractor->stft_ctx);
    }

    // 외부에서 제공된 메모리 풀이 아닌 경우에만 해제
    if (extractor->mem_pool && !extractor->is_initialized) {
        et_destroy_memory_pool(extractor->mem_pool);
    }

    free(extractor);
}

ETResult world_f0_extractor_initialize(WorldF0Extractor* extractor, int sample_rate, int audio_length) {
    if (!extractor || sample_rate <= 0 || audio_length <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 버퍼 크기 계산
    extractor->buffer_size = audio_length * 2;  // 여유분 포함

    // 작업용 버퍼 할당
    extractor->work_buffer = (double*)et_alloc_from_pool(extractor->mem_pool,
                                                          extractor->buffer_size * sizeof(double));
    if (!extractor->work_buffer) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 필터링된 신호 버퍼 할당
    extractor->filtered_signal = (double*)et_alloc_from_pool(extractor->mem_pool,
                                                              extractor->buffer_size * sizeof(double));
    if (!extractor->filtered_signal) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다운샘플링된 신호 버퍼 할당
    extractor->decimated_signal = (double*)et_alloc_from_pool(extractor->mem_pool,
                                                               extractor->buffer_size * sizeof(double));
    if (!extractor->decimated_signal) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // DIO 알고리즘 전용 버퍼 할당
    int f0_length = world_get_samples_for_dio(audio_length, sample_rate, extractor->config.frame_period);
    extractor->dio_candidates_count = (int)(extractor->config.channels_in_octave *
                                           log2(extractor->config.f0_ceil / extractor->config.f0_floor));

    extractor->dio_f0_candidates = (double*)et_alloc_from_pool(extractor->mem_pool,
                                                                f0_length * extractor->dio_candidates_count * sizeof(double));
    if (!extractor->dio_f0_candidates) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    extractor->dio_f0_scores = (double*)et_alloc_from_pool(extractor->mem_pool,
                                                            f0_length * extractor->dio_candidates_count * sizeof(double));
    if (!extractor->dio_f0_scores) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // Harvest 알고리즘 전용 버퍼 할당
    extractor->harvest_f0_map = (double*)et_alloc_from_pool(extractor->mem_pool,
                                                             f0_length * extractor->dio_candidates_count * sizeof(double));
    if (!extractor->harvest_f0_map) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    extractor->harvest_reliability = (double*)et_alloc_from_pool(extractor->mem_pool,
                                                                  f0_length * sizeof(double));
    if (!extractor->harvest_reliability) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    extractor->is_initialized = true;
    extractor->last_sample_rate = sample_rate;
    extractor->last_audio_length = audio_length;

    return ET_SUCCESS;
}

ETResult world_f0_extractor_dio(WorldF0Extractor* extractor,
                               const float* audio, int audio_length, int sample_rate,
                               double* f0, double* time_axis, int f0_length) {
    if (!extractor || !audio || !f0 || !time_axis) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!extractor->is_initialized ||
        extractor->last_sample_rate != sample_rate ||
        extractor->last_audio_length < audio_length) {
        ETResult result = world_f0_extractor_initialize(extractor, sample_rate, audio_length);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 시간축 초기화
    for (int i = 0; i < f0_length; i++) {
        time_axis[i] = i * extractor->config.frame_period / 1000.0;
    }

    // DIO 알고리즘 구현 (최적화된 버전 사용)
    ETResult result = world_dio_f0_estimation_optimized(extractor, audio, audio_length, sample_rate, f0, f0_length);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

ETResult world_f0_extractor_harvest(WorldF0Extractor* extractor,
                                   const float* audio, int audio_length, int sample_rate,
                                   double* f0, double* time_axis, int f0_length) {
    if (!extractor || !audio || !f0 || !time_axis) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!extractor->is_initialized ||
        extractor->last_sample_rate != sample_rate ||
        extractor->last_audio_length < audio_length) {
        ETResult result = world_f0_extractor_initialize(extractor, sample_rate, audio_length);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 시간축 초기화
    for (int i = 0; i < f0_length; i++) {
        time_axis[i] = i * extractor->config.frame_period / 1000.0;
    }

    // Harvest 알고리즘 구현
    ETResult result = world_harvest_f0_estimation(extractor, audio, audio_length, sample_rate, f0, f0_length);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

ETResult world_f0_extractor_extract(WorldF0Extractor* extractor,
                                   const float* audio, int audio_length, int sample_rate,
                                   double* f0, double* time_axis, int f0_length) {
    if (!extractor) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 설정된 알고리즘에 따라 적절한 함수 호출
    if (extractor->config.algorithm == 0) {
        // DIO 알고리즘
        return world_f0_extractor_dio(extractor, audio, audio_length, sample_rate,
                                     f0, time_axis, f0_length);
    } else {
        // Harvest 알고리즘
        return world_f0_extractor_harvest(extractor, audio, audio_length, sample_rate,
                                         f0, time_axis, f0_length);
    }
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
    engine->mem_pool = et_create_memory_pool(config->memory_pool_size, ET_DEFAULT_ALIGNMENT);
    if (!engine->mem_pool) {
        free(engine);
        return NULL;
    }

    // F0 추출기 생성
    engine->f0_extractor = world_f0_extractor_create(&config->f0_config, engine->mem_pool);
    if (!engine->f0_extractor) {
        et_destroy_memory_pool(engine->mem_pool);
        free(engine);
        return NULL;
    }

    // STFT 컨텍스트 생성 (libetude 통합)
    ETSTFTConfig stft_config = {0};
    stft_config.fft_size = config->spectrum_config.fft_size;
    if (stft_config.fft_size == 0) {
        stft_config.fft_size = 2048;  // 기본값
    }
    stft_config.hop_size = stft_config.fft_size / 4;  // 기본값
    stft_config.window_type = ET_WINDOW_HANN;

    engine->stft_ctx = et_stft_create_context(&stft_config);
    if (!engine->stft_ctx) {
        world_f0_extractor_destroy(engine->f0_extractor);
        et_destroy_memory_pool(engine->mem_pool);
        free(engine);
        return NULL;
    }

    // 작업용 버퍼 할당
    engine->work_buffer_size = 1024 * 1024;  // 1MB
    engine->work_buffer = (double*)et_alloc_from_pool(engine->mem_pool, engine->work_buffer_size);
    if (!engine->work_buffer) {
        et_stft_destroy_context(engine->stft_ctx);
        world_f0_extractor_destroy(engine->f0_extractor);
        et_destroy_memory_pool(engine->mem_pool);
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
        et_stft_destroy_context(engine->stft_ctx);
    }

    if (engine->f0_extractor) {
        world_f0_extractor_destroy(engine->f0_extractor);
    }

    if (engine->mem_pool) {
        et_destroy_memory_pool(engine->mem_pool);
    }

    free(engine);
}

ETResult world_analyze_audio(WorldAnalysisEngine* engine,
                            const float* audio, int audio_length, int sample_rate,
                            WorldParameters* params) {
    if (!engine || !audio || !params || audio_length <= 0 || sample_rate <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!engine->is_initialized) {
        return ET_ERROR_INVALID_STATE;
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

ETResult world_extract_f0(WorldAnalysisEngine* engine,
                         const float* audio, int audio_length, int sample_rate,
                         double* f0, double* time_axis, int f0_length) {
    if (!engine || !engine->f0_extractor) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // F0 추출기를 사용하여 F0 추출
    return world_f0_extractor_extract(engine->f0_extractor, audio, audio_length, sample_rate,
                                     f0, time_axis, f0_length);
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
    engine->mem_pool = et_create_memory_pool(config->memory_pool_size, ET_DEFAULT_ALIGNMENT);
    if (!engine->mem_pool) {
        free(engine);
        return NULL;
    }

    // 보코더 컨텍스트 생성 (libetude 통합)
    ETVocoderConfig vocoder_config = et_vocoder_default_config();
    vocoder_config.sample_rate = config->sample_rate;
    vocoder_config.hop_length = (int)(config->sample_rate * config->frame_period / 1000.0);

    engine->vocoder_ctx = et_create_vocoder(NULL, &vocoder_config);
    if (!engine->vocoder_ctx) {
        et_destroy_memory_pool(engine->mem_pool);
        free(engine);
        return NULL;
    }

    // 출력 버퍼는 필요시 동적으로 할당

    // 합성용 버퍼 할당
    engine->synthesis_buffer_size = 1024 * 1024;  // 1MB
    engine->synthesis_buffer = (double*)et_alloc_from_pool(engine->mem_pool,
                                                            engine->synthesis_buffer_size);
    if (!engine->synthesis_buffer) {
        et_destroy_vocoder(engine->vocoder_ctx);
        et_destroy_memory_pool(engine->mem_pool);
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

    if (engine->vocoder_ctx) {
        et_destroy_vocoder(engine->vocoder_ctx);
    }

    if (engine->mem_pool) {
        et_destroy_memory_pool(engine->mem_pool);
    }

    free(engine);
}

ETResult world_synthesize_audio(WorldSynthesisEngine* engine,
                               const WorldParameters* params,
                               float* output_audio, int* output_length) {
    if (!engine || !params || !output_audio || !output_length) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!engine->is_initialized) {
        return ET_ERROR_INVALID_STATE;
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
                                   WorldAudioStreamCallback callback, void* user_data) {
    if (!engine || !params || !callback) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!engine->is_initialized) {
        return ET_ERROR_INVALID_STATE;
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

// ============================================================================
// DIO 알고리즘 구현 (WORLD F0 추출)
// ============================================================================

/**
 * @brief 저역 통과 필터 적용
 */
static void apply_lowpass_filter(const float* input, double* output, int length,
                                int sample_rate, double cutoff_freq) {
    // 간단한 1차 저역 통과 필터 구현
    double rc = 1.0 / (2.0 * M_PI * cutoff_freq);
    double dt = 1.0 / (double)sample_rate;
    double alpha = dt / (rc + dt);

    output[0] = (double)input[0];
    for (int i = 1; i < length; i++) {
        output[i] = alpha * (double)input[i] + (1.0 - alpha) * output[i - 1];
    }
}

/**
 * @brief 다운샘플링 수행
 */
static void downsample_signal(const double* input, double* output, int input_length,
                             int decimation_factor, int* output_length) {
    *output_length = input_length / decimation_factor;
    for (int i = 0; i < *output_length; i++) {
        output[i] = input[i * decimation_factor];
    }
}

/**
 * @brief 제로 크로싱 검출
 */
static void detect_zero_crossings(const double* signal, int length, double* zero_crossings) {
    for (int i = 0; i < length - 1; i++) {
        if ((signal[i] >= 0.0 && signal[i + 1] < 0.0) ||
            (signal[i] < 0.0 && signal[i + 1] >= 0.0)) {
            // 선형 보간으로 정확한 제로 크로싱 위치 계산
            double ratio = -signal[i] / (signal[i + 1] - signal[i]);
            zero_crossings[i] = (double)i + ratio;
        } else {
            zero_crossings[i] = -1.0;  // 제로 크로싱 없음
        }
    }
    zero_crossings[length - 1] = -1.0;
}

/**
 * @brief F0 후보 생성
 */
static void generate_f0_candidates(WorldF0Extractor* extractor, int sample_rate,
                                  double* candidates, int* num_candidates) {
    double f0_floor = extractor->config.f0_floor;
    double f0_ceil = extractor->config.f0_ceil;
    double channels_in_octave = extractor->config.channels_in_octave;

    double log_f0_floor = log2(f0_floor);
    double log_f0_ceil = log2(f0_ceil);

    *num_candidates = (int)(channels_in_octave * (log_f0_ceil - log_f0_floor)) + 1;

    for (int i = 0; i < *num_candidates; i++) {
        double log_f0 = log_f0_floor + (double)i / channels_in_octave;
        candidates[i] = pow(2.0, log_f0);
    }
}

/**
 * @brief 각 F0 후보에 대한 점수 계산
 */
static void calculate_f0_scores(WorldF0Extractor* extractor, const double* filtered_signal,
                               int signal_length, int sample_rate, const double* candidates,
                               int num_candidates, int frame_index, double* scores) {
    double frame_period_samples = extractor->config.frame_period * sample_rate / 1000.0;
    int center_sample = (int)(frame_index * frame_period_samples);

    // 분석 윈도우 크기 (약 3 피치 주기)
    int window_size = (int)(3.0 * sample_rate / extractor->config.f0_floor);
    int start_sample = center_sample - window_size / 2;
    int end_sample = center_sample + window_size / 2;

    // 경계 체크
    if (start_sample < 0) start_sample = 0;
    if (end_sample >= signal_length) end_sample = signal_length - 1;

    for (int c = 0; c < num_candidates; c++) {
        double f0_candidate = candidates[c];
        double period_samples = sample_rate / f0_candidate;

        // 자기상관 기반 점수 계산
        double score = 0.0;
        int valid_samples = 0;

        for (int i = start_sample; i < end_sample - (int)period_samples; i++) {
            if (i + (int)period_samples < signal_length) {
                double correlation = filtered_signal[i] * filtered_signal[i + (int)period_samples];
                score += correlation;
                valid_samples++;
            }
        }

        if (valid_samples > 0) {
            score /= valid_samples;

            // 정규화 및 가중치 적용
            double frequency_weight = 1.0 / (1.0 + pow((f0_candidate - 150.0) / 100.0, 2.0));
            scores[c] = score * frequency_weight;
        } else {
            scores[c] = 0.0;
        }
    }
}

/**
 * @brief 최적 F0 선택
 */
static double select_best_f0(const double* candidates, const double* scores, int num_candidates,
                            double threshold) {
    double best_score = -1e10;
    double best_f0 = 0.0;

    for (int i = 0; i < num_candidates; i++) {
        if (scores[i] > best_score && scores[i] > threshold) {
            best_score = scores[i];
            best_f0 = candidates[i];
        }
    }

    return best_f0;
}

/**
 * @brief DIO F0 추정 메인 함수
 */
ETResult world_dio_f0_estimation(WorldF0Extractor* extractor, const float* audio,
                                int audio_length, int sample_rate, double* f0, int f0_length) {
    if (!extractor || !audio || !f0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 1. 저역 통과 필터 적용 (앨리어싱 방지)
    double cutoff_freq = sample_rate / 2.0 * 0.9;  // 나이퀴스트 주파수의 90%
    apply_lowpass_filter(audio, extractor->filtered_signal, audio_length, sample_rate, cutoff_freq);

    // 2. 다운샘플링 (계산 효율성을 위해)
    int decimation_factor = (int)(sample_rate / (4.0 * extractor->config.f0_ceil));
    if (decimation_factor < 1) decimation_factor = 1;

    int decimated_length;
    downsample_signal(extractor->filtered_signal, extractor->decimated_signal,
                     audio_length, decimation_factor, &decimated_length);

    int decimated_sample_rate = sample_rate / decimation_factor;

    // 3. F0 후보 생성
    double candidates[256];  // 최대 256개 후보
    int num_candidates;
    generate_f0_candidates(extractor, decimated_sample_rate, candidates, &num_candidates);

    if (num_candidates > 256) num_candidates = 256;

    // 4. 각 프레임에 대해 F0 추정
    double frame_period_samples = extractor->config.frame_period * decimated_sample_rate / 1000.0;

    for (int frame = 0; frame < f0_length; frame++) {
        double scores[256];

        // 각 F0 후보에 대한 점수 계산
        calculate_f0_scores(extractor, extractor->decimated_signal, decimated_length,
                           decimated_sample_rate, candidates, num_candidates, frame, scores);

        // 최적 F0 선택
        double threshold = 0.1;  // 임계값
        f0[frame] = select_best_f0(candidates, scores, num_candidates, threshold);

        // 후처리: 연속성 체크 및 스무딩
        if (frame > 0 && f0[frame] > 0.0 && f0[frame - 1] > 0.0) {
            double ratio = f0[frame] / f0[frame - 1];
            // 급격한 변화 감지 (옥타브 에러 보정)
            if (ratio > 1.8) {
                f0[frame] /= 2.0;  // 옥타브 다운
            } else if (ratio < 0.6) {
                f0[frame] *= 2.0;  // 옥타브 업
            }
        }
    }

    // 5. 후처리: 메디안 필터링으로 노이즈 제거
    world_apply_median_filter(f0, f0_length, 3);

    return ET_SUCCESS;
}

/**
 * @brief 메디안 필터 적용 (노이즈 제거용)
 */
void world_apply_median_filter(double* signal, int length, int window_size) {
    if (window_size % 2 == 0) window_size++;  // 홀수로 만들기
    int half_window = window_size / 2;

    double* temp = (double*)malloc(length * sizeof(double));
    if (!temp) return;

    memcpy(temp, signal, length * sizeof(double));

    for (int i = 0; i < length; i++) {
        double window[window_size];
        int window_count = 0;

        // 윈도우 내 값들 수집
        for (int j = -half_window; j <= half_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < length) {
                window[window_count++] = temp[idx];
            }
        }

        // 정렬하여 메디안 찾기
        for (int a = 0; a < window_count - 1; a++) {
            for (int b = a + 1; b < window_count; b++) {
                if (window[a] > window[b]) {
                    double swap = window[a];
                    window[a] = window[b];
                    window[b] = swap;
                }
            }
        }

        signal[i] = window[window_count / 2];
    }

    free(temp);
}

// ============================================================================
// Harvest 알고리즘 구현 (WORLD F0 추출 - 고정밀도)
// ============================================================================

/**
 * @brief 복소수 구조체
 */
typedef struct {
    double real;
    double imag;
} Complex;

/**
 * @brief 복소수 곱셈
 */
static Complex complex_multiply(Complex a, Complex b) {
    Complex result;
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

/**
 * @brief 복소수 크기 계산
 */
static double complex_magnitude(Complex c) {
    return sqrt(c.real * c.real + c.imag * c.imag);
}

/**
 * @brief 간단한 FFT 구현 (Harvest용)
 */
static void simple_fft(Complex* data, int n, bool inverse) {
    // 비트 역순 정렬
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;

        if (i < j) {
            Complex temp = data[i];
            data[i] = data[j];
            data[j] = temp;
        }
    }

    // FFT 계산
    for (int len = 2; len <= n; len <<= 1) {
        double angle = 2.0 * M_PI / len * (inverse ? -1 : 1);
        Complex wlen = {cos(angle), sin(angle)};

        for (int i = 0; i < n; i += len) {
            Complex w = {1.0, 0.0};
            for (int j = 0; j < len / 2; j++) {
                Complex u = data[i + j];
                Complex v = complex_multiply(data[i + j + len / 2], w);

                data[i + j].real = u.real + v.real;
                data[i + j].imag = u.imag + v.imag;
                data[i + j + len / 2].real = u.real - v.real;
                data[i + j + len / 2].imag = u.imag - v.imag;

                w = complex_multiply(w, wlen);
            }
        }
    }

    if (inverse) {
        for (int i = 0; i < n; i++) {
            data[i].real /= n;
            data[i].imag /= n;
        }
    }
}

/**
 * @brief 윈도우 함수 적용 (Harvest용 Blackman 윈도우)
 */
static void apply_blackman_window(double* signal, int length) {
    for (int i = 0; i < length; i++) {
        double w = 0.42 - 0.5 * cos(2.0 * M_PI * i / (length - 1)) +
                   0.08 * cos(4.0 * M_PI * i / (length - 1));
        signal[i] *= w;
    }
}

/**
 * @brief 스펙트럼 기반 F0 후보 검출
 */
static void detect_f0_candidates_spectrum(const double* signal, int signal_length,
                                         int sample_rate, double f0_floor, double f0_ceil,
                                         double* candidates, double* reliabilities,
                                         int* num_candidates) {
    // FFT 크기 계산 (2의 거듭제곱)
    int fft_size = 1;
    while (fft_size < signal_length) fft_size <<= 1;

    // FFT 입력 준비
    Complex* fft_input = (Complex*)calloc(fft_size, sizeof(Complex));
    if (!fft_input) {
        *num_candidates = 0;
        return;
    }

    // 신호를 복사하고 윈도우 적용
    for (int i = 0; i < signal_length; i++) {
        fft_input[i].real = signal[i];
        fft_input[i].imag = 0.0;
    }

    // Blackman 윈도우 적용
    double* windowed_signal = (double*)malloc(signal_length * sizeof(double));
    if (!windowed_signal) {
        free(fft_input);
        *num_candidates = 0;
        return;
    }

    for (int i = 0; i < signal_length; i++) {
        windowed_signal[i] = fft_input[i].real;
    }
    apply_blackman_window(windowed_signal, signal_length);

    for (int i = 0; i < signal_length; i++) {
        fft_input[i].real = windowed_signal[i];
    }

    // FFT 수행
    simple_fft(fft_input, fft_size, false);

    // 스펙트럼 크기 계산
    double* spectrum = (double*)malloc(fft_size / 2 * sizeof(double));
    if (!spectrum) {
        free(fft_input);
        free(windowed_signal);
        *num_candidates = 0;
        return;
    }

    for (int i = 0; i < fft_size / 2; i++) {
        spectrum[i] = complex_magnitude(fft_input[i]);
    }

    // F0 후보 검출
    double freq_resolution = (double)sample_rate / fft_size;
    int min_bin = (int)(f0_floor / freq_resolution);
    int max_bin = (int)(f0_ceil / freq_resolution);

    *num_candidates = 0;

    // 피크 검출
    for (int i = min_bin + 1; i < max_bin - 1 && *num_candidates < 64; i++) {
        if (spectrum[i] > spectrum[i - 1] && spectrum[i] > spectrum[i + 1]) {
            double freq = i * freq_resolution;

            // 하모닉 강도 확인
            double harmonic_strength = 0.0;
            int num_harmonics = 0;

            for (int h = 2; h <= 5; h++) {  // 2-5차 하모닉 확인
                int harmonic_bin = i * h;
                if (harmonic_bin < fft_size / 2) {
                    harmonic_strength += spectrum[harmonic_bin];
                    num_harmonics++;
                }
            }

            if (num_harmonics > 0) {
                harmonic_strength /= num_harmonics;
                double reliability = harmonic_strength / (spectrum[i] + 1e-10);

                if (reliability > 0.1) {  // 임계값
                    candidates[*num_candidates] = freq;
                    reliabilities[*num_candidates] = reliability;
                    (*num_candidates)++;
                }
            }
        }
    }

    free(fft_input);
    free(windowed_signal);
    free(spectrum);
}

/**
 * @brief 시간 도메인 정밀 F0 추정
 */
static double refine_f0_time_domain(const double* signal, int signal_length,
                                   int sample_rate, double initial_f0) {
    if (initial_f0 <= 0.0) return 0.0;

    double period_samples = sample_rate / initial_f0;
    int search_range = (int)(period_samples * 0.2);  // ±20% 범위에서 검색

    double best_correlation = -1.0;
    double best_period = period_samples;

    // 자기상관 기반 정밀 추정
    for (int lag = (int)period_samples - search_range;
         lag <= (int)period_samples + search_range; lag++) {

        if (lag <= 0 || lag >= signal_length / 2) continue;

        double correlation = 0.0;
        double energy1 = 0.0, energy2 = 0.0;
        int valid_samples = 0;

        for (int i = 0; i < signal_length - lag; i++) {
            correlation += signal[i] * signal[i + lag];
            energy1 += signal[i] * signal[i];
            energy2 += signal[i + lag] * signal[i + lag];
            valid_samples++;
        }

        if (valid_samples > 0 && energy1 > 0.0 && energy2 > 0.0) {
            correlation /= sqrt(energy1 * energy2);

            if (correlation > best_correlation) {
                best_correlation = correlation;
                best_period = lag;
            }
        }
    }

    if (best_correlation > 0.3) {  // 신뢰도 임계값
        return sample_rate / best_period;
    }

    return 0.0;  // 신뢰할 수 없는 F0
}

/**
 * @brief Harvest F0 추정 메인 함수
 */
ETResult world_harvest_f0_estimation(WorldF0Extractor* extractor, const float* audio,
                                    int audio_length, int sample_rate, double* f0, int f0_length) {
    if (!extractor || !audio || !f0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    double frame_period_samples = extractor->config.frame_period * sample_rate / 1000.0;

    // 각 프레임에 대해 F0 추정
    for (int frame = 0; frame < f0_length; frame++) {
        int center_sample = (int)(frame * frame_period_samples);

        // 분석 윈도우 크기 (약 4 피치 주기)
        int window_size = (int)(4.0 * sample_rate / extractor->config.f0_floor);
        int start_sample = center_sample - window_size / 2;
        int end_sample = center_sample + window_size / 2;

        // 경계 체크
        if (start_sample < 0) start_sample = 0;
        if (end_sample >= audio_length) end_sample = audio_length - 1;

        int actual_window_size = end_sample - start_sample;
        if (actual_window_size < window_size / 2) {
            f0[frame] = 0.0;
            continue;
        }

        // 윈도우 신호 추출
        double* window_signal = (double*)malloc(actual_window_size * sizeof(double));
        if (!window_signal) {
            f0[frame] = 0.0;
            continue;
        }

        for (int i = 0; i < actual_window_size; i++) {
            window_signal[i] = (double)audio[start_sample + i];
        }

        // 1단계: 스펙트럼 기반 F0 후보 검출
        double candidates[64];
        double reliabilities[64];
        int num_candidates;

        detect_f0_candidates_spectrum(window_signal, actual_window_size, sample_rate,
                                     extractor->config.f0_floor, extractor->config.f0_ceil,
                                     candidates, reliabilities, &num_candidates);

        // 2단계: 시간 도메인 정밀 추정
        double best_f0 = 0.0;
        double best_reliability = 0.0;

        for (int c = 0; c < num_candidates; c++) {
            double refined_f0 = refine_f0_time_domain(window_signal, actual_window_size,
                                                     sample_rate, candidates[c]);

            if (refined_f0 > 0.0 && reliabilities[c] > best_reliability) {
                best_f0 = refined_f0;
                best_reliability = reliabilities[c];
            }
        }

        f0[frame] = best_f0;

        free(window_signal);
    }

    // 후처리: 연속성 개선 및 노이즈 제거
    world_harvest_postprocess(f0, f0_length, extractor->config.f0_floor, extractor->config.f0_ceil);

    return ET_SUCCESS;
}

/**
 * @brief Harvest 후처리 (연속성 개선)
 */
void world_harvest_postprocess(double* f0, int f0_length, double f0_floor, double f0_ceil) {
    // 1. 옥타브 에러 보정
    for (int i = 1; i < f0_length - 1; i++) {
        if (f0[i] > 0.0 && f0[i - 1] > 0.0 && f0[i + 1] > 0.0) {
            double ratio_prev = f0[i] / f0[i - 1];
            double ratio_next = f0[i + 1] / f0[i];

            // 2배 또는 1/2배 관계 확인
            if (ratio_prev > 1.8 && ratio_next < 0.6) {
                f0[i] /= 2.0;  // 옥타브 다운
            } else if (ratio_prev < 0.6 && ratio_next > 1.8) {
                f0[i] *= 2.0;  // 옥타브 업
            }
        }
    }

    // 2. 짧은 무음 구간 보간
    for (int i = 1; i < f0_length - 1; i++) {
        if (f0[i] == 0.0 && f0[i - 1] > 0.0 && f0[i + 1] > 0.0) {
            double ratio = f0[i + 1] / f0[i - 1];
            if (ratio > 0.8 && ratio < 1.25) {  // 유사한 피치
                f0[i] = sqrt(f0[i - 1] * f0[i + 1]);  // 기하평균으로 보간
            }
        }
    }

    // 3. 범위 제한
    for (int i = 0; i < f0_length; i++) {
        if (f0[i] < f0_floor || f0[i] > f0_ceil) {
            f0[i] = 0.0;
        }
    }

    // 4. 가벼운 스무딩 (3점 이동평균)
    double* temp = (double*)malloc(f0_length * sizeof(double));
    if (temp) {
        memcpy(temp, f0, f0_length * sizeof(double));

        for (int i = 1; i < f0_length - 1; i++) {
            if (temp[i] > 0.0 && temp[i - 1] > 0.0 && temp[i + 1] > 0.0) {
                f0[i] = (temp[i - 1] + temp[i] + temp[i + 1]) / 3.0;
            }
        }

        free(temp);
    }
}

// ============================================================================
// SIMD 최적화 함수들
// ============================================================================

/**
 * @brief SIMD 최적화된 자기상관 계산 (SSE2)
 */
#ifdef __SSE2__
static double calculate_autocorrelation_sse2(const double* signal, int length, int lag) {
    if (lag >= length) return 0.0;

    __m128d sum = _mm_setzero_pd();
    int simd_length = (length - lag) & ~1;  // 2의 배수로 맞춤

    for (int i = 0; i < simd_length; i += 2) {
        __m128d a = _mm_loadu_pd(&signal[i]);
        __m128d b = _mm_loadu_pd(&signal[i + lag]);
        __m128d prod = _mm_mul_pd(a, b);
        sum = _mm_add_pd(sum, prod);
    }

    // 결과 합산
    double result[2];
    _mm_storeu_pd(result, sum);
    double correlation = result[0] + result[1];

    // 나머지 처리
    for (int i = simd_length; i < length - lag; i++) {
        correlation += signal[i] * signal[i + lag];
    }

    return correlation;
}
#endif

/**
 * @brief SIMD 최적화된 벡터 곱셈 (AVX)
 */
#ifdef __AVX__
static void vector_multiply_avx(const double* a, const double* b, double* result, int length) {
    int avx_length = length & ~3;  // 4의 배수로 맞춤

    for (int i = 0; i < avx_length; i += 4) {
        __m256d va = _mm256_loadu_pd(&a[i]);
        __m256d vb = _mm256_loadu_pd(&b[i]);
        __m256d vr = _mm256_mul_pd(va, vb);
        _mm256_storeu_pd(&result[i], vr);
    }

    // 나머지 처리
    for (int i = avx_length; i < length; i++) {
        result[i] = a[i] * b[i];
    }
}
#endif

/**
 * @brief SIMD 최적화된 자기상관 계산 (NEON)
 */
#ifdef __ARM_NEON
static double calculate_autocorrelation_neon(const float* signal, int length, int lag) {
    if (lag >= length) return 0.0;

    float32x4_t sum = vdupq_n_f32(0.0f);
    int neon_length = (length - lag) & ~3;  // 4의 배수로 맞춤

    for (int i = 0; i < neon_length; i += 4) {
        float32x4_t a = vld1q_f32(&signal[i]);
        float32x4_t b = vld1q_f32(&signal[i + lag]);
        float32x4_t prod = vmulq_f32(a, b);
        sum = vaddq_f32(sum, prod);
    }

    // 결과 합산
    float result[4];
    vst1q_f32(result, sum);
    double correlation = result[0] + result[1] + result[2] + result[3];

    // 나머지 처리
    for (int i = neon_length; i < length - lag; i++) {
        correlation += signal[i] * signal[i + lag];
    }

    return correlation;
}
#endif

/**
 * @brief 최적화된 자기상관 계산 (플랫폼 자동 선택)
 */
static double calculate_autocorrelation_optimized(const double* signal, int length, int lag) {
#ifdef __SSE2__
    return calculate_autocorrelation_sse2(signal, length, lag);
#else
    // 기본 구현
    double correlation = 0.0;
    for (int i = 0; i < length - lag; i++) {
        correlation += signal[i] * signal[i + lag];
    }
    return correlation;
#endif
}

/**
 * @brief 메모리 효율적인 F0 후보 점수 계산
 */
static void calculate_f0_scores_optimized(WorldF0Extractor* extractor, const double* signal,
                                         int signal_length, int sample_rate,
                                         const double* candidates, int num_candidates,
                                         int frame_index, double* scores) {
    double frame_period_samples = extractor->config.frame_period * sample_rate / 1000.0;
    int center_sample = (int)(frame_index * frame_period_samples);

    // 적응적 윈도우 크기 (메모리 사용량 최적화)
    int base_window_size = (int)(2.0 * sample_rate / extractor->config.f0_ceil);
    int max_window_size = (int)(4.0 * sample_rate / extractor->config.f0_floor);

    for (int c = 0; c < num_candidates; c++) {
        double f0_candidate = candidates[c];
        double period_samples = sample_rate / f0_candidate;

        // 후보별 적응적 윈도우 크기
        int window_size = (int)(3.0 * period_samples);
        if (window_size < base_window_size) window_size = base_window_size;
        if (window_size > max_window_size) window_size = max_window_size;

        int start_sample = center_sample - window_size / 2;
        int end_sample = center_sample + window_size / 2;

        // 경계 체크
        if (start_sample < 0) start_sample = 0;
        if (end_sample >= signal_length) end_sample = signal_length - 1;

        int actual_window_size = end_sample - start_sample;
        if (actual_window_size < (int)period_samples) {
            scores[c] = 0.0;
            continue;
        }

        // 최적화된 자기상관 계산
        double correlation = calculate_autocorrelation_optimized(
            &signal[start_sample], actual_window_size, (int)period_samples);

        // 정규화
        double energy = 0.0;
        for (int i = start_sample; i < end_sample - (int)period_samples; i++) {
            energy += signal[i] * signal[i];
        }

        if (energy > 0.0) {
            correlation /= energy;

            // 주파수 가중치 (인간 음성 특성 고려)
            double freq_weight = 1.0;
            if (f0_candidate < 80.0) {
                freq_weight = 0.5;  // 너무 낮은 주파수 억제
            } else if (f0_candidate > 500.0) {
                freq_weight = 0.7;  // 너무 높은 주파수 억제
            }

            scores[c] = correlation * freq_weight;
        } else {
            scores[c] = 0.0;
        }
    }
}

/**
 * @brief 최적화된 DIO F0 추정
 */
ETResult world_dio_f0_estimation_optimized(WorldF0Extractor* extractor, const float* audio,
                                          int audio_length, int sample_rate, double* f0, int f0_length) {
    if (!extractor || !audio || !f0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // SIMD 최적화 활성화 확인
    bool use_simd = true;  // 기본적으로 활성화

    // 1. 저역 통과 필터 적용 (최적화된 버전)
    double cutoff_freq = sample_rate / 2.0 * 0.9;
    apply_lowpass_filter(audio, extractor->filtered_signal, audio_length, sample_rate, cutoff_freq);

    // 2. 적응적 다운샘플링
    int decimation_factor = 1;
    if (sample_rate > 16000) {
        decimation_factor = (int)(sample_rate / (4.0 * extractor->config.f0_ceil));
        if (decimation_factor < 1) decimation_factor = 1;
        if (decimation_factor > 4) decimation_factor = 4;  // 최대 4배 다운샘플링
    }

    int decimated_length;
    downsample_signal(extractor->filtered_signal, extractor->decimated_signal,
                     audio_length, decimation_factor, &decimated_length);

    int decimated_sample_rate = sample_rate / decimation_factor;

    // 3. 최적화된 F0 후보 생성 (적응적 개수)
    double candidates[128];  // 메모리 사용량 최적화
    int num_candidates;
    generate_f0_candidates(extractor, decimated_sample_rate, candidates, &num_candidates);

    if (num_candidates > 128) num_candidates = 128;

    // 4. 병렬화된 F0 추정 (OpenMP 사용 가능시)
    #pragma omp parallel for if(f0_length > 100)
    for (int frame = 0; frame < f0_length; frame++) {
        double scores[128];

        // 최적화된 점수 계산
        calculate_f0_scores_optimized(extractor, extractor->decimated_signal, decimated_length,
                                     decimated_sample_rate, candidates, num_candidates, frame, scores);

        // 최적 F0 선택 (적응적 임계값)
        double adaptive_threshold = 0.05 + 0.1 * (double)frame / f0_length;  // 점진적 임계값
        f0[frame] = select_best_f0(candidates, scores, num_candidates, adaptive_threshold);

        // 실시간 연속성 체크
        if (frame > 0 && f0[frame] > 0.0 && f0[frame - 1] > 0.0) {
            double ratio = f0[frame] / f0[frame - 1];
            if (ratio > 1.8) {
                f0[frame] /= 2.0;
            } else if (ratio < 0.6) {
                f0[frame] *= 2.0;
            }
        }
    }

    // 5. 경량 후처리 (성능 최적화)
    world_apply_lightweight_postprocess(f0, f0_length);

    return ET_SUCCESS;
}

/**
 * @brief 경량 후처리 (성능 최적화)
 */
void world_apply_lightweight_postprocess(double* f0, int f0_length) {
    // 1. 간단한 메디안 필터 (3점)
    if (f0_length >= 3) {
        double prev = f0[0];
        for (int i = 1; i < f0_length - 1; i++) {
            double curr = f0[i];
            double next = f0[i + 1];

            // 3점 메디안
            if ((prev <= curr && curr <= next) || (next <= curr && curr <= prev)) {
                // 이미 메디안
            } else if ((prev <= next && next <= curr) || (curr <= next && next <= prev)) {
                f0[i] = next;
            } else {
                f0[i] = prev;
            }

            prev = curr;
        }
    }

    // 2. 짧은 무음 구간 보간 (1-2 프레임만)
    for (int i = 1; i < f0_length - 1; i++) {
        if (f0[i] == 0.0 && f0[i - 1] > 0.0 && f0[i + 1] > 0.0) {
            double ratio = f0[i + 1] / f0[i - 1];
            if (ratio > 0.8 && ratio < 1.25) {
                f0[i] = sqrt(f0[i - 1] * f0[i + 1]);
            }
        }
    }
}

/**
 * @brief 메모리 사용량 모니터링
 */
void world_monitor_memory_usage(WorldF0Extractor* extractor, size_t* current_usage, size_t* peak_usage) {
    if (!extractor || !current_usage || !peak_usage) return;

    // 현재 메모리 사용량 계산
    *current_usage = 0;

    if (extractor->work_buffer) {
        *current_usage += extractor->buffer_size * sizeof(double);
    }

    if (extractor->filtered_signal) {
        *current_usage += extractor->buffer_size * sizeof(double);
    }

    if (extractor->decimated_signal) {
        *current_usage += extractor->buffer_size * sizeof(double);
    }

    if (extractor->dio_f0_candidates) {
        *current_usage += extractor->dio_candidates_count * sizeof(double);
    }

    if (extractor->dio_f0_scores) {
        *current_usage += extractor->dio_candidates_count * sizeof(double);
    }

    // 피크 사용량 업데이트 (간단한 추적)
    static size_t recorded_peak = 0;
    if (*current_usage > recorded_peak) {
        recorded_peak = *current_usage;
    }
    *peak_usage = recorded_peak;
}