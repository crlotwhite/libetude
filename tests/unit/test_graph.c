#include "libetude/graph.h"
#include "libetude/memory.h"
#include "libetude/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 에러 코드 매핑 (호환성을 위해)
#define ET_SUCCESS LIBETUDE_SUCCESS
#define ET_ERROR_INVALID_ARGUMENT LIBETUDE_ERROR_INVALID_ARGUMENT
#define ET_ERROR_OUT_OF_MEMORY LIBETUDE_ERROR_OUT_OF_MEMORY
#define ET_ERROR_RUNTIME LIBETUDE_ERROR_RUNTIME

// 테스트용 간단한 연산자 구현
static void dummy_forward(ETNode* node) {
    // 간단한 더미 순방향 연산
    if (node && node->inputs && node->outputs && node->num_inputs > 0 && node->num_outputs > 0) {
        // 입력을 출력으로 복사 (패스스루)
        node->outputs[0] = node->inputs[0];
    }
}

static void add_forward(ETNode* node) {
    // 간단한 덧셈 연산 (실제로는 텐서 연산을 수행해야 함)
    if (node && node->inputs && node->outputs && node->num_inputs >= 2 && node->num_outputs > 0) {
        // 실제 구현에서는 텐서 덧셈을 수행
        // 여기서는 테스트를 위해 첫 번째 입력을 출력으로 설정
        node->outputs[0] = node->inputs[0];
    }
}

static void create_dummy_operator(ETNode* node, void* attributes) {
    (void)attributes; // 미사용 매개변수
    node->forward = dummy_forward;
}

static void create_add_operator(ETNode* node, void* attributes) {
    (void)attributes; // 미사용 매개변수
    node->forward = add_forward;
}

// 테스트 함수들
static void test_graph_creation_and_destruction() {
    printf("Testing graph creation and destruction...\n");

    ETGraph* graph = et_create_graph(10);
    assert(graph != NULL);
    assert(graph->num_nodes == 0);
    assert(graph->nodes_capacity == 10);
    assert(graph->mem_pool != NULL);
    assert(graph->is_sorted == false);

    et_destroy_graph(graph);
    printf("✓ Graph creation and destruction test passed\n");
}

static void test_node_creation_and_destruction() {
    printf("Testing node creation and destruction...\n");

    ETMemoryPool* pool = et_create_memory_pool(1024, 32);
    assert(pool != NULL);

    ETNode* node = et_create_node("test_node", "dummy", pool);
    assert(node != NULL);
    assert(strcmp(node->name, "test_node") == 0);
    assert(strcmp(node->op_type, "dummy") == 0);
    assert(node->state == ET_NODE_READY);
    assert(node->execution_order == -1);
    assert(node->mem_pool == pool);

    et_destroy_node(node);
    et_destroy_memory_pool(pool);
    printf("✓ Node creation and destruction test passed\n");
}

static void test_node_connections() {
    printf("Testing node connections...\n");

    ETMemoryPool* pool = et_create_memory_pool(1024, 32);
    assert(pool != NULL);

    ETNode* node1 = et_create_node("node1", "dummy", pool);
    ETNode* node2 = et_create_node("node2", "dummy", pool);
    ETNode* node3 = et_create_node("node3", "dummy", pool);

    assert(node1 != NULL && node2 != NULL && node3 != NULL);

    // 노드 연결: node1 -> node2 -> node3
    int result = et_connect_nodes(node1, node2);
    assert(result == ET_SUCCESS);
    assert(node1->num_output_nodes == 1);
    assert(node1->output_nodes[0] == node2);
    assert(node2->num_input_nodes == 1);
    assert(node2->input_nodes[0] == node1);

    result = et_connect_nodes(node2, node3);
    assert(result == ET_SUCCESS);
    assert(node2->num_output_nodes == 1);
    assert(node2->output_nodes[0] == node3);
    assert(node3->num_input_nodes == 1);
    assert(node3->input_nodes[0] == node2);

    // 연결 해제 테스트
    result = et_disconnect_nodes(node1, node2);
    assert(result == ET_SUCCESS);
    assert(node1->num_output_nodes == 0);
    assert(node2->num_input_nodes == 0);

    et_destroy_node(node1);
    et_destroy_node(node2);
    et_destroy_node(node3);
    et_destroy_memory_pool(pool);
    printf("✓ Node connections test passed\n");
}

