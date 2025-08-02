/**
 * @file world_pipeline_config.c
 * @brief WORLD 파이프라인 설정 시스템 구현
 */

#include "world_pipeline_config.h"
#include "world_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

// =============================================================================
// 내부 헬퍼 함수 선언
// =============================================================================

static double get_current_time(void);
static bool is_power_of_two(int value);
static int next_power_of_two(int value);
static void trim_string(char* str);
static ETResult parse_json_value(const char* json, const char* key, char* value, size_t value_size);
static ETResult format_json_field(char* buffer, size_t buffer_size, const char* key,
                                  const char* value, bool is_last);

// =============================================================================
// 카테고리별 기본 설정 생성 함수들
// =============================================================================

WorldAudioConfig world_audio_config_default(void) {
    WorldAudioConfig config = {0};

    config.sample_rate = 44100;
    config.frame_size = 1024;
    config.buffer_size = 4096;
    config.bit_depth = 16;
    config.channel_count = 1;
    config.enable_dithering = false;
    config.input_gain = 0.0f;
    config.output_gain = 0.0f;
    config.enable_dc_removal = true;
    config.enable_anti_aliasing = true;

    return config;
}

WorldF0Config world_f0_config_default(void) {
    WorldF0Config config = {0};

    config.frame_period = 5.0;
    config.f0_floor = 71.0;
    config.f0_ceil = 800.0;
    config.algorithm = 0; // DIO
    config.channels_in_octave = 2.0;
    config.target_fs = 4000.0;
    config.enable_refinement = true;
    config.speed = 1.0;
    config.allow_range_extension = false;
    config.threshold = 0.85;

    return config;
}

WorldSpectrumConfig world_spectrum_config_default(void) {
    WorldSpectrumConfig config = {0};

    config.q1 = -0.15;
    config.fft_size = 2048;
    config.enable_power_spectrum = false;
    config.frequency_interval = 3000.0;
    config.frequency_bins = 513; // (fft_size/2 + 1)
    config.enable_spectral_smoothing = false;
    config.smoothing_factor = 0.1;
    config.enable_preemphasis = false;
    config.preemphasis_coefficient = 0.97;

    return config;
}

WorldAperiodicityConfig world_aperiodicity_config_default(void) {
    WorldAperiodicityConfig config = {0};

    config.threshold = 0.85;
    config.frequency_bands = 5;
    config.enable_band_aperiodicity = true;
    config.window_length = 35.0;
    config.enable_adaptive_windowing = false;
    config.noise_floor = -60.0;
    config.enable_spectral_recovery = false;

    return config;
}

WorldSynthesisConfig world_synthesis_config_default(void) {
    WorldSynthesisConfig config = {0};

    config.sample_rate = 44100;
    config.frame_period = 5.0;
    config.enable_postfilter = true;
    config.postfilter_coefficient = 0.5;
    config.enable_pitch_adaptive_spectral_smoothing = false;
    config.enable_seed_signals = false;
    config.synthesis_speed = 1.0;
    config.enable_overlap_add = true;
    config.overlap_length = 512;

    return config;
}

WorldGraphOptimizationConfig world_graph_optimization_config_default(void) {
    WorldGraphOptimizationConfig config = {0};

    config.enable_node_fusion = true;
    config.enable_memory_reuse = true;
    config.enable_simd_optimization = true;
    config.enable_parallel_execution = true;
    config.max_thread_count = 0; // 자동
    config.enable_cache_optimization = true;
    config.enable_dead_code_elimination = true;
    config.enable_constant_folding = true;
    config.optimization_level = 0.8;
    config.memory_budget = 128 * 1024 * 1024; // 128MB

    return config;
}

WorldMemoryConfig world_memory_config_default(void) {
    WorldMemoryConfig config = {0};

    config.memory_pool_size = 64 * 1024 * 1024; // 64MB
    config.analysis_pool_size = 16 * 1024 * 1024; // 16MB
    config.synthesis_pool_size = 16 * 1024 * 1024; // 16MB
    config.cache_pool_size = 32 * 1024 * 1024; // 32MB
    config.enable_memory_tracking = false;
    config.enable_leak_detection = false;
    config.gc_threshold = 0.8;
    config.enable_memory_compression = false;

    return config;
}

