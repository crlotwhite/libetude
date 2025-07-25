#include "libetude/graph.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// 내부 상수 정의
#define DEFAULT_NODES_CAPACITY 32
#define DEFAULT_CONNECTIONS_CAPACITY 8

// 에러 코드 매핑 (호환성을 위해)
#define ET_SUCCESS LIBETUDE_SUCCESS
#define ET_ERROR_INVALID_ARGUMENT LIBETUDE_ERROR_INVALID_ARGUMENT
#define ET_ERROR_OUT_OF_MEMORY LIBETUDE_ERROR_OUT_OF_MEMORY
#define ET_ERROR_RUNTIME LIBETUDE_ERROR_RUNTIME

// 내부 함수 선언
static int resize_nodes_array(ETGraph* graph, size_t new_capacity);
static int resize_node_connections(ETNode* node, bool is_input, size_t new_capacity);
static void reset_node_states(ETGraph* graph);
static int dfs_visit(ETNode* node, int* visit_state, ETNode** sorted_nodes, int* index, ETGraph* graph);

// =============================================================================
// 그래프 생성 및 관리 함수
// =============================================================================

ETGraph* et_create_graph(size_t initial_nodes_capacity) {
    if (initial_nodes_capacity == 0) {
        initial_nodes_capacity = DEFAULT_NODES_CAPACITY;
    }

    ETGraph* graph = (ETGraph*)malloc(sizeof(ETGraph));
    if (!graph) {
        return NULL;
    }

    // 노드 배열 초기화
    graph->nodes = (ETNode**)calloc(initial_nodes_capacity, sizeof(ETNode*));
    if (!graph->nodes) {
        free(graph);
        return NULL;
    }

    graph->num_nodes = 0;
    graph->nodes_capacity = initial_nodes_capacity;

    // 입출력 노드 배열 초기화
    graph->input_nodes = NULL;
    graph->num_input_nodes = 0;
    graph->output_nodes = NULL;
    graph->num_output_nodes = 0;

    // 실행 순서 배열 초기화
    graph->execution_order = NULL;
    graph->execution_order_size = 0;
    graph->is_sorted = false;

    // 메모리 풀 초기화 (기본 크기: 1MB)
    graph->mem_pool = et_create_memory_pool(1024 * 1024, 32);
    if (!graph->mem_pool) {
        free(graph->nodes);
        free(graph);
        return NULL;
    }

    // 메타데이터 초기화
    graph->name = NULL;
    graph->is_optimized = false;

    return graph;
}

void et_destroy_graph(ETGraph* graph) {
    if (!graph) return;

    // 모든 노드 해제
    for (size_t i = 0; i < graph->num_nodes; i++) {
        et_destroy_node(graph->nodes[i]);
    }

    // 배열들 해제
    free(graph->nodes);
    free(graph->input_nodes);
    free(graph->output_nodes);
    free(graph->execution_order);
    free(graph->name);

    // 메모리 풀 해제
    et_destroy_memory_pool(graph->mem_pool);

    free(graph);
}

int et_add_node(ETGraph* graph, ETNode* node) {
    if (!graph || !node) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 용량 확장이 필요한 경우
    if (graph->num_nodes >= graph->nodes_capacity) {
        int result = resize_nodes_array(graph, graph->nodes_capacity * 2);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 노드 추가
    graph->nodes[graph->num_nodes] = node;
    graph->num_nodes++;

    // 토폴로지 정렬 무효화
    graph->is_sorted = false;

    return ET_SUCCESS;
}

int et_remove_node(ETGraph* graph, ETNode* node) {
    if (!graph || !node) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 노드 찾기
    size_t node_index = SIZE_MAX;
    for (size_t i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i] == node) {
            node_index = i;
            break;
        }
    }

    if (node_index == SIZE_MAX) {
        return ET_ERROR_INVALID_ARGUMENT; // 노드를 찾을 수 없음
    }

    // 다른 노드들과의 연결 해제
    for (size_t i = 0; i < graph->num_nodes; i++) {
        if (i != node_index) {
            et_disconnect_nodes(node, graph->nodes[i]);
            et_disconnect_nodes(graph->nodes[i], node);
        }
    }

    // 배열에서 노드 제거 (뒤의 요소들을 앞으로 이동)
    for (size_t i = node_index; i < graph->num_nodes - 1; i++) {
        graph->nodes[i] = graph->nodes[i + 1];
    }
    graph->num_nodes--;

    // 토폴로지 정렬 무효화
    graph->is_sorted = false;

    return ET_SUCCESS;
}

