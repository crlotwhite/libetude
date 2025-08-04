#include "libetude/graph.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "libetude/threading.h"
#include <semaphore.h>
#include <unistd.h>

#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// 내부 상수 정의
#define DEFAULT_NODES_CAPACITY 32
#define DEFAULT_CONNECTIONS_CAPACITY 8
#define MAX_WORKER_THREADS 8
#define READY_QUEUE_SIZE 256

// 에러 코드 매핑 (호환성을 위해)
#define ET_SUCCESS LIBETUDE_SUCCESS
#define ET_ERROR_INVALID_ARGUMENT LIBETUDE_ERROR_INVALID_ARGUMENT
#define ET_ERROR_OUT_OF_MEMORY LIBETUDE_ERROR_OUT_OF_MEMORY
#define ET_ERROR_RUNTIME LIBETUDE_ERROR_RUNTIME

// 병렬 실행을 위한 구조체
typedef struct {
    ETGraph* graph;
    ETNode** ready_queue;
    size_t queue_head;
    size_t queue_tail;
    size_t queue_size;
    pthread_mutex_t queue_mutex;
    sem_t ready_semaphore;
    pthread_t* worker_threads;
    size_t num_workers;
    bool shutdown;
    int* node_dependencies; // 각 노드의 남은 의존성 수
} ParallelExecutor;

// 메모리 계획을 위한 구조체
typedef struct {
    int start_step;  // 텐서가 생성되는 단계
    int end_step;    // 텐서가 마지막으로 사용되는 단계
    size_t size;     // 텐서 크기
    bool is_input;   // 입력 텐서 여부
    bool is_output;  // 출력 텐서 여부
} TensorLifetime;

typedef struct {
    TensorLifetime* lifetimes;
    size_t num_tensors;
    size_t peak_memory;
    size_t* memory_usage_per_step;
    size_t num_steps;
} MemoryPlan;

// 내부 함수 선언
static int resize_nodes_array(ETGraph* graph, size_t new_capacity);
static int resize_node_connections(ETNode* node, bool is_input, size_t new_capacity);
static void reset_node_states(ETGraph* graph);
static int dfs_visit(ETNode* node, int* visit_state, ETNode** sorted_nodes, int* index, ETGraph* graph);

// 병렬 실행 관련 함수
static int execute_graph_parallel(ETGraph* graph, ETTensor** inputs, ETTensor** outputs);
static void* execute_node_worker(void* arg);
static int schedule_ready_nodes(ETGraph* graph);
static bool is_node_ready_for_execution(ETNode* node);

// 메모리 계획 최적화 함수
static int optimize_memory_plan(ETGraph* graph);
static int analyze_tensor_lifetimes(ETGraph* graph);
static int allocate_memory_efficiently(ETGraph* graph);
static void free_intermediate_tensors(ETGraph* graph, int current_step);

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

    // 메모리 계획 최적화 수행
    int result = optimize_memory_plan(graph);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 병렬 실행 가능 여부 확인 (노드 수가 충분히 많은 경우)
    if (graph->num_nodes > 4) {
        return execute_graph_parallel(graph, inputs, outputs);
    }

    // 순차 실행 (기존 방식)
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

            // 중간 텐서 해제 (메모리 최적화)
            free_intermediate_tensors(graph, (int)i);
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

// =============================================================================
// 병렬 실행 함수 구현
// =============================================================================

