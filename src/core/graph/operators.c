#include "libetude/graph.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// 연산자 융합 헬퍼 함수들
static int fuse_linear_relu(ETGraph* graph, ETNode* linear_node, ETNode* relu_node);
static int fuse_conv1d_relu(ETGraph* graph, ETNode* conv_node, ETNode* relu_node);
static int fuse_stft_melscale(ETGraph* graph, ETNode* stft_node, ETNode* mel_node);

// 불필요한 연산 제거 헬퍼 함수들
static void mark_reachable_nodes(ETGraph* graph, ETNode* node, bool* reachable);

// 메모리 접근 최적화 헬퍼 함수들
static void optimize_inplace_operations(ETGraph* graph);
static void optimize_memory_reuse(ETGraph* graph);
static void optimize_tensor_lifetime(ETGraph* graph);

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
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    bool fusion_occurred = true;
    int fusion_count = 0;

    // 융합 가능한 패턴을 찾을 때까지 반복
    while (fusion_occurred && fusion_count < 10) { // 최대 10회 반복으로 무한루프 방지
        fusion_occurred = false;

        // 모든 노드에 대해 융합 가능한 패턴 검사
        for (size_t i = 0; i < graph->num_nodes && !fusion_occurred; i++) {
            ETNode* node = graph->nodes[i];

            // Linear + ReLU 융합 패턴 검사
            if (strcmp(node->op_type, "Linear") == 0 && node->num_output_nodes == 1) {
                ETNode* next_node = node->output_nodes[0];

                if (strcmp(next_node->op_type, "ReLU") == 0 && next_node->num_input_nodes == 1) {
                    // Linear + ReLU 융합 수행
                    if (fuse_linear_relu(graph, node, next_node) == ET_SUCCESS) {
                        fusion_occurred = true;
                        fusion_count++;
                        break;
                    }
                }
            }

            // Conv1D + ReLU 융합 패턴 검사
            if (strcmp(node->op_type, "Conv1D") == 0 && node->num_output_nodes == 1) {
                ETNode* next_node = node->output_nodes[0];

                if (strcmp(next_node->op_type, "ReLU") == 0 && next_node->num_input_nodes == 1) {
                    // Conv1D + ReLU 융합 수행
                    if (fuse_conv1d_relu(graph, node, next_node) == ET_SUCCESS) {
                        fusion_occurred = true;
                        fusion_count++;
                        break;
                    }
                }
            }

            // STFT + MelScale 융합 패턴 검사 (음성 특화)
            if (strcmp(node->op_type, "STFT") == 0 && node->num_output_nodes == 1) {
                ETNode* next_node = node->output_nodes[0];

                if (strcmp(next_node->op_type, "MelScale") == 0 && next_node->num_input_nodes == 1) {
                    // STFT + MelScale 융합 수행
                    if (fuse_stft_melscale(graph, node, next_node) == ET_SUCCESS) {
                        fusion_occurred = true;
                        fusion_count++;
                        break;
                    }
                }
            }
        }
    }

    return ET_SUCCESS;
}

static int optimize_dead_code_elimination(ETGraph* graph) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 노드가 없으면 최적화할 것이 없음
    if (graph->num_nodes == 0) {
        return ET_SUCCESS;
    }

    // 출력 노드들로부터 역방향으로 도달 가능한 노드들을 마킹
    bool* reachable = (bool*)calloc(graph->num_nodes, sizeof(bool));
    if (!reachable) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 출력 노드들부터 시작하여 역방향 DFS 수행
    for (size_t i = 0; i < graph->num_output_nodes; i++) {
        ETNode* output_node = graph->output_nodes[i];
        mark_reachable_nodes(graph, output_node, reachable);
    }

    // 도달 불가능한 노드들을 제거할 목록에 추가
    ETNode** nodes_to_remove = (ETNode**)malloc(graph->num_nodes * sizeof(ETNode*));
    if (!nodes_to_remove) {
        free(reachable);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    size_t remove_count = 0;
    for (size_t i = 0; i < graph->num_nodes; i++) {
        if (!reachable[i]) {
            nodes_to_remove[remove_count++] = graph->nodes[i];
        }
    }

    // 도달 불가능한 노드들 제거
    for (size_t i = 0; i < remove_count; i++) {
        et_remove_node(graph, nodes_to_remove[i]);
        et_destroy_node(nodes_to_remove[i]);
    }

    free(reachable);
    free(nodes_to_remove);

    return ET_SUCCESS;
}

static int optimize_memory_access(ETGraph* graph) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 1. 인플레이스 연산 최적화
    optimize_inplace_operations(graph);

    // 2. 메모리 재사용 최적화
    optimize_memory_reuse(graph);

    // 3. 텐서 생명주기 분석 및 최적화
    optimize_tensor_lifetime(graph);

    return ET_SUCCESS;
}

