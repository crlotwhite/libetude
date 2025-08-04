/**
 * @file world_dsp_blocks.c
 * @brief WORLD 전용 DSP 블록 구현
 */

#include "world_dsp_blocks.h"
#include "audio_file_io.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// 오디오 입력 블록
// =============================================================================

DSPBlock* create_audio_input_block(const char* name,
                                  const float* audio_buffer, int audio_length,
                                  int sample_rate, int frame_size,
                                  ETMemoryPool* mem_pool) {
    if (!name || !audio_buffer || audio_length <= 0 || sample_rate <= 0 || frame_size <= 0) {
        return NULL;
    }

    // 블록 생성 (출력 포트 1개: 오디오)
    DSPBlock* block = dsp_block_create(name, DSP_BLOCK_TYPE_AUDIO_INPUT, 0, 1, mem_pool);
    if (!block) {
        return NULL;
    }

    // 블록 데이터 할당
    AudioInputBlockData* data = (AudioInputBlockData*)et_memory_pool_alloc(mem_pool, sizeof(AudioInputBlockData));
    if (!data) {
        dsp_block_destroy(block);
        return NULL;
    }

    // 오디오 버퍼 복사
    data->audio_buffer = (float*)et_memory_pool_alloc(mem_pool, sizeof(float) * audio_length);
    if (!data->audio_buffer) {
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }
    memcpy(data->audio_buffer, audio_buffer, sizeof(float) * audio_length);

    // 데이터 초기화
    data->audio_length = audio_length;
    data->sample_rate = sample_rate;
    data->current_position = 0;
    data->frame_size = frame_size;

    // 블록 설정
    block->block_data = data;
    block->block_data_size = sizeof(AudioInputBlockData);
    block->process = audio_input_block_process;
    block->initialize = audio_input_block_initialize;
    block->cleanup = audio_input_block_cleanup;

    // 출력 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT, "audio_out",
                      DSP_PORT_TYPE_AUDIO, frame_size);

    return block;
}

ETResult audio_input_block_initialize(DSPBlock* block) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    AudioInputBlockData* data = (AudioInputBlockData*)block->block_data;
    data->current_position = 0;

    return ET_SUCCESS;
}