// =============================================================================
// 노드 생성 및 관리 함수
// =============================================================================

ETNode* et_create_node(const char* name, const char* op_type, ETMemoryPool* mem_pool) {
    if (!name || !op_type) {
        return NULL;
    }

    ETNode* node = (ETNode*)malloc(sizeof(ETNode));
    if (!node) {
        return NULL;
    }

    // 이름 복사
    node->name = (char*)malloc(strlen(name) + 1);
    if (!node->name) {
        free(node);
        return NULL;
    }
    strcpy(node->name, name);

    // 연산자 타입 복사
    node->op_type = (char*)malloc(strlen(op_type) + 1);
    if (!node->op_type) {
        free(node->name);
        free(node);
        return NULL;
    }
    strcpy(node->op_type, op_type);

    // 텐서 배열 초기화
    node->inputs = NULL;
    node->num_inputs = 0;
    node->outputs = NULL;
    node->num_outputs = 0;

    // 노드 연결 배열 초기화
    node->input_nodes = NULL;
    node->num_input_nodes = 0;
    node->output_nodes = NULL;
    node->num_output_nodes = 0;

    // 연산자 속성 및 함수 초기화
    node->attributes = NULL;
    node->forward = NULL;
    node->backward = NULL;
    node->destroy_attributes = NULL;

    // 실행 상태 초기화
    node->state = ET_NODE_READY;
    node->execution_order = -1;
    node->is_input_node = false;
    node->is_output_node = false;

    // 메모리 풀 설정
    node->mem_pool = mem_pool;

    return node;
}

void et_destroy_node(ETNode* node) {
    if (!node) return;

    // 속성 해제
    if (node->attributes && node->destroy_attributes) {
        node->destroy_attributes(node->attributes);
    }

    // 텐서 배열 해제 (텐서 자체는 해제하지 않음 - 소유권이 다른 곳에 있을 수 있음)
    free(node->inputs);
    free(node->outputs);

    // 노드 연결 배열 해제
    free(node->input_nodes);
    free(node->output_nodes);

    // 문자열 해제
    free(node->name);
    free(node->op_type);

    free(node);
}

int et_connect_nodes(ETNode* src, ETNode* dst) {
    if (!src || !dst) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 이미 연결되어 있는지 확인
    for (size_t i = 0; i < src->num_output_nodes; i++) {
        if (src->output_nodes[i] == dst) {
            return ET_SUCCESS; // 이미 연결됨
        }
    }

    // src의 출력 노드 배열에 dst 추가
    int result = resize_node_connections(src, false, src->num_output_nodes + 1);
    if (result != ET_SUCCESS) {
        return result;
    }
    src->output_nodes[src->num_output_nodes] = dst;
    src->num_output_nodes++;

    // dst의 입력 노드 배열에 src 추가
    result = resize_node_connections(dst, true, dst->num_input_nodes + 1);
    if (result != ET_SUCCESS) {
        // 롤백: src에서 dst 제거
        src->num_output_nodes--;
        return result;
    }
    dst->input_nodes[dst->num_input_nodes] = src;
    dst->num_input_nodes++;

    return ET_SUCCESS;
}