// =============================================================================
// 기본 연산자 구현
// =============================================================================

// Linear 연산자 속성 구조체
typedef struct {
    size_t input_size;
    size_t output_size;
    ETTensor* weight;
    ETTensor* bias;
    bool use_bias;
} LinearAttributes;

// Conv1D 연산자 속성 구조체
typedef struct {
    size_t in_channels;
    size_t out_channels;
    size_t kernel_size;
    size_t stride;
    size_t padding;
    size_t dilation;
    ETTensor* weight;
    ETTensor* bias;
    bool use_bias;
} Conv1DAttributes;

// Attention 연산자 속성 구조체
typedef struct {
    size_t embed_dim;
    size_t num_heads;
    size_t head_dim;
    ETTensor* q_weight;
    ETTensor* k_weight;
    ETTensor* v_weight;
    ETTensor* out_weight;
    ETTensor* q_bias;
    ETTensor* k_bias;
    ETTensor* v_bias;
    ETTensor* out_bias;
    bool use_bias;
    float dropout_rate;
} AttentionAttributes;

// STFT 연산자 속성 구조체 (음성 특화)
typedef struct {
    size_t n_fft;
    size_t hop_length;
    size_t win_length;
    ETTensor* window;
    bool center;
    bool normalized;
} STFTAttributes;

// Mel Scale 연산자 속성 구조체 (음성 특화)
typedef struct {
    size_t n_mels;
    size_t n_fft;
    size_t sample_rate;
    float f_min;
    float f_max;
    ETTensor* mel_filters;
    bool htk;
    bool norm;
} MelScaleAttributes;

// Vocoder 연산자 속성 구조체 (음성 특화)
typedef struct {
    size_t mel_channels;
    size_t upsample_factor;
    size_t sample_rate;
    ETTensor* upsample_weights;
    ETTensor* conv_weights;
} VocoderAttributes;

// =============================================================================
// Linear 연산자 구현
// =============================================================================

static void linear_create(ETNode* node, void* attributes) {
    if (!node || !attributes) return;

    LinearAttributes* attrs = (LinearAttributes*)attributes;

    // 속성 복사
    LinearAttributes* node_attrs = (LinearAttributes*)malloc(sizeof(LinearAttributes));
    if (!node_attrs) return;

    memcpy(node_attrs, attrs, sizeof(LinearAttributes));
    node->attributes = node_attrs;

    // 입력/출력 텐서 배열 할당
    node->inputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->outputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->num_inputs = 1;
    node->num_outputs = 1;

    // 출력 텐서 생성 (크기는 실행 시 결정)
    node->outputs[0] = NULL;
}

static void linear_forward(ETNode* node) {
    if (!node || !node->attributes || !node->inputs || !node->outputs) return;

    LinearAttributes* attrs = (LinearAttributes*)node->attributes;
    ETTensor* input = node->inputs[0];

    if (!input || !attrs->weight) return;

    // 출력 텐서가 없으면 생성
    if (!node->outputs[0]) {
        size_t output_shape[2] = {input->shape[0], attrs->output_size};
        node->outputs[0] = et_create_tensor(node->mem_pool, input->dtype, 2, output_shape);
        if (!node->outputs[0]) return;
    }

    ETTensor* output = node->outputs[0];

    // Linear 연산: output = input * weight^T + bias
    // 행렬 곱셈 수행
    et_matmul(input, attrs->weight, output, NULL);

    // 바이어스 추가 (사용하는 경우)
    if (attrs->use_bias && attrs->bias) {
        et_add(output, attrs->bias, output, NULL);
    }
}

static void linear_destroy(ETNode* node) {
    if (!node || !node->attributes) return;

    LinearAttributes* attrs = (LinearAttributes*)node->attributes;

    // 가중치와 바이어스는 외부에서 관리되므로 해제하지 않음
    free(attrs);
    node->attributes = NULL;
}

// =============================================================================
// Conv1D 연산자 구현
// =============================================================================

static void conv1d_create(ETNode* node, void* attributes) {
    if (!node || !attributes) return;

    Conv1DAttributes* attrs = (Conv1DAttributes*)attributes;

    // 속성 복사
    Conv1DAttributes* node_attrs = (Conv1DAttributes*)malloc(sizeof(Conv1DAttributes));
    if (!node_attrs) return;

    memcpy(node_attrs, attrs, sizeof(Conv1DAttributes));
    node->attributes = node_attrs;

    // 입력/출력 텐서 배열 할당
    node->inputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->outputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->num_inputs = 1;
    node->num_outputs = 1;

    node->outputs[0] = NULL;
}

