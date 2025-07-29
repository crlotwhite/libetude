#include "libetude/audio_effects.h"
#include "libetude/memory.h"
#include "libetude/fast_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 컴프레서 플러그인 내부 상태
typedef struct {
    AudioEffectConfig config;
    CompressorParams params;

    // 엔벨로프 팔로워 상태
    float envelope;                  // 현재 엔벨로프 레벨
    float attack_coeff;              // 어택 계수
    float release_coeff;             // 릴리즈 계수

    // 게인 리덕션 상태
    float gain_reduction;            // 현재 게인 리덕션 (dB)
    float makeup_gain_linear;        // 선형 메이크업 게인

    // 룩어헤드 딜레이 (더 정확한 피크 감지용)
    float* lookahead_buffer;         // 룩어헤드 버퍼
    int lookahead_size;              // 룩어헤드 크기
    int lookahead_index;             // 룩어헤드 인덱스

    // 사이드체인 필터 (선택적)
    float sidechain_highpass_state;  // 사이드체인 하이패스 필터 상태
    float sidechain_lowpass_state;   // 사이드체인 로우패스 필터 상태

    // 분석 데이터
    AudioAnalysisData analysis;
    bool analysis_enabled;

    // 메모리 풀
    ETMemoryPool* mem_pool;
} CompressorState;

// 기본 컴프레서 파라미터
static const CompressorParams default_compressor_params = {
    .threshold = -12.0f,             // -12dB 임계값
    .ratio = 4.0f,                   // 4:1 압축 비율
    .attack_time = 5.0f,             // 5ms 어택
    .release_time = 100.0f,          // 100ms 릴리즈
    .knee = 0.5f,                    // 소프트 니
    .makeup_gain = 0.0f,             // 메이크업 게인 없음
    .auto_makeup = false             // 자동 메이크업 게인 비활성화
};

// dB를 선형 게인으로 변환
static inline float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

// 선형 게인을 dB로 변환
static inline float linear_to_db(float linear) {
    return 20.0f * log10f(fmaxf(linear, 1e-10f));
}

// 시간 상수를 계수로 변환
static float time_to_coeff(float time_ms, float sample_rate) {
    if (time_ms <= 0.0f) return 1.0f;
    return expf(-1.0f / (time_ms * 0.001f * sample_rate));
}

// 소프트 니 압축 함수
static float soft_knee_compression(float input_db, float threshold_db, float ratio, float knee_width) {
    if (knee_width <= 0.0f) {
        // 하드 니
        if (input_db <= threshold_db) {
            return input_db;
        } else {
            return threshold_db + (input_db - threshold_db) / ratio;
        }
    } else {
        // 소프트 니
        float knee_start = threshold_db - knee_width / 2.0f;
        float knee_end = threshold_db + knee_width / 2.0f;

        if (input_db <= knee_start) {
            return input_db;
        } else if (input_db >= knee_end) {
            return threshold_db + (input_db - threshold_db) / ratio;
        } else {
            // 니 영역에서의 부드러운 전환
            float knee_ratio = (input_db - knee_start) / knee_width;
            float soft_ratio = 1.0f + (1.0f / ratio - 1.0f) * knee_ratio * knee_ratio;
            return input_db + (threshold_db - input_db) * (1.0f - 1.0f / soft_ratio);
        }
    }
}

// 자동 메이크업 게인 계산
static float calculate_auto_makeup_gain(float threshold_db, float ratio) {
    // 임계값에서의 게인 리덕션을 보상
    float reduction_at_threshold = threshold_db * (1.0f - 1.0f / ratio);
    return -reduction_at_threshold * 0.5f; // 50% 보상
}

