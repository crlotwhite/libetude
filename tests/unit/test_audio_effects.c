#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "libetude/audio_effects.h"
#include "libetude/plugin.h"
#include "libetude/error.h"

// 테스트 프레임워크 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return 0; \
        } \
    } while(0)

#define TEST_SUCCESS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return 1; \
    } while(0)

// 테스트용 오디오 신호 생성
void generate_sine_wave(float* buffer, int num_samples, float frequency, float sample_rate) {
    for (int i = 0; i < num_samples; i++) {
        buffer[i] = 0.5f * sinf(2.0f * M_PI * frequency * i / sample_rate);
    }
}

// RMS 계산
float calculate_rms(const float* buffer, int num_samples) {
    float sum = 0.0f;
    for (int i = 0; i < num_samples; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrtf(sum / num_samples);
}

// 리버브 플러그인 생성 테스트
int test_reverb_plugin_creation(void) {
    PluginInstance* reverb = create_reverb_plugin(NULL);
    TEST_ASSERT(reverb != NULL, "Failed to create reverb plugin");
    TEST_ASSERT(reverb->metadata.type == PLUGIN_TYPE_AUDIO_EFFECT, "Wrong plugin type");
    TEST_ASSERT(strcmp(reverb->metadata.name, "LibEtude Reverb") == 0, "Wrong plugin name");
    TEST_ASSERT(reverb->num_parameters == 4, "Wrong number of parameters");

    // 메모리 해제
    if (reverb->param_values) {
        free(reverb->param_values);
    }
    free(reverb);

    TEST_SUCCESS();
}

// 리버브 플러그인 초기화 테스트
int test_reverb_plugin_initialization(void) {
    PluginInstance* reverb = create_reverb_plugin(NULL);
    TEST_ASSERT(reverb != NULL, "Failed to create reverb plugin");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 0.5f
    };

    ETErrorCode result = plugin_initialize(reverb, &config);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to initialize reverb plugin");
    TEST_ASSERT(reverb->state == PLUGIN_STATE_INITIALIZED, "Wrong plugin state");

    // 정리
    plugin_finalize(reverb);
    free(reverb->param_values);
    free(reverb);

    TEST_SUCCESS();
}

// 리버브 플러그인 처리 테스트
int test_reverb_plugin_processing(void) {
    PluginInstance* reverb = create_reverb_plugin(NULL);
    TEST_ASSERT(reverb != NULL, "Failed to create reverb plugin");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 0.3f
    };

    ETErrorCode result = plugin_initialize(reverb, &config);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to initialize reverb plugin");

    result = plugin_activate(reverb);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to activate reverb plugin");

    // 테스트 신호 생성
    const int buffer_size = 1024;
    float* input = (float*)malloc(buffer_size * sizeof(float));
    float* output = (float*)malloc(buffer_size * sizeof(float));

    generate_sine_wave(input, buffer_size, 440.0f, 44100.0f);
    float input_rms = calculate_rms(input, buffer_size);

    // 리버브 처리
    result = plugin_process(reverb, input, output, buffer_size);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to process reverb");

    float output_rms = calculate_rms(output, buffer_size);
    TEST_ASSERT(output_rms > 0.0f, "Output is silent");
    TEST_ASSERT(fabsf(output_rms - input_rms) < input_rms, "Output level too different from input");

    // 정리
    free(input);
    free(output);
    plugin_deactivate(reverb);
    plugin_finalize(reverb);
    free(reverb->param_values);
    free(reverb);

    TEST_SUCCESS();
}

// 리버브 파라미터 설정 테스트
int test_reverb_parameter_setting(void) {
    PluginInstance* reverb = create_reverb_plugin(NULL);
    TEST_ASSERT(reverb != NULL, "Failed to create reverb plugin");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 0.5f
    };

    plugin_initialize(reverb, &config);
    plugin_activate(reverb);

    // room_size 파라미터 설정
    PluginParamValue value;
    value.float_value = 0.8f;
    ETErrorCode result = plugin_set_parameter_by_id(reverb, 0, value);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to set room_size parameter");

    // 파라미터 값 확인
    PluginParamValue retrieved_value;
    result = plugin_get_parameter_by_id(reverb, 0, &retrieved_value);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to get room_size parameter");
    TEST_ASSERT(fabsf(retrieved_value.float_value - 0.8f) < 0.001f, "Parameter value mismatch");

    // 정리
    plugin_deactivate(reverb);
    plugin_finalize(reverb);
    free(reverb->param_values);
    free(reverb);

    TEST_SUCCESS();
}

