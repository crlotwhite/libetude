/**
 * @file test_world4utau_integration.c
 * @brief world4utau 통합 테스트 스위트
 *
 * 실제 UTAU 사용 시나리오를 테스트하여 전체 시스템의 동작을 검증합니다.
 * 요구사항 6.1, 6.4를 만족하는 통합 테스트를 구현합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>

// libetude 헤더
#include "libetude/api.h"
#include "libetude/audio_io.h"
#include "libetude/memory.h"
#include "libetude/benchmark.h"
#include "libetude/performance_analyzer.h"

// world4utau 헤더
#include "../../examples/world4utau/include/utau_interface.h"
#include "../../examples/world4utau/include/world_engine.h"
#include "../../examples/world4utau/include/audio_file_io.h"
#include "../../examples/world4utau/include/world_error.h"

// 테스트 설정
#define TEST_SAMPLE_RATE 44100
#define TEST_AUDIO_LENGTH 22050  // 0.5초
#define TEST_TOLERANCE 0.01f
#define MAX_PROCESSING_TIME_MS 100  // 요구사항 6.1: 100ms 이내 처리

// 테스트 결과 구조체
typedef struct {
    bool passed;
    double processing_time_ms;
    size_t memory_usage_bytes;
    char error_message[256];
} TestResult;

// 전역 변수
static ETBenchmarkContext* g_benchmark_ctx = NULL;
static ETPerformanceAnalyzer* g_perf_analyzer = NULL;

/**
 * @brief 테스트 초기화
 */
static bool initialize_test_environment(void) {
    // libetude 초기화
    ETResult result = et_initialize();
    if (result != ET_SUCCESS) {
        printf("libetude 초기화 실패: %d\n", result);
        return false;
    }

    // 벤치마크 컨텍스트 생성
    g_benchmark_ctx = et_benchmark_create();
    if (!g_benchmark_ctx) {
        printf("벤치마크 컨텍스트 생성 실패\n");
        return false;
    }

    // 성능 분석기 생성
    g_perf_analyzer = et_performance_analyzer_create();
    if (!g_perf_analyzer) {
        printf("성능 분석기 생성 실패\n");
        return false;
    }

    return true;
}

/**
 * @brief 테스트 정리
 */
static void cleanup_test_environment(void) {
    if (g_perf_analyzer) {
        et_performance_analyzer_destroy(g_perf_analyzer);
        g_perf_analyzer = NULL;
    }

    if (g_benchmark_ctx) {
        et_benchmark_destroy(g_benchmark_ctx);
        g_benchmark_ctx = NULL;
    }

    et_cleanup();
}

/**
 * @brief 테스트용 사인파 오디오 생성
 */
static float* generate_test_audio(int sample_rate, double duration, double frequency) {
    int length = (int)(sample_rate * duration);
    float* audio = (float*)malloc(length * sizeof(float));

    if (!audio) return NULL;

    for (int i = 0; i < length; i++) {
        double t = (double)i / sample_rate;
        audio[i] = 0.5f * sinf(2.0f * M_PI * frequency * t);
    }

    return audio;
}

/**
 * @brief 기본 UTAU 파라미터 파싱 테스트
 */
static TestResult test_utau_parameter_parsing(void) {
    TestResult result = {false, 0.0, 0, ""};

    printf("  - UTAU 파라미터 파싱 테스트...\n");

    clock_t start = clock();

    // 테스트 명령줄 파라미터 생성
    char* test_argv[] = {
        "world4utau",
        "input.wav",
        "output.wav",
        "440.0",      // target_pitch
        "100",        // velocity
        "-v", "0.8",  // volume
        "-m", "0.2"   // modulation
    };
    int test_argc = sizeof(test_argv) / sizeof(test_argv[0]);

    UTAUParameters params;
    WorldErrorCode error = parse_utau_parameters(test_argc, test_argv, &params);

    clock_t end = clock();
    result.processing_time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "파라미터 파싱 실패: %s", world_get_error_string(error));
        return result;
    }

    // 파싱 결과 검증
    if (fabs(params.target_pitch - 440.0f) > TEST_TOLERANCE ||
        fabs(params.velocity - 100.0f) > TEST_TOLERANCE ||
        fabs(params.volume - 0.8f) > TEST_TOLERANCE ||
        fabs(params.modulation - 0.2f) > TEST_TOLERANCE) {
        strcpy(result.error_message, "파싱된 파라미터 값이 예상과 다름");
        return result;
    }

    result.passed = true;
    return result;
}

