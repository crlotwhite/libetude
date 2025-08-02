#include "world_graph_node.h"
#include "world_engine.h"
#include "audio_file_io.h"
#include <libetude/memory.h>
#include <libetude/error.h>
#include <string.h>
#include <stdlib.h>

// 노드 타입 문자열 매핑
static const char* node_type_strings[] = {
    "AUDIO_INPUT",
    "F0_EXTRACTION",
    "SPECTRUM_ANALYSIS",
    "APERIODICITY_ANALYSIS",
    "PARAMETER_MERGE",
    "UTAU_MAPPING",
    "SYNTHESIS",
    "AUDIO_OUTPUT"
};

/**
 * @brief 오디오 입력 노드 생성
 */
WorldGraphNode* world_graph_node_create_audio_input(ETMemoryPool* pool,
                                                    const float* audio_buffer,
                                                    int buffer_size,
                                                    int sample_rate) {
    if (!pool || !audio_buffer || buffer_size <= 0 || sample_rate <= 0) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_AUDIO_INPUT;
    node->mem_pool = pool;
    node->execute = world_node_execute_audio_input;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL; // 메모리 풀에서 관리

    // 노드 데이터 생성
    AudioInputNodeData* data = (AudioInputNodeData*)et_memory_pool_alloc(pool, sizeof(AudioInputNodeData));
    if (!data) {
        return NULL;
    }

    data->buffer_size = buffer_size;
    data->sample_rate = sample_rate;
    data->current_position = 0;

    // 오디오 버퍼 복사
    data->audio_buffer = (float*)et_memory_pool_alloc(pool, buffer_size * sizeof(float));
    if (!data->audio_buffer) {
        return NULL;
    }
    memcpy(data->audio_buffer, audio_buffer, buffer_size * sizeof(float));

    node->node_data = data;

    return node;
}

/**
 * @brief F0 추출 노드 생성
 */
WorldGraphNode* world_graph_node_create_f0_extraction(ETMemoryPool* pool,
                                                      double frame_period,
                                                      double f0_floor,
                                                      double f0_ceil) {
    if (!pool || frame_period <= 0 || f0_floor <= 0 || f0_ceil <= f0_floor) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_F0_EXTRACTION;
    node->mem_pool = pool;
    node->execute = world_node_execute_f0_extraction;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    // 노드 데이터 생성
    F0ExtractionNodeData* data = (F0ExtractionNodeData*)et_memory_pool_alloc(pool, sizeof(F0ExtractionNodeData));
    if (!data) {
        return NULL;
    }

    data->frame_period = frame_period;
    data->f0_extractor = NULL; // 초기화 시 생성
    data->f0_output = NULL;
    data->time_axis = NULL;
    data->f0_length = 0;

    node->node_data = data;

    return node;
}

/**
 * @brief 스펙트럼 분석 노드 생성
 */
WorldGraphNode* world_graph_node_create_spectrum_analysis(ETMemoryPool* pool,
                                                          int fft_size,
                                                          double q1) {
    if (!pool || fft_size <= 0 || q1 <= 0) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_SPECTRUM_ANALYSIS;
    node->mem_pool = pool;
    node->execute = world_node_execute_spectrum_analysis;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    // 노드 데이터 생성
    SpectrumAnalysisNodeData* data = (SpectrumAnalysisNodeData*)et_memory_pool_alloc(pool, sizeof(SpectrumAnalysisNodeData));
    if (!data) {
        return NULL;
    }

    data->fft_size = fft_size;
    data->spectrum_analyzer = NULL; // 초기화 시 생성
    data->spectrum_output = NULL;
    data->spectrum_length = 0;

    node->node_data = data;

    return node;
}

/**
 * @brief 비주기성 분석 노드 생성
 */
WorldGraphNode* world_graph_node_create_aperiodicity_analysis(ETMemoryPool* pool,
                                                              int fft_size,
                                                              double threshold) {
    if (!pool || fft_size <= 0 || threshold <= 0) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_APERIODICITY_ANALYSIS;
    node->mem_pool = pool;
    node->execute = world_node_execute_aperiodicity_analysis;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    // 노드 데이터 생성
    AperiodicityAnalysisNodeData* data = (AperiodicityAnalysisNodeData*)et_memory_pool_alloc(pool, sizeof(AperiodicityAnalysisNodeData));
    if (!data) {
        return NULL;
    }

    data->fft_size = fft_size;
    data->aperiodicity_analyzer = NULL; // 초기화 시 생성
    data->aperiodicity_output = NULL;
    data->aperiodicity_length = 0;

    node->node_data = data;

    return node;
}

/**
 * @brief 파라미터 병합 노드 생성
 */
