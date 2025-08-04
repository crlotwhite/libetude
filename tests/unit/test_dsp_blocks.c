/**
 * @file test_dsp_blocks.c
 * @brief DSP 블록 시스템 단위 테스트
 *
 * DSP 블록 다이어그램 시스템의 각 구성 요소에 대한 단위 테스트를 수행합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

// libetude 헤더
#include <libetude/types.h>
#include <libetude/memory.h>
#include <libetude/error.h>

// world4utau DSP 블록 헤더들
#include "../../examples/world4utau/include/dsp_blocks.h"
#include "../../examples/world4utau/include/dsp_block_diagram.h"
#include "../../examples/world4utau/include/world_dsp_blocks.h"
#include "../../examples/world4utau/include/dsp_diagram_builder.h"
#include "../../examples/world4utau/include/dsp_block_factory.h"

// 테스트 헬퍼 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return false; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return true; \
    } while(0)

// 전역 테스트 변수
static ETMemoryPool* g_test_mem_pool = NULL;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

// 테스트 헬퍼 함수들
static bool setup_test_environment(void);
static void cleanup_test_environment(void);
static void run_test(bool (*test_func)(void), const char* test_name);
static float* create_test_audio_data(int length, int sample_rate);

// =============================================================================
// DSP 블록 기본 기능 테스트
// =============================================================================

/**
 * @brief DSP 블록 생성 및 해제 테스트
 */
static bool test_dsp_block_create_destroy(void) {
    DSPBlock* block = dsp_block_create("test_block", DSP_BLOCK_TYPE_CUSTOM, 2, 1, g_test_mem_pool);
    TEST_ASSERT(block != NULL, "Block creation failed");
    TEST_ASSERT(strcmp(block->name, "test_block") == 0, "Block name mismatch");
    TEST_ASSERT(block->type == DSP_BLOCK_TYPE_CUSTOM, "Block type mismatch");
    TEST_ASSERT(block->input_port_count == 2, "Input port count mismatch");
    TEST_ASSERT(block->output_port_count == 1, "Output port count mismatch");
    TEST_ASSERT(block->is_enabled == true, "Block should be enabled by default");
    TEST_ASSERT(block->is_initialized == false, "Block should not be initialized by default");

    dsp_block_destroy(block);
    TEST_PASS();
}

/**
 * @brief DSP 블록 포트 설정 테스트
 */
static bool test_dsp_block_port_configuration(void) {
    DSPBlock* block = dsp_block_create("test_block", DSP_BLOCK_TYPE_CUSTOM, 1, 1, g_test_mem_pool);
    TEST_ASSERT(block != NULL, "Block creation failed");

    // 입력 포트 설정
    ETResult result = dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT,
                                        "audio_in", DSP_PORT_TYPE_AUDIO, 1024);
    TEST_ASSERT(result == ET_SUCCESS, "Input port configuration failed");

    DSPPort* input_port = dsp_block_get_input_port(block, 0);
    TEST_ASSERT(input_port != NULL, "Input port retrieval failed");
    TEST_ASSERT(strcmp(input_port->name, "audio_in") == 0, "Input port name mismatch");
    TEST_ASSERT(input_port->type == DSP_PORT_TYPE_AUDIO, "Input port type mismatch");
    TEST_ASSERT(input_port->buffer_size == 1024, "Input port buffer size mismatch");

    // 출력 포트 설정
    result = dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT,
                               "audio_out", DSP_PORT_TYPE_AUDIO, 1024);
    TEST_ASSERT(result == ET_SUCCESS, "Output port configuration failed");

    DSPPort* output_port = dsp_block_get_output_port(block, 0);
    TEST_ASSERT(output_port != NULL, "Output port retrieval failed");
    TEST_ASSERT(strcmp(output_port->name, "audio_out") == 0, "Output port name mismatch");
    TEST_ASSERT(output_port->type == DSP_PORT_TYPE_AUDIO, "Output port type mismatch");
    TEST_ASSERT(output_port->buffer_size == 1024, "Output port buffer size mismatch");

    dsp_block_destroy(block);
    TEST_PASS();
}

/**
 * @brief DSP 블록 초기화 테스트
 */