WorldPerformanceConfig world_performance_config_default(void) {
    WorldPerformanceConfig config = {0};

    config.enable_profiling = false;
    config.enable_timing_analysis = false;
    config.enable_memory_profiling = false;
    config.enable_cpu_profiling = false;
    config.enable_gpu_profiling = false;
    config.profiling_interval_ms = 100;
    strcpy(config.profile_output_dir, "./profile");
    config.enable_realtime_monitoring = false;

    return config;
}

WorldDebugConfig world_debug_config_default(void) {
    WorldDebugConfig config = {0};

    config.enable_debug_output = false;
    config.enable_verbose_logging = false;
    config.enable_intermediate_dumps = false;
    config.enable_graph_visualization = false;
    strcpy(config.debug_output_dir, "./debug");
    strcpy(config.log_file_path, "./debug/world_pipeline.log");
    config.log_level = 2; // INFO 레벨
    config.enable_assertion_checks = false;

    return config;
}

// =============================================================================
// 통합 설정 관리 함수들
// =============================================================================

WorldPipelineConfiguration world_pipeline_config_create_default(void) {
    WorldPipelineConfiguration config = {0};

    // 카테고리별 기본 설정 적용
    config.audio = world_audio_config_default();
    config.f0 = world_f0_config_default();
    config.spectrum = world_spectrum_config_default();
    config.aperiodicity = world_aperiodicity_config_default();
    config.synthesis = world_synthesis_config_default();
    config.optimization = world_graph_optimization_config_default();
    config.memory = world_memory_config_default();
    config.performance = world_performance_config_default();
    config.debug = world_debug_config_default();

    // 메타 정보 설정
    strcpy(config.config_name, "Default");
    strcpy(config.config_version, "1.0.0");
    strcpy(config.description, "기본 WORLD 파이프라인 설정");
    config.creation_time = get_current_time();
    config.modification_time = config.creation_time;

    return config;
}