static void test_graph_node_management() {
    printf("Testing graph node management...\n");

    ETGraph* graph = et_create_graph(5);
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);

    assert(graph != NULL && pool != NULL);

    // 노드 생성 및 그래프에 추가
    ETNode* node1 = et_create_node("input", "input", pool);
    ETNode* node2 = et_create_node("hidden", "add", pool);
    ETNode* node3 = et_create_node("output", "output", pool);

    assert(node1 != NULL && node2 != NULL && node3 != NULL);

    int result = et_add_node(graph, node1);
    assert(result == ET_SUCCESS);
    assert(graph->num_nodes == 1);

    result = et_add_node(graph, node2);
    assert(result == ET_SUCCESS);
    assert(graph->num_nodes == 2);

    result = et_add_node(graph, node3);
    assert(result == ET_SUCCESS);
    assert(graph->num_nodes == 3);

    // 노드 찾기 테스트
    ETNode* found = et_find_node_by_name(graph, "hidden");
    assert(found == node2);

    found = et_find_node_by_name(graph, "nonexistent");
    assert(found == NULL);

    // 노드 제거 테스트
    result = et_remove_node(graph, node2);
    assert(result == ET_SUCCESS);
    assert(graph->num_nodes == 2);

    found = et_find_node_by_name(graph, "hidden");
    assert(found == NULL);

    et_destroy_node(node2); // 제거된 노드는 수동으로 해제
    et_destroy_graph(graph); // 그래프 해제시 남은 노드들도 해제됨
    et_destroy_memory_pool(pool);
    printf("✓ Graph node management test passed\n");
}

static void test_topological_sort() {
    printf("Testing topological sort...\n");

    ETGraph* graph = et_create_graph(10);
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);

    assert(graph != NULL && pool != NULL);

    // 간단한 선형 그래프 생성: A -> B -> C
    ETNode* nodeA = et_create_node("A", "dummy", pool);
    ETNode* nodeB = et_create_node("B", "dummy", pool);
    ETNode* nodeC = et_create_node("C", "dummy", pool);

    assert(nodeA != NULL && nodeB != NULL && nodeC != NULL);

    et_add_node(graph, nodeA);
    et_add_node(graph, nodeB);
    et_add_node(graph, nodeC);

    et_connect_nodes(nodeA, nodeB);
    et_connect_nodes(nodeB, nodeC);

    // 토폴로지 정렬 수행
    int result = et_topological_sort(graph);
    assert(result == ET_SUCCESS);
    assert(graph->is_sorted == true);
    assert(graph->execution_order_size == 3);

    // 실행 순서 확인 (A -> B -> C 순서여야 함)
    assert(graph->execution_order[0] == nodeA);
    assert(graph->execution_order[1] == nodeB);
    assert(graph->execution_order[2] == nodeC);

    et_destroy_graph(graph);
    et_destroy_memory_pool(pool);
    printf("✓ Topological sort test passed\n");
}

static void test_cycle_detection() {
    printf("Testing cycle detection...\n");

    ETGraph* graph = et_create_graph(10);
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);

    assert(graph != NULL && pool != NULL);

    // 순환 그래프 생성: A -> B -> C -> A
    ETNode* nodeA = et_create_node("A", "dummy", pool);
    ETNode* nodeB = et_create_node("B", "dummy", pool);
    ETNode* nodeC = et_create_node("C", "dummy", pool);

    assert(nodeA != NULL && nodeB != NULL && nodeC != NULL);

    et_add_node(graph, nodeA);
    et_add_node(graph, nodeB);
    et_add_node(graph, nodeC);

    et_connect_nodes(nodeA, nodeB);
    et_connect_nodes(nodeB, nodeC);
    et_connect_nodes(nodeC, nodeA); // 순환 생성

    // 순환 참조 검사
    bool has_cycle = et_has_cycle(graph);
    assert(has_cycle == true);

    // 순환 제거
    et_disconnect_nodes(nodeC, nodeA);
    has_cycle = et_has_cycle(graph);
    assert(has_cycle == false);

    et_destroy_graph(graph);
    et_destroy_memory_pool(pool);
    printf("✓ Cycle detection test passed\n");
}

static void test_operator_registry() {
    printf("Testing operator registry...\n");

    ETOperatorRegistry* registry = et_create_operator_registry(10);
    assert(registry != NULL);
    assert(registry->num_operators == 0);
    assert(registry->capacity == 10);

    // 연산자 등록
    ETOperator dummy_op = {
        .name = "dummy",
        .create = create_dummy_operator,
        .forward = dummy_forward,
        .backward = NULL,
        .destroy = NULL
    };

    ETOperator add_op = {
        .name = "add",
        .create = create_add_operator,
        .forward = add_forward,
        .backward = NULL,
        .destroy = NULL
    };

    int result = et_register_operator(registry, &dummy_op);
    assert(result == ET_SUCCESS);
    assert(registry->num_operators == 1);

    result = et_register_operator(registry, &add_op);
    assert(result == ET_SUCCESS);
    assert(registry->num_operators == 2);

    // 중복 등록 시도
    result = et_register_operator(registry, &dummy_op);
    assert(result == ET_ERROR_INVALID_ARGUMENT);
    assert(registry->num_operators == 2);

    // 연산자 찾기
    ETOperator* found = et_find_operator(registry, "dummy");
    assert(found != NULL);
    assert(strcmp(found->name, "dummy") == 0);
    assert(found->forward == dummy_forward);

    found = et_find_operator(registry, "add");
    assert(found != NULL);
    assert(strcmp(found->name, "add") == 0);
    assert(found->forward == add_forward);

    found = et_find_operator(registry, "nonexistent");
    assert(found == NULL);

    et_destroy_operator_registry(registry);
    printf("✓ Operator registry test passed\n");
}