static void conv1d_forward(ETNode* node) {
    if (!node || !node->attributes || !node->inputs || !node->outputs) return;

    Conv1DAttributes* attrs = (Conv1DAttributes*)node->attributes;
    ETTensor* input = node->inputs[0];

    if (!input || !attrs->weight) return;

    // 출력 크기 계산
    size_t input_length = input->shape[2]; // [batch, in_channels, length]
    size_t output_length = (input_length + 2 * attrs->padding - attrs->dilation * (attrs->kernel_size - 1) - 1) / attrs->stride + 1;

    // 출력 텐서가 없으면 생성
    if (!node->outputs[0]) {
        size_t output_shape[3] = {input->shape[0], attrs->out_channels, output_length};
        node->outputs[0] = et_create_tensor(node->mem_pool, input->dtype, 3, output_shape);
        if (!node->outputs[0]) return;
    }

    ETTensor* output = node->outputs[0];

    // Conv1D 연산 수행 (간단한 구현)
    // 실제로는 최적화된 convolution 알고리즘을 사용해야 함
    float* input_data = (float*)input->data;
    float* weight_data = (float*)attrs->weight->data;
    float* output_data = (float*)output->data;

    size_t batch_size = input->shape[0];

    // 배치별로 처리
    for (size_t b = 0; b < batch_size; b++) {
        for (size_t oc = 0; oc < attrs->out_channels; oc++) {
            for (size_t ol = 0; ol < output_length; ol++) {
                float sum = 0.0f;

                // 커널 적용
                for (size_t ic = 0; ic < attrs->in_channels; ic++) {
                    for (size_t k = 0; k < attrs->kernel_size; k++) {
                        int input_pos = (int)(ol * attrs->stride + k * attrs->dilation) - (int)attrs->padding;

                        if (input_pos >= 0 && input_pos < (int)input_length) {
                            size_t input_idx = b * attrs->in_channels * input_length + ic * input_length + input_pos;
                            size_t weight_idx = oc * attrs->in_channels * attrs->kernel_size + ic * attrs->kernel_size + k;

                            sum += input_data[input_idx] * weight_data[weight_idx];
                        }
                    }
                }

                // 바이어스 추가
                if (attrs->use_bias && attrs->bias) {
                    float* bias_data = (float*)attrs->bias->data;
                    sum += bias_data[oc];
                }

                size_t output_idx = b * attrs->out_channels * output_length + oc * output_length + ol;
                output_data[output_idx] = sum;
            }
        }
    }
}

static void conv1d_destroy(ETNode* node) {
    if (!node || !node->attributes) return;

    Conv1DAttributes* attrs = (Conv1DAttributes*)node->attributes;
    free(attrs);
    node->attributes = NULL;
}

// =============================================================================
// Attention 연산자 구현
// =============================================================================

static void attention_create(ETNode* node, void* attributes) {
    if (!node || !attributes) return;

    AttentionAttributes* attrs = (AttentionAttributes*)attributes;

    // 속성 복사
    AttentionAttributes* node_attrs = (AttentionAttributes*)malloc(sizeof(AttentionAttributes));
    if (!node_attrs) return;

    memcpy(node_attrs, attrs, sizeof(AttentionAttributes));
    node->attributes = node_attrs;

    // 입력/출력 텐서 배열 할당 (query, key, value 입력)
    node->inputs = (ETTensor**)malloc(3 * sizeof(ETTensor*));
    node->outputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->num_inputs = 3;
    node->num_outputs = 1;

    node->outputs[0] = NULL;
}

