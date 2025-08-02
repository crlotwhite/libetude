#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>

// 테스트 대상 헤더 파일들
#include "../../examples/world4utau/include/world_visualization.h"
#include "../../examples/world4utau/include/world_debug_tools.h"
#include "../../examples/world4utau/include/world_profiler.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("✗ %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_STRING_EQUAL(expected, actual, message) \
    TEST_ASSERT(strcmp((expected), (actual)) == 0, message)

// 테스트용 임시 파일 경로
static const char* TEST_OUTPUT_DIR = "/tmp/world_debug_test";
static const char* TEST_DOT_FILE = "/tmp/world_debug_test/test_diagram.dot";
static const char* TEST_LOG_FILE = "/tmp/world_debug_test/test_debug.log";
static const char* TEST_REPORT_FILE = "/tmp/world_debug_test/test_report.txt";

/**
 * @brief 테스트 환경 설정
 */
static void setup_test_environment(void) {
    // 테스트 출력 디렉토리 생성
    mkdir(TEST_OUTPUT_DIR, 0755);

    printf("=== WORLD 디버깅 도구 단위 테스트 시작 ===\n\n");
}

/**
 * @brief 테스트 환경 정리
 */
static void cleanup_test_environment(void) {
    // 테스트 파일들 정리
    unlink(TEST_DOT_FILE);
    unlink(TEST_LOG_FILE);
    unlink(TEST_REPORT_FILE);
    rmdir(TEST_OUTPUT_DIR);

    printf("\n=== 테스트 결과 ===\n");
    printf("총 테스트: %d\n", tests_run);
    printf("성공: %d\n", tests_passed);
    printf("실패: %d\n", tests_failed);
    printf("성공률: %.1f%%\n", tests_run > 0 ? (float)tests_passed / tests_run * 100.0 : 0.0);
}

/**
 * @brief 시각화 컨텍스트 생성/해제 테스트
 */
static void test_visualization_context_lifecycle(void) {
    printf("--- 시각화 컨텍스트 생명주기 테스트 ---\n");

    VisualizationConfig config = {
        .show_execution_time = true,
        .show_data_flow = true,
        .show_memory_usage = true,
        .use_colors = true
    };
    strcpy(config.output_format, "dot");

    // 시각화 컨텍스트 생성 테스트
    VisualizationContext* viz_ctx = world_visualization_create(&config);
    TEST_ASSERT_NOT_NULL(viz_ctx, "시각화 컨텍스트 생성");

    if (viz_ctx) {
        // 설정 확인
        TEST_ASSERT_EQUAL(true, viz_ctx->config.show_execution_time, "실행 시간 표시 설정");
        TEST_ASSERT_EQUAL(true, viz_ctx->config.show_data_flow, "데이터 흐름 표시 설정");
        TEST_ASSERT_EQUAL(true, viz_ctx->config.use_colors, "색상 사용 설정");

        // 시각화 컨텍스트 해제
        world_visualization_destroy(viz_ctx);
        printf("✓ 시각화 컨텍스트 해제\n");
    }

    // NULL 매개변수 테스트
    VisualizationContext* null_ctx = world_visualization_create(NULL);
    TEST_ASSERT_NULL(null_ctx, "NULL 설정으로 생성 시 NULL 반환");

    printf("\n");
}

/**
 * @brief DSP 다이어그램 시각화 테스트
 */