int et_disconnect_nodes(ETNode* src, ETNode* dst) {
    if (!src || !dst) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // src의 출력 노드 배열에서 dst 제거
    bool found = false;
    for (size_t i = 0; i < src->num_output_nodes; i++) {
        if (src->output_nodes[i] == dst) {
            // 뒤의 요소들을 앞으로 이동
            for (size_t j = i; j < src->num_output_nodes - 1; j++) {
                src->output_nodes[j] = src->output_nodes[j + 1];
            }
            src->num_output_nodes--;
            found = true;
            break;
        }
    }

    if (!found) {
        return ET_ERROR_INVALID_ARGUMENT; // 연결이 존재하지 않음
    }

    // dst의 입력 노드 배열에서 src 제거
    for (size_t i = 0; i < dst->num_input_nodes; i++) {
        if (dst->input_nodes[i] == src) {
            // 뒤의 요소들을 앞으로 이동
            for (size_t j = i; j < dst->num_input_nodes - 1; j++) {
                dst->input_nodes[j] = dst->input_nodes[j + 1];
            }
            dst->num_input_nodes--;
            break;
        }
    }

    return ET_SUCCESS;
}

// =============================================================================
// 그래프 순회 및 실행 함수
// =============================================================================

int et_topological_sort(ETGraph* graph) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (graph->num_nodes == 0) {
        graph->is_sorted = true;
        return ET_SUCCESS;
    }

    // 실행 순서 배열 할당
    if (graph->execution_order) {
        free(graph->execution_order);
    }
    graph->execution_order = (ETNode**)malloc(graph->num_nodes * sizeof(ETNode*));
    if (!graph->execution_order) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // DFS 방문 상태: 0=미방문, 1=방문중, 2=방문완료
    int* visit_state = (int*)calloc(graph->num_nodes, sizeof(int));
    if (!visit_state) {
        free(graph->execution_order);
        graph->execution_order = NULL;
        return ET_ERROR_OUT_OF_MEMORY;
    }

    int index = graph->num_nodes - 1; // 역순으로 채움

    // 모든 노드에 대해 DFS 수행
    for (size_t i = 0; i < graph->num_nodes; i++) {
        if (visit_state[i] == 0) {
            int result = dfs_visit(graph->nodes[i], visit_state, graph->execution_order, &index, graph);
            if (result != ET_SUCCESS) {
                free(visit_state);
                free(graph->execution_order);
                graph->execution_order = NULL;
                return result; // 순환 참조 발견
            }
        }
    }

    free(visit_state);

    // 실행 순서 설정
    for (size_t i = 0; i < graph->num_nodes; i++) {
        graph->execution_order[i]->execution_order = (int)i;
    }

    graph->execution_order_size = graph->num_nodes;
    graph->is_sorted = true;

    return ET_SUCCESS;
}

int et_execute_graph(ETGraph* graph, ETTensor** inputs, ETTensor** outputs) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 토폴로지 정렬이 필요한 경우 수행
    if (!graph->is_sorted) {
        int result = et_topological_sort(graph);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 노드 상태 초기화
    reset_node_states(graph);

    // 입력 텐서 설정 (입력 노드들에 텐서 할당)
    if (inputs && graph->input_nodes) {
        for (size_t i = 0; i < graph->num_input_nodes; i++) {
            ETNode* input_node = graph->input_nodes[i];
            if (input_node->num_outputs > 0 && i < graph->num_input_nodes) {
                input_node->outputs[0] = inputs[i];
            }
        }
    }

    // 실행 순서에 따라 노드들 실행
    for (size_t i = 0; i < graph->execution_order_size; i++) {
        ETNode* node = graph->execution_order[i];

        // 입력 노드는 이미 텐서가 설정되어 있으므로 건너뜀
        if (node->is_input_node) {
            node->state = ET_NODE_COMPLETED;
            continue;
        }

        // 노드 실행
        if (node->forward) {
            node->state = ET_NODE_RUNNING;
            node->forward(node);
            node->state = ET_NODE_COMPLETED;
        } else {
            // 순방향 함수가 없는 경우 오류
            node->state = ET_NODE_ERROR;
            return ET_ERROR_RUNTIME;
        }
    }

    // 출력 텐서 설정
    if (outputs && graph->output_nodes) {
        for (size_t i = 0; i < graph->num_output_nodes; i++) {
            ETNode* output_node = graph->output_nodes[i];
            if (output_node->num_outputs > 0 && i < graph->num_output_nodes) {
                outputs[i] = output_node->outputs[0];
            }
        }
    }

    return ET_SUCCESS;
}

