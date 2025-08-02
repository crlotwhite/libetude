/**
 * @file benchmark_world4utau.c
 * @brief world4utau 성능 벤치마크 도구
 *
 * libetude 기반 world4utau 구현의 성능을 측정하고 기존 구현과 비교합니다.
 * 요구사항 6.1, 6.2, 6.3을 만족하는 성능 벤치마크를 구현합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

// libetude 헤더
#include "libetude/api.h"
#include "libetude/benchmark.h"
#include "libetude/performance_analyzer.h"
#include "libetude/memory.h"
#include "libetude/profiler.h"

// world4utau 헤더
#include "../../examples/world4utau/include/utau_interface.h"
#include "../../examples/world4utau/include/world_engine.h"
#include "../../examples/world4utau/include/audio_file_io.h"
#include "../../examples/world4utau/include/world_error.h"

// 벤치마크 설정
#define BENCHMARK_ITERATIONS 10
#define MAX_AUDIO_LENGTHS 5
#define MAX_SAMPLE_RATES 3

// 테스트 오디오 길이 (샘플 수)
static const int test_audio_lengths[MAX_AUDIO_LENGTHS] = {
    4410,   // 0.1초 @ 44.1kHz
    22050,  // 0.5초 @ 44.1kHz
    44100,  // 1.0초 @ 44.1kHz
    88200,  // 2.0초 @ 44.1kHz
    220500  // 5.0초 @ 44.1kHz
};

// 테스트 샘플링 레이트
static const int test_sample_rates[MAX_SAMPLE_RATES] = {
    22050,  // 22.05kHz
    44100,  // 44.1kHz
    48000   // 48kHz
};

// 벤치마크 결과 구조체
typedef struct {
    double analysis_time_ms;      // 분석 시간 (ms)
    double synthesis_time_ms;     // 합성 시간 (ms)
    double total_time_ms;         // 총 처리 시간 (ms)
    size_t peak_memory_bytes;     // 최대 메모리 사용량 (bytes)
    size_t avg_memory_bytes;      // 평균 메모리 사용량 (bytes)
    double cpu_usage_percent;     // CPU 사용률 (%)
    int audio_length;             // 오디오 길이 (샘플)
    int sample_rate;              // 샘플링 레이트
    bool success;                 // 성공 여부
    char error_message[256];      // 에러 메시지
} BenchmarkResult;

// 전역 변수
static ETBenchmarkContext* g_benchmark_ctx = NULL;
static ETPerformanceAnalyzer* g_perf_analyzer = NULL;
static ETProfiler* g_profiler = NULL;

/**
 * @brief 고해상도 시간 측정
 */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/**
 * @brief 벤치마크 환경 초기화
 */
static bool initialize_benchmark_environment(void) {
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

    // 프로파일러 생성
    g_profiler = et_profiler_create();
    if (!g_profiler) {
        printf("프로파일러 생성 실패\n");
        return false;
    }

    return true;
}

/**
 * @brief 벤치마크 환경 정리
 */