static int execute_graph_parallel(ETGraph* graph, ETTensor** inputs, ETTensor** outputs) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 병렬 실행기 초기화
    ParallelExecutor executor;
    executor.graph = graph;
    executor.ready_queue = (ETNode**)malloc(READY_QUEUE_SIZE * sizeof(ETNode*));
    if (!executor.ready_queue) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    executor.queue_head = 0;
    executor.queue_tail = 0;
    executor.queue_size = READY_QUEUE_SIZE;
    executor.shutdown = false;

    // 뮤텍스와 세마포어 초기화
    if (pthread_mutex_init(&executor.queue_mutex, NULL) != 0) {
        free(executor.ready_queue);
        return ET_ERROR_RUNTIME;
    }

    if (sem_init(&executor.ready_semaphore, 0, 0) != 0) {
        pthread_mutex_destroy(&executor.queue_mutex);
        free(executor.ready_queue);
        return ET_ERROR_RUNTIME;
    }

    // 각 노드의 의존성 수 계산
    executor.node_dependencies = (int*)malloc(graph->num_nodes * sizeof(int));
    if (!executor.node_dependencies) {
        sem_destroy(&executor.ready_semaphore);
        pthread_mutex_destroy(&executor.queue_mutex);
        free(executor.ready_queue);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < graph->num_nodes; i++) {
        executor.node_dependencies[i] = (int)graph->nodes[i]->num_input_nodes;
    }

    // 워커 스레드 수 결정 (CPU 코어 수 기반)
    executor.num_workers = (size_t)sysconf(_SC_NPROCESSORS_ONLN);
    if (executor.num_workers > MAX_WORKER_THREADS) {
        executor.num_workers = MAX_WORKER_THREADS;
    }
    if (executor.num_workers < 1) {
        executor.num_workers = 1;
    }

    // 워커 스레드 생성
    executor.worker_threads = (pthread_t*)malloc(executor.num_workers * sizeof(pthread_t));
    if (!executor.worker_threads) {
        free(executor.node_dependencies);
        sem_destroy(&executor.ready_semaphore);
        pthread_mutex_destroy(&executor.queue_mutex);
        free(executor.ready_queue);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < executor.num_workers; i++) {
        if (pthread_create(&executor.worker_threads[i], NULL, execute_node_worker, &executor) != 0) {
            // 생성된 스레드들 정리
            executor.shutdown = true;
            for (size_t j = 0; j < i; j++) {
                pthread_cancel(executor.worker_threads[j]);
                pthread_join(executor.worker_threads[j], NULL);
            }
            free(executor.worker_threads);
            free(executor.node_dependencies);
            sem_destroy(&executor.ready_semaphore);
            pthread_mutex_destroy(&executor.queue_mutex);
            free(executor.ready_queue);
            return ET_ERROR_RUNTIME;
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
                input_node->state = ET_NODE_COMPLETED;
            }
        }
    }

    // 초기 실행 가능한 노드들을 큐에 추가
    pthread_mutex_lock(&executor.queue_mutex);
    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];
        if (executor.node_dependencies[i] == 0 && node->state == ET_NODE_READY) {
            size_t next_tail = (executor.queue_tail + 1) % executor.queue_size;
            if (next_tail != executor.queue_head) {
                executor.ready_queue[executor.queue_tail] = node;
                executor.queue_tail = next_tail;
                sem_post(&executor.ready_semaphore);
            }
        }
    }
    pthread_mutex_unlock(&executor.queue_mutex);

    // 모든 노드가 완료될 때까지 대기
    size_t completed_nodes = 0;
    while (completed_nodes < graph->num_nodes) {
        usleep(1000); // 1ms 대기

        // 완료된 노드 수 계산
        completed_nodes = 0;
        for (size_t i = 0; i < graph->num_nodes; i++) {
            if (graph->nodes[i]->state == ET_NODE_COMPLETED) {
                completed_nodes++;
            }
        }
    }

    // 워커 스레드 종료
    executor.shutdown = true;
    for (size_t i = 0; i < executor.num_workers; i++) {
        sem_post(&executor.ready_semaphore); // 대기 중인 스레드들 깨우기
    }

    for (size_t i = 0; i < executor.num_workers; i++) {
        pthread_join(executor.worker_threads[i], NULL);
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

    // 리소스 정리
    free(executor.worker_threads);
    free(executor.node_dependencies);
    sem_destroy(&executor.ready_semaphore);
    pthread_mutex_destroy(&executor.queue_mutex);
    free(executor.ready_queue);

    return ET_SUCCESS;
}