ETResult audio_input_block_process(DSPBlock* block, int frame_count) {
    if (!block || !block->block_data || frame_count <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    AudioInputBlockData* data = (AudioInputBlockData*)block->block_data;
    DSPPort* output_port = dsp_block_get_output_port(block, 0);

    if (!output_port || !output_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    float* output_buffer = (float*)output_port->buffer;
    int samples_to_copy = (frame_count < data->frame_size) ? frame_count : data->frame_size;

    // 남은 샘플 수 확인
    int remaining_samples = data->audio_length - data->current_position;
    if (remaining_samples <= 0) {
        // 더 이상 읽을 데이터가 없음 - 제로 패딩
        memset(output_buffer, 0, sizeof(float) * samples_to_copy);
        return ET_SUCCESS;
    }

    if (samples_to_copy > remaining_samples) {
        samples_to_copy = remaining_samples;
    }

    // 오디오 데이터 복사
    memcpy(output_buffer, &data->audio_buffer[data->current_position],
           sizeof(float) * samples_to_copy);

    // 남은 부분은 제로 패딩
    if (samples_to_copy < frame_count) {
        memset(&output_buffer[samples_to_copy], 0,
               sizeof(float) * (frame_count - samples_to_copy));
    }

    data->current_position += samples_to_copy;

    return ET_SUCCESS;
}

void audio_input_block_cleanup(DSPBlock* block) {
    if (!block || !block->block_data) {
        return;
    }

    AudioInputBlockData* data = (AudioInputBlockData*)block->block_data;
    data->current_position = 0;
}

// =============================================================================
// F0 추출 블록
// =============================================================================

DSPBlock* create_f0_extraction_block(const char* name,
                                    const F0ExtractionConfig* config,
                                    ETMemoryPool* mem_pool) {
    if (!name || !config) {
        return NULL;
    }

    // 블록 생성 (입력 포트 1개: 오디오, 출력 포트 2개: F0, 시간축)
    DSPBlock* block = dsp_block_create(name, DSP_BLOCK_TYPE_F0_EXTRACTION, 1, 2, mem_pool);
    if (!block) {
        return NULL;
    }

    // 블록 데이터 할당
    F0ExtractionBlockData* data = (F0ExtractionBlockData*)et_memory_pool_alloc(mem_pool, sizeof(F0ExtractionBlockData));
    if (!data) {
        dsp_block_destroy(block);
        return NULL;
    }

    // 설정 복사
    memcpy(&data->config, config, sizeof(F0ExtractionConfig));

    // 프레임 수 계산
    data->frame_count = (int)(config->audio_length / (config->frame_period * config->sample_rate / 1000.0)) + 1;
    data->current_frame = 0;

    // F0 추출기 생성
    data->extractor = world_f0_extractor_create(&data->config, mem_pool);
    if (!data->extractor) {
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 버퍼 할당
    data->input_buffer = (float*)et_memory_pool_alloc(mem_pool, sizeof(float) * config->audio_length);
    data->f0_output = (double*)et_memory_pool_alloc(mem_pool, sizeof(double) * data->frame_count);
    data->time_axis = (double*)et_memory_pool_alloc(mem_pool, sizeof(double) * data->frame_count);

    if (!data->input_buffer || !data->f0_output || !data->time_axis) {
        world_f0_extractor_destroy(data->extractor);
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 블록 설정
    block->block_data = data;
    block->block_data_size = sizeof(F0ExtractionBlockData);
    block->process = f0_extraction_block_process;
    block->initialize = f0_extraction_block_initialize;
    block->cleanup = f0_extraction_block_cleanup;

    // 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT, "audio_in",
                      DSP_PORT_TYPE_AUDIO, config->audio_length);
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT, "f0_out",
                      DSP_PORT_TYPE_F0, data->frame_count);
    dsp_block_set_port(block, 1, DSP_PORT_DIRECTION_OUTPUT, "time_out",
                      DSP_PORT_TYPE_F0, data->frame_count);

    return block;
}

ETResult f0_extraction_block_initialize(DSPBlock* block) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    F0ExtractionBlockData* data = (F0ExtractionBlockData*)block->block_data;
    data->current_frame = 0;

    return ET_SUCCESS;
}

ETResult f0_extraction_block_process(DSPBlock* block, int frame_count) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    F0ExtractionBlockData* data = (F0ExtractionBlockData*)block->block_data;
    DSPPort* input_port = dsp_block_get_input_port(block, 0);
    DSPPort* f0_output_port = dsp_block_get_output_port(block, 0);
    DSPPort* time_output_port = dsp_block_get_output_port(block, 1);

    if (!input_port || !f0_output_port || !time_output_port ||
        !input_port->buffer || !f0_output_port->buffer || !time_output_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    // 입력 오디오 데이터 복사
    float* input_audio = (float*)input_port->buffer;
    memcpy(data->input_buffer, input_audio, sizeof(float) * data->config.audio_length);

    // F0 추출 수행
    ETResult result = world_f0_extractor_extract(data->extractor, data->input_buffer,
                                                data->f0_output, data->time_axis);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 출력 버퍼에 결과 복사
    double* f0_output = (double*)f0_output_port->buffer;
    double* time_output = (double*)time_output_port->buffer;

    memcpy(f0_output, data->f0_output, sizeof(double) * data->frame_count);
    memcpy(time_output, data->time_axis, sizeof(double) * data->frame_count);

    return ET_SUCCESS;
}

void f0_extraction_block_cleanup(DSPBlock* block) {
    if (!block || !block->block_data) {
        return;
    }

    F0ExtractionBlockData* data = (F0ExtractionBlockData*)block->block_data;
    if (data->extractor) {
        world_f0_extractor_destroy(data->extractor);
        data->extractor = NULL;
    }
}

// =============================================================================
// 스펙트럼 분석 블록
// =============================================================================

DSPBlock* create_spectrum_analysis_block(const char* name,
                                        const SpectrumConfig* config,
                                        ETMemoryPool* mem_pool) {
    if (!name || !config) {
        return NULL;
    }

    // 블록 생성 (입력 포트 2개: 오디오, F0, 출력 포트 1개: 스펙트럼)
    DSPBlock* block = dsp_block_create(name, DSP_BLOCK_TYPE_SPECTRUM_ANALYSIS, 2, 1, mem_pool);
    if (!block) {
        return NULL;
    }

    // 블록 데이터 할당
    SpectrumAnalysisBlockData* data = (SpectrumAnalysisBlockData*)et_memory_pool_alloc(mem_pool, sizeof(SpectrumAnalysisBlockData));
    if (!data) {
        dsp_block_destroy(block);
        return NULL;
    }

    // 설정 복사
    memcpy(&data->config, config, sizeof(SpectrumConfig));

    // 프레임 수 및 FFT 크기 설정
    data->frame_count = (int)(config->audio_length / (config->frame_period * config->sample_rate / 1000.0)) + 1;
    data->fft_size = config->fft_size;

    // 스펙트럼 분석기 생성
    data->analyzer = world_spectrum_analyzer_create(&data->config, mem_pool);
    if (!data->analyzer) {
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 버퍼 할당
    data->input_buffer = (float*)et_memory_pool_alloc(mem_pool, sizeof(float) * config->audio_length);
    data->f0_input = (double*)et_memory_pool_alloc(mem_pool, sizeof(double) * data->frame_count);

    // 2D 스펙트럼 배열 할당
    data->spectrum_output = (double**)et_memory_pool_alloc(mem_pool, sizeof(double*) * data->frame_count);
    if (data->spectrum_output) {
        for (int i = 0; i < data->frame_count; i++) {
            data->spectrum_output[i] = (double*)et_memory_pool_alloc(mem_pool, sizeof(double) * (data->fft_size / 2 + 1));
            if (!data->spectrum_output[i]) {
                // 할당 실패 시 이미 할당된 메모리 정리
                for (int j = 0; j < i; j++) {
                    et_memory_pool_free(mem_pool, data->spectrum_output[j]);
                }
                et_memory_pool_free(mem_pool, data->spectrum_output);
                data->spectrum_output = NULL;
                break;
            }
        }
    }

    if (!data->input_buffer || !data->f0_input || !data->spectrum_output) {
        world_spectrum_analyzer_destroy(data->analyzer);
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 블록 설정
    block->block_data = data;
    block->block_data_size = sizeof(SpectrumAnalysisBlockData);
    block->process = spectrum_analysis_block_process;
    block->initialize = spectrum_analysis_block_initialize;
    block->cleanup = spectrum_analysis_block_cleanup;

    // 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT, "audio_in",
                      DSP_PORT_TYPE_AUDIO, config->audio_length);
    dsp_block_set_port(block, 1, DSP_PORT_DIRECTION_INPUT, "f0_in",
                      DSP_PORT_TYPE_F0, data->frame_count);
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT, "spectrum_out",
                      DSP_PORT_TYPE_SPECTRUM, data->frame_count);

    return block;
}

ETResult spectrum_analysis_block_initialize(DSPBlock* block) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return ET_SUCCESS;
}