static void cleanup_benchmark_environment(void) {
    if (g_profiler) {
        et_profiler_destroy(g_profiler);
        g_profiler = NULL;
    }

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
 * @brief 테스트용 복합 오디오 신호 생성
 */
static float* generate_complex_test_audio(int sample_rate, int length) {
    float* audio = (float*)malloc(length * sizeof(float));
    if (!audio) return NULL;

    // 복합 신호 생성 (기본 주파수 + 하모닉 + 노이즈)
    for (int i = 0; i < length; i++) {
        double t = (double)i / sample_rate;

        // 기본 주파수 (220Hz)
        double fundamental = 0.5 * sin(2.0 * M_PI * 220.0 * t);

        // 하모닉 (440Hz, 660Hz)
        double harmonic1 = 0.3 * sin(2.0 * M_PI * 440.0 * t);
        double harmonic2 = 0.2 * sin(2.0 * M_PI * 660.0 * t);

        // 약간의 노이즈
        double noise = 0.05 * ((double)rand() / RAND_MAX - 0.5);

        // 엔벨로프 (페이드 인/아웃)
        double envelope = 1.0;
        if (i < sample_rate * 0.1) {  // 0.1초 페이드 인
            envelope = (double)i / (sample_rate * 0.1);
        } else if (i > length - sample_rate * 0.1) {  // 0.1초 페이드 아웃
            envelope = (double)(length - i) / (sample_rate * 0.1);
        }

        audio[i] = (float)((fundamental + harmonic1 + harmonic2 + noise) * envelope);
    }

    return audio;
}

/**
 * @brief 메모리 사용량 측정
 */
static size_t get_memory_usage(void) {
    // 간단한 메모리 사용량 추정 (실제 구현에서는 더 정확한 방법 사용)
    FILE* file = fopen("/proc/self/status", "r");
    if (!file) return 0;

    char line[256];
    size_t memory_kb = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %zu kB", &memory_kb);
            break;
        }
    }

    fclose(file);
    return memory_kb * 1024;  // bytes로 변환
}

/**
 * @brief WORLD 분석 성능 벤치마크
 */
static BenchmarkResult benchmark_world_analysis(int sample_rate, int audio_length) {
    BenchmarkResult result = {0};
    result.sample_rate = sample_rate;
    result.audio_length = audio_length;

    // 테스트 오디오 생성
    float* test_audio = generate_complex_test_audio(sample_rate, audio_length);
    if (!test_audio) {
        strcpy(result.error_message, "테스트 오디오 생성 실패");
        return result;
    }

    // WORLD 분석 엔진 설정
    WorldAnalysisConfig config = {0};
    config.sample_rate = sample_rate;
    config.frame_period = 5.0;  // 5ms
    config.f0_floor = 80.0;
    config.f0_ceil = 800.0;

    double total_time = 0.0;
    size_t total_memory = 0;
    int successful_runs = 0;

    // 여러 번 실행하여 평균 성능 측정
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        size_t memory_before = get_memory_usage();
        double start_time = get_time_ms();

        // 프로파일링 시작
        et_profiler_start(g_profiler, "world_analysis");

        WorldAnalysisEngine* engine = world_analysis_create(&config);
        if (!engine) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "분석 엔진 생성 실패 (반복 %d)", i);
            continue;
        }

        WorldParameters world_params = {0};
        WorldErrorCode error = world_analyze_audio(engine, test_audio, audio_length, &world_params);

        // 프로파일링 종료
        et_profiler_end(g_profiler, "world_analysis");

        double end_time = get_time_ms();
        size_t memory_after = get_memory_usage();

        if (error == WORLD_SUCCESS) {
            total_time += (end_time - start_time);
            total_memory += (memory_after - memory_before);
            successful_runs++;
        } else {
            snprintf(result.error_message, sizeof(result.error_message),
                    "분석 실패 (반복 %d): %s", i, world_get_error_string(error));
        }

        world_analysis_destroy(engine);
    }

    free(test_audio);

    if (successful_runs > 0) {
        result.analysis_time_ms = total_time / successful_runs;
        result.avg_memory_bytes = total_memory / successful_runs;
        result.success = true;
    }

    return result;
}

/**
 * @brief WORLD 합성 성능 벤치마크
 */
