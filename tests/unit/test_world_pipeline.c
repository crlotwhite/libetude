/**
 * @file test_world_pipeline.c
 * @brief WORLD 파이프라인 통합 테스트
 *
 * 전체 파이프라인 동작 및 성능 테스트를 수행합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>

// libetude 헤더
#include <libetude/api.h>
#include <libetude/types.h>
#include <libetude/memory.h>

// WORLD 파이프라인 헤더
#include "../../examples/world4utau/include/world_pipeline.h"
#include "../../examples/world4utau/include/world_pipeline_config.h"
#include "../../examples/world4utau/include/world_streaming.h"
#include "../../examples/world4utau/include/world_performance_monitor.h"
#include "../../examples/world4utau/include/utau_interface.h"

// =============================================================================
// 테스트 헬퍼 함수들
// =============================================================================

/**
 * @brief 테스트용 UTAU 파라미터 생성
 */
static UTAUParameters create_test_utau_parameters(void) {
    UTAUParameters params = {0};

    strcpy(params.input_wav_path, "test_input.wav");
    strcpy(params.output_wav_path, "test_output.wav");
    params.target_pitch = 440.0f;
    params.velocity = 100.0f;
    params.volume = 0.8f;
    params.modulation = 0.1f;
    params.consonant_velocity = 100.0f;
    params.pre_utterance = 50.0f;
    params.overlap = 10.0f;
    params.sample_rate = 44100;

    // 간단한 피치 벤드 데이터
    params.pitch_bend_length = 10;
    params.pitch_bend = (float*)malloc(params.pitch_bend_length * sizeof(float));
    for (int i = 0; i < params.pitch_bend_length; i++) {
        params.pitch_bend[i] = 0.0f; // 피치 변화 없음
    }

    return params;
}

/**
 * @brief 테스트용 오디오 데이터 생성
 */
static float* create_test_audio_data(int sample_count, int sample_rate) {
    float* audio_data = (float*)malloc(sample_count * sizeof(float));
    if (!audio_data) return NULL;

    // 440Hz 사인파 생성
    double frequency = 440.0;
    for (int i = 0; i < sample_count; i++) {
        double t = (double)i / sample_rate;
        audio_data[i] = 0.5f * sin(2.0 * M_PI * frequency * t);
    }

    return audio_data;
}

/**
 * @brief 테스트 결과 검증
 */
static bool validate_audio_output(const float* audio_data, int sample_count) {
    if (!audio_data || sample_count <= 0) return false;

    // 기본적인 유효성 검사
    for (int i = 0; i < sample_count; i++) {
        if (isnan(audio_data[i]) || isinf(audio_data[i])) {
            return false;
        }
        if (fabs(audio_data[i]) > 2.0f) { // 합리적인 범위 체크
            return false;
        }
    }

    return true;
}

/**
 * @brief 성능 임계값 검증
 */
static bool validate_performance_thresholds(const WorldPipelinePerformance* perf) {
    if (!perf) return false;

    // 실시간 성능 체크 (최소 0.1x 이상)
    if (perf->realtime_performance < 0.1) return false;

    // 메모리 사용량 체크 (최대 1GB)
    if (perf->peak_total_memory > 1024 * 1024 * 1024) return false;

    // 처리 시간 체크 (최대 10초)
    if (perf->total_processing_time > 10.0) return false;

    return true;
}

// =============================================================================
// 기본 파이프라인 테스트
// =============================================================================

/**
 * @brief 파이프라인 생성 및 해제 테스트
 */
static void test_pipeline_creation_destruction(void) {
    printf("Testing pipeline creation and destruction...\n");

    // 기본 설정으로 파이프라인 생성
    WorldPipelineConfig config = world_pipeline_config_create_default();
    WorldPipeline* pipeline = world_pipeline_create(&config);

    assert(pipeline != NULL);
    assert(world_pipeline_get_state(pipeline) == WORLD_PIPELINE_STATE_UNINITIALIZED);

    // 파이프라인 해제
    world_pipeline_destroy(pipeline);

    printf("✓ Pipeline creation and destruction test passed\n");
}

/**
 * @brief 파이프라인 초기화 테스트
 */
