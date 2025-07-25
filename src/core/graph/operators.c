#include "libetude/graph.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 내부 상수 정의
#define DEFAULT_REGISTRY_CAPACITY 64

// 에러 코드 매핑 (호환성을 위해)
#define ET_SUCCESS LIBETUDE_SUCCESS
#define ET_ERROR_INVALID_ARGUMENT LIBETUDE_ERROR_INVALID_ARGUMENT
#define ET_ERROR_OUT_OF_MEMORY LIBETUDE_ERROR_OUT_OF_MEMORY
#define ET_ERROR_RUNTIME LIBETUDE_ERROR_RUNTIME

// 내부 함수 선언
static bool dfs_cycle_check(ETNode* node, int* visit_state, ETNode** node_map, size_t num_nodes);
static int optimize_operator_fusion(ETGraph* graph);
static int optimize_dead_code_elimination(ETGraph* graph);
static int optimize_memory_access(ETGraph* graph);

// =============================================================================
// 연산자 레지스트리 함수
// =============================================================================

ETOperatorRegistry* et_create_operator_registry(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = DEFAULT_REGISTRY_CAPACITY;
    }

    ETOperatorRegistry* registry = (ETOperatorRegistry*)malloc(sizeof(ETOperatorRegistry));
    if (!registry) {
        return NULL;
    }

    registry->operators = (ETOperator*)calloc(initial_capacity, sizeof(ETOperator));
    if (!registry->operators) {
        free(registry);
        return NULL;
    }

    registry->num_operators = 0;
    registry->capacity = initial_capacity;

    return registry;
}

void et_destroy_operator_registry(ETOperatorRegistry* registry) {
    if (!registry) return;

    // 등록된 연산자들의 이름 문자열 해제
    for (size_t i = 0; i < registry->num_operators; i++) {
        free(registry->operators[i].name);
    }

    free(registry->operators);
    free(registry);
}

int et_register_operator(ETOperatorRegistry* registry, ETOperator* op) {
    if (!registry || !op || !op->name) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 이미 등록된 연산자인지 확인
    for (size_t i = 0; i < registry->num_operators; i++) {
        if (strcmp(registry->operators[i].name, op->name) == 0) {
            return ET_ERROR_INVALID_ARGUMENT; // 이미 등록됨
        }
    }

    // 용량 확장이 필요한 경우
    if (registry->num_operators >= registry->capacity) {
        size_t new_capacity = registry->capacity * 2;
        ETOperator* new_operators = (ETOperator*)realloc(registry->operators,
                                                        new_capacity * sizeof(ETOperator));
        if (!new_operators) {
            return ET_ERROR_OUT_OF_MEMORY;
        }

        // 새로 할당된 부분을 0으로 초기화
        memset(&new_operators[registry->capacity], 0,
               (new_capacity - registry->capacity) * sizeof(ETOperator));

        registry->operators = new_operators;
        registry->capacity = new_capacity;
    }

    // 연산자 등록
    ETOperator* slot = &registry->operators[registry->num_operators];

    // 이름 복사
    slot->name = (char*)malloc(strlen(op->name) + 1);
    if (!slot->name) {
        return ET_ERROR_OUT_OF_MEMORY;
    }
    strcpy(slot->name, op->name);

    // 함수 포인터들 복사
    slot->create = op->create;
    slot->forward = op->forward;
    slot->backward = op->backward;
    slot->destroy = op->destroy;

    registry->num_operators++;

    return ET_SUCCESS;
}

ETOperator* et_find_operator(ETOperatorRegistry* registry, const char* name) {
    if (!registry || !name) {
        return NULL;
    }

    for (size_t i = 0; i < registry->num_operators; i++) {
        if (strcmp(registry->operators[i].name, name) == 0) {
            return &registry->operators[i];
        }
    }

    return NULL;
}

// =============================================================================
// 유틸리티 함수
// =============================================================================

ETNode* et_find_node_by_name(ETGraph* graph, const char* name) {
    if (!graph || !name) {
        return NULL;
    }

    for (size_t i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i]->name && strcmp(graph->nodes[i]->name, name) == 0) {
            return graph->nodes[i];
        }
    }

    return NULL;
}

bool et_has_cycle(ETGraph* graph) {
    if (!graph || graph->num_nodes == 0) {
        return false;
    }

    // DFS 방문 상태: 0=미방문, 1=방문중, 2=방문완료
    int* visit_state = (int*)calloc(graph->num_nodes, sizeof(int));
    if (!visit_state) {
        return true; // 메모리 할당 실패시 안전하게 true 반환
    }

    // 노드 인덱스 매핑을 위한 간단한 구현
    // 실제로는 해시맵을 사용하는 것이 좋음
    ETNode** node_map = (ETNode**)malloc(graph->num_nodes * sizeof(ETNode*));
    if (!node_map) {
        free(visit_state);
        return true;
    }

    for (size_t i = 0; i < graph->num_nodes; i++) {
        node_map[i] = graph->nodes[i];
    }

    bool has_cycle = false;

    // 모든 노드에 대해 DFS 수행
    for (size_t i = 0; i < graph->num_nodes && !has_cycle; i++) {
        if (visit_state[i] == 0) {
            has_cycle = dfs_cycle_check(graph->nodes[i], visit_state, node_map, graph->num_nodes);
        }
    }

    free(visit_state);
    free(node_map);

    return has_cycle;
}