static BenchmarkResult benchmark_world_synthesis(int sample_rate, int audio_length) {
    BenchmarkResult result = {0};
    result.sample_rate = sample_rate;
    result.audio_length = audio_length;

    // 먼저 분석을 수행하여 WORLD 파라미터 획득
    float* test_audio = generate_complex_test_audio(sample_rate, audio_length);
    if (!test_audio) {
        strcpy(result.error_message, "테스트 오디오 생성 실패");
        return result;
    }

    WorldAnalysisConfig analysis_config = {0};
    analysis_config.sample_rate = sample_rate;
    analysis_config.frame_period = 5.0;
    analysis_config.f0_floor = 80.0;
    analysis_config.f0_ceil = 800.0;

    WorldAnalysisEngine* analysis_engine = world_analysis_create(&analysis_config);
    if (!analysis_engine) {
        free(test_audio);
        strcpy(result.error_message, "분석 엔진 생성 실패");
        return result;
    }

    WorldParameters world_params = {0};
    WorldErrorCode error = world_analyze_audio(analysis_engine, test_audio, audio_length, &world_params);
    if (error != WORLD_SUCCESS) {
        snprintf(result.error_message, sizeof(result.error_message),
                "분석 단계 실패: %s", world_get_error_string(error));
        world_analysis_destroy(analysis_engine);
        free(test_audio);
        return result;
    }

    // 합성 성능 측정
    WorldSynthesisConfig synthesis_config = {0};
    synthesis_config.sample_rate = sample_rate;
    synthesis_config.frame_period = 5.0;

    double total_time = 0.0;
    size_t total_memory = 0;
    int successful_runs = 0;

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        size_t memory_before = get_memory_usage();
        double start_time = get_time_ms();

        // 프로파일링 시작
        et_profiler_start(g_profiler, "world_synthesis");

        WorldSynthesisEngine* synthesis_engine = world_synthesis_create(&synthesis_config);
        if (!synthesis_engine) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "합성 엔진 생성 실패 (반복 %d)", i);
            continue;
        }

        float* output_audio = (float*)malloc(audio_length * sizeof(float));
        int output_length = 0;

        error = world_synthesize_audio(synthesis_engine, &world_params, output_audio, &output_length);

        // 프로파일링 종료
        et_profiler_end(g_profiler, "world_synthesis");

        double end_time = get_time_ms();
        size_t memory_after = get_memory_usage();

        if (error == WORLD_SUCCESS) {
            total_time += (end_time - start_time);
            total_memory += (memory_after - memory_before);
            successful_runs++;
        } else {
            snprintf(result.error_message, sizeof(result.error_message),
                    "합성 실패 (반복 %d): %s", i, world_get_error_string(error));
        }

        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
    }

    world_analysis_destroy(analysis_engine);
    free(test_audio);

    if (successful_runs > 0) {
        result.synthesis_time_ms = total_time / successful_runs;
        result.avg_memory_bytes = total_memory / successful_runs;
        result.success = true;
    }

    return result;
}

/**
 * @brief 전체 파이프라인 성능 벤치마크
 */