ETResult spectrum_analysis_block_process(DSPBlock* block, int frame_count) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    SpectrumAnalysisBlockData* data = (SpectrumAnalysisBlockData*)block->block_data;
    DSPPort* audio_input_port = dsp_block_get_input_port(block, 0);
    DSPPort* f0_input_port = dsp_block_get_input_port(block, 1);
    DSPPort* spectrum_output_port = dsp_block_get_output_port(block, 0);

    if (!audio_input_port || !f0_input_port || !spectrum_output_port ||
        !audio_input_port->buffer || !f0_input_port->buffer || !spectrum_output_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    // 입력 데이터 복사
    float* input_audio = (float*)audio_input_port->buffer;
    double* input_f0 = (double*)f0_input_port->buffer;

    memcpy(data->input_buffer, input_audio, sizeof(float) * data->config.audio_length);
    memcpy(data->f0_input, input_f0, sizeof(double) * data->frame_count);

    // 스펙트럼 분석 수행
    ETResult result = world_spectrum_analyzer_analyze(data->analyzer, data->input_buffer,
                                                     data->f0_input, data->spectrum_output);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 출력 버퍼에 결과 복사
    double** spectrum_output = (double**)spectrum_output_port->buffer;
    memcpy(spectrum_output, data->spectrum_output, sizeof(double*) * data->frame_count);

    return ET_SUCCESS;
}