static bool test_dsp_block_initialization(void) {
    DSPBlock* block = dsp_block_create("test_block", DSP_BLOCK_TYPE_CUSTOM, 1, 1, g_test_mem_pool);
    TEST_ASSERT(block != NULL, "Block creation failed");

    // 포트 설정
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_INPUT, "input", DSP_PORT_TYPE_AUDIO, 512);
    dsp_block_set_port(block, 0, DSP_PORT_DIRECTION_OUTPUT, "output", DSP_PORT_TYPE_AUDIO, 512);

    // 초기화
    ETResult result = dsp_block_initialize(block);
    TEST_ASSERT(result == ET_SUCCESS, "Block initialization failed");
    TEST_ASSERT(block->is_initialized == true, "Block initialization flag not set");

    // 포트 버퍼 할당 확인
    DSPPort* input_port = dsp_block_get_input_port(block, 0);
    DSPPort* output_port = dsp_block_get_output_port(block, 0);
    TEST_ASSERT(input_port->buffer != NULL, "Input port buffer not allocated");
    TEST_ASSERT(output_port->buffer != NULL, "Output port buffer not allocated");

    dsp_block_destroy(block);
    TEST_PASS();
}

// =============================================================================
// DSP 연결 테스트
// =============================================================================

/**
 * @brief DSP 연결 생성 및 검증 테스트
 */
static bool test_dsp_connection_create_validate(void) {
    // 소스 블록 생성
    DSPBlock* source_block = dsp_block_create("source", DSP_BLOCK_TYPE_CUSTOM, 0, 1, g_test_mem_pool);
    TEST_ASSERT(source_block != NULL, "Source block creation failed");
    dsp_block_set_port(source_block, 0, DSP_PORT_DIRECTION_OUTPUT, "out", DSP_PORT_TYPE_AUDIO, 1024);

    // 대상 블록 생성
    DSPBlock* dest_block = dsp_block_create("dest", DSP_BLOCK_TYPE_CUSTOM, 1, 0, g_test_mem_pool);
    TEST_ASSERT(dest_block != NULL, "Destination block creation failed");
    dsp_block_set_port(dest_block, 0, DSP_PORT_DIRECTION_INPUT, "in", DSP_PORT_TYPE_AUDIO, 1024);

    // 연결 생성
    DSPConnection* connection = dsp_connection_create(source_block, 0, dest_block, 0, g_test_mem_pool);
    TEST_ASSERT(connection != NULL, "Connection creation failed");
    TEST_ASSERT(connection->source_block == source_block, "Source block mismatch");
    TEST_ASSERT(connection->dest_block == dest_block, "Destination block mismatch");
    TEST_ASSERT(connection->source_port_id == 0, "Source port ID mismatch");
    TEST_ASSERT(connection->dest_port_id == 0, "Destination port ID mismatch");

    // 연결 검증
    bool is_valid = dsp_connection_validate(connection);
    TEST_ASSERT(is_valid == true, "Connection validation failed");

    // 연결 활성화
    ETResult result = dsp_connection_activate(connection);
    TEST_ASSERT(result == ET_SUCCESS, "Connection activation failed");
    TEST_ASSERT(connection->is_active == true, "Connection not marked as active");

    // 포트 연결 상태 확인
    DSPPort* source_port = dsp_block_get_output_port(source_block, 0);
    DSPPort* dest_port = dsp_block_get_input_port(dest_block, 0);
    TEST_ASSERT(source_port->is_connected == true, "Source port not marked as connected");
    TEST_ASSERT(dest_port->is_connected == true, "Destination port not marked as connected");

    dsp_connection_destroy(connection);
    dsp_block_destroy(source_block);
    dsp_block_destroy(dest_block);
    TEST_PASS();
}

// =============================================================================
// DSP 블록 다이어그램 테스트
// =============================================================================

/**
 * @brief DSP 블록 다이어그램 생성 및 관리 테스트
 */
