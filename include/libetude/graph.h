#ifndef LIBETUDE_GRAPH_H
#define LIBETUDE_GRAPH_H

#include "tensor.h"
#include "memory.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 전방 선언 (Forward declarations)
typedef struct ETNode ETNode;
typedef struct ETGraph ETGraph;
typedef struct ETOperator ETOperator;
typedef struct ETOperatorRegistry ETOperatorRegistry;

// 그래프 최적화 플래그
typedef enum {
    ET_OPT_NONE = 0,
    ET_OPT_OPERATOR_FUSION = 1 << 0,      // 연산자 융합
    ET_OPT_DEAD_CODE_ELIMINATION = 1 << 1, // 불필요한 연산 제거
    ET_OPT_MEMORY_OPTIMIZATION = 1 << 2,   // 메모리 접근 최적화
    ET_OPT_ALL = 0xFFFFFFFF
} ETOptimizationFlags;

// 노드 실행 상태
typedef enum {
    ET_NODE_READY = 0,      // 실행 준비 완료
    ET_NODE_RUNNING = 1,    // 실행 중
    ET_NODE_COMPLETED = 2,  // 실행 완료
    ET_NODE_ERROR = 3       // 실행 오류
} ETNodeState;

// 그래프 노드 구조체
struct ETNode {
    char* name;                // 노드 이름
    char* op_type;            // 연산자 타입

    // 텐서 연결
    ETTensor** inputs;        // 입력 텐서 배열
    size_t num_inputs;        // 입력 텐서 수
    ETTensor** outputs;       // 출력 텐서 배열
    size_t num_outputs;       // 출력 텐서 수

    // 노드 연결 (그래프 구조)
    ETNode** input_nodes;     // 입력 노드 배열
    size_t num_input_nodes;   // 입력 노드 수
    ETNode** output_nodes;    // 출력 노드 배열
    size_t num_output_nodes;  // 출력 노드 수

    // 연산자 속성 및 실행
    void* attributes;         // 연산자 속성
    void (*forward)(ETNode*); // 순방향 함수
    void (*backward)(ETNode*); // 역방향 함수 (필요시)
    void (*destroy_attributes)(void*); // 속성 소멸 함수

    // 실행 상태 및 메타데이터
    ETNodeState state;        // 노드 실행 상태
    int execution_order;      // 실행 순서 (토폴로지 정렬 결과)
    bool is_input_node;       // 입력 노드 여부
    bool is_output_node;      // 출력 노드 여부

    // 메모리 관리
    ETMemoryPool* mem_pool;   // 메모리 풀
};

// 계산 그래프 구조체
struct ETGraph {
    ETNode** nodes;           // 모든 노드 배열
    size_t num_nodes;         // 노드 수
    size_t nodes_capacity;    // 노드 배열 용량

    ETNode** input_nodes;     // 입력 노드 배열
    size_t num_input_nodes;   // 입력 노드 수
    ETNode** output_nodes;    // 출력 노드 배열
    size_t num_output_nodes;  // 출력 노드 수

    // 실행 순서 (토폴로지 정렬 결과)
    ETNode** execution_order; // 실행 순서 배열
    size_t execution_order_size; // 실행 순서 배열 크기
    bool is_sorted;           // 토폴로지 정렬 완료 여부

    // 메모리 관리
    ETMemoryPool* mem_pool;   // 메모리 풀

    // 그래프 메타데이터
    char* name;               // 그래프 이름
    bool is_optimized;        // 최적화 완료 여부
};

// 연산자 구조체
struct ETOperator {
    char* name;                                // 연산자 이름
    void (*create)(ETNode*, void* attributes); // 생성 함수
    void (*forward)(ETNode*);                  // 순방향 함수
    void (*backward)(ETNode*);                 // 역방향 함수 (필요시)
    void (*destroy)(ETNode*);                  // 소멸 함수
};

// 연산자 레지스트리 구조체
struct ETOperatorRegistry {
    ETOperator* operators;    // 연산자 배열
    size_t num_operators;     // 등록된 연산자 수
    size_t capacity;          // 연산자 배열 용량
};

// =============================================================================
// 그래프 생성 및 관리 함수
// =============================================================================

/**
 * @brief 새로운 계산 그래프를 생성합니다
 * @param initial_nodes_capacity 초기 노드 배열 용량
 * @return 생성된 그래프 포인터, 실패시 NULL
 */
ETGraph* et_create_graph(size_t initial_nodes_capacity);