WorldGraphNode* world_graph_node_create_parameter_merge(ETMemoryPool* pool) {
    if (!pool) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_PARAMETER_MERGE;
    node->mem_pool = pool;
    node->execute = world_node_execute_parameter_merge;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    // 노드 데이터 생성
    ParameterMergeNodeData* data = (ParameterMergeNodeData*)et_memory_pool_alloc(pool, sizeof(ParameterMergeNodeData));
    if (!data) {
        return NULL;
    }

    data->world_parameters = NULL;
    data->f0_ready = false;
    data->spectrum_ready = false;
    data->aperiodicity_ready = false;

    node->node_data = data;

    return node;
}

/**
 * @brief UTAU 매핑 노드 생성
 */
WorldGraphNode* world_graph_node_create_utau_mapping(ETMemoryPool* pool) {
    if (!pool) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_UTAU_MAPPING;
    node->mem_pool = pool;
    node->execute = world_node_execute_utau_mapping;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    node->node_data = NULL; // UTAU 매핑은 별도 데이터 불필요

    return node;
}

/**
 * @brief 합성 노드 생성
 */
WorldGraphNode* world_graph_node_create_synthesis(ETMemoryPool* pool,
                                                  int sample_rate,
                                                  double frame_period) {
    if (!pool || sample_rate <= 0 || frame_period <= 0) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_SYNTHESIS;
    node->mem_pool = pool;
    node->execute = world_node_execute_synthesis;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    // 노드 데이터 생성
    SynthesisNodeData* data = (SynthesisNodeData*)et_memory_pool_alloc(pool, sizeof(SynthesisNodeData));
    if (!data) {
        return NULL;
    }

    data->synthesis_engine = NULL; // 초기화 시 생성
    data->audio_output = NULL;
    data->output_length = 0;

    node->node_data = data;

    return node;
}

/**
 * @brief 오디오 출력 노드 생성
 */
WorldGraphNode* world_graph_node_create_audio_output(ETMemoryPool* pool,
                                                     const char* output_path) {
    if (!pool) {
        return NULL;
    }

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    // 기본 설정
    node->node_type = WORLD_NODE_AUDIO_OUTPUT;
    node->mem_pool = pool;
    node->execute = world_node_execute_audio_output;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    // 노드 데이터 생성
    AudioOutputNodeData* data = (AudioOutputNodeData*)et_memory_pool_alloc(pool, sizeof(AudioOutputNodeData));
    if (!data) {
        return NULL;
    }

    data->output_buffer = NULL;
    data->buffer_size = 0;
    data->write_to_file = (output_path != NULL);

    if (output_path) {
        size_t path_len = strlen(output_path) + 1;
        data->output_file_path = (char*)et_memory_pool_alloc(pool, path_len);
        if (!data->output_file_path) {
            return NULL;
        }
        strcpy(data->output_file_path, output_path);
    } else {
        data->output_file_path = NULL;
    }

    node->node_data = data;

    return node;
}

/**
 * @brief 그래프 노드 초기화
 */
ETResult world_graph_node_initialize(WorldGraphNode* node) {
    if (!node) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 노드 타입별 초기화 로직
    switch (node->node_type) {
        case WORLD_NODE_AUDIO_INPUT:
            // 오디오 입력 노드는 생성 시 이미 초기화됨
            break;

        case WORLD_NODE_F0_EXTRACTION: {
            F0ExtractionNodeData* data = (F0ExtractionNodeData*)node->node_data;
            if (data && !data->f0_extractor) {
                // F0 추출기 생성 (실제 구현에서는 WorldF0Extractor 사용)
                data->f0_extractor = (void*)0x1; // 임시 더미 포인터
            }
            break;
        }

        case WORLD_NODE_SPECTRUM_ANALYSIS: {
            SpectrumAnalysisNodeData* data = (SpectrumAnalysisNodeData*)node->node_data;
            if (data && !data->spectrum_analyzer) {
                // 스펙트럼 분석기 생성
                data->spectrum_analyzer = (void*)0x1; // 임시 더미 포인터
            }
            break;
        }

        case WORLD_NODE_APERIODICITY_ANALYSIS: {
            AperiodicityAnalysisNodeData* data = (AperiodicityAnalysisNodeData*)node->node_data;
            if (data && !data->aperiodicity_analyzer) {
                // 비주기성 분석기 생성
                data->aperiodicity_analyzer = (void*)0x1; // 임시 더미 포인터
            }
            break;
        }

        case WORLD_NODE_SYNTHESIS: {
            SynthesisNodeData* data = (SynthesisNodeData*)node->node_data;
            if (data && !data->synthesis_engine) {
                // 합성 엔진 생성
                data->synthesis_engine = (void*)0x1; // 임시 더미 포인터
            }
            break;
        }

        default:
            // 다른 노드들은 특별한 초기화 불필요
            break;
    }

    return ET_SUCCESS;
}