static void test_dsp_diagram_visualization(void) {
    printf("--- DSP 다이어그램 시각화 테스트 ---\n");

    VisualizationConfig config = {
        .show_execution_time = true,
        .show_data_flow = true,
        .use_colors = true
    };
    strcpy(config.output_format, "dot");

    VisualizationContext* viz_ctx = world_visualization_create(&config);
    TEST_ASSERT_NOT_NULL(viz_ctx, "시각화 컨텍스트 생성");

    if (viz_ctx) {
        // 테스트용 DSP 블록 다이어그램 생성
        DSPBlockDiagram diagram = {0};
        diagram.block_count = 3;
        diagram.blocks = calloc(3, sizeof(DSPBlock));

        // 테스트 블록들 설정
        strcpy(diagram.blocks[0].name, "AudioInput");
        strcpy(diagram.blocks[1].name, "F0Extraction");
        strcpy(diagram.blocks[2].name, "AudioOutput");

        diagram.connection_count = 2;
        diagram.connections = calloc(2, sizeof(DSPConnection));

        // 테스트 연결들 설정
        diagram.connections[0].source_block_id = 0;
        diagram.connections[0].dest_block_id = 1;
        diagram.connections[0].source_port = 0;
        diagram.connections[0].dest_port = 0;
        diagram.connections[0].buffer_size = 1024;

        diagram.connections[1].source_block_id = 1;
        diagram.connections[1].dest_block_id = 2;
        diagram.connections[1].source_port = 0;
        diagram.connections[1].dest_port = 0;
        diagram.connections[1].buffer_size = 512;

        // 실행 통계 추가
        world_visualization_add_execution_stats(viz_ctx, "AudioInput", 1.5, 2048);
        world_visualization_add_execution_stats(viz_ctx, "F0Extraction", 15.2, 8192);
        world_visualization_add_execution_stats(viz_ctx, "AudioOutput", 0.8, 1024);

        // DSP 다이어그램 DOT 파일 생성
        int result = world_visualization_export_dsp_diagram(viz_ctx, &diagram, TEST_DOT_FILE);
        TEST_ASSERT_EQUAL(0, result, "DSP 다이어그램 DOT 파일 생성");

        // 파일 존재 확인
        FILE* test_file = fopen(TEST_DOT_FILE, "r");
        TEST_ASSERT_NOT_NULL(test_file, "생성된 DOT 파일 존재 확인");
        if (test_file) {
            fclose(test_file);
        }

        // 메모리 정리
        free(diagram.blocks);
        free(diagram.connections);
        world_visualization_destroy(viz_ctx);
    }

    printf("\n");
}

/**
 * @brief 디버그 컨텍스트 생성/해제 테스트
 */
static void test_debug_context_lifecycle(void) {
    printf("--- 디버그 컨텍스트 생명주기 테스트 ---\n");

    // 디버그 컨텍스트 생성 테스트
    DebugContext* debug_ctx = world_debug_create(10, 20, 100);
    TEST_ASSERT_NOT_NULL(debug_ctx, "디버그 컨텍스트 생성");

    if (debug_ctx) {
        // 초기 상태 확인
        TEST_ASSERT_EQUAL(10, debug_ctx->max_nodes, "최대 노드 수 설정");
        TEST_ASSERT_EQUAL(20, debug_ctx->max_traces, "최대 추적 수 설정");
        TEST_ASSERT_EQUAL(100, debug_ctx->max_events, "최대 이벤트 수 설정");
        TEST_ASSERT_EQUAL(true, debug_ctx->is_enabled, "디버깅 기본 활성화");
        TEST_ASSERT_EQUAL(0, debug_ctx->node_count, "초기 노드 수 0");
        TEST_ASSERT_EQUAL(0, debug_ctx->trace_count, "초기 추적 수 0");
        TEST_ASSERT_EQUAL(0, debug_ctx->event_count, "초기 이벤트 수 0");

        // 디버깅 활성화/비활성화 테스트
        world_debug_set_enabled(debug_ctx, false);
        TEST_ASSERT_EQUAL(false, debug_ctx->is_enabled, "디버깅 비활성화");

        world_debug_set_enabled(debug_ctx, true);
        TEST_ASSERT_EQUAL(true, debug_ctx->is_enabled, "디버깅 재활성화");

        // 로그 파일 설정 테스트
        int log_result = world_debug_set_log_file(debug_ctx, TEST_LOG_FILE);
        TEST_ASSERT_EQUAL(0, log_result, "로그 파일 설정");
        TEST_ASSERT_NOT_NULL(debug_ctx->log_file, "로그 파일 핸들 생성");

        // 디버그 컨텍스트 해제
        world_debug_destroy(debug_ctx);
        printf("✓ 디버그 컨텍스트 해제\n");
    }

    // 잘못된 매개변수 테스트
    DebugContext* invalid_ctx = world_debug_create(0, 10, 10);
    TEST_ASSERT_NULL(invalid_ctx, "잘못된 매개변수로 생성 시 NULL 반환");

    printf("\n");
}