/**
 * @brief 전체 WORLD 분석 파이프라인 테스트
 */
static TestResult test_world_analysis_pipeline(void) {
    TestResult result = {false, 0.0, 0, ""};

    printf("  - WORLD 분석 파이프라인 테스트...\n");

    // 테스트 오디오 생성 (440Hz 사인파, 0.5초)
    float* test_audio = generate_test_audio(TEST_SAMPLE_RATE, 0.5, 440.0);
    if (!test_audio) {
        strcpy(result.error_message, "테스트 오디오 생성 실패");
        return result;
    }

    clock_t start = clock();

    // WORLD 분석 엔진 생성
    WorldAnalysisConfig config = {0};
    config.sample_rate = TEST_SAMPLE_RATE;
    config.frame_period = 5.0;  // 5ms
    config.f0_floor = 80.0;
    config.f0_ceil = 800.0;

    WorldAnalysisEngine* engine = world_analysis_create(&config);
    if (!engine) {
        free(test_audio);
        strcpy(result.error_message, "WORLD 분석 엔진 생성 실패");
        return result;
    }

    // 음성 분석 수행
    WorldParameters world_params = {0};
    WorldErrorCode error = world_analyze_audio(engine, test_audio, TEST_AUDIO_LENGTH, &world_params);

    clock_t end = clock();
    result.processing_time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "WORLD 분석 실패: %s", world_get_error_string(error));
        world_analysis_destroy(engine);
        free(test_audio);
        return result;
    }

    // 분석 결과 검증
    if (!world_params.f0 || !world_params.spectrogram || !world_params.aperiodicity) {
        strcpy(result.error_message, "분석 결과가 NULL");
        world_analysis_destroy(engine);
        free(test_audio);
        return result;
    }

    // F0 값이 예상 범위 내에 있는지 확인 (440Hz ± 50Hz)
    bool f0_valid = false;
    for (int i = 0; i < world_params.f0_length; i++) {
        if (world_params.f0[i] > 0 &&
            world_params.f0[i] >= 390.0 && world_params.f0[i] <= 490.0) {
            f0_valid = true;
            break;
        }
    }

    if (!f0_valid) {
        strcpy(result.error_message, "F0 추출 결과가 예상 범위를 벗어남");
        world_analysis_destroy(engine);
        free(test_audio);
        return result;
    }

    // 메모리 사용량 추정
    result.memory_usage_bytes = world_params.f0_length * sizeof(double) +
                               world_params.f0_length * (world_params.fft_size / 2 + 1) * sizeof(double) * 2;

    world_analysis_destroy(engine);
    free(test_audio);

    result.passed = true;
    return result;
}

/**
 * @brief 전체 WORLD 합성 파이프라인 테스트
 */
