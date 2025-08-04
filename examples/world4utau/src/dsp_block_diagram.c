/**
 * @file dsp_block_diagram.c
 * @brief DSP 블록 다이어그램 관리 구현
 */

#include "dsp_block_diagram.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
// DSP 블록 다이어그램 관리 함수들
// =============================================================================

DSPBlockDiagram* dsp_block_diagram_create(const char* name, int max_blocks,
                                         int max_connections, ETMemoryPool* mem_pool) {
    if (!name || max_blocks <= 0 || max_connections <= 0 || !mem_pool) {
        return NULL;
    }

    // 다이어그램 메모리 할당
    DSPBlockDiagram* diagram = (DSPBlockDiagram*)et_memory_pool_alloc(mem_pool, sizeof(DSPBlockDiagram));
    if (!diagram) {
        return NULL;
    }

    memset(diagram, 0, sizeof(DSPBlockDiagram));

    // 기본 정보 설정
    strncpy(diagram->name, name, sizeof(diagram->name) - 1);
    diagram->name[sizeof(diagram->name) - 1] = '\0';
    diagram->max_blocks = max_blocks;
    diagram->max_connections = max_connections;
    diagram->mem_pool = mem_pool;
    diagram->next_block_id = 1;
    diagram->next_connection_id = 1;

    // 블록 배열 할당
    diagram->blocks = (DSPBlock*)et_memory_pool_alloc(mem_pool, sizeof(DSPBlock) * max_blocks);
    if (!diagram->blocks) {
        et_memory_pool_free(mem_pool, diagram);
        return NULL;
    }
    memset(diagram->blocks, 0, sizeof(DSPBlock) * max_blocks);

    // 연결 배열 할당
    diagram->connections = (DSPConnection*)et_memory_pool_alloc(mem_pool, sizeof(DSPConnection) * max_connections);
    if (!diagram->connections) {
        et_memory_pool_free(mem_pool, diagram->blocks);
        et_memory_pool_free(mem_pool, diagram);
        return NULL;
    }
    memset(diagram->connections, 0, sizeof(DSPConnection) * max_connections);

    return diagram;
}

void dsp_block_diagram_destroy(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return;
    }

    // 다이어그램 정리
    dsp_block_diagram_cleanup(diagram);

    // 모든 블록 해제
    for (int i = 0; i < diagram->block_count; i++) {
        dsp_block_destroy(&diagram->blocks[i]);
    }

    // 메모리 해제
    if (diagram->blocks) {
        et_memory_pool_free(diagram->mem_pool, diagram->blocks);
    }
    if (diagram->connections) {
        et_memory_pool_free(diagram->mem_pool, diagram->connections);
    }

    et_memory_pool_free(diagram->mem_pool, diagram);
}

ETResult dsp_block_diagram_add_block(DSPBlockDiagram* diagram, DSPBlock* block) {
    if (!diagram || !block) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (diagram->block_count >= diagram->max_blocks) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 블록 ID 할당
    block->block_id = diagram->next_block_id++;

    // 블록 복사
    memcpy(&diagram->blocks[diagram->block_count], block, sizeof(DSPBlock));
    diagram->block_count++;

    // 빌드 상태 초기화
    diagram->is_built = false;
    diagram->is_validated = false;

    return ET_SUCCESS;
}