static void attention_forward(ETNode* node) {
    if (!node || !node->attributes || !node->inputs || !node->outputs) return;

    AttentionAttributes* attrs = (AttentionAttributes*)node->attributes;
    ETTensor* query = node->inputs[0];
    ETTensor* key = node->inputs[1];
    ETTensor* value = node->inputs[2];

    if (!query || !key || !value) return;

    size_t batch_size = query->shape[0];
    size_t seq_len = query->shape[1];

    // 출력 텐서가 없으면 생성
    if (!node->outputs[0]) {
        size_t output_shape[3] = {batch_size, seq_len, attrs->embed_dim};
        node->outputs[0] = et_create_tensor(node->mem_pool, query->dtype, 3, output_shape);
        if (!node->outputs[0]) return;
    }

    ETTensor* output = node->outputs[0];

    // Multi-Head Attention 연산 수행
    // 1. Query, Key, Value 변환
    // 2. Scaled Dot-Product Attention 계산
    // 3. Multi-head 결합
    // 4. 출력 변환

    // 간단한 구현 (실제로는 더 최적화된 구현 필요)
    float* query_data = (float*)query->data;
    float* key_data = (float*)key->data;
    float* value_data = (float*)value->data;
    float* output_data = (float*)output->data;

    // 스케일 팩터
    float scale = 1.0f / sqrtf((float)attrs->head_dim);

    // 간단한 attention 계산 (단일 헤드로 근사)
    for (size_t b = 0; b < batch_size; b++) {
        for (size_t i = 0; i < seq_len; i++) {
            for (size_t d = 0; d < attrs->embed_dim; d++) {
                float sum = 0.0f;
                float attention_sum = 0.0f;

                // Attention weights 계산
                for (size_t j = 0; j < seq_len; j++) {
                    float attention_score = 0.0f;

                    // Query와 Key의 내적
                    for (size_t k = 0; k < attrs->embed_dim; k++) {
                        size_t q_idx = b * seq_len * attrs->embed_dim + i * attrs->embed_dim + k;
                        size_t k_idx = b * seq_len * attrs->embed_dim + j * attrs->embed_dim + k;
                        attention_score += query_data[q_idx] * key_data[k_idx];
                    }

                    attention_score *= scale;
                    float attention_weight = expf(attention_score);
                    attention_sum += attention_weight;

                    // Value와 가중합
                    size_t v_idx = b * seq_len * attrs->embed_dim + j * attrs->embed_dim + d;
                    sum += attention_weight * value_data[v_idx];
                }

                // 정규화
                if (attention_sum > 0.0f) {
                    sum /= attention_sum;
                }

                size_t out_idx = b * seq_len * attrs->embed_dim + i * attrs->embed_dim + d;
                output_data[out_idx] = sum;
            }
        }
    }
}

static void attention_destroy(ETNode* node) {
    if (!node || !node->attributes) return;

    AttentionAttributes* attrs = (AttentionAttributes*)node->attributes;
    free(attrs);
    node->attributes = NULL;
}

// =============================================================================
// 음성 특화 연산자 구현 - STFT
// =============================================================================

static void stft_create(ETNode* node, void* attributes) {
    if (!node || !attributes) return;

    STFTAttributes* attrs = (STFTAttributes*)attributes;

    // 속성 복사
    STFTAttributes* node_attrs = (STFTAttributes*)malloc(sizeof(STFTAttributes));
    if (!node_attrs) return;

    memcpy(node_attrs, attrs, sizeof(STFTAttributes));
    node->attributes = node_attrs;

    // 입력/출력 텐서 배열 할당
    node->inputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->outputs = (ETTensor**)malloc(2 * sizeof(ETTensor*)); // magnitude, phase
    node->num_inputs = 1;
    node->num_outputs = 2;

    node->outputs[0] = NULL; // magnitude
    node->outputs[1] = NULL; // phase
}

static void stft_forward(ETNode* node) {
    if (!node || !node->attributes || !node->inputs || !node->outputs) return;

    STFTAttributes* attrs = (STFTAttributes*)node->attributes;
    ETTensor* input = node->inputs[0]; // 오디오 신호

    if (!input) return;

    size_t batch_size = input->shape[0];
    size_t signal_length = input->shape[1];

    // 출력 크기 계산
    size_t n_frames = (signal_length - attrs->n_fft) / attrs->hop_length + 1;
    size_t n_bins = attrs->n_fft / 2 + 1;

    // 출력 텐서가 없으면 생성
    if (!node->outputs[0]) {
        size_t mag_shape[3] = {batch_size, n_frames, n_bins};
        node->outputs[0] = et_create_tensor(node->mem_pool, input->dtype, 3, mag_shape);
        if (!node->outputs[0]) return;
    }

    if (!node->outputs[1]) {
        size_t phase_shape[3] = {batch_size, n_frames, n_bins};
        node->outputs[1] = et_create_tensor(node->mem_pool, input->dtype, 3, phase_shape);
        if (!node->outputs[1]) return;
    }

    // STFT 연산 수행 (간단한 구현)
    // 실제로는 FFT 라이브러리를 사용해야 함
    float* input_data = (float*)input->data;
    float* mag_data = (float*)node->outputs[0]->data;
    float* phase_data = (float*)node->outputs[1]->data;
    float* window_data = attrs->window ? (float*)attrs->window->data : NULL;

    for (size_t b = 0; b < batch_size; b++) {
        for (size_t frame = 0; frame < n_frames; frame++) {
            size_t start_pos = frame * attrs->hop_length;

            // 윈도우 적용 및 FFT (간단한 DFT로 근사)
            for (size_t bin = 0; bin < n_bins; bin++) {
                float real = 0.0f, imag = 0.0f;

                for (size_t n = 0; n < attrs->n_fft && (start_pos + n) < signal_length; n++) {
                    float sample = input_data[b * signal_length + start_pos + n];

                    // 윈도우 적용
                    if (window_data) {
                        sample *= window_data[n];
                    }

                    // DFT 계산
                    float angle = -2.0f * M_PI * bin * n / attrs->n_fft;
                    real += sample * cosf(angle);
                    imag += sample * sinf(angle);
                }

                // 크기와 위상 계산
                size_t idx = b * n_frames * n_bins + frame * n_bins + bin;
                mag_data[idx] = sqrtf(real * real + imag * imag);
                phase_data[idx] = atan2f(imag, real);
            }
        }
    }
}