static void* execute_node_worker(void* arg) {
    ParallelExecutor* executor = (ParallelExecutor*)arg;
    ETGraph* graph = executor->graph;

    while (!executor->shutdown) {
        // 실행 가능한 노드 대기
        if (sem_wait(&executor->ready_semaphore) != 0) {
            break;
        }

        if (executor->shutdown) {
            break;
        }

        // 큐에서 노드 가져오기
        pthread_mutex_lock(&executor->queue_mutex);

        ETNode* node = NULL;
        if (executor->queue_head != executor->queue_tail) {
            node = executor->ready_queue[executor->queue_head];
            executor->queue_head = (executor->queue_head + 1) % executor->queue_size;
        }

        pthread_mutex_unlock(&executor->queue_mutex);

        if (!node) {
            continue;
        }

        // 노드 실행
        if (node->forward && node->state == ET_NODE_READY) {
            node->state = ET_NODE_RUNNING;
            node->forward(node);
            node->state = ET_NODE_COMPLETED;

            // 의존성 업데이트 및 새로운 실행 가능한 노드들 스케줄링
            pthread_mutex_lock(&executor->queue_mutex);

            for (size_t i = 0; i < node->num_output_nodes; i++) {
                ETNode* output_node = node->output_nodes[i];

                // 노드 인덱스 찾기
                int node_idx = -1;
                for (size_t j = 0; j < graph->num_nodes; j++) {
                    if (graph->nodes[j] == output_node) {
                        node_idx = (int)j;
                        break;
                    }
                }

                if (node_idx >= 0) {
                    executor->node_dependencies[node_idx]--;

                    // 의존성이 모두 해결되면 실행 가능한 노드로 추가
                    if (executor->node_dependencies[node_idx] == 0 &&
                        output_node->state == ET_NODE_READY) {

                        size_t next_tail = (executor->queue_tail + 1) % executor->queue_size;
                        if (next_tail != executor->queue_head) { // 큐가 가득 차지 않은 경우
                            executor->ready_queue[executor->queue_tail] = output_node;
                            executor->queue_tail = next_tail;
                            sem_post(&executor->ready_semaphore);
                        }
                    }
                }
            }

            pthread_mutex_unlock(&executor->queue_mutex);
        }
    }

    return NULL;
}

static int schedule_ready_nodes(ETGraph* graph) {
    // 초기 실행 가능한 노드들 (의존성이 없는 노드들) 찾기
    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];
        if (is_node_ready_for_execution(node)) {
            // 실행 가능한 노드를 큐에 추가하는 로직은 execute_node_worker에서 처리
            // 여기서는 입력 노드들만 처리
            if (node->is_input_node || node->num_input_nodes == 0) {
                node->state = ET_NODE_READY;
            }
        }
    }

    return ET_SUCCESS;
}

static bool is_node_ready_for_execution(ETNode* node) {
    if (!node || node->state != ET_NODE_READY) {
        return false;
    }

    // 모든 입력 노드들이 완료되었는지 확인
    for (size_t i = 0; i < node->num_input_nodes; i++) {
        if (node->input_nodes[i]->state != ET_NODE_COMPLETED) {
            return false;
        }
    }

    return true;
}

// =============================================================================
// 메모리 계획 최적화 함수 구현
// =============================================================================

static int optimize_memory_plan(ETGraph* graph) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 텐서 생명주기 분석
    int result = analyze_tensor_lifetimes(graph);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 효율적인 메모리 할당
    result = allocate_memory_efficiently(graph);
    if (result != ET_SUCCESS) {
        return result;
    }

    return ET_SUCCESS;
}

