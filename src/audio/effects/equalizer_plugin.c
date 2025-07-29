#include "libetude/audio_effects.h"
#include "libetude/memory.h"
#include "libetude/fast_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 바이쿼드 필터 계수
typedef struct {
    float b0, b1, b2;  // 분자 계수
    float a1, a2;      // 분모 계수 (a0는 항상 1)
} BiquadCoeffs;

// 바이쿼드 필터 상태
typedef struct {
    float x1, x2;      // 입력 지연
    float y1, y2;      // 출력 지연
} BiquadState;

// EQ 밴드 구조체
typedef struct {
    EQBand params;
    BiquadCoeffs coeffs;
    BiquadState state;
    bool needs_update;
} EQBandProcessor;

// 이퀄라이저 플러그인 내부 상태
typedef struct {
    AudioEffectConfig config;
    EqualizerParams params;

    EQBandProcessor* bands;          // EQ 밴드 프로세서 배열
    int num_bands;                   // 밴드 수

    float sample_rate;               // 샘플링 레이트
    float overall_gain_linear;       // 선형 전체 게인

    // 자동 게인 보정
    float auto_gain_compensation;    // 자동 게인 보정값

    // 분석 데이터
    AudioAnalysisData analysis;
    bool analysis_enabled;
    float* fft_buffer;               // FFT 버퍼
    int fft_size;                    // FFT 크기

    // 메모리 풀
    ETMemoryPool* mem_pool;
} EqualizerState;

// 기본 10밴드 이퀄라이저 주파수
static const float default_eq_frequencies[] = {
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

// dB를 선형 게인으로 변환
static inline float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

// 선형 게인을 dB로 변환
static inline float linear_to_db(float linear) {
    return 20.0f * log10f(fmaxf(linear, 1e-10f));
}

// 피킹 EQ 필터 계수 계산
static void calculate_peaking_coeffs(BiquadCoeffs* coeffs, float freq, float gain_db, float q, float sample_rate) {
    float omega = 2.0f * M_PI * freq / sample_rate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float A = powf(10.0f, gain_db / 40.0f);
    float alpha = sin_omega / (2.0f * q);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cos_omega;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cos_omega;
    float a2 = 1.0f - alpha / A;

    // a0로 정규화
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = a1 / a0;
    coeffs->a2 = a2 / a0;
}

// 로우셸프 필터 계수 계산
static void calculate_lowshelf_coeffs(BiquadCoeffs* coeffs, float freq, float gain_db, float sample_rate) {
    float omega = 2.0f * M_PI * freq / sample_rate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float A = powf(10.0f, gain_db / 40.0f);
    float S = 1.0f; // 셸프 기울기
    float beta = sqrtf(A) / S;

    float b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega + beta * sin_omega);
    float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_omega);
    float b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega - beta * sin_omega);
    float a0 = (A + 1.0f) + (A - 1.0f) * cos_omega + beta * sin_omega;
    float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_omega);
    float a2 = (A + 1.0f) + (A - 1.0f) * cos_omega - beta * sin_omega;

    // a0로 정규화
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = a1 / a0;
    coeffs->a2 = a2 / a0;
}

// 하이셸프 필터 계수 계산
static void calculate_highshelf_coeffs(BiquadCoeffs* coeffs, float freq, float gain_db, float sample_rate) {
    float omega = 2.0f * M_PI * freq / sample_rate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float A = powf(10.0f, gain_db / 40.0f);
    float S = 1.0f; // 셸프 기울기
    float beta = sqrtf(A) / S;

    float b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_omega + beta * sin_omega);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_omega);
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_omega - beta * sin_omega);
    float a0 = (A + 1.0f) - (A - 1.0f) * cos_omega + beta * sin_omega;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_omega);
    float a2 = (A + 1.0f) - (A - 1.0f) * cos_omega - beta * sin_omega;

    // a0로 정규화
    coeffs->b0 = b0 / a0;
    coeffs->b1 = b1 / a0;
    coeffs->b2 = b2 / a0;
    coeffs->a1 = a1 / a0;
    coeffs->a2 = a2 / a0;
}

