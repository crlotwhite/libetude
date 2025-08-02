#ifndef WORLD_GRAPH_BUILDER_H
#define WORLD_GRAPH_BUILDER_H

#include <libetude/graph.h>
#include <libetude/types.h>
#include <libetude/memory.h>
#include "world_graph_node.h"
#include "dsp_block_diagram.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 그래프 연결 정보
 */
typedef struct {
    int source_node_id;          /**< 소스 노드 ID */
    int source_port;             /**< 소스 포트 번호 */
    int dest_node_id;            /**< 대상 노드 ID */
    int dest_port;               /**< 대상 포트 번호 */
    size_t buffer_size;          /**< 연결 버퍼 크기 */
} WorldGraphConnection;

/**
 * @brief WORLD 그래프 빌더 구조체
 */
typedef struct {
    ETGraphBuilder* base_builder; /**< libetude 그래프 빌더 */
    DSPBlockDiagram* diagram;     /**< DSP 블록 다이어그램 */

    // 노드 관리
    WorldGraphNode** nodes;       /**< 그래프 노드 배열 */
    int node_count;               /**< 노드 수 */
    int max_nodes;                /**< 최대 노드 수 */

    // 연결 관리
    WorldGraphConnection* connections; /**< 연결 배열 */
    int connection_count;         /**< 연결 수 */
    int max_connections;          /**< 최대 연결 수 */

    // 메모리 관리
    ETMemoryPool* mem_pool;       /**< 메모리 풀 */

    // 빌드 상태
    bool is_built;                /**< 빌드 완료 여부 */
    ETGraph* built_graph;         /**< 빌드된 그래프 */
} WorldGraphBuilder;

/**
 * @brief 그래프 빌더 설정
 */
typedef struct {
    int max_nodes;                /**< 최대 노드 수 */
    int max_connections;          /**< 최대 연결 수 */
    size_t memory_pool_size;      /**< 메모리 풀 크기 */
    bool enable_optimization;     /**< 최적화 활성화 */
    bool enable_validation;       /**< 검증 활성화 */
} WorldGraphBuilderConfig;

// 그래프 빌더 생성 및 관리
WorldGraphBuilder* world_graph_builder_create(const WorldGraphBuilderConfig* config);
WorldGraphBuilder* world_graph_builder_create_from_diagram(DSPBlockDiagram* diagram);
void world_graph_builder_destroy(WorldGraphBuilder* builder);

// 노드 추가 및 관리
ETResult world_graph_builder_add_node(WorldGraphBuilder* builder, WorldGraphNode* node);
ETResult world_graph_builder_add_dsp_block(WorldGraphBuilder* builder,
                                           DSPBlock* block,
                                           WorldNodeType type);
ETResult world_graph_builder_remove_node(WorldGraphBuilder* builder, int node_id);
WorldGraphNode* world_graph_builder_get_node(WorldGraphBuilder* builder, int node_id);
int world_graph_builder_get_node_count(WorldGraphBuilder* builder);

// 연결 관리
ETResult world_graph_builder_connect_nodes(WorldGraphBuilder* builder,
                                          int source_node, int source_port,
                                          int dest_node, int dest_port);
ETResult world_graph_builder_connect_nodes_with_buffer(WorldGraphBuilder* builder,
                                                      int source_node, int source_port,
                                                      int dest_node, int dest_port,
                                                      size_t buffer_size);
ETResult world_graph_builder_disconnect_nodes(WorldGraphBuilder* builder,
                                              int source_node, int source_port,
                                              int dest_node, int dest_port);
int world_graph_builder_get_connection_count(WorldGraphBuilder* builder);

// DSP 블록 다이어그램 변환
ETResult world_graph_builder_convert_from_diagram(WorldGraphBuilder* builder,
                                                  DSPBlockDiagram* diagram);
ETResult world_graph_builder_add_diagram_block(WorldGraphBuilder* builder,
                                               DSPBlock* block,
                                               int block_id);
ETResult world_graph_builder_add_diagram_connections(WorldGraphBuilder* builder,
                                                     DSPBlockDiagram* diagram);

// 그래프 검증
ETResult world_graph_builder_validate(WorldGraphBuilder* builder);
ETResult world_graph_builder_check_cycles(WorldGraphBuilder* builder);
ETResult world_graph_builder_check_connectivity(WorldGraphBuilder* builder);
ETResult world_graph_builder_check_port_compatibility(WorldGraphBuilder* builder);

// 그래프 빌드
ETGraph* world_graph_builder_build(WorldGraphBuilder* builder);
ETResult world_graph_builder_rebuild(WorldGraphBuilder* builder);
bool world_graph_builder_is_built(WorldGraphBuilder* builder);

// 그래프 최적화
ETResult world_graph_builder_optimize(WorldGraphBuilder* builder);
ETResult world_graph_builder_merge_compatible_nodes(WorldGraphBuilder* builder);
ETResult world_graph_builder_eliminate_redundant_connections(WorldGraphBuilder* builder);
ETResult world_graph_builder_reorder_nodes_for_cache(WorldGraphBuilder* builder);

// 디버깅 및 시각화
ETResult world_graph_builder_export_dot(WorldGraphBuilder* builder, const char* filename);
ETResult world_graph_builder_print_topology(WorldGraphBuilder* builder);
ETResult world_graph_builder_validate_execution_order(WorldGraphBuilder* builder);

// 유틸리티 함수들
ETResult world_graph_builder_clear(WorldGraphBuilder* builder);
ETResult world_graph_builder_reset(WorldGraphBuilder* builder);
size_t world_graph_builder_get_memory_usage(WorldGraphBuilder* builder);

// 노드 팩토리 함수들 (DSP 블록에서 그래프 노드로 변환)
WorldGraphNode* world_graph_builder_create_node_from_block(WorldGraphBuilder* builder,
                                                          DSPBlock* block);
ETResult world_graph_builder_configure_node_from_block(WorldGraphNode* node,
                                                       DSPBlock* block);

// 연결 검증 함수들
bool world_graph_builder_is_valid_connection(WorldGraphBuilder* builder,
                                            int source_node, int source_port,
                                            int dest_node, int dest_port);
bool world_graph_builder_has_connection(WorldGraphBuilder* builder,
                                       int source_node, int source_port,
                                       int dest_node, int dest_port);

#ifdef __cplusplus
}
#endif

#endif // WORLD_GRAPH_BUILDER_H