WorldPipelineConfiguration world_pipeline_config_create_preset(WorldConfigPreset preset) {
    WorldPipelineConfiguration config = world_pipeline_config_create_default();

    switch (preset) {
        case WORLD_CONFIG_PRESET_HIGH_QUALITY:
            strcpy(config.config_name, "High Quality");
            strcpy(config.description, "고품질 음성 합성을 위한 설정");

            // 고품질 설정 적용
            config.audio.sample_rate = 48000;
            config.audio.bit_depth = 24;
            config.audio.enable_dithering = true;
            config.audio.enable_anti_aliasing = true;

            config.f0.algorithm = 1; // Harvest (더 정확함)
            config.f0.enable_refinement = true;
            config.f0.speed = 0.5; // 느리지만 정확

            config.spectrum.fft_size = 4096;
            config.spectrum.enable_spectral_smoothing = true;
            config.spectrum.smoothing_factor = 0.05;

            config.synthesis.enable_postfilter = true;
            config.synthesis.enable_pitch_adaptive_spectral_smoothing = true;

            config.optimization.optimization_level = 0.5; // 품질 우선
            break;

        case WORLD_CONFIG_PRESET_FAST:
            strcpy(config.config_name, "Fast Processing");
            strcpy(config.description, "고속 처리를 위한 설정");

            // 고속 처리 설정 적용
            config.audio.frame_size = 512;
            config.audio.buffer_size = 2048;

            config.f0.algorithm = 0; // DIO (빠름)
            config.f0.frame_period = 10.0; // 더 큰 프레임 주기
            config.f0.speed = 2.0; // 빠른 처리
            config.f0.enable_refinement = false;

            config.spectrum.fft_size = 1024;
            config.spectrum.enable_spectral_smoothing = false;

            config.synthesis.enable_postfilter = false;
            config.synthesis.synthesis_speed = 1.5;

            config.optimization.optimization_level = 1.0; // 최대 최적화
            config.optimization.max_thread_count = 8;
            break;

        case WORLD_CONFIG_PRESET_LOW_LATENCY:
            strcpy(config.config_name, "Low Latency");
            strcpy(config.description, "저지연 실시간 처리를 위한 설정");

            // 저지연 설정 적용
            config.audio.frame_size = 256;
            config.audio.buffer_size = 1024;

            config.f0.frame_period = 2.5;
            config.f0.speed = 3.0;

            config.spectrum.fft_size = 512;

            config.optimization.enable_parallel_execution = true;
            config.optimization.max_thread_count = 4;

            config.memory.memory_pool_size = 32 * 1024 * 1024; // 32MB
            break;

        case WORLD_CONFIG_PRESET_LOW_MEMORY:
            strcpy(config.config_name, "Low Memory");
            strcpy(config.description, "저메모리 환경을 위한 설정");

            // 저메모리 설정 적용
            config.audio.frame_size = 512;
            config.audio.buffer_size = 1024;

            config.spectrum.fft_size = 1024;

            config.memory.memory_pool_size = 16 * 1024 * 1024; // 16MB
            config.memory.analysis_pool_size = 4 * 1024 * 1024; // 4MB
            config.memory.synthesis_pool_size = 4 * 1024 * 1024; // 4MB
            config.memory.cache_pool_size = 8 * 1024 * 1024; // 8MB
            config.memory.enable_memory_compression = true;
            config.memory.gc_threshold = 0.6;

            config.optimization.enable_memory_reuse = true;
            config.optimization.memory_budget = 32 * 1024 * 1024; // 32MB
            break;

        case WORLD_CONFIG_PRESET_REALTIME:
            strcpy(config.config_name, "Realtime");
            strcpy(config.description, "실시간 스트리밍을 위한 설정");

            // 실시간 설정 적용
            config.audio.frame_size = 256;
            config.audio.buffer_size = 512;

            config.f0.frame_period = 5.0;
            config.f0.speed = 2.0;

            config.optimization.enable_parallel_execution = true;
            config.optimization.max_thread_count = 6;

            config.performance.enable_realtime_monitoring = true;
            config.performance.profiling_interval_ms = 50;
            break;

        case WORLD_CONFIG_PRESET_DEBUG:
            strcpy(config.config_name, "Debug");
            strcpy(config.description, "디버깅 및 개발을 위한 설정");

            // 디버깅 설정 적용
            config.debug.enable_debug_output = true;
            config.debug.enable_verbose_logging = true;
            config.debug.enable_intermediate_dumps = true;
            config.debug.enable_graph_visualization = true;
            config.debug.log_level = 5; // DEBUG 레벨
            config.debug.enable_assertion_checks = true;

            config.performance.enable_profiling = true;
            config.performance.enable_timing_analysis = true;
            config.performance.enable_memory_profiling = true;

            config.memory.enable_memory_tracking = true;
            config.memory.enable_leak_detection = true;
            break;

        case WORLD_CONFIG_PRESET_BATCH:
            strcpy(config.config_name, "Batch Processing");
            strcpy(config.description, "배치 처리를 위한 설정");

            // 배치 처리 설정 적용
            config.audio.buffer_size = 8192;

            config.f0.enable_refinement = true;
            config.spectrum.enable_spectral_smoothing = true;
            config.synthesis.enable_postfilter = true;

            config.optimization.max_thread_count = 0; // 모든 코어 사용
            config.memory.memory_pool_size = 256 * 1024 * 1024; // 256MB
            break;

        default:
            // 기본 설정 유지
            break;
    }

    config.modification_time = get_current_time();
    return config;
}

ETResult world_pipeline_config_copy(const WorldPipelineConfiguration* src,
                                   WorldPipelineConfiguration* dst) {
    if (!src || !dst) return ET_ERROR_INVALID_PARAMETER;

    memcpy(dst, src, sizeof(WorldPipelineConfiguration));
    dst->modification_time = get_current_time();

    return ET_SUCCESS;
}

// =============================================================================
// 설정 검증 함수들
// =============================================================================

