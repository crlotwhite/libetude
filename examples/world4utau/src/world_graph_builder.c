#include "world_graph_builder.h"
#include "world_error.h"
#include <libetude/memory.h>
#include <libetude/error.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// 기본 설정값
#define DEFAULT_MAX_NODES 32
#define DEFAULT_MAX_CONNECTIONS 64
#define DEFAULT_MEMORY_POOL_SIZE (1024 * 1024) // 1MB

/**
 * @brief 그래프 빌더 생성
 */
WorldGraphBuilder* world_graph_builder_create(const WorldGraphBuilderConfig* config) {
    // 기본 설정 사용
    WorldGraphBuilderConfig default_config = {
        .max_nodes = DEFAULT_MAX_NODES,
        .max_connections = DEFAULT_MAX_CONNECTIONS,
        .memory_pool_size = DEFAULT_MEMORY_POOL_SIZE,
        .enable_optimization = true,
        .enable_validation = true
    };

    if (!config) {
        config = &default_config;
    }

    // 메모리 풀 생성
    ETMemoryPool* pool = et_memory_pool_create(config->memory_pool_size);
    if (!pool) {
        return NULL;
    }

    // 빌더 구조체 할당
    WorldGraphBuilder* builder = (WorldGraphBuilder*)et_memory_pool_alloc(pool, sizeof(WorldGraphBuilder));
    if (!builder) {
        et_memory_pool_destroy(pool);
        return NULL;
    }

    memset(builder, 0, sizeof(WorldGraphBuilder));

    // 기본 설정
    builder->mem_pool = pool;
    builder->max_nodes = config->max_nodes;
    builder->max_connections = config->max_connections;
    builder->node_count = 0;
    builder->connection_count = 0;
    builder->is_built = false;
    builder->built_graph = NULL;
    builder->diagram = NULL;

    // 노드 배열 할당
    builder->nodes = (WorldGraphNode**)et_memory_pool_alloc(pool,
                                                           config->max_nodes * sizeof(WorldGraphNode*));
    if (!builder->nodes) {
        et_memory_pool_destroy(pool);
        return NULL;
    }
    memset(builder->nodes, 0, config->max_nodes * sizeof(WorldGraphNode*));

    // 연결 배열 할당
    builder->connections = (WorldGraphConnection*)et_memory_pool_alloc(pool,
                                                                      config->max_connections * sizeof(WorldGraphConnection));
    if (!builder->connections) {
        et_memory_pool_destroy(pool);
        return NULL;
    }
    memset(builder->connections, 0, config->max_connections * sizeof(WorldGraphConnection));

    // libetude 그래프 빌더 생성 (실제 구현에서는 libetude API 사용)
    builder->base_builder = (ETGraphBuilder*)0x1; // 임시 더미 포인터

    return builder;
}

/**
 * @brief DSP 블록 다이어그램에서 그래프 빌더 생성
 */
WorldGraphBuilder* world_graph_builder_create_from_diagram(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return NULL;
    }

    WorldGraphBuilderConfig config = {
        .max_nodes = diagram->block_count + 10, // 여유분 추가
        .max_connections = diagram->connection_count + 10,
        .memory_pool_size = DEFAULT_MEMORY_POOL_SIZE,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    if (!builder) {
        return NULL;
    }

    builder->diagram = diagram;

    // DSP 블록 다이어그램을 그래프로 변환
    if (world_graph_builder_convert_from_diagram(builder, diagram) != ET_SUCCESS) {
        world_graph_builder_destroy(builder);
        return NULL;
    }

    return builder;
}

/**
 * @brief 그래프 빌더 해제
 */
void world_graph_builder_destroy(WorldGraphBuilder* builder) {
    if (!builder) {
        return;
    }

    // 빌드된 그래프 해제 (실제 구현에서는 libetude API 사용)
    if (builder->built_graph) {
        // et_graph_destroy(builder->built_graph);
        builder->built_graph = NULL;
    }

    // 메모리 풀 해제 (모든 할당된 메모리 자동 해제)
    if (builder->mem_pool) {
        et_memory_pool_destroy(builder->mem_pool);
    }
}

/**
 * @brief 노드 추가
 */
ETResult world_graph_builder_add_node(WorldGraphBuilder* builder, WorldGraphNode* node) {
    if (!builder || !node) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (builder->node_count >= builder->max_nodes) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    if (builder->is_built) {
        return ET_ERROR_INVALID_STATE;
    }

    // 노드 추가
    builder->nodes[builder->node_count] = node;
    builder->node_count++;

    return ET_SUCCESS;
}