static void test_pipeline_initialization(void) {
    printf("Testing pipeline initialization...\n");

    WorldPipelineConfig config = world_pipeline_config_create_default();
    WorldPipeline* pipeline = world_pipeline_create(&config);
    assert(pipeline != NULL);

    // 초기화
    ETResult result = world_pipeline_initialize(pipeline);
    assert(result == ET_SUCCESS);
    assert(world_pipeline_get_state(pipeline) == WORLD_PIPELINE_STATE_READY);

    // 중복 초기화 (성공해야 함)
    result = world_pipeline_initialize(pipeline);
    assert(result == ET_SUCCESS);

    world_pipeline_destroy(pipeline);

    printf("✓ Pipeline initialization test passed\n");
}

/**
 * @brief 파이프라인 설정 테스트
 */
static void test_pipeline_configuration(void) {
    printf("Testing pipeline configuration...\n");

    // 다양한 프리셋 테스트
    WorldConfigPreset presets[] = {
        WORLD_CONFIG_PRESET_DEFAULT,
        WORLD_CONFIG_PRESET_HIGH_QUALITY,
        WORLD_CONFIG_PRESET_FAST,
        WORLD_CONFIG_PRESET_LOW_LATENCY,
        WORLD_CONFIG_PRESET_LOW_MEMORY
    };

    for (int i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        WorldPipelineConfig config = world_pipeline_config_create_preset(presets[i]);
        assert(world_pipeline_config_validate(&config));

        WorldPipeline* pipeline = world_pipeline_create(&config);
        assert(pipeline != NULL);

        ETResult result = world_pipeline_initialize(pipeline);
        assert(result == ET_SUCCESS);

        world_pipeline_destroy(pipeline);
    }

    printf("✓ Pipeline configuration test passed\n");
}

// =============================================================================
// 파이프라인 처리 테스트
// =============================================================================

/**
 * @brief 기본 파이프라인 처리 테스트
 */
static void test_pipeline_basic_processing(void) {
    printf("Testing basic pipeline processing...\n");

    WorldPipelineConfig config = world_pipeline_config_create_default();
    WorldPipeline* pipeline = world_pipeline_create(&config);
    assert(pipeline != NULL);

    ETResult result = world_pipeline_initialize(pipeline);
    assert(result == ET_SUCCESS);

    // 테스트 파라미터 생성
    UTAUParameters utau_params = create_test_utau_parameters();

    // 출력 버퍼 준비
    int output_length = config.audio.buffer_size;
    float* output_audio = (float*)malloc(output_length * sizeof(float));
    assert(output_audio != NULL);

    // 파이프라인 처리
    result = world_pipeline_process(pipeline, &utau_params, output_audio, &output_length);
    assert(result == ET_SUCCESS);
    assert(output_length > 0);

    // 출력 검증
    assert(validate_audio_output(output_audio, output_length));

    // 정리
    free(output_audio);
    free(utau_params.pitch_bend);
    world_pipeline_destroy(pipeline);

    printf("✓ Basic pipeline processing test passed\n");
}

/**
 * @brief 파이프라인 비동기 처리 테스트
 */
static bool async_completion_called = false;
static void async_completion_callback(void* user_data, ETResult result, const char* message) {
    async_completion_called = true;
    assert(result == ET_SUCCESS);
    printf("Async processing completed: %s\n", message ? message : "Success");
}

static void test_pipeline_async_processing(void) {
    printf("Testing async pipeline processing...\n");

    WorldPipelineConfig config = world_pipeline_config_create_default();
    WorldPipeline* pipeline = world_pipeline_create(&config);
    assert(pipeline != NULL);

    ETResult result = world_pipeline_initialize(pipeline);
    assert(result == ET_SUCCESS);

    // 테스트 파라미터 생성
    UTAUParameters utau_params = create_test_utau_parameters();

    // 비동기 처리 시작
    async_completion_called = false;
    result = world_pipeline_process_async(pipeline, &utau_params,
                                         async_completion_callback, NULL);
    assert(result == ET_SUCCESS);

    // 완료 대기 (간단한 폴링)
    int timeout = 100; // 10초 타임아웃
    while (!async_completion_called && timeout > 0) {
        usleep(100000); // 100ms 대기
        timeout--;
    }

    assert(async_completion_called);

    // 정리
    free(utau_params.pitch_bend);
    world_pipeline_destroy(pipeline);

    printf("✓ Async pipeline processing test passed\n");
}