static void stft_destroy(ETNode* node) {
    if (!node || !node->attributes) return;

    STFTAttributes* attrs = (STFTAttributes*)node->attributes;
    free(attrs);
    node->attributes = NULL;
}

// =============================================================================
// 음성 특화 연산자 구현 - Mel Scale
// =============================================================================

static void mel_scale_create(ETNode* node, void* attributes) {
    if (!node || !attributes) return;

    MelScaleAttributes* attrs = (MelScaleAttributes*)attributes;

    // 속성 복사
    MelScaleAttributes* node_attrs = (MelScaleAttributes*)malloc(sizeof(MelScaleAttributes));
    if (!node_attrs) return;

    memcpy(node_attrs, attrs, sizeof(MelScaleAttributes));
    node->attributes = node_attrs;

    // 입력/출력 텐서 배열 할당
    node->inputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->outputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->num_inputs = 1;
    node->num_outputs = 1;

    node->outputs[0] = NULL;
}

static void mel_scale_forward(ETNode* node) {
    if (!node || !node->attributes || !node->inputs || !node->outputs) return;

    MelScaleAttributes* attrs = (MelScaleAttributes*)node->attributes;
    ETTensor* input = node->inputs[0]; // 스펙트로그램

    if (!input || !attrs->mel_filters) return;

    size_t batch_size = input->shape[0];
    size_t n_frames = input->shape[1];
    size_t n_bins = input->shape[2];

    // 출력 텐서가 없으면 생성
    if (!node->outputs[0]) {
        size_t output_shape[3] = {batch_size, n_frames, attrs->n_mels};
        node->outputs[0] = et_create_tensor(node->mem_pool, input->dtype, 3, output_shape);
        if (!node->outputs[0]) return;
    }

    ETTensor* output = node->outputs[0];

    // Mel scale 변환 수행
    float* input_data = (float*)input->data;
    float* output_data = (float*)output->data;
    float* filter_data = (float*)attrs->mel_filters->data;

    for (size_t b = 0; b < batch_size; b++) {
        for (size_t frame = 0; frame < n_frames; frame++) {
            for (size_t mel = 0; mel < attrs->n_mels; mel++) {
                float sum = 0.0f;

                // Mel 필터 적용
                for (size_t bin = 0; bin < n_bins; bin++) {
                    size_t input_idx = b * n_frames * n_bins + frame * n_bins + bin;
                    size_t filter_idx = mel * n_bins + bin;

                    sum += input_data[input_idx] * filter_data[filter_idx];
                }

                size_t output_idx = b * n_frames * attrs->n_mels + frame * attrs->n_mels + mel;
                output_data[output_idx] = sum;
            }
        }
    }
}

static void mel_scale_destroy(ETNode* node) {
    if (!node || !node->attributes) return;

    MelScaleAttributes* attrs = (MelScaleAttributes*)node->attributes;
    free(attrs);
    node->attributes = NULL;
}

// =============================================================================
// 음성 특화 연산자 구현 - Vocoder
// =============================================================================

static void vocoder_create(ETNode* node, void* attributes) {
    if (!node || !attributes) return;

    VocoderAttributes* attrs = (VocoderAttributes*)attributes;

    // 속성 복사
    VocoderAttributes* node_attrs = (VocoderAttributes*)malloc(sizeof(VocoderAttributes));
    if (!node_attrs) return;

    memcpy(node_attrs, attrs, sizeof(VocoderAttributes));
    node->attributes = node_attrs;

    // 입력/출력 텐서 배열 할당
    node->inputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->outputs = (ETTensor**)malloc(sizeof(ETTensor*));
    node->num_inputs = 1;
    node->num_outputs = 1;

    node->outputs[0] = NULL;
}