// 이퀄라이저 플러그인 생성 테스트
int test_equalizer_plugin_creation(void) {
    PluginInstance* eq = create_equalizer_plugin(NULL);
    TEST_ASSERT(eq != NULL, "Failed to create equalizer plugin");
    TEST_ASSERT(eq->metadata.type == PLUGIN_TYPE_AUDIO_EFFECT, "Wrong plugin type");
    TEST_ASSERT(strcmp(eq->metadata.name, "LibEtude Equalizer") == 0, "Wrong plugin name");
    TEST_ASSERT(eq->num_parameters == 33, "Wrong number of parameters"); // 10밴드*3 + 3전역

    // 메모리 해제
    if (eq->param_values) {
        free(eq->param_values);
    }
    free(eq);

    TEST_SUCCESS();
}

// 이퀄라이저 플러그인 처리 테스트
int test_equalizer_plugin_processing(void) {
    PluginInstance* eq = create_equalizer_plugin(NULL);
    TEST_ASSERT(eq != NULL, "Failed to create equalizer plugin");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    ETErrorCode result = plugin_initialize(eq, &config);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to initialize equalizer plugin");

    result = plugin_activate(eq);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to activate equalizer plugin");

    // 테스트 신호 생성
    const int buffer_size = 1024;
    float* input = (float*)malloc(buffer_size * sizeof(float));
    float* output = (float*)malloc(buffer_size * sizeof(float));

    generate_sine_wave(input, buffer_size, 1000.0f, 44100.0f);

    // 이퀄라이저 처리
    result = plugin_process(eq, input, output, buffer_size);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to process equalizer");

    float output_rms = calculate_rms(output, buffer_size);
    TEST_ASSERT(output_rms > 0.0f, "Output is silent");

    // 정리
    free(input);
    free(output);
    plugin_deactivate(eq);
    plugin_finalize(eq);
    free(eq->param_values);
    free(eq);

    TEST_SUCCESS();
}

// 오디오 효과 파이프라인 테스트
int test_audio_effect_pipeline(void) {
    // 파이프라인 생성
    AudioEffectPipeline* pipeline = create_audio_effect_pipeline(5);
    TEST_ASSERT(pipeline != NULL, "Failed to create audio effect pipeline");

    // 플러그인들 생성
    PluginInstance* reverb = create_reverb_plugin(NULL);
    PluginInstance* eq = create_equalizer_plugin(NULL);
    TEST_ASSERT(reverb != NULL && eq != NULL, "Failed to create plugins");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    // 플러그인 초기화
    plugin_initialize(reverb, &config);
    plugin_activate(reverb);
    plugin_initialize(eq, &config);
    plugin_activate(eq);

    // 파이프라인에 추가
    ETErrorCode result = add_effect_to_pipeline(pipeline, reverb);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add reverb to pipeline");

    result = add_effect_to_pipeline(pipeline, eq);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add equalizer to pipeline");

    // 테스트 신호 생성
    const int buffer_size = 1024;
    float* input = (float*)malloc(buffer_size * sizeof(float));
    float* output = (float*)malloc(buffer_size * sizeof(float));

    generate_sine_wave(input, buffer_size, 440.0f, 44100.0f);

    // 파이프라인 처리
    result = process_audio_pipeline(pipeline, input, output, buffer_size);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to process audio pipeline");

    float output_rms = calculate_rms(output, buffer_size);
    TEST_ASSERT(output_rms > 0.0f, "Pipeline output is silent");

    // 바이패스 테스트
    set_pipeline_bypass(pipeline, true);
    result = process_audio_pipeline(pipeline, input, output, buffer_size);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to process bypassed pipeline");

    float bypassed_rms = calculate_rms(output, buffer_size);
    float input_rms = calculate_rms(input, buffer_size);
    TEST_ASSERT(fabsf(bypassed_rms - input_rms) < 0.001f, "Bypass not working correctly");

    // 정리
    free(input);
    free(output);
    destroy_audio_effect_pipeline(pipeline);

    plugin_deactivate(reverb);
    plugin_finalize(reverb);
    free(reverb->param_values);
    free(reverb);

    plugin_deactivate(eq);
    plugin_finalize(eq);
    free(eq->param_values);
    free(eq);

    TEST_SUCCESS();
}