int et_execute_until_node(ETGraph* graph, ETNode* target_node, ETTensor** inputs) {
    if (!graph || !target_node) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 토폴로지 정렬이 필요한 경우 수행
    if (!graph->is_sorted) {
        int result = et_topological_sort(graph);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 노드 상태 초기화
    reset_node_states(graph);

    // 입력 텐서 설정
    if (inputs && graph->input_nodes) {
        for (size_t i = 0; i < graph->num_input_nodes; i++) {
            ETNode* input_node = graph->input_nodes[i];
            if (input_node->num_outputs > 0 && i < graph->num_input_nodes) {
                input_node->outputs[0] = inputs[i];
            }
        }
    }

    // 목표 노드까지만 실행
    for (size_t i = 0; i < graph->execution_order_size; i++) {
        ETNode* node = graph->execution_order[i];

        // 입력 노드는 이미 텐서가 설정되어 있으므로 건너뜀
        if (node->is_input_node) {
            node->state = ET_NODE_COMPLETED;
        } else if (node->forward) {
            node->state = ET_NODE_RUNNING;
            node->forward(node);
            node->state = ET_NODE_COMPLETED;
        } else {
            node->state = ET_NODE_ERROR;
            return ET_ERROR_RUNTIME;
        }

        // 목표 노드에 도달하면 중단
        if (node == target_node) {
            break;
        }
    }

    return ET_SUCCESS;
}

// =============================================================================
// 내부 함수 구현
// =============================================================================

static int resize_nodes_array(ETGraph* graph, size_t new_capacity) {
    if (new_capacity <= graph->nodes_capacity) {
        return ET_SUCCESS;
    }

    ETNode** new_nodes = (ETNode**)realloc(graph->nodes, new_capacity * sizeof(ETNode*));
    if (!new_nodes) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 새로 할당된 부분을 NULL로 초기화
    for (size_t i = graph->nodes_capacity; i < new_capacity; i++) {
        new_nodes[i] = NULL;
    }

    graph->nodes = new_nodes;
    graph->nodes_capacity = new_capacity;

    return ET_SUCCESS;
}

static int resize_node_connections(ETNode* node, bool is_input, size_t new_capacity) {
    if (is_input) {
        if (new_capacity <= node->num_input_nodes) {
            return ET_SUCCESS;
        }

        ETNode** new_connections = (ETNode**)realloc(node->input_nodes, new_capacity * sizeof(ETNode*));
        if (!new_connections) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
        node->input_nodes = new_connections;
    } else {
        if (new_capacity <= node->num_output_nodes) {
            return ET_SUCCESS;
        }

        ETNode** new_connections = (ETNode**)realloc(node->output_nodes, new_capacity * sizeof(ETNode*));
        if (!new_connections) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
        node->output_nodes = new_connections;
    }

    return ET_SUCCESS;
}

static void reset_node_states(ETGraph* graph) {
    for (size_t i = 0; i < graph->num_nodes; i++) {
        graph->nodes[i]->state = ET_NODE_READY;
    }
}

static int dfs_visit(ETNode* node, int* visit_state, ETNode** sorted_nodes, int* index, ETGraph* graph) {
    // 노드 인덱스 찾기
    int node_index = -1;
    for (size_t i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i] == node) {
            node_index = (int)i;
            break;
        }
    }

    if (node_index == -1) {
        return ET_ERROR_RUNTIME; // 노드를 찾을 수 없음
    }

    if (visit_state[node_index] == 1) { // 방문 중인 노드를 다시 방문 = 순환 참조
        return ET_ERROR_RUNTIME;
    }

    if (visit_state[node_index] == 2) { // 이미 방문 완료
        return ET_SUCCESS;
    }

    visit_state[node_index] = 1; // 방문 중 표시

    // 출력 노드들을 재귀적으로 방문
    for (size_t i = 0; i < node->num_output_nodes; i++) {
        int result = dfs_visit(node->output_nodes[i], visit_state, sorted_nodes, index, graph);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    visit_state[node_index] = 2; // 방문 완료 표시
    sorted_nodes[*index] = node;
    (*index)--;

    return ET_SUCCESS;
}