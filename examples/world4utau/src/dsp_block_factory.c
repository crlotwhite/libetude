/**
 * @file dsp_block_factory.c
 * @brief DSP 블록 팩토리 구현
 */

#include "dsp_block_factory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// 내부 함수 선언
static void set_factory_error(DSPBlockFactory* factory, const char* error_msg);
static int calculate_frame_count(int audio_length, double frame_period_ms, int sample_rate);
static int calculate_frame_size(double frame_period_ms, int sample_rate);
static int calculate_buffer_size(double max_duration_sec, int sample_rate);

// =============================================================================
// DSP 블록 팩토리 기본 함수들
// =============================================================================

DSPBlockFactory* dsp_block_factory_create(ETMemoryPool* mem_pool) {
    if (!mem_pool) {
        return NULL;
    }

    // 팩토리 메모리 할당
    DSPBlockFactory* factory = (DSPBlockFactory*)et_memory_pool_alloc(mem_pool, sizeof(DSPBlockFactory));
    if (!factory) {
        return NULL;
    }

    memset(factory, 0, sizeof(DSPBlockFactory));

    // 기본 설정
    factory->mem_pool = mem_pool;
    factory->blocks_created = 0;
    factory->blocks_destroyed = 0;
    factory->last_error[0] = '\0';

    return factory;
}

void dsp_block_factory_destroy(DSPBlockFactory* factory) {
    if (!factory) {
        return;
    }

    et_memory_pool_free(factory->mem_pool, factory);
}