void spectrum_analysis_block_cleanup(DSPBlock* block) {
    if (!block || !block->block_data) {
        return;
    }

    SpectrumAnalysisBlockData* data = (SpectrumAnalysisBlockData*)block->block_data;
    if (data->analyzer) {
        world_spectrum_analyzer_destroy(data->analyzer);
        data->analyzer = NULL;
    }
}

// =============================================================================
// 비주기성 분석 블록
// =============================================================================

DSPBlock* create_aperiodicity_analysis_block(const char* name,
                                            const AperiodicityConfig* config,
                                            ETMemoryPool* mem_pool) {
    if (!name || !config) {
        return NULL;
    }

    // 블록 생성 (입력 포트 2개: 오디오, F0, 출력 포트 1개: 비주기성)
    DSPBlock* block = dsp_block_create(name, DSP_BLOCK_TYPE_APERIODICITY_ANALYSIS, 2, 1, mem_pool);
    if (!block) {
        return NULL;
    }

    // 블록 데이터 할당
    AperiodicityAnalysisBlockData* data = (AperiodicityAnalysisBlockData*)et_memory_pool_alloc(mem_pool, sizeof(AperiodicityAnalysisBlockData));
    if (!data) {
        dsp_block_destroy(block);
        return NULL;
    }

    // 설정 복사
    memcpy(&data->config, config, sizeof(AperiodicityConfig));

    // 프레임 수 및 FFT 크기 설정
    data->frame_count = (int)(config->audio_length / (config->frame_period * config->sample_rate / 1000.0)) + 1;
    data->fft_size = config->fft_size;

    // 비주기성 분석기 생성
    data->analyzer = world_aperiodicity_analyzer_create(&data->config, mem_pool);
    if (!data->analyzer) {
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 버퍼 할당
    data->input_buffer = (float*)et_memory_pool_alloc(mem_pool, sizeof(float) * config->audio_length);
    data->f0_input = (double*)et_memory_pool_alloc(mem_pool, sizeof(double) * data->frame_count);

    // 2D 비주기성 배열 할당
    data->aperiodicity_output = (double**)et_memory_pool_alloc(mem_pool, sizeof(double*) * data->frame_count);
    if (data->aperiodicity_output) {
        for (int i = 0; i < data->frame_count; i++) {
            data->aperiodicity_output[i] = (double*)et_memory_pool_alloc(mem_pool, sizeof(double) * (data->fft_size / 2 + 1));
            if (!data->aperiodicity_output[i]) {
                // 할당 실패 시 이미 할당된 메모리 정리
                for (int j = 0; j < i; j++) {
                    et_memory_pool_free(mem_pool, data->aperiodicity_output[j]);
                }
                et_memory_pool_free(mem_pool, data->aperiodicity_output);
                data->aperiodicity_output = NULL;
                break;
            }
        }
    }

    if (!data->input_buffer || !data->f0_input || !data->aperiodicity_output) {
        world_aperiodicity_analyzer_destroy(data->analyzer);
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 블록 설정
    block->block_data = data;
    block->block_data_size = sizeof(AperiodicityAnalysisBlockData);
    block->process = aperiodicity_analysis_block_process;
    block->initialize = aperiodicity_analysis_block_initialize;
    block->cleanup = aperiodicity_analysis_block_cleanup;

    // 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT, "audio_in",
                      DSP_PORT_TYPE_AUDIO, config->audio_length);
    dsp_block_set_port(block, 1, DSP_PORT_DIRECTION_INPUT, "f0_in",
                      DSP_PORT_TYPE_F0, data->frame_count);
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT, "aperiodicity_out",
                      DSP_PORT_TYPE_APERIODICITY, data->frame_count);

    return block;
}

ETResult aperiodicity_analysis_block_initialize(DSPBlock* block) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return ET_SUCCESS;
}