/**
 * @brief 그래프를 해제합니다
 * @param graph 해제할 그래프
 */
void et_destroy_graph(ETGraph* graph);

/**
 * @brief 그래프에 노드를 추가합니다
 * @param graph 대상 그래프
 * @param node 추가할 노드
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_add_node(ETGraph* graph, ETNode* node);

/**
 * @brief 그래프에서 노드를 제거합니다
 * @param graph 대상 그래프
 * @param node 제거할 노드
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_remove_node(ETGraph* graph, ETNode* node);

// =============================================================================
// 노드 생성 및 관리 함수
// =============================================================================

/**
 * @brief 새로운 노드를 생성합니다
 * @param name 노드 이름
 * @param op_type 연산자 타입
 * @param mem_pool 메모리 풀
 * @return 생성된 노드 포인터, 실패시 NULL
 */
ETNode* et_create_node(const char* name, const char* op_type, ETMemoryPool* mem_pool);

/**
 * @brief 노드를 해제합니다
 * @param node 해제할 노드
 */
void et_destroy_node(ETNode* node);

/**
 * @brief 두 노드를 연결합니다 (src -> dst)
 * @param src 소스 노드
 * @param dst 대상 노드
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_connect_nodes(ETNode* src, ETNode* dst);

/**
 * @brief 두 노드의 연결을 해제합니다
 * @param src 소스 노드
 * @param dst 대상 노드
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_disconnect_nodes(ETNode* src, ETNode* dst);

// =============================================================================
// 그래프 순회 및 실행 함수
// =============================================================================

/**
 * @brief 그래프의 토폴로지 정렬을 수행합니다
 * @param graph 대상 그래프
 * @return 성공시 0, 실패시 음수 에러 코드 (순환 참조 등)
 */
int et_topological_sort(ETGraph* graph);

/**
 * @brief 그래프를 실행합니다
 * @param graph 실행할 그래프
 * @param inputs 입력 텐서 배열
 * @param outputs 출력 텐서 배열
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_execute_graph(ETGraph* graph, ETTensor** inputs, ETTensor** outputs);

/**
 * @brief 그래프의 특정 노드까지만 실행합니다
 * @param graph 실행할 그래프
 * @param target_node 목표 노드
 * @param inputs 입력 텐서 배열
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_execute_until_node(ETGraph* graph, ETNode* target_node, ETTensor** inputs);

// =============================================================================
// 그래프 최적화 함수
// =============================================================================

/**
 * @brief 그래프를 최적화합니다
 * @param graph 최적화할 그래프
 * @param flags 최적화 플래그
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_optimize_graph(ETGraph* graph, ETOptimizationFlags flags);

// =============================================================================
// 연산자 레지스트리 함수
// =============================================================================

/**
 * @brief 연산자 레지스트리를 생성합니다
 * @param initial_capacity 초기 용량
 * @return 생성된 레지스트리 포인터, 실패시 NULL
 */
ETOperatorRegistry* et_create_operator_registry(size_t initial_capacity);

/**
 * @brief 연산자 레지스트리를 해제합니다
 * @param registry 해제할 레지스트리
 */
void et_destroy_operator_registry(ETOperatorRegistry* registry);

/**
 * @brief 연산자를 레지스트리에 등록합니다
 * @param registry 대상 레지스트리
 * @param op 등록할 연산자
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_register_operator(ETOperatorRegistry* registry, ETOperator* op);

/**
 * @brief 연산자를 이름으로 찾습니다
 * @param registry 검색할 레지스트리
 * @param name 연산자 이름
 * @return 찾은 연산자 포인터, 없으면 NULL
 */
ETOperator* et_find_operator(ETOperatorRegistry* registry, const char* name);

// =============================================================================
// 유틸리티 함수
// =============================================================================

/**
 * @brief 그래프에서 이름으로 노드를 찾습니다
 * @param graph 검색할 그래프
 * @param name 노드 이름
 * @return 찾은 노드 포인터, 없으면 NULL
 */
ETNode* et_find_node_by_name(ETGraph* graph, const char* name);

/**
 * @brief 그래프의 순환 참조를 검사합니다
 * @param graph 검사할 그래프
 * @return 순환 참조가 있으면 true, 없으면 false
 */
bool et_has_cycle(ETGraph* graph);

/**
 * @brief 그래프 정보를 출력합니다 (디버깅용)
 * @param graph 출력할 그래프
 */
void et_print_graph_info(ETGraph* graph);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_GRAPH_H