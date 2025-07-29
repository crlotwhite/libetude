#include "libetude/audio_effects.h"
#include "libetude/memory.h"
#include "libetude/fast_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 딜레이 플러그인 내부 상태
typedef struct {
    AudioEffectConfig config;
    DelayParams params;

    // 딜레이 라인
    float* delay_buffer;             // 딜레이 버퍼
    int delay_buffer_size;           // 딜레이 버퍼 크기
    int write_index;                 // 쓰기 인덱스
    int read_index;                  // 읽기 인덱스

    // 필터 상태 (피드백 필터링용)
    float high_cut_state;            // 고주파 컷 필터 상태
    float low_cut_state;             // 저주파 컷 필터 상태

    // 템포 동기화
    float samples_per_beat;          // 비트당 샘플 수
    int sync_delay_samples;          // 동기화된 딜레이 샘플 수

    // 분석 데이터
    AudioAnalysisData analysis;
    bool analysis_enabled;

    // 메모리 풀
    ETMemoryPool* mem_pool;
} DelayState;

// 기본 딜레이 파라미터
static const DelayParams default_delay_params = {
    .delay_time = 250.0f,            // 250ms
    .feedback = 0.3f,                // 30% 피드백
    .high_cut = 8000.0f,             // 8kHz 고주파 컷
    .low_cut = 100.0f,               // 100Hz 저주파 컷
    .sync_to_tempo = false,          // 템포 동기화 비활성화
    .tempo_bpm = 120.0f              // 120 BPM
};

// 1차 로우패스 필터
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

// 딜레이 시간을 샘플 수로 변환
static int delay_time_to_samples(float delay_ms, float sample_rate) {
    return (int)(delay_ms * sample_rate / 1000.0f);
}

// 템포 동기화된 딜레이 시간 계산
static int calculate_sync_delay(float tempo_bpm, float sample_rate, float note_value) {
    // note_value: 1.0 = 1/4 note, 0.5 = 1/8 note, 2.0 = 1/2 note
    float beat_duration_ms = 60000.0f / tempo_bpm; // 1비트의 지속시간 (ms)
    float delay_ms = beat_duration_ms * note_value;
    return delay_time_to_samples(delay_ms, sample_rate);
}

