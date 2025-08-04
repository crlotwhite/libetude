/**
 * @file test_world_synthesis.c
 * @brief WORLD 음성 합성 엔진 단위 테스트
 *
 * WORLD 합성 엔진의 기능과 성능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

// libetude 헤더
#include <libetude/api.h>
#include <libetude/types.h>
#include <libetude/error.h>
#include <libetude/memory.h>

// world4utau 헤더
#include "../../examples/world4utau/include/world_engine.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 테스트 상수
#define TEST_SAMPLE_RATE 44100
#define TEST_FRAME_PERIOD 5.0
#define TEST_F0_LENGTH 100
#define TEST_FFT_SIZE 2048
#define TEST_AUDIO_LENGTH 22050  // 0.5초
#define TEST_CHUNK_SIZE 1024
#define EPSILON 1e-6

// 테스트 결과 구조체
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    double total_time_ms;
} TestResults;

// 전역 테스트 결과
static TestResults g_test_results = {0, 0, 0, 0.0};

// ============================================================================
// 테스트 유틸리티 함수들
// ============================================================================

/**
 * @brief 테스트 시작
 */
static void test_start(const char* test_name) {
    printf("Testing %s... ", test_name);
    fflush(stdout);
    g_test_results.total_tests++;
}

/**
 * @brief 테스트 성공
 */
static void test_pass(void) {
    printf("PASS\n");
    g_test_results.passed_tests++;
}

/**
 * @brief 테스트 실패
 */
static void test_fail(const char* reason) {
    printf("FAIL: %s\n", reason);
    g_test_results.failed_tests++;
}

/**
 * @brief 부동소수점 비교
 */