static int analyze_tensor_lifetimes(ETGraph* graph) {
    if (!graph || !graph->is_sorted) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 각 텐서의 생명주기 분석
    // 실제 구현에서는 더 정교한 분석이 필요
    for (size_t i = 0; i < graph->execution_order_size; i++) {
        ETNode* node = graph->execution_order[i];

        // 출력 텐서들의 생명주기 설정
        for (size_t j = 0; j < node->num_outputs; j++) {
            ETTensor* tensor = node->outputs[j];
            if (tensor) {
                // 텐서가 생성되는 단계 기록
                // 텐서가 마지막으로 사용되는 단계 계산
                // 이 정보는 메모리 해제 시점 결정에 사용
            }
        }
    }

    return ET_SUCCESS;
}

static int allocate_memory_efficiently(ETGraph* graph) {
    if (!graph) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 메모리 풀 크기 최적화
    // 피크 메모리 사용량 기반으로 메모리 풀 크기 조정
    size_t estimated_peak_memory = 0;

    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];

        // 각 노드의 예상 메모리 사용량 계산
        for (size_t j = 0; j < node->num_outputs; j++) {
            if (node->outputs[j]) {
                estimated_peak_memory += node->outputs[j]->size * sizeof(float);
            }
        }
    }

    // 메모리 풀 크기 조정 (여유분 20% 추가)
    size_t optimal_pool_size = estimated_peak_memory * 12 / 10;

    if (graph->mem_pool && optimal_pool_size > graph->mem_pool->total_size) {
        // 메모리 풀 확장 (실제 구현에서는 더 정교한 로직 필요)
        et_reset_pool(graph->mem_pool);
    }

    return ET_SUCCESS;
}

// 헬퍼 함수: 텐서가 아직 필요한지 확인
static bool tensor_is_still_needed(ETTensor* tensor, ETGraph* graph, int current_step) {
    if (!tensor || !graph) return false;

    // 간단한 구현: 출력 텐서는 항상 필요
    // 실제로는 더 정교한 생명주기 분석 필요
    for (size_t i = current_step + 1; i < graph->execution_order_size; i++) {
        ETNode* future_node = graph->execution_order[i];

        for (size_t j = 0; j < future_node->num_inputs; j++) {
            if (future_node->inputs[j] == tensor) {
                return true;
            }
        }
    }

    return false;
}

static void free_intermediate_tensors(ETGraph* graph, int current_step) {
    if (!graph) return;

    // 현재 단계에서 더 이상 사용되지 않는 중간 텐서들을 해제
    // 실제 구현에서는 텐서 생명주기 분석 결과를 사용
    for (size_t i = 0; i < (size_t)current_step; i++) {
        if (i >= graph->execution_order_size) break;

        ETNode* node = graph->execution_order[i];

        // 출력 노드가 아니고, 더 이상 참조되지 않는 텐서들 해제
        if (!node->is_output_node) {
            for (size_t j = 0; j < node->num_outputs; j++) {
                ETTensor* tensor = node->outputs[j];
                if (tensor && !tensor_is_still_needed(tensor, graph, current_step)) {
                    // 텐서 메모리를 풀로 반환 (실제 해제는 하지 않음)
                    // et_free_to_pool(graph->mem_pool, tensor->data);
                }
            }
        }
    }
}