// EQ 밴드 계수 업데이트
static void update_band_coeffs(EQBandProcessor* band, float sample_rate) {
    if (!band->needs_update) return;

    if (band->params.frequency < 100.0f) {
        // 저주파는 로우셸프 필터
        calculate_lowshelf_coeffs(&band->coeffs, band->params.frequency,
                                 band->params.gain, sample_rate);
    } else if (band->params.frequency > 10000.0f) {
        // 고주파는 하이셸프 필터
        calculate_highshelf_coeffs(&band->coeffs, band->params.frequency,
                                  band->params.gain, sample_rate);
    } else {
        // 중간 주파수는 피킹 필터
        calculate_peaking_coeffs(&band->coeffs, band->params.frequency,
                                band->params.gain, band->params.q_factor, sample_rate);
    }

    band->needs_update = false;
}

// 바이쿼드 필터 처리
static inline float process_biquad(float input, const BiquadCoeffs* coeffs, BiquadState* state) {
    float output = coeffs->b0 * input + coeffs->b1 * state->x1 + coeffs->b2 * state->x2
                  - coeffs->a1 * state->y1 - coeffs->a2 * state->y2;

    // 상태 업데이트
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;

    return output;
}

// 자동 게인 보정 계산
static float calculate_auto_gain_compensation(const EQBandProcessor* bands, int num_bands) {
    float total_gain = 0.0f;
    int active_bands = 0;

    for (int i = 0; i < num_bands; i++) {
        if (bands[i].params.enabled && fabsf(bands[i].params.gain) > 0.1f) {
            total_gain += bands[i].params.gain;
            active_bands++;
        }
    }

    if (active_bands == 0) return 1.0f;

    // 평균 게인의 역수로 보정
    float avg_gain = total_gain / active_bands;
    return db_to_linear(-avg_gain * 0.5f); // 50% 보정
}