static TestResult test_world_synthesis_pipeline(void) {
    TestResult result = {false, 0.0, 0, ""};

    printf("  - WORLD 합성 파이프라인 테스트...\n");

    // 먼저 분석을 수행하여 WORLD 파라미터 획득
    float* test_audio = generate_test_audio(TEST_SAMPLE_RATE, 0.5, 440.0);
    if (!test_audio) {
        strcpy(result.error_message, "테스트 오디오 생성 실패");
        return result;
    }

    WorldAnalysisConfig analysis_config = {0};
    analysis_config.sample_rate = TEST_SAMPLE_RATE;
    analysis_config.frame_period = 5.0;
    analysis_config.f0_floor = 80.0;
    analysis_config.f0_ceil = 800.0;

    WorldAnalysisEngine* analysis_engine = world_analysis_create(&analysis_config);
    if (!analysis_engine) {
        free(test_audio);
        strcpy(result.error_message, "WORLD 분석 엔진 생성 실패");
        return result;
    }

    WorldParameters world_params = {0};
    WorldErrorCode error = world_analyze_audio(analysis_engine, test_audio, TEST_AUDIO_LENGTH, &world_params);
    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "분석 단계 실패: %s", world_get_error_string(error));
        world_analysis_destroy(analysis_engine);
        free(test_audio);
        return result;
    }

    clock_t start = clock();

    // WORLD 합성 엔진 생성
    WorldSynthesisConfig synthesis_config = {0};
    synthesis_config.sample_rate = TEST_SAMPLE_RATE;
    synthesis_config.frame_period = 5.0;

    WorldSynthesisEngine* synthesis_engine = world_synthesis_create(&synthesis_config);
    if (!synthesis_engine) {
        world_analysis_destroy(analysis_engine);
        free(test_audio);
        strcpy(result.error_message, "WORLD 합성 엔진 생성 실패");
        return result;
    }

    // 음성 합성 수행
    float* output_audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    int output_length = 0;

    error = world_synthesize_audio(synthesis_engine, &world_params, output_audio, &output_length);

    clock_t end = clock();
    result.processing_time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "WORLD 합성 실패: %s", world_get_error_string(error));
        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        free(test_audio);
        return result;
    }

    // 합성 결과 검증
    if (output_length <= 0 || !output_audio) {
        strcpy(result.error_message, "합성 결과가 비어있음");
        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        free(test_audio);
        return result;
    }

    // 출력 오디오의 RMS 값 확인 (무음이 아닌지 검증)
    double rms = 0.0;
    for (int i = 0; i < output_length; i++) {
        rms += output_audio[i] * output_audio[i];
    }
    rms = sqrt(rms / output_length);

    if (rms < 0.001) {  // 너무 작은 값이면 무음으로 간주
        strcpy(result.error_message, "합성된 오디오가 무음에 가까움");
        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        free(test_audio);
        return result;
    }

    free(output_audio);
    world_synthesis_destroy(synthesis_engine);
    world_analysis_destroy(analysis_engine);
    free(test_audio);

    result.passed = true;
    return result;
}

/**
 * @brief 실시간 성능 요구사항 테스트 (요구사항 6.1)
 */
