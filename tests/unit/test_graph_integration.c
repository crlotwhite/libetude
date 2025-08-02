#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

// libetude 헤더
#include <libetude/types.h>
#include <libetude/memory.h>
#include <libetude/error.h>

// world4utau 그래프 엔진 헤더
#include "../../examples/world4utau/include/world_graph_node.h"
#include "../../examples/world4utau/include/world_graph_builder.h"
#include "../../examples/world4utau/include/world_graph_context.h"
#include "../../examples/world4utau/include/world_graph_optimizer.h"
#include "../../examples/world4utau/include/utau_interface.h"

// 테스트 프레임워크 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return 0; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return 1; \
    } while(0)

// 테스트용 더미 데이터
static float test_audio_data[1024];
static UTAUParameters test_utau_params;

/**
 * @brief 테스트 데이터 초기화
 */
static void setup_test_data(void) {
    // 테스트용 오디오 데이터 생성 (사인파)
    for (int i = 0; i < 1024; i++) {
        test_audio_data[i] = sinf(2.0f * M_PI * 440.0f * i / 44100.0f);
    }

    // 테스트용 UTAU 파라미터 설정
    memset(&test_utau_params, 0, sizeof(UTAUParameters));
    strcpy(test_utau_params.input_wav_path, "test_input.wav");
    strcpy(test_utau_params.output_wav_path, "test_output.wav");
    test_utau_params.target_pitch = 440.0f;
    test_utau_params.velocity = 100.0f;
    test_utau_params.volume = 1.0f;
    test_utau_params.sample_rate = 44100;
}

/**
 * @brief 그래프 노드 생성 테스트
 */
static int test_graph_node_creation(void) {
    ETMemoryPool* pool = et_memory_pool_create(1024 * 1024);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    // 오디오 입력 노드 생성 테스트
    WorldGraphNode* input_node = world_graph_node_create_audio_input(
        pool, test_audio_data, 1024, 44100);
    TEST_ASSERT(input_node != NULL, "Audio input node creation failed");
    TEST_ASSERT(input_node->node_type == WORLD_NODE_AUDIO_INPUT, "Wrong node type");

    // F0 추출 노드 생성 테스트
    WorldGraphNode* f0_node = world_graph_node_create_f0_extraction(
        pool, 5.0, 80.0, 800.0);
    TEST_ASSERT(f0_node != NULL, "F0 extraction node creation failed");
    TEST_ASSERT(f0_node->node_type == WORLD_NODE_F0_EXTRACTION, "Wrong node type");

    // 스펙트럼 분석 노드 생성 테스트
    WorldGraphNode* spectrum_node = world_graph_node_create_spectrum_analysis(
        pool, 2048, 3.0);
    TEST_ASSERT(spectrum_node != NULL, "Spectrum analysis node creation failed");
    TEST_ASSERT(spectrum_node->node_type == WORLD_NODE_SPECTRUM_ANALYSIS, "Wrong node type");

    // 합성 노드 생성 테스트
    WorldGraphNode* synthesis_node = world_graph_node_create_synthesis(
        pool, 44100, 5.0);
    TEST_ASSERT(synthesis_node != NULL, "Synthesis node creation failed");
    TEST_ASSERT(synthesis_node->node_type == WORLD_NODE_SYNTHESIS, "Wrong node type");

    // 오디오 출력 노드 생성 테스트
    WorldGraphNode* output_node = world_graph_node_create_audio_output(
        pool, "test_output.wav");
    TEST_ASSERT(output_node != NULL, "Audio output node creation failed");
    TEST_ASSERT(output_node->node_type == WORLD_NODE_AUDIO_OUTPUT, "Wrong node type");

    et_memory_pool_destroy(pool);
    TEST_PASS();
}

/**
 * @brief 그래프 노드 초기화 테스트
 */
