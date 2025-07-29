#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libetude/plugin.h"
#include "libetude/error.h"

// 간단한 리버브 효과 플러그인 예제
typedef struct {
    float* delay_buffer;             // 딜레이 버퍼
    int buffer_size;                 // 버퍼 크기
    int write_pos;                   // 쓰기 위치
    float decay;                     // 감쇠율
    float mix;                       // 믹스 레벨
    bool enabled;                    // 활성화 여부
} ReverbContext;

// 리버브 플러그인 메타데이터
static const PluginMetadata reverb_metadata = {
    .name = "SimpleReverb",
    .description = "Simple reverb effect plugin",
    .author = "LibEtude Team",
    .vendor = "LibEtude",
    .version = {1, 0, 0, 1},
    .api_version = {1, 0, 0, 0},
    .type = PLUGIN_TYPE_AUDIO_EFFECT,
    .flags = 0,
    .uuid = "reverb01-1234-5678-9abc-123456789abc",
    .checksum = 0x12345678
};

// 리버브 플러그인 파라미터
static const PluginParameter reverb_parameters[] = {
    {
        .name = "decay",
        .display_name = "Decay",
        .description = "Reverb decay time",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.1f,
            .max_value = 0.9f,
            .default_value = 0.5f,
            .step = 0.01f
        }
    },
    {
        .name = "mix",
        .display_name = "Mix",
        .description = "Dry/wet mix",
        .type = PARAM_TYPE_FLOAT,
        .value.float_param = {
            .min_value = 0.0f,
            .max_value = 1.0f,
            .default_value = 0.3f,
            .step = 0.01f
        }
    },
    {
        .name = "enabled",
        .display_name = "Enabled",
        .description = "Enable/disable reverb",
        .type = PARAM_TYPE_BOOL,
        .value.bool_param = {
            .default_value = true
        }
    }
};

// 리버브 플러그인 초기화
static PluginError reverb_initialize(PluginContext* ctx, const void* config) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    ReverbContext* reverb = (ReverbContext*)malloc(sizeof(ReverbContext));
    if (!reverb) return ET_ERROR_OUT_OF_MEMORY;

    // 딜레이 버퍼 크기 (약 100ms at 44.1kHz)
    reverb->buffer_size = 4410;
    reverb->delay_buffer = (float*)calloc(reverb->buffer_size, sizeof(float));
    if (!reverb->delay_buffer) {
        free(reverb);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    reverb->write_pos = 0;
    reverb->decay = 0.5f;
    reverb->mix = 0.3f;
    reverb->enabled = true;

    ctx->user_data = reverb;
    return ET_SUCCESS;
}

// 리버브 플러그인 처리
static PluginError reverb_process(PluginContext* ctx, const float* input, float* output, int num_samples) {
    if (!ctx || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbContext* reverb = (ReverbContext*)ctx->user_data;
    if (!reverb) return ET_ERROR_RUNTIME;

    if (!reverb->enabled) {
        // 비활성화된 경우 입력을 그대로 출력에 복사
        memcpy(output, input, num_samples * sizeof(float));
        return ET_SUCCESS;
    }

    for (int i = 0; i < num_samples; i++) {
        // 현재 딜레이 버퍼에서 읽기
        float delayed = reverb->delay_buffer[reverb->write_pos];

        // 새로운 값을 딜레이 버퍼에 쓰기 (입력 + 피드백)
        reverb->delay_buffer[reverb->write_pos] = input[i] + delayed * reverb->decay;

        // 출력 계산 (드라이 + 웨트 믹스)
        output[i] = input[i] * (1.0f - reverb->mix) + delayed * reverb->mix;

        // 쓰기 위치 업데이트
        reverb->write_pos = (reverb->write_pos + 1) % reverb->buffer_size;
    }

    return ET_SUCCESS;
}

// 리버브 플러그인 종료
static PluginError reverb_finalize(PluginContext* ctx) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    ReverbContext* reverb = (ReverbContext*)ctx->user_data;
    if (reverb) {
        free(reverb->delay_buffer);
        free(reverb);
        ctx->user_data = NULL;
    }

    return ET_SUCCESS;
}