// =============================================================================
// 스트리밍 테스트
// =============================================================================

static int streaming_chunk_count = 0;
static void streaming_audio_callback(const float* audio_data, int frame_count, void* user_data) {
    streaming_chunk_count++;
    assert(audio_data != NULL);
    assert(frame_count > 0);
    assert(validate_audio_output(audio_data, frame_count));
    printf("Received streaming chunk %d with %d frames\n", streaming_chunk_count, frame_count);
}

/**
 * @brief 파이프라인 스트리밍 처리 테스트
 */
static void test_pipeline_streaming_processing(void) {
    printf("Testing streaming pipeline processing...\n");

    WorldPipelineConfig config = world_pipeline_config_create_preset(WORLD_CONFIG_PRESET_REALTIME);
    WorldPipeline* pipeline = world_pipeline_create(&config);
    assert(pipeline != NULL);

    ETResult result = world_pipeline_initialize(pipeline);
    assert(result == ET_SUCCESS);

    // 테스트 파라미터 생성
    UTAUParameters utau_params = create_test_utau_parameters();

    // 스트리밍 처리 시작
    streaming_chunk_count = 0;
    result = world_pipeline_process_streaming(pipeline, &utau_params,
                                             streaming_audio_callback, NULL);
    assert(result == ET_SUCCESS);

    // 스트리밍 활성 상태 확인
    assert(world_pipeline_is_running(pipeline));

    // 잠시 대기하여 스트리밍 데이터 수신
    sleep(1);

    // 스트리밍 중지
    result = world_pipeline_stop(pipeline);
    assert(result == ET_SUCCESS);

    // 청크가 수신되었는지 확인
    assert(streaming_chunk_count > 0);

    // 정리
    free(utau_params.pitch_bend);
    world_pipeline_destroy(pipeline);

    printf("✓ Streaming pipeline processing test passed (received %d chunks)\n", streaming_chunk_count);
}

// =============================================================================
// 성능 모니터링 테스트
// =============================================================================

/**
 * @brief 성능 모니터링 테스트
 */
static void test_pipeline_performance_monitoring(void) {
    printf("Testing pipeline performance monitoring...\n");

    // 성능 모니터링이 활성화된 설정
    WorldPipelineConfig config = world_pipeline_config_create_default();
    config.performance.enable_profiling = true;
    config.performance.enable_timing_analysis = true;
    config.performance.enable_memory_profiling = true;

    WorldPipeline* pipeline = world_pipeline_create(&config);
    assert(pipeline != NULL);

    ETResult result = world_pipeline_initialize(pipeline);
    assert(result == ET_SUCCESS);

    // 테스트 파라미터 생성
    UTAUParameters utau_params = create_test_utau_parameters();

    // 출력 버퍼 준비
    int output_length = config.audio.buffer_size;
    float* output_audio = (float*)malloc(output_length * sizeof(float));
    assert(output_audio != NULL);

    // 파이프라인 처리 (성능 측정 포함)
    result = world_pipeline_process(pipeline, &utau_params, output_audio, &output_length);
    assert(result == ET_SUCCESS);

    // 성능 통계 조회
    const WorldPipelinePerformance* perf = world_pipeline_get_performance_stats(pipeline);
    assert(perf != NULL);

    // 성능 데이터 검증
    assert(perf->total_processing_time > 0.0);
    assert(perf->total_processed_samples > 0);
    assert(validate_performance_thresholds(perf));

    // 단계별 성능 확인
    for (int i = 0; i < WORLD_PERF_STAGE_COUNT; i++) {
        const WorldStagePerformance* stage_perf = &perf->stages[i];
        if (stage_perf->execution_count > 0) {
            assert(stage_perf->total_execution_time >= 0.0);
            assert(stage_perf->last_execution_time >= 0.0);
            printf("Stage %s: %.6f seconds (%llu executions)\n",
                   stage_perf->stage_name,
                   stage_perf->total_execution_time,
                   stage_perf->execution_count);
        }
    }

    // 실시간 메트릭 조회
    double realtime_factor, current_latency, throughput;
    result = world_perf_monitor_get_realtime_metrics(
        pipeline->perf_monitor, &realtime_factor, &current_latency, &throughput);
    assert(result == ET_SUCCESS);

    printf("Performance metrics - Realtime: %.2fx, Latency: %.1fms, Throughput: %.0f sps\n",
           realtime_factor, current_latency, throughput);

    // 정리
    free(output_audio);
    free(utau_params.pitch_bend);
    world_pipeline_destroy(pipeline);

    printf("✓ Pipeline performance monitoring test passed\n");
}

