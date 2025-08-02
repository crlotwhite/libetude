#include "world_debug_tools.h"
#include "world_error.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// 디버그 이벤트 콜백
static DebugEventCallback g_event_callback = NULL;
static void* g_callback_user_data = NULL;

/**
 * @brief 현재 시간을 마이크로초로 반환
 */
static uint64_t get_current_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/**
 * @brief 노드 상태를 문자열로 변환
 */
static const char* node_state_to_string(NodeExecutionState state) {
    switch (state) {
        case NODE_STATE_IDLE: return "IDLE";
        case NODE_STATE_RUNNING: return "RUNNING";
        case NODE_STATE_COMPLETED: return "COMPLETED";
        case NODE_STATE_ERROR: return "ERROR";
        case NODE_STATE_BLOCKED: return "BLOCKED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 디버그 이벤트 타입을 문자열로 변환
 */
static const char* debug_event_type_to_string(DebugEventType type) {
    switch (type) {
        case DEBUG_EVENT_NODE_START: return "NODE_START";
        case DEBUG_EVENT_NODE_COMPLETE: return "NODE_COMPLETE";
        case DEBUG_EVENT_NODE_ERROR: return "NODE_ERROR";
        case DEBUG_EVENT_DATA_TRANSFER: return "DATA_TRANSFER";
        case DEBUG_EVENT_MEMORY_ALLOC: return "MEMORY_ALLOC";
        case DEBUG_EVENT_MEMORY_FREE: return "MEMORY_FREE";
        case DEBUG_EVENT_GRAPH_START: return "GRAPH_START";
        case DEBUG_EVENT_GRAPH_COMPLETE: return "GRAPH_COMPLETE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 디버그 이벤트 추가
 */
static int add_debug_event(DebugContext* debug_ctx, DebugEventType type,
                          const char* node_name, const char* message) {
    if (!debug_ctx || !debug_ctx->is_enabled) {
        return 0;
    }

    if (debug_ctx->event_count >= debug_ctx->max_events) {
        WORLD_LOG_WARNING(DEBUG, "디버그 이벤트 배열이 가득참");
        return -1;
    }

    DebugEvent* event = &debug_ctx->events[debug_ctx->event_count];
    event->type = type;
    event->timestamp_us = get_current_time_us();

    if (node_name) {
        strncpy(event->node_name, node_name, sizeof(event->node_name) - 1);
        event->node_name[sizeof(event->node_name) - 1] = '\0';
    } else {
        event->node_name[0] = '\0';
    }

    if (message) {
        strncpy(event->message, message, sizeof(event->message) - 1);
        event->message[sizeof(event->message) - 1] = '\0';
    } else {
        event->message[0] = '\0';
    }

    event->data = NULL;
    event->data_size = 0;

    debug_ctx->event_count++;

    // 로그 파일에 기록
    if (debug_ctx->log_file) {
        fprintf(debug_ctx->log_file, "[%lu] %s: %s - %s\n",
               event->timestamp_us, debug_event_type_to_string(type),
               node_name ? node_name : "N/A", message ? message : "");
        fflush(debug_ctx->log_file);
    }

    // 콜백 호출
    if (g_event_callback) {
        g_event_callback(event, g_callback_user_data);
    }

    return 0;
}

DebugContext* world_debug_create(int max_nodes, int max_traces, int max_events) {
    if (max_nodes <= 0 || max_traces <= 0 || max_events <= 0) {
        WORLD_LOG_ERROR(DEBUG, "잘못된 매개변수");
        return NULL;
    }

    DebugContext* debug_ctx = calloc(1, sizeof(DebugContext));
    if (!debug_ctx) {
        WORLD_LOG_ERROR(DEBUG, "디버그 컨텍스트 메모리 할당 실패");
        return NULL;
    }

    // 노드 디버그 정보 배열 할당
    debug_ctx->node_infos = calloc(max_nodes, sizeof(NodeDebugInfo));
    if (!debug_ctx->node_infos) {
        WORLD_LOG_ERROR(DEBUG, "노드 디버그 정보 배열 메모리 할당 실패");
        free(debug_ctx);
        return NULL;
    }

    // 데이터 흐름 추적 배열 할당
    debug_ctx->flow_traces = calloc(max_traces, sizeof(DataFlowTrace));
    if (!debug_ctx->flow_traces) {
        WORLD_LOG_ERROR(DEBUG, "데이터 흐름 추적 배열 메모리 할당 실패");
        free(debug_ctx->node_infos);
        free(debug_ctx);
        return NULL;
    }

    // 디버그 이벤트 배열 할당
    debug_ctx->events = calloc(max_events, sizeof(DebugEvent));
    if (!debug_ctx->events) {
        WORLD_LOG_ERROR(DEBUG, "디버그 이벤트 배열 메모리 할당 실패");
        free(debug_ctx->flow_traces);
        free(debug_ctx->node_infos);
        free(debug_ctx);
        return NULL;
    }

    debug_ctx->max_nodes = max_nodes;
    debug_ctx->max_traces = max_traces;
    debug_ctx->max_events = max_events;
    debug_ctx->is_enabled = true;
    debug_ctx->trace_data_flow = true;
    debug_ctx->trace_memory = true;
    debug_ctx->verbose_logging = false;
    debug_ctx->log_file = NULL;

    WORLD_LOG_INFO(DEBUG, "디버그 컨텍스트 생성 완료 (노드: %d, 추적: %d, 이벤트: %d)",
                  max_nodes, max_traces, max_events);
    return debug_ctx;
}

void world_debug_set_enabled(DebugContext* debug_ctx, bool enabled) {
    if (!debug_ctx) {
        return;
    }

    debug_ctx->is_enabled = enabled;
    WORLD_LOG_INFO(DEBUG, "디버깅 %s", enabled ? "활성화" : "비활성화");
}

int world_debug_set_log_file(DebugContext* debug_ctx, const char* log_file_path) {
    if (!debug_ctx || !log_file_path) {
        WORLD_LOG_ERROR(DEBUG, "잘못된 매개변수");
        return -1;
    }

    // 기존 로그 파일 닫기
    if (debug_ctx->log_file) {
        fclose(debug_ctx->log_file);
        debug_ctx->log_file = NULL;
    }

    // 새 로그 파일 열기
    debug_ctx->log_file = fopen(log_file_path, "w");
    if (!debug_ctx->log_file) {
        WORLD_LOG_ERROR(DEBUG, "로그 파일 열기 실패: %s", log_file_path);
        return -1;
    }

    strncpy(debug_ctx->log_file_path, log_file_path, sizeof(debug_ctx->log_file_path) - 1);
    debug_ctx->log_file_path[sizeof(debug_ctx->log_file_path) - 1] = '\0';

    // 로그 파일 헤더 작성
    fprintf(debug_ctx->log_file, "# WORLD Debug Log\n");
    fprintf(debug_ctx->log_file, "# Generated at: %lu\n", get_current_time_us());
    fprintf(debug_ctx->log_file, "# Format: [timestamp_us] event_type: node_name - message\n\n");
    fflush(debug_ctx->log_file);

    WORLD_LOG_INFO(DEBUG, "로그 파일 설정 완료: %s", log_file_path);
    return 0;
}

int world_debug_trace_node_start(DebugContext* debug_ctx, const char* node_name, int node_id) {
    if (!debug_ctx || !node_name) {
        return -1;
    }

    if (!debug_ctx->is_enabled) {
        return 0;
    }

    // 기존 노드 정보 찾기 또는 새로 추가
    NodeDebugInfo* node_info = NULL;
    for (int i = 0; i < debug_ctx->node_count; i++) {
        if (debug_ctx->node_infos[i].node_id == node_id) {
            node_info = &debug_ctx->node_infos[i];
            break;
        }
    }

    if (!node_info) {
        if (debug_ctx->node_count >= debug_ctx->max_nodes) {
            WORLD_LOG_ERROR(DEBUG, "노드 디버그 정보 배열이 가득함");
            return -1;
        }
        node_info = &debug_ctx->node_infos[debug_ctx->node_count++];
        node_info->node_id = node_id;
        strncpy(node_info->node_name, node_name, sizeof(node_info->node_name) - 1);
        node_info->node_name[sizeof(node_info->node_name) - 1] = '\0';
    }

    node_info->state = NODE_STATE_RUNNING;
    node_info->start_time_us = get_current_time_us();
    node_info->execution_count++;

    char message[256];
    snprintf(message, sizeof(message), "노드 실행 시작 (실행 횟수: %d)", node_info->execution_count);
    add_debug_event(debug_ctx, DEBUG_EVENT_NODE_START, node_name, message);

    return 0;
}

int world_debug_trace_node_complete(DebugContext* debug_ctx, const char* node_name,
                                   int node_id, uint64_t execution_time_us) {
    if (!debug_ctx || !node_name) {
        return -1;
    }

    if (!debug_ctx->is_enabled) {
        return 0;
    }

    // 노드 정보 찾기
    NodeDebugInfo* node_info = NULL;
    for (int i = 0; i < debug_ctx->node_count; i++) {
        if (debug_ctx->node_infos[i].node_id == node_id) {
            node_info = &debug_ctx->node_infos[i];
            break;
        }
    }

    if (!node_info) {
        WORLD_LOG_WARNING(DEBUG, "노드 정보를 찾을 수 없음: %s (ID: %d)", node_name, node_id);
        return -1;
    }

    node_info->state = NODE_STATE_COMPLETED;
    node_info->end_time_us = get_current_time_us();
    node_info->execution_time_us = execution_time_us;

    char message[256];
    snprintf(message, sizeof(message), "노드 실행 완료 (실행 시간: %lu μs)", execution_time_us);
    add_debug_event(debug_ctx, DEBUG_EVENT_NODE_COMPLETE, node_name, message);

    return 0;
}

int world_debug_trace_node_error(DebugContext* debug_ctx, const char* node_name,
                                int node_id, int error_code, const char* error_message) {
    if (!debug_ctx || !node_name) {
        return -1;
    }

    if (!debug_ctx->is_enabled) {
        return 0;
    }

    // 노드 정보 찾기
    NodeDebugInfo* node_info = NULL;
    for (int i = 0; i < debug_ctx->node_count; i++) {
        if (debug_ctx->node_infos[i].node_id == node_id) {
            node_info = &debug_ctx->node_infos[i];
            break;
        }
    }

    if (!node_info) {
        WORLD_LOG_WARNING(DEBUG, "노드 정보를 찾을 수 없음: %s (ID: %d)", node_name, node_id);
        return -1;
    }

    node_info->state = NODE_STATE_ERROR;
    node_info->error_code = error_code;
    if (error_message) {
        strncpy(node_info->error_message, error_message, sizeof(node_info->error_message) - 1);
        node_info->error_message[sizeof(node_info->error_message) - 1] = '\0';
    }

    char message[512];
    snprintf(message, sizeof(message), "노드 오류 발생 (코드: %d, 메시지: %s)",
             error_code, error_message ? error_message : "N/A");
    add_debug_event(debug_ctx, DEBUG_EVENT_NODE_ERROR, node_name, message);

    return 0;
}

int world_debug_trace_data_flow(DebugContext* debug_ctx,
                               const char* source_node, const char* dest_node,
                               int source_port, int dest_port,
                               size_t data_size, const char* data_type) {
    if (!debug_ctx || !source_node || !dest_node) {
        return -1;
    }

    if (!debug_ctx->is_enabled || !debug_ctx->trace_data_flow) {
        return 0;
    }

    if (debug_ctx->trace_count >= debug_ctx->max_traces) {
        WORLD_LOG_WARNING(DEBUG, "데이터 흐름 추적 배열이 가득참");
        return -1;
    }

    DataFlowTrace* trace = &debug_ctx->flow_traces[debug_ctx->trace_count++];
    strncpy(trace->source_node, source_node, sizeof(trace->source_node) - 1);
    trace->source_node[sizeof(trace->source_node) - 1] = '\0';
    strncpy(trace->dest_node, dest_node, sizeof(trace->dest_node) - 1);
    trace->dest_node[sizeof(trace->dest_node) - 1] = '\0';

    trace->source_port = source_port;
    trace->dest_port = dest_port;
    trace->data_size = data_size;
    trace->transfer_time_us = get_current_time_us();
    trace->is_valid = true;

    if (data_type) {
        strncpy(trace->data_type, data_type, sizeof(trace->data_type) - 1);
        trace->data_type[sizeof(trace->data_type) - 1] = '\0';
    } else {
        strcpy(trace->data_type, "unknown");
    }

    char message[512];
    snprintf(message, sizeof(message), "데이터 전송: %s[%d] -> %s[%d] (%zu bytes, %s)",
             source_node, source_port, dest_node, dest_port, data_size,
             data_type ? data_type : "unknown");
    add_debug_event(debug_ctx, DEBUG_EVENT_DATA_TRANSFER, source_node, message);

    return 0;
}

int world_debug_trace_memory_alloc(DebugContext* debug_ctx, const char* node_name,
                                  size_t size, void* ptr) {
    if (!debug_ctx || !node_name) {
        return -1;
    }

    if (!debug_ctx->is_enabled || !debug_ctx->trace_memory) {
        return 0;
    }

    // 노드 정보 찾기
    NodeDebugInfo* node_info = NULL;
    for (int i = 0; i < debug_ctx->node_count; i++) {
        if (strcmp(debug_ctx->node_infos[i].node_name, node_name) == 0) {
            node_info = &debug_ctx->node_infos[i];
            break;
        }
    }

    if (node_info) {
        node_info->memory_allocated += size;
        if (node_info->memory_allocated > node_info->memory_peak) {
            node_info->memory_peak = node_info->memory_allocated;
        }
    }

    char message[256];
    snprintf(message, sizeof(message), "메모리 할당: %zu bytes (ptr: %p)", size, ptr);
    add_debug_event(debug_ctx, DEBUG_EVENT_MEMORY_ALLOC, node_name, message);

    return 0;
}

int world_debug_trace_memory_free(DebugContext* debug_ctx, const char* node_name, void* ptr) {
    if (!debug_ctx || !node_name) {
        return -1;
    }

    if (!debug_ctx->is_enabled || !debug_ctx->trace_memory) {
        return 0;
    }

    char message[256];
    snprintf(message, sizeof(message), "메모리 해제 (ptr: %p)", ptr);
    add_debug_event(debug_ctx, DEBUG_EVENT_MEMORY_FREE, node_name, message);

    return 0;
}

const NodeDebugInfo* world_debug_get_node_info(const DebugContext* debug_ctx,
                                               const char* node_name) {
    if (!debug_ctx || !node_name) {
        return NULL;
    }

    for (int i = 0; i < debug_ctx->node_count; i++) {
        if (strcmp(debug_ctx->node_infos[i].node_name, node_name) == 0) {
            return &debug_ctx->node_infos[i];
        }
    }

    return NULL;
}

void world_debug_print_node_states(const DebugContext* debug_ctx, FILE* output_file) {
    if (!debug_ctx) {
        return;
    }

    FILE* out = output_file ? output_file : stdout;

    fprintf(out, "\n=== 노드 실행 상태 ===\n");
    fprintf(out, "%-20s %-10s %-15s %-15s %-10s %-10s\n",
           "노드 이름", "상태", "실행 시간(μs)", "메모리(bytes)", "실행 횟수", "오류 코드");
    fprintf(out, "--------------------------------------------------------------------------------\n");

    for (int i = 0; i < debug_ctx->node_count; i++) {
        const NodeDebugInfo* info = &debug_ctx->node_infos[i];
        fprintf(out, "%-20s %-10s %-15lu %-15zu %-10d %-10d\n",
               info->node_name, node_state_to_string(info->state),
               info->execution_time_us, info->memory_peak,
               info->execution_count, info->error_code);

        if (info->state == NODE_STATE_ERROR && info->error_message[0] != '\0') {
            fprintf(out, "    오류 메시지: %s\n", info->error_message);
        }
    }
    fprintf(out, "\n");
}

void world_debug_print_data_flows(const DebugContext* debug_ctx, FILE* output_file) {
    if (!debug_ctx) {
        return;
    }

    FILE* out = output_file ? output_file : stdout;

    fprintf(out, "\n=== 데이터 흐름 추적 ===\n");
    fprintf(out, "%-15s %-15s %-8s %-8s %-12s %-15s %-10s\n",
           "소스 노드", "대상 노드", "소스포트", "대상포트", "데이터크기", "전송시간(μs)", "데이터타입");
    fprintf(out, "-------------------------------------------------------------------------------------\n");

    for (int i = 0; i < debug_ctx->trace_count; i++) {
        const DataFlowTrace* trace = &debug_ctx->flow_traces[i];
        fprintf(out, "%-15s %-15s %-8d %-8d %-12zu %-15lu %-10s\n",
               trace->source_node, trace->dest_node,
               trace->source_port, trace->dest_port,
               trace->data_size, trace->transfer_time_us, trace->data_type);
    }
    fprintf(out, "\n");
}

int world_debug_generate_performance_report(const DebugContext* debug_ctx,
                                           const char* output_path) {
    if (!debug_ctx || !output_path) {
        WORLD_LOG_ERROR(DEBUG, "잘못된 매개변수");
        return -1;
    }

    FILE* report_file = fopen(output_path, "w");
    if (!report_file) {
        WORLD_LOG_ERROR(DEBUG, "성능 보고서 파일 열기 실패: %s", output_path);
        return -1;
    }

    fprintf(report_file, "# WORLD 성능 분석 보고서\n");
    fprintf(report_file, "생성 시간: %lu\n\n", get_current_time_us());

    // 전체 통계
    uint64_t total_execution_time = 0;
    size_t total_memory_usage = 0;
    int error_count = 0;

    for (int i = 0; i < debug_ctx->node_count; i++) {
        const NodeDebugInfo* info = &debug_ctx->node_infos[i];
        total_execution_time += info->execution_time_us;
        total_memory_usage += info->memory_peak;
        if (info->state == NODE_STATE_ERROR) {
            error_count++;
        }
    }

    fprintf(report_file, "## 전체 통계\n");
    fprintf(report_file, "- 총 노드 수: %d\n", debug_ctx->node_count);
    fprintf(report_file, "- 총 실행 시간: %lu μs (%.2f ms)\n",
           total_execution_time, total_execution_time / 1000.0);
    fprintf(report_file, "- 총 메모리 사용량: %zu bytes (%.2f KB)\n",
           total_memory_usage, total_memory_usage / 1024.0);
    fprintf(report_file, "- 오류 발생 노드 수: %d\n", error_count);
    fprintf(report_file, "- 데이터 흐름 추적 수: %d\n\n", debug_ctx->trace_count);

    // 노드별 성능 분석
    fprintf(report_file, "## 노드별 성능 분석\n");
    world_debug_print_node_states(debug_ctx, report_file);

    // 데이터 흐름 분석
    fprintf(report_file, "## 데이터 흐름 분석\n");
    world_debug_print_data_flows(debug_ctx, report_file);

    // 병목 지점 분석
    fprintf(report_file, "## 병목 지점 분석\n");
    uint64_t max_execution_time = 0;
    const char* slowest_node = NULL;

    for (int i = 0; i < debug_ctx->node_count; i++) {
        const NodeDebugInfo* info = &debug_ctx->node_infos[i];
        if (info->execution_time_us > max_execution_time) {
            max_execution_time = info->execution_time_us;
            slowest_node = info->node_name;
        }
    }

    if (slowest_node) {
        fprintf(report_file, "- 가장 느린 노드: %s (%.2f ms)\n",
               slowest_node, max_execution_time / 1000.0);
        fprintf(report_file, "- 전체 실행 시간의 %.1f%% 차지\n",
               (double)max_execution_time / total_execution_time * 100.0);
    }

    fclose(report_file);

    WORLD_LOG_INFO(DEBUG, "성능 보고서 생성 완료: %s", output_path);
    return 0;
}

int world_debug_generate_error_report(const DebugContext* debug_ctx,
                                     const char* output_path) {
    if (!debug_ctx || !output_path) {
        WORLD_LOG_ERROR(DEBUG, "잘못된 매개변수");
        return -1;
    }

    FILE* report_file = fopen(output_path, "w");
    if (!report_file) {
        WORLD_LOG_ERROR(DEBUG, "오류 보고서 파일 열기 실패: %s", output_path);
        return -1;
    }

    fprintf(report_file, "# WORLD 오류 진단 보고서\n");
    fprintf(report_file, "생성 시간: %lu\n\n", get_current_time_us());

    // 오류 발생 노드들
    fprintf(report_file, "## 오류 발생 노드\n");
    bool has_errors = false;

    for (int i = 0; i < debug_ctx->node_count; i++) {
        const NodeDebugInfo* info = &debug_ctx->node_infos[i];
        if (info->state == NODE_STATE_ERROR) {
            has_errors = true;
            fprintf(report_file, "### %s (ID: %d)\n", info->node_name, info->node_id);
            fprintf(report_file, "- 오류 코드: %d\n", info->error_code);
            fprintf(report_file, "- 오류 메시지: %s\n", info->error_message);
            fprintf(report_file, "- 실행 횟수: %d\n", info->execution_count);
            fprintf(report_file, "- 메모리 사용량: %zu bytes\n\n", info->memory_peak);
        }
    }

    if (!has_errors) {
        fprintf(report_file, "오류가 발생한 노드가 없습니다.\n\n");
    }

    // 오류 이벤트들
    fprintf(report_file, "## 오류 이벤트 로그\n");
    bool has_error_events = false;

    for (int i = 0; i < debug_ctx->event_count; i++) {
        const DebugEvent* event = &debug_ctx->events[i];
        if (event->type == DEBUG_EVENT_NODE_ERROR) {
            has_error_events = true;
            fprintf(report_file, "[%lu] %s: %s\n",
                   event->timestamp_us, event->node_name, event->message);
        }
    }

    if (!has_error_events) {
        fprintf(report_file, "오류 이벤트가 없습니다.\n");
    }

    fclose(report_file);

    WORLD_LOG_INFO(DEBUG, "오류 보고서 생성 완료: %s", output_path);
    return 0;
}

void world_debug_set_event_callback(DebugContext* debug_ctx,
                                   DebugEventCallback callback, void* user_data) {
    if (!debug_ctx) {
        return;
    }

    g_event_callback = callback;
    g_callback_user_data = user_data;

    WORLD_LOG_INFO(DEBUG, "디버그 이벤트 콜백 설정 완료");
}

void world_debug_reset(DebugContext* debug_ctx) {
    if (!debug_ctx) {
        return;
    }

    debug_ctx->node_count = 0;
    debug_ctx->trace_count = 0;
    debug_ctx->event_count = 0;

    memset(debug_ctx->node_infos, 0, sizeof(NodeDebugInfo) * debug_ctx->max_nodes);
    memset(debug_ctx->flow_traces, 0, sizeof(DataFlowTrace) * debug_ctx->max_traces);
    memset(debug_ctx->events, 0, sizeof(DebugEvent) * debug_ctx->max_events);

    WORLD_LOG_INFO(DEBUG, "디버그 컨텍스트 초기화 완료");
}

void world_debug_destroy(DebugContext* debug_ctx) {
    if (!debug_ctx) {
        return;
    }

    if (debug_ctx->log_file) {
        fclose(debug_ctx->log_file);
    }

    free(debug_ctx->node_infos);
    free(debug_ctx->flow_traces);
    free(debug_ctx->events);
    free(debug_ctx);

    WORLD_LOG_INFO(DEBUG, "디버그 컨텍스트 해제 완료");
}