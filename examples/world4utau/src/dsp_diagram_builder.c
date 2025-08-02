/**
 * @file dsp_diagram_builder.c
 * @brief DSP 블록 다이어그램 빌더 구현
 */

#include "dsp_diagram_builder.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// 내부 함수 선언
static void set_builder_error(DSPDiagramBuilder* builder, const char* error_msg);
static int calculate_frame_count(int audio_length, double frame_period, int sample_rate);

// =============================================================================
// DSP 다이어그램 빌더 기본 함수들
// =============================================================================

DSPDiagramBuilder* dsp_diagram_builder_create(const char* name, int max_blocks,
                                             int max_connections, ETMemoryPool* mem_pool) {
    if (!name || max_blocks <= 0 || max_connections <= 0 || !mem_pool) {
        return NULL;
    }

    // 빌더 메모리 할당
    DSPDiagramBuilder* builder = (DSPDiagramBuilder*)et_memory_pool_alloc(mem_pool, sizeof(DSPDiagramBuilder));
    if (!builder) {
        return NULL;
    }

    memset(builder, 0, sizeof(DSPDiagramBuilder));

    // 다이어그램 생성
    builder->diagram = dsp_block_diagram_create(name, max_blocks, max_connections, mem_pool);
    if (!builder->diagram) {
        et_memory_pool_free(mem_pool, builder);
        return NULL;
    }

    // 빌더 초기화
    builder->mem_pool = mem_pool;
    builder->is_building = false;

    // 블록 ID 초기화 (0은 무효한 ID)
    builder->audio_input_block_id = 0;
    builder->f0_extraction_block_id = 0;
    builder->spectrum_analysis_block_id = 0;
    builder->aperiodicity_analysis_block_id = 0;
    builder->parameter_merge_block_id = 0;
    builder->synthesis_block_id = 0;
    builder->audio_output_block_id = 0;

    return builder;
}

void dsp_diagram_builder_destroy(DSPDiagramBuilder* builder) {
    if (!builder) {
        return;
    }

    if (builder->diagram) {
        dsp_block_diagram_destroy(builder->diagram);
    }

    et_memory_pool_free(builder->mem_pool, builder);
}

ETResult dsp_diagram_builder_begin(DSPDiagramBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (builder->is_building) {
        set_builder_error(builder, "Builder is already in building state");
        return ET_ERROR_INVALID_STATE;
    }

    builder->is_building = true;
    builder->error_message[0] = '\0';

    return ET_SUCCESS;
}

DSPBlockDiagram* dsp_diagram_builder_finish(DSPDiagramBuilder* builder) {
    if (!builder || !builder->is_building) {
        return NULL;
    }

    // 다이어그램 검증
    if (!dsp_diagram_builder_validate_connections(builder)) {
        set_builder_error(builder, "Connection validation failed");
        return NULL;
    }

    if (!dsp_diagram_builder_validate_data_flow(builder)) {
        set_builder_error(builder, "Data flow validation failed");
        return NULL;
    }

    // 다이어그램 빌드
    ETResult result = dsp_block_diagram_build(builder->diagram);
    if (result != ET_SUCCESS) {
        set_builder_error(builder, "Diagram build failed");
        return NULL;
    }

    builder->is_building = false;
    return builder->diagram;
}

const char* dsp_diagram_builder_get_error(DSPDiagramBuilder* builder) {
    if (!builder) {
        return "Invalid builder";
    }

    return builder->error_message;
}

// =============================================================================
// 블록 추가 함수들
// =============================================================================