bool world_audio_config_validate(const WorldAudioConfig* config) {
    if (!config) return false;

    if (config->sample_rate <= 0 || config->sample_rate > 192000) return false;
    if (config->frame_size <= 0 || config->frame_size > 8192) return false;
    if (config->buffer_size < config->frame_size) return false;
    if (config->bit_depth != 16 && config->bit_depth != 24 && config->bit_depth != 32) return false;
    if (config->channel_count <= 0 || config->channel_count > 8) return false;
    if (config->input_gain < -60.0f || config->input_gain > 60.0f) return false;
    if (config->output_gain < -60.0f || config->output_gain > 60.0f) return false;

    return true;
}

bool world_f0_config_validate(const WorldF0Config* config) {
    if (!config) return false;

    if (config->frame_period <= 0.0 || config->frame_period > 50.0) return false;
    if (config->f0_floor <= 0.0 || config->f0_floor >= config->f0_ceil) return false;
    if (config->f0_ceil <= 0.0 || config->f0_ceil > 2000.0) return false;
    if (config->algorithm < 0 || config->algorithm > 1) return false;
    if (config->channels_in_octave <= 0.0 || config->channels_in_octave > 10.0) return false;
    if (config->target_fs <= 0.0 || config->target_fs > 48000.0) return false;
    if (config->speed <= 0.0 || config->speed > 10.0) return false;
    if (config->threshold < 0.0 || config->threshold > 1.0) return false;

    return true;
}

bool world_spectrum_config_validate(const WorldSpectrumConfig* config) {
    if (!config) return false;

    if (config->q1 < -1.0 || config->q1 > 1.0) return false;
    if (config->fft_size < 512 || config->fft_size > 16384) return false;
    if (!is_power_of_two(config->fft_size)) return false;
    if (config->frequency_interval <= 0.0 || config->frequency_interval > 24000.0) return false;
    if (config->frequency_bins <= 0 || config->frequency_bins > config->fft_size) return false;
    if (config->smoothing_factor < 0.0 || config->smoothing_factor > 1.0) return false;
    if (config->preemphasis_coefficient < 0.0 || config->preemphasis_coefficient >= 1.0) return false;

    return true;
}

bool world_aperiodicity_config_validate(const WorldAperiodicityConfig* config) {
    if (!config) return false;

    if (config->threshold < 0.0 || config->threshold > 1.0) return false;
    if (config->frequency_bands <= 0 || config->frequency_bands > 20) return false;
    if (config->window_length <= 0.0 || config->window_length > 100.0) return false;
    if (config->noise_floor < -120.0 || config->noise_floor > 0.0) return false;

    return true;
}

bool world_synthesis_config_validate(const WorldSynthesisConfig* config) {
    if (!config) return false;

    if (config->sample_rate <= 0 || config->sample_rate > 192000) return false;
    if (config->frame_period <= 0.0 || config->frame_period > 50.0) return false;
    if (config->postfilter_coefficient < 0.0 || config->postfilter_coefficient > 1.0) return false;
    if (config->synthesis_speed <= 0.0 || config->synthesis_speed > 10.0) return false;
    if (config->overlap_length < 0 || config->overlap_length > 4096) return false;

    return true;
}

bool world_graph_optimization_config_validate(const WorldGraphOptimizationConfig* config) {
    if (!config) return false;

    if (config->max_thread_count < 0 || config->max_thread_count > 64) return false;
    if (config->optimization_level < 0.0 || config->optimization_level > 1.0) return false;
    if (config->memory_budget < 1024 * 1024) return false; // 최소 1MB

    return true;
}

bool world_memory_config_validate(const WorldMemoryConfig* config) {
    if (!config) return false;

    if (config->memory_pool_size < 1024 * 1024) return false; // 최소 1MB
    if (config->analysis_pool_size > config->memory_pool_size) return false;
    if (config->synthesis_pool_size > config->memory_pool_size) return false;
    if (config->cache_pool_size > config->memory_pool_size) return false;
    if (config->gc_threshold < 0.0 || config->gc_threshold > 1.0) return false;

    return true;
}

bool world_performance_config_validate(const WorldPerformanceConfig* config) {
    if (!config) return false;

    if (config->profiling_interval_ms <= 0 || config->profiling_interval_ms > 10000) return false;

    return true;
}

bool world_debug_config_validate(const WorldDebugConfig* config) {
    if (!config) return false;

    if (config->log_level < 0 || config->log_level > 5) return false;

    return true;
}