// 이퀄라이저 플러그인 초기화
static PluginError equalizer_initialize(PluginContext* ctx, const void* config) {
    if (!ctx || !config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const AudioEffectConfig* effect_config = (const AudioEffectConfig*)config;

    // 상태 구조체 할당
    EqualizerState* state = (EqualizerState*)malloc(sizeof(EqualizerState));
    if (!state) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(state, 0, sizeof(EqualizerState));
    state->config = *effect_config;
    state->sample_rate = effect_config->sample_rate;
    state->num_bands = 10; // 기본 10밴드
    state->overall_gain_linear = 1.0f;
    state->auto_gain_compensation = 1.0f;

    // 메모리 풀 생성
    state->mem_pool = et_create_memory_pool(512 * 1024, 16); // 512KB 풀
    if (!state->mem_pool) {
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // EQ 밴드 할당
    state->bands = (EQBandProcessor*)et_alloc_from_pool(state->mem_pool,
                                                       state->num_bands * sizeof(EQBandProcessor));
    if (!state->bands) {
        et_destroy_memory_pool(state->mem_pool);
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기본 EQ 밴드 설정
    for (int i = 0; i < state->num_bands; i++) {
        state->bands[i].params.frequency = default_eq_frequencies[i];
        state->bands[i].params.gain = 0.0f;
        state->bands[i].params.q_factor = 1.0f;
        state->bands[i].params.enabled = true;
        state->bands[i].needs_update = true;

        // 상태 초기화
        memset(&state->bands[i].state, 0, sizeof(BiquadState));

        // 계수 업데이트
        update_band_coeffs(&state->bands[i], state->sample_rate);
    }

    // 파라미터 구조체 설정
    state->params.bands = (EQBand*)malloc(state->num_bands * sizeof(EQBand));
    if (!state->params.bands) {
        et_destroy_memory_pool(state->mem_pool);
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    for (int i = 0; i < state->num_bands; i++) {
        state->params.bands[i] = state->bands[i].params;
    }
    state->params.num_bands = state->num_bands;
    state->params.overall_gain = 0.0f;
    state->params.auto_gain = false;

    // 분석 데이터 초기화
    state->fft_size = 1024;
    state->analysis.spectrum_size = state->fft_size / 2;
    state->analysis.spectrum = (float*)malloc(state->analysis.spectrum_size * sizeof(float));
    state->fft_buffer = (float*)malloc(state->fft_size * sizeof(float));

    if (!state->analysis.spectrum || !state->fft_buffer) {
        free(state->params.bands);
        et_destroy_memory_pool(state->mem_pool);
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(state->analysis.spectrum, 0, state->analysis.spectrum_size * sizeof(float));
    memset(state->fft_buffer, 0, state->fft_size * sizeof(float));
    state->analysis_enabled = false;

    ctx->internal_state = state;
    ctx->state_size = sizeof(EqualizerState);

    return ET_SUCCESS;
}

// 이퀄라이저 플러그인 처리
static PluginError equalizer_process(PluginContext* ctx, const float* input, float* output, int num_samples) {
    if (!ctx || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    EqualizerState* state = (EqualizerState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    bool bypass = state->config.bypass;

    for (int i = 0; i < num_samples; i++) {
        float sample = input[i];

        if (!bypass) {
            // 각 EQ 밴드를 순차적으로 처리
            for (int band = 0; band < state->num_bands; band++) {
                if (state->bands[band].params.enabled) {
                    // 계수 업데이트 (필요시)
                    update_band_coeffs(&state->bands[band], state->sample_rate);

                    // 바이쿼드 필터 처리
                    sample = process_biquad(sample, &state->bands[band].coeffs,
                                          &state->bands[band].state);
                }
            }

            // 전체 게인 적용
            sample *= state->overall_gain_linear;

            // 자동 게인 보정 적용
            if (state->params.auto_gain) {
                sample *= state->auto_gain_compensation;
            }
        }

        output[i] = sample;

        // 분석 데이터 업데이트
        if (state->analysis_enabled) {
            float abs_sample = fabsf(sample);
            if (abs_sample > state->analysis.peak_level) {
                state->analysis.peak_level = abs_sample;
            }
            state->analysis.rms_level = state->analysis.rms_level * 0.999f + abs_sample * abs_sample * 0.001f;
        }
    }

    return ET_SUCCESS;
}

// 이퀄라이저 플러그인 종료
static PluginError equalizer_finalize(PluginContext* ctx) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    EqualizerState* state = (EqualizerState*)ctx->internal_state;
    if (state) {
        if (state->params.bands) {
            free(state->params.bands);
        }

        if (state->analysis.spectrum) {
            free(state->analysis.spectrum);
        }

        if (state->fft_buffer) {
            free(state->fft_buffer);
        }

        if (state->mem_pool) {
            et_destroy_memory_pool(state->mem_pool);
        }

        free(state);
        ctx->internal_state = NULL;
    }

    return ET_SUCCESS;
}

// 파라미터 설정
static PluginError equalizer_set_parameter(PluginContext* ctx, int param_id, PluginParamValue value) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    EqualizerState* state = (EqualizerState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    if (param_id < 0 || param_id >= state->num_bands * 3 + 3) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (param_id < state->num_bands) {
        // 밴드 게인 설정
        int band_idx = param_id;
        state->bands[band_idx].params.gain = value.float_value;
        state->bands[band_idx].needs_update = true;
        state->params.bands[band_idx].gain = value.float_value;

        // 자동 게인 보정 업데이트
        if (state->params.auto_gain) {
            state->auto_gain_compensation = calculate_auto_gain_compensation(state->bands, state->num_bands);
        }
    } else if (param_id < state->num_bands * 2) {
        // 밴드 주파수 설정
        int band_idx = param_id - state->num_bands;
        state->bands[band_idx].params.frequency = value.float_value;
        state->bands[band_idx].needs_update = true;
        state->params.bands[band_idx].frequency = value.float_value;
    } else if (param_id < state->num_bands * 3) {
        // 밴드 Q 팩터 설정
        int band_idx = param_id - state->num_bands * 2;
        state->bands[band_idx].params.q_factor = value.float_value;
        state->bands[band_idx].needs_update = true;
        state->params.bands[band_idx].q_factor = value.float_value;
    } else {
        // 전역 파라미터
        int global_param = param_id - state->num_bands * 3;
        switch (global_param) {
            case 0: // overall_gain
                state->params.overall_gain = value.float_value;
                state->overall_gain_linear = db_to_linear(value.float_value);
                break;
            case 1: // auto_gain
                state->params.auto_gain = value.bool_value;
                if (state->params.auto_gain) {
                    state->auto_gain_compensation = calculate_auto_gain_compensation(state->bands, state->num_bands);
                } else {
                    state->auto_gain_compensation = 1.0f;
                }
                break;
            case 2: // bypass
                state->config.bypass = value.bool_value;
                break;
            default:
                return ET_ERROR_INVALID_ARGUMENT;
        }
    }

    return ET_SUCCESS;
}

// 파라미터 조회
static PluginError equalizer_get_parameter(PluginContext* ctx, int param_id, PluginParamValue* value) {
    if (!ctx || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    EqualizerState* state = (EqualizerState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    if (param_id < 0 || param_id >= state->num_bands * 3 + 3) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (param_id < state->num_bands) {
        // 밴드 게인 조회
        int band_idx = param_id;
        value->float_value = state->bands[band_idx].params.gain;
    } else if (param_id < state->num_bands * 2) {
        // 밴드 주파수 조회
        int band_idx = param_id - state->num_bands;
        value->float_value = state->bands[band_idx].params.frequency;
    } else if (param_id < state->num_bands * 3) {
        // 밴드 Q 팩터 조회
        int band_idx = param_id - state->num_bands * 2;
        value->float_value = state->bands[band_idx].params.q_factor;
    } else {
        // 전역 파라미터
        int global_param = param_id - state->num_bands * 3;
        switch (global_param) {
            case 0: // overall_gain
                value->float_value = state->params.overall_gain;
                break;
            case 1: // auto_gain
                value->bool_value = state->params.auto_gain;
                break;
            case 2: // bypass
                value->bool_value = state->config.bypass;
                break;
            default:
                return ET_ERROR_INVALID_ARGUMENT;
        }
    }

    return ET_SUCCESS;
}

// 이퀄라이저 플러그인 메타데이터
static PluginMetadata equalizer_metadata = {
    .name = "LibEtude Equalizer",
    .description = "10-band parametric equalizer with auto-gain compensation",
    .author = "LibEtude Team",
    .vendor = "LibEtude",
    .version = {1, 0, 0, 0},
    .api_version = {LIBETUDE_PLUGIN_API_VERSION_MAJOR, LIBETUDE_PLUGIN_API_VERSION_MINOR, LIBETUDE_PLUGIN_API_VERSION_PATCH, 0},
    .type = PLUGIN_TYPE_AUDIO_EFFECT,
    .flags = 0,
    .uuid = "550e8400-e29b-41d4-a716-446655440002",
    .checksum = 0
};

// 이퀄라이저 플러그인 생성
PluginInstance* create_equalizer_plugin(const EqualizerParams* params) {
    PluginInstance* plugin = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!plugin) {
        return NULL;
    }

    memset(plugin, 0, sizeof(PluginInstance));
    plugin->metadata = equalizer_metadata;
    plugin->state = PLUGIN_STATE_LOADED;

    // 함수 포인터 설정
    plugin->functions.initialize = equalizer_initialize;
    plugin->functions.process = equalizer_process;
    plugin->functions.finalize = equalizer_finalize;
    plugin->functions.set_parameter = equalizer_set_parameter;
    plugin->functions.get_parameter = equalizer_get_parameter;

    // 파라미터 수 계산 (10밴드 * 3파라미터 + 3전역파라미터)
    plugin->num_parameters = 10 * 3 + 3;
    plugin->param_values = (PluginParamValue*)calloc(plugin->num_parameters, sizeof(PluginParamValue));

    if (plugin->param_values) {
        // 기본값 설정 (모든 밴드 게인 0dB)
        for (int i = 0; i < 10; i++) {
            plugin->param_values[i].float_value = 0.0f; // 게인
            plugin->param_values[i + 10].float_value = default_eq_frequencies[i]; // 주파수
            plugin->param_values[i + 20].float_value = 1.0f; // Q 팩터
        }
        plugin->param_values[30].float_value = 0.0f; // overall_gain
        plugin->param_values[31].bool_value = false; // auto_gain
        plugin->param_values[32].bool_value = false; // bypass
    }

    return plugin;
}