int et_execute_graph_parallel_explicit(ETGraph* graph, ETTensor** inputs, ETTensor** outputs, size_t num_threads) {
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

    // 메모리 계획 최적화 수행
    int result = optimize_memory_plan(graph);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 스레드 수가 1이면 순차 실행
    if (num_threads <= 1) {
        return et_execute_graph(graph, inputs, outputs);
    }

    // 병렬 실행기 초기화 (사용자 지정 스레드 수 사용)
    ParallelExecutor executor;
    executor.graph = graph;
    executor.ready_queue = (ETNode**)malloc(READY_QUEUE_SIZE * sizeof(ETNode*));
    if (!executor.ready_queue) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    executor.queue_head = 0;
    executor.queue_tail = 0;
    executor.queue_size = READY_QUEUE_SIZE;
    executor.shutdown = false;

    // 뮤텍스와 세마포어 초기화
    if (pthread_mutex_init(&executor.queue_mutex, NULL) != 0) {
        free(executor.ready_queue);
        return ET_ERROR_RUNTIME;
    }

    if (sem_init(&executor.ready_semaphore, 0, 0) != 0) {
        pthread_mutex_destroy(&executor.queue_mutex);
        free(executor.ready_queue);
        return ET_ERROR_RUNTIME;
    }

    // 각 노드의 의존성 수 계산
    executor.node_dependencies = (int*)malloc(graph->num_nodes * sizeof(int));
    if (!executor.node_dependencies) {
        sem_destroy(&executor.ready_semaphore);
        pthread_mutex_destroy(&executor.queue_mutex);
        free(executor.ready_queue);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < graph->num_nodes; i++) {
        executor.node_dependencies[i] = (int)graph->nodes[i]->num_input_nodes;
    }

    // 사용자 지정 스레드 수 사용
    executor.num_workers = num_threads;
    if (executor.num_workers > MAX_WORKER_THREADS) {
        executor.num_workers = MAX_WORKER_THREADS;
    }

    // 워커 스레드 생성
    executor.worker_threads = (pthread_t*)malloc(executor.num_workers * sizeof(pthread_t));
    if (!executor.worker_threads) {
        free(executor.node_dependencies);
        sem_destroy(&executor.ready_semaphore);
        pthread_mutex_destroy(&executor.queue_mutex);
        free(executor.ready_queue);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < executor.num_workers; i++) {
        if (pthread_create(&executor.worker_threads[i], NULL, execute_node_worker, &executor) != 0) {
            // 생성된 스레드들 정리
            executor.shutdown = true;
            for (size_t j = 0; j < i; j++) {
                pthread_cancel(executor.worker_threads[j]);
                pthread_join(executor.worker_threads[j], NULL);
            }
            free(executor.worker_threads);
            free(executor.node_dependencies);
            sem_destroy(&executor.ready_semaphore);
            pthread_mutex_destroy(&executor.queue_mutex);
            free(executor.ready_queue);
            return ET_ERROR_RUNTIME;
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
                input_node->state = ET_NODE_COMPLETED;
            }
        }
    }

    // 초기 실행 가능한 노드들을 큐에 추가
    pthread_mutex_lock(&executor.queue_mutex);
    for (size_t i = 0; i < graph->num_nodes; i++) {
        ETNode* node = graph->nodes[i];
        if (executor.node_dependencies[i] == 0 && node->state == ET_NODE_READY) {
            size_t next_tail = (executor.queue_tail + 1) % executor.queue_size;
            if (next_tail != executor.queue_head) {
                executor.ready_queue[executor.queue_tail] = node;
                executor.queue_tail = next_tail;
                sem_post(&executor.ready_semaphore);
            }
        }
    }
    pthread_mutex_unlock(&executor.queue_mutex);

    // 모든 노드가 완료될 때까지 대기
    size_t completed_nodes = 0;
    while (completed_nodes < graph->num_nodes) {
        usleep(1000); // 1ms 대기

        // 완료된 노드 수 계산
        completed_nodes = 0;
        for (size_t i = 0; i < graph->num_nodes; i++) {
            if (graph->nodes[i]->state == ET_NODE_COMPLETED) {
                completed_nodes++;
            }
        }
    }

    // 워커 스레드 종료
    executor.shutdown = true;
    for (size_t i = 0; i < executor.num_workers; i++) {
        sem_post(&executor.ready_semaphore); // 대기 중인 스레드들 깨우기
    }

    for (size_t i = 0; i < executor.num_workers; i++) {
        pthread_join(executor.worker_threads[i], NULL);
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

    // 리소스 정리
    free(executor.worker_threads);
    free(executor.node_dependencies);
    sem_destroy(&executor.ready_semaphore);
    pthread_mutex_destroy(&executor.queue_mutex);
    free(executor.ready_queue);

    return ET_SUCCESS;
}

#ifdef __APPLE__
#pragma clang diagnostic pop
#endif