bool world_pipeline_config_validate(const WorldPipelineConfiguration* config) {
    if (!config) return false;

    // 각 카테고리별 검증
    if (!world_audio_config_validate(&config->audio)) return false;
    if (!world_f0_config_validate(&config->f0)) return false;
    if (!world_spectrum_config_validate(&config->spectrum)) return false;
    if (!world_aperiodicity_config_validate(&config->aperiodicity)) return false;
    if (!world_synthesis_config_validate(&config->synthesis)) return false;
    if (!world_graph_optimization_config_validate(&config->optimization)) return false;
    if (!world_memory_config_validate(&config->memory)) return false;
    if (!world_performance_config_validate(&config->performance)) return false;
    if (!world_debug_config_validate(&config->debug)) return false;

    // 크로스 카테고리 검증
    if (config->audio.sample_rate != config->synthesis.sample_rate) return false;
    if (config->f0.frame_period != config->synthesis.frame_period) return false;

    return true;
}

ETResult world_pipeline_config_normalize(WorldPipelineConfiguration* config) {
    if (!config) return ET_ERROR_INVALID_PARAMETER;

    // 오디오 설정 정규화
    if (config->audio.sample_rate <= 0) config->audio.sample_rate = 44100;
    if (config->audio.sample_rate > 192000) config->audio.sample_rate = 192000;
    if (config->audio.frame_size <= 0) config->audio.frame_size = 1024;
    if (config->audio.frame_size > 8192) config->audio.frame_size = 8192;
    if (config->audio.buffer_size < config->audio.frame_size) {
        config->audio.buffer_size = config->audio.frame_size * 4;
    }

    // FFT 크기를 2의 거듭제곱으로 조정
    if (!is_power_of_two(config->spectrum.fft_size)) {
        config->spectrum.fft_size = next_power_of_two(config->spectrum.fft_size);
    }

    // 주파수 빈 수 조정
    config->spectrum.frequency_bins = config->spectrum.fft_size / 2 + 1;

    // 샘플링 레이트 일관성 보장
    config->synthesis.sample_rate = config->audio.sample_rate;

    // 메모리 풀 크기 조정
    size_t total_sub_pools = config->memory.analysis_pool_size +
                            config->memory.synthesis_pool_size +
                            config->memory.cache_pool_size;
    if (total_sub_pools > config->memory.memory_pool_size) {
        config->memory.memory_pool_size = total_sub_pools * 1.2; // 20% 여유
    }

    config->modification_time = get_current_time();
    return ET_SUCCESS;
}

// =============================================================================
// 설정 파일 I/O 함수들
// =============================================================================

ETResult world_pipeline_config_save_to_file(const WorldPipelineConfiguration* config,
                                            const char* filename) {
    if (!config || !filename) return ET_ERROR_INVALID_PARAMETER;

    FILE* file = fopen(filename, "w");
    if (!file) return ET_ERROR_FILE_IO;

    // 간단한 텍스트 형식으로 저장
    fprintf(file, "# WORLD Pipeline Configuration\n");
    fprintf(file, "# Generated at %.3f\n\n", get_current_time());

    fprintf(file, "[Meta]\n");
    fprintf(file, "name=%s\n", config->config_name);
    fprintf(file, "version=%s\n", config->config_version);
    fprintf(file, "description=%s\n", config->description);
    fprintf(file, "creation_time=%.3f\n", config->creation_time);
    fprintf(file, "modification_time=%.3f\n\n", config->modification_time);

    fprintf(file, "[Audio]\n");
    fprintf(file, "sample_rate=%d\n", config->audio.sample_rate);
    fprintf(file, "frame_size=%d\n", config->audio.frame_size);
    fprintf(file, "buffer_size=%d\n", config->audio.buffer_size);
    fprintf(file, "bit_depth=%d\n", config->audio.bit_depth);
    fprintf(file, "channel_count=%d\n", config->audio.channel_count);
    fprintf(file, "enable_dithering=%s\n", config->audio.enable_dithering ? "true" : "false");
    fprintf(file, "input_gain=%.2f\n", config->audio.input_gain);
    fprintf(file, "output_gain=%.2f\n", config->audio.output_gain);
    fprintf(file, "enable_dc_removal=%s\n", config->audio.enable_dc_removal ? "true" : "false");
    fprintf(file, "enable_anti_aliasing=%s\n\n", config->audio.enable_anti_aliasing ? "true" : "false");

    fprintf(file, "[F0]\n");
    fprintf(file, "frame_period=%.2f\n", config->f0.frame_period);
    fprintf(file, "f0_floor=%.2f\n", config->f0.f0_floor);
    fprintf(file, "f0_ceil=%.2f\n", config->f0.f0_ceil);
    fprintf(file, "algorithm=%d\n", config->f0.algorithm);
    fprintf(file, "channels_in_octave=%.2f\n", config->f0.channels_in_octave);
    fprintf(file, "target_fs=%.2f\n", config->f0.target_fs);
    fprintf(file, "enable_refinement=%s\n", config->f0.enable_refinement ? "true" : "false");
    fprintf(file, "speed=%.2f\n", config->f0.speed);
    fprintf(file, "allow_range_extension=%s\n", config->f0.allow_range_extension ? "true" : "false");
    fprintf(file, "threshold=%.3f\n\n", config->f0.threshold);

    // 다른 섹션들도 유사하게 저장...

    fclose(file);
    return ET_SUCCESS;
}