// 프리셋 관리 테스트
int test_preset_management(void) {
    PluginInstance* reverb = create_reverb_plugin(NULL);
    TEST_ASSERT(reverb != NULL, "Failed to create reverb plugin");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    plugin_initialize(reverb, &config);

    // 파라미터 설정
    PluginParamValue value;
    value.float_value = 0.7f;
    plugin_set_parameter_by_id(reverb, 0, value); // room_size

    value.float_value = 0.3f;
    plugin_set_parameter_by_id(reverb, 1, value); // damping

    // 프리셋 저장
    AudioEffectPreset preset;
    ETErrorCode result = save_effect_preset(reverb, "Test Preset", &preset);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to save preset");
    TEST_ASSERT(strcmp(preset.name, "Test Preset") == 0, "Wrong preset name");
    TEST_ASSERT(preset.params != NULL, "Preset parameters not saved");

    // 파라미터 변경
    value.float_value = 0.1f;
    plugin_set_parameter_by_id(reverb, 0, value);

    // 프리셋 로드
    result = load_effect_preset(reverb, &preset);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to load preset");

    // 파라미터 확인
    PluginParamValue loaded_value;
    plugin_get_parameter_by_id(reverb, 0, &loaded_value);
    TEST_ASSERT(fabsf(loaded_value.float_value - 0.7f) < 0.001f, "Preset not loaded correctly");

    // 정리
    if (preset.params) {
        free(preset.params);
    }
    plugin_finalize(reverb);
    free(reverb->param_values);
    free(reverb);

    TEST_SUCCESS();
}

// 실시간 파라미터 조정 테스트
int test_realtime_parameter_adjustment(void) {
    PluginInstance* reverb = create_reverb_plugin(NULL);
    TEST_ASSERT(reverb != NULL, "Failed to create reverb plugin");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 0.5f
    };

    plugin_initialize(reverb, &config);
    plugin_activate(reverb);

    // 웨트/드라이 믹스 설정 테스트
    ETErrorCode result = set_effect_wet_dry_mix(reverb, 0.8f);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to set wet/dry mix");

    // 바이패스 설정 테스트
    result = set_effect_bypass(reverb, true);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to set bypass");

    // 지연 시간 조회 테스트
    int latency_samples;
    result = get_effect_latency(reverb, &latency_samples);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to get latency");
    TEST_ASSERT(latency_samples >= 0, "Invalid latency value");

    // 테일 타임 조회 테스트
    float tail_time;
    result = get_effect_tail_time(reverb, &tail_time);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to get tail time");
    TEST_ASSERT(tail_time >= 0.0f, "Invalid tail time value");

    // 정리
    plugin_deactivate(reverb);
    plugin_finalize(reverb);
    free(reverb->param_values);
    free(reverb);

    TEST_SUCCESS();
}

// 딜레이 플러그인 테스트
int test_delay_plugin_creation(void) {
    PluginInstance* delay = create_delay_plugin(NULL);
    TEST_ASSERT(delay != NULL, "Failed to create delay plugin");
    TEST_ASSERT(delay->metadata.type == PLUGIN_TYPE_AUDIO_EFFECT, "Wrong plugin type");
    TEST_ASSERT(strcmp(delay->metadata.name, "LibEtude Delay") == 0, "Wrong plugin name");
    TEST_ASSERT(delay->num_parameters == 6, "Wrong number of parameters");

    // 메모리 해제
    if (delay->param_values) {
        free(delay->param_values);
    }
    free(delay);

    TEST_SUCCESS();
}

// 컴프레서 플러그인 테스트
int test_compressor_plugin_creation(void) {
    PluginInstance* compressor = create_compressor_plugin(NULL);
    TEST_ASSERT(compressor != NULL, "Failed to create compressor plugin");
    TEST_ASSERT(compressor->metadata.type == PLUGIN_TYPE_AUDIO_EFFECT, "Wrong plugin type");
    TEST_ASSERT(strcmp(compressor->metadata.name, "LibEtude Compressor") == 0, "Wrong plugin name");
    TEST_ASSERT(compressor->num_parameters == 8, "Wrong number of parameters");

    // 메모리 해제
    if (compressor->param_values) {
        free(compressor->param_values);
    }
    free(compressor);

    TEST_SUCCESS();
}