static int test_graph_node_initialization(void) {
    ETMemoryPool* pool = et_memory_pool_create(1024 * 1024);
    TEST_ASSERT(pool != NULL, "Memory pool creation failed");

    WorldGraphNode* f0_node = world_graph_node_create_f0_extraction(
        pool, 5.0, 80.0, 800.0);
    TEST_ASSERT(f0_node != NULL, "F0 extraction node creation failed");

    // 노드 초기화 테스트
    ETResult result = world_graph_node_initialize(f0_node);
    TEST_ASSERT(result == ET_SUCCESS, "Node initialization failed");

    et_memory_pool_destroy(pool);
    TEST_PASS();
}

/**
 * @brief 그래프 빌더 생성 및 기본 기능 테스트
 */
static int test_graph_builder_creation(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 초기 상태 확인
    TEST_ASSERT(world_graph_builder_get_node_count(builder) == 0, "Initial node count should be 0");
    TEST_ASSERT(world_graph_builder_get_connection_count(builder) == 0, "Initial connection count should be 0");
    TEST_ASSERT(!world_graph_builder_is_built(builder), "Builder should not be built initially");

    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 그래프 빌더 노드 추가 테스트
 */
static int test_graph_builder_node_management(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 노드 생성
    WorldGraphNode* input_node = world_graph_node_create_audio_input(
        builder->mem_pool, test_audio_data, 1024, 44100);
    TEST_ASSERT(input_node != NULL, "Audio input node creation failed");

    WorldGraphNode* f0_node = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);
    TEST_ASSERT(f0_node != NULL, "F0 extraction node creation failed");

    // 노드 추가
    ETResult result = world_graph_builder_add_node(builder, input_node);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add input node");

    result = world_graph_builder_add_node(builder, f0_node);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to add F0 node");

    // 노드 수 확인
    TEST_ASSERT(world_graph_builder_get_node_count(builder) == 2, "Node count should be 2");

    // 노드 조회
    WorldGraphNode* retrieved_node = world_graph_builder_get_node(builder, 0);
    TEST_ASSERT(retrieved_node != NULL, "Failed to retrieve node");
    TEST_ASSERT(retrieved_node == input_node, "Retrieved wrong node");

    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 그래프 빌더 연결 관리 테스트
 */
static int test_graph_builder_connection_management(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 노드 추가
    WorldGraphNode* input_node = world_graph_node_create_audio_input(
        builder->mem_pool, test_audio_data, 1024, 44100);
    WorldGraphNode* f0_node = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);

    world_graph_builder_add_node(builder, input_node);
    world_graph_builder_add_node(builder, f0_node);

    // 노드 연결
    ETResult result = world_graph_builder_connect_nodes(builder, 0, 0, 1, 0);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to connect nodes");

    // 연결 수 확인
    TEST_ASSERT(world_graph_builder_get_connection_count(builder) == 1, "Connection count should be 1");

    // 연결 존재 확인
    bool has_connection = world_graph_builder_has_connection(builder, 0, 0, 1, 0);
    TEST_ASSERT(has_connection, "Connection should exist");

    // 중복 연결 시도
    result = world_graph_builder_connect_nodes(builder, 0, 0, 1, 0);
    TEST_ASSERT(result == ET_ERROR_ALREADY_EXISTS, "Duplicate connection should be rejected");

    // 연결 해제
    result = world_graph_builder_disconnect_nodes(builder, 0, 0, 1, 0);
    TEST_ASSERT(result == ET_SUCCESS, "Failed to disconnect nodes");

    TEST_ASSERT(world_graph_builder_get_connection_count(builder) == 0, "Connection count should be 0");

    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 그래프 빌드 테스트
 */
static int test_graph_building(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 완전한 WORLD 파이프라인 구성
    WorldGraphNode* input_node = world_graph_node_create_audio_input(
        builder->mem_pool, test_audio_data, 1024, 44100);
    WorldGraphNode* f0_node = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);
    WorldGraphNode* spectrum_node = world_graph_node_create_spectrum_analysis(
        builder->mem_pool, 2048, 3.0);
    WorldGraphNode* synthesis_node = world_graph_node_create_synthesis(
        builder->mem_pool, 44100, 5.0);
    WorldGraphNode* output_node = world_graph_node_create_audio_output(
        builder->mem_pool, "test_output.wav");

    // 노드 추가
    world_graph_builder_add_node(builder, input_node);
    world_graph_builder_add_node(builder, f0_node);
    world_graph_builder_add_node(builder, spectrum_node);
    world_graph_builder_add_node(builder, synthesis_node);
    world_graph_builder_add_node(builder, output_node);

    // 연결 구성
    world_graph_builder_connect_nodes(builder, 0, 0, 1, 0); // input -> f0
    world_graph_builder_connect_nodes(builder, 0, 0, 2, 0); // input -> spectrum
    world_graph_builder_connect_nodes(builder, 1, 0, 3, 0); // f0 -> synthesis
    world_graph_builder_connect_nodes(builder, 2, 0, 3, 1); // spectrum -> synthesis
    world_graph_builder_connect_nodes(builder, 3, 0, 4, 0); // synthesis -> output

    // 그래프 빌드
    ETGraph* graph = world_graph_builder_build(builder);
    TEST_ASSERT(graph != NULL, "Graph build failed");
    TEST_ASSERT(world_graph_builder_is_built(builder), "Builder should be marked as built");

    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 그래프 실행 컨텍스트 생성 테스트
 */
static int test_graph_context_creation(void) {
    WorldGraphContext* context = world_graph_context_create(&test_utau_params);
    TEST_ASSERT(context != NULL, "Graph context creation failed");

    // 초기 상태 확인
    TEST_ASSERT(world_graph_context_get_state(context) == WORLD_GRAPH_STATE_IDLE, "Initial state should be IDLE");
    TEST_ASSERT(!world_graph_context_is_running(context), "Context should not be running initially");
    TEST_ASSERT(!world_graph_context_is_complete(context), "Context should not be complete initially");

    // 파라미터 확인
    const UTAUParameters* params = world_graph_context_get_utau_parameters(context);
    TEST_ASSERT(params != NULL, "UTAU parameters should be available");
    TEST_ASSERT(params->target_pitch == 440.0f, "Target pitch should match");

    world_graph_context_destroy(context);
    TEST_PASS();
}

/**
 * @brief 그래프 실행 테스트
 */
static int test_graph_execution(void) {
    // 그래프 빌더 생성 및 구성
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 간단한 파이프라인 구성
    WorldGraphNode* input_node = world_graph_node_create_audio_input(
        builder->mem_pool, test_audio_data, 1024, 44100);
    WorldGraphNode* output_node = world_graph_node_create_audio_output(
        builder->mem_pool, "test_output.wav");

    world_graph_builder_add_node(builder, input_node);
    world_graph_builder_add_node(builder, output_node);
    world_graph_builder_connect_nodes(builder, 0, 0, 1, 0);

    ETGraph* graph = world_graph_builder_build(builder);
    TEST_ASSERT(graph != NULL, "Graph build failed");

    // 실행 컨텍스트 생성
    WorldGraphContext* context = world_graph_context_create(&test_utau_params);
    TEST_ASSERT(context != NULL, "Graph context creation failed");

    // 그래프 실행
    ETResult result = world_graph_execute(graph, context);
    TEST_ASSERT(result == ET_SUCCESS, "Graph execution failed");

    // 실행 완료 확인
    TEST_ASSERT(world_graph_context_is_complete(context), "Context should be complete");
    TEST_ASSERT(world_graph_context_get_progress(context) == 1.0f, "Progress should be 100%");

    // 실행 시간 확인
    double execution_time = world_graph_context_get_execution_time(context);
    TEST_ASSERT(execution_time > 0.0, "Execution time should be positive");

    world_graph_context_destroy(context);
    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 그래프 최적화 테스트
 */
static int test_graph_optimization(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 최적화 가능한 파이프라인 구성
    WorldGraphNode* input_node = world_graph_node_create_audio_input(
        builder->mem_pool, test_audio_data, 1024, 44100);
    WorldGraphNode* f0_node1 = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);
    WorldGraphNode* f0_node2 = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);
    WorldGraphNode* output_node = world_graph_node_create_audio_output(
        builder->mem_pool, "test_output.wav");

    world_graph_builder_add_node(builder, input_node);
    world_graph_builder_add_node(builder, f0_node1);
    world_graph_builder_add_node(builder, f0_node2);
    world_graph_builder_add_node(builder, output_node);

    world_graph_builder_connect_nodes(builder, 0, 0, 1, 0);
    world_graph_builder_connect_nodes(builder, 1, 0, 2, 0);
    world_graph_builder_connect_nodes(builder, 2, 0, 3, 0);

    // 최적화 옵션 설정
    WorldGraphOptimizationOptions options = world_graph_get_default_optimization_options();
    WorldGraphOptimizationStats stats = {0};

    // 최적화 실행
    ETResult result = world_graph_optimize_with_builder(builder, &options, &stats);
    TEST_ASSERT(result == ET_SUCCESS, "Graph optimization failed");

    // 최적화 통계 확인
    TEST_ASSERT(stats.optimization_time > 0.0, "Optimization time should be positive");
    TEST_ASSERT(stats.estimated_speedup >= 1.0, "Estimated speedup should be at least 1.0x");

    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 노드 융합 최적화 테스트
 */
static int test_node_fusion_optimization(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 융합 가능한 노드들 추가
    WorldGraphNode* f0_node1 = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);
    WorldGraphNode* f0_node2 = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);

    world_graph_builder_add_node(builder, f0_node1);
    world_graph_builder_add_node(builder, f0_node2);

    int initial_node_count = world_graph_builder_get_node_count(builder);

    // 노드 융합 최적화
    WorldNodeFusionInfo* fusion_info = NULL;
    int fusion_count = 0;

    ETResult result = world_graph_optimize_node_fusion(builder, &fusion_info, &fusion_count);
    TEST_ASSERT(result == ET_SUCCESS, "Node fusion optimization failed");

    // 융합 결과 확인 (실제 융합이 발생했는지는 구현에 따라 다름)
    if (fusion_count > 0) {
        TEST_ASSERT(fusion_info != NULL, "Fusion info should be available");
        world_node_fusion_info_destroy(fusion_info, fusion_count);
    }

    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief SIMD 최적화 테스트
 */