ETResult world_pipeline_config_load_from_file(const char* filename,
                                              WorldPipelineConfiguration* config) {
    if (!filename || !config) return ET_ERROR_INVALID_PARAMETER;

    FILE* file = fopen(filename, "r");
    if (!file) return ET_ERROR_FILE_IO;

    // 기본 설정으로 초기화
    *config = world_pipeline_config_create_default();

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), file)) {
        trim_string(line);

        // 주석이나 빈 줄 건너뛰기
        if (line[0] == '#' || line[0] == '\0') continue;

        // 섹션 헤더 처리
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strcpy(section, line + 1);
            }
            continue;
        }

        // 키=값 쌍 처리
        char* equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        trim_string(key);
        trim_string(value);

        // 섹션별 파싱
        if (strcmp(section, "Meta") == 0) {
            if (strcmp(key, "name") == 0) {
                strncpy(config->config_name, value, sizeof(config->config_name) - 1);
            } else if (strcmp(key, "version") == 0) {
                strncpy(config->config_version, value, sizeof(config->config_version) - 1);
            } else if (strcmp(key, "description") == 0) {
                strncpy(config->description, value, sizeof(config->description) - 1);
            }
        } else if (strcmp(section, "Audio") == 0) {
            if (strcmp(key, "sample_rate") == 0) {
                config->audio.sample_rate = atoi(value);
            } else if (strcmp(key, "frame_size") == 0) {
                config->audio.frame_size = atoi(value);
            } else if (strcmp(key, "buffer_size") == 0) {
                config->audio.buffer_size = atoi(value);
            }
            // 다른 오디오 설정들...
        }
        // 다른 섹션들도 유사하게 처리...
    }

    fclose(file);

    // 설정 정규화 및 검증
    world_pipeline_config_normalize(config);
    if (!world_pipeline_config_validate(config)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return ET_SUCCESS;
}

// =============================================================================
// 설정 비교 및 병합 함수들
// =============================================================================

bool world_pipeline_config_compare(const WorldPipelineConfiguration* config1,
                                   const WorldPipelineConfiguration* config2) {
    if (!config1 || !config2) return false;

    // 메타 정보는 제외하고 실제 설정만 비교
    return (memcmp(&config1->audio, &config2->audio, sizeof(WorldAudioConfig)) == 0) &&
           (memcmp(&config1->f0, &config2->f0, sizeof(WorldF0Config)) == 0) &&
           (memcmp(&config1->spectrum, &config2->spectrum, sizeof(WorldSpectrumConfig)) == 0) &&
           (memcmp(&config1->aperiodicity, &config2->aperiodicity, sizeof(WorldAperiodicityConfig)) == 0) &&
           (memcmp(&config1->synthesis, &config2->synthesis, sizeof(WorldSynthesisConfig)) == 0) &&
           (memcmp(&config1->optimization, &config2->optimization, sizeof(WorldGraphOptimizationConfig)) == 0) &&
           (memcmp(&config1->memory, &config2->memory, sizeof(WorldMemoryConfig)) == 0) &&
           (memcmp(&config1->performance, &config2->performance, sizeof(WorldPerformanceConfig)) == 0) &&
           (memcmp(&config1->debug, &config2->debug, sizeof(WorldDebugConfig)) == 0);
}