/**
 * @brief 노드 실행 추적 테스트
 */
static void test_node_execution_tracing(void) {
    printf("--- 노드 실행 추적 테스트 ---\n");

    DebugContext* debug_ctx = world_debug_create(5, 10, 50);
    TEST_ASSERT_NOT_NULL(debug_ctx, "디버그 컨텍스트 생성");

    if (debug_ctx) {
        // 노드 실행 시작 추적
        int start_result = world_debug_trace_node_start(debug_ctx, "TestNode", 1);
        TEST_ASSERT_EQUAL(0, start_result, "노드 실행 시작 추적");
        TEST_ASSERT_EQUAL(1, debug_ctx->node_count, "노드 정보 추가 확인");

        // 노드 정보 확인
        const NodeDebugInfo* node_info = world_debug_get_node_info(debug_ctx, "TestNode");
        TEST_ASSERT_NOT_NULL(node_info, "노드 정보 조회");
        if (node_info) {
            TEST_ASSERT_STRING_EQUAL("TestNode", node_info->node_name, "노드 이름 확인");
            TEST_ASSERT_EQUAL(1, node_info->node_id, "노드 ID 확인");
            TEST_ASSERT_EQUAL(NODE_STATE_RUNNING, node_info->state, "노드 실행 상태 확인");
            TEST_ASSERT_EQUAL(1, node_info->execution_count, "실행 횟수 확인");
        }

        // 노드 실행 완료 추적
        int complete_result = world_debug_trace_node_complete(debug_ctx, "TestNode", 1, 1500);
        TEST_ASSERT_EQUAL(0, complete_result, "노드 실행 완료 추적");

        // 완료 후 상태 확인
        node_info = world_debug_get_node_info(debug_ctx, "TestNode");
        if (node_info) {
            TEST_ASSERT_EQUAL(NODE_STATE_COMPLETED, node_info->state, "노드 완료 상태 확인");
            TEST_ASSERT_EQUAL(1500, node_info->execution_time_us, "실행 시간 확인");
        }

        // 노드 오류 추적 테스트
        int error_result = world_debug_trace_node_error(debug_ctx, "TestNode", 1, -1, "테스트 오류");
        TEST_ASSERT_EQUAL(0, error_result, "노드 오류 추적");

        node_info = world_debug_get_node_info(debug_ctx, "TestNode");
        if (node_info) {
            TEST_ASSERT_EQUAL(NODE_STATE_ERROR, node_info->state, "노드 오류 상태 확인");
            TEST_ASSERT_EQUAL(-1, node_info->error_code, "오류 코드 확인");
            TEST_ASSERT_STRING_EQUAL("테스트 오류", node_info->error_message, "오류 메시지 확인");
        }

        world_debug_destroy(debug_ctx);
    }

    printf("\n");
}

/**
 * @brief 데이터 흐름 추적 테스트
 */