// 다중 효과 파이프라인 테스트
int test_multi_effect_pipeline(void) {
    // 파이프라인 생성
    AudioEffectPipeline* pipeline = create_audio_effect_pipeline(10);
    TEST_ASSERT(pipeline != NULL, "Failed to create audio effect pipeline");

    // 여러 플러그인들 생성
    PluginInstance* reverb = create_reverb_plugin(NULL);
    PluginInstance* eq = create_equalizer_plugin(NULL);
    PluginInstance* delay = create_delay_plugin(NULL);
    PluginInstance* compressor = create_compressor_plugin(NULL);

    TEST_ASSERT(reverb != NULL && eq != NULL && delay != NULL && compressor != NULL,
                "Failed to create plugins");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    // 플러그인 초기화
    plugin_initialize(reverb, &config);
    plugin_activate(reverb);
    plugin_initialize(eq, &config);
    plugin_activate(eq);
    plugin_initialize(delay, &config);
    plugin_activate(delay);
    plugin_initialize(compressor, &config);
    plugin_activate(compressor);

    // 파이프라인에 추가 (컴프레서 -> EQ -> 딜레이 -> 리버브 순서)
    ETErrorCode result = add_effect_to_pipeline(pipeline, compressor);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add compressor to pipeline");

    result = add_effect_to_pipeline(pipeline, eq);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add equalizer to pipeline");

    result = add_effect_to_pipeline(pipeline, delay);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add delay to pipeline");

    result = add_effect_to_pipeline(pipeline, reverb);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add reverb to pipeline");

    // 테스트 신호 생성
    const int buffer_size = 1024;
    float* input = (float*)malloc(buffer_size * sizeof(float));
    float* output = (float*)malloc(buffer_size * sizeof(float));

    generate_sine_wave(input, buffer_size, 440.0f, 44100.0f);

    // 파이프라인 처리
    result = process_audio_pipeline(pipeline, input, output, buffer_size);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to process multi-effect pipeline");

    float output_rms = calculate_rms(output, buffer_size);
    TEST_ASSERT(output_rms > 0.0f, "Multi-effect pipeline output is silent");

    // 정리
    free(input);
    free(output);
    destroy_audio_effect_pipeline(pipeline);

    plugin_deactivate(reverb);
    plugin_finalize(reverb);
    free(reverb->param_values);
    free(reverb);

    plugin_deactivate(eq);
    plugin_finalize(eq);
    free(eq->param_values);
    free(eq);

    plugin_deactivate(delay);
    plugin_finalize(delay);
    free(delay->param_values);
    free(delay);

    plugin_deactivate(compressor);
    plugin_finalize(compressor);
    free(compressor->param_values);
    free(compressor);

    TEST_SUCCESS();
}

// 파라미터 자동화 테스트
int test_parameter_automation(void) {
    PluginInstance* delay = create_delay_plugin(NULL);
    TEST_ASSERT(delay != NULL, "Failed to create delay plugin");

    AudioEffectConfig config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 0.5f
    };

    plugin_initialize(delay, &config);
    plugin_activate(delay);

    // 파라미터 자동화 시뮬레이션 (딜레이 시간 변경)
    const int buffer_size = 512;
    float* input = (float*)malloc(buffer_size * sizeof(float));
    float* output = (float*)malloc(buffer_size * sizeof(float));

    generate_sine_wave(input, buffer_size, 1000.0f, 44100.0f);

    // 여러 딜레이 시간으로 처리
    float delay_times[] = {100.0f, 200.0f, 300.0f, 150.0f};
    int num_variations = sizeof(delay_times) / sizeof(float);

    for (int i = 0; i < num_variations; i++) {
        PluginParamValue value;
        value.float_value = delay_times[i];
        ETErrorCode result = plugin_set_parameter_by_id(delay, 0, value);
        TEST_ASSERT(result == ET_SUCCESS, "Failed to set delay time parameter");

        result = plugin_process(delay, input, output, buffer_size);
        TEST_ASSERT(result == ET_SUCCESS, "Failed to process delay with automation");

        float output_rms = calculate_rms(output, buffer_size);
        TEST_ASSERT(output_rms > 0.0f, "Automated delay output is silent");
    }

    // 정리
    free(input);
    free(output);
    plugin_deactivate(delay);
    plugin_finalize(delay);
    free(delay->param_values);
    free(delay);

    TEST_SUCCESS();
}

// 메인 테스트 함수
int main(void) {
    printf("LibEtude Audio Effects Plugin Tests\n");
    printf("===================================\n\n");

    int passed = 0;
    int total = 0;

    // 기존 테스트들
    total++; passed += test_reverb_plugin_creation();
    total++; passed += test_reverb_plugin_initialization();
    total++; passed += test_reverb_plugin_processing();
    total++; passed += test_reverb_parameter_setting();
    total++; passed += test_equalizer_plugin_creation();
    total++; passed += test_equalizer_plugin_processing();
    total++; passed += test_audio_effect_pipeline();
    total++; passed += test_preset_management();
    total++; passed += test_realtime_parameter_adjustment();

    // 새로운 테스트들
    total++; passed += test_delay_plugin_creation();
    total++; passed += test_compressor_plugin_creation();
    total++; passed += test_multi_effect_pipeline();
    total++; passed += test_parameter_automation();

    // 결과 출력
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", passed, total);

    if (passed == total) {
        printf("All tests passed! ✓\n");
        return 0;
    } else {
        printf("Some tests failed! ✗\n");
        return 1;
    }
}