static bool test_dsp_block_diagram_management(void) {
    DSPBlockDiagram* diagram = dsp_block_diagram_create("test_diagram", 10, 20, g_test_mem_pool);
    TEST_ASSERT(diagram != NULL, "Diagram creation failed");
    TEST_ASSERT(strcmp(diagram->name, "test_diagram") == 0, "Diagram name mismatch");
    TEST_ASSERT(diagram->max_blocks == 10, "Max blocks mismatch");
    TEST_ASSERT(diagram->max_connections == 20, "Max connections mismatch");
    TEST_ASSERT(diagram->block_count == 0, "Initial block count should be 0");
    TEST_ASSERT(diagram->connection_count == 0, "Initial connection count should be 0");

    // 블록 생성 및 추가
    DSPBlock* block1 = dsp_block_create("block1", DSP_BLOCK_TYPE_CUSTOM, 0, 1, g_test_mem_pool);
    DSPBlock* block2 = dsp_block_create("block2", DSP_BLOCK_TYPE_CUSTOM, 1, 0, g_test_mem_pool);

    ETResult result = dsp_block_diagram_add_block(diagram, block1);
    TEST_ASSERT(result == ET_SUCCESS, "Block1 addition failed");
    TEST_ASSERT(diagram->block_count == 1, "Block count after first addition");

    result = dsp_block_diagram_add_block(diagram, block2);
    TEST_ASSERT(result == ET_SUCCESS, "Block2 addition failed");
    TEST_ASSERT(diagram->block_count == 2, "Block count after second addition");

    // 블록 검색
    DSPBlock* found_block = dsp_block_diagram_find_block_by_name(diagram, "block1");
    TEST_ASSERT(found_block != NULL, "Block search by name failed");
    TEST_ASSERT(strcmp(found_block->name, "block1") == 0, "Found block name mismatch");

    dsp_block_diagram_destroy(diagram);
    TEST_PASS();
}

/**
 * @brief DSP 블록 다이어그램 연결 테스트
 */
static bool test_dsp_block_diagram_connections(void) {
    DSPBlockDiagram* diagram = dsp_block_diagram_create("test_diagram", 5, 10, g_test_mem_pool);
    TEST_ASSERT(diagram != NULL, "Diagram creation failed");

    // 블록 생성 및 설정
    DSPBlock* source_block = dsp_block_create("source", DSP_BLOCK_TYPE_CUSTOM, 0, 1, g_test_mem_pool);
    DSPBlock* dest_block = dsp_block_create("dest", DSP_BLOCK_TYPE_CUSTOM, 1, 0, g_test_mem_pool);

    dsp_block_set_port(source_block, 0, DSP_PORT_DIRECTION_OUTPUT, "out", DSP_PORT_TYPE_AUDIO, 1024);
    dsp_block_set_port(dest_block, 0, DSP_PORT_DIRECTION_INPUT, "in", DSP_PORT_TYPE_AUDIO, 1024);

    // 다이어그램에 블록 추가
    dsp_block_diagram_add_block(diagram, source_block);
    dsp_block_diagram_add_block(diagram, dest_block);

    // 연결 생성
    ETResult result = dsp_block_diagram_connect(diagram, source_block->block_id, 0, dest_block->block_id, 0);
    TEST_ASSERT(result == ET_SUCCESS, "Diagram connection failed");
    TEST_ASSERT(diagram->connection_count == 1, "Connection count mismatch");

    // 다이어그램 검증
    bool is_valid = dsp_block_diagram_validate(diagram);
    TEST_ASSERT(is_valid == true, "Diagram validation failed");

    // 다이어그램 빌드
    result = dsp_block_diagram_build(diagram);
    TEST_ASSERT(result == ET_SUCCESS, "Diagram build failed");
    TEST_ASSERT(diagram->is_built == true, "Diagram build flag not set");

    dsp_block_diagram_destroy(diagram);
    TEST_PASS();
}

// =============================================================================
// WORLD DSP 블록 테스트
// =============================================================================

/**
 * @brief 오디오 입력 블록 테스트
 */