static void vocoder_forward(ETNode* node) {
    if (!node || !node->attributes || !node->inputs || !node->outputs) return;

    VocoderAttributes* attrs = (VocoderAttributes*)node->attributes;
    ETTensor* input = node->inputs[0]; // Mel 스펙트로그램

    if (!input) return;

    size_t batch_size = input->shape[0];
    size_t n_frames = input->shape[1];
    size_t mel_channels = input->shape[2];

    // 출력 오디오 길이 계산
    size_t audio_length = n_frames * attrs->upsample_factor;

    // 출력 텐서가 없으면 생성
    if (!node->outputs[0]) {
        size_t output_shape[2] = {batch_size, audio_length};
        node->outputs[0] = et_create_tensor(node->mem_pool, input->dtype, 2, output_shape);
        if (!node->outputs[0]) return;
    }

    ETTensor* output = node->outputs[0];

    // Vocoder 연산 수행 (간단한 업샘플링 구현)
    float* input_data = (float*)input->data;
    float* output_data = (float*)output->data;

    for (size_t b = 0; b < batch_size; b++) {
        for (size_t frame = 0; frame < n_frames; frame++) {
            // 각 프레임을 업샘플링
            for (size_t up = 0; up < attrs->upsample_factor; up++) {
                float sample = 0.0f;

                // Mel 채널들의 가중합으로 오디오 샘플 생성
                for (size_t mel = 0; mel < mel_channels; mel++) {
                    size_t input_idx = b * n_frames * mel_channels + frame * mel_channels + mel;
                    sample += input_data[input_idx] * 0.1f; // 간단한 가중치
                }

                size_t output_idx = b * audio_length + frame * attrs->upsample_factor + up;
                output_data[output_idx] = sample;
            }
        }
    }
}

static void vocoder_destroy(ETNode* node) {
    if (!node || !node->attributes) return;

    VocoderAttributes* attrs = (VocoderAttributes*)node->attributes;
    free(attrs);
    node->attributes = NULL;
}

// =============================================================================
// 연산자 등록 함수들
// =============================================================================

