#include "libetude/audio_effects.h"
#include "libetude/memory.h"
#include "libetude/fast_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 리버브 플러그인 내부 상태
typedef struct {
    AudioEffectConfig config;
    ReverbParams params;

    // 딜레이 라인들
    float* delay_lines[8];           // 8개의 딜레이 라인
    int delay_lengths[8];            // 각 딜레이 라인의 길이
    int delay_indices[8];            // 현재 딜레이 인덱스

    // 올패스 필터들 (확산용)
    float* allpass_buffers[4];       // 4개의 올패스 필터
    int allpass_lengths[4];          // 올패스 필터 길이
    int allpass_indices[4];          // 올패스 인덱스
    float allpass_feedback[4];       // 올패스 피드백 계수

    // 콤 필터들 (색상용)
    float* comb_buffers[8];          // 8개의 콤 필터
    int comb_lengths[8];             // 콤 필터 길이
    int comb_indices[8];             // 콤 인덱스
    float comb_feedback[8];          // 콤 피드백 계수
    float comb_damping[8];           // 댐핑 계수
    float comb_filter_store[8];      // 댐핑 필터 상태

    // 프리 딜레이
    float* pre_delay_buffer;         // 프리 딜레이 버퍼
    int pre_delay_length;            // 프리 딜레이 길이
    int pre_delay_index;             // 프리 딜레이 인덱스

    // 필터 상태
    float high_cut_state[2];         // 고주파 컷 필터 상태
    float low_cut_state[2];          // 저주파 컷 필터 상태

    // 분석 데이터
    AudioAnalysisData analysis;
    bool analysis_enabled;

    // 메모리 풀
    ETMemoryPool* mem_pool;
} ReverbState;

// 기본 리버브 파라미터
static const ReverbParams default_reverb_params = {
    .room_size = 0.5f,
    .damping = 0.5f,
    .pre_delay = 20.0f,
    .decay_time = 2.0f,
    .early_reflections = 0.3f,
    .late_reverb = 0.7f,
    .high_cut = 8000.0f,
    .low_cut = 100.0f
};