static void test_graph_execution() {
    printf("Testing graph execution...\n");

    ETGraph* graph = et_create_graph(10);
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);

    assert(graph != NULL && pool != NULL);

    // 간단한 그래프 생성: input -> process -> output
    ETNode* input_node = et_create_node("input", "input", pool);
    ETNode* process_node = et_create_node("process", "dummy", pool);
    ETNode* output_node = et_create_node("output", "output", pool);

    assert(input_node != NULL && process_node != NULL && output_node != NULL);

    // 연산자 설정
    input_node->forward = dummy_forward;
    process_node->forward = dummy_forward;
    output_node->forward = dummy_forward;

    // 노드 속성 설정
    input_node->is_input_node = true;
    output_node->is_output_node = true;

    et_add_node(graph, input_node);
    et_add_node(graph, process_node);
    et_add_node(graph, output_node);

    et_connect_nodes(input_node, process_node);
    et_connect_nodes(process_node, output_node);

    // 입출력 노드 배열 설정
    graph->input_nodes = (ETNode**)malloc(sizeof(ETNode*));
    graph->input_nodes[0] = input_node;
    graph->num_input_nodes = 1;

    graph->output_nodes = (ETNode**)malloc(sizeof(ETNode*));
    graph->output_nodes[0] = output_node;
    graph->num_output_nodes = 1;

    // 그래프 실행 (실제 텐서 없이 테스트)
    int result = et_execute_graph(graph, NULL, NULL);
    assert(result == ET_SUCCESS);
    assert(graph->is_sorted == true);

    // 모든 노드가 완료 상태인지 확인
    for (size_t i = 0; i < graph->num_nodes; i++) {
        assert(graph->nodes[i]->state == ET_NODE_COMPLETED);
    }

    et_destroy_graph(graph);
    et_destroy_memory_pool(pool);
    printf("✓ Graph execution test passed\n");
}

static void test_graph_optimization() {
    printf("Testing graph optimization...\n");

    ETGraph* graph = et_create_graph(10);
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);

    assert(graph != NULL && pool != NULL);

    // 간단한 그래프 생성
    ETNode* node1 = et_create_node("node1", "dummy", pool);
    ETNode* node2 = et_create_node("node2", "dummy", pool);

    assert(node1 != NULL && node2 != NULL);

    et_add_node(graph, node1);
    et_add_node(graph, node2);
    et_connect_nodes(node1, node2);

    assert(graph->is_optimized == false);

    // 그래프 최적화 수행
    int result = et_optimize_graph(graph, ET_OPT_ALL);
    assert(result == ET_SUCCESS);
    assert(graph->is_optimized == true);
    assert(graph->is_sorted == false); // 최적화 후 정렬 무효화

    et_destroy_graph(graph);
    et_destroy_memory_pool(pool);
    printf("✓ Graph optimization test passed\n");
}

static void test_basic_operators_registration() {
    printf("Testing basic operators registration...\n");

    ETOperatorRegistry* registry = et_create_operator_registry(20);
    assert(registry != NULL);

    // 기본 연산자들 등록
    int result = et_register_basic_operators(registry);
    assert(result == ET_SUCCESS);
    assert(registry->num_operators >= 3); // Linear, Conv1D, Attention

    // 등록된 연산자들 확인
    ETOperator* linear_op = et_find_operator(registry, "Linear");
    assert(linear_op != NULL);
    assert(linear_op->create != NULL);
    assert(linear_op->forward != NULL);
    assert(linear_op->destroy != NULL);

    ETOperator* conv1d_op = et_find_operator(registry, "Conv1D");
    assert(conv1d_op != NULL);
    assert(conv1d_op->create != NULL);
    assert(conv1d_op->forward != NULL);
    assert(conv1d_op->destroy != NULL);

    ETOperator* attention_op = et_find_operator(registry, "Attention");
    assert(attention_op != NULL);
    assert(attention_op->create != NULL);
    assert(attention_op->forward != NULL);
    assert(attention_op->destroy != NULL);

    et_destroy_operator_registry(registry);
    printf("✓ Basic operators registration test passed\n");
}