static void test_data_flow_tracing(void) {
    printf("--- 데이터 흐름 추적 테스트 ---\n");

    DebugContext* debug_ctx = world_debug_create(5, 10, 50);
    TEST_ASSERT_NOT_NULL(debug_ctx, "디버그 컨텍스트 생성");

    if (debug_ctx) {
        // 데이터 흐름 추적
        int flow_result = world_debug_trace_data_flow(debug_ctx, "SourceNode", "DestNode",
                                                     0, 1, 1024, "audio_samples");
        TEST_ASSERT_EQUAL(0, flow_result, "데이터 흐름 추적");
        TEST_ASSERT_EQUAL(1, debug_ctx->trace_count, "데이터 흐름 추적 수 확인");

        // 추적 정보 확인
        if (debug_ctx->trace_count > 0) {
            const DataFlowTrace* trace = &debug_ctx->flow_traces[0];
            TEST_ASSERT_STRING_EQUAL("SourceNode", trace->source_node, "소스 노드 이름 확인");
            TEST_ASSERT_STRING_EQUAL("DestNode", trace->dest_node, "대상 노드 이름 확인");
            TEST_ASSERT_EQUAL(0, trace->source_port, "소스 포트 확인");
            TEST_ASSERT_EQUAL(1, trace->dest_port, "대상 포트 확인");
            TEST_ASSERT_EQUAL(1024, trace->data_size, "데이터 크기 확인");
            TEST_ASSERT_STRING_EQUAL("audio_samples", trace->data_type, "데이터 타입 확인");
            TEST_ASSERT_EQUAL(true, trace->is_valid, "데이터 유효성 확인");
        }

        // 메모리 할당/해제 추적 테스트
        int alloc_result = world_debug_trace_memory_alloc(debug_ctx, "TestNode", 2048, (void*)0x12345678);
        TEST_ASSERT_EQUAL(0, alloc_result, "메모리 할당 추적");

        int free_result = world_debug_trace_memory_free(debug_ctx, "TestNode", (void*)0x12345678);
        TEST_ASSERT_EQUAL(0, free_result, "메모리 해제 추적");

        world_debug_destroy(debug_ctx);
    }

    printf("\n");
}

/**
 * @brief 성능 프로파일러 생성/해제 테스트
 */
static void test_profiler_lifecycle(void) {
    printf("--- 성능 프로파일러 생명주기 테스트 ---\n");

    ProfilerConfig config = {
        .enable_timing = true,
        .enable_memory_tracking = true,
        .enable_cpu_monitoring = false,
        .enable_cache_analysis = false,
        .enable_realtime_monitoring = false,
        .sampling_interval_ms = 10,
        .max_samples = 1000,
        .max_blocks = 50
    };
    strcpy(config.output_format, "json");
    config.generate_charts = false;

    // 프로파일러 생성 테스트
    ProfilerContext* profiler = world_profiler_create(&config);
    TEST_ASSERT_NOT_NULL(profiler, "성능 프로파일러 생성");

    if (profiler) {
        // 설정 확인
        TEST_ASSERT_EQUAL(true, profiler->config.enable_timing, "시간 측정 활성화 설정");
        TEST_ASSERT_EQUAL(true, profiler->config.enable_memory_tracking, "메모리 추적 활성화 설정");
        TEST_ASSERT_EQUAL(10, profiler->config.sampling_interval_ms, "샘플링 간격 설정");
        TEST_ASSERT_EQUAL(1000, profiler->config.max_samples, "최대 샘플 수 설정");
        TEST_ASSERT_EQUAL(50, profiler->config.max_blocks, "최대 블록 수 설정");

        // 초기 상태 확인
        TEST_ASSERT_EQUAL(false, profiler->is_active, "초기 비활성 상태");
        TEST_ASSERT_EQUAL(false, profiler->is_paused, "초기 비일시정지 상태");
        TEST_ASSERT_EQUAL(0, profiler->metric_count, "초기 메트릭 수 0");
        TEST_ASSERT_EQUAL(0, profiler->block_count, "초기 블록 수 0");

        // 프로파일러 해제
        world_profiler_destroy(profiler);
        printf("✓ 성능 프로파일러 해제\n");
    }

    // NULL 매개변수 테스트
    ProfilerContext* null_profiler = world_profiler_create(NULL);
    TEST_ASSERT_NULL(null_profiler, "NULL 설정으로 생성 시 NULL 반환");

    printf("\n");
}

/**
 * @brief 성능 측정 테스트
 */