// =============================================================================
// 오류 처리 테스트
// =============================================================================

/**
 * @brief 파이프라인 오류 처리 테스트
 */
static void test_pipeline_error_handling(void) {
    printf("Testing pipeline error handling...\n");

    WorldPipelineConfig config = world_pipeline_config_create_default();
    WorldPipeline* pipeline = world_pipeline_create(&config);
    assert(pipeline != NULL);

    // 초기화되지 않은 상태에서 처리 시도
    UTAUParameters utau_params = create_test_utau_parameters();
    int output_length = 1024;
    float* output_audio = (float*)malloc(output_length * sizeof(float));

    // 초기화 없이 처리하면 자동으로 초기화되어야 함
    ETResult result = world_pipeline_process(pipeline, &utau_params, output_audio, &output_length);
    assert(result == ET_SUCCESS);

    // NULL 파라미터 테스트
    result = world_pipeline_process(NULL, &utau_params, output_audio, &output_length);
    assert(result == ET_ERROR_INVALID_PARAMETER);

    result = world_pipeline_process(pipeline, NULL, output_audio, &output_length);
    assert(result == ET_ERROR_INVALID_PARAMETER);

    result = world_pipeline_process(pipeline, &utau_params, NULL, &output_length);
    assert(result == ET_ERROR_INVALID_PARAMETER);

    // 오류 상태 확인
    ETResult last_error = world_pipeline_get_last_error(pipeline);
    const char* error_message = world_pipeline_get_error_message(pipeline);

    printf("Last error: %d, Message: %s\n", last_error, error_message ? error_message : "None");

    // 오류 초기화
    result = world_pipeline_clear_error(pipeline);
    assert(result == ET_SUCCESS);
    assert(world_pipeline_get_last_error(pipeline) == ET_SUCCESS);

    // 정리
    free(output_audio);
    free(utau_params.pitch_bend);
    world_pipeline_destroy(pipeline);

    printf("✓ Pipeline error handling test passed\n");
}

// =============================================================================
// 메모리 관리 테스트
// =============================================================================

/**
 * @brief 파이프라인 메모리 관리 테스트
 */
static void test_pipeline_memory_management(void) {
    printf("Testing pipeline memory management...\n");

    // 저메모리 설정으로 테스트
    WorldPipelineConfig config = world_pipeline_config_create_preset(WORLD_CONFIG_PRESET_LOW_MEMORY);
    WorldPipeline* pipeline = world_pipeline_create(&config);
    assert(pipeline != NULL);

    ETResult result = world_pipeline_initialize(pipeline);
    assert(result == ET_SUCCESS);

    // 초기 메모리 사용량 확인
    size_t initial_memory = world_pipeline_get_memory_usage(pipeline);
    printf("Initial memory usage: %.2f MB\n", initial_memory / (1024.0 * 1024.0));

    // 여러 번 처리하여 메모리 누수 확인
    UTAUParameters utau_params = create_test_utau_parameters();
    int output_length = config.audio.buffer_size;
    float* output_audio = (float*)malloc(output_length * sizeof(float));

    for (int i = 0; i < 10; i++) {
        result = world_pipeline_process(pipeline, &utau_params, output_audio, &output_length);
        assert(result == ET_SUCCESS);

        size_t current_memory = world_pipeline_get_memory_usage(pipeline);
        printf("Iteration %d memory usage: %.2f MB\n", i + 1, current_memory / (1024.0 * 1024.0));

        // 메모리 사용량이 급격히 증가하지 않는지 확인
        assert(current_memory < initial_memory * 10); // 10배 이상 증가하면 문제
    }

    // 최종 메모리 사용량 확인
    size_t final_memory = world_pipeline_get_memory_usage(pipeline);
    printf("Final memory usage: %.2f MB\n", final_memory / (1024.0 * 1024.0));

    // 정리
    free(output_audio);
    free(utau_params.pitch_bend);
    world_pipeline_destroy(pipeline);

    printf("✓ Pipeline memory management test passed\n");
}