ETResult dsp_block_diagram_remove_block(DSPBlockDiagram* diagram, int block_id) {
    if (!diagram || block_id <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 블록 찾기
    int block_index = -1;
    for (int i = 0; i < diagram->block_count; i++) {
        if (diagram->blocks[i].block_id == block_id) {
            block_index = i;
            break;
        }
    }

    if (block_index == -1) {
        return ET_ERROR_NOT_FOUND;
    }

    // 관련 연결 제거
    for (int i = diagram->connection_count - 1; i >= 0; i--) {
        DSPConnection* conn = &diagram->connections[i];
        if (conn->source_block_id == block_id || conn->dest_block_id == block_id) {
            dsp_block_diagram_disconnect(diagram, conn->connection_id);
        }
    }

    // 블록 해제
    dsp_block_destroy(&diagram->blocks[block_index]);

    // 배열에서 제거 (뒤의 요소들을 앞으로 이동)
    for (int i = block_index; i < diagram->block_count - 1; i++) {
        memcpy(&diagram->blocks[i], &diagram->blocks[i + 1], sizeof(DSPBlock));
    }
    diagram->block_count--;

    // 빌드 상태 초기화
    diagram->is_built = false;
    diagram->is_validated = false;

    return ET_SUCCESS;
}

DSPBlock* dsp_block_diagram_find_block(DSPBlockDiagram* diagram, int block_id) {
    if (!diagram || block_id <= 0) {
        return NULL;
    }

    for (int i = 0; i < diagram->block_count; i++) {
        if (diagram->blocks[i].block_id == block_id) {
            return &diagram->blocks[i];
        }
    }

    return NULL;
}

DSPBlock* dsp_block_diagram_find_block_by_name(DSPBlockDiagram* diagram, const char* name) {
    if (!diagram || !name) {
        return NULL;
    }

    for (int i = 0; i < diagram->block_count; i++) {
        if (strcmp(diagram->blocks[i].name, name) == 0) {
            return &diagram->blocks[i];
        }
    }

    return NULL;
}

ETResult dsp_block_diagram_connect(DSPBlockDiagram* diagram,
                                  int source_block_id, int source_port_index,
                                  int dest_block_id, int dest_port_index) {
    if (!diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (diagram->connection_count >= diagram->max_connections) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 블록 찾기
    DSPBlock* source_block = dsp_block_diagram_find_block(diagram, source_block_id);
    DSPBlock* dest_block = dsp_block_diagram_find_block(diagram, dest_block_id);

    if (!source_block || !dest_block) {
        return ET_ERROR_NOT_FOUND;
    }

    // 연결 생성
    DSPConnection* connection = dsp_connection_create(source_block, source_port_index,
                                                     dest_block, dest_port_index,
                                                     diagram->mem_pool);
    if (!connection) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 연결 ID 할당
    connection->connection_id = diagram->next_connection_id++;

    // 연결 배열에 추가
    memcpy(&diagram->connections[diagram->connection_count], connection, sizeof(DSPConnection));
    diagram->connection_count++;

    // 임시 연결 객체 해제
    et_memory_pool_free(diagram->mem_pool, connection);

    // 빌드 상태 초기화
    diagram->is_built = false;
    diagram->is_validated = false;

    return ET_SUCCESS;
}

ETResult dsp_block_diagram_disconnect(DSPBlockDiagram* diagram, int connection_id) {
    if (!diagram || connection_id <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 연결 찾기
    int connection_index = -1;
    for (int i = 0; i < diagram->connection_count; i++) {
        if (diagram->connections[i].connection_id == connection_id) {
            connection_index = i;
            break;
        }
    }

    if (connection_index == -1) {
        return ET_ERROR_NOT_FOUND;
    }

    // 연결 비활성화
    dsp_connection_deactivate(&diagram->connections[connection_index]);

    // 배열에서 제거
    for (int i = connection_index; i < diagram->connection_count - 1; i++) {
        memcpy(&diagram->connections[i], &diagram->connections[i + 1], sizeof(DSPConnection));
    }
    diagram->connection_count--;

    // 빌드 상태 초기화
    diagram->is_built = false;
    diagram->is_validated = false;

    return ET_SUCCESS;
}

bool dsp_block_diagram_validate(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return false;
    }

    // 블록 검증
    for (int i = 0; i < diagram->block_count; i++) {
        DSPBlock* block = &diagram->blocks[i];
        if (!block->process) {
            return false; // 처리 함수가 없음
        }
    }

    // 연결 검증
    for (int i = 0; i < diagram->connection_count; i++) {
        if (!dsp_connection_validate(&diagram->connections[i])) {
            return false;
        }
    }

    // 순환 참조 검사 (간단한 DFS 기반)
    bool* visited = (bool*)calloc(diagram->block_count, sizeof(bool));
    bool* rec_stack = (bool*)calloc(diagram->block_count, sizeof(bool));

    if (!visited || !rec_stack) {
        free(visited);
        free(rec_stack);
        return false;
    }

    bool has_cycle = false;
    for (int i = 0; i < diagram->block_count && !has_cycle; i++) {
        if (!visited[i]) {
            // DFS로 순환 검사 (구현 생략 - 실제로는 재귀적 DFS 구현 필요)
            // 여기서는 간단히 true로 설정
        }
    }

    free(visited);
    free(rec_stack);

    diagram->is_validated = !has_cycle;
    return diagram->is_validated;
}

ETResult dsp_block_diagram_build(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 검증
    if (!dsp_block_diagram_validate(diagram)) {
        return ET_ERROR_INVALID_DIAGRAM;
    }

    // 모든 연결 활성화
    for (int i = 0; i < diagram->connection_count; i++) {
        ETResult result = dsp_connection_activate(&diagram->connections[i]);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    diagram->is_built = true;
    return ET_SUCCESS;
}

ETResult dsp_block_diagram_initialize(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!diagram->is_built) {
        ETResult result = dsp_block_diagram_build(diagram);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 모든 블록 초기화
    for (int i = 0; i < diagram->block_count; i++) {
        ETResult result = dsp_block_initialize(&diagram->blocks[i]);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    return ET_SUCCESS;
}

ETResult dsp_block_diagram_process(DSPBlockDiagram* diagram, int frame_count) {
    if (!diagram || frame_count <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 실행 순서 계산
    int* execution_order = (int*)malloc(sizeof(int) * diagram->block_count);
    if (!execution_order) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int order_size = dsp_block_diagram_calculate_execution_order(diagram, execution_order, diagram->block_count);
    if (order_size <= 0) {
        free(execution_order);
        return ET_ERROR_INVALID_DIAGRAM;
    }

    // 순서에 따른 처리
    ETResult result = dsp_block_diagram_process_ordered(diagram, execution_order, order_size, frame_count);

    free(execution_order);
    return result;
}

void dsp_block_diagram_cleanup(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return;
    }

    // 모든 블록 정리
    for (int i = 0; i < diagram->block_count; i++) {
        dsp_block_cleanup(&diagram->blocks[i]);
    }

    // 모든 연결 비활성화
    for (int i = 0; i < diagram->connection_count; i++) {
        dsp_connection_deactivate(&diagram->connections[i]);
    }
}

void dsp_block_diagram_print_status(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return;
    }

    printf("=== DSP Block Diagram Status ===\n");
    printf("Name: %s\n", diagram->name);
    printf("Blocks: %d/%d\n", diagram->block_count, diagram->max_blocks);
    printf("Connections: %d/%d\n", diagram->connection_count, diagram->max_connections);
    printf("Built: %s\n", diagram->is_built ? "Yes" : "No");
    printf("Validated: %s\n", diagram->is_validated ? "Yes" : "No");

    printf("\nBlocks:\n");
    for (int i = 0; i < diagram->block_count; i++) {
        DSPBlock* block = &diagram->blocks[i];
        printf("  [%d] %s (Type: %d, Inputs: %d, Outputs: %d)\n",
               block->block_id, block->name, block->type,
               block->input_port_count, block->output_port_count);
    }

    printf("\nConnections:\n");
    for (int i = 0; i < diagram->connection_count; i++) {
        DSPConnection* conn = &diagram->connections[i];
        printf("  [%d] Block %d:%d -> Block %d:%d %s\n",
               conn->connection_id,
               conn->source_block_id, conn->source_port_id,
               conn->dest_block_id, conn->dest_port_id,
               conn->is_active ? "(Active)" : "(Inactive)");
    }
    printf("================================\n");
}

ETResult dsp_block_diagram_export_dot(DSPBlockDiagram* diagram, const char* filename) {
    if (!diagram || !filename) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    FILE* file = fopen(filename, "w");
    if (!file) {
        return ET_ERROR_FILE_IO;
    }

    fprintf(file, "digraph DSPBlockDiagram {\n");
    fprintf(file, "  rankdir=LR;\n");
    fprintf(file, "  node [shape=box];\n\n");

    // 블록 노드 출력
    for (int i = 0; i < diagram->block_count; i++) {
        DSPBlock* block = &diagram->blocks[i];
        fprintf(file, "  block_%d [label=\"%s\\nID: %d\\nType: %d\"];\n",
                block->block_id, block->name, block->block_id, block->type);
    }

    fprintf(file, "\n");

    // 연결 엣지 출력
    for (int i = 0; i < diagram->connection_count; i++) {
        DSPConnection* conn = &diagram->connections[i];
        fprintf(file, "  block_%d -> block_%d [label=\"%d:%d\"%s];\n",
                conn->source_block_id, conn->dest_block_id,
                conn->source_port_id, conn->dest_port_id,
                conn->is_active ? "" : ", style=dashed");
    }

    fprintf(file, "}\n");
    fclose(file);

    return ET_SUCCESS;
}

// =============================================================================
// 데이터 흐름 관리 함수들
// =============================================================================

ETResult dsp_connection_transfer_data(DSPConnection* connection) {
    if (!connection || !connection->is_active) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!connection->source_port || !connection->dest_port) {
        return ET_ERROR_INVALID_CONNECTION;
    }

    if (!connection->source_port->buffer || !connection->dest_port->buffer) {
        return ET_ERROR_BUFFER_NOT_ALLOCATED;
    }

    // 버퍼 크기 확인
    if (connection->source_port->buffer_size != connection->dest_port->buffer_size) {
        return ET_ERROR_BUFFER_SIZE_MISMATCH;
    }

    // 포트 타입에 따른 데이터 크기 계산
    size_t element_size = sizeof(float);
    switch (connection->source_port->type) {
        case DSP_PORT_TYPE_AUDIO:
            element_size = sizeof(float);
            break;
        case DSP_PORT_TYPE_F0:
            element_size = sizeof(double);
            break;
        case DSP_PORT_TYPE_SPECTRUM:
        case DSP_PORT_TYPE_APERIODICITY:
            element_size = sizeof(double*);
            break;
        case DSP_PORT_TYPE_PARAMETERS:
            element_size = sizeof(void*);
            break;
        case DSP_PORT_TYPE_CONTROL:
            element_size = sizeof(int);
            break;
    }

    size_t total_size = element_size * connection->buffer_size;

    // 데이터 복사
    memcpy(connection->dest_port->buffer, connection->source_port->buffer, total_size);

    return ET_SUCCESS;
}

ETResult dsp_block_diagram_transfer_all_data(DSPBlockDiagram* diagram) {
    if (!diagram) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    for (int i = 0; i < diagram->connection_count; i++) {
        ETResult result = dsp_connection_transfer_data(&diagram->connections[i]);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    return ET_SUCCESS;
}

int dsp_block_diagram_calculate_execution_order(DSPBlockDiagram* diagram,
                                               int* execution_order,
                                               int max_order_size) {
    if (!diagram || !execution_order || max_order_size < diagram->block_count) {
        return -1;
    }

    // 간단한 토폴로지 정렬 구현 (Kahn's algorithm)
    int* in_degree = (int*)calloc(diagram->block_count, sizeof(int));
    if (!in_degree) {
        return -1;
    }

    // 각 블록의 입력 차수 계산
    for (int i = 0; i < diagram->connection_count; i++) {
        DSPConnection* conn = &diagram->connections[i];
        for (int j = 0; j < diagram->block_count; j++) {
            if (diagram->blocks[j].block_id == conn->dest_block_id) {
                in_degree[j]++;
                break;
            }
        }
    }

    // 입력 차수가 0인 블록들을 큐에 추가
    int* queue = (int*)malloc(sizeof(int) * diagram->block_count);
    if (!queue) {
        free(in_degree);
        return -1;
    }

    int queue_front = 0, queue_rear = 0;
    for (int i = 0; i < diagram->block_count; i++) {
        if (in_degree[i] == 0) {
            queue[queue_rear++] = i;
        }
    }

    int order_count = 0;

    // 토폴로지 정렬 수행
    while (queue_front < queue_rear && order_count < max_order_size) {
        int current_index = queue[queue_front++];
        execution_order[order_count++] = diagram->blocks[current_index].block_id;

        // 현재 블록에서 나가는 연결들 처리
        for (int i = 0; i < diagram->connection_count; i++) {
            DSPConnection* conn = &diagram->connections[i];
            if (conn->source_block_id == diagram->blocks[current_index].block_id) {
                // 대상 블록의 입력 차수 감소
                for (int j = 0; j < diagram->block_count; j++) {
                    if (diagram->blocks[j].block_id == conn->dest_block_id) {
                        in_degree[j]--;
                        if (in_degree[j] == 0) {
                            queue[queue_rear++] = j;
                        }
                        break;
                    }
                }
            }
        }
    }

    free(in_degree);
    free(queue);

    // 모든 블록이 처리되었는지 확인 (순환 참조 검사)
    if (order_count != diagram->block_count) {
        return -1; // 순환 참조 존재
    }

    return order_count;
}

ETResult dsp_block_diagram_process_ordered(DSPBlockDiagram* diagram,
                                          const int* execution_order,
                                          int order_size, int frame_count) {
    if (!diagram || !execution_order || order_size <= 0 || frame_count <= 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 순서에 따라 블록 처리
    for (int i = 0; i < order_size; i++) {
        DSPBlock* block = dsp_block_diagram_find_block(diagram, execution_order[i]);
        if (!block) {
            return ET_ERROR_NOT_FOUND;
        }

        // 블록 처리
        ETResult result = dsp_block_process(block, frame_count);
        if (result != ET_SUCCESS) {
            return result;
        }

        // 출력 데이터를 연결된 블록으로 전송
        for (int j = 0; j < diagram->connection_count; j++) {
            DSPConnection* conn = &diagram->connections[j];
            if (conn->source_block_id == execution_order[i]) {
                ETResult transfer_result = dsp_connection_transfer_data(conn);
                if (transfer_result != ET_SUCCESS) {
                    return transfer_result;
                }
            }
        }
    }

    return ET_SUCCESS;
}