static void test_audio_operators_registration() {
    printf("Testing audio operators registration...\n");

    ETOperatorRegistry* registry = et_create_operator_registry(20);
    assert(registry != NULL);

    // 음성 특화 연산자들 등록
    int result = et_register_audio_operators(registry);
    assert(result == ET_SUCCESS);
    assert(registry->num_operators >= 3); // STFT, MelScale, Vocoder

    // 등록된 연산자들 확인
    ETOperator* stft_op = et_find_operator(registry, "STFT");
    assert(stft_op != NULL);
    assert(stft_op->create != NULL);
    assert(stft_op->forward != NULL);
    assert(stft_op->destroy != NULL);

    ETOperator* mel_op = et_find_operator(registry, "MelScale");
    assert(mel_op != NULL);
    assert(mel_op->create != NULL);
    assert(mel_op->forward != NULL);
    assert(mel_op->destroy != NULL);

    ETOperator* vocoder_op = et_find_operator(registry, "Vocoder");
    assert(vocoder_op != NULL);
    assert(vocoder_op->create != NULL);
    assert(vocoder_op->forward != NULL);
    assert(vocoder_op->destroy != NULL);

    et_destroy_operator_registry(registry);
    printf("✓ Audio operators registration test passed\n");
}

static void test_all_operators_registration() {
    printf("Testing all operators registration...\n");

    ETOperatorRegistry* registry = et_create_operator_registry(20);
    assert(registry != NULL);

    // 모든 연산자들 등록
    int result = et_register_all_operators(registry);
    assert(result == ET_SUCCESS);
    assert(registry->num_operators >= 6); // 기본 3개 + 음성 3개

    // 기본 연산자들 확인
    assert(et_find_operator(registry, "Linear") != NULL);
    assert(et_find_operator(registry, "Conv1D") != NULL);
    assert(et_find_operator(registry, "Attention") != NULL);

    // 음성 특화 연산자들 확인
    assert(et_find_operator(registry, "STFT") != NULL);
    assert(et_find_operator(registry, "MelScale") != NULL);
    assert(et_find_operator(registry, "Vocoder") != NULL);

    et_destroy_operator_registry(registry);
    printf("✓ All operators registration test passed\n");
}

static void test_operator_node_creation() {
    printf("Testing operator node creation...\n");

    ETOperatorRegistry* registry = et_create_operator_registry(20);
    ETMemoryPool* pool = et_create_memory_pool(1024, 32);

    assert(registry != NULL && pool != NULL);

    // 모든 연산자들 등록
    int result = et_register_all_operators(registry);
    assert(result == ET_SUCCESS);

    // Linear 연산자로 노드 생성 테스트
    ETNode* linear_node = et_create_node("linear_test", "Linear", pool);
    assert(linear_node != NULL);

    ETOperator* linear_op = et_find_operator(registry, "Linear");
    assert(linear_op != NULL);

    // 연산자 속성 설정 (간단한 테스트용)
    typedef struct {
        size_t input_size;
        size_t output_size;
        void* weight;
        void* bias;
        bool use_bias;
    } TestLinearAttributes;

    TestLinearAttributes attrs = {
        .input_size = 128,
        .output_size = 64,
        .weight = NULL,
        .bias = NULL,
        .use_bias = false
    };

    // 연산자 생성 함수 호출
    if (linear_op->create) {
        linear_op->create(linear_node, &attrs);
        assert(linear_node->attributes != NULL);
        assert(linear_node->num_inputs == 1);
        assert(linear_node->num_outputs == 1);
    }

    // 정리
    if (linear_op->destroy) {
        linear_op->destroy(linear_node);
    }
    et_destroy_node(linear_node);
    et_destroy_operator_registry(registry);
    et_destroy_memory_pool(pool);

    printf("✓ Operator node creation test passed\n");
}

// 메인 테스트 함수
int main() {
    printf("=== LibEtude Graph System Tests ===\n\n");

    test_graph_creation_and_destruction();
    test_node_creation_and_destruction();
    test_node_connections();
    test_graph_node_management();
    test_topological_sort();
    test_cycle_detection();
    test_operator_registry();
    test_graph_execution();
    test_graph_optimization();

    // 새로운 연산자 레지스트리 테스트들
    test_basic_operators_registration();
    test_audio_operators_registration();
    test_all_operators_registration();
    test_operator_node_creation();

    printf("\n=== All Graph System Tests Passed! ===\n");
    return 0;
}