ETResult world_pipeline_config_merge(WorldPipelineConfiguration* config1,
                                     const WorldPipelineConfiguration* config2) {
    if (!config1 || !config2) return ET_ERROR_INVALID_PARAMETER;

    // config2의 값으로 config1 업데이트 (메타 정보 제외)
    config1->audio = config2->audio;
    config1->f0 = config2->f0;
    config1->spectrum = config2->spectrum;
    config1->aperiodicity = config2->aperiodicity;
    config1->synthesis = config2->synthesis;
    config1->optimization = config2->optimization;
    config1->memory = config2->memory;
    config1->performance = config2->performance;
    config1->debug = config2->debug;

    config1->modification_time = get_current_time();

    return ET_SUCCESS;
}

// =============================================================================
// 유틸리티 함수들
// =============================================================================

void world_pipeline_config_print(const WorldPipelineConfiguration* config) {
    if (!config) {
        printf("Configuration: NULL\n");
        return;
    }

    printf("WORLD Pipeline Configuration\n");
    printf("============================\n");
    printf("Name: %s\n", config->config_name);
    printf("Version: %s\n", config->config_version);
    printf("Description: %s\n", config->description);
    printf("Created: %.3f\n", config->creation_time);
    printf("Modified: %.3f\n\n", config->modification_time);

    printf("Audio Configuration:\n");
    printf("  Sample Rate: %d Hz\n", config->audio.sample_rate);
    printf("  Frame Size: %d samples\n", config->audio.frame_size);
    printf("  Buffer Size: %d samples\n", config->audio.buffer_size);
    printf("  Bit Depth: %d bits\n", config->audio.bit_depth);
    printf("  Channels: %d\n", config->audio.channel_count);
    printf("  Input Gain: %.2f dB\n", config->audio.input_gain);
    printf("  Output Gain: %.2f dB\n\n", config->audio.output_gain);

    printf("F0 Configuration:\n");
    printf("  Frame Period: %.2f ms\n", config->f0.frame_period);
    printf("  F0 Range: %.2f - %.2f Hz\n", config->f0.f0_floor, config->f0.f0_ceil);
    printf("  Algorithm: %s\n", config->f0.algorithm == 0 ? "DIO" : "Harvest");
    printf("  Speed: %.2fx\n", config->f0.speed);
    printf("  Refinement: %s\n\n", config->f0.enable_refinement ? "Enabled" : "Disabled");

    printf("Spectrum Configuration:\n");
    printf("  FFT Size: %d\n", config->spectrum.fft_size);
    printf("  Q1 Parameter: %.3f\n", config->spectrum.q1);
    printf("  Frequency Bins: %d\n", config->spectrum.frequency_bins);
    printf("  Spectral Smoothing: %s\n\n", config->spectrum.enable_spectral_smoothing ? "Enabled" : "Disabled");

    printf("Memory Configuration:\n");
    printf("  Total Pool Size: %.2f MB\n", config->memory.memory_pool_size / (1024.0 * 1024.0));
    printf("  Analysis Pool: %.2f MB\n", config->memory.analysis_pool_size / (1024.0 * 1024.0));
    printf("  Synthesis Pool: %.2f MB\n", config->memory.synthesis_pool_size / (1024.0 * 1024.0));
    printf("  Cache Pool: %.2f MB\n", config->memory.cache_pool_size / (1024.0 * 1024.0));
    printf("  Memory Tracking: %s\n\n", config->memory.enable_memory_tracking ? "Enabled" : "Disabled");

    printf("Optimization Configuration:\n");
    printf("  Node Fusion: %s\n", config->optimization.enable_node_fusion ? "Enabled" : "Disabled");
    printf("  Memory Reuse: %s\n", config->optimization.enable_memory_reuse ? "Enabled" : "Disabled");
    printf("  SIMD Optimization: %s\n", config->optimization.enable_simd_optimization ? "Enabled" : "Disabled");
    printf("  Parallel Execution: %s\n", config->optimization.enable_parallel_execution ? "Enabled" : "Disabled");
    printf("  Max Threads: %d\n", config->optimization.max_thread_count);
    printf("  Optimization Level: %.1f\n\n", config->optimization.optimization_level);
}