ETResult dsp_diagram_builder_add_audio_input(DSPDiagramBuilder* builder,
                                            const char* name,
                                            const float* audio_buffer,
                                            int audio_length,
                                            int sample_rate,
                                            int frame_size) {
    if (!builder || !builder->is_building || !name || !audio_buffer) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 오디오 입력 블록 생성
    DSPBlock* block = create_audio_input_block(name, audio_buffer, audio_length,
                                              sample_rate, frame_size, builder->mem_pool);
    if (!block) {
        set_builder_error(builder, "Failed to create audio input block");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다이어그램에 추가
    ETResult result = dsp_block_diagram_add_block(builder->diagram, block);
    if (result != ET_SUCCESS) {
        dsp_block_destroy(block);
        set_builder_error(builder, "Failed to add audio input block to diagram");
        return result;
    }

    // 블록 ID 저장
    builder->audio_input_block_id = block->block_id;
    builder->sample_rate = sample_rate;
    builder->audio_length = audio_length;

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_add_f0_extraction(DSPDiagramBuilder* builder,
                                              const char* name,
                                              const F0ExtractionConfig* config) {
    if (!builder || !builder->is_building || !name || !config) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // F0 추출 블록 생성
    DSPBlock* block = create_f0_extraction_block(name, config, builder->mem_pool);
    if (!block) {
        set_builder_error(builder, "Failed to create F0 extraction block");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다이어그램에 추가
    ETResult result = dsp_block_diagram_add_block(builder->diagram, block);
    if (result != ET_SUCCESS) {
        dsp_block_destroy(block);
        set_builder_error(builder, "Failed to add F0 extraction block to diagram");
        return result;
    }

    // 블록 ID 저장
    builder->f0_extraction_block_id = block->block_id;
    builder->frame_period = config->frame_period;

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_add_spectrum_analysis(DSPDiagramBuilder* builder,
                                                  const char* name,
                                                  const SpectrumConfig* config) {
    if (!builder || !builder->is_building || !name || !config) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 스펙트럼 분석 블록 생성
    DSPBlock* block = create_spectrum_analysis_block(name, config, builder->mem_pool);
    if (!block) {
        set_builder_error(builder, "Failed to create spectrum analysis block");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다이어그램에 추가
    ETResult result = dsp_block_diagram_add_block(builder->diagram, block);
    if (result != ET_SUCCESS) {
        dsp_block_destroy(block);
        set_builder_error(builder, "Failed to add spectrum analysis block to diagram");
        return result;
    }

    // 블록 ID 저장
    builder->spectrum_analysis_block_id = block->block_id;
    builder->fft_size = config->fft_size;

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_add_aperiodicity_analysis(DSPDiagramBuilder* builder,
                                                      const char* name,
                                                      const AperiodicityConfig* config) {
    if (!builder || !builder->is_building || !name || !config) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 비주기성 분석 블록 생성
    DSPBlock* block = create_aperiodicity_analysis_block(name, config, builder->mem_pool);
    if (!block) {
        set_builder_error(builder, "Failed to create aperiodicity analysis block");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다이어그램에 추가
    ETResult result = dsp_block_diagram_add_block(builder->diagram, block);
    if (result != ET_SUCCESS) {
        dsp_block_destroy(block);
        set_builder_error(builder, "Failed to add aperiodicity analysis block to diagram");
        return result;
    }

    // 블록 ID 저장
    builder->aperiodicity_analysis_block_id = block->block_id;

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_add_parameter_merge(DSPDiagramBuilder* builder,
                                                const char* name,
                                                int frame_count,
                                                int fft_size) {
    if (!builder || !builder->is_building || !name || frame_count <= 0 || fft_size <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 파라미터 병합 블록 생성
    DSPBlock* block = create_parameter_merge_block(name, frame_count, fft_size, builder->mem_pool);
    if (!block) {
        set_builder_error(builder, "Failed to create parameter merge block");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다이어그램에 추가
    ETResult result = dsp_block_diagram_add_block(builder->diagram, block);
    if (result != ET_SUCCESS) {
        dsp_block_destroy(block);
        set_builder_error(builder, "Failed to add parameter merge block to diagram");
        return result;
    }

    // 블록 ID 저장
    builder->parameter_merge_block_id = block->block_id;

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_add_synthesis(DSPDiagramBuilder* builder,
                                          const char* name,
                                          const SynthesisConfig* config) {
    if (!builder || !builder->is_building || !name || !config) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 합성 블록 생성
    DSPBlock* block = create_synthesis_block(name, config, builder->mem_pool);
    if (!block) {
        set_builder_error(builder, "Failed to create synthesis block");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다이어그램에 추가
    ETResult result = dsp_block_diagram_add_block(builder->diagram, block);
    if (result != ET_SUCCESS) {
        dsp_block_destroy(block);
        set_builder_error(builder, "Failed to add synthesis block to diagram");
        return result;
    }

    // 블록 ID 저장
    builder->synthesis_block_id = block->block_id;

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_add_audio_output(DSPDiagramBuilder* builder,
                                             const char* name,
                                             int buffer_size,
                                             int sample_rate,
                                             const char* output_filename) {
    if (!builder || !builder->is_building || !name || buffer_size <= 0 || sample_rate <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 오디오 출력 블록 생성
    DSPBlock* block = create_audio_output_block(name, buffer_size, sample_rate,
                                               output_filename, builder->mem_pool);
    if (!block) {
        set_builder_error(builder, "Failed to create audio output block");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 다이어그램에 추가
    ETResult result = dsp_block_diagram_add_block(builder->diagram, block);
    if (result != ET_SUCCESS) {
        dsp_block_destroy(block);
        set_builder_error(builder, "Failed to add audio output block to diagram");
        return result;
    }

    // 블록 ID 저장
    builder->audio_output_block_id = block->block_id;

    return ET_SUCCESS;
}

// =============================================================================
// 연결 함수들
// =============================================================================

ETResult dsp_diagram_builder_connect_by_name(DSPDiagramBuilder* builder,
                                            const char* source_block_name,
                                            int source_port_index,
                                            const char* dest_block_name,
                                            int dest_port_index) {
    if (!builder || !builder->is_building || !source_block_name || !dest_block_name) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 블록 이름으로 블록 찾기
    DSPBlock* source_block = dsp_block_diagram_find_block_by_name(builder->diagram, source_block_name);
    DSPBlock* dest_block = dsp_block_diagram_find_block_by_name(builder->diagram, dest_block_name);

    if (!source_block || !dest_block) {
        set_builder_error(builder, "Block not found for connection");
        return ET_ERROR_NOT_FOUND;
    }

    // 블록 ID로 연결
    return dsp_diagram_builder_connect_by_id(builder, source_block->block_id, source_port_index,
                                            dest_block->block_id, dest_port_index);
}

ETResult dsp_diagram_builder_connect_by_id(DSPDiagramBuilder* builder,
                                          int source_block_id,
                                          int source_port_index,
                                          int dest_block_id,
                                          int dest_port_index) {
    if (!builder || !builder->is_building) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 다이어그램에서 연결 생성
    ETResult result = dsp_block_diagram_connect(builder->diagram, source_block_id, source_port_index,
                                               dest_block_id, dest_port_index);
    if (result != ET_SUCCESS) {
        set_builder_error(builder, "Failed to create connection");
        return result;
    }

    return ET_SUCCESS;
}

// =============================================================================
// 고수준 빌더 함수들
// =============================================================================

ETResult dsp_diagram_builder_build_world_analysis_pipeline(DSPDiagramBuilder* builder,
                                                          const float* audio_buffer,
                                                          int audio_length,
                                                          const WorldPipelineConfig* config) {
    if (!builder || !audio_buffer || !config) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETResult result;
    int frame_count = calculate_frame_count(audio_length, config->frame_period, config->sample_rate);
    int frame_size = (int)(config->frame_period * config->sample_rate / 1000.0);

    // 1. 오디오 입력 블록 추가
    result = dsp_diagram_builder_add_audio_input(builder, "audio_input",
                                                audio_buffer, audio_length,
                                                config->sample_rate, frame_size);
    if (result != ET_SUCCESS) return result;

    // 2. F0 추출 블록 추가
    result = dsp_diagram_builder_add_f0_extraction(builder, "f0_extraction", &config->f0_config);
    if (result != ET_SUCCESS) return result;

    // 3. 스펙트럼 분석 블록 추가
    result = dsp_diagram_builder_add_spectrum_analysis(builder, "spectrum_analysis", &config->spectrum_config);
    if (result != ET_SUCCESS) return result;

    // 4. 비주기성 분석 블록 추가
    result = dsp_diagram_builder_add_aperiodicity_analysis(builder, "aperiodicity_analysis", &config->aperiodicity_config);
    if (result != ET_SUCCESS) return result;

    // 5. 파라미터 병합 블록 추가
    result = dsp_diagram_builder_add_parameter_merge(builder, "parameter_merge",
                                                    frame_count, config->fft_size);
    if (result != ET_SUCCESS) return result;

    // 6. 연결 생성
    // 오디오 입력 -> F0 추출
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->audio_input_block_id, 0,
                                              builder->f0_extraction_block_id, 0);
    if (result != ET_SUCCESS) return result;

    // 오디오 입력 -> 스펙트럼 분석
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->audio_input_block_id, 0,
                                              builder->spectrum_analysis_block_id, 0);
    if (result != ET_SUCCESS) return result;

    // F0 추출 -> 스펙트럼 분석
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->f0_extraction_block_id, 0,
                                              builder->spectrum_analysis_block_id, 1);
    if (result != ET_SUCCESS) return result;

    // 오디오 입력 -> 비주기성 분석
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->audio_input_block_id, 0,
                                              builder->aperiodicity_analysis_block_id, 0);
    if (result != ET_SUCCESS) return result;

    // F0 추출 -> 비주기성 분석
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->f0_extraction_block_id, 0,
                                              builder->aperiodicity_analysis_block_id, 1);
    if (result != ET_SUCCESS) return result;

    // F0 추출 -> 파라미터 병합
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->f0_extraction_block_id, 0,
                                              builder->parameter_merge_block_id, 0);
    if (result != ET_SUCCESS) return result;

    // 스펙트럼 분석 -> 파라미터 병합
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->spectrum_analysis_block_id, 0,
                                              builder->parameter_merge_block_id, 1);
    if (result != ET_SUCCESS) return result;

    // 비주기성 분석 -> 파라미터 병합
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->aperiodicity_analysis_block_id, 0,
                                              builder->parameter_merge_block_id, 2);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_build_world_synthesis_pipeline(DSPDiagramBuilder* builder,
                                                           const WorldParameters* world_params,
                                                           const WorldPipelineConfig* config) {
    if (!builder || !world_params || !config) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETResult result;
    int output_length = (int)(config->synthesis_config.max_duration_sec * config->sample_rate);

    // 1. 합성 블록 추가
    result = dsp_diagram_builder_add_synthesis(builder, "synthesis", &config->synthesis_config);
    if (result != ET_SUCCESS) return result;

    // 2. 오디오 출력 블록 추가 (필요한 경우)
    if (config->enable_file_output) {
        result = dsp_diagram_builder_add_audio_output(builder, "audio_output",
                                                     output_length, config->sample_rate,
                                                     config->output_filename);
        if (result != ET_SUCCESS) return result;

        // 합성 -> 오디오 출력 연결
        result = dsp_diagram_builder_connect_by_id(builder,
                                                  builder->synthesis_block_id, 0,
                                                  builder->audio_output_block_id, 0);
        if (result != ET_SUCCESS) return result;
    }

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_build_complete_world_pipeline(DSPDiagramBuilder* builder,
                                                          const float* audio_buffer,
                                                          int audio_length,
                                                          const WorldPipelineConfig* config) {
    if (!builder || !audio_buffer || !config) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETResult result;

    // 1. 분석 파이프라인 구성
    result = dsp_diagram_builder_build_world_analysis_pipeline(builder, audio_buffer,
                                                              audio_length, config);
    if (result != ET_SUCCESS) return result;

    // 2. 합성 블록 추가
    result = dsp_diagram_builder_add_synthesis(builder, "synthesis", &config->synthesis_config);
    if (result != ET_SUCCESS) return result;

    // 3. 오디오 출력 블록 추가 (필요한 경우)
    if (config->enable_file_output) {
        int output_length = (int)(config->synthesis_config.max_duration_sec * config->sample_rate);
        result = dsp_diagram_builder_add_audio_output(builder, "audio_output",
                                                     output_length, config->sample_rate,
                                                     config->output_filename);
        if (result != ET_SUCCESS) return result;

        // 합성 -> 오디오 출력 연결
        result = dsp_diagram_builder_connect_by_id(builder,
                                                  builder->synthesis_block_id, 0,
                                                  builder->audio_output_block_id, 0);
        if (result != ET_SUCCESS) return result;
    }

    // 4. 파라미터 병합 -> 합성 연결
    result = dsp_diagram_builder_connect_by_id(builder,
                                              builder->parameter_merge_block_id, 0,
                                              builder->synthesis_block_id, 0);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}

// =============================================================================
// 검증 및 최적화 함수들
// =============================================================================

bool dsp_diagram_builder_validate_connections(DSPDiagramBuilder* builder) {
    if (!builder || !builder->diagram) {
        return false;
    }

    return dsp_block_diagram_validate(builder->diagram);
}

bool dsp_diagram_builder_validate_data_flow(DSPDiagramBuilder* builder) {
    if (!builder || !builder->diagram) {
        return false;
    }

    // 기본적인 데이터 흐름 검증
    // 1. 입력 블록이 있는지 확인
    if (builder->audio_input_block_id == 0) {
        set_builder_error(builder, "No audio input block found");
        return false;
    }

    // 2. 출력 블록이나 최종 처리 블록이 있는지 확인
    if (builder->audio_output_block_id == 0 && builder->synthesis_block_id == 0 &&
        builder->parameter_merge_block_id == 0) {
        set_builder_error(builder, "No output or final processing block found");
        return false;
    }

    // 3. 연결되지 않은 블록이 있는지 확인
    for (int i = 0; i < builder->diagram->block_count; i++) {
        DSPBlock* block = &builder->diagram->blocks[i];

        // 입력 포트 연결 확인 (오디오 입력 블록 제외)
        if (block->type != DSP_BLOCK_TYPE_AUDIO_INPUT) {
            bool has_input_connection = false;
            for (int j = 0; j < block->input_port_count; j++) {
                if (block->input_ports[j].is_connected) {
                    has_input_connection = true;
                    break;
                }
            }
            if (!has_input_connection) {
                set_builder_error(builder, "Block has no input connections");
                return false;
            }
        }

        // 출력 포트 연결 확인 (오디오 출력 블록 제외)
        if (block->type != DSP_BLOCK_TYPE_AUDIO_OUTPUT) {
            bool has_output_connection = false;
            for (int j = 0; j < block->output_port_count; j++) {
                if (block->output_ports[j].is_connected) {
                    has_output_connection = true;
                    break;
                }
            }
            if (!has_output_connection) {
                set_builder_error(builder, "Block has no output connections");
                return false;
            }
        }
    }

    return true;
}

ETResult dsp_diagram_builder_optimize(DSPDiagramBuilder* builder) {
    if (!builder || !builder->diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 기본적인 최적화 수행
    ETResult result = dsp_diagram_builder_remove_unused_blocks(builder);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

ETResult dsp_diagram_builder_remove_unused_blocks(DSPDiagramBuilder* builder) {
    if (!builder || !builder->diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 사용되지 않는 블록 찾기 및 제거
    bool removed_any = false;

    for (int i = builder->diagram->block_count - 1; i >= 0; i--) {
        DSPBlock* block = &builder->diagram->blocks[i];

        // 연결되지 않은 블록 확인
        bool is_connected = false;

        // 입력 포트 연결 확인
        for (int j = 0; j < block->input_port_count; j++) {
            if (block->input_ports[j].is_connected) {
                is_connected = true;
                break;
            }
        }

        // 출력 포트 연결 확인
        if (!is_connected) {
            for (int j = 0; j < block->output_port_count; j++) {
                if (block->output_ports[j].is_connected) {
                    is_connected = true;
                    break;
                }
            }
        }

        // 연결되지 않은 블록 제거 (입력/출력 블록은 제외)
        if (!is_connected && block->type != DSP_BLOCK_TYPE_AUDIO_INPUT &&
            block->type != DSP_BLOCK_TYPE_AUDIO_OUTPUT) {
            dsp_block_diagram_remove_block(builder->diagram, block->block_id);
            removed_any = true;
        }
    }

    return ET_SUCCESS;
}

// =============================================================================
// 디버깅 및 시각화 함수들
// =============================================================================

void dsp_diagram_builder_print_status(DSPDiagramBuilder* builder) {
    if (!builder) {
        printf("Invalid builder\n");
        return;
    }

    printf("=== DSP Diagram Builder Status ===\n");
    printf("Building: %s\n", builder->is_building ? "Yes" : "No");
    printf("Sample Rate: %d Hz\n", builder->sample_rate);
    printf("Audio Length: %d samples\n", builder->audio_length);
    printf("Frame Period: %.2f ms\n", builder->frame_period);
    printf("FFT Size: %d\n", builder->fft_size);

    printf("\nBlock IDs:\n");
    printf("  Audio Input: %d\n", builder->audio_input_block_id);
    printf("  F0 Extraction: %d\n", builder->f0_extraction_block_id);
    printf("  Spectrum Analysis: %d\n", builder->spectrum_analysis_block_id);
    printf("  Aperiodicity Analysis: %d\n", builder->aperiodicity_analysis_block_id);
    printf("  Parameter Merge: %d\n", builder->parameter_merge_block_id);
    printf("  Synthesis: %d\n", builder->synthesis_block_id);
    printf("  Audio Output: %d\n", builder->audio_output_block_id);

    if (builder->error_message[0] != '\0') {
        printf("\nError: %s\n", builder->error_message);
    }

    if (builder->diagram) {
        printf("\n");
        dsp_block_diagram_print_status(builder->diagram);
    }

    printf("==================================\n");
}

ETResult dsp_diagram_builder_export_dot(DSPDiagramBuilder* builder, const char* filename) {
    if (!builder || !builder->diagram || !filename) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return dsp_block_diagram_export_dot(builder->diagram, filename);
}

void dsp_diagram_builder_set_logging(DSPDiagramBuilder* builder, bool enable_logging) {
    if (!builder) {
        return;
    }

    // 로깅 설정 (현재는 단순히 무시)
    // 실제 구현에서는 로깅 시스템과 연동
}

// =============================================================================
// 내부 함수들
// =============================================================================

static void set_builder_error(DSPDiagramBuilder* builder, const char* error_msg) {
    if (!builder || !error_msg) {
        return;
    }

    strncpy(builder->error_message, error_msg, sizeof(builder->error_message) - 1);
    builder->error_message[sizeof(builder->error_message) - 1] = '\0';
}

static int calculate_frame_count(int audio_length, double frame_period, int sample_rate) {
    return (int)(audio_length / (frame_period * sample_rate / 1000.0)) + 1;
}