#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libetude/plugin.h"
#include "libetude/audio_effects.h"
#include "libetude/error.h"

// 테스트용 오디오 신호 생성 (사인파)
void generate_test_signal(float* buffer, int num_samples, float frequency, float sample_rate) {
    for (int i = 0; i < num_samples; i++) {
        buffer[i] = 0.5f * sinf(2.0f * M_PI * frequency * i / sample_rate);
    }
}

// 오디오 신호 분석 (RMS 계산)
float calculate_rms(const float* buffer, int num_samples) {
    float sum = 0.0f;
    for (int i = 0; i < num_samples; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrtf(sum / num_samples);
}

// 리버브 효과 데모
void demo_reverb_effect(float* input_buffer, float* output_buffer, int buffer_size, float sample_rate) {
    printf("=== Reverb Effect Demo ===\n");

    // 오디오 효과 설정
    AudioEffectConfig effect_config = {
        .sample_rate = sample_rate,
        .num_channels = 1,
        .buffer_size = buffer_size,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 0.5f
    };

    // 리버브 효과 생성
    PluginInstance* reverb_plugin = create_reverb_plugin(NULL);
    if (!reverb_plugin) {
        printf("Failed to create reverb plugin\n");
        return;
    }

    // 리버브 초기화
    ETErrorCode result = plugin_initialize(reverb_plugin, &effect_config);
    if (result != ET_SUCCESS) {
        printf("Failed to initialize reverb plugin: %d\n", result);
        return;
    }

    // 리버브 활성화
    result = plugin_activate(reverb_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to activate reverb plugin: %d\n", result);
        plugin_finalize(reverb_plugin);
        return;
    }

    // 리버브 파라미터 설정
    PluginParamValue param_value;
    param_value.float_value = 0.7f; // room_size
    plugin_set_parameter_by_id(reverb_plugin, 0, param_value);

    param_value.float_value = 0.3f; // damping
    plugin_set_parameter_by_id(reverb_plugin, 1, param_value);

    param_value.float_value = 0.4f; // wet_dry_mix
    plugin_set_parameter_by_id(reverb_plugin, 2, param_value);

    printf("Reverb parameters set: room_size=0.7, damping=0.3, wet_dry_mix=0.4\n");

    // 리버브 처리
    result = plugin_process(reverb_plugin, input_buffer, output_buffer, buffer_size);
    if (result != ET_SUCCESS) {
        printf("Failed to process reverb: %d\n", result);
    } else {
        printf("✓ Reverb processed successfully\n");
        printf("  Input RMS: %.4f, Output RMS: %.4f\n",
               calculate_rms(input_buffer, buffer_size),
               calculate_rms(output_buffer, buffer_size));

        // 지연 시간 및 테일 타임 조회
        int latency_samples;
        float tail_time;
        if (get_effect_latency(reverb_plugin, &latency_samples) == ET_SUCCESS) {
            printf("  Latency: %d samples (%.2f ms)\n",
                   latency_samples, (float)latency_samples * 1000.0f / sample_rate);
        }
        if (get_effect_tail_time(reverb_plugin, &tail_time) == ET_SUCCESS) {
            printf("  Tail time: %.2f seconds\n", tail_time);
        }
    }

    // 정리
    plugin_deactivate(reverb_plugin);
    plugin_finalize(reverb_plugin);
    free(reverb_plugin->param_values);
    free(reverb_plugin);

    printf("Reverb demo completed.\n\n");
}

// 딜레이 효과 데모
void demo_delay_effect(float* input_buffer, float* output_buffer, int buffer_size, float sample_rate) {
    printf("=== Delay Effect Demo ===\n");

    // 오디오 효과 설정
    AudioEffectConfig effect_config = {
        .sample_rate = sample_rate,
        .num_channels = 1,
        .buffer_size = buffer_size,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 0.6f
    };

    // 딜레이 효과 생성
    PluginInstance* delay_plugin = create_delay_plugin(NULL);
    if (!delay_plugin) {
        printf("Failed to create delay plugin\n");
        return;
    }

    // 딜레이 초기화
    ETErrorCode result = plugin_initialize(delay_plugin, &effect_config);
    if (result != ET_SUCCESS) {
        printf("Failed to initialize delay plugin: %d\n", result);
        return;
    }

    // 딜레이 활성화
    result = plugin_activate(delay_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to activate delay plugin: %d\n", result);
        plugin_finalize(delay_plugin);
        return;
    }

    // 딜레이 파라미터 설정
    PluginParamValue param_value;
    param_value.float_value = 300.0f; // delay_time (300ms)
    plugin_set_parameter_by_id(delay_plugin, 0, param_value);

    param_value.float_value = 0.4f; // feedback
    plugin_set_parameter_by_id(delay_plugin, 1, param_value);

    param_value.float_value = 0.6f; // wet_dry_mix
    plugin_set_parameter_by_id(delay_plugin, 4, param_value);

    printf("Delay parameters set: delay_time=300ms, feedback=0.4, wet_dry_mix=0.6\n");

    // 딜레이 처리
    result = plugin_process(delay_plugin, input_buffer, output_buffer, buffer_size);
    if (result != ET_SUCCESS) {
        printf("Failed to process delay: %d\n", result);
    } else {
        printf("✓ Delay processed successfully\n");
        printf("  Input RMS: %.4f, Output RMS: %.4f\n",
               calculate_rms(input_buffer, buffer_size),
               calculate_rms(output_buffer, buffer_size));

        // 지연 시간 조회
        int latency_samples;
        if (get_effect_latency(delay_plugin, &latency_samples) == ET_SUCCESS) {
            printf("  Latency: %d samples (%.2f ms)\n",
                   latency_samples, (float)latency_samples * 1000.0f / sample_rate);
        }
    }

    // 정리
    plugin_deactivate(delay_plugin);
    plugin_finalize(delay_plugin);
    free(delay_plugin->param_values);
    free(delay_plugin);

    printf("Delay demo completed.\n\n");
}

// 컴프레서 효과 데모
void demo_compressor_effect(float* input_buffer, float* output_buffer, int buffer_size, float sample_rate) {
    printf("=== Compressor Effect Demo ===\n");

    // 오디오 효과 설정
    AudioEffectConfig effect_config = {
        .sample_rate = sample_rate,
        .num_channels = 1,
        .buffer_size = buffer_size,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    // 컴프레서 효과 생성
    PluginInstance* compressor_plugin = create_compressor_plugin(NULL);
    if (!compressor_plugin) {
        printf("Failed to create compressor plugin\n");
        return;
    }

    // 컴프레서 초기화
    ETErrorCode result = plugin_initialize(compressor_plugin, &effect_config);
    if (result != ET_SUCCESS) {
        printf("Failed to initialize compressor plugin: %d\n", result);
        return;
    }

    // 컴프레서 활성화
    result = plugin_activate(compressor_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to activate compressor plugin: %d\n", result);
        plugin_finalize(compressor_plugin);
        return;
    }

    // 컴프레서 파라미터 설정
    PluginParamValue param_value;
    param_value.float_value = -18.0f; // threshold
    plugin_set_parameter_by_id(compressor_plugin, 0, param_value);

    param_value.float_value = 6.0f; // ratio
    plugin_set_parameter_by_id(compressor_plugin, 1, param_value);

    param_value.float_value = 3.0f; // attack_time
    plugin_set_parameter_by_id(compressor_plugin, 2, param_value);

    param_value.float_value = 80.0f; // release_time
    plugin_set_parameter_by_id(compressor_plugin, 3, param_value);

    param_value.bool_value = true; // auto_makeup
    plugin_set_parameter_by_id(compressor_plugin, 6, param_value);

    printf("Compressor parameters set: threshold=-18dB, ratio=6:1, attack=3ms, release=80ms, auto_makeup=on\n");

    // 컴프레서 처리
    result = plugin_process(compressor_plugin, input_buffer, output_buffer, buffer_size);
    if (result != ET_SUCCESS) {
        printf("Failed to process compressor: %d\n", result);
    } else {
        printf("✓ Compressor processed successfully\n");
        printf("  Input RMS: %.4f, Output RMS: %.4f\n",
               calculate_rms(input_buffer, buffer_size),
               calculate_rms(output_buffer, buffer_size));

        // 지연 시간 조회
        int latency_samples;
        if (get_effect_latency(compressor_plugin, &latency_samples) == ET_SUCCESS) {
            printf("  Latency: %d samples (%.2f ms)\n",
                   latency_samples, (float)latency_samples * 1000.0f / sample_rate);
        }
    }

    // 정리
    plugin_deactivate(compressor_plugin);
    plugin_finalize(compressor_plugin);
    free(compressor_plugin->param_values);
    free(compressor_plugin);

    printf("Compressor demo completed.\n\n");
}

// 이퀄라이저 효과 데모
void demo_equalizer_effect(float* input_buffer, float* output_buffer, int buffer_size, float sample_rate) {
    printf("=== Equalizer Effect Demo ===\n");

    // 오디오 효과 설정
    AudioEffectConfig effect_config = {
        .sample_rate = sample_rate,
        .num_channels = 1,
        .buffer_size = buffer_size,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    // 이퀄라이저 효과 생성
    PluginInstance* eq_plugin = create_equalizer_plugin(NULL);
    if (!eq_plugin) {
        printf("Failed to create equalizer plugin\n");
        return;
    }

    // 이퀄라이저 초기화
    ETErrorCode result = plugin_initialize(eq_plugin, &effect_config);
    if (result != ET_SUCCESS) {
        printf("Failed to initialize equalizer plugin: %d\n", result);
        return;
    }

    // 이퀄라이저 활성화
    result = plugin_activate(eq_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to activate equalizer plugin: %d\n", result);
        plugin_finalize(eq_plugin);
        return;
    }

    // 이퀄라이저 파라미터 설정 (몇 개 밴드만)
    PluginParamValue param_value;

    param_value.float_value = 3.0f; // 첫 번째 밴드 +3dB
    plugin_set_parameter_by_id(eq_plugin, 0, param_value);

    param_value.float_value = -2.0f; // 두 번째 밴드 -2dB
    plugin_set_parameter_by_id(eq_plugin, 1, param_value);

    param_value.float_value = 1.5f; // 세 번째 밴드 +1.5dB
    plugin_set_parameter_by_id(eq_plugin, 2, param_value);

    printf("EQ parameters set: Band1=+3dB, Band2=-2dB, Band3=+1.5dB\n");

    // 이퀄라이저 처리
    result = plugin_process(eq_plugin, input_buffer, output_buffer, buffer_size);
    if (result != ET_SUCCESS) {
        printf("Failed to process equalizer: %d\n", result);
    } else {
        printf("✓ Equalizer processed successfully\n");
        printf("  Input RMS: %.4f, Output RMS: %.4f\n",
               calculate_rms(input_buffer, buffer_size),
               calculate_rms(output_buffer, buffer_size));
    }

    // 정리
    plugin_deactivate(eq_plugin);
    plugin_finalize(eq_plugin);
    free(eq_plugin->param_values);
    free(eq_plugin);

    printf("Equalizer demo completed.\n\n");
}

// 오디오 효과 파이프라인 데모
void demo_audio_pipeline(float* input_buffer, float* output_buffer, int buffer_size, float sample_rate) {
    printf("=== Audio Effect Pipeline Demo ===\n");

    // 오디오 효과 설정
    AudioEffectConfig effect_config = {
        .sample_rate = sample_rate,
        .num_channels = 1,
        .buffer_size = buffer_size,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    // 여러 플러그인 생성
    PluginInstance* compressor_plugin = create_compressor_plugin(NULL);
    PluginInstance* eq_plugin = create_equalizer_plugin(NULL);
    PluginInstance* delay_plugin = create_delay_plugin(NULL);
    PluginInstance* reverb_plugin = create_reverb_plugin(NULL);

    if (!compressor_plugin || !eq_plugin || !delay_plugin || !reverb_plugin) {
        printf("Failed to create plugins\n");
        return;
    }

    // 플러그인 초기화 및 활성화
    if (plugin_initialize(compressor_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(compressor_plugin) != ET_SUCCESS ||
        plugin_initialize(eq_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(eq_plugin) != ET_SUCCESS ||
        plugin_initialize(delay_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(delay_plugin) != ET_SUCCESS ||
        plugin_initialize(reverb_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(reverb_plugin) != ET_SUCCESS) {
        printf("Failed to initialize plugins\n");
        goto cleanup;
    }

    // 오디오 효과 파이프라인 생성
    AudioEffectPipeline* pipeline = create_audio_effect_pipeline(10);
    if (!pipeline) {
        printf("Failed to create audio effect pipeline\n");
        goto cleanup;
    }

    // 파이프라인에 효과들 추가 (컴프레서 -> 이퀄라이저 -> 딜레이 -> 리버브 순서)
    ETErrorCode result = add_effect_to_pipeline(pipeline, compressor_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to add compressor to pipeline: %d\n", result);
        goto cleanup;
    }

    result = add_effect_to_pipeline(pipeline, eq_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to add equalizer to pipeline: %d\n", result);
        goto cleanup;
    }

    result = add_effect_to_pipeline(pipeline, delay_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to add delay to pipeline: %d\n", result);
        goto cleanup;
    }

    result = add_effect_to_pipeline(pipeline, reverb_plugin);
    if (result != ET_SUCCESS) {
        printf("Failed to add reverb to pipeline: %d\n", result);
        goto cleanup;
    }

    printf("Added compressor, equalizer, delay, and reverb to pipeline\n");

    // 파이프라인으로 처리
    result = process_audio_pipeline(pipeline, input_buffer, output_buffer, buffer_size);
    if (result != ET_SUCCESS) {
        printf("Failed to process audio pipeline: %d\n", result);
    } else {
        printf("✓ Pipeline processed successfully\n");
        printf("  Input RMS: %.4f, Output RMS: %.4f\n",
               calculate_rms(input_buffer, buffer_size),
               calculate_rms(output_buffer, buffer_size));
    }

    // 파이프라인 바이패스 테스트
    printf("Testing pipeline bypass...\n");
    set_pipeline_bypass(pipeline, true);
    result = process_audio_pipeline(pipeline, input_buffer, output_buffer, buffer_size);
    if (result == ET_SUCCESS) {
        printf("✓ Pipeline bypass successful\n");
        printf("  Bypassed output RMS: %.4f (should equal input)\n",
               calculate_rms(output_buffer, buffer_size));
    }

    // 파이프라인 정리
    destroy_audio_effect_pipeline(pipeline);

cleanup:
    // 플러그인 정리
    if (reverb_plugin) {
        plugin_deactivate(reverb_plugin);
        plugin_finalize(reverb_plugin);
        free(reverb_plugin->param_values);
        free(reverb_plugin);
    }

    if (eq_plugin) {
        plugin_deactivate(eq_plugin);
        plugin_finalize(eq_plugin);
        free(eq_plugin->param_values);
        free(eq_plugin);
    }

    printf("Audio pipeline demo completed.\n\n");
}

// 프리셋 관리 데모
void demo_preset_management(void) {
    printf("=== Preset Management Demo ===\n");

    // 오디오 효과 설정
    AudioEffectConfig effect_config = {
        .sample_rate = 44100,
        .num_channels = 1,
        .buffer_size = 1024,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    // 리버브 플러그인 생성 및 초기화
    PluginInstance* reverb_plugin = create_reverb_plugin(NULL);
    if (!reverb_plugin) {
        printf("Failed to create reverb plugin\n");
        return;
    }

    if (plugin_initialize(reverb_plugin, &effect_config) != ET_SUCCESS) {
        printf("Failed to initialize reverb plugin\n");
        free(reverb_plugin);
        return;
    }

    // 리버브 파라미터 설정
    PluginParamValue param_value;
    param_value.float_value = 0.8f; // room_size
    plugin_set_parameter_by_id(reverb_plugin, 0, param_value);

    param_value.float_value = 0.2f; // damping
    plugin_set_parameter_by_id(reverb_plugin, 1, param_value);

    param_value.float_value = 0.6f; // wet_dry_mix
    plugin_set_parameter_by_id(reverb_plugin, 2, param_value);

    printf("Set reverb parameters: room_size=0.8, damping=0.2, wet_dry_mix=0.6\n");

    // 프리셋 저장
    AudioEffectPreset reverb_preset;
    ETErrorCode result = save_effect_preset(reverb_plugin, "Large Cathedral", &reverb_preset);
    if (result == ET_SUCCESS) {
        printf("✓ Reverb preset 'Large Cathedral' saved successfully\n");

        // 프리셋을 파일로 내보내기
        result = export_preset_to_file(&reverb_preset, "large_cathedral_reverb.preset");
        if (result == ET_SUCCESS) {
            printf("✓ Preset exported to file: large_cathedral_reverb.preset\n");
        }

        // 파라미터 변경
        param_value.float_value = 0.3f; // room_size 변경
        plugin_set_parameter_by_id(reverb_plugin, 0, param_value);
        printf("Changed room_size to 0.3\n");

        // 프리셋 로드
        result = load_effect_preset(reverb_plugin, &reverb_preset);
        if (result == ET_SUCCESS) {
            printf("✓ Preset loaded successfully\n");

            // 파라미터 확인
            PluginParamValue loaded_value;
            plugin_get_parameter_by_id(reverb_plugin, 0, &loaded_value);
            printf("  Restored room_size: %.2f (should be 0.8)\n", loaded_value.float_value);
        }

        // 프리셋 메모리 해제
        if (reverb_preset.params) {
            free(reverb_preset.params);
        }
    }

    // 정리
    plugin_finalize(reverb_plugin);
    free(reverb_plugin->param_values);
    free(reverb_plugin);

    printf("Preset management demo completed.\n\n");
}

// 성능 테스트 데모
void demo_performance_test(float* input_buffer, float* output_buffer, int buffer_size, float sample_rate) {
    printf("=== Performance Test Demo ===\n");

    // 오디오 효과 설정
    AudioEffectConfig effect_config = {
        .sample_rate = sample_rate,
        .num_channels = 1,
        .buffer_size = buffer_size,
        .quality = AUDIO_QUALITY_HIGH,
        .bypass = false,
        .wet_dry_mix = 1.0f
    };

    // 여러 효과 플러그인 생성
    PluginInstance* compressor_plugin = create_compressor_plugin(NULL);
    PluginInstance* eq_plugin = create_equalizer_plugin(NULL);
    PluginInstance* delay_plugin = create_delay_plugin(NULL);
    PluginInstance* reverb_plugin = create_reverb_plugin(NULL);

    if (!compressor_plugin || !eq_plugin || !delay_plugin || !reverb_plugin) {
        printf("Failed to create plugins\n");
        return;
    }

    // 플러그인 초기화 및 활성화
    if (plugin_initialize(compressor_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(compressor_plugin) != ET_SUCCESS ||
        plugin_initialize(eq_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(eq_plugin) != ET_SUCCESS ||
        plugin_initialize(delay_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(delay_plugin) != ET_SUCCESS ||
        plugin_initialize(reverb_plugin, &effect_config) != ET_SUCCESS ||
        plugin_activate(reverb_plugin) != ET_SUCCESS) {
        printf("Failed to initialize plugins\n");
        goto cleanup;
    }

    // 파이프라인 생성 및 설정
    AudioEffectPipeline* pipeline = create_audio_effect_pipeline(10);
    if (!pipeline) {
        printf("Failed to create pipeline\n");
        goto cleanup;
    }

    add_effect_to_pipeline(pipeline, compressor_plugin);
    add_effect_to_pipeline(pipeline, eq_plugin);
    add_effect_to_pipeline(pipeline, delay_plugin);
    add_effect_to_pipeline(pipeline, reverb_plugin);

    // 성능 테스트 (여러 번 처리)
    const int num_iterations = 1000;
    printf("Running performance test (%d iterations)...\n", num_iterations);

    // 간단한 시간 측정 (실제로는 고해상도 타이머 사용)
    for (int i = 0; i < num_iterations; i++) {
        process_audio_pipeline(pipeline, input_buffer, output_buffer, buffer_size);
    }

    printf("✓ Performance test completed\n");
    printf("  Processed %d samples x %d iterations = %d total samples\n",
           buffer_size, num_iterations, buffer_size * num_iterations);
    printf("  Final output RMS: %.4f\n", calculate_rms(output_buffer, buffer_size));

    // 파이프라인 정리
    destroy_audio_effect_pipeline(pipeline);

cleanup:
    // 플러그인 정리
    if (reverb_plugin) {
        plugin_deactivate(reverb_plugin);
        plugin_finalize(reverb_plugin);
        free(reverb_plugin->param_values);
        free(reverb_plugin);
    }

    if (eq_plugin) {
        plugin_deactivate(eq_plugin);
        plugin_finalize(eq_plugin);
        free(eq_plugin->param_values);
        free(eq_plugin);
    }

    printf("Performance test demo completed.\n\n");
}

int main() {
    printf("LibEtude Audio Effects Plugin Example\n");
    printf("=====================================\n\n");

    // 오디오 설정
    const int sample_rate = 44100;
    const int buffer_size = 1024;
    const float test_frequency = 440.0f; // A4 음

    // 테스트 버퍼 할당
    float* input_buffer = (float*)malloc(buffer_size * sizeof(float));
    float* output_buffer = (float*)malloc(buffer_size * sizeof(float));

    if (!input_buffer || !output_buffer) {
        printf("Failed to allocate audio buffers\n");
        return 1;
    }

    // 테스트 신호 생성
    generate_test_signal(input_buffer, buffer_size, test_frequency, sample_rate);
    printf("Generated test signal: %.1f Hz sine wave\n", test_frequency);
    printf("Input RMS level: %.4f\n\n", calculate_rms(input_buffer, buffer_size));

    // 각종 데모 실행
    demo_reverb_effect(input_buffer, output_buffer, buffer_size, sample_rate);
    demo_equalizer_effect(input_buffer, output_buffer, buffer_size, sample_rate);
    demo_delay_effect(input_buffer, output_buffer, buffer_size, sample_rate);
    demo_compressor_effect(input_buffer, output_buffer, buffer_size, sample_rate);
    demo_audio_pipeline(input_buffer, output_buffer, buffer_size, sample_rate);
    demo_preset_management();
    demo_performance_test(input_buffer, output_buffer, buffer_size, sample_rate);

    // 버퍼 해제
    free(input_buffer);
    free(output_buffer);

    printf("All audio effects plugin examples completed successfully!\n");
    return 0;
}