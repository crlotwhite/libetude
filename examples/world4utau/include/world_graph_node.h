#ifndef WORLD_GRAPH_NODE_H
#define WORLD_GRAPH_NODE_H

#include <libetude/graph.h>
#include <libetude/types.h>
#include <libetude/memory.h>
#include "dsp_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WORLD 처리를 위한 그래프 노드 타입
 */
typedef enum {
    WORLD_NODE_AUDIO_INPUT,      /**< 오디오 입력 노드 */
    WORLD_NODE_F0_EXTRACTION,    /**< F0 추출 노드 */
    WORLD_NODE_SPECTRUM_ANALYSIS, /**< 스펙트럼 분석 노드 */
    WORLD_NODE_APERIODICITY_ANALYSIS, /**< 비주기성 분석 노드 */
    WORLD_NODE_PARAMETER_MERGE,  /**< 파라미터 병합 노드 */
    WORLD_NODE_UTAU_MAPPING,     /**< UTAU 매핑 노드 */
    WORLD_NODE_SYNTHESIS,        /**< 합성 노드 */
    WORLD_NODE_AUDIO_OUTPUT,     /**< 오디오 출력 노드 */
    WORLD_NODE_TYPE_COUNT        /**< 노드 타입 개수 */
} WorldNodeType;

/**
 * @brief WORLD 그래프 노드 구조체
 */
typedef struct WorldGraphNode {
    ETGraphNode base;            /**< libetude 그래프 노드 베이스 */
    WorldNodeType node_type;     /**< 노드 타입 */
    DSPBlock* dsp_block;         /**< 연결된 DSP 블록 */
    void* node_data;             /**< 노드별 데이터 */

    // 노드 실행 함수
    ETResult (*execute)(struct WorldGraphNode* node, ETGraphContext* context);

    // 노드 초기화/해제 함수
    ETResult (*initialize)(struct WorldGraphNode* node);
    void (*cleanup)(struct WorldGraphNode* node);

    // 메모리 관리
    ETMemoryPool* mem_pool;      /**< 메모리 풀 */
} WorldGraphNode;

/**
 * @brief 오디오 입력 노드 데이터
 */
typedef struct {
    float* audio_buffer;         /**< 오디오 버퍼 */
    int buffer_size;             /**< 버퍼 크기 */
    int sample_rate;             /**< 샘플링 레이트 */
    int current_position;        /**< 현재 위치 */
} AudioInputNodeData;

/**
 * @brief F0 추출 노드 데이터
 */
typedef struct {
    void* f0_extractor;          /**< F0 추출기 */
    double* f0_output;           /**< F0 출력 버퍼 */
    double* time_axis;           /**< 시간축 버퍼 */
    int f0_length;               /**< F0 길이 */
    double frame_period;         /**< 프레임 주기 */
} F0ExtractionNodeData;

/**
 * @brief 스펙트럼 분석 노드 데이터
 */
typedef struct {
    void* spectrum_analyzer;     /**< 스펙트럼 분석기 */
    double** spectrum_output;    /**< 스펙트럼 출력 버퍼 */
    int spectrum_length;         /**< 스펙트럼 길이 */
    int fft_size;                /**< FFT 크기 */
} SpectrumAnalysisNodeData;

/**
 * @brief 비주기성 분석 노드 데이터
 */
typedef struct {
    void* aperiodicity_analyzer; /**< 비주기성 분석기 */
    double** aperiodicity_output; /**< 비주기성 출력 버퍼 */
    int aperiodicity_length;     /**< 비주기성 길이 */
    int fft_size;                /**< FFT 크기 */
} AperiodicityAnalysisNodeData;

/**
 * @brief 파라미터 병합 노드 데이터
 */
typedef struct {
    void* world_parameters;      /**< WORLD 파라미터 */
    bool f0_ready;               /**< F0 준비 상태 */
    bool spectrum_ready;         /**< 스펙트럼 준비 상태 */
    bool aperiodicity_ready;     /**< 비주기성 준비 상태 */
} ParameterMergeNodeData;

/**
 * @brief 합성 노드 데이터
 */
typedef struct {
    void* synthesis_engine;      /**< 합성 엔진 */
    float* audio_output;         /**< 오디오 출력 버퍼 */
    int output_length;           /**< 출력 길이 */
} SynthesisNodeData;

/**
 * @brief 오디오 출력 노드 데이터
 */
typedef struct {
    float* output_buffer;        /**< 출력 버퍼 */
    int buffer_size;             /**< 버퍼 크기 */
    char* output_file_path;      /**< 출력 파일 경로 */
    bool write_to_file;          /**< 파일 쓰기 여부 */
} AudioOutputNodeData;

// WORLD 그래프 노드 생성 함수들
WorldGraphNode* world_graph_node_create_audio_input(ETMemoryPool* pool,
                                                    const float* audio_buffer,
                                                    int buffer_size,
                                                    int sample_rate);

WorldGraphNode* world_graph_node_create_f0_extraction(ETMemoryPool* pool,
                                                      double frame_period,
                                                      double f0_floor,
                                                      double f0_ceil);

WorldGraphNode* world_graph_node_create_spectrum_analysis(ETMemoryPool* pool,
                                                          int fft_size,
                                                          double q1);

WorldGraphNode* world_graph_node_create_aperiodicity_analysis(ETMemoryPool* pool,
                                                              int fft_size,
                                                              double threshold);

WorldGraphNode* world_graph_node_create_parameter_merge(ETMemoryPool* pool);

WorldGraphNode* world_graph_node_create_utau_mapping(ETMemoryPool* pool);

WorldGraphNode* world_graph_node_create_synthesis(ETMemoryPool* pool,
                                                  int sample_rate,
                                                  double frame_period);

WorldGraphNode* world_graph_node_create_audio_output(ETMemoryPool* pool,
                                                     const char* output_path);

// WORLD 그래프 노드 관리 함수들
ETResult world_graph_node_initialize(WorldGraphNode* node);
ETResult world_graph_node_execute(WorldGraphNode* node, ETGraphContext* context);
void world_graph_node_destroy(WorldGraphNode* node);

// 노드 타입별 실행 함수들
ETResult world_node_execute_audio_input(WorldGraphNode* node, ETGraphContext* context);
ETResult world_node_execute_f0_extraction(WorldGraphNode* node, ETGraphContext* context);
ETResult world_node_execute_spectrum_analysis(WorldGraphNode* node, ETGraphContext* context);
ETResult world_node_execute_aperiodicity_analysis(WorldGraphNode* node, ETGraphContext* context);
ETResult world_node_execute_parameter_merge(WorldGraphNode* node, ETGraphContext* context);
ETResult world_node_execute_utau_mapping(WorldGraphNode* node, ETGraphContext* context);
ETResult world_node_execute_synthesis(WorldGraphNode* node, ETGraphContext* context);
ETResult world_node_execute_audio_output(WorldGraphNode* node, ETGraphContext* context);

// 유틸리티 함수들
const char* world_node_type_to_string(WorldNodeType type);
WorldNodeType world_node_type_from_string(const char* type_str);

#ifdef __cplusplus
}
#endif

#endif // WORLD_GRAPH_NODE_H