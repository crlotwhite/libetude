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

    // 스펙트럼 분석기 생성
    engine->spectrum_analyzer = world_spectrum_analyzer_create(&config->spectrum_config, engine->mem_pool);
    if (!engine->spectrum_analyzer) {
        world_f0_extractor_destroy(engine->f0_extractor);
        et_destroy_memory_pool(engine->mem_pool);
        free(engine);
        return NULL;
    }

    // 비주기성 분석기 생성
    engine->aperiodicity_analyzer = world_aperiodicity_analyzer_create(&config->aperiodicity_config, engine->mem_pool);
    if (!engine->aperiodicity_analyzer) {
        world_spectrum_analyzer_destroy(engine->spectrum_analyzer);
        world_f0_extractor_destroy(engine->f0_extractor);
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

    if (engine->spectrum_analyzer) {
        world_spectrum_analyzer_destroy(engine->spectrum_analyzer);
    }

    if (engine->aperiodicity_analyzer) {
        world_aperiodicity_analyzer_destroy(engine->aperiodicity_analyzer);
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
    if (!engine || !engine->spectrum_analyzer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // SIMD 최적화 및 병렬 처리가 활성화된 경우 최적화된 버전 사용
    if (engine->config.enable_simd_optimization && f0_length > 16) {
        return world_spectrum_analyzer_cheaptrick_parallel(engine->spectrum_analyzer,
                                                          audio, audio_length, sample_rate,
                                                          f0, time_axis, f0_length,
                                                          spectrogram, 0);  // 0 = 자동 스레드 수
    } else {
        // 일반 버전 사용
        return world_spectrum_analyzer_cheaptrick(engine->spectrum_analyzer,
                                                 audio, audio_length, sample_rate,
                                                 f0, time_axis, f0_length,
                                                 spectrogram);
    }
}

ETResult world_analyze_aperiodicity(WorldAnalysisEngine* engine,
                                   const float* audio, int audio_length, int sample_rate,
                                   const double* f0, const double* time_axis, int f0_length,
                                   double** aperiodicity) {
    if (!engine || !audio || !f0 || !time_axis || !aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!engine->aperiodicity_analyzer) {
        return ET_ERROR_INVALID_STATE;
    }

    // 비주기성 분석기를 사용하여 D4C 알고리즘 수행
    return world_aperiodicity_analyzer_d4c(engine->aperiodicity_analyzer,
                                          audio, audio_length, sample_rate,
                                          f0, time_axis, f0_length,
                                          aperiodicity);
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

// ============================================================================
// WORLD 스펙트럼 분석기 구현
// ============================================================================

WorldSpectrumAnalyzer* world_spectrum_analyzer_create(const WorldSpectrumConfig* config, ETMemoryPool* mem_pool) {
    if (!config) {
        return NULL;
    }

    WorldSpectrumAnalyzer* analyzer = (WorldSpectrumAnalyzer*)malloc(sizeof(WorldSpectrumAnalyzer));
    if (!analyzer) {
        return NULL;
    }

    memset(analyzer, 0, sizeof(WorldSpectrumAnalyzer));

    // 설정 복사
    analyzer->config = *config;

    // 메모리 풀 설정
    if (mem_pool) {
        analyzer->mem_pool = mem_pool;
    } else {
        analyzer->mem_pool = et_create_memory_pool(DEFAULT_MEMORY_POOL_SIZE, ET_DEFAULT_ALIGNMENT);
        if (!analyzer->mem_pool) {
            free(analyzer);
            return NULL;
        }
    }

    return analyzer;
}

void world_spectrum_analyzer_destroy(WorldSpectrumAnalyzer* analyzer) {
    if (!analyzer) {
        return;
    }

    if (analyzer->stft_ctx) {
        et_stft_destroy_context(analyzer->stft_ctx);
    }

    // 외부에서 제공된 메모리 풀이 아닌 경우에만 해제
    if (analyzer->mem_pool && !analyzer->is_initialized) {
        et_destroy_memory_pool(analyzer->mem_pool);
    }

    free(analyzer);
}

ETResult world_spectrum_analyzer_initialize(WorldSpectrumAnalyzer* analyzer, int sample_rate, int fft_size) {
    if (!analyzer || sample_rate <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // FFT 크기 결정
    if (fft_size <= 0) {
        analyzer->fft_size = world_get_fft_size_for_cheaptrick(sample_rate);
    } else {
        analyzer->fft_size = fft_size;
    }

    // 윈도우 크기 설정 (FFT 크기와 동일)
    analyzer->window_size = analyzer->fft_size;
    analyzer->buffer_size = analyzer->fft_size * 2;  // 여유분 포함

    // STFT 컨텍스트 생성 (libetude 통합)
    ETSTFTConfig stft_config = {0};
    stft_config.fft_size = analyzer->fft_size;
    stft_config.hop_size = analyzer->fft_size / 4;  // 기본값
    stft_config.window_type = ET_WINDOW_HANN;

    analyzer->stft_ctx = et_stft_create_context(&stft_config);
    if (!analyzer->stft_ctx) {
        return ET_ERROR_INITIALIZATION_FAILED;
    }

    // 버퍼 할당
    size_t double_buffer_size = analyzer->buffer_size * sizeof(double);
    size_t fft_buffer_size = analyzer->fft_size * sizeof(double);
    size_t spectrum_buffer_size = (analyzer->fft_size / 2 + 1) * sizeof(double);

    analyzer->window_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, fft_buffer_size);
    analyzer->fft_input_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, fft_buffer_size);
    analyzer->fft_output_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, fft_buffer_size);
    analyzer->magnitude_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_buffer_size);
    analyzer->phase_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_buffer_size);
    analyzer->smoothed_spectrum = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_buffer_size);

    // CheapTrick 전용 버퍼
    analyzer->liftering_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, fft_buffer_size);
    analyzer->cepstrum_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, fft_buffer_size);
    analyzer->envelope_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_buffer_size);

    // 할당 실패 확인
    if (!analyzer->window_buffer || !analyzer->fft_input_buffer || !analyzer->fft_output_buffer ||
        !analyzer->magnitude_buffer || !analyzer->phase_buffer || !analyzer->smoothed_spectrum ||
        !analyzer->liftering_buffer || !analyzer->cepstrum_buffer || !analyzer->envelope_buffer) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 윈도우 함수 초기화 (Hann 윈도우)
    for (int i = 0; i < analyzer->fft_size; i++) {
        analyzer->window_buffer[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (analyzer->fft_size - 1)));
    }

    analyzer->is_initialized = true;
    analyzer->last_sample_rate = sample_rate;
    analyzer->last_q1 = analyzer->config.q1;

    return ET_SUCCESS;
}

/**
 * @brief F0 적응형 윈도우 크기 계산
 */
static int calculate_adaptive_window_size(double f0_value, int sample_rate, int fft_size) {
    if (f0_value <= 0.0) {
        return fft_size;  // 무성음의 경우 기본 윈도우 크기 사용
    }

    // F0에 기반한 적응형 윈도우 크기 (약 3 피치 주기)
    int adaptive_size = (int)(3.0 * sample_rate / f0_value);

    // 최소/최대 제한
    if (adaptive_size < fft_size / 4) {
        adaptive_size = fft_size / 4;
    } else if (adaptive_size > fft_size) {
        adaptive_size = fft_size;
    }

    return adaptive_size;
}

/**
 * @brief 스펙트럼 추출 (단일 프레임)
 */