void et_print_graph_info(ETGraph* graph) {
    if (!graph) {
        printf("Graph: NULL\n");
        return;
    }

    printf("=== Graph Information ===\n");
    printf("Name: %s\n", graph->name ? graph->name : "Unnamed");
    printf("Nodes: %zu/%zu\n", graph->num_nodes, graph->nodes_capacity);
    printf("Input nodes: %zu\n", graph->num_input_nodes);
    printf("Output nodes: %zu\n", graph->num_output_nodes);
    printf("Is sorted: %s\n", graph->is_sorted ? "Yes" : "No");
    printf("Is optimized: %s\n", graph->is_optimized ? "Yes" : "No");

    printf("\n--- Nodes ---\n");
    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];
        printf("Node[%zu]: %s (%s)\n", i, node->name, node->op_type);
        printf("  Inputs: %zu, Outputs: %zu\n", node->num_inputs, node->num_outputs);
        printf("  Input nodes: %zu, Output nodes: %zu\n",
               node->num_input_nodes, node->num_output_nodes);
        printf("  State: %d, Execution order: %d\n",
               node->state, node->execution_order);
        printf("  Is input: %s, Is output: %s\n",
               node->is_input_node ? "Yes" : "No",
               node->is_output_node ? "Yes" : "No");

        // 연결된 노드들 출력
        if (node->num_input_nodes > 0) {
            printf("  Connected from: ");
            for (size_t j = 0; j < node->num_input_nodes; j++) {
                printf("%s", node->input_nodes[j]->name);
                if (j < node->num_input_nodes - 1) printf(", ");
            }
            printf("\n");
        }

        if (node->num_output_nodes > 0) {
            printf("  Connected to: ");
            for (size_t j = 0; j < node->num_output_nodes; j++) {
                printf("%s", node->output_nodes[j]->name);
                if (j < node->num_output_nodes - 1) printf(", ");
            }
            printf("\n");
        }
        printf("\n");
    }

    if (graph->is_sorted && graph->execution_order) {
        printf("--- Execution Order ---\n");
        for (size_t i = 0; i < graph->execution_order_size; i++) {
            printf("%zu: %s\n", i, graph->execution_order[i]->name);
        }
    }

    printf("========================\n");
}

// =============================================================================
// 그래프 최적화 함수
// =============================================================================

int et_optimize_graph(ETGraph* graph, ETOptimizationFlags flags) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int result = ET_SUCCESS;

    // 연산자 융합 최적화
    if (flags & ET_OPT_OPERATOR_FUSION) {
        result = optimize_operator_fusion(graph);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 불필요한 연산 제거
    if (flags & ET_OPT_DEAD_CODE_ELIMINATION) {
        result = optimize_dead_code_elimination(graph);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 메모리 접근 최적화
    if (flags & ET_OPT_MEMORY_OPTIMIZATION) {
        result = optimize_memory_access(graph);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 최적화 완료 표시
    graph->is_optimized = true;

    // 토폴로지 정렬 무효화 (최적화로 인해 그래프 구조가 변경될 수 있음)
    graph->is_sorted = false;

    return ET_SUCCESS;
}

// =============================================================================
// 내부 함수 구현
// =============================================================================

static bool dfs_cycle_check(ETNode* node, int* visit_state, ETNode** node_map, size_t num_nodes) {
    // 노드 인덱스 찾기
    int node_index = -1;
    for (size_t i = 0; i < num_nodes; i++) {
        if (node_map[i] == node) {
            node_index = (int)i;
            break;
        }
    }

    if (node_index == -1) {
        return false; // 노드를 찾을 수 없음
    }

    if (visit_state[node_index] == 1) {
        return true; // 순환 참조 발견
    }

    if (visit_state[node_index] == 2) {
        return false; // 이미 방문 완료
    }

    visit_state[node_index] = 1; // 방문 중 표시

    // 출력 노드들을 재귀적으로 확인
    for (size_t i = 0; i < node->num_output_nodes; i++) {
        if (dfs_cycle_check(node->output_nodes[i], visit_state, node_map, num_nodes)) {
            return true;
        }
    }

    visit_state[node_index] = 2; // 방문 완료 표시
    return false;
}

static int optimize_operator_fusion(ETGraph* graph) {
    // 연산자 융합 최적화 구현
    // 예: Conv + BatchNorm + ReLU 융합
    // 현재는 기본 구현만 제공
    (void)graph; // 미사용 매개변수 경고 방지
    return ET_SUCCESS;
}

static int optimize_dead_code_elimination(ETGraph* graph) {
    // 불필요한 연산 제거 최적화 구현
    // 출력에 영향을 주지 않는 노드들을 제거
    (void)graph; // 미사용 매개변수 경고 방지
    return ET_SUCCESS;
}

static int optimize_memory_access(ETGraph* graph) {
    // 메모리 접근 최적화 구현
    // 메모리 지역성을 고려한 노드 재배치 등
    (void)graph; // 미사용 매개변수 경고 방지
    return ET_SUCCESS;
}