void world_pipeline_config_print_summary(const WorldPipelineConfiguration* config) {
    if (!config) {
        printf("Configuration: NULL\n");
        return;
    }

    printf("Config: %s (v%s)\n", config->config_name, config->config_version);
    printf("Audio: %dHz, %d samples, %d-bit\n",
           config->audio.sample_rate, config->audio.frame_size, config->audio.bit_depth);
    printf("F0: %.1fms, %s, %.1fx speed\n",
           config->f0.frame_period,
           config->f0.algorithm == 0 ? "DIO" : "Harvest",
           config->f0.speed);
    printf("Memory: %.1fMB total\n", config->memory.memory_pool_size / (1024.0 * 1024.0));
    printf("Threads: %d\n", config->optimization.max_thread_count);
}

const char* world_config_preset_get_name(WorldConfigPreset preset) {
    switch (preset) {
        case WORLD_CONFIG_PRESET_DEFAULT: return "Default";
        case WORLD_CONFIG_PRESET_HIGH_QUALITY: return "High Quality";
        case WORLD_CONFIG_PRESET_FAST: return "Fast Processing";
        case WORLD_CONFIG_PRESET_LOW_LATENCY: return "Low Latency";
        case WORLD_CONFIG_PRESET_LOW_MEMORY: return "Low Memory";
        case WORLD_CONFIG_PRESET_REALTIME: return "Realtime";
        case WORLD_CONFIG_PRESET_BATCH: return "Batch Processing";
        case WORLD_CONFIG_PRESET_DEBUG: return "Debug";
        case WORLD_CONFIG_PRESET_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* world_config_preset_get_description(WorldConfigPreset preset) {
    switch (preset) {
        case WORLD_CONFIG_PRESET_DEFAULT: return "균형잡힌 기본 설정";
        case WORLD_CONFIG_PRESET_HIGH_QUALITY: return "최고 품질의 음성 합성";
        case WORLD_CONFIG_PRESET_FAST: return "빠른 처리 속도 우선";
        case WORLD_CONFIG_PRESET_LOW_LATENCY: return "실시간 저지연 처리";
        case WORLD_CONFIG_PRESET_LOW_MEMORY: return "메모리 사용량 최소화";
        case WORLD_CONFIG_PRESET_REALTIME: return "실시간 스트리밍 최적화";
        case WORLD_CONFIG_PRESET_BATCH: return "대용량 배치 처리";
        case WORLD_CONFIG_PRESET_DEBUG: return "디버깅 및 개발용";
        case WORLD_CONFIG_PRESET_CUSTOM: return "사용자 정의 설정";
        default: return "알 수 없는 프리셋";
    }
}

uint64_t world_pipeline_config_hash(const WorldPipelineConfiguration* config) {
    if (!config) return 0;

    // 간단한 해시 계산 (실제로는 더 정교한 해시 함수 사용 권장)
    uint64_t hash = 0;
    const char* data = (const char*)config;
    size_t size = sizeof(WorldPipelineConfiguration);

    for (size_t i = 0; i < size; i++) {
        hash = hash * 31 + data[i];
    }

    return hash;
}

// =============================================================================
// 내부 헬퍼 함수 구현
// =============================================================================

static double get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

static bool is_power_of_two(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

static int next_power_of_two(int value) {
    if (value <= 0) return 1;

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;

    return value;
}

static void trim_string(char* str) {
    if (!str) return;

    // 앞쪽 공백 제거
    char* start = str;
    while (isspace(*start)) start++;

    // 뒤쪽 공백 제거
    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;

    // 문자열 이동 및 종료
    size_t len = end - start + 1;
    memmove(str, start, len);
    str[len] = '\0';
}