static int test_simd_optimization(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // SIMD 최적화 가능한 노드들 추가
    WorldGraphNode* f0_node = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);
    WorldGraphNode* spectrum_node = world_graph_node_create_spectrum_analysis(
        builder->mem_pool, 2048, 3.0);

    world_graph_builder_add_node(builder, f0_node);
    world_graph_builder_add_node(builder, spectrum_node);

    // SIMD 지원 확인
    bool f0_supports_simd = world_graph_node_supports_simd(f0_node);
    bool spectrum_supports_simd = world_graph_node_supports_simd(spectrum_node);

    TEST_ASSERT(f0_supports_simd, "F0 node should support SIMD");
    TEST_ASSERT(spectrum_supports_simd, "Spectrum node should support SIMD");

    // SIMD 최적화 실행
    ETResult result = world_graph_optimize_simd(builder);
    TEST_ASSERT(result == ET_SUCCESS, "SIMD optimization failed");

    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 메모리 사용량 분석 테스트
 */
static int test_memory_usage_analysis(void) {
    WorldGraphBuilderConfig config = {
        .max_nodes = 16,
        .max_connections = 32,
        .memory_pool_size = 1024 * 1024,
        .enable_optimization = true,
        .enable_validation = true
    };

    WorldGraphBuilder* builder = world_graph_builder_create(&config);
    TEST_ASSERT(builder != NULL, "Graph builder creation failed");

    // 다양한 노드들 추가
    WorldGraphNode* input_node = world_graph_node_create_audio_input(
        builder->mem_pool, test_audio_data, 1024, 44100);
    WorldGraphNode* f0_node = world_graph_node_create_f0_extraction(
        builder->mem_pool, 5.0, 80.0, 800.0);
    WorldGraphNode* synthesis_node = world_graph_node_create_synthesis(
        builder->mem_pool, 44100, 5.0);

    world_graph_builder_add_node(builder, input_node);
    world_graph_builder_add_node(builder, f0_node);
    world_graph_builder_add_node(builder, synthesis_node);

    // 메모리 사용량 분석
    size_t* memory_usage_per_node = NULL;
    size_t total_usage = 0;

    ETResult result = world_graph_analyze_memory_usage(builder, &memory_usage_per_node, &total_usage);
    TEST_ASSERT(result == ET_SUCCESS, "Memory usage analysis failed");
    TEST_ASSERT(memory_usage_per_node != NULL, "Memory usage per node should be available");
    TEST_ASSERT(total_usage > 0, "Total memory usage should be positive");

    // 노드별 메모리 사용량 확인
    for (int i = 0; i < world_graph_builder_get_node_count(builder); i++) {
        TEST_ASSERT(memory_usage_per_node[i] > 0, "Each node should use some memory");
    }

    free(memory_usage_per_node);
    world_graph_builder_destroy(builder);
    TEST_PASS();
}