/**
 * @brief 기본 연산자들을 레지스트리에 등록합니다
 * @param registry 대상 레지스트리
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_register_basic_operators(ETOperatorRegistry* registry) {
    if (!registry) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int result;

    // Linear 연산자 등록
    ETOperator linear_op = {
        .name = "Linear",
        .create = linear_create,
        .forward = linear_forward,
        .backward = NULL,
        .destroy = linear_destroy
    };
    result = et_register_operator(registry, &linear_op);
    if (result != ET_SUCCESS) return result;

    // Conv1D 연산자 등록
    ETOperator conv1d_op = {
        .name = "Conv1D",
        .create = conv1d_create,
        .forward = conv1d_forward,
        .backward = NULL,
        .destroy = conv1d_destroy
    };
    result = et_register_operator(registry, &conv1d_op);
    if (result != ET_SUCCESS) return result;

    // Attention 연산자 등록
    ETOperator attention_op = {
        .name = "Attention",
        .create = attention_create,
        .forward = attention_forward,
        .backward = NULL,
        .destroy = attention_destroy
    };
    result = et_register_operator(registry, &attention_op);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}



// =============================================================================
// 그래프 최적화 헬퍼 함수들
// =============================================================================

// 연산자 융합 헬퍼 함수들
static int fuse_linear_relu(ETGraph* graph, ETNode* linear_node, ETNode* relu_node) {
    if (!graph || !linear_node || !relu_node) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Linear 노드의 연산자 타입을 "LinearReLU"로 변경
    free(linear_node->op_type);
    linear_node->op_type = (char*)malloc(strlen("LinearReLU") + 1);
    if (!linear_node->op_type) {
        return ET_ERROR_OUT_OF_MEMORY;
    }
    strcpy(linear_node->op_type, "LinearReLU");

    // ReLU 노드의 출력 연결을 Linear 노드로 이전
    for (size_t i = 0; i < relu_node->num_output_nodes; i++) {
        ETNode* output_node = relu_node->output_nodes[i];

        // output_node의 입력에서 relu_node를 linear_node로 교체
        for (size_t j = 0; j < output_node->num_input_nodes; j++) {
            if (output_node->input_nodes[j] == relu_node) {
                output_node->input_nodes[j] = linear_node;
                break;
            }
        }
    }

    // Linear 노드의 출력 연결을 ReLU 노드의 출력으로 교체
    free(linear_node->output_nodes);
    linear_node->output_nodes = relu_node->output_nodes;
    linear_node->num_output_nodes = relu_node->num_output_nodes;

    // ReLU 노드의 출력 연결 정보를 무효화 (이중 해제 방지)
    relu_node->output_nodes = NULL;
    relu_node->num_output_nodes = 0;

    // ReLU 노드를 그래프에서 제거
    et_remove_node(graph, relu_node);
    et_destroy_node(relu_node);

    return ET_SUCCESS;
}

static int fuse_conv1d_relu(ETGraph* graph, ETNode* conv_node, ETNode* relu_node) {
    if (!graph || !conv_node || !relu_node) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Conv1D 노드의 연산자 타입을 "Conv1DReLU"로 변경
    free(conv_node->op_type);
    conv_node->op_type = (char*)malloc(strlen("Conv1DReLU") + 1);
    if (!conv_node->op_type) {
        return ET_ERROR_OUT_OF_MEMORY;
    }
    strcpy(conv_node->op_type, "Conv1DReLU");

    // ReLU 노드의 출력 연결을 Conv1D 노드로 이전
    for (size_t i = 0; i < relu_node->num_output_nodes; i++) {
        ETNode* output_node = relu_node->output_nodes[i];

        // output_node의 입력에서 relu_node를 conv_node로 교체
        for (size_t j = 0; j < output_node->num_input_nodes; j++) {
            if (output_node->input_nodes[j] == relu_node) {
                output_node->input_nodes[j] = conv_node;
                break;
            }
        }
    }

    // Conv1D 노드의 출력 연결을 ReLU 노드의 출력으로 교체
    free(conv_node->output_nodes);
    conv_node->output_nodes = relu_node->output_nodes;
    conv_node->num_output_nodes = relu_node->num_output_nodes;

    // ReLU 노드의 출력 연결 정보를 무효화
    relu_node->output_nodes = NULL;
    relu_node->num_output_nodes = 0;

    // ReLU 노드를 그래프에서 제거
    et_remove_node(graph, relu_node);
    et_destroy_node(relu_node);

    return ET_SUCCESS;
}

static int fuse_stft_melscale(ETGraph* graph, ETNode* stft_node, ETNode* mel_node) {
    if (!graph || !stft_node || !mel_node) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // STFT 노드의 연산자 타입을 "STFTMelScale"로 변경
    free(stft_node->op_type);
    stft_node->op_type = (char*)malloc(strlen("STFTMelScale") + 1);
    if (!stft_node->op_type) {
        return ET_ERROR_OUT_OF_MEMORY;
    }
    strcpy(stft_node->op_type, "STFTMelScale");

    // MelScale 노드의 속성을 STFT 노드에 병합
    // 실제 구현에서는 속성 구조체를 병합해야 함

    // MelScale 노드의 출력 연결을 STFT 노드로 이전
    for (size_t i = 0; i < mel_node->num_output_nodes; i++) {
        ETNode* output_node = mel_node->output_nodes[i];

        for (size_t j = 0; j < output_node->num_input_nodes; j++) {
            if (output_node->input_nodes[j] == mel_node) {
                output_node->input_nodes[j] = stft_node;
                break;
            }
        }
    }

    // STFT 노드의 출력을 MelScale 출력으로 변경 (magnitude만 사용)
    if (stft_node->num_outputs > 1) {
        // phase 출력 제거하고 magnitude만 유지
        stft_node->num_outputs = 1;
    }

    free(stft_node->output_nodes);
    stft_node->output_nodes = mel_node->output_nodes;
    stft_node->num_output_nodes = mel_node->num_output_nodes;

    mel_node->output_nodes = NULL;
    mel_node->num_output_nodes = 0;

    // MelScale 노드를 그래프에서 제거
    et_remove_node(graph, mel_node);
    et_destroy_node(mel_node);

    return ET_SUCCESS;
}

// 불필요한 연산 제거 헬퍼 함수들
static void mark_reachable_nodes(ETGraph* graph, ETNode* node, bool* reachable) {
    if (!graph || !node || !reachable) return;

    // 노드 인덱스 찾기
    int node_index = -1;
    for (size_t i = 0; i < graph->num_nodes; i++) {
        if (graph->nodes[i] == node) {
            node_index = (int)i;
            break;
        }
    }

    if (node_index == -1 || reachable[node_index]) {
        return; // 노드를 찾을 수 없거나 이미 방문함
    }

    // 현재 노드를 도달 가능으로 마킹
    reachable[node_index] = true;

    // 입력 노드들을 재귀적으로 마킹
    for (size_t i = 0; i < node->num_input_nodes; i++) {
        mark_reachable_nodes(graph, node->input_nodes[i], reachable);
    }
}

// 메모리 접근 최적화 헬퍼 함수들
static void optimize_inplace_operations(ETGraph* graph) {
    if (!graph) return;

    // 인플레이스 연산이 가능한 노드들을 찾아서 최적화
    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];

        // ReLU, Sigmoid 등의 요소별 연산은 인플레이스 가능
        if (strcmp(node->op_type, "ReLU") == 0 ||
            strcmp(node->op_type, "Sigmoid") == 0 ||
            strcmp(node->op_type, "Tanh") == 0) {

            // 입력과 출력 텐서가 같은 크기이고, 입력이 다른 곳에서 사용되지 않으면
            // 인플레이스 연산으로 변경 가능
            if (node->num_inputs == 1 && node->num_outputs == 1) {
                ETTensor* input = node->inputs[0];
                ETTensor* output = node->outputs[0];

                // 입력 텐서가 다른 노드에서 사용되지 않는지 확인
                bool input_used_elsewhere = false;
                for (size_t j = 0; j < graph->num_nodes; j++) {
                    if (i == j) continue;

                    ETNode* other_node = graph->nodes[j];
                    for (size_t k = 0; k < other_node->num_inputs; k++) {
                        if (other_node->inputs[k] == input) {
                            input_used_elsewhere = true;
                            break;
                        }
                    }
                    if (input_used_elsewhere) break;
                }

                // 인플레이스 연산으로 변경
                if (!input_used_elsewhere && input && output) {
                    // 출력 텐서를 입력 텐서로 교체
                    node->outputs[0] = input;

                    // 기존 출력 텐서 해제 (필요시)
                    if (output != input) {
                        et_destroy_tensor(output);
                    }
                }
            }
        }
    }
}

static void optimize_memory_reuse(ETGraph* graph) {
    if (!graph) return;

    // 텐서 생명주기 분석을 통한 메모리 재사용 최적화
    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];

        // 임시 텐서들을 메모리 풀에서 재사용하도록 최적화
        for (size_t j = 0; j < node->num_outputs; j++) {
            ETTensor* tensor = node->outputs[j];
            if (tensor && tensor->pool) {
                // 텐서가 더 이상 사용되지 않으면 메모리 풀로 반환
                // 실제 구현에서는 참조 카운팅이나 생명주기 분석 필요
            }
        }
    }
}

static void optimize_tensor_lifetime(ETGraph* graph) {
    if (!graph) return;

    // 텐서 생명주기 분석 및 최적화
    // 1. 각 텐서의 생성 시점과 마지막 사용 시점 분석
    // 2. 생명주기가 겹치지 않는 텐서들의 메모리 공유
    // 3. 메모리 할당 순서 최적화

    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];

        // 노드의 실행 순서를 고려하여 텐서 생명주기 최적화
        if (node->execution_order >= 0) {
            // 이전 노드들의 출력 텐서 중 더 이상 사용되지 않는 것들을 해제
            for (size_t j = 0; j < i; j++) {
                ETNode* prev_node = graph->nodes[j];
                if (prev_node->execution_order < node->execution_order) {
                    // 이전 노드의 출력이 현재 노드 이후에 사용되지 않으면 해제 가능
                    // 실제 구현에서는 더 정교한 분석 필요
                }
            }
        }
    }
}

/**
 * @brief 음성 특화 연산자들을 레지스트리에 등록합니다
 * @param registry 대상 레지스트리
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_register_audio_operators(ETOperatorRegistry* registry) {
    if (!registry) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int result;

    // STFT 연산자 등록
    ETOperator stft_op = {
        .name = "STFT",
        .create = stft_create,
        .forward = stft_forward,
        .backward = NULL,
        .destroy = stft_destroy
    };
    result = et_register_operator(registry, &stft_op);
    if (result != ET_SUCCESS) return result;

    // Mel Scale 연산자 등록
    ETOperator mel_scale_op = {
        .name = "MelScale",
        .create = mel_scale_create,
        .forward = mel_scale_forward,
        .backward = NULL,
        .destroy = mel_scale_destroy
    };
    result = et_register_operator(registry, &mel_scale_op);
    if (result != ET_SUCCESS) return result;

    // Vocoder 연산자 등록
    ETOperator vocoder_op = {
        .name = "Vocoder",
        .create = vocoder_create,
        .forward = vocoder_forward,
        .backward = NULL,
        .destroy = vocoder_destroy
    };
    result = et_register_operator(registry, &vocoder_op);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}

/**
 * @brief 모든 기본 연산자들을 레지스트리에 등록합니다
 * @param registry 대상 레지스트리
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_register_all_operators(ETOperatorRegistry* registry) {
    if (!registry) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int result;

    // 기본 연산자들 등록
    result = et_register_basic_operators(registry);
    if (result != ET_SUCCESS) return result;

    // 음성 특화 연산자들 등록
    result = et_register_audio_operators(registry);
    if (result != ET_SUCCESS) return result;

    return ET_SUCCESS;
}