static void test_performance_measurement(void) {
    printf("--- 성능 측정 테스트 ---\n");

    ProfilerConfig config = {
        .enable_timing = true,
        .enable_memory_tracking = true,
        .max_samples = 100,
        .max_blocks = 10
    };

    ProfilerContext* profiler = world_profiler_create(&config);
    TEST_ASSERT_NOT_NULL(profiler, "성능 프로파일러 생성");

    if (profiler) {
        // 프로파일링 시작
        int start_result = world_profiler_start(profiler);
        TEST_ASSERT_EQUAL(0, start_result, "프로파일링 시작");
        TEST_ASSERT_EQUAL(true, profiler->is_active, "프로파일링 활성 상태");

        // 블록 실행 시간 측정
        int begin_result = world_profiler_begin_block_timing(profiler, "TestBlock", 1);
        TEST_ASSERT_EQUAL(0, begin_result, "블록 실행 시간 측정 시작");

        // 짧은 지연 (실제 작업 시뮬레이션)
        usleep(1000); // 1ms

        int end_result = world_profiler_end_block_timing(profiler, "TestBlock", 1);
        TEST_ASSERT_EQUAL(0, end_result, "블록 실행 시간 측정 종료");

        // 블록 통계 확인
        const BlockPerformanceStats* stats = world_profiler_get_block_stats(profiler, "TestBlock");
        TEST_ASSERT_NOT_NULL(stats, "블록 통계 조회");
        if (stats) {
            TEST_ASSERT_STRING_EQUAL("TestBlock", stats->block_name, "블록 이름 확인");
            TEST_ASSERT_EQUAL(1, stats->block_id, "블록 ID 확인");
            TEST_ASSERT_EQUAL(1, stats->execution_count, "실행 횟수 확인");
        }

        // 메모리 사용량 기록
        int memory_result = world_profiler_record_memory_usage(profiler, "TestBlock", 4096, true);
        TEST_ASSERT_EQUAL(0, memory_result, "메모리 사용량 기록");

        // 처리량 기록
        int throughput_result = world_profiler_record_throughput(profiler, "TestBlock", 48000, 10.0);
        TEST_ASSERT_EQUAL(0, throughput_result, "처리량 기록");

        // 사용자 정의 메트릭 추가
        int custom_result = world_profiler_add_custom_metric(profiler, "CustomMetric", 123.45, "units");
        TEST_ASSERT_EQUAL(0, custom_result, "사용자 정의 메트릭 추가");

        // 프로파일링 일시정지/재개
        world_profiler_pause(profiler);
        TEST_ASSERT_EQUAL(true, profiler->is_paused, "프로파일링 일시정지");

        world_profiler_resume(profiler);
        TEST_ASSERT_EQUAL(false, profiler->is_paused, "프로파일링 재개");

        // 프로파일링 중지
        int stop_result = world_profiler_stop(profiler);
        TEST_ASSERT_EQUAL(0, stop_result, "프로파일링 중지");
        TEST_ASSERT_EQUAL(false, profiler->is_active, "프로파일링 비활성 상태");

        world_profiler_destroy(profiler);
    }

    printf("\n");
}

/**
 * @brief 병목 지점 분석 테스트
 */
static void test_bottleneck_analysis(void) {
    printf("--- 병목 지점 분석 테스트 ---\n");

    ProfilerConfig config = {
        .enable_timing = true,
        .max_samples = 100,
        .max_blocks = 10
    };

    ProfilerContext* profiler = world_profiler_create(&config);
    TEST_ASSERT_NOT_NULL(profiler, "성능 프로파일러 생성");

    if (profiler) {
        world_profiler_start(profiler);

        // 여러 블록의 성능 데이터 시뮬레이션
        world_profiler_begin_block_timing(profiler, "FastBlock", 1);
        world_profiler_end_block_timing(profiler, "FastBlock", 1);

        world_profiler_begin_block_timing(profiler, "SlowBlock", 2);
        world_profiler_end_block_timing(profiler, "SlowBlock", 2);

        world_profiler_begin_block_timing(profiler, "MediumBlock", 3);
        world_profiler_end_block_timing(profiler, "MediumBlock", 3);

        // 병목 지점 분석
        int analysis_result = world_profiler_analyze_bottlenecks(profiler);
        TEST_ASSERT_EQUAL(0, analysis_result, "병목 지점 분석");
        TEST_ASSERT_EQUAL(3, profiler->bottleneck_count, "병목 지점 수 확인");

        // 최적화 권장사항 생성
        int recommendation_result = world_profiler_generate_optimization_recommendations(profiler, TEST_REPORT_FILE);
        TEST_ASSERT_EQUAL(0, recommendation_result, "최적화 권장사항 생성");

        // 파일 존재 확인
        FILE* report_file = fopen(TEST_REPORT_FILE, "r");
        TEST_ASSERT_NOT_NULL(report_file, "최적화 권장사항 파일 존재 확인");
        if (report_file) {
            fclose(report_file);
        }

        world_profiler_stop(profiler);
        world_profiler_destroy(profiler);
    }

    printf("\n");
}