static BenchmarkResult benchmark_full_pipeline(int sample_rate, int audio_length) {
    BenchmarkResult result = {0};
    result.sample_rate = sample_rate;
    result.audio_length = audio_length;

    float* test_audio = generate_complex_test_audio(sample_rate, audio_length);
    if (!test_audio) {
        strcpy(result.error_message, "테스트 오디오 생성 실패");
        return result;
    }

    double total_time = 0.0;
    double total_analysis_time = 0.0;
    double total_synthesis_time = 0.0;
    size_t total_memory = 0;
    size_t peak_memory = 0;
    int successful_runs = 0;

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        size_t memory_before = get_memory_usage();
        double pipeline_start = get_time_ms();

        // 분석 단계
        double analysis_start = get_time_ms();

        WorldAnalysisConfig analysis_config = {0};
        analysis_config.sample_rate = sample_rate;
        analysis_config.frame_period = 5.0;
        analysis_config.f0_floor = 80.0;
        analysis_config.f0_ceil = 800.0;

        WorldAnalysisEngine* analysis_engine = world_analysis_create(&analysis_config);
        if (!analysis_engine) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "분석 엔진 생성 실패 (반복 %d)", i);
            continue;
        }

        WorldParameters world_params = {0};
        WorldErrorCode error = world_analyze_audio(analysis_engine, test_audio, audio_length, &world_params);

        double analysis_end = get_time_ms();
        size_t memory_after_analysis = get_memory_usage();

        if (error != WORLD_SUCCESS) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "분석 실패 (반복 %d): %s", i, world_get_error_string(error));
            world_analysis_destroy(analysis_engine);
            continue;
        }

        // 합성 단계
        double synthesis_start = get_time_ms();

        WorldSynthesisConfig synthesis_config = {0};
        synthesis_config.sample_rate = sample_rate;
        synthesis_config.frame_period = 5.0;

        WorldSynthesisEngine* synthesis_engine = world_synthesis_create(&synthesis_config);
        if (!synthesis_engine) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "합성 엔진 생성 실패 (반복 %d)", i);
            world_analysis_destroy(analysis_engine);
            continue;
        }

        float* output_audio = (float*)malloc(audio_length * sizeof(float));
        int output_length = 0;

        error = world_synthesize_audio(synthesis_engine, &world_params, output_audio, &output_length);

        double synthesis_end = get_time_ms();
        double pipeline_end = get_time_ms();
        size_t memory_after = get_memory_usage();

        if (error == WORLD_SUCCESS) {
            total_analysis_time += (analysis_end - analysis_start);
            total_synthesis_time += (synthesis_end - synthesis_start);
            total_time += (pipeline_end - pipeline_start);

            size_t current_memory = memory_after - memory_before;
            total_memory += current_memory;
            if (current_memory > peak_memory) {
                peak_memory = current_memory;
            }

            successful_runs++;
        } else {
            snprintf(result.error_message, sizeof(result.error_message),
                    "합성 실패 (반복 %d): %s", i, world_get_error_string(error));
        }

        free(output_audio);
        world_synthesis_destroy(synthesis_engine);
        world_analysis_destroy(analysis_engine);
    }

    free(test_audio);

    if (successful_runs > 0) {
        result.analysis_time_ms = total_analysis_time / successful_runs;
        result.synthesis_time_ms = total_synthesis_time / successful_runs;
        result.total_time_ms = total_time / successful_runs;
        result.avg_memory_bytes = total_memory / successful_runs;
        result.peak_memory_bytes = peak_memory;
        result.success = true;
    }

    return result;
}

/**
 * @brief 실시간 성능 요구사항 검증
 */
static void verify_realtime_requirements(const BenchmarkResult* results, int result_count) {
    printf("\n=== 실시간 성능 요구사항 검증 ===\n");
    printf("요구사항: 짧은 음성 세그먼트 처리 시간 < 100ms\n\n");

    bool all_passed = true;

    for (int i = 0; i < result_count; i++) {
        if (!results[i].success) continue;

        double audio_duration_ms = (double)results[i].audio_length / results[i].sample_rate * 1000.0;
        bool is_short_segment = audio_duration_ms <= 500.0;  // 0.5초 이하를 짧은 세그먼트로 간주

        if (is_short_segment) {
            bool passed = results[i].total_time_ms < 100.0;
            printf("오디오 길이: %.1fms, 처리 시간: %.2fms - %s\n",
                   audio_duration_ms, results[i].total_time_ms,
                   passed ? "✓ 통과" : "✗ 실패");

            if (!passed) all_passed = false;
        }
    }

    printf("\n실시간 성능 요구사항: %s\n", all_passed ? "✓ 만족" : "✗ 불만족");
}

/**
 * @brief 메모리 효율성 분석
 */
static void analyze_memory_efficiency(const BenchmarkResult* results, int result_count) {
    printf("\n=== 메모리 효율성 분석 ===\n");

    size_t total_avg_memory = 0;
    size_t total_peak_memory = 0;
    int valid_results = 0;

    for (int i = 0; i < result_count; i++) {
        if (!results[i].success) continue;

        double audio_duration_s = (double)results[i].audio_length / results[i].sample_rate;
        double memory_per_second_mb = (double)results[i].avg_memory_bytes / (1024 * 1024) / audio_duration_s;

        printf("오디오: %.1fs, 평균 메모리: %.2fMB, 초당 메모리: %.2fMB/s\n",
               audio_duration_s,
               (double)results[i].avg_memory_bytes / (1024 * 1024),
               memory_per_second_mb);

        total_avg_memory += results[i].avg_memory_bytes;
        total_peak_memory += results[i].peak_memory_bytes;
        valid_results++;
    }

    if (valid_results > 0) {
        printf("\n평균 메모리 사용량: %.2fMB\n", (double)total_avg_memory / valid_results / (1024 * 1024));
        printf("평균 최대 메모리: %.2fMB\n", (double)total_peak_memory / valid_results / (1024 * 1024));
    }
}