static bool test_audio_input_block(void) {
    int audio_length = 44100; // 1초
    int sample_rate = 44100;
    int frame_size = 1024;

    float* test_audio = create_test_audio_data(audio_length, sample_rate);
    TEST_ASSERT(test_audio != NULL, "Test audio creation failed");

    DSPBlock* block = create_audio_input_block("audio_input", test_audio, audio_length,
                                              sample_rate, frame_size, g_test_mem_pool);
    TEST_ASSERT(block != NULL, "Audio input block creation failed");
    TEST_ASSERT(block->type == DSP_BLOCK_TYPE_AUDIO_INPUT, "Block type mismatch");
    TEST_ASSERT(block->input_port_count == 0, "Input port count should be 0");
    TEST_ASSERT(block->output_port_count == 1, "Output port count should be 1");

    // 블록 초기화
    ETResult result = dsp_block_initialize(block);
    TEST_ASSERT(result == ET_SUCCESS, "Block initialization failed");

    // 블록 처리 테스트
    result = dsp_block_process(block, frame_size);
    TEST_ASSERT(result == ET_SUCCESS, "Block processing failed");

    // 출력 데이터 확인
    DSPPort* output_port = dsp_block_get_output_port(block, 0);
    TEST_ASSERT(output_port != NULL, "Output port retrieval failed");
    TEST_ASSERT(output_port->buffer != NULL, "Output buffer not allocated");

    float* output_buffer = (float*)output_port->buffer;
    // 첫 번째 프레임의 데이터가 올바른지 확인
    bool data_matches = true;
    for (int i = 0; i < frame_size && i < audio_length; i++) {
        if (fabs(output_buffer[i] - test_audio[i]) > 1e-6) {
            data_matches = false;
            break;
        }
    }
    TEST_ASSERT(data_matches == true, "Output data mismatch");

    dsp_block_destroy(block);
    free(test_audio);
    TEST_PASS();
}

// =============================================================================
// DSP 다이어그램 빌더 테스트
// =============================================================================

/**
 * @brief DSP 다이어그램 빌더 기본 기능 테스트
 */
static bool test_dsp_diagram_builder_basic(void) {
    DSPDiagramBuilder* builder = dsp_diagram_builder_create("test_builder", 10, 20, g_test_mem_pool);
    TEST_ASSERT(builder != NULL, "Builder creation failed");
    TEST_ASSERT(builder->diagram != NULL, "Builder diagram not created");
    TEST_ASSERT(builder->is_building == false, "Builder should not be building initially");

    // 빌드 시작
    ETResult result = dsp_diagram_builder_begin(builder);
    TEST_ASSERT(result == ET_SUCCESS, "Builder begin failed");
    TEST_ASSERT(builder->is_building == true, "Builder should be in building state");

    // 오디오 입력 블록 추가
    int audio_length = 44100;
    float* test_audio = create_test_audio_data(audio_length, 44100);
    result = dsp_diagram_builder_add_audio_input(builder, "audio_input", test_audio,
                                                audio_length, 44100, 1024);
    TEST_ASSERT(result == ET_SUCCESS, "Audio input block addition failed");
    TEST_ASSERT(builder->audio_input_block_id != 0, "Audio input block ID not set");

    // 빌드 완료
    DSPBlockDiagram* diagram = dsp_diagram_builder_finish(builder);
    TEST_ASSERT(diagram != NULL, "Builder finish failed");
    TEST_ASSERT(builder->is_building == false, "Builder should not be building after finish");

    dsp_diagram_builder_destroy(builder);
    free(test_audio);
    TEST_PASS();
}

// =============================================================================
// DSP 블록 팩토리 테스트
// =============================================================================

/**
 * @brief DSP 블록 팩토리 기본 기능 테스트
 */
static bool test_dsp_block_factory_basic(void) {
    DSPBlockFactory* factory = dsp_block_factory_create(g_test_mem_pool);
    TEST_ASSERT(factory != NULL, "Factory creation failed");
    TEST_ASSERT(factory->blocks_created == 0, "Initial blocks created should be 0");
    TEST_ASSERT(factory->blocks_destroyed == 0, "Initial blocks destroyed should be 0");

    // 기본 설정 초기화
    ETResult result = dsp_block_factory_initialize_defaults(factory, 44100, 5.0, 2048);
    TEST_ASSERT(result == ET_SUCCESS, "Factory defaults initialization failed");

    // 오디오 입력 블록 설정 및 생성
    AudioInputBlockConfig audio_config;
    int audio_length = 44100;
    float* test_audio = create_test_audio_data(audio_length, 44100);

    dsp_block_factory_init_audio_input_config(&audio_config, "test_audio_input",
                                              test_audio, audio_length, 44100);

    DSPBlock* block = dsp_block_factory_create_audio_input(factory, &audio_config);
    TEST_ASSERT(block != NULL, "Factory audio input block creation failed");
    TEST_ASSERT(factory->blocks_created == 1, "Blocks created count should be 1");

    // 블록 해제
    dsp_block_factory_destroy_block(factory, block);
    TEST_ASSERT(factory->blocks_destroyed == 1, "Blocks destroyed count should be 1");

    dsp_block_factory_destroy(factory);
    free(test_audio);
    TEST_PASS();
}