ETResult dsp_block_factory_initialize_defaults(DSPBlockFactory* factory,
                                              int sample_rate,
                                              double frame_period_ms,
                                              int fft_size) {
    if (!factory || sample_rate <= 0 || frame_period_ms <= 0 || fft_size <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // F0 추출 기본 설정
    factory->default_f0_config.sample_rate = sample_rate;
    factory->default_f0_config.frame_period = frame_period_ms;
    factory->default_f0_config.f0_floor = 71.0;
    factory->default_f0_config.f0_ceil = 800.0;
    factory->default_f0_config.algorithm = 0; // DIO
    factory->default_f0_config.audio_length = 0; // 런타임에 설정

    // 스펙트럼 분석 기본 설정
    factory->default_spectrum_config.sample_rate = sample_rate;
    factory->default_spectrum_config.frame_period = frame_period_ms;
    factory->default_spectrum_config.fft_size = fft_size;
    factory->default_spectrum_config.q1 = -0.15;
    factory->default_spectrum_config.audio_length = 0; // 런타임에 설정

    // 비주기성 분석 기본 설정
    factory->default_aperiodicity_config.sample_rate = sample_rate;
    factory->default_aperiodicity_config.frame_period = frame_period_ms;
    factory->default_aperiodicity_config.fft_size = fft_size;
    factory->default_aperiodicity_config.threshold = 0.85;
    factory->default_aperiodicity_config.audio_length = 0; // 런타임에 설정

    // 합성 기본 설정
    factory->default_synthesis_config.sample_rate = sample_rate;
    factory->default_synthesis_config.frame_period = frame_period_ms;
    factory->default_synthesis_config.enable_postfilter = true;
    factory->default_synthesis_config.max_duration_sec = 10.0;

    return ET_SUCCESS;
}

const char* dsp_block_factory_get_error(DSPBlockFactory* factory) {
    if (!factory) {
        return "Invalid factory";
    }

    return factory->last_error;
}

void dsp_block_factory_print_stats(DSPBlockFactory* factory) {
    if (!factory) {
        printf("Invalid factory\n");
        return;
    }

    printf("=== DSP Block Factory Statistics ===\n");
    printf("Blocks Created: %d\n", factory->blocks_created);
    printf("Blocks Destroyed: %d\n", factory->blocks_destroyed);
    printf("Active Blocks: %d\n", factory->blocks_created - factory->blocks_destroyed);

    if (factory->last_error[0] != '\0') {
        printf("Last Error: %s\n", factory->last_error);
    }

    printf("====================================\n");
}

// =============================================================================
// 블록 생성 함수들
// =============================================================================

DSPBlock* dsp_block_factory_create_block(DSPBlockFactory* factory,
                                         const DSPBlockConfig* config) {
    if (!factory || !config) {
        set_factory_error(factory, "Invalid parameters");
        return NULL;
    }

    DSPBlock* block = NULL;

    switch (config->block_type) {
        case DSP_BLOCK_TYPE_AUDIO_INPUT:
            block = dsp_block_factory_create_audio_input(factory, &config->config.audio_input);
            break;

        case DSP_BLOCK_TYPE_F0_EXTRACTION:
            block = dsp_block_factory_create_f0_extraction(factory, &config->config.f0_extraction);
            break;

        case DSP_BLOCK_TYPE_SPECTRUM_ANALYSIS:
            block = dsp_block_factory_create_spectrum_analysis(factory, &config->config.spectrum_analysis);
            break;

        case DSP_BLOCK_TYPE_APERIODICITY_ANALYSIS:
            block = dsp_block_factory_create_aperiodicity_analysis(factory, &config->config.aperiodicity_analysis);
            break;

        case DSP_BLOCK_TYPE_PARAMETER_MERGE:
            block = dsp_block_factory_create_parameter_merge(factory, &config->config.parameter_merge);
            break;

        case DSP_BLOCK_TYPE_SYNTHESIS:
            block = dsp_block_factory_create_synthesis(factory, &config->config.synthesis);
            break;

        case DSP_BLOCK_TYPE_AUDIO_OUTPUT:
            block = dsp_block_factory_create_audio_output(factory, &config->config.audio_output);
            break;

        default:
            set_factory_error(factory, "Unknown block type");
            return NULL;
    }

    if (block) {
        factory->blocks_created++;
    }

    return block;
}

DSPBlock* dsp_block_factory_create_audio_input(DSPBlockFactory* factory,
                                               const AudioInputBlockConfig* config) {
    if (!factory || !config || !config->audio_buffer) {
        set_factory_error(factory, "Invalid audio input config");
        return NULL;
    }

    int frame_size = config->frame_size;

    // 프레임 크기 자동 계산
    if (config->auto_calculate_frame_size) {
        frame_size = calculate_frame_size(config->frame_period_ms, config->sample_rate);
    }

    DSPBlock* block = create_audio_input_block(config->name, config->audio_buffer,
                                              config->audio_length, config->sample_rate,
                                              frame_size, factory->mem_pool);

    if (!block) {
        set_factory_error(factory, "Failed to create audio input block");
    }

    return block;
}

DSPBlock* dsp_block_factory_create_f0_extraction(DSPBlockFactory* factory,
                                                 const F0ExtractionBlockConfig* config) {
    if (!factory || !config) {
        set_factory_error(factory, "Invalid F0 extraction config");
        return NULL;
    }

    F0ExtractionConfig f0_config;

    if (config->use_default_config) {
        memcpy(&f0_config, &factory->default_f0_config, sizeof(F0ExtractionConfig));
    } else {
        memcpy(&f0_config, &config->f0_config, sizeof(F0ExtractionConfig));
    }

    DSPBlock* block = create_f0_extraction_block(config->name, &f0_config, factory->mem_pool);

    if (!block) {
        set_factory_error(factory, "Failed to create F0 extraction block");
    }

    return block;
}

DSPBlock* dsp_block_factory_create_spectrum_analysis(DSPBlockFactory* factory,
                                                    const SpectrumAnalysisBlockConfig* config) {
    if (!factory || !config) {
        set_factory_error(factory, "Invalid spectrum analysis config");
        return NULL;
    }

    SpectrumConfig spectrum_config;

    if (config->use_default_config) {
        memcpy(&spectrum_config, &factory->default_spectrum_config, sizeof(SpectrumConfig));
    } else {
        memcpy(&spectrum_config, &config->spectrum_config, sizeof(SpectrumConfig));
    }

    DSPBlock* block = create_spectrum_analysis_block(config->name, &spectrum_config, factory->mem_pool);

    if (!block) {
        set_factory_error(factory, "Failed to create spectrum analysis block");
    }

    return block;
}

DSPBlock* dsp_block_factory_create_aperiodicity_analysis(DSPBlockFactory* factory,
                                                        const AperiodicityAnalysisBlockConfig* config) {
    if (!factory || !config) {
        set_factory_error(factory, "Invalid aperiodicity analysis config");
        return NULL;
    }

    AperiodicityConfig aperiodicity_config;

    if (config->use_default_config) {
        memcpy(&aperiodicity_config, &factory->default_aperiodicity_config, sizeof(AperiodicityConfig));
    } else {
        memcpy(&aperiodicity_config, &config->aperiodicity_config, sizeof(AperiodicityConfig));
    }

    DSPBlock* block = create_aperiodicity_analysis_block(config->name, &aperiodicity_config, factory->mem_pool);

    if (!block) {
        set_factory_error(factory, "Failed to create aperiodicity analysis block");
    }

    return block;
}

DSPBlock* dsp_block_factory_create_parameter_merge(DSPBlockFactory* factory,
                                                   const ParameterMergeBlockConfig* config) {
    if (!factory || !config) {
        set_factory_error(factory, "Invalid parameter merge config");
        return NULL;
    }

    int frame_count = config->frame_count;

    // 프레임 수 자동 계산
    if (config->auto_calculate_frame_count) {
        frame_count = calculate_frame_count(config->audio_length, config->frame_period_ms, config->sample_rate);
    }

    DSPBlock* block = create_parameter_merge_block(config->name, frame_count, config->fft_size, factory->mem_pool);

    if (!block) {
        set_factory_error(factory, "Failed to create parameter merge block");
    }

    return block;
}

DSPBlock* dsp_block_factory_create_synthesis(DSPBlockFactory* factory,
                                            const SynthesisBlockConfig* config) {
    if (!factory || !config) {
        set_factory_error(factory, "Invalid synthesis config");
        return NULL;
    }

    SynthesisConfig synthesis_config;

    if (config->use_default_config) {
        memcpy(&synthesis_config, &factory->default_synthesis_config, sizeof(SynthesisConfig));
    } else {
        memcpy(&synthesis_config, &config->synthesis_config, sizeof(SynthesisConfig));
    }

    DSPBlock* block = create_synthesis_block(config->name, &synthesis_config, factory->mem_pool);

    if (!block) {
        set_factory_error(factory, "Failed to create synthesis block");
    }

    return block;
}

DSPBlock* dsp_block_factory_create_audio_output(DSPBlockFactory* factory,
                                                const AudioOutputBlockConfig* config) {
    if (!factory || !config) {
        set_factory_error(factory, "Invalid audio output config");
        return NULL;
    }

    int buffer_size = config->buffer_size;

    // 버퍼 크기 자동 계산
    if (config->auto_calculate_buffer_size) {
        buffer_size = calculate_buffer_size(config->max_duration_sec, config->sample_rate);
    }

    const char* output_filename = config->enable_file_output ? config->output_filename : NULL;

    DSPBlock* block = create_audio_output_block(config->name, buffer_size, config->sample_rate,
                                               output_filename, factory->mem_pool);

    if (!block) {
        set_factory_error(factory, "Failed to create audio output block");
    }

    return block;
}

void dsp_block_factory_destroy_block(DSPBlockFactory* factory, DSPBlock* block) {
    if (!factory || !block) {
        return;
    }

    dsp_block_destroy(block);
    factory->blocks_destroyed++;
}

// =============================================================================
// 설정 헬퍼 함수들
// =============================================================================

void dsp_block_factory_init_audio_input_config(AudioInputBlockConfig* config,
                                               const char* name,
                                               const float* audio_buffer,
                                               int audio_length,
                                               int sample_rate) {
    if (!config || !name || !audio_buffer) {
        return;
    }

    memset(config, 0, sizeof(AudioInputBlockConfig));

    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->audio_buffer = audio_buffer;
    config->audio_length = audio_length;
    config->sample_rate = sample_rate;
    config->frame_size = 1024; // 기본값
    config->auto_calculate_frame_size = false;
    config->frame_period_ms = 5.0; // 기본값
}

void dsp_block_factory_init_f0_extraction_config(F0ExtractionBlockConfig* config,
                                                 const char* name,
                                                 bool use_default) {
    if (!config || !name) {
        return;
    }

    memset(config, 0, sizeof(F0ExtractionBlockConfig));

    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->use_default_config = use_default;

    if (!use_default) {
        // 기본 F0 설정 초기화
        config->f0_config.sample_rate = 44100;
        config->f0_config.frame_period = 5.0;
        config->f0_config.f0_floor = 71.0;
        config->f0_config.f0_ceil = 800.0;
        config->f0_config.algorithm = 0; // DIO
        config->f0_config.audio_length = 0;
    }
}

void dsp_block_factory_init_spectrum_analysis_config(SpectrumAnalysisBlockConfig* config,
                                                     const char* name,
                                                     bool use_default) {
    if (!config || !name) {
        return;
    }

    memset(config, 0, sizeof(SpectrumAnalysisBlockConfig));

    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->use_default_config = use_default;

    if (!use_default) {
        // 기본 스펙트럼 설정 초기화
        config->spectrum_config.sample_rate = 44100;
        config->spectrum_config.frame_period = 5.0;
        config->spectrum_config.fft_size = 2048;
        config->spectrum_config.q1 = -0.15;
        config->spectrum_config.audio_length = 0;
    }
}

void dsp_block_factory_init_aperiodicity_analysis_config(AperiodicityAnalysisBlockConfig* config,
                                                         const char* name,
                                                         bool use_default) {
    if (!config || !name) {
        return;
    }

    memset(config, 0, sizeof(AperiodicityAnalysisBlockConfig));

    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->use_default_config = use_default;

    if (!use_default) {
        // 기본 비주기성 설정 초기화
        config->aperiodicity_config.sample_rate = 44100;
        config->aperiodicity_config.frame_period = 5.0;
        config->aperiodicity_config.fft_size = 2048;
        config->aperiodicity_config.threshold = 0.85;
        config->aperiodicity_config.audio_length = 0;
    }
}

void dsp_block_factory_init_parameter_merge_config(ParameterMergeBlockConfig* config,
                                                   const char* name,
                                                   int frame_count,
                                                   int fft_size) {
    if (!config || !name) {
        return;
    }

    memset(config, 0, sizeof(ParameterMergeBlockConfig));

    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->frame_count = frame_count;
    config->fft_size = fft_size;
    config->auto_calculate_frame_count = false;
    config->audio_length = 0;
    config->frame_period_ms = 5.0;
    config->sample_rate = 44100;
}

void dsp_block_factory_init_synthesis_config(SynthesisBlockConfig* config,
                                             const char* name,
                                             bool use_default) {
    if (!config || !name) {
        return;
    }

    memset(config, 0, sizeof(SynthesisBlockConfig));

    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->use_default_config = use_default;

    if (!use_default) {
        // 기본 합성 설정 초기화
        config->synthesis_config.sample_rate = 44100;
        config->synthesis_config.frame_period = 5.0;
        config->synthesis_config.enable_postfilter = true;
        config->synthesis_config.max_duration_sec = 10.0;
    }
}

void dsp_block_factory_init_audio_output_config(AudioOutputBlockConfig* config,
                                                const char* name,
                                                int sample_rate,
                                                const char* output_filename) {
    if (!config || !name) {
        return;
    }

    memset(config, 0, sizeof(AudioOutputBlockConfig));

    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->buffer_size = sample_rate * 10; // 10초 기본값
    config->sample_rate = sample_rate;
    config->enable_file_output = (output_filename != NULL);
    config->auto_calculate_buffer_size = false;
    config->max_duration_sec = 10.0;

    if (output_filename) {
        strncpy(config->output_filename, output_filename, sizeof(config->output_filename) - 1);
        config->output_filename[sizeof(config->output_filename) - 1] = '\0';
    } else {
        config->output_filename[0] = '\0';
    }
}

// =============================================================================
// 배치 생성 함수들
// =============================================================================

int dsp_block_factory_create_blocks_batch(DSPBlockFactory* factory,
                                          const DSPBlockConfig* configs,
                                          int config_count,
                                          DSPBlock** blocks,
                                          int max_blocks) {
    if (!factory || !configs || !blocks || config_count <= 0 || max_blocks <= 0) {
        set_factory_error(factory, "Invalid batch creation parameters");
        return -1;
    }

    int created_count = 0;

    for (int i = 0; i < config_count && created_count < max_blocks; i++) {
        DSPBlock* block = dsp_block_factory_create_block(factory, &configs[i]);
        if (block) {
            blocks[created_count] = block;
            created_count++;
        } else {
            // 실패한 경우 이미 생성된 블록들 정리
            for (int j = 0; j < created_count; j++) {
                dsp_block_factory_destroy_block(factory, blocks[j]);
                blocks[j] = NULL;
            }
            return -1;
        }
    }

    return created_count;
}

void dsp_block_factory_destroy_blocks_batch(DSPBlockFactory* factory,
                                            DSPBlock** blocks,
                                            int block_count) {
    if (!factory || !blocks || block_count <= 0) {
        return;
    }

    for (int i = 0; i < block_count; i++) {
        if (blocks[i]) {
            dsp_block_factory_destroy_block(factory, blocks[i]);
            blocks[i] = NULL;
        }
    }
}

// =============================================================================
// 설정 파일 지원 함수들
// =============================================================================

int dsp_block_factory_load_config_from_json(DSPBlockFactory* factory,
                                            const char* config_filename,
                                            DSPBlockConfig* configs,
                                            int max_configs) {
    if (!factory || !config_filename || !configs || max_configs <= 0) {
        set_factory_error(factory, "Invalid JSON load parameters");
        return -1;
    }

    // JSON 파싱 구현 (현재는 스텁)
    // 실제 구현에서는 JSON 라이브러리를 사용하여 파일을 파싱하고
    // DSPBlockConfig 구조체들을 채워야 함

    set_factory_error(factory, "JSON loading not implemented yet");
    return -1;
}

ETResult dsp_block_factory_save_config_to_json(DSPBlockFactory* factory,
                                               const char* config_filename,
                                               const DSPBlockConfig* configs,
                                               int config_count) {
    if (!factory || !config_filename || !configs || config_count <= 0) {
        set_factory_error(factory, "Invalid JSON save parameters");
        return ET_ERROR_INVALID_PARAMETER;
    }

    // JSON 직렬화 구현 (현재는 스텁)
    // 실제 구현에서는 DSPBlockConfig 구조체들을 JSON 형식으로 직렬화하여
    // 파일에 저장해야 함

    set_factory_error(factory, "JSON saving not implemented yet");
    return ET_ERROR_NOT_IMPLEMENTED;
}

// =============================================================================
// 내부 함수들
// =============================================================================

static void set_factory_error(DSPBlockFactory* factory, const char* error_msg) {
    if (!factory || !error_msg) {
        return;
    }

    strncpy(factory->last_error, error_msg, sizeof(factory->last_error) - 1);
    factory->last_error[sizeof(factory->last_error) - 1] = '\0';
}

static int calculate_frame_count(int audio_length, double frame_period_ms, int sample_rate) {
    if (audio_length <= 0 || frame_period_ms <= 0 || sample_rate <= 0) {
        return 0;
    }

    return (int)(audio_length / (frame_period_ms * sample_rate / 1000.0)) + 1;
}

static int calculate_frame_size(double frame_period_ms, int sample_rate) {
    if (frame_period_ms <= 0 || sample_rate <= 0) {
        return 1024; // 기본값
    }

    return (int)(frame_period_ms * sample_rate / 1000.0);
}

static int calculate_buffer_size(double max_duration_sec, int sample_rate) {
    if (max_duration_sec <= 0 || sample_rate <= 0) {
        return sample_rate * 10; // 10초 기본값
    }

    return (int)(max_duration_sec * sample_rate);
}