ETResult aperiodicity_analysis_block_process(DSPBlock* block, int frame_count) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    AperiodicityAnalysisBlockData* data = (AperiodicityAnalysisBlockData*)block->block_data;
    DSPPort* audio_input_port = dsp_block_get_input_port(block, 0);
    DSPPort* f0_input_port = dsp_block_get_input_port(block, 1);
    DSPPort* aperiodicity_output_port = dsp_block_get_output_port(block, 0);

    if (!audio_input_port || !f0_input_port || !aperiodicity_output_port ||
        !audio_input_port->buffer || !f0_input_port->buffer || !aperiodicity_output_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    // 입력 데이터 복사
    float* input_audio = (float*)audio_input_port->buffer;
    double* input_f0 = (double*)f0_input_port->buffer;

    memcpy(data->input_buffer, input_audio, sizeof(float) * data->config.audio_length);
    memcpy(data->f0_input, input_f0, sizeof(double) * data->frame_count);

    // 비주기성 분석 수행
    ETResult result = world_aperiodicity_analyzer_analyze(data->analyzer, data->input_buffer,
                                                         data->f0_input, data->aperiodicity_output);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 출력 버퍼에 결과 복사
    double** aperiodicity_output = (double**)aperiodicity_output_port->buffer;
    memcpy(aperiodicity_output, data->aperiodicity_output, sizeof(double*) * data->frame_count);

    return ET_SUCCESS;
}

void aperiodicity_analysis_block_cleanup(DSPBlock* block) {
    if (!block || !block->block_data) {
        return;
    }

    AperiodicityAnalysisBlockData* data = (AperiodicityAnalysisBlockData*)block->block_data;
    if (data->analyzer) {
        world_aperiodicity_analyzer_destroy(data->analyzer);
        data->analyzer = NULL;
    }
}

// =============================================================================
// 파라미터 병합 블록
// =============================================================================

DSPBlock* create_parameter_merge_block(const char* name,
                                      int frame_count, int fft_size,
                                      ETMemoryPool* mem_pool) {
    if (!name || frame_count <= 0 || fft_size <= 0) {
        return NULL;
    }

    // 블록 생성 (입력 포트 3개: F0, 스펙트럼, 비주기성, 출력 포트 1개: WORLD 파라미터)
    DSPBlock* block = dsp_block_create(name, DSP_BLOCK_TYPE_PARAMETER_MERGE, 3, 1, mem_pool);
    if (!block) {
        return NULL;
    }

    // 블록 데이터 할당
    ParameterMergeBlockData* data = (ParameterMergeBlockData*)et_memory_pool_alloc(mem_pool, sizeof(ParameterMergeBlockData));
    if (!data) {
        dsp_block_destroy(block);
        return NULL;
    }

    // 데이터 초기화
    data->frame_count = frame_count;
    data->fft_size = fft_size;
    data->is_merged = false;

    // WORLD 파라미터 생성
    data->world_params = world_parameters_create(frame_count, fft_size, mem_pool);
    if (!data->world_params) {
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 입력 버퍼 할당
    data->f0_input = (double*)et_memory_pool_alloc(mem_pool, sizeof(double) * frame_count);
    data->spectrum_input = (double**)et_memory_pool_alloc(mem_pool, sizeof(double*) * frame_count);
    data->aperiodicity_input = (double**)et_memory_pool_alloc(mem_pool, sizeof(double*) * frame_count);

    if (!data->f0_input || !data->spectrum_input || !data->aperiodicity_input) {
        world_parameters_destroy(data->world_params);
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 블록 설정
    block->block_data = data;
    block->block_data_size = sizeof(ParameterMergeBlockData);
    block->process = parameter_merge_block_process;
    block->initialize = parameter_merge_block_initialize;
    block->cleanup = parameter_merge_block_cleanup;

    // 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT, "f0_in",
                      DSP_PORT_TYPE_F0, frame_count);
    dsp_block_set_port(block, 1, DSP_PORT_DIRECTION_INPUT, "spectrum_in",
                      DSP_PORT_TYPE_SPECTRUM, frame_count);
    dsp_block_set_port(block, 2, DSP_PORT_DIRECTION_INPUT, "aperiodicity_in",
                      DSP_PORT_TYPE_APERIODICITY, frame_count);
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT, "world_params_out",
                      DSP_PORT_TYPE_PARAMETERS, 1);

    return block;
}

ETResult parameter_merge_block_initialize(DSPBlock* block) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ParameterMergeBlockData* data = (ParameterMergeBlockData*)block->block_data;
    data->is_merged = false;

    return ET_SUCCESS;
}

