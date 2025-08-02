#ifndef WORLD_DEBUG_TOOLS_H
#define WORLD_DEBUG_TOOLS_H

#include "world_graph_node.h"
#include "world_graph_context.h"
#include "dsp_block_diagram.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 노드 실행 상태
 */
typedef enum {
    NODE_STATE_IDLE,        // 대기 상태
    NODE_STATE_RUNNING,     // 실행 중
    NODE_STATE_COMPLETED,   // 완료
    NODE_STATE_ERROR,       // 오류
    NODE_STATE_BLOCKED      // 차단됨 (의존성 대기)
} NodeExecutionState;

/**
 * @brief 노드 디버그 정보
 */
typedef struct {
    char node_name[64];              // 노드 이름
    int node_id;                     // 노드 ID
    NodeExecutionState state;        // 실행 상태
    uint64_t start_time_us;          // 시작 시간 (마이크로초)
    uint64_t end_time_us;            // 종료 시간 (마이크로초)
    uint64_t execution_time_us;      // 실행 시간 (마이크로초)
    size_t input_data_size;          // 입력 데이터 크기
    size_t output_data_size;         // 출력 데이터 크기
    size_t memory_allocated;         // 할당된 메모리
    size_t memory_peak;              // 최대 메모리 사용량
    int error_code;                  // 오류 코드
    char error_message[256];         // 오류 메시지
    int execution_count;             // 실행 횟수
} NodeDebugInfo;

/**
 * @brief 데이터 흐름 추적 정보
 */
typedef struct {
    char source_node[64];            // 소스 노드 이름
    char dest_node[64];              // 대상 노드 이름
    int source_port;                 // 소스 포트
    int dest_port;                   // 대상 포트
    size_t data_size;                // 데이터 크기
    uint64_t transfer_time_us;       // 전송 시간 (마이크로초)
    bool is_valid;                   // 데이터 유효성
    char data_type[32];              // 데이터 타입
} DataFlowTrace;

/**
 * @brief 디버그 이벤트 타입
 */
typedef enum {
    DEBUG_EVENT_NODE_START,          // 노드 시작
    DEBUG_EVENT_NODE_COMPLETE,       // 노드 완료
    DEBUG_EVENT_NODE_ERROR,          // 노드 오류
    DEBUG_EVENT_DATA_TRANSFER,       // 데이터 전송
    DEBUG_EVENT_MEMORY_ALLOC,        // 메모리 할당
    DEBUG_EVENT_MEMORY_FREE,         // 메모리 해제
    DEBUG_EVENT_GRAPH_START,         // 그래프 시작
    DEBUG_EVENT_GRAPH_COMPLETE       // 그래프 완료
} DebugEventType;

/**
 * @brief 디버그 이벤트
 */
typedef struct {
    DebugEventType type;             // 이벤트 타입
    uint64_t timestamp_us;           // 타임스탬프 (마이크로초)
    char node_name[64];              // 관련 노드 이름
    char message[256];               // 이벤트 메시지
    void* data;                      // 추가 데이터
    size_t data_size;                // 추가 데이터 크기
} DebugEvent;

/**
 * @brief 디버그 컨텍스트
 */
typedef struct {
    NodeDebugInfo* node_infos;       // 노드 디버그 정보 배열
    int node_count;                  // 노드 수
    int max_nodes;                   // 최대 노드 수

    DataFlowTrace* flow_traces;      // 데이터 흐름 추적 배열
    int trace_count;                 // 추적 수
    int max_traces;                  // 최대 추적 수

    DebugEvent* events;              // 디버그 이벤트 배열
    int event_count;                 // 이벤트 수
    int max_events;                  // 최대 이벤트 수

    bool is_enabled;                 // 디버깅 활성화 여부
    bool trace_data_flow;            // 데이터 흐름 추적 여부
    bool trace_memory;               // 메모리 추적 여부
    bool verbose_logging;            // 상세 로깅 여부

    FILE* log_file;                  // 로그 파일
    char log_file_path[256];         // 로그 파일 경로
} DebugContext;

/**
 * @brief 디버그 이벤트 콜백 함수 타입
 */
typedef void (*DebugEventCallback)(const DebugEvent* event, void* user_data);

/**
 * @brief 디버그 컨텍스트 생성
 * @param max_nodes 최대 노드 수
 * @param max_traces 최대 추적 수
 * @param max_events 최대 이벤트 수
 * @return 생성된 디버그 컨텍스트 (실패 시 NULL)
 */
DebugContext* world_debug_create(int max_nodes, int max_traces, int max_events);