// 콤 필터 길이 (샘플 단위, 44.1kHz 기준)
static const int comb_lengths_44k[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static const int allpass_lengths_44k[] = {556, 441, 341, 225};

// 1차 로우패스 필터 (댐핑용)
static inline float lowpass_filter(float input, float cutoff, float* state) {
    float alpha = cutoff;
    *state = *state + alpha * (input - *state);
    return *state;
}

// 1차 하이패스 필터
static inline float highpass_filter(float input, float cutoff, float* state) {
    float alpha = cutoff;
    float output = input - *state;
    *state = *state + alpha * output;
    return output;
}

// 올패스 필터 처리
static inline float process_allpass(float input, float* buffer, int length, int* index, float feedback) {
    int idx = *index;
    float delayed = buffer[idx];
    float output = -input + delayed;
    buffer[idx] = input + feedback * delayed;

    *index = (idx + 1) % length;
    return output;
}

// 콤 필터 처리
static inline float process_comb(float input, float* buffer, int length, int* index,
                                float feedback, float damping, float* filter_state) {
    int idx = *index;
    float delayed = buffer[idx];

    // 댐핑 필터 적용
    delayed = lowpass_filter(delayed, damping, filter_state);

    float output = delayed;
    buffer[idx] = input + feedback * delayed;

    *index = (idx + 1) % length;
    return output;
}

// 리버브 플러그인 초기화
static PluginError reverb_initialize(PluginContext* ctx, const void* config) {
    if (!ctx || !config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const AudioEffectConfig* effect_config = (const AudioEffectConfig*)config;

    // 상태 구조체 할당
    ReverbState* state = (ReverbState*)malloc(sizeof(ReverbState));
    if (!state) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(state, 0, sizeof(ReverbState));
    state->config = *effect_config;
    state->params = default_reverb_params;

    // 메모리 풀 생성
    state->mem_pool = et_create_memory_pool(1024 * 1024, 16); // 1MB 풀
    if (!state->mem_pool) {
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    float sample_rate = effect_config->sample_rate;
    float scale_factor = sample_rate / 44100.0f;

    // 콤 필터 버퍼 할당
    for (int i = 0; i < 8; i++) {
        state->comb_lengths[i] = (int)(comb_lengths_44k[i] * scale_factor);
        state->comb_buffers[i] = (float*)et_alloc_from_pool(state->mem_pool,
                                                           state->comb_lengths[i] * sizeof(float));
        if (!state->comb_buffers[i]) {
            // 정리 후 오류 반환
            et_destroy_memory_pool(state->mem_pool);
            free(state);
            return ET_ERROR_OUT_OF_MEMORY;
        }
        memset(state->comb_buffers[i], 0, state->comb_lengths[i] * sizeof(float));
        state->comb_indices[i] = 0;
        state->comb_feedback[i] = 0.84f;
        state->comb_damping[i] = 0.2f;
        state->comb_filter_store[i] = 0.0f;
    }

    // 올패스 필터 버퍼 할당
    for (int i = 0; i < 4; i++) {
        state->allpass_lengths[i] = (int)(allpass_lengths_44k[i] * scale_factor);
        state->allpass_buffers[i] = (float*)et_alloc_from_pool(state->mem_pool,
                                                              state->allpass_lengths[i] * sizeof(float));
        if (!state->allpass_buffers[i]) {
            et_destroy_memory_pool(state->mem_pool);
            free(state);
            return ET_ERROR_OUT_OF_MEMORY;
        }
        memset(state->allpass_buffers[i], 0, state->allpass_lengths[i] * sizeof(float));
        state->allpass_indices[i] = 0;
        state->allpass_feedback[i] = 0.5f;
    }

    // 프리 딜레이 버퍼 할당
    state->pre_delay_length = (int)(state->params.pre_delay * sample_rate / 1000.0f);
    if (state->pre_delay_length > 0) {
        state->pre_delay_buffer = (float*)et_alloc_from_pool(state->mem_pool,
                                                           state->pre_delay_length * sizeof(float));
        if (!state->pre_delay_buffer) {
            et_destroy_memory_pool(state->mem_pool);
            free(state);
            return ET_ERROR_OUT_OF_MEMORY;
        }
        memset(state->pre_delay_buffer, 0, state->pre_delay_length * sizeof(float));
        state->pre_delay_index = 0;
    }

    // 분석 데이터 초기화
    state->analysis.spectrum_size = 512;
    state->analysis.spectrum = (float*)malloc(state->analysis.spectrum_size * sizeof(float));
    if (!state->analysis.spectrum) {
        et_destroy_memory_pool(state->mem_pool);
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }
    memset(state->analysis.spectrum, 0, state->analysis.spectrum_size * sizeof(float));
    state->analysis_enabled = false;

    ctx->internal_state = state;
    ctx->state_size = sizeof(ReverbState);

    return ET_SUCCESS;
}

// 리버브 플러그인 처리
static PluginError reverb_process(PluginContext* ctx, const float* input, float* output, int num_samples) {
    if (!ctx || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbState* state = (ReverbState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    float wet_dry_mix = state->config.wet_dry_mix;
    bool bypass = state->config.bypass;

    for (int i = 0; i < num_samples; i++) {
        float input_sample = input[i];
        float wet_sample = input_sample;

        if (!bypass) {
            // 프리 딜레이 처리
            if (state->pre_delay_length > 0) {
                float delayed = state->pre_delay_buffer[state->pre_delay_index];
                state->pre_delay_buffer[state->pre_delay_index] = wet_sample;
                state->pre_delay_index = (state->pre_delay_index + 1) % state->pre_delay_length;
                wet_sample = delayed;
            }

            // 초기 반사음 (얼리 리플렉션)
            float early_out = 0.0f;
            for (int j = 0; j < 4; j++) {
                early_out += process_allpass(wet_sample, state->allpass_buffers[j],
                                           state->allpass_lengths[j], &state->allpass_indices[j],
                                           state->allpass_feedback[j]);
            }
            early_out *= state->params.early_reflections;

            // 후기 리버브 (콤 필터들)
            float late_out = 0.0f;
            for (int j = 0; j < 8; j++) {
                late_out += process_comb(wet_sample, state->comb_buffers[j],
                                       state->comb_lengths[j], &state->comb_indices[j],
                                       state->comb_feedback[j], state->comb_damping[j],
                                       &state->comb_filter_store[j]);
            }
            late_out *= state->params.late_reverb / 8.0f;

            // 최종 리버브 출력
            wet_sample = early_out + late_out;

            // 고주파/저주파 필터링
            wet_sample = lowpass_filter(wet_sample, 0.1f, &state->high_cut_state[0]);
            wet_sample = highpass_filter(wet_sample, 0.05f, &state->low_cut_state[0]);
        }

        // 웨트/드라이 믹싱
        output[i] = input_sample * (1.0f - wet_dry_mix) + wet_sample * wet_dry_mix;

        // 분석 데이터 업데이트 (간단한 피크/RMS 계산)
        if (state->analysis_enabled) {
            float abs_sample = fabsf(output[i]);
            if (abs_sample > state->analysis.peak_level) {
                state->analysis.peak_level = abs_sample;
            }
            state->analysis.rms_level = state->analysis.rms_level * 0.999f + abs_sample * abs_sample * 0.001f;
        }
    }

    return ET_SUCCESS;
}

// 리버브 플러그인 종료
static PluginError reverb_finalize(PluginContext* ctx) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbState* state = (ReverbState*)ctx->internal_state;
    if (state) {
        if (state->analysis.spectrum) {
            free(state->analysis.spectrum);
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
static PluginError reverb_set_parameter(PluginContext* ctx, int param_id, PluginParamValue value) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbState* state = (ReverbState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    switch (param_id) {
        case 0: // room_size
            state->params.room_size = value.float_value;
            // 콤 필터 피드백 업데이트
            for (int i = 0; i < 8; i++) {
                state->comb_feedback[i] = 0.7f + state->params.room_size * 0.2f;
            }
            break;
        case 1: // damping
            state->params.damping = value.float_value;
            // 댐핑 계수 업데이트
            for (int i = 0; i < 8; i++) {
                state->comb_damping[i] = state->params.damping * 0.4f;
            }
            break;
        case 2: // wet_dry_mix
            state->config.wet_dry_mix = value.float_value;
            break;
        case 3: // bypass
            state->config.bypass = value.bool_value;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 파라미터 조회
static PluginError reverb_get_parameter(PluginContext* ctx, int param_id, PluginParamValue* value) {
    if (!ctx || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbState* state = (ReverbState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    switch (param_id) {
        case 0: // room_size
            value->float_value = state->params.room_size;
            break;
        case 1: // damping
            value->float_value = state->params.damping;
            break;
        case 2: // wet_dry_mix
            value->float_value = state->config.wet_dry_mix;
            break;
        case 3: // bypass
            value->bool_value = state->config.bypass;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 지연 시간 조회
static PluginError reverb_get_latency(PluginContext* ctx, int* latency_samples) {
    if (!ctx || !latency_samples) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbState* state = (ReverbState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    *latency_samples = state->pre_delay_length;
    return ET_SUCCESS;
}

// 테일 타임 조회
static PluginError reverb_get_tail_time(PluginContext* ctx, float* tail_time_seconds) {
    if (!ctx || !tail_time_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbState* state = (ReverbState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    *tail_time_seconds = state->params.decay_time;
    return ET_SUCCESS;
}

// 리버브 플러그인 파라미터 정의
static PluginParameter reverb_parameters[] = {
    {
        .name = "room_size",
        .display_name = "Room Size",
        .description = "Size of the reverb room",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 1.0f,
            .default_value = 0.5f,
            .step = 0.01f
        }
    },
    {
        .name = "damping",
        .display_name = "Damping",
        .description = "High frequency damping",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 1.0f,
            .default_value = 0.5f,
            .step = 0.01f
        }
    },
    {
        .name = "wet_dry_mix",
        .display_name = "Wet/Dry Mix",
        .description = "Mix between dry and wet signal",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 1.0f,
            .default_value = 0.3f,
            .step = 0.01f
        }
    },
    {
        .name = "bypass",
        .display_name = "Bypass",
        .description = "Bypass the effect",
        .type = PARAM_TYPE_BOOL,
        .value.bool_param = {
            .default_value = false
        }
    }
};

// 리버브 플러그인 메타데이터
static PluginMetadata reverb_metadata = {
    .name = "LibEtude Reverb",
    .description = "High-quality algorithmic reverb effect",
    .author = "LibEtude Team",
    .vendor = "LibEtude",
    .version = {1, 0, 0, 0},
    .api_version = {LIBETUDE_PLUGIN_API_VERSION_MAJOR, LIBETUDE_PLUGIN_API_VERSION_MINOR, LIBETUDE_PLUGIN_API_VERSION_PATCH, 0},
    .type = PLUGIN_TYPE_AUDIO_EFFECT,
    .flags = 0,
    .uuid = "550e8400-e29b-41d4-a716-446655440001",
    .checksum = 0
};

// 리버브 플러그인 생성
PluginInstance* create_reverb_plugin(const ReverbParams* params) {
    PluginInstance* plugin = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!plugin) {
        return NULL;
    }

    memset(plugin, 0, sizeof(PluginInstance));
    plugin->metadata = reverb_metadata;
    plugin->state = PLUGIN_STATE_LOADED;

    // 함수 포인터 설정
    plugin->functions.initialize = reverb_initialize;
    plugin->functions.process = reverb_process;
    plugin->functions.finalize = reverb_finalize;
    plugin->functions.set_parameter = reverb_set_parameter;
    plugin->functions.get_parameter = reverb_get_parameter;
    plugin->functions.get_latency = reverb_get_latency;
    plugin->functions.get_tail_time = reverb_get_tail_time;

    // 파라미터 설정
    plugin->parameters = reverb_parameters;
    plugin->num_parameters = sizeof(reverb_parameters) / sizeof(PluginParameter);
    plugin->param_values = (PluginParamValue*)calloc(plugin->num_parameters, sizeof(PluginParamValue));

    if (plugin->param_values) {
        // 기본값 설정
        plugin->param_values[0].float_value = 0.5f; // room_size
        plugin->param_values[1].float_value = 0.5f; // damping
        plugin->param_values[2].float_value = 0.3f; // wet_dry_mix
        plugin->param_values[3].bool_value = false; // bypass
    }

    return plugin;
}