static bool float_equals(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/**
 * @brief 테스트용 WORLD 파라미터 생성
 */
static WorldParameters* create_test_world_parameters(void) {
    WorldParameters* params = world_parameters_create(TEST_F0_LENGTH, TEST_FFT_SIZE, NULL);
    if (!params) {
        return NULL;
    }

    // 기본 정보 설정
    world_parameters_init(params, TEST_SAMPLE_RATE, TEST_AUDIO_LENGTH, TEST_FRAME_PERIOD);

    // 테스트용 F0 데이터 생성 (440Hz 사인파)
    for (int i = 0; i < TEST_F0_LENGTH; i++) {
        double time = i * TEST_FRAME_PERIOD / 1000.0;
        params->time_axis[i] = time;

        // 기본 주파수 440Hz + 약간의 변조
        params->f0[i] = 440.0 + 20.0 * sin(2.0 * M_PI * 2.0 * time);

        // 일부 프레임을 무성음으로 설정
        if (i % 20 < 2) {
            params->f0[i] = 0.0;  // 무성음
        }
    }

    // 테스트용 스펙트럼 데이터 생성
    int spectrum_length = TEST_FFT_SIZE / 2 + 1;
    for (int i = 0; i < TEST_F0_LENGTH; i++) {
        for (int j = 0; j < spectrum_length; j++) {
            double freq = (double)j * TEST_SAMPLE_RATE / TEST_FFT_SIZE;

            // 간단한 하모닉 스펙트럼 모델
            double magnitude = 0.0;
            if (params->f0[i] > 0.0) {
                // 유성음: 하모닉 구조
                double f0 = params->f0[i];
                for (int h = 1; h <= 5; h++) {
                    double harmonic_freq = f0 * h;
                    if (fabs(freq - harmonic_freq) < f0 * 0.1) {
                        magnitude += 1.0 / h;  // 하모닉 강도
                    }
                }
            } else {
                // 무성음: 평평한 스펙트럼
                magnitude = 0.1 * exp(-freq / 4000.0);
            }

            params->spectrogram[i][j] = magnitude;
        }
    }

    // 테스트용 비주기성 데이터 생성
    for (int i = 0; i < TEST_F0_LENGTH; i++) {
        for (int j = 0; j < spectrum_length; j++) {
            if (params->f0[i] > 0.0) {
                // 유성음: 낮은 비주기성
                params->aperiodicity[i][j] = 0.1 + 0.1 * (double)j / spectrum_length;
            } else {
                // 무성음: 높은 비주기성
                params->aperiodicity[i][j] = 0.9;
            }
        }
    }

    return params;
}

/**
 * @brief 테스트용 합성 설정 생성
 */
static WorldSynthesisConfig create_test_synthesis_config(void) {
    WorldSynthesisConfig config;
    world_get_default_synthesis_config(&config);

    config.sample_rate = TEST_SAMPLE_RATE;
    config.frame_period = TEST_FRAME_PERIOD;
    config.enable_postfilter = true;
    config.enable_simd_optimization = true;
    config.enable_gpu_acceleration = false;
    config.memory_pool_size = 2 * 1024 * 1024;  // 2MB

    return config;
}

// ============================================================================
// 기본 기능 테스트
// ============================================================================

/**
 * @brief 합성 엔진 생성/해제 테스트
 */
static void test_synthesis_engine_creation(void) {
    test_start("synthesis engine creation/destruction");

    WorldSynthesisConfig config = create_test_synthesis_config();

    // 엔진 생성
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    if (!engine) {
        test_fail("Failed to create synthesis engine");
        return;
    }

    // 초기화 상태 확인
    if (!engine->is_initialized) {
        test_fail("Engine not properly initialized");
        world_synthesis_destroy(engine);
        return;
    }

    // 설정 확인
    if (engine->config.sample_rate != TEST_SAMPLE_RATE) {
        test_fail("Sample rate not set correctly");
        world_synthesis_destroy(engine);
        return;
    }

    // 엔진 해제
    world_synthesis_destroy(engine);

    test_pass();
}

/**
 * @brief NULL 포인터 처리 테스트
 */
static void test_null_pointer_handling(void) {
    test_start("null pointer handling");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();
    float output_buffer[TEST_AUDIO_LENGTH];
    int output_length = TEST_AUDIO_LENGTH;

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // NULL 엔진 테스트
    ETResult result = world_synthesize_audio(NULL, params, output_buffer, &output_length);
    if (result != ET_ERROR_INVALID_ARGUMENT) {
        test_fail("Should return ET_ERROR_INVALID_ARGUMENT for NULL engine");
        goto cleanup;
    }

    // NULL 파라미터 테스트
    result = world_synthesize_audio(engine, NULL, output_buffer, &output_length);
    if (result != ET_ERROR_INVALID_ARGUMENT) {
        test_fail("Should return ET_ERROR_INVALID_ARGUMENT for NULL params");
        goto cleanup;
    }

    // NULL 출력 버퍼 테스트
    result = world_synthesize_audio(engine, params, NULL, &output_length);
    if (result != ET_ERROR_INVALID_ARGUMENT) {
        test_fail("Should return ET_ERROR_INVALID_ARGUMENT for NULL output");
        goto cleanup;
    }

    // NULL 길이 포인터 테스트
    result = world_synthesize_audio(engine, params, output_buffer, NULL);
    if (result != ET_ERROR_INVALID_ARGUMENT) {
        test_fail("Should return ET_ERROR_INVALID_ARGUMENT for NULL length");
        goto cleanup;
    }

    test_pass();

cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

/**
 * @brief 기본 음성 합성 테스트
 */
static void test_basic_audio_synthesis(void) {
    test_start("basic audio synthesis");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 출력 버퍼 할당
    float* output_buffer = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    if (!output_buffer) {
        test_fail("Failed to allocate output buffer");
        goto cleanup;
    }

    int output_length = TEST_AUDIO_LENGTH;

    // 음성 합성 수행
    clock_t start_time = clock();
    ETResult result = world_synthesize_audio(engine, params, output_buffer, &output_length);
    clock_t end_time = clock();

    double processing_time = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;

    if (result != ET_SUCCESS) {
        test_fail("Audio synthesis failed");
        goto cleanup_buffer;
    }

    // 출력 길이 확인
    if (output_length != TEST_AUDIO_LENGTH) {
        test_fail("Output length mismatch");
        goto cleanup_buffer;
    }

    // 출력 신호 유효성 검사
    bool has_non_zero = false;
    bool has_reasonable_amplitude = true;

    for (int i = 0; i < output_length; i++) {
        if (fabs(output_buffer[i]) > EPSILON) {
            has_non_zero = true;
        }

        if (fabs(output_buffer[i]) > 2.0) {  // 클리핑 체크
            has_reasonable_amplitude = false;
            break;
        }
    }

    if (!has_non_zero) {
        test_fail("Output is all zeros");
        goto cleanup_buffer;
    }

    if (!has_reasonable_amplitude) {
        test_fail("Output amplitude too high (clipping detected)");
        goto cleanup_buffer;
    }

    printf("(%.2f ms) ", processing_time);
    test_pass();

cleanup_buffer:
    free(output_buffer);
cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

/**
 * @brief 스트리밍 합성 테스트
 */
typedef struct {
    float* collected_audio;
    int collected_samples;
    int max_samples;
    bool callback_called;
} StreamingTestData;

static bool streaming_callback(const float* audio_data, int sample_count, void* user_data) {
    StreamingTestData* test_data = (StreamingTestData*)user_data;

    test_data->callback_called = true;

    // 오디오 데이터 수집
    if (test_data->collected_samples + sample_count <= test_data->max_samples) {
        memcpy(&test_data->collected_audio[test_data->collected_samples],
               audio_data, sample_count * sizeof(float));
        test_data->collected_samples += sample_count;
    }

    return true;  // 계속 처리
}

static void test_streaming_synthesis(void) {
    test_start("streaming synthesis");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 스트리밍 테스트 데이터 준비
    StreamingTestData test_data;
    test_data.max_samples = TEST_AUDIO_LENGTH;
    test_data.collected_audio = (float*)calloc(test_data.max_samples, sizeof(float));
    test_data.collected_samples = 0;
    test_data.callback_called = false;

    if (!test_data.collected_audio) {
        test_fail("Failed to allocate streaming buffer");
        goto cleanup;
    }

    // 스트리밍 합성 수행
    ETResult result = world_synthesize_streaming(engine, params, streaming_callback, &test_data);

    if (result != ET_SUCCESS) {
        test_fail("Streaming synthesis failed");
        goto cleanup_buffer;
    }

    // 콜백 호출 확인
    if (!test_data.callback_called) {
        test_fail("Streaming callback was not called");
        goto cleanup_buffer;
    }

    // 수집된 샘플 수 확인
    if (test_data.collected_samples == 0) {
        test_fail("No samples collected from streaming");
        goto cleanup_buffer;
    }

    // 출력 신호 유효성 검사
    bool has_non_zero = false;
    for (int i = 0; i < test_data.collected_samples; i++) {
        if (fabs(test_data.collected_audio[i]) > EPSILON) {
            has_non_zero = true;
            break;
        }
    }

    if (!has_non_zero) {
        test_fail("Streaming output is all zeros");
        goto cleanup_buffer;
    }

    test_pass();

cleanup_buffer:
    free(test_data.collected_audio);
cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

// ============================================================================
// 실시간 처리 테스트
// ============================================================================

/**
 * @brief 실시간 합성 초기화 테스트
 */
static void test_realtime_synthesis_init(void) {
    test_start("realtime synthesis initialization");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 실시간 합성 초기화
    ETResult result = world_synthesize_realtime_init(engine, params, TEST_CHUNK_SIZE);

    if (result != ET_SUCCESS) {
        test_fail("Realtime synthesis initialization failed");
        goto cleanup;
    }

    // 초기화 상태 확인
    if (!engine->realtime_mode) {
        test_fail("Realtime mode not activated");
        goto cleanup;
    }

    if (engine->chunk_size != TEST_CHUNK_SIZE) {
        test_fail("Chunk size not set correctly");
        goto cleanup;
    }

    if (!engine->realtime_output_buffer) {
        test_fail("Realtime output buffer not allocated");
        goto cleanup;
    }

    if (!engine->overlap_buffer) {
        test_fail("Overlap buffer not allocated");
        goto cleanup;
    }

    test_pass();

cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

/**
 * @brief 실시간 청크 처리 테스트
 */
static void test_realtime_chunk_processing(void) {
    test_start("realtime chunk processing");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 실시간 합성 초기화
    ETResult result = world_synthesize_realtime_init(engine, params, TEST_CHUNK_SIZE);
    if (result != ET_SUCCESS) {
        test_fail("Realtime initialization failed");
        goto cleanup;
    }

    // 청크 버퍼 할당
    float* chunk_buffer = (float*)malloc(TEST_CHUNK_SIZE * sizeof(float));
    if (!chunk_buffer) {
        test_fail("Failed to allocate chunk buffer");
        goto cleanup;
    }

    // 여러 청크 처리
    int total_chunks = (TEST_AUDIO_LENGTH + TEST_CHUNK_SIZE - 1) / TEST_CHUNK_SIZE;
    bool all_chunks_processed = true;
    double total_processing_time = 0.0;

    for (int chunk = 0; chunk < total_chunks; chunk++) {
        clock_t start_time = clock();
        result = world_synthesize_realtime_process(engine, chunk_buffer, TEST_CHUNK_SIZE);
        clock_t end_time = clock();

        double chunk_time = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
        total_processing_time += chunk_time;

        if (result != ET_SUCCESS) {
            all_chunks_processed = false;
            break;
        }

        // 청크 출력 유효성 검사
        bool chunk_has_data = false;
        for (int i = 0; i < TEST_CHUNK_SIZE; i++) {
            if (fabs(chunk_buffer[i]) > EPSILON) {
                chunk_has_data = true;
                break;
            }
        }

        // 첫 번째와 마지막 청크는 데이터가 적을 수 있음
        if (chunk > 0 && chunk < total_chunks - 1 && !chunk_has_data) {
            printf("Warning: Chunk %d has no data\n", chunk);
        }
    }

    if (!all_chunks_processed) {
        test_fail("Not all chunks processed successfully");
        goto cleanup_buffer;
    }

    // 평균 처리 시간 계산
    double avg_processing_time = total_processing_time / total_chunks;
    double target_time = (double)TEST_CHUNK_SIZE / TEST_SAMPLE_RATE * 1000.0;

    printf("(avg: %.2f ms, target: %.2f ms) ", avg_processing_time, target_time);

    if (avg_processing_time > target_time * 2.0) {
        printf("Warning: Processing time too high ");
    }

    test_pass();

cleanup_buffer:
    free(chunk_buffer);
cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

/**
 * @brief 지연 시간 최적화 테스트
 */
static void test_latency_optimization(void) {
    test_start("latency optimization");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 실시간 합성 초기화
    ETResult result = world_synthesize_realtime_init(engine, params, TEST_CHUNK_SIZE);
    if (result != ET_SUCCESS) {
        test_fail("Realtime initialization failed");
        goto cleanup;
    }

    // 다양한 최적화 레벨 테스트
    double latency_ms;
    for (int level = 0; level <= 3; level++) {
        result = world_optimize_latency(engine, &latency_ms, level);

        if (result != ET_SUCCESS) {
            test_fail("Latency optimization failed");
            goto cleanup;
        }

        if (engine->optimization_level != level) {
            test_fail("Optimization level not set correctly");
            goto cleanup;
        }
    }

    // 적응적 최적화 테스트
    double target_latency = 10.0;  // 10ms 목표
    result = world_adaptive_optimization(engine, target_latency);

    if (result != ET_SUCCESS) {
        test_fail("Adaptive optimization failed");
        goto cleanup;
    }

    test_pass();

cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

// ============================================================================
// 성능 테스트
// ============================================================================

/**
 * @brief 성능 벤치마크 테스트
 */
static void test_performance_benchmark(void) {
    test_start("performance benchmark");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 벤치마크 반복 횟수
    const int benchmark_iterations = 10;
    double total_time = 0.0;

    float* output_buffer = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    if (!output_buffer) {
        test_fail("Failed to allocate output buffer");
        goto cleanup;
    }

    // 벤치마크 실행
    for (int i = 0; i < benchmark_iterations; i++) {
        int output_length = TEST_AUDIO_LENGTH;

        clock_t start_time = clock();
        ETResult result = world_synthesize_audio(engine, params, output_buffer, &output_length);
        clock_t end_time = clock();

        if (result != ET_SUCCESS) {
            test_fail("Synthesis failed during benchmark");
            goto cleanup_buffer;
        }

        double iteration_time = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
        total_time += iteration_time;
    }

    double avg_time = total_time / benchmark_iterations;
    double audio_duration = (double)TEST_AUDIO_LENGTH / TEST_SAMPLE_RATE * 1000.0;
    double realtime_factor = audio_duration / avg_time;

    printf("(avg: %.2f ms, RT factor: %.2fx) ", avg_time, realtime_factor);

    // 실시간 처리 가능 여부 확인
    if (realtime_factor < 1.0) {
        printf("Warning: Not real-time capable ");
    }

    test_pass();

cleanup_buffer:
    free(output_buffer);
cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

/**
 * @brief 메모리 사용량 테스트
 */
static void test_memory_usage(void) {
    test_start("memory usage");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 실시간 모드 초기화
    ETResult result = world_synthesize_realtime_init(engine, params, TEST_CHUNK_SIZE);
    if (result != ET_SUCCESS) {
        test_fail("Realtime initialization failed");
        goto cleanup;
    }

    // 메모리 사용량 모니터링
    double cpu_usage, memory_usage, latency;
    result = world_monitor_realtime_performance(engine, &cpu_usage, &memory_usage, &latency);

    if (result != ET_SUCCESS) {
        test_fail("Performance monitoring failed");
        goto cleanup;
    }

    printf("(mem: %.2f MB) ", memory_usage);

    // 메모리 사용량 합리성 검사
    if (memory_usage > 100.0) {  // 100MB 이상이면 경고
        printf("Warning: High memory usage ");
    }

    if (memory_usage < 0.1) {  // 너무 적으면 측정 오류 가능성
        printf("Warning: Suspiciously low memory usage ");
    }

    test_pass();

cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

// ============================================================================
// 품질 테스트
// ============================================================================

/**
 * @brief 합성 품질 테스트
 */
static void test_synthesis_quality(void) {
    test_start("synthesis quality");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    float* output_buffer = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    if (!output_buffer) {
        test_fail("Failed to allocate output buffer");
        goto cleanup;
    }

    int output_length = TEST_AUDIO_LENGTH;
    ETResult result = world_synthesize_audio(engine, params, output_buffer, &output_length);

    if (result != ET_SUCCESS) {
        test_fail("Audio synthesis failed");
        goto cleanup_buffer;
    }

    // 품질 메트릭 계산
    double rms_energy = 0.0;
    double peak_amplitude = 0.0;
    double zero_crossing_rate = 0.0;
    int zero_crossings = 0;

    for (int i = 0; i < output_length; i++) {
        double sample = output_buffer[i];

        // RMS 에너지
        rms_energy += sample * sample;

        // 피크 진폭
        if (fabs(sample) > peak_amplitude) {
            peak_amplitude = fabs(sample);
        }

        // 제로 크로싱
        if (i > 0) {
            if ((output_buffer[i-1] >= 0.0 && sample < 0.0) ||
                (output_buffer[i-1] < 0.0 && sample >= 0.0)) {
                zero_crossings++;
            }
        }
    }

    rms_energy = sqrt(rms_energy / output_length);
    zero_crossing_rate = (double)zero_crossings / output_length * TEST_SAMPLE_RATE;

    printf("(RMS: %.4f, Peak: %.4f, ZCR: %.1f Hz) ", rms_energy, peak_amplitude, zero_crossing_rate);

    // 품질 기준 검사
    if (rms_energy < 0.001) {
        test_fail("RMS energy too low (signal too weak)");
        goto cleanup_buffer;
    }

    if (peak_amplitude > 0.95) {
        test_fail("Peak amplitude too high (clipping risk)");
        goto cleanup_buffer;
    }

    if (zero_crossing_rate < 10.0 || zero_crossing_rate > 10000.0) {
        printf("Warning: Unusual zero crossing rate ");
    }

    test_pass();

cleanup_buffer:
    free(output_buffer);
cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

/**
 * @brief 연속성 테스트 (프레임 간 불연속 검사)
 */
static void test_synthesis_continuity(void) {
    test_start("synthesis continuity");

    WorldSynthesisConfig config = create_test_synthesis_config();
    WorldSynthesisEngine* engine = world_synthesis_create(&config);
    WorldParameters* params = create_test_world_parameters();

    if (!engine || !params) {
        test_fail("Failed to create test objects");
        if (engine) world_synthesis_destroy(engine);
        if (params) world_parameters_destroy(params);
        return;
    }

    // 실시간 모드로 청크별 합성
    ETResult result = world_synthesize_realtime_init(engine, params, TEST_CHUNK_SIZE);
    if (result != ET_SUCCESS) {
        test_fail("Realtime initialization failed");
        goto cleanup;
    }

    // 전체 출력 버퍼
    int total_samples = TEST_AUDIO_LENGTH;
    float* full_output = (float*)calloc(total_samples, sizeof(float));
    if (!full_output) {
        test_fail("Failed to allocate full output buffer");
        goto cleanup;
    }

    // 청크별 처리
    float* chunk_buffer = (float*)malloc(TEST_CHUNK_SIZE * sizeof(float));
    if (!chunk_buffer) {
        test_fail("Failed to allocate chunk buffer");
        goto cleanup_full;
    }

    int processed_samples = 0;
    int chunk_count = 0;

    while (processed_samples < total_samples) {
        int current_chunk_size = TEST_CHUNK_SIZE;
        if (processed_samples + current_chunk_size > total_samples) {
            current_chunk_size = total_samples - processed_samples;
        }

        result = world_synthesize_realtime_process(engine, chunk_buffer, current_chunk_size);
        if (result != ET_SUCCESS) {
            test_fail("Chunk processing failed");
            goto cleanup_chunk;
        }

        // 청크를 전체 버퍼에 복사
        memcpy(&full_output[processed_samples], chunk_buffer, current_chunk_size * sizeof(float));
        processed_samples += current_chunk_size;
        chunk_count++;
    }

    // 연속성 검사 (청크 경계에서의 불연속 검출)
    int discontinuities = 0;
    double max_discontinuity = 0.0;

    for (int chunk = 1; chunk < chunk_count; chunk++) {
        int boundary_sample = chunk * TEST_CHUNK_SIZE;
        if (boundary_sample < total_samples - 1) {
            double diff = fabs(full_output[boundary_sample] - full_output[boundary_sample - 1]);

            if (diff > 0.1) {  // 임계값
                discontinuities++;
                if (diff > max_discontinuity) {
                    max_discontinuity = diff;
                }
            }
        }
    }

    printf("(discontinuities: %d, max: %.4f) ", discontinuities, max_discontinuity);

    if (discontinuities > chunk_count / 10) {  // 10% 이상이면 문제
        test_fail("Too many discontinuities detected");
        goto cleanup_chunk;
    }

    if (max_discontinuity > 0.5) {
        test_fail("Discontinuity too large");
        goto cleanup_chunk;
    }

    test_pass();

cleanup_chunk:
    free(chunk_buffer);
cleanup_full:
    free(full_output);
cleanup:
    world_synthesis_destroy(engine);
    world_parameters_destroy(params);
}

// ============================================================================
// 메인 테스트 실행 함수
// ============================================================================

/**
 * @brief 모든 테스트 실행
 */
int main(void) {
    printf("=== WORLD Synthesis Engine Unit Tests ===\n\n");

    clock_t start_time = clock();

    // 기본 기능 테스트
    printf("Basic Functionality Tests:\n");
    test_synthesis_engine_creation();
    test_null_pointer_handling();
    test_basic_audio_synthesis();
    test_streaming_synthesis();

    // 실시간 처리 테스트
    printf("\nReal-time Processing Tests:\n");
    test_realtime_synthesis_init();
    test_realtime_chunk_processing();
    test_latency_optimization();

    // 성능 테스트
    printf("\nPerformance Tests:\n");
    test_performance_benchmark();
    test_memory_usage();

    // 품질 테스트
    printf("\nQuality Tests:\n");
    test_synthesis_quality();
    test_synthesis_continuity();

    clock_t end_time = clock();
    double total_time = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;

    // 테스트 결과 출력
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", g_test_results.total_tests);
    printf("Passed: %d\n", g_test_results.passed_tests);
    printf("Failed: %d\n", g_test_results.failed_tests);
    printf("Success rate: %.1f%%\n",
           (double)g_test_results.passed_tests / g_test_results.total_tests * 100.0);
    printf("Total time: %.2f ms\n", total_time);

    // 종료 코드 반환
    return (g_test_results.failed_tests == 0) ? 0 : 1;
}