// 리버브 플러그인 파라미터 설정
static PluginError reverb_set_parameter(PluginContext* ctx, int param_id, PluginParamValue value) {
    if (!ctx || param_id < 0 || param_id >= 3) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbContext* reverb = (ReverbContext*)ctx->user_data;
    if (!reverb) return ET_ERROR_RUNTIME;

    switch (param_id) {
        case 0: // decay
            reverb->decay = value.float_value;
            break;
        case 1: // mix
            reverb->mix = value.float_value;
            break;
        case 2: // enabled
            reverb->enabled = value.bool_value;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 리버브 플러그인 파라미터 조회
static PluginError reverb_get_parameter(PluginContext* ctx, int param_id, PluginParamValue* value) {
    if (!ctx || !value || param_id < 0 || param_id >= 3) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ReverbContext* reverb = (ReverbContext*)ctx->user_data;
    if (!reverb) return ET_ERROR_RUNTIME;

    switch (param_id) {
        case 0: // decay
            value->float_value = reverb->decay;
            break;
        case 1: // mix
            value->float_value = reverb->mix;
            break;
        case 2: // enabled
            value->bool_value = reverb->enabled;
            break;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

// 테스트용 리버브 플러그인 인스턴스 생성
static PluginInstance* create_reverb_plugin(void) {
    PluginInstance* plugin = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!plugin) return NULL;

    memset(plugin, 0, sizeof(PluginInstance));

    plugin->metadata = reverb_metadata;
    plugin->state = PLUGIN_STATE_LOADED;
    plugin->handle = NULL;

    // 함수 포인터 설정
    plugin->functions.initialize = reverb_initialize;
    plugin->functions.process = reverb_process;
    plugin->functions.finalize = reverb_finalize;
    plugin->functions.set_parameter = reverb_set_parameter;
    plugin->functions.get_parameter = reverb_get_parameter;

    // 파라미터 설정
    plugin->parameters = (PluginParameter*)reverb_parameters;
    plugin->num_parameters = 3;
    plugin->param_values = (PluginParamValue*)calloc(3, sizeof(PluginParamValue));
    if (plugin->param_values) {
        plugin->param_values[0].float_value = 0.5f; // decay
        plugin->param_values[1].float_value = 0.3f; // mix
        plugin->param_values[2].bool_value = true;   // enabled
    }

    return plugin;
}

// 간단한 게인 플러그인 예제
typedef struct {
    float gain;
    bool enabled;
} GainContext;

static const PluginMetadata gain_metadata = {
    .name = "SimpleGain",
    .description = "Simple gain control plugin",
    .author = "LibEtude Team",
    .vendor = "LibEtude",
    .version = {1, 0, 0, 1},
    .api_version = {1, 0, 0, 0},
    .type = PLUGIN_TYPE_AUDIO_EFFECT,
    .flags = 0,
    .uuid = "gain0001-1234-5678-9abc-123456789abc",
    .checksum = 0x87654321
};

static PluginError gain_initialize(PluginContext* ctx, const void* config) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    GainContext* gain = (GainContext*)malloc(sizeof(GainContext));
    if (!gain) return ET_ERROR_OUT_OF_MEMORY;

    gain->gain = 1.0f;
    gain->enabled = true;

    ctx->user_data = gain;
    return ET_SUCCESS;
}

static PluginError gain_process(PluginContext* ctx, const float* input, float* output, int num_samples) {
    if (!ctx || !input || !output || num_samples <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    GainContext* gain = (GainContext*)ctx->user_data;
    if (!gain) return ET_ERROR_RUNTIME;

    if (!gain->enabled) {
        memcpy(output, input, num_samples * sizeof(float));
        return ET_SUCCESS;
    }

    for (int i = 0; i < num_samples; i++) {
        output[i] = input[i] * gain->gain;
    }

    return ET_SUCCESS;
}

static PluginError gain_finalize(PluginContext* ctx) {
    if (!ctx) return ET_ERROR_INVALID_ARGUMENT;

    GainContext* gain = (GainContext*)ctx->user_data;
    if (gain) {
        free(gain);
        ctx->user_data = NULL;
    }

    return ET_SUCCESS;
}

static PluginInstance* create_gain_plugin(void) {
    PluginInstance* plugin = (PluginInstance*)malloc(sizeof(PluginInstance));
    if (!plugin) return NULL;

    memset(plugin, 0, sizeof(PluginInstance));

    plugin->metadata = gain_metadata;
    plugin->state = PLUGIN_STATE_LOADED;
    plugin->handle = NULL;

    plugin->functions.initialize = gain_initialize;
    plugin->functions.process = gain_process;
    plugin->functions.finalize = gain_finalize;

    return plugin;
}

// 플러그인 체인 데모
void demo_plugin_chain(void) {
    printf("=== Plugin Chain Demo ===\n");

    // 플러그인 체인 생성
    PluginChain* chain = plugin_create_chain();
    if (!chain) {
        printf("Failed to create plugin chain\n");
        return;
    }

    // 게인 플러그인과 리버브 플러그인 생성
    PluginInstance* gain_plugin = create_gain_plugin();
    PluginInstance* reverb_plugin = create_reverb_plugin();

    if (!gain_plugin || !reverb_plugin) {
        printf("Failed to create plugins\n");
        plugin_destroy_chain(chain);
        return;
    }

    // 플러그인 초기화 및 활성화
    if (plugin_initialize(gain_plugin, NULL) != ET_SUCCESS ||
        plugin_activate(gain_plugin) != ET_SUCCESS ||
        plugin_initialize(reverb_plugin, NULL) != ET_SUCCESS ||
        plugin_activate(reverb_plugin) != ET_SUCCESS) {
        printf("Failed to initialize plugins\n");
        goto cleanup;
    }

    // 체인에 플러그인 추가 (게인 -> 리버브 순서)
    if (plugin_chain_add(chain, gain_plugin) != ET_SUCCESS ||
        plugin_chain_add(chain, reverb_plugin) != ET_SUCCESS) {
        printf("Failed to add plugins to chain\n");
        goto cleanup;
    }

    // 게인 플러그인의 게인을 2.0으로 설정
    GainContext* gain_ctx = (GainContext*)gain_plugin->context->user_data;
    if (gain_ctx) {
        gain_ctx->gain = 2.0f;
    }

    // 테스트 신호 생성 (사인파)
    const int num_samples = 1024;
    float input[num_samples];
    float output[num_samples];

    for (int i = 0; i < num_samples; i++) {
        input[i] = 0.5f * sinf(2.0f * M_PI * 440.0f * i / 44100.0f); // 440Hz 사인파
    }

    // 체인 처리
    PluginError error = plugin_chain_process(chain, input, output, num_samples);
    if (error == ET_SUCCESS) {
        printf("✓ Plugin chain processing successful\n");

        // 출력 레벨 확인
        float max_output = 0.0f;
        for (int i = 0; i < num_samples; i++) {
            if (fabsf(output[i]) > max_output) {
                max_output = fabsf(output[i]);
            }
        }
        printf("  Input peak: %.3f, Output peak: %.3f\n", 0.5f, max_output);
    } else {
        printf("✗ Plugin chain processing failed\n");
    }

    // 리버브 플러그인 바이패스 테스트
    printf("\nTesting plugin bypass...\n");
    error = plugin_chain_set_bypass(chain, reverb_plugin, true);
    if (error == ET_SUCCESS) {
        error = plugin_chain_process(chain, input, output, num_samples);
        if (error == ET_SUCCESS) {
            printf("✓ Plugin bypass successful\n");

            // 바이패스된 경우 게인만 적용되어야 함
            float expected_peak = 0.5f * 2.0f; // 원본 * 게인
            float actual_peak = 0.0f;
            for (int i = 0; i < num_samples; i++) {
                if (fabsf(output[i]) > actual_peak) {
                    actual_peak = fabsf(output[i]);
                }
            }
            printf("  Expected peak: %.3f, Actual peak: %.3f\n", expected_peak, actual_peak);
        }
    }

cleanup:
    // 정리
    if (gain_plugin) {
        if (gain_plugin->state == PLUGIN_STATE_ACTIVE) {
            plugin_deactivate(gain_plugin);
        }
        if (gain_plugin->state == PLUGIN_STATE_INITIALIZED) {
            plugin_finalize(gain_plugin);
        }
        free(gain_plugin);
    }

    if (reverb_plugin) {
        if (reverb_plugin->state == PLUGIN_STATE_ACTIVE) {
            plugin_deactivate(reverb_plugin);
        }
        if (reverb_plugin->state == PLUGIN_STATE_INITIALIZED) {
            plugin_finalize(reverb_plugin);
        }
        free(reverb_plugin->param_values);
        free(reverb_plugin);
    }

    plugin_destroy_chain(chain);
    printf("Plugin chain demo completed.\n\n");
}

// 플러그인 레지스트리 데모
void demo_plugin_registry(void) {
    printf("=== Plugin Registry Demo ===\n");

    // 레지스트리 생성
    PluginRegistry* registry = plugin_create_registry();
    if (!registry) {
        printf("Failed to create plugin registry\n");
        return;
    }

    // 검색 경로 추가
    plugin_add_search_path(registry, "/usr/lib/libetude/plugins");
    plugin_add_search_path(registry, "/usr/local/lib/libetude/plugins");
    plugin_add_search_path(registry, "./plugins");

    printf("✓ Added search paths to registry\n");

    // 테스트용 플러그인들 생성 및 등록
    PluginInstance* gain_plugin = create_gain_plugin();
    PluginInstance* reverb_plugin = create_reverb_plugin();

    if (gain_plugin && reverb_plugin) {
        plugin_register(registry, gain_plugin);
        plugin_register(registry, reverb_plugin);
        printf("✓ Registered test plugins\n");

        // 이름으로 플러그인 찾기
        PluginInstance* found_gain = plugin_find_by_name(registry, "SimpleGain");
        PluginInstance* found_reverb = plugin_find_by_name(registry, "SimpleReverb");

        if (found_gain && found_reverb) {
            printf("✓ Found plugins by name\n");
            printf("  Gain plugin: %s v%d.%d.%d\n",
                   found_gain->metadata.name,
                   found_gain->metadata.version.major,
                   found_gain->metadata.version.minor,
                   found_gain->metadata.version.patch);
            printf("  Reverb plugin: %s v%d.%d.%d\n",
                   found_reverb->metadata.name,
                   found_reverb->metadata.version.major,
                   found_reverb->metadata.version.minor,
                   found_reverb->metadata.version.patch);
        }

        // UUID로 플러그인 찾기
        PluginInstance* found_by_uuid = plugin_find_by_uuid(registry,
                                                           "gain0001-1234-5678-9abc-123456789abc");
        if (found_by_uuid) {
            printf("✓ Found plugin by UUID: %s\n", found_by_uuid->metadata.name);
        }
    }

    // 정리
    plugin_destroy_registry(registry);
    printf("Plugin registry demo completed.\n\n");
}

// 플러그인 파라미터 데모
void demo_plugin_parameters(void) {
    printf("=== Plugin Parameters Demo ===\n");

    PluginInstance* reverb_plugin = create_reverb_plugin();
    if (!reverb_plugin) {
        printf("Failed to create reverb plugin\n");
        return;
    }

    // 플러그인 초기화
    if (plugin_initialize(reverb_plugin, NULL) != ET_SUCCESS) {
        printf("Failed to initialize reverb plugin\n");
        free(reverb_plugin);
        return;
    }

    // 파라미터 정보 출력
    printf("Plugin: %s\n", reverb_plugin->metadata.name);
    printf("Parameters:\n");
    for (int i = 0; i < reverb_plugin->num_parameters; i++) {
        const PluginParameter* param = &reverb_plugin->parameters[i];
        printf("  %d. %s (%s): %s\n", i, param->display_name, param->name, param->description);

        if (param->type == PARAM_TYPE_FLOAT) {
            printf("     Range: %.2f - %.2f, Default: %.2f\n",
                   param->value.float_param.min_value,
                   param->value.float_param.max_value,
                   param->value.float_param.default_value);
        } else if (param->type == PARAM_TYPE_BOOL) {
            printf("     Default: %s\n", param->value.bool_param.default_value ? "true" : "false");
        }
    }

    // 파라미터 값 변경 테스트
    printf("\nTesting parameter changes...\n");

    PluginParamValue decay_value;
    decay_value.float_value = 0.8f;
    if (reverb_plugin->functions.set_parameter(reverb_plugin->context, 0, decay_value) == ET_SUCCESS) {
        printf("✓ Set decay parameter to %.2f\n", decay_value.float_value);
    }

    PluginParamValue mix_value;
    mix_value.float_value = 0.6f;
    if (reverb_plugin->functions.set_parameter(reverb_plugin->context, 1, mix_value) == ET_SUCCESS) {
        printf("✓ Set mix parameter to %.2f\n", mix_value.float_value);
    }

    // 파라미터 값 조회 테스트
    PluginParamValue current_decay;
    if (reverb_plugin->functions.get_parameter(reverb_plugin->context, 0, &current_decay) == ET_SUCCESS) {
        printf("✓ Current decay value: %.2f\n", current_decay.float_value);
    }

    // 정리
    plugin_finalize(reverb_plugin);
    free(reverb_plugin->param_values);
    free(reverb_plugin);

    printf("Plugin parameters demo completed.\n\n");
}

int main(void) {
    printf("LibEtude Plugin System Examples\n");
    printf("===============================\n\n");

    demo_plugin_registry();
    demo_plugin_parameters();
    demo_plugin_chain();

    printf("All plugin demos completed successfully!\n");
    return 0;
}