/**
 * @brief DSP 블록 팩토리 배치 생성 테스트
 */
static bool test_dsp_block_factory_batch(void) {
    DSPBlockFactory* factory = dsp_block_factory_create(g_test_mem_pool);
    TEST_ASSERT(factory != NULL, "Factory creation failed");

    dsp_block_factory_initialize_defaults(factory, 44100, 5.0, 2048);

    // 배치 설정 준비
    DSPBlockConfig configs[3];
    DSPBlock* blocks[3];

    // 오디오 입력 블록 설정
    configs[0].block_type = DSP_BLOCK_TYPE_AUDIO_INPUT;
    int audio_length = 44100;
    float* test_audio = create_test_audio_data(audio_length, 44100);
    dsp_block_factory_init_audio_input_config(&configs[0].config.audio_input,
                                              "audio_input", test_audio, audio_length, 44100);

    // F0 추출 블록 설정
    configs[1].block_type = DSP_BLOCK_TYPE_F0_EXTRACTION;
    dsp_block_factory_init_f0_extraction_config(&configs[1].config.f0_extraction,
                                                "f0_extraction", true);

    // 파라미터 병합 블록 설정
    configs[2].block_type = DSP_BLOCK_TYPE_PARAMETER_MERGE;
    dsp_block_factory_init_parameter_merge_config(&configs[2].config.parameter_merge,
                                                  "parameter_merge", 100, 2048);

    // 배치 생성
    int created_count = dsp_block_factory_create_blocks_batch(factory, configs, 3, blocks, 3);
    TEST_ASSERT(created_count == 3, "Batch creation count mismatch");
    TEST_ASSERT(factory->blocks_created == 3, "Factory blocks created count mismatch");

    // 생성된 블록들 확인
    TEST_ASSERT(blocks[0]->type == DSP_BLOCK_TYPE_AUDIO_INPUT, "Block 0 type mismatch");
    TEST_ASSERT(blocks[1]->type == DSP_BLOCK_TYPE_F0_EXTRACTION, "Block 1 type mismatch");
    TEST_ASSERT(blocks[2]->type == DSP_BLOCK_TYPE_PARAMETER_MERGE, "Block 2 type mismatch");

    // 배치 해제
    dsp_block_factory_destroy_blocks_batch(factory, blocks, 3);
    TEST_ASSERT(factory->blocks_destroyed == 3, "Factory blocks destroyed count mismatch");

    dsp_block_factory_destroy(factory);
    free(test_audio);
    TEST_PASS();
}

// =============================================================================
// 통합 테스트
// =============================================================================

/**
 * @brief 간단한 DSP 파이프라인 통합 테스트
 */
static bool test_simple_dsp_pipeline(void) {
    // 빌더 생성
    DSPDiagramBuilder* builder = dsp_diagram_builder_create("pipeline_test", 10, 20, g_test_mem_pool);
    TEST_ASSERT(builder != NULL, "Builder creation failed");

    // 빌드 시작
    ETResult result = dsp_diagram_builder_begin(builder);
    TEST_ASSERT(result == ET_SUCCESS, "Builder begin failed");

    // 테스트 오디오 데이터 생성
    int audio_length = 44100;
    float* test_audio = create_test_audio_data(audio_length, 44100);

    // 오디오 입력 블록 추가
    result = dsp_diagram_builder_add_audio_input(builder, "audio_input", test_audio,
                                                audio_length, 44100, 1024);
    TEST_ASSERT(result == ET_SUCCESS, "Audio input addition failed");

    // 오디오 출력 블록 추가
    result = dsp_diagram_builder_add_audio_output(builder, "audio_output", audio_length,
                                                 44100, "test_output.wav");
    TEST_ASSERT(result == ET_SUCCESS, "Audio output addition failed");

    // 연결 생성
    result = dsp_diagram_builder_connect_by_id(builder, builder->audio_input_block_id, 0,
                                              builder->audio_output_block_id, 0);
    TEST_ASSERT(result == ET_SUCCESS, "Connection creation failed");

    // 다이어그램 완성
    DSPBlockDiagram* diagram = dsp_diagram_builder_finish(builder);
    TEST_ASSERT(diagram != NULL, "Diagram finish failed");
    TEST_ASSERT(diagram->is_built == true, "Diagram should be built");
    TEST_ASSERT(diagram->block_count == 2, "Block count should be 2");
    TEST_ASSERT(diagram->connection_count == 1, "Connection count should be 1");

    // 다이어그램 초기화
    result = dsp_block_diagram_initialize(diagram);
    TEST_ASSERT(result == ET_SUCCESS, "Diagram initialization failed");

    // 다이어그램 처리 (한 프레임)
    result = dsp_block_diagram_process(diagram, 1024);
    TEST_ASSERT(result == ET_SUCCESS, "Diagram processing failed");

    dsp_diagram_builder_destroy(builder);
    free(test_audio);
    TEST_PASS();
}