/**
 * @brief 그래프 노드 실행
 */
ETResult world_graph_node_execute(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->execute) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return node->execute(node, context);
}

/**
 * @brief 그래프 노드 해제
 */
void world_graph_node_destroy(WorldGraphNode* node) {
    if (!node) {
        return;
    }

    if (node->cleanup) {
        node->cleanup(node);
    }

    // 메모리 풀에서 관리되므로 별도 해제 불필요
}

// 노드 타입별 실행 함수들

/**
 * @brief 오디오 입력 노드 실행
 */
ETResult world_node_execute_audio_input(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->node_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    AudioInputNodeData* data = (AudioInputNodeData*)node->node_data;

    // 오디오 데이터를 그래프 컨텍스트에 설정
    // 실제 구현에서는 ETGraphContext를 통해 데이터 전달

    return ET_SUCCESS;
}

/**
 * @brief F0 추출 노드 실행
 */
ETResult world_node_execute_f0_extraction(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->node_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    F0ExtractionNodeData* data = (F0ExtractionNodeData*)node->node_data;

    // F0 추출 로직 실행
    // 실제 구현에서는 WORLD F0 추출 알고리즘 호출

    return ET_SUCCESS;
}

/**
 * @brief 스펙트럼 분석 노드 실행
 */
ETResult world_node_execute_spectrum_analysis(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->node_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    SpectrumAnalysisNodeData* data = (SpectrumAnalysisNodeData*)node->node_data;

    // 스펙트럼 분석 로직 실행
    // 실제 구현에서는 CheapTrick 알고리즘 호출

    return ET_SUCCESS;
}

/**
 * @brief 비주기성 분석 노드 실행
 */
ETResult world_node_execute_aperiodicity_analysis(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->node_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    AperiodicityAnalysisNodeData* data = (AperiodicityAnalysisNodeData*)node->node_data;

    // 비주기성 분석 로직 실행
    // 실제 구현에서는 D4C 알고리즘 호출

    return ET_SUCCESS;
}

/**
 * @brief 파라미터 병합 노드 실행
 */
ETResult world_node_execute_parameter_merge(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->node_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ParameterMergeNodeData* data = (ParameterMergeNodeData*)node->node_data;

    // 모든 파라미터가 준비되었는지 확인
    if (data->f0_ready && data->spectrum_ready && data->aperiodicity_ready) {
        // WORLD 파라미터 병합 로직 실행
        return ET_SUCCESS;
    }

    return ET_ERROR_NOT_READY;
}

/**
 * @brief UTAU 매핑 노드 실행
 */
ETResult world_node_execute_utau_mapping(WorldGraphNode* node, ETGraphContext* context) {
    if (!node) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // UTAU 파라미터를 WORLD 파라미터에 적용
    // 피치 벤드, 볼륨, 모듈레이션 등 적용

    return ET_SUCCESS;
}

/**
 * @brief 합성 노드 실행
 */
ETResult world_node_execute_synthesis(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->node_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    SynthesisNodeData* data = (SynthesisNodeData*)node->node_data;

    // WORLD 합성 로직 실행
    // 실제 구현에서는 WorldSynthesisEngine 사용

    return ET_SUCCESS;
}

/**
 * @brief 오디오 출력 노드 실행
 */
ETResult world_node_execute_audio_output(WorldGraphNode* node, ETGraphContext* context) {
    if (!node || !node->node_data) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    AudioOutputNodeData* data = (AudioOutputNodeData*)node->node_data;

    if (data->write_to_file && data->output_file_path) {
        // WAV 파일로 출력
        // 실제 구현에서는 write_wav_file 함수 사용
    }

    return ET_SUCCESS;
}

// 유틸리티 함수들

/**
 * @brief 노드 타입을 문자열로 변환
 */
const char* world_node_type_to_string(WorldNodeType type) {
    if (type >= 0 && type < WORLD_NODE_TYPE_COUNT) {
        return node_type_strings[type];
    }
    return "UNKNOWN";
}

/**
 * @brief 문자열을 노드 타입으로 변환
 */
WorldNodeType world_node_type_from_string(const char* type_str) {
    if (!type_str) {
        return WORLD_NODE_TYPE_COUNT; // 잘못된 타입
    }

    for (int i = 0; i < WORLD_NODE_TYPE_COUNT; i++) {
        if (strcmp(type_str, node_type_strings[i]) == 0) {
            return (WorldNodeType)i;
        }
    }

    return WORLD_NODE_TYPE_COUNT; // 잘못된 타입
}