ETResult parameter_merge_block_process(DSPBlock* block, int frame_count) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ParameterMergeBlockData* data = (ParameterMergeBlockData*)block->block_data;
    DSPPort* f0_input_port = dsp_block_get_input_port(block, 0);
    DSPPort* spectrum_input_port = dsp_block_get_input_port(block, 1);
    DSPPort* aperiodicity_input_port = dsp_block_get_input_port(block, 2);
    DSPPort* output_port = dsp_block_get_output_port(block, 0);

    if (!f0_input_port || !spectrum_input_port || !aperiodicity_input_port || !output_port ||
        !f0_input_port->buffer || !spectrum_input_port->buffer ||
        !aperiodicity_input_port->buffer || !output_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    // 입력 데이터 복사
    double* f0_input = (double*)f0_input_port->buffer;
    double** spectrum_input = (double**)spectrum_input_port->buffer;
    double** aperiodicity_input = (double**)aperiodicity_input_port->buffer;

    memcpy(data->f0_input, f0_input, sizeof(double) * data->frame_count);
    memcpy(data->spectrum_input, spectrum_input, sizeof(double*) * data->frame_count);
    memcpy(data->aperiodicity_input, aperiodicity_input, sizeof(double*) * data->frame_count);

    // WORLD 파라미터에 데이터 병합
    memcpy(data->world_params->f0, data->f0_input, sizeof(double) * data->frame_count);

    for (int i = 0; i < data->frame_count; i++) {
        memcpy(data->world_params->spectrogram[i], data->spectrum_input[i],
               sizeof(double) * (data->fft_size / 2 + 1));
        memcpy(data->world_params->aperiodicity[i], data->aperiodicity_input[i],
               sizeof(double) * (data->fft_size / 2 + 1));
    }

    // 출력 포트에 WORLD 파라미터 설정
    WorldParameters** output_params = (WorldParameters**)output_port->buffer;
    *output_params = data->world_params;

    data->is_merged = true;

    return ET_SUCCESS;
}

void parameter_merge_block_cleanup(DSPBlock* block) {
    if (!block || !block->block_data) {
        return;
    }

    ParameterMergeBlockData* data = (ParameterMergeBlockData*)block->block_data;
    data->is_merged = false;
}

// =============================================================================
// 음성 합성 블록
// =============================================================================

DSPBlock* create_synthesis_block(const char* name,
                                const SynthesisConfig* config,
                                ETMemoryPool* mem_pool) {
    if (!name || !config) {
        return NULL;
    }

    // 블록 생성 (입력 포트 1개: WORLD 파라미터, 출력 포트 1개: 오디오)
    DSPBlock* block = dsp_block_create(name, DSP_BLOCK_TYPE_SYNTHESIS, 1, 1, mem_pool);
    if (!block) {
        return NULL;
    }

    // 블록 데이터 할당
    SynthesisBlockData* data = (SynthesisBlockData*)et_memory_pool_alloc(mem_pool, sizeof(SynthesisBlockData));
    if (!data) {
        dsp_block_destroy(block);
        return NULL;
    }

    // 설정 복사
    memcpy(&data->config, config, sizeof(SynthesisConfig));
    data->sample_rate = config->sample_rate;
    data->output_length = 0;

    // 합성 엔진 생성
    data->engine = world_synthesis_engine_create(&data->config, mem_pool);
    if (!data->engine) {
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 출력 버퍼 할당 (최대 길이로 할당)
    int max_output_length = (int)(config->max_duration_sec * config->sample_rate);
    data->audio_output = (float*)et_memory_pool_alloc(mem_pool, sizeof(float) * max_output_length);
    if (!data->audio_output) {
        world_synthesis_engine_destroy(data->engine);
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 블록 설정
    block->block_data = data;
    block->block_data_size = sizeof(SynthesisBlockData);
    block->process = synthesis_block_process;
    block->initialize = synthesis_block_initialize;
    block->cleanup = synthesis_block_cleanup;

    // 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT, "world_params_in",
                      DSP_PORT_TYPE_PARAMETERS, 1);
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT, "audio_out",
                      DSP_PORT_TYPE_AUDIO, max_output_length);

    return block;
}

ETResult synthesis_block_initialize(DSPBlock* block) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    SynthesisBlockData* data = (SynthesisBlockData*)block->block_data;
    data->output_length = 0;

    return ET_SUCCESS;
}