/**
 * @brief 보고서 생성 테스트
 */
static void test_report_generation(void) {
    printf("--- 보고서 생성 테스트 ---\n");

    // 디버그 보고서 테스트
    DebugContext* debug_ctx = world_debug_create(5, 10, 50);
    if (debug_ctx) {
        // 테스트 데이터 추가
        world_debug_trace_node_start(debug_ctx, "TestNode", 1);
        world_debug_trace_node_complete(debug_ctx, "TestNode", 1, 2000);
        world_debug_trace_data_flow(debug_ctx, "Source", "Dest", 0, 1, 1024, "audio");

        // 성능 보고서 생성
        int perf_report_result = world_debug_generate_performance_report(debug_ctx, TEST_REPORT_FILE);
        TEST_ASSERT_EQUAL(0, perf_report_result, "디버그 성능 보고서 생성");

        // 오류 보고서 생성
        char error_report_path[256];
        snprintf(error_report_path, sizeof(error_report_path), "%s/error_report.txt", TEST_OUTPUT_DIR);
        int error_report_result = world_debug_generate_error_report(debug_ctx, error_report_path);
        TEST_ASSERT_EQUAL(0, error_report_result, "디버그 오류 보고서 생성");

        world_debug_destroy(debug_ctx);
    }

    // 프로파일러 보고서 테스트
    ProfilerConfig config = {
        .enable_timing = true,
        .max_samples = 100,
        .max_blocks = 10
    };

    ProfilerContext* profiler = world_profiler_create(&config);
    if (profiler) {
        world_profiler_start(profiler);

        // 테스트 데이터 추가
        world_profiler_begin_block_timing(profiler, "TestBlock", 1);
        world_profiler_end_block_timing(profiler, "TestBlock", 1);

        world_profiler_stop(profiler);

        // JSON 형식 보고서 생성
        char json_report_path[256];
        snprintf(json_report_path, sizeof(json_report_path), "%s/profiler_report.json", TEST_OUTPUT_DIR);
        int json_report_result = world_profiler_generate_report(profiler, json_report_path, "json");
        TEST_ASSERT_EQUAL(0, json_report_result, "프로파일러 JSON 보고서 생성");

        // 텍스트 형식 보고서 생성
        char text_report_path[256];
        snprintf(text_report_path, sizeof(text_report_path), "%s/profiler_report.txt", TEST_OUTPUT_DIR);
        int text_report_result = world_profiler_generate_report(profiler, text_report_path, "text");
        TEST_ASSERT_EQUAL(0, text_report_result, "프로파일러 텍스트 보고서 생성");

        world_profiler_destroy(profiler);
    }

    printf("\n");
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    setup_test_environment();

    // 시각화 도구 테스트
    test_visualization_context_lifecycle();
    test_dsp_diagram_visualization();

    // 디버깅 도구 테스트
    test_debug_context_lifecycle();
    test_node_execution_tracing();
    test_data_flow_tracing();

    // 성능 프로파일링 도구 테스트
    test_profiler_lifecycle();
    test_performance_measurement();
    test_bottleneck_analysis();

    // 보고서 생성 테스트
    test_report_generation();

    cleanup_test_environment();

    return (tests_failed == 0) ? 0 : 1;
}