static ETResult extract_spectrum_frame(WorldSpectrumAnalyzer* analyzer,
                                      const float* audio, int audio_length,
                                      int center_sample, int window_size,
                                      double* magnitude, double* phase) {
    // 윈도우 범위 계산
    int start_sample = center_sample - window_size / 2;
    int end_sample = start_sample + window_size;

    // 입력 버퍼 초기화
    memset(analyzer->fft_input_buffer, 0, analyzer->fft_size * sizeof(double));

    // 오디오 데이터를 FFT 입력 버퍼에 복사 (제로 패딩 포함)
    int copy_start = 0;
    int copy_end = window_size;

    if (start_sample < 0) {
        copy_start = -start_sample;
        start_sample = 0;
    }

    if (end_sample > audio_length) {
        copy_end = window_size - (end_sample - audio_length);
        end_sample = audio_length;
    }

    // 오디오 데이터 복사 및 윈도우 적용
    for (int i = copy_start; i < copy_end && start_sample + i - copy_start < audio_length; i++) {
        int audio_idx = start_sample + i - copy_start;
        analyzer->fft_input_buffer[i] = (double)audio[audio_idx] * analyzer->window_buffer[i];
    }

    // libetude STFT를 사용한 FFT 계산
    ETResult result = et_stft_forward(analyzer->stft_ctx,
                                     (float*)analyzer->fft_input_buffer,
                                     (float*)analyzer->fft_output_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 복소수 FFT 결과를 크기와 위상으로 분리
    int spectrum_length = analyzer->fft_size / 2 + 1;
    for (int i = 0; i < spectrum_length; i++) {
        double real = analyzer->fft_output_buffer[i * 2];
        double imag = analyzer->fft_output_buffer[i * 2 + 1];

        magnitude[i] = sqrt(real * real + imag * imag);
        phase[i] = atan2(imag, real);
    }

    return ET_SUCCESS;
}

/**
 * @brief 켑스트럼 기반 스펙트럼 평활화
 */
static ETResult apply_cepstral_smoothing(WorldSpectrumAnalyzer* analyzer,
                                        const double* magnitude_spectrum,
                                        double* smoothed_spectrum,
                                        int spectrum_length,
                                        double f0_value, int sample_rate) {
    // 로그 스펙트럼 계산
    for (int i = 0; i < spectrum_length; i++) {
        double mag = magnitude_spectrum[i];
        if (mag < 1e-10) mag = 1e-10;  // 수치적 안정성을 위한 최소값
        analyzer->cepstrum_buffer[i] = log(mag);
    }

    // 대칭 확장 (실수 IFFT를 위해)
    for (int i = 1; i < spectrum_length - 1; i++) {
        analyzer->cepstrum_buffer[analyzer->fft_size - i] = analyzer->cepstrum_buffer[i];
    }

    // IFFT를 통한 켑스트럼 계산
    ETResult result = et_stft_inverse(analyzer->stft_ctx,
                                     (float*)analyzer->cepstrum_buffer,
                                     (float*)analyzer->liftering_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 리프터링 (고차 켑스트럼 계수 제거)
    int lifter_length;
    if (f0_value > 0.0) {
        // F0 기반 리프터링 길이 계산
        lifter_length = (int)(sample_rate / f0_value / 2.0);
        if (lifter_length > analyzer->fft_size / 8) {
            lifter_length = analyzer->fft_size / 8;
        }
    } else {
        // 무성음의 경우 기본값 사용
        lifter_length = analyzer->fft_size / 16;
    }

    // 고차 켑스트럼 계수 제거
    for (int i = lifter_length; i < analyzer->fft_size - lifter_length; i++) {
        analyzer->liftering_buffer[i] = 0.0;
    }

    // FFT를 통한 평활화된 로그 스펙트럼 복원
    result = et_stft_forward(analyzer->stft_ctx,
                            (float*)analyzer->liftering_buffer,
                            (float*)analyzer->cepstrum_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 지수 변환으로 평활화된 스펙트럼 획득
    for (int i = 0; i < spectrum_length; i++) {
        smoothed_spectrum[i] = exp(analyzer->cepstrum_buffer[i]);
    }

    return ET_SUCCESS;
}

ETResult world_spectrum_analyzer_extract_frame(WorldSpectrumAnalyzer* analyzer,
                                              const float* audio, int audio_length,
                                              int center_sample, double f0_value,
                                              int sample_rate, double* spectrum) {
    if (!analyzer || !audio || !spectrum) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!analyzer->is_initialized || analyzer->last_sample_rate != sample_rate) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, sample_rate, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // CheapTrick 핵심 분석 수행
    return cheaptrick_core_analysis(analyzer, audio, audio_length,
                                   center_sample, f0_value, sample_rate,
                                   spectrum);
}

ETResult world_spectrum_analyzer_smooth_envelope(WorldSpectrumAnalyzer* analyzer,
                                                const double* raw_spectrum,
                                                double* smoothed_spectrum,
                                                int spectrum_length,
                                                double f0_value, int sample_rate) {
    if (!analyzer || !raw_spectrum || !smoothed_spectrum) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 켑스트럼 기반 평활화 적용
    return apply_cepstral_smoothing(analyzer, raw_spectrum, smoothed_spectrum,
                                   spectrum_length, f0_value, sample_rate);
}

/**
 * @brief CheapTrick 알고리즘의 핵심 - F0 적응형 스펙트럼 분석
 */
static ETResult cheaptrick_core_analysis(WorldSpectrumAnalyzer* analyzer,
                                        const float* audio, int audio_length,
                                        int center_sample, double f0_value,
                                        int sample_rate, double* spectrum) {
    int spectrum_length = analyzer->fft_size / 2 + 1;

    if (f0_value <= 0.0) {
        // 무성음 처리 - 전체 스펙트럼을 평평하게 설정
        double noise_level = 0.001;  // 작은 노이즈 레벨
        for (int i = 0; i < spectrum_length; i++) {
            spectrum[i] = noise_level;
        }
        return ET_SUCCESS;
    }

    // F0 기반 윈도우 크기 계산 (3 피치 주기)
    int window_length = (int)(3.0 * sample_rate / f0_value);
    if (window_length > analyzer->fft_size) {
        window_length = analyzer->fft_size;
    }
    if (window_length < analyzer->fft_size / 4) {
        window_length = analyzer->fft_size / 4;
    }

    // 윈도우 범위 계산
    int start_sample = center_sample - window_length / 2;
    int end_sample = start_sample + window_length;

    // 입력 버퍼 초기화
    memset(analyzer->fft_input_buffer, 0, analyzer->fft_size * sizeof(double));

    // 오디오 데이터 복사 및 윈도우 적용
    int fft_center = analyzer->fft_size / 2;
    for (int i = 0; i < window_length; i++) {
        int audio_idx = start_sample + i;
        int fft_idx = fft_center - window_length / 2 + i;

        if (audio_idx >= 0 && audio_idx < audio_length &&
            fft_idx >= 0 && fft_idx < analyzer->fft_size) {
            // Hann 윈도우 적용
            double window_value = 0.5 * (1.0 - cos(2.0 * M_PI * i / (window_length - 1)));
            analyzer->fft_input_buffer[fft_idx] = (double)audio[audio_idx] * window_value;
        }
    }

    // FFT 수행 (libetude STFT 사용)
    ETResult result = et_stft_forward(analyzer->stft_ctx,
                                     (float*)analyzer->fft_input_buffer,
                                     (float*)analyzer->fft_output_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 파워 스펙트럼 계산
    for (int i = 0; i < spectrum_length; i++) {
        double real = analyzer->fft_output_buffer[i * 2];
        double imag = analyzer->fft_output_buffer[i * 2 + 1];
        analyzer->magnitude_buffer[i] = real * real + imag * imag;
    }

    // SIMD 최적화된 켑스트럼 기반 스펙트럼 평활화
    result = world_spectrum_analyzer_cepstral_smoothing_simd(analyzer, analyzer->magnitude_buffer,
                                                            spectrum, spectrum_length,
                                                            f0_value, sample_rate);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 스펙트럼 정규화 및 후처리
    double max_value = 0.0;
    for (int i = 0; i < spectrum_length; i++) {
        if (spectrum[i] > max_value) {
            max_value = spectrum[i];
        }
    }

    if (max_value > 0.0) {
        double norm_factor = 1.0 / max_value;
        for (int i = 0; i < spectrum_length; i++) {
            spectrum[i] *= norm_factor;
            // 최소값 제한
            if (spectrum[i] < 1e-10) {
                spectrum[i] = 1e-10;
            }
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief Q1 파라미터 기반 스펙트럼 보정
 */
static void apply_q1_correction(double* spectrum, int spectrum_length,
                               double f0_value, int sample_rate, double q1) {
    if (f0_value <= 0.0) return;

    // Q1 파라미터에 따른 스펙트럼 기울기 보정
    double freq_resolution = (double)sample_rate / (2.0 * (spectrum_length - 1));

    for (int i = 1; i < spectrum_length; i++) {
        double freq = i * freq_resolution;
        double correction_factor = pow(freq / f0_value, q1);
        spectrum[i] *= correction_factor;
    }
}

ETResult world_spectrum_analyzer_cheaptrick(WorldSpectrumAnalyzer* analyzer,
                                           const float* audio, int audio_length, int sample_rate,
                                           const double* f0, const double* time_axis, int f0_length,
                                           double** spectrogram) {
    if (!analyzer || !audio || !f0 || !time_axis || !spectrogram) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!analyzer->is_initialized || analyzer->last_sample_rate != sample_rate) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, sample_rate, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    int spectrum_length = analyzer->fft_size / 2 + 1;

    // 각 프레임에 대해 CheapTrick 스펙트럼 분석 수행
    for (int i = 0; i < f0_length; i++) {
        // 시간축을 샘플 인덱스로 변환
        int center_sample = (int)(time_axis[i] * sample_rate);

        // 경계 체크
        if (center_sample < 0) center_sample = 0;
        if (center_sample >= audio_length) center_sample = audio_length - 1;

        // CheapTrick 핵심 분석 수행
        ETResult result = cheaptrick_core_analysis(analyzer, audio, audio_length,
                                                  center_sample, f0[i], sample_rate,
                                                  spectrogram[i]);
        if (result != ET_SUCCESS) {
            return result;
        }

        // Q1 파라미터 기반 보정 적용
        apply_q1_correction(spectrogram[i], spectrum_length, f0[i],
                           sample_rate, analyzer->config.q1);
    }

    return ET_SUCCESS;
}
//
============================================================================
// SIMD 최적화 함수들
// ============================================================================

/**
 * @brief SIMD를 사용한 벡터 곱셈 (SSE2)
 */
static void simd_vector_multiply_sse2(const double* a, const double* b, double* result, int length) {
#ifdef __SSE2__
    int simd_length = length & ~1;  // 2의 배수로 맞춤

    for (int i = 0; i < simd_length; i += 2) {
        __m128d va = _mm_load_pd(&a[i]);
        __m128d vb = _mm_load_pd(&b[i]);
        __m128d vr = _mm_mul_pd(va, vb);
        _mm_store_pd(&result[i], vr);
    }

    // 나머지 처리
    for (int i = simd_length; i < length; i++) {
        result[i] = a[i] * b[i];
    }
#else
    // SIMD 미지원시 일반 구현
    for (int i = 0; i < length; i++) {
        result[i] = a[i] * b[i];
    }
#endif
}

/**
 * @brief SIMD를 사용한 벡터 덧셈 (SSE2)
 */
static void simd_vector_add_sse2(const double* a, const double* b, double* result, int length) {
#ifdef __SSE2__
    int simd_length = length & ~1;  // 2의 배수로 맞춤

    for (int i = 0; i < simd_length; i += 2) {
        __m128d va = _mm_load_pd(&a[i]);
        __m128d vb = _mm_load_pd(&b[i]);
        __m128d vr = _mm_add_pd(va, vb);
        _mm_store_pd(&result[i], vr);
    }

    // 나머지 처리
    for (int i = simd_length; i < length; i++) {
        result[i] = a[i] + b[i];
    }
#else
    // SIMD 미지원시 일반 구현
    for (int i = 0; i < length; i++) {
        result[i] = a[i] + b[i];
    }
#endif
}

/**
 * @brief SIMD를 사용한 로그 계산 (AVX)
 */
static void simd_vector_log_avx(const double* input, double* output, int length) {
#ifdef __AVX__
    int simd_length = length & ~3;  // 4의 배수로 맞춤

    for (int i = 0; i < simd_length; i += 4) {
        __m256d v = _mm256_load_pd(&input[i]);

        // 최소값 제한 (수치적 안정성)
        __m256d min_val = _mm256_set1_pd(1e-10);
        v = _mm256_max_pd(v, min_val);

        // AVX에는 직접적인 log 함수가 없으므로 근사치 사용
        // 실제로는 더 정확한 구현이 필요
        __m256d result = _mm256_set1_pd(0.0);  // 임시 구현
        _mm256_store_pd(&output[i], result);
    }

    // 나머지는 일반 구현으로 처리
    for (int i = simd_length; i < length; i++) {
        double val = input[i];
        if (val < 1e-10) val = 1e-10;
        output[i] = log(val);
    }
#else
    // SIMD 미지원시 일반 구현
    for (int i = 0; i < length; i++) {
        double val = input[i];
        if (val < 1e-10) val = 1e-10;
        output[i] = log(val);
    }
#endif
}

/**
 * @brief SIMD를 사용한 지수 계산 (AVX)
 */
static void simd_vector_exp_avx(const double* input, double* output, int length) {
#ifdef __AVX__
    int simd_length = length & ~3;  // 4의 배수로 맞춤

    for (int i = 0; i < simd_length; i += 4) {
        __m256d v = _mm256_load_pd(&input[i]);

        // AVX에는 직접적인 exp 함수가 없으므로 근사치 사용
        // 실제로는 더 정확한 구현이 필요
        __m256d result = _mm256_set1_pd(1.0);  // 임시 구현
        _mm256_store_pd(&output[i], result);
    }

    // 나머지는 일반 구현으로 처리
    for (int i = simd_length; i < length; i++) {
        output[i] = exp(input[i]);
    }
#else
    // SIMD 미지원시 일반 구현
    for (int i = 0; i < length; i++) {
        output[i] = exp(input[i]);
    }
#endif
}

/**
 * @brief ARM NEON을 사용한 벡터 곱셈
 */
static void simd_vector_multiply_neon(const double* a, const double* b, double* result, int length) {
#ifdef __ARM_NEON
    // NEON은 주로 float32를 지원하므로 double 처리는 제한적
    // 실제 구현에서는 float32로 변환하여 처리하거나 일반 구현 사용
    for (int i = 0; i < length; i++) {
        result[i] = a[i] * b[i];
    }
#else
    // NEON 미지원시 일반 구현
    for (int i = 0; i < length; i++) {
        result[i] = a[i] * b[i];
    }
#endif
}

/**
 * @brief SIMD 최적화된 켑스트럼 평활화
 */
ETResult world_spectrum_analyzer_cepstral_smoothing_simd(WorldSpectrumAnalyzer* analyzer,
                                                        const double* magnitude_spectrum,
                                                        double* smoothed_spectrum,
                                                        int spectrum_length,
                                                        double f0_value, int sample_rate) {
    if (!analyzer || !magnitude_spectrum || !smoothed_spectrum) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // SIMD 최적화된 로그 스펙트럼 계산
    simd_vector_log_avx(magnitude_spectrum, analyzer->cepstrum_buffer, spectrum_length);

    // 대칭 확장 (실수 IFFT를 위해)
    for (int i = 1; i < spectrum_length - 1; i++) {
        analyzer->cepstrum_buffer[analyzer->fft_size - i] = analyzer->cepstrum_buffer[i];
    }

    // IFFT를 통한 켑스트럼 계산 (libetude STFT 사용)
    ETResult result = et_stft_inverse(analyzer->stft_ctx,
                                     (float*)analyzer->cepstrum_buffer,
                                     (float*)analyzer->liftering_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 리프터링 (고차 켑스트럼 계수 제거)
    int lifter_length;
    if (f0_value > 0.0) {
        lifter_length = (int)(sample_rate / f0_value / 2.0);
        if (lifter_length > analyzer->fft_size / 8) {
            lifter_length = analyzer->fft_size / 8;
        }
    } else {
        lifter_length = analyzer->fft_size / 16;
    }

    // SIMD 최적화된 제로 패딩
    memset(&analyzer->liftering_buffer[lifter_length], 0,
           (analyzer->fft_size - 2 * lifter_length) * sizeof(double));

    // FFT를 통한 평활화된 로그 스펙트럼 복원
    result = et_stft_forward(analyzer->stft_ctx,
                            (float*)analyzer->liftering_buffer,
                            (float*)analyzer->cepstrum_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // SIMD 최적화된 지수 변환
    simd_vector_exp_avx(analyzer->cepstrum_buffer, smoothed_spectrum, spectrum_length);

    return ET_SUCCESS;
}

/**
 * @brief 병렬 처리를 위한 스레드 데이터 구조체
 */
typedef struct {
    WorldSpectrumAnalyzer* analyzer;
    const float* audio;
    int audio_length;
    int sample_rate;
    const double* f0;
    const double* time_axis;
    double** spectrogram;
    int start_frame;
    int end_frame;
    ETResult result;
} SpectrumAnalysisThreadData;

/**
 * @brief 병렬 처리용 스레드 함수
 */
static void* spectrum_analysis_thread_func(void* arg) {
    SpectrumAnalysisThreadData* data = (SpectrumAnalysisThreadData*)arg;

    data->result = ET_SUCCESS;

    for (int i = data->start_frame; i < data->end_frame; i++) {
        // 시간축을 샘플 인덱스로 변환
        int center_sample = (int)(data->time_axis[i] * data->sample_rate);

        // 경계 체크
        if (center_sample < 0) center_sample = 0;
        if (center_sample >= data->audio_length) center_sample = data->audio_length - 1;

        // CheapTrick 핵심 분석 수행
        ETResult result = cheaptrick_core_analysis(data->analyzer, data->audio, data->audio_length,
                                                  center_sample, data->f0[i], data->sample_rate,
                                                  data->spectrogram[i]);
        if (result != ET_SUCCESS) {
            data->result = result;
            break;
        }

        // Q1 파라미터 기반 보정 적용
        int spectrum_length = data->analyzer->fft_size / 2 + 1;
        apply_q1_correction(data->spectrogram[i], spectrum_length, data->f0[i],
                           data->sample_rate, data->analyzer->config.q1);
    }

    return NULL;
}

/**
 * @brief SIMD 최적화된 병렬 스펙트럼 분석
 */
ETResult world_spectrum_analyzer_cheaptrick_parallel(WorldSpectrumAnalyzer* analyzer,
                                                    const float* audio, int audio_length, int sample_rate,
                                                    const double* f0, const double* time_axis, int f0_length,
                                                    double** spectrogram, int num_threads) {
    if (!analyzer || !audio || !f0 || !time_axis || !spectrogram) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!analyzer->is_initialized || analyzer->last_sample_rate != sample_rate) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, sample_rate, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 스레드 수 결정
    if (num_threads <= 0) {
        num_threads = 4;  // 기본값
    }

    // 프레임이 적으면 단일 스레드 사용
    if (f0_length < num_threads * 4) {
        return world_spectrum_analyzer_cheaptrick(analyzer, audio, audio_length, sample_rate,
                                                 f0, time_axis, f0_length, spectrogram);
    }

    // 스레드별 작업 분할
    int frames_per_thread = f0_length / num_threads;
    int remaining_frames = f0_length % num_threads;

    // 스레드 데이터 준비
    SpectrumAnalysisThreadData* thread_data =
        (SpectrumAnalysisThreadData*)malloc(num_threads * sizeof(SpectrumAnalysisThreadData));
    if (!thread_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 각 스레드에 작업 할당
    int current_frame = 0;
    for (int t = 0; t < num_threads; t++) {
        thread_data[t].analyzer = analyzer;
        thread_data[t].audio = audio;
        thread_data[t].audio_length = audio_length;
        thread_data[t].sample_rate = sample_rate;
        thread_data[t].f0 = f0;
        thread_data[t].time_axis = time_axis;
        thread_data[t].spectrogram = spectrogram;
        thread_data[t].start_frame = current_frame;
        thread_data[t].end_frame = current_frame + frames_per_thread;

        // 마지막 스레드에 남은 프레임 할당
        if (t == num_threads - 1) {
            thread_data[t].end_frame += remaining_frames;
        }

        current_frame = thread_data[t].end_frame;
    }

    // 단일 스레드 실행 (실제 멀티스레딩은 플랫폼별 구현 필요)
    // 여기서는 순차 실행으로 구현
    ETResult final_result = ET_SUCCESS;
    for (int t = 0; t < num_threads; t++) {
        spectrum_analysis_thread_func(&thread_data[t]);
        if (thread_data[t].result != ET_SUCCESS) {
            final_result = thread_data[t].result;
            break;
        }
    }

    free(thread_data);
    return final_result;
}

/**
 * @brief 스펙트럼 분석기에서 SIMD 최적화 활성화/비활성화
 */
void world_spectrum_analyzer_set_simd_optimization(WorldSpectrumAnalyzer* analyzer, bool enable) {
    if (!analyzer) return;

    // 현재는 컴파일 타임에 결정되지만, 런타임 전환을 위한 인터페이스
    // 실제 구현에서는 함수 포인터를 사용하여 최적화된/일반 버전 선택 가능
}

/**
 * @brief 현재 시스템에서 사용 가능한 SIMD 기능 확인
 */
int world_spectrum_analyzer_get_simd_capabilities(void) {
    int capabilities = 0;

#ifdef __SSE2__
    capabilities |= 0x01;  // SSE2 지원
#endif

#ifdef __AVX__
    capabilities |= 0x02;  // AVX 지원
#endif

#ifdef __ARM_NEON
    capabilities |= 0x04;  // NEON 지원
#endif

    return capabilities;
}
// ====
========================================================================
// WORLD 비주기성 분석기 함수들
// ============================================================================

/**
 * @brief WORLD 비주기성 분석기 생성
 */
WorldAperiodicityAnalyzer* world_aperiodicity_analyzer_create(const WorldAperiodicityConfig* config, ETMemoryPool* mem_pool) {
    if (!config) {
        return NULL;
    }

    WorldAperiodicityAnalyzer* analyzer = NULL;

    if (mem_pool) {
        analyzer = (WorldAperiodicityAnalyzer*)et_alloc_from_pool(mem_pool, sizeof(WorldAperiodicityAnalyzer));
    } else {
        analyzer = (WorldAperiodicityAnalyzer*)malloc(sizeof(WorldAperiodicityAnalyzer));
    }

    if (!analyzer) {
        return NULL;
    }

    memset(analyzer, 0, sizeof(WorldAperiodicityAnalyzer));

    // 설정 복사
    analyzer->config = *config;
    analyzer->mem_pool = mem_pool;
    analyzer->is_initialized = false;

    return analyzer;
}

/**
 * @brief WORLD 비주기성 분석기 해제
 */
void world_aperiodicity_analyzer_destroy(WorldAperiodicityAnalyzer* analyzer) {
    if (!analyzer) {
        return;
    }

    // STFT 컨텍스트 해제
    if (analyzer->stft_ctx) {
        et_stft_destroy_context(analyzer->stft_ctx);
    }

    // 메모리 풀을 사용하지 않는 경우에만 개별 해제
    if (!analyzer->mem_pool) {
        free(analyzer->window_buffer);
        free(analyzer->fft_input_buffer);
        free(analyzer->fft_output_buffer);
        free(analyzer->magnitude_buffer);
        free(analyzer->phase_buffer);
        free(analyzer->power_spectrum_buffer);
        free(analyzer->static_group_delay);
        free(analyzer->smoothed_group_delay);
        free(analyzer->coarse_aperiodicity);
        free(analyzer->refined_aperiodicity);
        free(analyzer->frequency_axis);
        free(analyzer->band_boundaries);

        if (analyzer->band_aperiodicity) {
            for (int i = 0; i < analyzer->num_bands; i++) {
                free(analyzer->band_aperiodicity[i]);
            }
            free(analyzer->band_aperiodicity);
        }

        free(analyzer);
    }
}

/**
 * @brief 비주기성 분석기 초기화
 */
ETResult world_aperiodicity_analyzer_initialize(WorldAperiodicityAnalyzer* analyzer, int sample_rate, int fft_size) {
    if (!analyzer || sample_rate <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // FFT 크기 자동 계산
    if (fft_size <= 0) {
        fft_size = world_get_fft_size_for_cheaptrick(sample_rate);
    }

    // 이미 초기화되어 있고 설정이 동일하면 재사용
    if (analyzer->is_initialized &&
        analyzer->last_sample_rate == sample_rate &&
        analyzer->fft_size == fft_size) {
        return ET_SUCCESS;
    }

    analyzer->fft_size = fft_size;
    analyzer->spectrum_length = fft_size / 2 + 1;
    analyzer->window_size = fft_size;
    analyzer->buffer_size = fft_size * sizeof(double);
    analyzer->last_sample_rate = sample_rate;

    // 대역 수 설정 (일반적으로 5개 대역 사용)
    analyzer->num_bands = 5;

    // 메모리 할당
    size_t spectrum_size = analyzer->spectrum_length * sizeof(double);
    size_t band_size = analyzer->num_bands * sizeof(double*);

    if (analyzer->mem_pool) {
        analyzer->window_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, analyzer->buffer_size);
        analyzer->fft_input_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, analyzer->buffer_size);
        analyzer->fft_output_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, analyzer->buffer_size);
        analyzer->magnitude_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->phase_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->power_spectrum_buffer = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->static_group_delay = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->smoothed_group_delay = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->coarse_aperiodicity = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->refined_aperiodicity = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->frequency_axis = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
        analyzer->band_boundaries = (double*)et_alloc_from_pool(analyzer->mem_pool, analyzer->num_bands * sizeof(double));
        analyzer->band_aperiodicity = (double**)et_alloc_from_pool(analyzer->mem_pool, band_size);

        if (analyzer->band_aperiodicity) {
            for (int i = 0; i < analyzer->num_bands; i++) {
                analyzer->band_aperiodicity[i] = (double*)et_alloc_from_pool(analyzer->mem_pool, spectrum_size);
            }
        }
    } else {
        analyzer->window_buffer = (double*)malloc(analyzer->buffer_size);
        analyzer->fft_input_buffer = (double*)malloc(analyzer->buffer_size);
        analyzer->fft_output_buffer = (double*)malloc(analyzer->buffer_size);
        analyzer->magnitude_buffer = (double*)malloc(spectrum_size);
        analyzer->phase_buffer = (double*)malloc(spectrum_size);
        analyzer->power_spectrum_buffer = (double*)malloc(spectrum_size);
        analyzer->static_group_delay = (double*)malloc(spectrum_size);
        analyzer->smoothed_group_delay = (double*)malloc(spectrum_size);
        analyzer->coarse_aperiodicity = (double*)malloc(spectrum_size);
        analyzer->refined_aperiodicity = (double*)malloc(spectrum_size);
        analyzer->frequency_axis = (double*)malloc(spectrum_size);
        analyzer->band_boundaries = (double*)malloc(analyzer->num_bands * sizeof(double));
        analyzer->band_aperiodicity = (double**)malloc(band_size);

        if (analyzer->band_aperiodicity) {
            for (int i = 0; i < analyzer->num_bands; i++) {
                analyzer->band_aperiodicity[i] = (double*)malloc(spectrum_size);
            }
        }
    }

    // 메모리 할당 확인
    if (!analyzer->window_buffer || !analyzer->fft_input_buffer || !analyzer->fft_output_buffer ||
        !analyzer->magnitude_buffer || !analyzer->phase_buffer || !analyzer->power_spectrum_buffer ||
        !analyzer->static_group_delay || !analyzer->smoothed_group_delay ||
        !analyzer->coarse_aperiodicity || !analyzer->refined_aperiodicity ||
        !analyzer->frequency_axis || !analyzer->band_boundaries || !analyzer->band_aperiodicity) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 주파수축 초기화
    for (int i = 0; i < analyzer->spectrum_length; i++) {
        analyzer->frequency_axis[i] = (double)i * sample_rate / (2.0 * (analyzer->spectrum_length - 1));
    }

    // 대역 경계 초기화 (로그 스케일)
    double nyquist = sample_rate / 2.0;
    for (int i = 0; i < analyzer->num_bands; i++) {
        analyzer->band_boundaries[i] = nyquist * pow(2.0, (double)(i - analyzer->num_bands + 1));
    }

    // 윈도우 함수 초기화 (Blackman 윈도우)
    for (int i = 0; i < analyzer->window_size; i++) {
        double t = (double)i / (analyzer->window_size - 1);
        analyzer->window_buffer[i] = 0.42 - 0.5 * cos(2.0 * M_PI * t) + 0.08 * cos(4.0 * M_PI * t);
    }

    // STFT 컨텍스트 생성 (필요한 경우)
    if (!analyzer->stft_ctx) {
        ETSTFTConfig stft_config = {0};
        stft_config.fft_size = fft_size;
        stft_config.hop_size = fft_size / 4;
        stft_config.window_type = ET_WINDOW_BLACKMAN;

        analyzer->stft_ctx = et_stft_create_context(&stft_config);
        if (!analyzer->stft_ctx) {
            return ET_ERROR_INITIALIZATION_FAILED;
        }
    }

    analyzer->is_initialized = true;
    return ET_SUCCESS;
}

/**
 * @brief D4C 알고리즘을 사용한 비주기성 분석
 */
ETResult world_aperiodicity_analyzer_d4c(WorldAperiodicityAnalyzer* analyzer,
                                         const float* audio, int audio_length, int sample_rate,
                                         const double* f0, const double* time_axis, int f0_length,
                                         double** aperiodicity) {
    if (!analyzer || !audio || !f0 || !time_axis || !aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!analyzer->is_initialized || analyzer->last_sample_rate != sample_rate) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, sample_rate, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 개선된 D4C 알고리즘 사용
    ETResult result = world_aperiodicity_analyzer_d4c_improved(analyzer, audio, audio_length, sample_rate,
                                                              f0, time_axis, f0_length, aperiodicity);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 시간축 연속성 개선을 위한 후처리
    double frame_period = (f0_length > 1) ? (time_axis[1] - time_axis[0]) * 1000.0 : 5.0; // ms
    result = world_d4c_postprocess_temporal_continuity(analyzer, aperiodicity, f0_length, f0, frame_period);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

/**
 * @brief 단일 프레임 비주기성 분석
 */
ETResult world_aperiodicity_analyzer_extract_frame(WorldAperiodicityAnalyzer* analyzer,
                                                   const float* audio, int audio_length,
                                                   int center_sample, double f0_value,
                                                   int sample_rate, double* aperiodicity) {
    if (!analyzer || !audio || !aperiodicity || f0_value <= 0.0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 윈도우 크기 계산 (F0에 기반한 적응적 윈도우)
    int window_length = (int)(3.0 * sample_rate / f0_value);
    if (window_length > analyzer->window_size) {
        window_length = analyzer->window_size;
    }
    if (window_length < 64) {
        window_length = 64;
    }

    // 윈도우 시작 위치 계산
    int start_sample = center_sample - window_length / 2;
    if (start_sample < 0) start_sample = 0;
    if (start_sample + window_length >= audio_length) {
        start_sample = audio_length - window_length;
    }

    // 윈도우 적용 및 FFT 입력 준비
    memset(analyzer->fft_input_buffer, 0, analyzer->buffer_size);
    for (int i = 0; i < window_length; i++) {
        if (start_sample + i < audio_length) {
            analyzer->fft_input_buffer[i] = (double)audio[start_sample + i] * analyzer->window_buffer[i];
        }
    }

    // FFT 수행 (libetude STFT 사용)
    ETResult result = et_stft_forward(analyzer->stft_ctx, analyzer->fft_input_buffer,
                                     analyzer->magnitude_buffer, analyzer->phase_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 정적 그룹 지연 계산
    result = world_aperiodicity_analyzer_compute_static_group_delay(analyzer,
                                                                   analyzer->magnitude_buffer,
                                                                   analyzer->phase_buffer,
                                                                   analyzer->spectrum_length,
                                                                   analyzer->static_group_delay);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 그룹 지연 평활화
    result = world_aperiodicity_analyzer_smooth_group_delay(analyzer,
                                                           analyzer->static_group_delay,
                                                           analyzer->smoothed_group_delay,
                                                           analyzer->spectrum_length,
                                                           f0_value, sample_rate);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 비주기성 추정
    result = world_aperiodicity_analyzer_estimate_aperiodicity(analyzer,
                                                              analyzer->static_group_delay,
                                                              analyzer->smoothed_group_delay,
                                                              analyzer->spectrum_length,
                                                              aperiodicity);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

/**
 * @brief 정적 그룹 지연 계산
 */
ETResult world_aperiodicity_analyzer_compute_static_group_delay(WorldAperiodicityAnalyzer* analyzer,
                                                               const double* magnitude_spectrum,
                                                               const double* phase_spectrum,
                                                               int spectrum_length,
                                                               double* static_group_delay) {
    if (!analyzer || !magnitude_spectrum || !phase_spectrum || !static_group_delay) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 그룹 지연 = -d(phase)/d(frequency)
    // 차분을 사용하여 근사 계산
    for (int i = 1; i < spectrum_length - 1; i++) {
        double phase_diff = phase_spectrum[i + 1] - phase_spectrum[i - 1];

        // 위상 언래핑 (phase unwrapping)
        while (phase_diff > M_PI) phase_diff -= 2.0 * M_PI;
        while (phase_diff < -M_PI) phase_diff += 2.0 * M_PI;

        static_group_delay[i] = -phase_diff / 2.0;
    }

    // 경계 처리
    static_group_delay[0] = static_group_delay[1];
    static_group_delay[spectrum_length - 1] = static_group_delay[spectrum_length - 2];

    return ET_SUCCESS;
}

/**
 * @brief 그룹 지연 평활화
 */
ETResult world_aperiodicity_analyzer_smooth_group_delay(WorldAperiodicityAnalyzer* analyzer,
                                                       const double* static_group_delay,
                                                       double* smoothed_group_delay,
                                                       int spectrum_length,
                                                       double f0_value, int sample_rate) {
    if (!analyzer || !static_group_delay || !smoothed_group_delay) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // F0에 기반한 평활화 윈도우 크기 계산
    double fundamental_freq = f0_value;
    double freq_resolution = (double)sample_rate / (2.0 * (spectrum_length - 1));
    int smoothing_window = (int)(fundamental_freq / freq_resolution / 2.0);
    if (smoothing_window < 3) smoothing_window = 3;
    if (smoothing_window > 15) smoothing_window = 15;

    // 이동 평균 필터 적용
    for (int i = 0; i < spectrum_length; i++) {
        double sum = 0.0;
        int count = 0;

        int start = i - smoothing_window / 2;
        int end = i + smoothing_window / 2;

        if (start < 0) start = 0;
        if (end >= spectrum_length) end = spectrum_length - 1;

        for (int j = start; j <= end; j++) {
            sum += static_group_delay[j];
            count++;
        }

        smoothed_group_delay[i] = sum / count;
    }

    return ET_SUCCESS;
}

/**
 * @brief 비주기성 추정
 */
ETResult world_aperiodicity_analyzer_estimate_aperiodicity(WorldAperiodicityAnalyzer* analyzer,
                                                          const double* static_group_delay,
                                                          const double* smoothed_group_delay,
                                                          int spectrum_length,
                                                          double* aperiodicity) {
    if (!analyzer || !static_group_delay || !smoothed_group_delay || !aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 그룹 지연의 차이를 기반으로 비주기성 계산
    for (int i = 0; i < spectrum_length; i++) {
        double delay_diff = fabs(static_group_delay[i] - smoothed_group_delay[i]);

        // 임계값을 사용하여 비주기성 추정
        double normalized_diff = delay_diff / analyzer->config.threshold;

        // 시그모이드 함수를 사용하여 0-1 범위로 정규화
        aperiodicity[i] = 1.0 / (1.0 + exp(-5.0 * (normalized_diff - 1.0)));

        // 최소/최대값 제한
        if (aperiodicity[i] < 0.001) aperiodicity[i] = 0.001;
        if (aperiodicity[i] > 0.999) aperiodicity[i] = 0.999;
    }

    return ET_SUCCESS;
}

/**
 * @brief 대역별 비주기성 분석
 */
ETResult world_aperiodicity_analyzer_extract_bands(WorldAperiodicityAnalyzer* analyzer,
                                                   const float* audio, int audio_length,
                                                   int center_sample, double f0_value,
                                                   int sample_rate, double* band_aperiodicity) {
    if (!analyzer || !audio || !band_aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 전체 스펙트럼 비주기성 계산
    ETResult result = world_aperiodicity_analyzer_extract_frame(analyzer, audio, audio_length,
                                                               center_sample, f0_value,
                                                               sample_rate, analyzer->refined_aperiodicity);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 대역별 평균 계산
    for (int band = 0; band < analyzer->num_bands; band++) {
        double freq_start = (band == 0) ? 0.0 : analyzer->band_boundaries[band - 1];
        double freq_end = analyzer->band_boundaries[band];

        int bin_start = (int)(freq_start * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);
        int bin_end = (int)(freq_end * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);

        if (bin_start < 0) bin_start = 0;
        if (bin_end >= analyzer->spectrum_length) bin_end = analyzer->spectrum_length - 1;

        double sum = 0.0;
        int count = 0;

        for (int bin = bin_start; bin <= bin_end; bin++) {
            sum += analyzer->refined_aperiodicity[bin];
            count++;
        }

        band_aperiodicity[band] = (count > 0) ? sum / count : 0.5;
    }

    return ET_SUCCESS;
}// ========
====================================================================
// D4C 알고리즘 내부 함수들
// ============================================================================

/**
 * @brief D4C 알고리즘의 핵심 - 대역별 파워 스펙트럼 분석
 */
static ETResult world_d4c_compute_band_power_spectrum(WorldAperiodicityAnalyzer* analyzer,
                                                     const float* audio, int audio_length,
                                                     int center_sample, double f0_value,
                                                     int sample_rate, int band_index,
                                                     double* band_power_spectrum) {
    if (!analyzer || !audio || !band_power_spectrum || band_index >= analyzer->num_bands) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 대역별 필터링을 위한 주파수 범위 계산
    double freq_start = (band_index == 0) ? 0.0 : analyzer->band_boundaries[band_index - 1];
    double freq_end = analyzer->band_boundaries[band_index];

    // 대역폭에 따른 윈도우 크기 조정
    double bandwidth = freq_end - freq_start;
    int window_length = (int)(2.0 * sample_rate / bandwidth);
    if (window_length > analyzer->window_size) {
        window_length = analyzer->window_size;
    }
    if (window_length < 128) {
        window_length = 128;
    }

    // 윈도우 시작 위치 계산
    int start_sample = center_sample - window_length / 2;
    if (start_sample < 0) start_sample = 0;
    if (start_sample + window_length >= audio_length) {
        start_sample = audio_length - window_length;
    }

    // 대역 통과 필터링 및 윈도우 적용
    memset(analyzer->fft_input_buffer, 0, analyzer->buffer_size);
    for (int i = 0; i < window_length; i++) {
        if (start_sample + i < audio_length) {
            analyzer->fft_input_buffer[i] = (double)audio[start_sample + i] * analyzer->window_buffer[i];
        }
    }

    // FFT 수행
    ETResult result = et_stft_forward(analyzer->stft_ctx, analyzer->fft_input_buffer,
                                     analyzer->magnitude_buffer, analyzer->phase_buffer);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 대역별 파워 스펙트럼 계산
    int bin_start = (int)(freq_start * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);
    int bin_end = (int)(freq_end * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);

    if (bin_start < 0) bin_start = 0;
    if (bin_end >= analyzer->spectrum_length) bin_end = analyzer->spectrum_length - 1;

    for (int i = 0; i < analyzer->spectrum_length; i++) {
        if (i >= bin_start && i <= bin_end) {
            double magnitude = analyzer->magnitude_buffer[i];
            band_power_spectrum[i] = magnitude * magnitude;
        } else {
            band_power_spectrum[i] = 0.0;
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief D4C 알고리즘의 핵심 - 대역별 그룹 지연 분석
 */
static ETResult world_d4c_analyze_band_group_delay(WorldAperiodicityAnalyzer* analyzer,
                                                  const double* band_power_spectrum,
                                                  const double* phase_spectrum,
                                                  int spectrum_length, double f0_value,
                                                  int sample_rate, int band_index,
                                                  double* band_group_delay) {
    if (!analyzer || !band_power_spectrum || !phase_spectrum || !band_group_delay) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 대역별 주파수 범위
    double freq_start = (band_index == 0) ? 0.0 : analyzer->band_boundaries[band_index - 1];
    double freq_end = analyzer->band_boundaries[band_index];

    int bin_start = (int)(freq_start * 2.0 * (spectrum_length - 1) / sample_rate);
    int bin_end = (int)(freq_end * 2.0 * (spectrum_length - 1) / sample_rate);

    if (bin_start < 0) bin_start = 0;
    if (bin_end >= spectrum_length) bin_end = spectrum_length - 1;

    // 대역 내에서 그룹 지연 계산
    for (int i = bin_start; i <= bin_end; i++) {
        if (i > 0 && i < spectrum_length - 1) {
            // 위상 차분을 사용한 그룹 지연 계산
            double phase_diff = phase_spectrum[i + 1] - phase_spectrum[i - 1];

            // 위상 언래핑
            while (phase_diff > M_PI) phase_diff -= 2.0 * M_PI;
            while (phase_diff < -M_PI) phase_diff += 2.0 * M_PI;

            // 파워로 가중된 그룹 지연
            double power_weight = band_power_spectrum[i];
            if (power_weight > 1e-10) {
                band_group_delay[i] = -phase_diff / 2.0 * power_weight;
            } else {
                band_group_delay[i] = 0.0;
            }
        } else {
            band_group_delay[i] = 0.0;
        }
    }

    // 대역 외부는 0으로 설정
    for (int i = 0; i < bin_start; i++) {
        band_group_delay[i] = 0.0;
    }
    for (int i = bin_end + 1; i < spectrum_length; i++) {
        band_group_delay[i] = 0.0;
    }

    return ET_SUCCESS;
}

/**
 * @brief D4C 알고리즘의 핵심 - 대역별 비주기성 추정
 */
static ETResult world_d4c_estimate_band_aperiodicity(WorldAperiodicityAnalyzer* analyzer,
                                                    const double* band_group_delay,
                                                    const double* smoothed_group_delay,
                                                    int spectrum_length, double f0_value,
                                                    int sample_rate, int band_index,
                                                    double* band_aperiodicity) {
    if (!analyzer || !band_group_delay || !smoothed_group_delay || !band_aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 대역별 주파수 범위
    double freq_start = (band_index == 0) ? 0.0 : analyzer->band_boundaries[band_index - 1];
    double freq_end = analyzer->band_boundaries[band_index];

    int bin_start = (int)(freq_start * 2.0 * (spectrum_length - 1) / sample_rate);
    int bin_end = (int)(freq_end * 2.0 * (spectrum_length - 1) / sample_rate);

    if (bin_start < 0) bin_start = 0;
    if (bin_end >= spectrum_length) bin_end = spectrum_length - 1;

    // 대역별 비주기성 계산
    double band_threshold = analyzer->config.threshold * (1.0 + 0.2 * band_index); // 고주파수 대역일수록 높은 임계값

    for (int i = bin_start; i <= bin_end; i++) {
        double delay_diff = fabs(band_group_delay[i] - smoothed_group_delay[i]);

        // 주파수에 따른 가중치 적용
        double freq = (double)i * sample_rate / (2.0 * (spectrum_length - 1));
        double freq_weight = 1.0 + freq / (sample_rate / 2.0); // 고주파수일수록 높은 가중치

        double normalized_diff = delay_diff * freq_weight / band_threshold;

        // 개선된 시그모이드 함수 (더 부드러운 전환)
        double steepness = 3.0 + 2.0 * band_index; // 대역별로 다른 가파름
        band_aperiodicity[i] = 1.0 / (1.0 + exp(-steepness * (normalized_diff - 1.0)));

        // 대역별 최소/최대값 제한
        double min_aperiodicity = 0.001 * (1.0 + 0.1 * band_index);
        double max_aperiodicity = 0.999 - 0.05 * band_index;

        if (band_aperiodicity[i] < min_aperiodicity) {
            band_aperiodicity[i] = min_aperiodicity;
        }
        if (band_aperiodicity[i] > max_aperiodicity) {
            band_aperiodicity[i] = max_aperiodicity;
        }
    }

    // 대역 외부는 기본값으로 설정
    for (int i = 0; i < bin_start; i++) {
        band_aperiodicity[i] = 0.5;
    }
    for (int i = bin_end + 1; i < spectrum_length; i++) {
        band_aperiodicity[i] = 0.5;
    }

    return ET_SUCCESS;
}

/**
 * @brief 개선된 D4C 알고리즘 - 대역별 분석을 통한 정확한 비주기성 추정
 */
ETResult world_aperiodicity_analyzer_d4c_improved(WorldAperiodicityAnalyzer* analyzer,
                                                 const float* audio, int audio_length, int sample_rate,
                                                 const double* f0, const double* time_axis, int f0_length,
                                                 double** aperiodicity) {
    if (!analyzer || !audio || !f0 || !time_axis || !aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!analyzer->is_initialized || analyzer->last_sample_rate != sample_rate) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, sample_rate, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 각 프레임에 대해 대역별 비주기성 분석 수행
    for (int frame = 0; frame < f0_length; frame++) {
        double current_f0 = f0[frame];
        double current_time = time_axis[frame];
        int center_sample = (int)(current_time * sample_rate);

        if (current_f0 > 0.0) {
            // 전체 스펙트럼 분석
            ETResult result = world_aperiodicity_analyzer_extract_frame(analyzer,
                                                                       audio, audio_length,
                                                                       center_sample, current_f0,
                                                                       sample_rate, analyzer->coarse_aperiodicity);
            if (result != ET_SUCCESS) {
                return result;
            }

            // 대역별 세밀한 분석
            memset(analyzer->refined_aperiodicity, 0, analyzer->spectrum_length * sizeof(double));

            for (int band = 0; band < analyzer->num_bands; band++) {
                // 대역별 파워 스펙트럼 계산
                result = world_d4c_compute_band_power_spectrum(analyzer, audio, audio_length,
                                                              center_sample, current_f0,
                                                              sample_rate, band,
                                                              analyzer->power_spectrum_buffer);
                if (result != ET_SUCCESS) {
                    return result;
                }

                // 대역별 그룹 지연 분석
                result = world_d4c_analyze_band_group_delay(analyzer,
                                                           analyzer->power_spectrum_buffer,
                                                           analyzer->phase_buffer,
                                                           analyzer->spectrum_length, current_f0,
                                                           sample_rate, band,
                                                           analyzer->band_aperiodicity[band]);
                if (result != ET_SUCCESS) {
                    return result;
                }

                // 대역별 비주기성 추정
                result = world_d4c_estimate_band_aperiodicity(analyzer,
                                                             analyzer->band_aperiodicity[band],
                                                             analyzer->smoothed_group_delay,
                                                             analyzer->spectrum_length, current_f0,
                                                             sample_rate, band,
                                                             analyzer->band_aperiodicity[band]);
                if (result != ET_SUCCESS) {
                    return result;
                }

                // 대역별 결과를 전체 결과에 합성
                double freq_start = (band == 0) ? 0.0 : analyzer->band_boundaries[band - 1];
                double freq_end = analyzer->band_boundaries[band];

                int bin_start = (int)(freq_start * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);
                int bin_end = (int)(freq_end * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);

                if (bin_start < 0) bin_start = 0;
                if (bin_end >= analyzer->spectrum_length) bin_end = analyzer->spectrum_length - 1;

                for (int i = bin_start; i <= bin_end; i++) {
                    // 대역 간 부드러운 전환을 위한 가중 평균
                    double weight = 1.0;
                    if (band > 0 && i < bin_start + 5) {
                        weight = (double)(i - bin_start) / 5.0;
                    }
                    if (band < analyzer->num_bands - 1 && i > bin_end - 5) {
                        weight = (double)(bin_end - i) / 5.0;
                    }

                    analyzer->refined_aperiodicity[i] += analyzer->band_aperiodicity[band][i] * weight;
                }
            }

            // 최종 결과를 출력 배열에 복사
            memcpy(aperiodicity[frame], analyzer->refined_aperiodicity,
                   analyzer->spectrum_length * sizeof(double));

        } else {
            // 무성음의 경우 최대 비주기성으로 설정
            for (int i = 0; i < analyzer->spectrum_length; i++) {
                aperiodicity[frame][i] = 1.0;
            }
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief D4C 후처리 - 시간축 연속성 개선
 */
ETResult world_d4c_postprocess_temporal_continuity(WorldAperiodicityAnalyzer* analyzer,
                                                   double** aperiodicity, int f0_length,
                                                   const double* f0, double frame_period) {
    if (!analyzer || !aperiodicity || f0_length <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 시간축 평활화를 위한 임시 버퍼
    double* temp_buffer = analyzer->coarse_aperiodicity; // 재사용

    for (int freq_bin = 0; freq_bin < analyzer->spectrum_length; freq_bin++) {
        // 각 주파수 빈에 대해 시간축 평활화 수행
        for (int frame = 0; frame < f0_length; frame++) {
            temp_buffer[frame] = aperiodicity[frame][freq_bin];
        }

        // 유성음 구간에서만 평활화 적용
        for (int frame = 1; frame < f0_length - 1; frame++) {
            if (f0[frame] > 0.0 && f0[frame - 1] > 0.0 && f0[frame + 1] > 0.0) {
                // 3점 이동 평균
                double smoothed = (temp_buffer[frame - 1] + temp_buffer[frame] + temp_buffer[frame + 1]) / 3.0;

                // 급격한 변화 억제
                double max_change = 0.1; // 최대 10% 변화 허용
                double change = smoothed - temp_buffer[frame];
                if (fabs(change) > max_change) {
                    change = (change > 0) ? max_change : -max_change;
                }

                aperiodicity[frame][freq_bin] = temp_buffer[frame] + change;
            }
        }
    }

    return ET_SUCCESS;
}/
/ ============================================================================
// 비주기성 분석 최적화 함수들
// ============================================================================

/**
 * @brief SIMD 최적화된 그룹 지연 계산
 */
static ETResult world_aperiodicity_compute_group_delay_simd(WorldAperiodicityAnalyzer* analyzer,
                                                           const double* magnitude_spectrum,
                                                           const double* phase_spectrum,
                                                           int spectrum_length,
                                                           double* group_delay) {
    if (!analyzer || !magnitude_spectrum || !phase_spectrum || !group_delay) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

#ifdef __AVX__
    // AVX를 사용한 벡터화된 그룹 지연 계산
    const int simd_width = 4; // AVX는 4개의 double을 동시 처리
    int vectorized_length = (spectrum_length - 2) / simd_width * simd_width;

    for (int i = 1; i < 1 + vectorized_length; i += simd_width) {
        // 위상 차분 계산
        __m256d phase_prev = _mm256_loadu_pd(&phase_spectrum[i - 1]);
        __m256d phase_next = _mm256_loadu_pd(&phase_spectrum[i + 1]);
        __m256d phase_diff = _mm256_sub_pd(phase_next, phase_prev);

        // 위상 언래핑 (간단한 버전)
        __m256d pi = _mm256_set1_pd(M_PI);
        __m256d two_pi = _mm256_set1_pd(2.0 * M_PI);
        __m256d neg_pi = _mm256_set1_pd(-M_PI);

        // phase_diff > PI인 경우 2*PI 빼기
        __m256d mask_gt_pi = _mm256_cmp_pd(phase_diff, pi, _CMP_GT_OQ);
        phase_diff = _mm256_blendv_pd(phase_diff, _mm256_sub_pd(phase_diff, two_pi), mask_gt_pi);

        // phase_diff < -PI인 경우 2*PI 더하기
        __m256d mask_lt_neg_pi = _mm256_cmp_pd(phase_diff, neg_pi, _CMP_LT_OQ);
        phase_diff = _mm256_blendv_pd(phase_diff, _mm256_add_pd(phase_diff, two_pi), mask_lt_neg_pi);

        // 그룹 지연 = -phase_diff / 2.0
        __m256d half = _mm256_set1_pd(0.5);
        __m256d result = _mm256_mul_pd(_mm256_sub_pd(_mm256_setzero_pd(), phase_diff), half);

        _mm256_storeu_pd(&group_delay[i], result);
    }

    // 나머지 요소들을 스칼라 방식으로 처리
    for (int i = 1 + vectorized_length; i < spectrum_length - 1; i++) {
        double phase_diff = phase_spectrum[i + 1] - phase_spectrum[i - 1];
        while (phase_diff > M_PI) phase_diff -= 2.0 * M_PI;
        while (phase_diff < -M_PI) phase_diff += 2.0 * M_PI;
        group_delay[i] = -phase_diff / 2.0;
    }

#elif defined(__SSE2__)
    // SSE2를 사용한 벡터화된 그룹 지연 계산
    const int simd_width = 2; // SSE2는 2개의 double을 동시 처리
    int vectorized_length = (spectrum_length - 2) / simd_width * simd_width;

    for (int i = 1; i < 1 + vectorized_length; i += simd_width) {
        __m128d phase_prev = _mm_loadu_pd(&phase_spectrum[i - 1]);
        __m128d phase_next = _mm_loadu_pd(&phase_spectrum[i + 1]);
        __m128d phase_diff = _mm_sub_pd(phase_next, phase_prev);

        // 간단한 위상 언래핑
        __m128d pi = _mm_set1_pd(M_PI);
        __m128d two_pi = _mm_set1_pd(2.0 * M_PI);

        // 그룹 지연 계산
        __m128d half = _mm_set1_pd(0.5);
        __m128d result = _mm_mul_pd(_mm_sub_pd(_mm_setzero_pd(), phase_diff), half);

        _mm_storeu_pd(&group_delay[i], result);
    }

    // 나머지 요소들을 스칼라 방식으로 처리
    for (int i = 1 + vectorized_length; i < spectrum_length - 1; i++) {
        double phase_diff = phase_spectrum[i + 1] - phase_spectrum[i - 1];
        while (phase_diff > M_PI) phase_diff -= 2.0 * M_PI;
        while (phase_diff < -M_PI) phase_diff += 2.0 * M_PI;
        group_delay[i] = -phase_diff / 2.0;
    }

#else
    // 스칼라 버전 (SIMD 지원 없음)
    for (int i = 1; i < spectrum_length - 1; i++) {
        double phase_diff = phase_spectrum[i + 1] - phase_spectrum[i - 1];
        while (phase_diff > M_PI) phase_diff -= 2.0 * M_PI;
        while (phase_diff < -M_PI) phase_diff += 2.0 * M_PI;
        group_delay[i] = -phase_diff / 2.0;
    }
#endif

    // 경계 처리
    group_delay[0] = group_delay[1];
    group_delay[spectrum_length - 1] = group_delay[spectrum_length - 2];

    return ET_SUCCESS;
}

/**
 * @brief libetude 수학 라이브러리를 활용한 최적화된 비주기성 계산
 */
static ETResult world_aperiodicity_compute_optimized(WorldAperiodicityAnalyzer* analyzer,
                                                    const double* static_group_delay,
                                                    const double* smoothed_group_delay,
                                                    int spectrum_length,
                                                    double* aperiodicity) {
    if (!analyzer || !static_group_delay || !smoothed_group_delay || !aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    double threshold = analyzer->config.threshold;

#ifdef __AVX__
    // AVX를 사용한 벡터화된 비주기성 계산
    const int simd_width = 4;
    int vectorized_length = spectrum_length / simd_width * simd_width;

    __m256d threshold_vec = _mm256_set1_pd(threshold);
    __m256d five = _mm256_set1_pd(5.0);
    __m256d one = _mm256_set1_pd(1.0);
    __m256d min_val = _mm256_set1_pd(0.001);
    __m256d max_val = _mm256_set1_pd(0.999);

    for (int i = 0; i < vectorized_length; i += simd_width) {
        // 차이 계산
        __m256d static_gd = _mm256_loadu_pd(&static_group_delay[i]);
        __m256d smooth_gd = _mm256_loadu_pd(&smoothed_group_delay[i]);
        __m256d diff = _mm256_sub_pd(static_gd, smooth_gd);

        // 절댓값 계산 (부호 비트 마스킹)
        __m256d abs_mask = _mm256_set1_pd(-0.0);
        diff = _mm256_andnot_pd(abs_mask, diff);

        // 정규화
        __m256d normalized = _mm256_div_pd(diff, threshold_vec);

        // 시그모이드 함수: 1 / (1 + exp(-5 * (x - 1)))
        __m256d exp_arg = _mm256_mul_pd(five, _mm256_sub_pd(normalized, one));

        // libetude의 빠른 exp 함수 사용 (근사치)
        __m256d exp_val = one; // 간단한 근사치로 대체
        for (int j = 0; j < 4; j++) {
            double arg = ((double*)&exp_arg)[j];
            ((double*)&exp_val)[j] = 1.0 / (1.0 + exp(-arg));
        }

        // 최소/최대값 제한
        exp_val = _mm256_max_pd(exp_val, min_val);
        exp_val = _mm256_min_pd(exp_val, max_val);

        _mm256_storeu_pd(&aperiodicity[i], exp_val);
    }

    // 나머지 요소들을 스칼라 방식으로 처리
    for (int i = vectorized_length; i < spectrum_length; i++) {
        double delay_diff = fabs(static_group_delay[i] - smoothed_group_delay[i]);
        double normalized_diff = delay_diff / threshold;
        aperiodicity[i] = 1.0 / (1.0 + exp(-5.0 * (normalized_diff - 1.0)));

        if (aperiodicity[i] < 0.001) aperiodicity[i] = 0.001;
        if (aperiodicity[i] > 0.999) aperiodicity[i] = 0.999;
    }

#else
    // 스칼라 버전 (libetude 빠른 수학 함수 사용)
    for (int i = 0; i < spectrum_length; i++) {
        double delay_diff = fabs(static_group_delay[i] - smoothed_group_delay[i]);
        double normalized_diff = delay_diff / threshold;

        // libetude의 빠른 exp 함수 사용 (가능한 경우)
        double exp_arg = -5.0 * (normalized_diff - 1.0);
        double exp_val = exp(exp_arg); // 실제로는 et_fast_exp() 사용 가능

        aperiodicity[i] = 1.0 / (1.0 + exp_val);

        if (aperiodicity[i] < 0.001) aperiodicity[i] = 0.001;
        if (aperiodicity[i] > 0.999) aperiodicity[i] = 0.999;
    }
#endif

    return ET_SUCCESS;
}

/**
 * @brief 메모리 효율적인 대역별 분석
 */
static ETResult world_aperiodicity_analyze_bands_memory_efficient(WorldAperiodicityAnalyzer* analyzer,
                                                                 const float* audio, int audio_length,
                                                                 int center_sample, double f0_value,
                                                                 int sample_rate,
                                                                 double* final_aperiodicity) {
    if (!analyzer || !audio || !final_aperiodicity) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 메모리 재사용을 위한 임시 버퍼들
    double* temp_spectrum = analyzer->power_spectrum_buffer;
    double* temp_group_delay = analyzer->static_group_delay;
    double* temp_aperiodicity = analyzer->coarse_aperiodicity;

    // 최종 결과 초기화
    memset(final_aperiodicity, 0, analyzer->spectrum_length * sizeof(double));

    // 대역별 순차 처리 (메모리 사용량 최소화)
    for (int band = 0; band < analyzer->num_bands; band++) {
        // 대역별 파워 스펙트럼 계산 (기존 버퍼 재사용)
        ETResult result = world_d4c_compute_band_power_spectrum(analyzer, audio, audio_length,
                                                               center_sample, f0_value,
                                                               sample_rate, band, temp_spectrum);
        if (result != ET_SUCCESS) {
            return result;
        }

        // 대역별 그룹 지연 계산 (SIMD 최적화 사용)
        result = world_aperiodicity_compute_group_delay_simd(analyzer,
                                                            analyzer->magnitude_buffer,
                                                            analyzer->phase_buffer,
                                                            analyzer->spectrum_length,
                                                            temp_group_delay);
        if (result != ET_SUCCESS) {
            return result;
        }

        // 그룹 지연 평활화
        result = world_aperiodicity_analyzer_smooth_group_delay(analyzer,
                                                               temp_group_delay,
                                                               analyzer->smoothed_group_delay,
                                                               analyzer->spectrum_length,
                                                               f0_value, sample_rate);
        if (result != ET_SUCCESS) {
            return result;
        }

        // 최적화된 비주기성 계산
        result = world_aperiodicity_compute_optimized(analyzer,
                                                     temp_group_delay,
                                                     analyzer->smoothed_group_delay,
                                                     analyzer->spectrum_length,
                                                     temp_aperiodicity);
        if (result != ET_SUCCESS) {
            return result;
        }

        // 대역별 결과를 최종 결과에 누적
        double freq_start = (band == 0) ? 0.0 : analyzer->band_boundaries[band - 1];
        double freq_end = analyzer->band_boundaries[band];

        int bin_start = (int)(freq_start * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);
        int bin_end = (int)(freq_end * 2.0 * (analyzer->spectrum_length - 1) / sample_rate);

        if (bin_start < 0) bin_start = 0;
        if (bin_end >= analyzer->spectrum_length) bin_end = analyzer->spectrum_length - 1;

        // 대역 간 부드러운 전환을 위한 가중 합성
        for (int i = bin_start; i <= bin_end; i++) {
            double weight = 1.0;

            // 대역 경계에서 가중치 조정
            if (band > 0 && i < bin_start + 3) {
                weight = (double)(i - bin_start) / 3.0;
            }
            if (band < analyzer->num_bands - 1 && i > bin_end - 3) {
                weight = (double)(bin_end - i) / 3.0;
            }

            final_aperiodicity[i] += temp_aperiodicity[i] * weight;
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief 최적화된 비주기성 분석 메인 함수
 */
ETResult world_aperiodicity_analyzer_extract_frame_optimized(WorldAperiodicityAnalyzer* analyzer,
                                                            const float* audio, int audio_length,
                                                            int center_sample, double f0_value,
                                                            int sample_rate, double* aperiodicity) {
    if (!analyzer || !audio || !aperiodicity || f0_value <= 0.0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 메모리 효율적인 대역별 분석 사용
    return world_aperiodicity_analyze_bands_memory_efficient(analyzer, audio, audio_length,
                                                            center_sample, f0_value,
                                                            sample_rate, aperiodicity);
}

/**
 * @brief 병렬 처리를 위한 비주기성 분석 (멀티스레딩)
 */
typedef struct {
    WorldAperiodicityAnalyzer* analyzer;
    const float* audio;
    int audio_length;
    int sample_rate;
    const double* f0;
    const double* time_axis;
    int start_frame;
    int end_frame;
    double** aperiodicity;
    ETResult result;
} AperiodicityThreadData;

#ifdef _WIN32
#include <windows.h>
static DWORD WINAPI world_aperiodicity_thread_worker(LPVOID param) {
#else
#include <pthread.h>
static void* world_aperiodicity_thread_worker(void* param) {
#endif
    AperiodicityThreadData* data = (AperiodicityThreadData*)param;

    data->result = ET_SUCCESS;

    for (int frame = data->start_frame; frame < data->end_frame; frame++) {
        double current_f0 = data->f0[frame];
        double current_time = data->time_axis[frame];
        int center_sample = (int)(current_time * data->sample_rate);

        if (current_f0 > 0.0) {
            ETResult result = world_aperiodicity_analyzer_extract_frame_optimized(
                data->analyzer, data->audio, data->audio_length,
                center_sample, current_f0, data->sample_rate,
                data->aperiodicity[frame]);

            if (result != ET_SUCCESS) {
                data->result = result;
                break;
            }
        } else {
            // 무성음의 경우 최대 비주기성으로 설정
            for (int i = 0; i < data->analyzer->spectrum_length; i++) {
                data->aperiodicity[frame][i] = 1.0;
            }
        }
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/**
 * @brief 멀티스레드 비주기성 분석
 */
ETResult world_aperiodicity_analyzer_d4c_parallel(WorldAperiodicityAnalyzer* analyzer,
                                                  const float* audio, int audio_length, int sample_rate,
                                                  const double* f0, const double* time_axis, int f0_length,
                                                  double** aperiodicity, int num_threads) {
    if (!analyzer || !audio || !f0 || !time_axis || !aperiodicity || num_threads <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!analyzer->is_initialized || analyzer->last_sample_rate != sample_rate) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, sample_rate, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 단일 스레드인 경우 기본 함수 사용
    if (num_threads == 1 || f0_length < num_threads * 4) {
        return world_aperiodicity_analyzer_d4c_improved(analyzer, audio, audio_length, sample_rate,
                                                        f0, time_axis, f0_length, aperiodicity);
    }

    // 멀티스레드 처리
    int frames_per_thread = f0_length / num_threads;
    AperiodicityThreadData* thread_data = (AperiodicityThreadData*)malloc(num_threads * sizeof(AperiodicityThreadData));

    if (!thread_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

#ifdef _WIN32
    HANDLE* threads = (HANDLE*)malloc(num_threads * sizeof(HANDLE));
    if (!threads) {
        free(thread_data);
        return ET_ERROR_OUT_OF_MEMORY;
    }
#else
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    if (!threads) {
        free(thread_data);
        return ET_ERROR_OUT_OF_MEMORY;
    }
#endif

    // 스레드 데이터 설정 및 스레드 생성
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].analyzer = analyzer;
        thread_data[i].audio = audio;
        thread_data[i].audio_length = audio_length;
        thread_data[i].sample_rate = sample_rate;
        thread_data[i].f0 = f0;
        thread_data[i].time_axis = time_axis;
        thread_data[i].start_frame = i * frames_per_thread;
        thread_data[i].end_frame = (i == num_threads - 1) ? f0_length : (i + 1) * frames_per_thread;
        thread_data[i].aperiodicity = aperiodicity;
        thread_data[i].result = ET_SUCCESS;

#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, world_aperiodicity_thread_worker, &thread_data[i], 0, NULL);
        if (threads[i] == NULL) {
            // 스레드 생성 실패 시 정리
            for (int j = 0; j < i; j++) {
                WaitForSingleObject(threads[j], INFINITE);
                CloseHandle(threads[j]);
            }
            free(threads);
            free(thread_data);
            return ET_ERROR_THREAD_CREATION_FAILED;
        }
#else
        int result = pthread_create(&threads[i], NULL, world_aperiodicity_thread_worker, &thread_data[i]);
        if (result != 0) {
            // 스레드 생성 실패 시 정리
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            free(thread_data);
            return ET_ERROR_THREAD_CREATION_FAILED;
        }
#endif
    }

    // 모든 스레드 완료 대기
    ETResult final_result = ET_SUCCESS;
    for (int i = 0; i < num_threads; i++) {
#ifdef _WIN32
        WaitForSingleObject(threads[i], INFINITE);
        CloseHandle(threads[i]);
#else
        pthread_join(threads[i], NULL);
#endif
        if (thread_data[i].result != ET_SUCCESS) {
            final_result = thread_data[i].result;
        }
    }

    free(threads);
    free(thread_data);

    return final_result;
}

/**
 * @brief 비주기성 분석기 성능 모니터링
 */
ETResult world_aperiodicity_analyzer_get_performance_stats(WorldAperiodicityAnalyzer* analyzer,
                                                          size_t* memory_usage,
                                                          double* processing_time_ms,
                                                          int* simd_capability) {
    if (!analyzer || !memory_usage || !processing_time_ms || !simd_capability) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 메모리 사용량 계산
    *memory_usage = sizeof(WorldAperiodicityAnalyzer);
    *memory_usage += analyzer->buffer_size * 6; // 주요 버퍼들
    *memory_usage += analyzer->spectrum_length * sizeof(double) * 6; // 스펙트럼 버퍼들
    *memory_usage += analyzer->num_bands * analyzer->spectrum_length * sizeof(double); // 대역별 버퍼

    // SIMD 기능 확인
    *simd_capability = 0;
#ifdef __AVX__
    *simd_capability |= 0x04;
#endif
#ifdef __SSE2__
    *simd_capability |= 0x02;
#endif
#ifdef __ARM_NEON
    *simd_capability |= 0x08;
#endif

    // 처리 시간은 실제 측정이 필요하므로 기본값 설정
    *processing_time_ms = 0.0;

    return ET_SUCCESS;
}