ETResult synthesis_block_process(DSPBlock* block, int frame_count) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    SynthesisBlockData* data = (SynthesisBlockData*)block->block_data;
    DSPPort* input_port = dsp_block_get_input_port(block, 0);
    DSPPort* output_port = dsp_block_get_output_port(block, 0);

    if (!input_port || !output_port || !input_port->buffer || !output_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    // 입력 WORLD 파라미터 가져오기
    WorldParameters** input_params = (WorldParameters**)input_port->buffer;
    data->input_params = *input_params;

    if (!data->input_params) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 음성 합성 수행
    ETResult result = world_synthesis_engine_synthesize(data->engine, data->input_params,
                                                       data->audio_output, &data->output_length);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 출력 버퍼에 결과 복사
    float* output_audio = (float*)output_port->buffer;
    memcpy(output_audio, data->audio_output, sizeof(float) * data->output_length);

    return ET_SUCCESS;
}

void synthesis_block_cleanup(DSPBlock* block) {
    if (!block || !block->block_data) {
        return;
    }

    SynthesisBlockData* data = (SynthesisBlockData*)block->block_data;
    if (data->engine) {
        world_synthesis_engine_destroy(data->engine);
        data->engine = NULL;
    }
}

// =============================================================================
// 오디오 출력 블록
// =============================================================================

DSPBlock* create_audio_output_block(const char* name,
                                   int buffer_size, int sample_rate,
                                   const char* output_filename,
                                   ETMemoryPool* mem_pool) {
    if (!name || buffer_size <= 0 || sample_rate <= 0) {
        return NULL;
    }

    // 블록 생성 (입력 포트 1개: 오디오, 출력 포트 0개)
    DSPBlock* block = dsp_block_create(name, DSP_BLOCK_TYPE_AUDIO_OUTPUT, 1, 0, mem_pool);
    if (!block) {
        return NULL;
    }

    // 블록 데이터 할당
    AudioOutputBlockData* data = (AudioOutputBlockData*)et_memory_pool_alloc(mem_pool, sizeof(AudioOutputBlockData));
    if (!data) {
        dsp_block_destroy(block);
        return NULL;
    }

    // 데이터 초기화
    data->buffer_size = buffer_size;
    data->sample_rate = sample_rate;
    data->write_to_file = (output_filename != NULL);

    if (output_filename) {
        strncpy(data->output_filename, output_filename, sizeof(data->output_filename) - 1);
        data->output_filename[sizeof(data->output_filename) - 1] = '\0';
    } else {
        data->output_filename[0] = '\0';
    }

    // 오디오 버퍼 할당
    data->audio_buffer = (float*)et_memory_pool_alloc(mem_pool, sizeof(float) * buffer_size);
    if (!data->audio_buffer) {
        et_memory_pool_free(mem_pool, data);
        dsp_block_destroy(block);
        return NULL;
    }

    // 블록 설정
    block->block_data = data;
    block->block_data_size = sizeof(AudioOutputBlockData);
    block->process = audio_output_block_process;
    block->initialize = audio_output_block_initialize;
    block->cleanup = audio_output_block_cleanup;

    // 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT, "audio_in",
                      DSP_PORT_TYPE_AUDIO, buffer_size);

    return block;
}

ETResult audio_output_block_initialize(DSPBlock* block) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return ET_SUCCESS;
}

ETResult audio_output_block_process(DSPBlock* block, int frame_count) {
    if (!block || !block->block_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    AudioOutputBlockData* data = (AudioOutputBlockData*)block->block_data;
    DSPPort* input_port = dsp_block_get_input_port(block, 0);

    if (!input_port || !input_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    // 입력 오디오 데이터 복사
    float* input_audio = (float*)input_port->buffer;
    int samples_to_copy = (frame_count < data->buffer_size) ? frame_count : data->buffer_size;

    memcpy(data->audio_buffer, input_audio, sizeof(float) * samples_to_copy);

    // 파일로 쓰기
    if (data->write_to_file && data->output_filename[0] != '\0') {
        ETResult result = write_wav_file(data->output_filename, data->audio_buffer,
                                        samples_to_copy, data->sample_rate);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    return ET_SUCCESS;
}

void audio_output_block_cleanup(DSPBlock* block) {
    if (!block || !block->block_data) {
        return;
    }

    // 정리할 특별한 작업 없음
}