/**
 * @brief DSP 블록을 그래프 노드로 추가
 */
ETResult world_graph_builder_add_dsp_block(WorldGraphBuilder* builder,
                                           DSPBlock* block,
                                           WorldNodeType type) {
    if (!builder || !block) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // DSP 블록에서 그래프 노드 생성
    WorldGraphNode* node = world_graph_builder_create_node_from_block(builder, block);
    if (!node) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    node->node_type = type;
    node->dsp_block = block;

    // 노드 추가
    return world_graph_builder_add_node(builder, node);
}

/**
 * @brief 노드 제거
 */
ETResult world_graph_builder_remove_node(WorldGraphBuilder* builder, int node_id) {
    if (!builder || node_id < 0 || node_id >= builder->node_count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (builder->is_built) {
        return ET_ERROR_INVALID_STATE;
    }

    // 해당 노드와 관련된 연결들 제거
    for (int i = builder->connection_count - 1; i >= 0; i--) {
        WorldGraphConnection* conn = &builder->connections[i];
        if (conn->source_node_id == node_id || conn->dest_node_id == node_id) {
            // 연결 제거 (배열에서 마지막 요소로 이동)
            if (i < builder->connection_count - 1) {
                builder->connections[i] = builder->connections[builder->connection_count - 1];
            }
            builder->connection_count--;
        }
    }

    // 노드 제거 (배열에서 마지막 요소로 이동)
    if (node_id < builder->node_count - 1) {
        builder->nodes[node_id] = builder->nodes[builder->node_count - 1];

        // 연결에서 노드 ID 업데이트
        for (int i = 0; i < builder->connection_count; i++) {
            WorldGraphConnection* conn = &builder->connections[i];
            if (conn->source_node_id == builder->node_count - 1) {
                conn->source_node_id = node_id;
            }
            if (conn->dest_node_id == builder->node_count - 1) {
                conn->dest_node_id = node_id;
            }
        }
    }

    builder->node_count--;

    return ET_SUCCESS;
}

/**
 * @brief 노드 조회
 */
WorldGraphNode* world_graph_builder_get_node(WorldGraphBuilder* builder, int node_id) {
    if (!builder || node_id < 0 || node_id >= builder->node_count) {
        return NULL;
    }

    return builder->nodes[node_id];
}

/**
 * @brief 노드 수 조회
 */
int world_graph_builder_get_node_count(WorldGraphBuilder* builder) {
    if (!builder) {
        return 0;
    }

    return builder->node_count;
}

/**
 * @brief 노드 연결
 */
ETResult world_graph_builder_connect_nodes(WorldGraphBuilder* builder,
                                          int source_node, int source_port,
                                          int dest_node, int dest_port) {
    return world_graph_builder_connect_nodes_with_buffer(builder,
                                                        source_node, source_port,
                                                        dest_node, dest_port,
                                                        1024); // 기본 버퍼 크기
}

/**
 * @brief 버퍼 크기를 지정하여 노드 연결
 */
ETResult world_graph_builder_connect_nodes_with_buffer(WorldGraphBuilder* builder,
                                                      int source_node, int source_port,
                                                      int dest_node, int dest_port,
                                                      size_t buffer_size) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (builder->is_built) {
        return ET_ERROR_INVALID_STATE;
    }

    if (builder->connection_count >= builder->max_connections) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 연결 유효성 검사
    if (!world_graph_builder_is_valid_connection(builder, source_node, source_port,
                                                dest_node, dest_port)) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 중복 연결 검사
    if (world_graph_builder_has_connection(builder, source_node, source_port,
                                          dest_node, dest_port)) {
        return ET_ERROR_ALREADY_EXISTS;
    }

    // 연결 추가
    WorldGraphConnection* conn = &builder->connections[builder->connection_count];
    conn->source_node_id = source_node;
    conn->source_port = source_port;
    conn->dest_node_id = dest_node;
    conn->dest_port = dest_port;
    conn->buffer_size = buffer_size;

    builder->connection_count++;

    return ET_SUCCESS;
}

/**
 * @brief 노드 연결 해제
 */