// 컴프레서 플러그인 초기화
static PluginError compressor_initialize(PluginContext* ctx, const void* config) {
    if (!ctx || !config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const AudioEffectConfig* effect_config = (const AudioEffectConfig*)config;

    // 상태 구조체 할당
    CompressorState* state = (CompressorState*)malloc(sizeof(CompressorState));
    if (!state) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(state, 0, sizeof(CompressorState));
    state->config = *effect_config;
    state->params = default_compressor_params;

    // 메모리 풀 생성
    state->mem_pool = et_create_memory_pool(256 * 1024, 16); // 256KB 풀
    if (!state->mem_pool) {
        free(state);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    float sample_rate = effect_config->sample_rate;

    // 어택/릴리즈 계수 계산
    state->attack_coeff = time_to_coeff(state->params.attack_time, sample_rate);
    state->release_coeff = time_to_coeff(state->params.release_time, sample_rate);

    // 메이크업 게인 설정
    if (state->params.auto_makeup) {
        state->makeup_gain_linear = db_to_linear(calculate_auto_makeup_gain(state->params.threshold, state->params.ratio));
    } else {
        state->makeup_gain_linear = db_to_linear(state->params.makeup_gain);
    }

    // 룩어헤드 버퍼 설정 (1ms 룩어헤드)
    state->lookahead_size = (int)(0.001f * sample_rate);
    if (state->lookahead_size > 0) {
        state->lookahead_buffer = (float*)et_alloc_from_pool(state->mem_pool,
                                                           state->lookahead_size * sizeof(float));
        if (!state->lookahead_buffer) {
            et_destroy_memory_pool(state->mem_pool);
            free(state);
            return ET_ERROR_OUT_OF_MEMORY;
        }
        memset(state->lookahead_buffer, 0, state->lookahead_size * sizeof(float));
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
    ctx->state_size = sizeof(CompressorState);

    return ET_SUCCESS;
}

// 컴프레서 플러그인 처리
static PluginError compressor_process(PluginContext* ctx, const float* input, float* output, int num_samples) {
    if (!ctx || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    CompressorState* state = (CompressorState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    bool bypass = state->config.bypass;

    for (int i = 0; i < num_samples; i++) {
        float input_sample = input[i];
        float output_sample = input_sample;

        if (!bypass) {
            // 룩어헤드 처리
            float current_sample = input_sample;
            if (state->lookahead_size > 0) {
                float delayed_sample = state->lookahead_buffer[state->lookahead_index];
                state->lookahead_buffer[state->lookahead_index] = input_sample;
                state->lookahead_index = (state->lookahead_index + 1) % state->lookahead_size;
                current_sample = delayed_sample;
            }

            // 입력 레벨 계산 (dB)
            float input_level = fabsf(current_sample);
            float input_db = linear_to_db(input_level);

            // 사이드체인 필터링 (선택적)
            // 여기서는 간단히 생략

            // 압축 계산
            float compressed_db = soft_knee_compression(input_db, state->params.threshold,
                                                      state->params.ratio, state->params.knee * 10.0f);

            // 게인 리덕션 계산
            float target_gain_reduction = compressed_db - input_db;

            // 엔벨로프 팔로워
            if (target_gain_reduction < state->gain_reduction) {
                // 어택 (게인 리덕션 증가)
                state->gain_reduction = target_gain_reduction + (state->gain_reduction - target_gain_reduction) * state->attack_coeff;
            } else {
                // 릴리즈 (게인 리덕션 감소)
                state->gain_reduction = target_gain_reduction + (state->gain_reduction - target_gain_reduction) * state->release_coeff;
            }

            // 게인 적용
            float compression_gain = db_to_linear(state->gain_reduction);
            output_sample = current_sample * compression_gain;

            // 메이크업 게인 적용
            output_sample *= state->makeup_gain_linear;
        }

        output[i] = output_sample;

        // 분석 데이터 업데이트
        if (state->analysis_enabled) {
            float abs_sample = fabsf(output_sample);
            if (abs_sample > state->analysis.peak_level) {
                state->analysis.peak_level = abs_sample;
            }
            state->analysis.rms_level = state->analysis.rms_level * 0.999f + abs_sample * abs_sample * 0.001f;
            state->analysis.gain_reduction = -state->gain_reduction; // 양수로 표시
        }
    }

    return ET_SUCCESS;
}

// 컴프레서 플러그인 종료
static PluginError compressor_finalize(PluginContext* ctx) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    CompressorState* state = (CompressorState*)ctx->internal_state;
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
static PluginError compressor_set_parameter(PluginContext* ctx, int param_id, PluginParamValue value) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    CompressorState* state = (CompressorState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    switch (param_id) {
        case 0: // threshold
            state->params.threshold = value.float_value;
            if (state->params.auto_makeup) {
                state->makeup_gain_linear = db_to_linear(calculate_auto_makeup_gain(state->params.threshold, state->params.ratio));
            }
            break;
        case 1: // ratio
            state->params.ratio = fmaxf(1.0f, value.float_value); // 최소 1:1
            if (state->params.auto_makeup) {
                state->makeup_gain_linear = db_to_linear(calculate_auto_makeup_gain(state->params.threshold, state->params.ratio));
            }
            break;
        case 2: // attack_time
            state->params.attack_time = value.float_value;
            state->attack_coeff = time_to_coeff(state->params.attack_time, state->config.sample_rate);
            break;
        case 3: // release_time
            state->params.release_time = value.float_value;
            state->release_coeff = time_to_coeff(state->params.release_time, state->config.sample_rate);
            break;
        case 4: // knee
            state->params.knee = value.float_value;
            break;
        case 5: // makeup_gain
            state->params.makeup_gain = value.float_value;
            if (!state->params.auto_makeup) {
                state->makeup_gain_linear = db_to_linear(state->params.makeup_gain);
            }
            break;
        case 6: // auto_makeup
            state->params.auto_makeup = value.bool_value;
            if (state->params.auto_makeup) {
                state->makeup_gain_linear = db_to_linear(calculate_auto_makeup_gain(state->params.threshold, state->params.ratio));
            } else {
                state->makeup_gain_linear = db_to_linear(state->params.makeup_gain);
            }
            break;
        case 7: // bypass
            state->config.bypass = value.bool_value;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 파라미터 조회
static PluginError compressor_get_parameter(PluginContext* ctx, int param_id, PluginParamValue* value) {
    if (!ctx || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    CompressorState* state = (CompressorState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    switch (param_id) {
        case 0: // threshold
            value->float_value = state->params.threshold;
            break;
        case 1: // ratio
            value->float_value = state->params.ratio;
            break;
        case 2: // attack_time
            value->float_value = state->params.attack_time;
            break;
        case 3: // release_time
            value->float_value = state->params.release_time;
            break;
        case 4: // knee
            value->float_value = state->params.knee;
            break;
        case 5: // makeup_gain
            value->float_value = state->params.makeup_gain;
            break;
        case 6: // auto_makeup
            value->bool_value = state->params.auto_makeup;
            break;
        case 7: // bypass
            value->bool_value = state->config.bypass;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 지연 시간 조회
static PluginError compressor_get_latency(PluginContext* ctx, int* latency_samples) {
    if (!ctx || !latency_samples) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    CompressorState* state = (CompressorState*)ctx->internal_state;
    if (!state) {
        return ET_ERROR_RUNTIME;
    }

    *latency_samples = state->lookahead_size;
    return ET_SUCCESS;
}

// 컴프레서 플러그인 파라미터 정의
static PluginParameter compressor_parameters[] = {
    {
        .name = "threshold",
        .display_name = "Threshold",
        .description = "Compression threshold in dB",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = -60.0f,
            .max_value = 0.0f,
            .default_value = -12.0f,
            .step = 0.1f
        }
    },
    {
        .name = "ratio",
        .display_name = "Ratio",
        .description = "Compression ratio",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 1.0f,
            .max_value = 20.0f,
            .default_value = 4.0f,
            .step = 0.1f
        }
    },
    {
        .name = "attack_time",
        .display_name = "Attack Time",
        .description = "Attack time in milliseconds",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.1f,
            .max_value = 100.0f,
            .default_value = 5.0f,
            .step = 0.1f
        }
    },
    {
        .name = "release_time",
        .display_name = "Release Time",
        .description = "Release time in milliseconds",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 10.0f,
            .max_value = 1000.0f,
            .default_value = 100.0f,
            .step = 1.0f
        }
    },
    {
        .name = "knee",
        .display_name = "Knee",
        .description = "Knee softness",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 1.0f,
            .default_value = 0.5f,
            .step = 0.01f
        }
    },
    {
        .name = "makeup_gain",
        .display_name = "Makeup Gain",
        .description = "Makeup gain in dB",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 30.0f,
            .default_value = 0.0f,
            .step = 0.1f
        }
    },
    {
        .name = "auto_makeup",
        .display_name = "Auto Makeup",
        .description = "Automatic makeup gain compensation",
        .type = PARAM_TYPE_BOOL,
        .value.bool_param = {
            .default_value = false
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

// 컴프레서 플러그인 메타데이터
static PluginMetadata compressor_metadata = {
    .name = "LibEtude Compressor",
    .description = "High-quality dynamics compressor with auto makeup gain",
    .author = "LibEtude Team",
    .vendor = "LibEtude",
    .version = {1, 0, 0, 0},
    .api_version = {LIBETUDE_PLUGIN_API_VERSION_MAJOR, LIBETUDE_PLUGIN_API_VERSION_MINOR, LIBETUDE_PLUGIN_API_VERSION_PATCH, 0},
    .type = PLUGIN_TYPE_AUDIO_EFFECT,
    .flags = 0,
    .uuid = "550e8400-e29b-41d4-a716-446655440004",
    .checksum = 0
};

// 컴프레서 플러그인 생성
PluginInstance* create_compressor_plugin(const CompressorParams* params) {
    PluginInstance* plugin = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!plugin) {
        return NULL;
    }

    memset(plugin, 0, sizeof(PluginInstance));
    plugin->metadata = compressor_metadata;
    plugin->state = PLUGIN_STATE_LOADED;

    // 함수 포인터 설정
    plugin->functions.initialize = compressor_initialize;
    plugin->functions.process = compressor_process;
    plugin->functions.finalize = compressor_finalize;
    plugin->functions.set_parameter = compressor_set_parameter;
    plugin->functions.get_parameter = compressor_get_parameter;
    plugin->functions.get_latency = compressor_get_latency;

    // 파라미터 설정
    plugin->parameters = compressor_parameters;
    plugin->num_parameters = sizeof(compressor_parameters) / sizeof(PluginParameter);
    plugin->param_values = (PluginParamValue*)calloc(plugin->num_parameters, sizeof(PluginParamValue));

    if (plugin->param_values) {
        // 기본값 설정
        plugin->param_values[0].float_value = -12.0f; // threshold
        plugin->param_values[1].float_value = 4.0f;   // ratio
        plugin->param_values[2].float_value = 5.0f;   // attack_time
        plugin->param_values[3].float_value = 100.0f; // release_time
        plugin->param_values[4].float_value = 0.5f;   // knee
        plugin->param_values[5].float_value = 0.0f;   // makeup_gain
        plugin->param_values[6].bool_value = false;   // auto_makeup
        plugin->param_values[7].bool_value = false;   // bypass
    }

    return plugin;
}