/**
 * @brief 오류 처리 테스트
 */
static int test_error_handling(void) {
    // NULL 포인터 테스트
    WorldGraphNode* null_node = world_graph_node_create_audio_input(NULL, test_audio_data, 1024, 44100);
    TEST_ASSERT(null_node == NULL, "Should fail with NULL memory pool");

    // 잘못된 파라미터 테스트
    ETMemoryPool* pool = et_memory_pool_create(1024 * 1024);
    WorldGraphNode* invalid_node = world_graph_node_create_f0_extraction(pool, -1.0, 80.0, 800.0);
    TEST_ASSERT(invalid_node == NULL, "Should fail with invalid frame period");

    // 빌더 NULL 테스트
    ETResult result = world_graph_builder_add_node(NULL, NULL);
    TEST_ASSERT(result == ET_ERROR_INVALID_PARAMETER, "Should return invalid parameter error");

    // 컨텍스트 NULL 테스트
    WorldGraphContext* null_context = world_graph_context_create(NULL);
    TEST_ASSERT(null_context == NULL, "Should fail with NULL UTAU parameters");

    et_memory_pool_destroy(pool);
    TEST_PASS();
}

/**
 * @brief 모든 테스트 실행
 */
int main(void) {
    printf("=== World4UTAU Graph Engine Integration Tests ===\n\n");

    // 테스트 데이터 초기화
    setup_test_data();

    int passed = 0;
    int total = 0;

    // 테스트 실행
    total++; passed += test_graph_node_creation();
    total++; passed += test_graph_node_initialization();
    total++; passed += test_graph_builder_creation();
    total++; passed += test_graph_builder_node_management();
    total++; passed += test_graph_builder_connection_management();
    total++; passed += test_graph_building();
    total++; passed += test_graph_context_creation();
    total++; passed += test_graph_execution();
    total++; passed += test_graph_optimization();
    total++; passed += test_node_fusion_optimization();
    total++; passed += test_simd_optimization();
    total++; passed += test_memory_usage_analysis();
    total++; passed += test_error_handling();

    // 결과 출력
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", passed, total);
    printf("Success Rate: %.1f%%\n", (float)passed / total * 100.0f);

    if (passed == total) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}