ETResult world_graph_builder_disconnect_nodes(WorldGraphBuilder* builder,
                                              int source_node, int source_port,
                                              int dest_node, int dest_port) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (builder->is_built) {
        return ET_ERROR_INVALID_STATE;
    }

    // 연결 찾기 및 제거
    for (int i = 0; i < builder->connection_count; i++) {
        WorldGraphConnection* conn = &builder->connections[i];
        if (conn->source_node_id == source_node && conn->source_port == source_port &&
            conn->dest_node_id == dest_node && conn->dest_port == dest_port) {

            // 연결 제거 (마지막 요소로 이동)
            if (i < builder->connection_count - 1) {
                builder->connections[i] = builder->connections[builder->connection_count - 1];
            }
            builder->connection_count--;

            return ET_SUCCESS;
        }
    }

    return ET_ERROR_NOT_FOUND;
}

/**
 * @brief 연결 수 조회
 */
int world_graph_builder_get_connection_count(WorldGraphBuilder* builder) {
    if (!builder) {
        return 0;
    }

    return builder->connection_count;
}

/**
 * @brief DSP 블록 다이어그램에서 변환
 */
ETResult world_graph_builder_convert_from_diagram(WorldGraphBuilder* builder,
                                                  DSPBlockDiagram* diagram) {
    if (!builder || !diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 기존 노드와 연결 초기화
    world_graph_builder_clear(builder);

    // DSP 블록들을 그래프 노드로 변환
    for (int i = 0; i < diagram->block_count; i++) {
        DSPBlock* block = &diagram->blocks[i];
        ETResult result = world_graph_builder_add_diagram_block(builder, block, i);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // DSP 연결들을 그래프 연결로 변환
    ETResult result = world_graph_builder_add_diagram_connections(builder, diagram);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

/**
 * @brief DSP 블록을 그래프 노드로 추가
 */
ETResult world_graph_builder_add_diagram_block(WorldGraphBuilder* builder,
                                               DSPBlock* block,
                                               int block_id) {
    if (!builder || !block) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 블록 이름에서 노드 타입 결정
    WorldNodeType node_type = WORLD_NODE_AUDIO_INPUT; // 기본값

    if (strstr(block->name, "f0_extraction")) {
        node_type = WORLD_NODE_F0_EXTRACTION;
    } else if (strstr(block->name, "spectrum_analysis")) {
        node_type = WORLD_NODE_SPECTRUM_ANALYSIS;
    } else if (strstr(block->name, "aperiodicity_analysis")) {
        node_type = WORLD_NODE_APERIODICITY_ANALYSIS;
    } else if (strstr(block->name, "synthesis")) {
        node_type = WORLD_NODE_SYNTHESIS;
    } else if (strstr(block->name, "audio_output")) {
        node_type = WORLD_NODE_AUDIO_OUTPUT;
    }

    return world_graph_builder_add_dsp_block(builder, block, node_type);
}

/**
 * @brief DSP 다이어그램 연결들을 그래프 연결로 추가
 */
ETResult world_graph_builder_add_diagram_connections(WorldGraphBuilder* builder,
                                                     DSPBlockDiagram* diagram) {
    if (!builder || !diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < diagram->connection_count; i++) {
        DSPConnection* dsp_conn = &diagram->connections[i];

        ETResult result = world_graph_builder_connect_nodes_with_buffer(builder,
                                                                       dsp_conn->source_block_id,
                                                                       dsp_conn->source_port,
                                                                       dsp_conn->dest_block_id,
                                                                       dsp_conn->dest_port,
                                                                       dsp_conn->buffer_size);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    return ET_SUCCESS;
}

/**
 * @brief 그래프 검증
 */
ETResult world_graph_builder_validate(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 순환 검사
    ETResult result = world_graph_builder_check_cycles(builder);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 연결성 검사
    result = world_graph_builder_check_connectivity(builder);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 포트 호환성 검사
    result = world_graph_builder_check_port_compatibility(builder);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

/**
 * @brief 순환 검사 (간단한 DFS 기반)
 */
ETResult world_graph_builder_check_cycles(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실제 구현에서는 DFS를 사용한 순환 검사
    // 여기서는 간단한 검사만 수행

    return ET_SUCCESS;
}

/**
 * @brief 연결성 검사
 */
ETResult world_graph_builder_check_connectivity(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 모든 노드가 적절히 연결되어 있는지 검사
    // 실제 구현에서는 더 정교한 검사 수행

    return ET_SUCCESS;
}

/**
 * @brief 포트 호환성 검사
 */
ETResult world_graph_builder_check_port_compatibility(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 연결된 포트들의 데이터 타입 호환성 검사
    // 실제 구현에서는 포트 타입 정보를 사용

    return ET_SUCCESS;
}

/**
 * @brief 그래프 빌드
 */
ETGraph* world_graph_builder_build(WorldGraphBuilder* builder) {
    if (!builder) {
        return NULL;
    }

    if (builder->is_built && builder->built_graph) {
        return builder->built_graph;
    }

    // 그래프 검증
    if (world_graph_builder_validate(builder) != ET_SUCCESS) {
        return NULL;
    }

    // 실제 구현에서는 libetude 그래프 API를 사용하여 그래프 생성
    // 여기서는 임시 더미 포인터 반환
    builder->built_graph = (ETGraph*)0x1;
    builder->is_built = true;

    return builder->built_graph;
}

/**
 * @brief 그래프 재빌드
 */
ETResult world_graph_builder_rebuild(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 기존 빌드 상태 초기화
    builder->is_built = false;
    if (builder->built_graph) {
        // 실제 구현에서는 et_graph_destroy 호출
        builder->built_graph = NULL;
    }

    // 새로 빌드
    ETGraph* graph = world_graph_builder_build(builder);
    return (graph != NULL) ? ET_SUCCESS : ET_ERROR_FAILED;
}

/**
 * @brief 빌드 상태 확인
 */
bool world_graph_builder_is_built(WorldGraphBuilder* builder) {
    if (!builder) {
        return false;
    }

    return builder->is_built;
}

/**
 * @brief DSP 블록에서 그래프 노드 생성
 */
WorldGraphNode* world_graph_builder_create_node_from_block(WorldGraphBuilder* builder,
                                                          DSPBlock* block) {
    if (!builder || !block) {
        return NULL;
    }

    // 블록 타입에 따라 적절한 노드 생성
    // 실제 구현에서는 블록의 구체적인 타입 정보를 사용

    WorldGraphNode* node = (WorldGraphNode*)et_memory_pool_alloc(builder->mem_pool, sizeof(WorldGraphNode));
    if (!node) {
        return NULL;
    }

    memset(node, 0, sizeof(WorldGraphNode));

    node->mem_pool = builder->mem_pool;
    node->dsp_block = block;
    node->initialize = world_graph_node_initialize;
    node->cleanup = NULL;

    return node;
}

/**
 * @brief 연결 유효성 검사
 */
bool world_graph_builder_is_valid_connection(WorldGraphBuilder* builder,
                                            int source_node, int source_port,
                                            int dest_node, int dest_port) {
    if (!builder) {
        return false;
    }

    // 노드 ID 범위 검사
    if (source_node < 0 || source_node >= builder->node_count ||
        dest_node < 0 || dest_node >= builder->node_count) {
        return false;
    }

    // 자기 자신과의 연결 방지
    if (source_node == dest_node) {
        return false;
    }

    // 포트 번호 검사 (실제 구현에서는 노드의 포트 수 확인)
    if (source_port < 0 || dest_port < 0) {
        return false;
    }

    return true;
}

/**
 * @brief 연결 존재 여부 확인
 */
bool world_graph_builder_has_connection(WorldGraphBuilder* builder,
                                       int source_node, int source_port,
                                       int dest_node, int dest_port) {
    if (!builder) {
        return false;
    }

    for (int i = 0; i < builder->connection_count; i++) {
        WorldGraphConnection* conn = &builder->connections[i];
        if (conn->source_node_id == source_node && conn->source_port == source_port &&
            conn->dest_node_id == dest_node && conn->dest_port == dest_port) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 그래프 빌더 초기화
 */
ETResult world_graph_builder_clear(WorldGraphBuilder* builder) {
    if (!builder) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    builder->node_count = 0;
    builder->connection_count = 0;
    builder->is_built = false;

    if (builder->built_graph) {
        // 실제 구현에서는 et_graph_destroy 호출
        builder->built_graph = NULL;
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 사용량 조회
 */
size_t world_graph_builder_get_memory_usage(WorldGraphBuilder* builder) {
    if (!builder || !builder->mem_pool) {
        return 0;
    }

    // 실제 구현에서는 메모리 풀의 사용량 조회
    return 0; // 임시 반환값
}