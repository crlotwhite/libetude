#ifndef WORLD_VISUALIZATION_H
#define WORLD_VISUALIZATION_H

#include "dsp_block_diagram.h"
#include "world_graph_node.h"
#include "world_graph_context.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DSP 블록 다이어그램 시각화 설정
 */
typedef struct {
    bool show_execution_time;    // 실행 시간 표시 여부
    bool show_data_flow;         // 데이터 흐름 표시 여부
    bool show_memory_usage;      // 메모리 사용량 표시 여부
    bool use_colors;             // 색상 사용 여부
    char output_format[16];      // 출력 형식 (dot, svg, png 등)
} VisualizationConfig;

/**
 * @brief 블록 실행 통계 정보
 */
typedef struct {
    char block_name[64];         // 블록 이름
    double execution_time_ms;    // 실행 시간 (밀리초)
    size_t memory_usage_bytes;   // 메모리 사용량 (바이트)
    int execution_count;         // 실행 횟수
    bool has_error;              // 오류 발생 여부
    char error_message[256];     // 오류 메시지
} BlockExecutionStats;

/**
 * @brief 시각화 컨텍스트
 */
typedef struct {
    VisualizationConfig config;
    BlockExecutionStats* block_stats;
    int block_count;
    FILE* output_file;
    char* dot_content;
    size_t dot_content_size;
} VisualizationContext;

/**
 * @brief 시각화 컨텍스트 생성
 * @param config 시각화 설정
 * @return 생성된 시각화 컨텍스트 (실패 시 NULL)
 */
VisualizationContext* world_visualization_create(const VisualizationConfig* config);

/**
 * @brief DSP 블록 다이어그램을 DOT 형식으로 출력
 * @param viz_ctx 시각화 컨텍스트
 * @param diagram DSP 블록 다이어그램
 * @param output_path 출력 파일 경로
 * @return 성공 시 0, 실패 시 음수
 */
int world_visualization_export_dsp_diagram(VisualizationContext* viz_ctx,
                                          const DSPBlockDiagram* diagram,
                                          const char* output_path);

/**
 * @brief 그래프 노드를 DOT 형식으로 출력
 * @param viz_ctx 시각화 컨텍스트
 * @param graph_context 그래프 컨텍스트
 * @param output_path 출력 파일 경로
 * @return 성공 시 0, 실패 시 음수
 */
int world_visualization_export_graph_nodes(VisualizationContext* viz_ctx,
                                          const WorldGraphContext* graph_context,
                                          const char* output_path);

/**
 * @brief 실행 통계를 시각화에 추가
 * @param viz_ctx 시각화 컨텍스트
 * @param block_name 블록 이름
 * @param execution_time 실행 시간 (밀리초)
 * @param memory_usage 메모리 사용량 (바이트)
 * @return 성공 시 0, 실패 시 음수
 */
int world_visualization_add_execution_stats(VisualizationContext* viz_ctx,
                                           const char* block_name,
                                           double execution_time,
                                           size_t memory_usage);

/**
 * @brief 데이터 흐름 정보를 시각화에 추가
 * @param viz_ctx 시각화 컨텍스트
 * @param source_block 소스 블록 이름
 * @param dest_block 대상 블록 이름
 * @param data_size 데이터 크기 (바이트)
 * @param transfer_time 전송 시간 (밀리초)
 * @return 성공 시 0, 실패 시 음수
 */
int world_visualization_add_data_flow(VisualizationContext* viz_ctx,
                                     const char* source_block,
                                     const char* dest_block,
                                     size_t data_size,
                                     double transfer_time);

/**
 * @brief DOT 파일을 이미지로 변환 (Graphviz 필요)
 * @param dot_file_path DOT 파일 경로
 * @param output_path 출력 이미지 경로
 * @param format 출력 형식 (svg, png, pdf 등)
 * @return 성공 시 0, 실패 시 음수
 */
int world_visualization_render_to_image(const char* dot_file_path,
                                       const char* output_path,
                                       const char* format);

/**
 * @brief 시각화 컨텍스트 해제
 * @param viz_ctx 시각화 컨텍스트
 */
void world_visualization_destroy(VisualizationContext* viz_ctx);

#ifdef __cplusplus
}
#endif

#endif // WORLD_VISUALIZATION_H