static TestResult test_realtime_performance_requirement(void) {
    TestResult result = {false, 0.0, 0, ""};

    printf("  - 실시간 성능 요구사항 테스트 (100ms 이내)...\n");

    // 짧은 오디오 세그먼트 생성 (100ms)
    int short_length = TEST_SAMPLE_RATE / 10;  // 0.1초
    float* short_audio = generate_test_audio(TEST_SAMPLE_RATE, 0.1, 440.0);
    if (!short_audio) {
        strcpy(result.error_message, "짧은 테스트 오디오 생성 실패");
        return result;
    }

    clock_t start = clock();

    // 전체 처리 파이프라인 실행
    WorldAnalysisConfig config = {0};
    config.sample_rate = TEST_SAMPLE_RATE;
    config.frame_period = 5.0;
    config.f0_floor = 80.0;
    config.f0_ceil = 800.0;

    WorldAnalysisEngine* analysis_engine = world_analysis_create(&config);
    if (!analysis_engine) {
        free(short_audio);
        strcpy(result.error_message, "분석 엔진 생성 실패");
        return result;
    }

    WorldParameters world_params = {0};
    WorldErrorCode error = world_analyze_audio(analysis_engine, short_audio, short_length, &world_params);
    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "분석 실패: %s", world_get_error_string(error));
        world_analysis_destroy(analysis_engine);
        free(short_audio);
        return result;
    }

    WorldSynthesisConfig synthesis_config = {0};
    synthesis_config.sample_rate = TEST_SAMPLE_RATE;
    synthesis_config.frame_period = 5.0;

    WorldSynthesisEngine* synthesis_engine = world_synthesis_create(&synthesis_config);
    if (!synthesis_engine) {
        world_analysis_destroy(analysis_engine);
        free(short_audio);
        strcpy(result.error_message, "합성 엔진 생성 실패");
        return result;
    }

    float* output_audio = (float*)malloc(short_length * sizeof(float));
    int output_length = 0;

    error = world_synthesize_audio(synthesis_engine, &world_params, output_audio, &output_length);

    clock_t end = clock();
    result.processing_time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "합성 실패: %s", world_get_error_string(error));
        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        free(short_audio);
        return result;
    }

    // 100ms 이내 처리 요구사항 검증
    if (result.processing_time_ms > MAX_PROCESSING_TIME_MS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "처리 시간이 요구사항을 초과함: %.2fms > %dms",
                result.processing_time_ms, MAX_PROCESSING_TIME_MS);
        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
        free(short_audio);
        return result;
    }

    free(output_audio);
    world_synthesis_destroy(synthesis_engine);
    world_analysis_destroy(analysis_engine);
    free(short_audio);

    result.passed = true;
    return result;
}

/**
 * @brief 파일 I/O 통합 테스트
 */
static TestResult test_file_io_integration(void) {
    TestResult result = {false, 0.0, 0, ""};

    printf("  - 파일 I/O 통합 테스트...\n");

    clock_t start = clock();

    // 테스트 WAV 파일 생성
    const char* test_input_file = "test_input.wav";
    const char* test_output_file = "test_output.wav";

    float* test_audio = generate_test_audio(TEST_SAMPLE_RATE, 0.5, 440.0);
    if (!test_audio) {
        strcpy(result.error_message, "테스트 오디오 생성 실패");
        return result;
    }

    // WAV 파일 쓰기 테스트
    WorldErrorCode error = write_wav_file(test_input_file, test_audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE);
    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "WAV 파일 쓰기 실패: %s", world_get_error_string(error));
        free(test_audio);
        return result;
    }

    // WAV 파일 읽기 테스트
    float* read_audio = NULL;
    int read_length = 0;
    int read_sample_rate = 0;

    error = read_wav_file(test_input_file, &read_audio, &read_length, &read_sample_rate);
    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "WAV 파일 읽기 실패: %s", world_get_error_string(error));
        free(test_audio);
        remove(test_input_file);
        return result;
    }

    // 읽은 데이터 검증
    if (read_length != TEST_AUDIO_LENGTH || read_sample_rate != TEST_SAMPLE_RATE) {
        snprintf(result.error_message, sizeof(result.error_message),
                "읽은 파일 정보가 다름: length=%d (expected %d), sr=%d (expected %d)",
                read_length, TEST_AUDIO_LENGTH, read_sample_rate, TEST_SAMPLE_RATE);
        free(test_audio);
        free(read_audio);
        remove(test_input_file);
        return result;
    }

    // 전체 처리 파이프라인으로 출력 파일 생성
    WorldAnalysisConfig config = {0};
    config.sample_rate = TEST_SAMPLE_RATE;
    config.frame_period = 5.0;
    config.f0_floor = 80.0;
    config.f0_ceil = 800.0;

    WorldAnalysisEngine* analysis_engine = world_analysis_create(&config);
    WorldParameters world_params = {0};
    error = world_analyze_audio(analysis_engine, read_audio, read_length, &world_params);

    if (error == WORLD_SUCCESS) {
        WorldSynthesisConfig synthesis_config = {0};
        synthesis_config.sample_rate = TEST_SAMPLE_RATE;
        synthesis_config.frame_period = 5.0;

        WorldSynthesisEngine* synthesis_engine = world_synthesis_create(&synthesis_config);
        float* output_audio = (float*)malloc(read_length * sizeof(float));
        int output_length = 0;

        error = world_synthesize_audio(synthesis_engine, &world_params, output_audio, &output_length);

        if (error == WORLD_SUCCESS) {
            error = write_wav_file(test_output_file, output_audio, output_length, TEST_SAMPLE_RATE);
        }

        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
    }

    world_analysis_destroy(analysis_engine);

    clock_t end = clock();
    result.processing_time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    // 정리
    free(test_audio);
    free(read_audio);
    remove(test_input_file);
    remove(test_output_file);

    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "파일 I/O 통합 처리 실패: %s", world_get_error_string(error));
        return result;
    }

    result.passed = true;
    return result;
}