/**
 * @brief 성능 비교 리포트 생성
 */
static void generate_performance_report(const BenchmarkResult* results, int result_count) {
    printf("\n=== 성능 벤치마크 리포트 ===\n");

    printf("%-10s %-8s %-12s %-12s %-12s %-12s\n",
           "길이(s)", "SR(Hz)", "분석(ms)", "합성(ms)", "총시간(ms)", "메모리(MB)");
    printf("------------------------------------------------------------------------\n");

    for (int i = 0; i < result_count; i++) {
        if (!results[i].success) {
            printf("%-10.1f %-8d 실패: %s\n",
                   (double)results[i].audio_length / results[i].sample_rate,
                   results[i].sample_rate,
                   results[i].error_message);
            continue;
        }

        printf("%-10.1f %-8d %-12.2f %-12.2f %-12.2f %-12.2f\n",
               (double)results[i].audio_length / results[i].sample_rate,
               results[i].sample_rate,
               results[i].analysis_time_ms,
               results[i].synthesis_time_ms,
               results[i].total_time_ms,
               (double)results[i].avg_memory_bytes / (1024 * 1024));
    }
}

/**
 * @brief 메인 벤치마크 실행
 */
int main(int argc, char* argv[]) {
    printf("=== world4utau 성능 벤치마크 ===\n");
    printf("반복 횟수: %d회\n", BENCHMARK_ITERATIONS);
    printf("테스트 오디오 길이: %d가지\n", MAX_AUDIO_LENGTHS);
    printf("테스트 샘플링 레이트: %d가지\n\n", MAX_SAMPLE_RATES);

    if (!initialize_benchmark_environment()) {
        printf("벤치마크 환경 초기화 실패\n");
        return 1;
    }

    // 벤치마크 결과 저장
    BenchmarkResult* results = (BenchmarkResult*)malloc(
        MAX_AUDIO_LENGTHS * MAX_SAMPLE_RATES * sizeof(BenchmarkResult));
    int result_count = 0;

    // 다양한 조건에서 벤치마크 실행
    for (int sr_idx = 0; sr_idx < MAX_SAMPLE_RATES; sr_idx++) {
        for (int len_idx = 0; len_idx < MAX_AUDIO_LENGTHS; len_idx++) {
            int sample_rate = test_sample_rates[sr_idx];
            int audio_length = (test_audio_lengths[len_idx] * sample_rate) / 44100;  // 샘플링 레이트에 맞게 조정

            printf("벤치마크 실행: %dHz, %.1fs...\n",
                   sample_rate, (double)audio_length / sample_rate);

            results[result_count] = benchmark_full_pipeline(sample_rate, audio_length);
            result_count++;
        }
    }

    // 결과 분석 및 리포트 생성
    generate_performance_report(results, result_count);
    verify_realtime_requirements(results, result_count);
    analyze_memory_efficiency(results, result_count);

    // 프로파일링 결과 출력
    printf("\n=== 프로파일링 결과 ===\n");
    et_profiler_print_results(g_profiler);

    // 성능 통계
    double total_processing_time = 0.0;
    int successful_tests = 0;

    for (int i = 0; i < result_count; i++) {
        if (results[i].success) {
            total_processing_time += results[i].total_time_ms;
            successful_tests++;
        }
    }

    printf("\n=== 전체 통계 ===\n");
    printf("성공한 테스트: %d/%d\n", successful_tests, result_count);
    printf("평균 처리 시간: %.2fms\n", total_processing_time / successful_tests);
    printf("성공률: %.1f%%\n", (double)successful_tests / result_count * 100.0);

    free(results);
    cleanup_benchmark_environment();

    return (successful_tests == result_count) ? 0 : 1;
}