/**
 * @brief 디버깅 활성화/비활성화
 * @param debug_ctx 디버그 컨텍스트
 * @param enabled 활성화 여부
 */
void world_debug_set_enabled(DebugContext* debug_ctx, bool enabled);

/**
 * @brief 로그 파일 설정
 * @param debug_ctx 디버그 컨텍스트
 * @param log_file_path 로그 파일 경로
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_set_log_file(DebugContext* debug_ctx, const char* log_file_path);

/**
 * @brief 노드 실행 시작 추적
 * @param debug_ctx 디버그 컨텍스트
 * @param node_name 노드 이름
 * @param node_id 노드 ID
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_trace_node_start(DebugContext* debug_ctx, const char* node_name, int node_id);

/**
 * @brief 노드 실행 완료 추적
 * @param debug_ctx 디버그 컨텍스트
 * @param node_name 노드 이름
 * @param node_id 노드 ID
 * @param execution_time_us 실행 시간 (마이크로초)
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_trace_node_complete(DebugContext* debug_ctx, const char* node_name,
                                   int node_id, uint64_t execution_time_us);

/**
 * @brief 노드 오류 추적
 * @param debug_ctx 디버그 컨텍스트
 * @param node_name 노드 이름
 * @param node_id 노드 ID
 * @param error_code 오류 코드
 * @param error_message 오류 메시지
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_trace_node_error(DebugContext* debug_ctx, const char* node_name,
                                int node_id, int error_code, const char* error_message);

/**
 * @brief 데이터 흐름 추적
 * @param debug_ctx 디버그 컨텍스트
 * @param source_node 소스 노드 이름
 * @param dest_node 대상 노드 이름
 * @param source_port 소스 포트
 * @param dest_port 대상 포트
 * @param data_size 데이터 크기
 * @param data_type 데이터 타입
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_trace_data_flow(DebugContext* debug_ctx,
                               const char* source_node, const char* dest_node,
                               int source_port, int dest_port,
                               size_t data_size, const char* data_type);

/**
 * @brief 메모리 할당 추적
 * @param debug_ctx 디버그 컨텍스트
 * @param node_name 노드 이름
 * @param size 할당 크기
 * @param ptr 할당된 포인터
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_trace_memory_alloc(DebugContext* debug_ctx, const char* node_name,
                                  size_t size, void* ptr);

/**
 * @brief 메모리 해제 추적
 * @param debug_ctx 디버그 컨텍스트
 * @param node_name 노드 이름
 * @param ptr 해제할 포인터
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_trace_memory_free(DebugContext* debug_ctx, const char* node_name, void* ptr);

/**
 * @brief 노드 상태 조회
 * @param debug_ctx 디버그 컨텍스트
 * @param node_name 노드 이름
 * @return 노드 디버그 정보 (찾지 못하면 NULL)
 */
const NodeDebugInfo* world_debug_get_node_info(const DebugContext* debug_ctx,
                                               const char* node_name);

/**
 * @brief 모든 노드 상태 출력
 * @param debug_ctx 디버그 컨텍스트
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void world_debug_print_node_states(const DebugContext* debug_ctx, FILE* output_file);

/**
 * @brief 데이터 흐름 추적 결과 출력
 * @param debug_ctx 디버그 컨텍스트
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void world_debug_print_data_flows(const DebugContext* debug_ctx, FILE* output_file);

/**
 * @brief 성능 분석 보고서 생성
 * @param debug_ctx 디버그 컨텍스트
 * @param output_path 출력 파일 경로
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_generate_performance_report(const DebugContext* debug_ctx,
                                           const char* output_path);

/**
 * @brief 오류 진단 보고서 생성
 * @param debug_ctx 디버그 컨텍스트
 * @param output_path 출력 파일 경로
 * @return 성공 시 0, 실패 시 음수
 */
int world_debug_generate_error_report(const DebugContext* debug_ctx,
                                     const char* output_path);

/**
 * @brief 디버그 이벤트 콜백 설정
 * @param debug_ctx 디버그 컨텍스트
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 */
void world_debug_set_event_callback(DebugContext* debug_ctx,
                                   DebugEventCallback callback, void* user_data);

/**
 * @brief 디버그 컨텍스트 초기화
 * @param debug_ctx 디버그 컨텍스트
 */
void world_debug_reset(DebugContext* debug_ctx);

/**
 * @brief 디버그 컨텍스트 해제
 * @param debug_ctx 디버그 컨텍스트
 */
void world_debug_destroy(DebugContext* debug_ctx);

#ifdef __cplusplus
}
#endif

#endif // WORLD_DEBUG_TOOLS_H