// 딜레이 플러그인 초기화
static PluginError delay_initialize(PluginContext* ctx, const void* config) {
    if (!ctx || !config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const AudioEffectConfig* effect_config = (const AudioEffectConfig*)config;

    // 상태 구조체 할당
    DelayState* state = (DelayState*)malloc(sizeof(DelayState));
    if (!state) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(state, 0, sizeof(DelayState));
    state->config = *effect_config;
    state->params = default_delay_params;

    // 메모리 풀 생성
    state->mem_pool = et_create_memory_pool(512 * 1024, 16); // 512KB 풀
    if (!state->mem_pool) {
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    float sample_rate = effect_config->sample_rate;

    // 최대 딜레이 시간 (2초)을 위한 버퍼 크기 계산
    int max_delay_samples = (int)(2.0f * sample_rate);
    state->delay_buffer_size = max_delay_samples;

    // 딜레이 버퍼 할당
    state->delay_buffer = (float*)et_alloc_from_pool(state->mem_pool,
                                                    state->delay_buffer_size * sizeof(float));
    if (!state->delay_buffer) {
        et_destroy_memory_pool(state->mem_pool);
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(state->delay_buffer, 0, state->delay_buffer_size * sizeof(float));

    // 초기 딜레이 시간 설정
    int delay_samples = delay_time_to_samples(state->params.delay_time, sample_rate);
    state->write_index = 0;
    state->read_index = (state->write_index - delay_samples + state->delay_buffer_size) % state->delay_buffer_size;

    // 템포 동기화 설정
    if (state->params.sync_to_tempo) {
        state->sync_delay_samples = calculate_sync_delay(state->params.tempo_bpm, sample_rate, 1.0f);
        state->read_index = (state->write_index - state->sync_delay_samples + state->delay_buffer_size) % state->delay_buffer_size;
    }

    // 분석 데이터 초기화
    state->analysis.spectrum_size = 256;
    state->analysis.spectrum = (float*)malloc(state->analysis.spectrum_size * sizeof(float));
    if (!state->analysis.spectrum) {
        et_destroy_memory_pool(state->mem_pool);
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }
    memset(state->analysis.spectrum, 0, state->analysis.spectrum_size * sizeof(float));
    state->analysis_enabled = false;

    ctx->internal_state = state;
    ctx->state_size = sizeof(DelayState);

    return ET_SUCCESS;
}

// 딜레이 플러그인 처리
static PluginError delay_process(PluginContext* ctx, const float* input, float* output, int num_samples) {
    if (!ctx || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DelayState* state = (DelayState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    float wet_dry_mix = state->config.wet_dry_mix;
    bool bypass = state->config.bypass;

    for (int i = 0; i < num_samples; i++) {
        float input_sample = input[i];
        float wet_sample = input_sample;

        if (!bypass) {
            // 딜레이 버퍼에서 읽기
            float delayed_sample = state->delay_buffer[state->read_index];

            // 피드백 필터링
            delayed_sample = lowpass_filter(delayed_sample, 0.1f, &state->high_cut_state);
            delayed_sample = highpass_filter(delayed_sample, 0.05f, &state->low_cut_state);

            // 피드백과 함께 딜레이 버퍼에 쓰기
            float feedback_sample = input_sample + delayed_sample * state->params.feedback;
            state->delay_buffer[state->write_index] = feedback_sample;

            // 웨트 신호 생성
            wet_sample = delayed_sample;

            // 인덱스 업데이트
            state->write_index = (state->write_index + 1) % state->delay_buffer_size;
            state->read_index = (state->read_index + 1) % state->delay_buffer_size;
        }

        // 웨트/드라이 믹싱
        output[i] = input_sample * (1.0f - wet_dry_mix) + wet_sample * wet_dry_mix;

        // 분석 데이터 업데이트
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

// 딜레이 플러그인 종료
static PluginError delay_finalize(PluginContext* ctx) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DelayState* state = (DelayState*)ctx->internal_state;
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
static PluginError delay_set_parameter(PluginContext* ctx, int param_id, PluginParamValue value) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DelayState* state = (DelayState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    switch (param_id) {
        case 0: // delay_time
            state->params.delay_time = value.float_value;
            if (!state->params.sync_to_tempo) {
                int delay_samples = delay_time_to_samples(state->params.delay_time, state->config.sample_rate);
                state->read_index = (state->write_index - delay_samples + state->delay_buffer_size) % state->delay_buffer_size;
            }
            break;
        case 1: // feedback
            state->params.feedback = fmaxf(0.0f, fminf(0.99f, value.float_value)); // 클램핑
            break;
        case 2: // sync_to_tempo
            state->params.sync_to_tempo = value.bool_value;
            if (state->params.sync_to_tempo) {
                state->sync_delay_samples = calculate_sync_delay(state->params.tempo_bpm, state->config.sample_rate, 1.0f);
                state->read_index = (state->write_index - state->sync_delay_samples + state->delay_buffer_size) % state->delay_buffer_size;
            }
            break;
        case 3: // tempo_bpm
            state->params.tempo_bpm = value.float_value;
            if (state->params.sync_to_tempo) {
                state->sync_delay_samples = calculate_sync_delay(state->params.tempo_bpm, state->config.sample_rate, 1.0f);
                state->read_index = (state->write_index - state->sync_delay_samples + state->delay_buffer_size) % state->delay_buffer_size;
            }
            break;
        case 4: // wet_dry_mix
            state->config.wet_dry_mix = value.float_value;
            break;
        case 5: // bypass
            state->config.bypass = value.bool_value;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 파라미터 조회
static PluginError delay_get_parameter(PluginContext* ctx, int param_id, PluginParamValue* value) {
    if (!ctx || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DelayState* state = (DelayState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    switch (param_id) {
        case 0: // delay_time
            value->float_value = state->params.delay_time;
            break;
        case 1: // feedback
            value->float_value = state->params.feedback;
            break;
        case 2: // sync_to_tempo
            value->bool_value = state->params.sync_to_tempo;
            break;
        case 3: // tempo_bpm
            value->float_value = state->params.tempo_bpm;
            break;
        case 4: // wet_dry_mix
            value->float_value = state->config.wet_dry_mix;
            break;
        case 5: // bypass
            value->bool_value = state->config.bypass;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 지연 시간 조회
static PluginError delay_get_latency(PluginContext* ctx, int* latency_samples) {
    if (!ctx || !latency_samples) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DelayState* state = (DelayState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    if (state->params.sync_to_tempo) {
        *latency_samples = state->sync_delay_samples;
    } else {
        *latency_samples = delay_time_to_samples(state->params.delay_time, state->config.sample_rate);
    }

    return ET_SUCCESS;
}

// 딜레이 플러그인 파라미터 정의
static PluginParameter delay_parameters[] = {
    {
        .name = "delay_time",
        .display_name = "Delay Time",
        .description = "Delay time in milliseconds",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 1.0f,
            .max_value = 2000.0f,
            .default_value = 250.0f,
            .step = 1.0f
        }
    },
    {
        .name = "feedback",
        .display_name = "Feedback",
        .description = "Feedback amount",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 0.99f,
            .default_value = 0.3f,
            .step = 0.01f
        }
    },
    {
        .name = "sync_to_tempo",
        .display_name = "Sync to Tempo",
        .description = "Synchronize delay to tempo",
        .type = PARAM_TYPE_BOOL,
        .value.bool_param = {
            .default_value = false
        }
    },
    {
        .name = "tempo_bpm",
        .display_name = "Tempo (BPM)",
        .description = "Tempo in beats per minute",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 60.0f,
            .max_value = 200.0f,
            .default_value = 120.0f,
            .step = 1.0f
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
            .default_value = 0.5f,
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

// 딜레이 플러그인 메타데이터
static PluginMetadata delay_metadata = {
    .name = "LibEtude Delay",
    .description = "High-quality digital delay with tempo sync",
    .author = "LibEtude Team",
    .vendor = "LibEtude",
    .version = {1, 0, 0, 0},
    .api_version = {LIBETUDE_PLUGIN_API_VERSION_MAJOR, LIBETUDE_PLUGIN_API_VERSION_MINOR, LIBETUDE_PLUGIN_API_VERSION_PATCH, 0},
    .type = PLUGIN_TYPE_AUDIO_EFFECT,
    .flags = 0,
    .uuid = "550e8400-e29b-41d4-a716-446655440003",
    .checksum = 0
};

// 딜레이 플러그인 생성
PluginInstance* create_delay_plugin(const DelayParams* params) {
    PluginInstance* plugin = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!plugin) {
        return NULL;
    }

    memset(plugin, 0, sizeof(PluginInstance));
    plugin->metadata = delay_metadata;
    plugin->state = PLUGIN_STATE_LOADED;

    // 함수 포인터 설정
    plugin->functions.initialize = delay_initialize;
    plugin->functions.process = delay_process;
    plugin->functions.finalize = delay_finalize;
    plugin->functions.set_parameter = delay_set_parameter;
    plugin->functions.get_parameter = delay_get_parameter;
    plugin->functions.get_latency = delay_get_latency;

    // 파라미터 설정
    plugin->parameters = delay_parameters;
    plugin->num_parameters = sizeof(delay_parameters) / sizeof(PluginParameter);
    plugin->param_values = (PluginParamValue*)calloc(plugin->num_parameters, sizeof(PluginParamValue));

    if (plugin->param_values) {
        // 기본값 설정
        plugin->param_values[0].float_value = 250.0f; // delay_time
        plugin->param_values[1].float_value = 0.3f;   // feedback
        plugin->param_values[2].bool_value = false;   // sync_to_tempo
        plugin->param_values[3].float_value = 120.0f; // tempo_bpm
        plugin->param_values[4].float_value = 0.5f;   // wet_dry_mix
        plugin->param_values[5].bool_value = false;   // bypass
    }

    return plugin;
}