// =============================================================================
// 통합 성능 테스트
// =============================================================================

/**
 * @brief 파이프라인 성능 벤치마크 테스트
 */
static void test_pipeline_performance_benchmark(void) {
    printf("Testing pipeline performance benchmark...\n");

    // 다양한 설정으로 성능 테스트
    WorldConfigPreset presets[] = {
        WORLD_CONFIG_PRESET_FAST,
        WORLD_CONFIG_PRESET_HIGH_QUALITY,
        WORLD_CONFIG_PRESET_LOW_LATENCY
    };

    const char* preset_names[] = {
        "Fast",
        "High Quality",
        "Low Latency"
    };

    for (int p = 0; p < 3; p++) {
        printf("\nTesting %s preset:\n", preset_names[p]);

        WorldPipelineConfig config = world_pipeline_config_create_preset(presets[p]);
        config.performance.enable_profiling = true;
        config.performance.enable_timing_analysis = true;

        WorldPipeline* pipeline = world_pipeline_create(&config);
        assert(pipeline != NULL);

        ETResult result = world_pipeline_initialize(pipeline);
        assert(result == ET_SUCCESS);

        // 벤치마크 실행
        UTAUParameters utau_params = create_test_utau_parameters();
        int output_length = config.audio.buffer_size;
        float* output_audio = (float*)malloc(output_length * sizeof(float));

        double total_time = 0.0;
        int iterations = 5;

        for (int i = 0; i < iterations; i++) {
            double start_time = (double)clock() / CLOCKS_PER_SEC;

            result = world_pipeline_process(pipeline, &utau_params, output_audio, &output_length);
            assert(result == ET_SUCCESS);

            double end_time = (double)clock() / CLOCKS_PER_SEC;
            double iteration_time = end_time - start_time;
            total_time += iteration_time;

            printf("  Iteration %d: %.6f seconds\n", i + 1, iteration_time);
        }

        double average_time = total_time / iterations;
        printf("  Average time: %.6f seconds\n", average_time);

        // 성능 통계 출력
        const WorldPipelinePerformance* perf = world_pipeline_get_performance_stats(pipeline);
        if (perf) {
            printf("  Realtime factor: %.2fx\n", perf->realtime_performance);
            printf("  Throughput: %.0f samples/sec\n", perf->overall_throughput);
            printf("  Peak memory: %.2f MB\n", perf->peak_total_memory / (1024.0 * 1024.0));
        }

        // 정리
        free(output_audio);
        free(utau_params.pitch_bend);
        world_pipeline_destroy(pipeline);
    }

    printf("✓ Pipeline performance benchmark test passed\n");
}

// =============================================================================
// 메인 테스트 함수
// =============================================================================

/**
 * @brief 모든 파이프라인 테스트 실행
 */
int main(void) {
    printf("Starting WORLD Pipeline Integration Tests\n");
    printf("=========================================\n\n");

    // libetude 초기화
    ETResult result = et_initialize();
    if (result != ET_SUCCESS) {
        printf("Failed to initialize libetude: %d\n", result);
        return 1;
    }

    try {
        // 기본 파이프라인 테스트
        test_pipeline_creation_destruction();
        test_pipeline_initialization();
        test_pipeline_configuration();

        // 파이프라인 처리 테스트
        test_pipeline_basic_processing();
        test_pipeline_async_processing();
        test_pipeline_streaming_processing();

        // 고급 기능 테스트
        test_pipeline_performance_monitoring();
        test_pipeline_error_handling();
        test_pipeline_memory_management();

        // 성능 테스트
        test_pipeline_performance_benchmark();

        printf("\n=========================================\n");
        printf("All WORLD Pipeline Integration Tests Passed! ✓\n");

    } catch (...) {
        printf("\n=========================================\n");
        printf("Test failed with exception!\n");
        et_cleanup();
        return 1;
    }

    // libetude 정리
    et_cleanup();

    return 0;
}