/**
 * @brief 메인 통합 테스트 실행
 */
int main(void) {
    printf("=== world4utau 통합 테스트 스위트 ===\n\n");

    if (!initialize_test_environment()) {
        printf("테스트 환경 초기화 실패\n");
        return 1;
    }

    TestResult results[5];
    int test_count = 0;
    int passed_count = 0;

    // 테스트 실행
    printf("1. UTAU 파라미터 파싱 테스트\n");
    results[test_count] = test_utau_parameter_parsing();
    if (results[test_count].passed) {
        printf("   ✓ 통과 (%.2fms)\n", results[test_count].processing_time_ms);
        passed_count++;
    } else {
        printf("   ✗ 실패: %s\n", results[test_count].error_message);
    }
    test_count++;

    printf("\n2. WORLD 분석 파이프라인 테스트\n");
    results[test_count] = test_world_analysis_pipeline();
    if (results[test_count].passed) {
        printf("   ✓ 통과 (%.2fms, %zu bytes)\n",
               results[test_count].processing_time_ms,
               results[test_count].memory_usage_bytes);
        passed_count++;
    } else {
        printf("   ✗ 실패: %s\n", results[test_count].error_message);
    }
    test_count++;

    printf("\n3. WORLD 합성 파이프라인 테스트\n");
    results[test_count] = test_world_synthesis_pipeline();
    if (results[test_count].passed) {
        printf("   ✓ 통과 (%.2fms)\n", results[test_count].processing_time_ms);
        passed_count++;
    } else {
        printf("   ✗ 실패: %s\n", results[test_count].error_message);
    }
    test_count++;

    printf("\n4. 실시간 성능 요구사항 테스트\n");
    results[test_count] = test_realtime_performance_requirement();
    if (results[test_count].passed) {
        printf("   ✓ 통과 (%.2fms < %dms)\n",
               results[test_count].processing_time_ms, MAX_PROCESSING_TIME_MS);
        passed_count++;
    } else {
        printf("   ✗ 실패: %s\n", results[test_count].error_message);
    }
    test_count++;

    printf("\n5. 파일 I/O 통합 테스트\n");
    results[test_count] = test_file_io_integration();
    if (results[test_count].passed) {
        printf("   ✓ 통과 (%.2fms)\n", results[test_count].processing_time_ms);
        passed_count++;
    } else {
        printf("   ✗ 실패: %s\n", results[test_count].error_message);
    }
    test_count++;

    // 결과 요약
    printf("\n=== 테스트 결과 요약 ===\n");
    printf("총 테스트: %d개\n", test_count);
    printf("통과: %d개\n", passed_count);
    printf("실패: %d개\n", test_count - passed_count);
    printf("성공률: %.1f%%\n", (double)passed_count / test_count * 100.0);

    // 성능 통계
    double total_time = 0.0;
    for (int i = 0; i < test_count; i++) {
        total_time += results[i].processing_time_ms;
    }
    printf("총 처리 시간: %.2fms\n", total_time);
    printf("평균 처리 시간: %.2fms\n", total_time / test_count);

    cleanup_test_environment();

    return (passed_count == test_count) ? 0 : 1;
}