// =============================================================================
// 테스트 헬퍼 함수들
// =============================================================================

static bool setup_test_environment(void) {
    // 메모리 풀 생성
    g_test_mem_pool = et_memory_pool_create(1024 * 1024); // 1MB
    if (!g_test_mem_pool) {
        printf("Failed to create test memory pool\n");
        return false;
    }

    return true;
}

static void cleanup_test_environment(void) {
    if (g_test_mem_pool) {
        et_memory_pool_destroy(g_test_mem_pool);
        g_test_mem_pool = NULL;
    }
}

static void run_test(bool (*test_func)(void), const char* test_name) {
    printf("Running test: %s\n", test_name);

    if (test_func()) {
        g_tests_passed++;
    } else {
        g_tests_failed++;
    }
}

static float* create_test_audio_data(int length, int sample_rate) {
    float* audio = (float*)malloc(sizeof(float) * length);
    if (!audio) {
        return NULL;
    }

    // 440Hz 사인파 생성
    double frequency = 440.0;
    double phase_increment = 2.0 * M_PI * frequency / sample_rate;

    for (int i = 0; i < length; i++) {
        audio[i] = (float)(0.5 * sin(i * phase_increment));
    }

    return audio;
}

// =============================================================================
// 메인 테스트 함수
// =============================================================================

int main(void) {
    printf("=== DSP 블록 시스템 단위 테스트 시작 ===\n\n");

    // 테스트 환경 설정
    if (!setup_test_environment()) {
        printf("테스트 환경 설정 실패\n");
        return 1;
    }

    // DSP 블록 기본 기능 테스트
    run_test(test_dsp_block_create_destroy, "DSP 블록 생성/해제");
    run_test(test_dsp_block_port_configuration, "DSP 블록 포트 설정");
    run_test(test_dsp_block_initialization, "DSP 블록 초기화");

    // DSP 연결 테스트
    run_test(test_dsp_connection_create_validate, "DSP 연결 생성/검증");

    // DSP 블록 다이어그램 테스트
    run_test(test_dsp_block_diagram_management, "DSP 다이어그램 관리");
    run_test(test_dsp_block_diagram_connections, "DSP 다이어그램 연결");

    // WORLD DSP 블록 테스트
    run_test(test_audio_input_block, "오디오 입력 블록");

    // DSP 다이어그램 빌더 테스트
    run_test(test_dsp_diagram_builder_basic, "DSP 다이어그램 빌더 기본");

    // DSP 블록 팩토리 테스트
    run_test(test_dsp_block_factory_basic, "DSP 블록 팩토리 기본");
    run_test(test_dsp_block_factory_batch, "DSP 블록 팩토리 배치");

    // 통합 테스트
    run_test(test_simple_dsp_pipeline, "간단한 DSP 파이프라인");

    // 테스트 환경 정리
    cleanup_test_environment();

    // 결과 출력
    printf("\n=== 테스트 결과 ===\n");
    printf("통과: %d\n", g_tests_passed);
    printf("실패: %d\n", g_tests_failed);
    printf("총 테스트: %d\n", g_tests_passed + g_tests_failed);

    if (g_tests_failed == 0) {
        printf("모든 테스트가 통과했습니다!\n");
        return 0;
    } else {
        printf("%d개의 테스트가 실패했습니다.\n", g_tests_failed);
        return 1;
    }
}