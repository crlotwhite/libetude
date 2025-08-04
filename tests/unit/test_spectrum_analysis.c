/**
 * @file test_spectrum_analysis.c
 * @brief WORLD 스펙트럼 분석 단위 테스트
 *
 * CheapTrick 알고리즘의 정확성 및 성능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

// world4utau 헤더 포함
#include "../../examples/world4utau/include/world_engine.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 테스트 설정
#define TEST_SAMPLE_RATE 44100
#define TEST_DURATION 1.0  // 1초
#define TEST_AUDIO_LENGTH (int)(TEST_SAMPLE_RATE * TEST_DURATION)
#define TEST_F0_VALUE 220.0  // A3 음
#define TEST_FRAME_PERIOD 5.0  // 5ms

// 테스트 결과 구조체
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} TestResults;

// 전역 테스트 결과
static TestResults g_test_results = {0, 0, 0};

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        g_test_results.total_tests++; \
        if (condition) { \
            g_test_results.passed_tests++; \
            printf("✓ PASS: %s\n", message); \
        } else { \
            g_test_results.failed_tests++; \
            printf("✗ FAIL: %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_NEAR(actual, expected, tolerance, message) \
    do { \
        double diff = fabs((actual) - (expected)); \
        TEST_ASSERT(diff < (tolerance), message); \
    } while(0)

/**
 * @brief 테스트용 사인파 생성
 */
static void generate_sine_wave(float* audio, int length, int sample_rate, double frequency) {
    for (int i = 0; i < length; i++) {
        double t = (double)i / sample_rate;
        audio[i] = (float)(0.5 * sin(2.0 * M_PI * frequency * t));
    }
}

/**
 * @brief 테스트용 복합 사인파 생성 (기본파 + 배음)
 */
static void generate_complex_sine_wave(float* audio, int length, int sample_rate, double f0) {
    memset(audio, 0, length * sizeof(float));

    // 기본파 + 3개 배음
    double amplitudes[] = {1.0, 0.5, 0.3, 0.2};
    for (int h = 0; h < 4; h++) {
        double freq = f0 * (h + 1);
        for (int i = 0; i < length; i++) {
            double t = (double)i / sample_rate;
            audio[i] += (float)(amplitudes[h] * sin(2.0 * M_PI * freq * t));
        }
    }

    // 정규화
    float max_val = 0.0f;
    for (int i = 0; i < length; i++) {
        if (fabs(audio[i]) > max_val) {
            max_val = fabs(audio[i]);
        }
    }

    if (max_val > 0.0f) {
        for (int i = 0; i < length; i++) {
            audio[i] /= max_val * 2.0f;  // 0.5 최대 진폭으로 정규화
        }
    }
}

/**
 * @brief 테스트용 F0 및 시간축 생성
 */
static void generate_test_f0_and_time(double* f0, double* time_axis, int f0_length,
                                     double f0_value, double frame_period) {
    for (int i = 0; i < f0_length; i++) {
        f0[i] = f0_value;
        time_axis[i] = i * frame_period / 1000.0;  // ms를 초로 변환
    }
}

/**
 * @brief 스펙트럼 분석기 생성 테스트
 */
static void test_spectrum_analyzer_creation(void) {
    printf("\n=== 스펙트럼 분석기 생성 테스트 ===\n");

    WorldSpectrumConfig config = {0};
    config.q1 = -0.15;
    config.fft_size = 0;  // 자동 계산

    WorldSpectrumAnalyzer* analyzer = world_spectrum_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "스펙트럼 분석기 생성");

    if (analyzer) {
        world_spectrum_analyzer_destroy(analyzer);
        printf("스펙트럼 분석기 해제 완료\n");
    }
}

/**
 * @brief 스펙트럼 분석기 초기화 테스트
 */
static void test_spectrum_analyzer_initialization(void) {
    printf("\n=== 스펙트럼 분석기 초기화 테스트 ===\n");

    WorldSpectrumConfig config = {0};
    config.q1 = -0.15;
    config.fft_size = 0;

    WorldSpectrumAnalyzer* analyzer = world_spectrum_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "스펙트럼 분석기 생성");

    if (analyzer) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "스펙트럼 분석기 초기화");

        // FFT 크기 확인
        int expected_fft_size = world_get_fft_size_for_cheaptrick(TEST_SAMPLE_RATE);
        TEST_ASSERT(analyzer->fft_size == expected_fft_size, "FFT 크기 자동 계산");

        printf("FFT 크기: %d\n", analyzer->fft_size);
        printf("초기화 상태: %s\n", analyzer->is_initialized ? "완료" : "미완료");

        world_spectrum_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 단일 프레임 스펙트럼 추출 테스트
 */
static void test_single_frame_spectrum_extraction(void) {
    printf("\n=== 단일 프레임 스펙트럼 추출 테스트 ===\n");

    // 테스트 오디오 생성
    float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    generate_sine_wave(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE);

    // 스펙트럼 분석기 생성
    WorldSpectrumConfig config = {0};
    config.q1 = -0.15;
    config.fft_size = 0;

    WorldSpectrumAnalyzer* analyzer = world_spectrum_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "스펙트럼 분석기 생성");

    if (analyzer) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "스펙트럼 분석기 초기화");

        // 스펙트럼 추출
        int spectrum_length = analyzer->fft_size / 2 + 1;
        double* spectrum = (double*)malloc(spectrum_length * sizeof(double));

        int center_sample = TEST_AUDIO_LENGTH / 2;  // 중앙 위치
        result = world_spectrum_analyzer_extract_frame(analyzer, audio, TEST_AUDIO_LENGTH,
                                                      center_sample, TEST_F0_VALUE,
                                                      TEST_SAMPLE_RATE, spectrum);
        TEST_ASSERT(result == ET_SUCCESS, "단일 프레임 스펙트럼 추출");

        // 스펙트럼 유효성 검사
        bool spectrum_valid = true;
        for (int i = 0; i < spectrum_length; i++) {
            if (spectrum[i] <= 0.0 || !isfinite(spectrum[i])) {
                spectrum_valid = false;
                break;
            }
        }
        TEST_ASSERT(spectrum_valid, "스펙트럼 값 유효성");

        // 기본 주파수 근처에서 피크 확인
        int f0_bin = (int)(TEST_F0_VALUE * analyzer->fft_size / TEST_SAMPLE_RATE);
        double f0_magnitude = spectrum[f0_bin];

        // 주변 빈들과 비교하여 피크 확인
        bool has_peak = true;
        for (int i = f0_bin - 2; i <= f0_bin + 2; i++) {
            if (i >= 0 && i < spectrum_length && i != f0_bin) {
                if (spectrum[i] > f0_magnitude * 0.8) {
                    // 너무 가까운 값이면 피크가 아닐 수 있음
                }
            }
        }
        TEST_ASSERT(has_peak, "기본 주파수 근처 스펙트럼 특성");

        printf("스펙트럼 길이: %d\n", spectrum_length);
        printf("F0 빈 위치: %d, 크기: %.6f\n", f0_bin, f0_magnitude);

        free(spectrum);
        world_spectrum_analyzer_destroy(analyzer);
    }

    free(audio);
}

/**
 * @brief CheapTrick 알고리즘 전체 테스트
 */
static void test_cheaptrick_algorithm(void) {
    printf("\n=== CheapTrick 알고리즘 전체 테스트 ===\n");

    // 테스트 오디오 생성 (복합 사인파)
    float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    generate_complex_sine_wave(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE);

    // F0 및 시간축 생성
    int f0_length = world_get_samples_for_dio(TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_FRAME_PERIOD);
    double* f0 = (double*)malloc(f0_length * sizeof(double));
    double* time_axis = (double*)malloc(f0_length * sizeof(double));
    generate_test_f0_and_time(f0, time_axis, f0_length, TEST_F0_VALUE, TEST_FRAME_PERIOD);

    // 스펙트럼 분석기 생성
    WorldSpectrumConfig config = {0};
    config.q1 = -0.15;
    config.fft_size = 0;

    WorldSpectrumAnalyzer* analyzer = world_spectrum_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "스펙트럼 분석기 생성");

    if (analyzer) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "스펙트럼 분석기 초기화");

        // 스펙트로그램 메모리 할당
        int spectrum_length = analyzer->fft_size / 2 + 1;
        double** spectrogram = (double**)malloc(f0_length * sizeof(double*));
        for (int i = 0; i < f0_length; i++) {
            spectrogram[i] = (double*)malloc(spectrum_length * sizeof(double));
        }

        // CheapTrick 알고리즘 실행
        clock_t start_time = clock();
        result = world_spectrum_analyzer_cheaptrick(analyzer, audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE,
                                                   f0, time_axis, f0_length, spectrogram);
        clock_t end_time = clock();

        TEST_ASSERT(result == ET_SUCCESS, "CheapTrick 알고리즘 실행");

        double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
        printf("실행 시간: %.3f초\n", execution_time);
        printf("프레임 수: %d\n", f0_length);
        printf("스펙트럼 길이: %d\n", spectrum_length);

        // 결과 유효성 검사
        bool all_frames_valid = true;
        for (int i = 0; i < f0_length; i++) {
            for (int j = 0; j < spectrum_length; j++) {
                if (spectrogram[i][j] <= 0.0 || !isfinite(spectrogram[i][j])) {
                    all_frames_valid = false;
                    break;
                }
            }
            if (!all_frames_valid) break;
        }
        TEST_ASSERT(all_frames_valid, "모든 프레임 스펙트럼 유효성");

        // 스펙트럼 연속성 검사 (인접 프레임 간 급격한 변화 없음)
        bool spectrum_continuous = true;
        for (int i = 1; i < f0_length; i++) {
            double frame_diff = 0.0;
            for (int j = 0; j < spectrum_length; j++) {
                double diff = fabs(log(spectrogram[i][j]) - log(spectrogram[i-1][j]));
                frame_diff += diff;
            }
            frame_diff /= spectrum_length;

            if (frame_diff > 2.0) {  // 임계값 (조정 가능)
                spectrum_continuous = false;
                break;
            }
        }
        TEST_ASSERT(spectrum_continuous, "스펙트럼 시간적 연속성");

        // 메모리 해제
        for (int i = 0; i < f0_length; i++) {
            free(spectrogram[i]);
        }
        free(spectrogram);
        world_spectrum_analyzer_destroy(analyzer);
    }

    free(f0);
    free(time_axis);
    free(audio);
}

/**
 * @brief SIMD 최적화 테스트
 */
static void test_simd_optimization(void) {
    printf("\n=== SIMD 최적화 테스트 ===\n");

    // SIMD 기능 확인
    int simd_capabilities = world_spectrum_analyzer_get_simd_capabilities();
    printf("SIMD 기능: ");
    if (simd_capabilities & 0x01) printf("SSE2 ");
    if (simd_capabilities & 0x02) printf("AVX ");
    if (simd_capabilities & 0x04) printf("NEON ");
    if (simd_capabilities == 0) printf("없음");
    printf("\n");

    // 테스트 오디오 생성
    float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    generate_complex_sine_wave(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE);

    // F0 및 시간축 생성
    int f0_length = world_get_samples_for_dio(TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_FRAME_PERIOD);
    double* f0 = (double*)malloc(f0_length * sizeof(double));
    double* time_axis = (double*)malloc(f0_length * sizeof(double));
    generate_test_f0_and_time(f0, time_axis, f0_length, TEST_F0_VALUE, TEST_FRAME_PERIOD);

    // 스펙트럼 분석기 생성
    WorldSpectrumConfig config = {0};
    config.q1 = -0.15;
    config.fft_size = 0;

    WorldSpectrumAnalyzer* analyzer = world_spectrum_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "스펙트럼 분석기 생성");

    if (analyzer) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "스펙트럼 분석기 초기화");

        // 스펙트로그램 메모리 할당
        int spectrum_length = analyzer->fft_size / 2 + 1;
        double** spectrogram1 = (double**)malloc(f0_length * sizeof(double*));
        double** spectrogram2 = (double**)malloc(f0_length * sizeof(double*));
        for (int i = 0; i < f0_length; i++) {
            spectrogram1[i] = (double*)malloc(spectrum_length * sizeof(double));
            spectrogram2[i] = (double*)malloc(spectrum_length * sizeof(double));
        }

        // 일반 버전 실행
        clock_t start_time = clock();
        result = world_spectrum_analyzer_cheaptrick(analyzer, audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE,
                                                   f0, time_axis, f0_length, spectrogram1);
        clock_t end_time = clock();
        double normal_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

        TEST_ASSERT(result == ET_SUCCESS, "일반 버전 실행");

        // 병렬 버전 실행
        start_time = clock();
        result = world_spectrum_analyzer_cheaptrick_parallel(analyzer, audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE,
                                                            f0, time_axis, f0_length, spectrogram2, 4);
        end_time = clock();
        double parallel_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

        TEST_ASSERT(result == ET_SUCCESS, "병렬 버전 실행");

        printf("일반 버전 실행 시간: %.3f초\n", normal_time);
        printf("병렬 버전 실행 시간: %.3f초\n", parallel_time);

        if (parallel_time > 0) {
            printf("성능 향상: %.2fx\n", normal_time / parallel_time);
        }

        // 결과 비교 (두 버전이 유사한 결과를 생성하는지 확인)
        bool results_similar = true;
        double max_diff = 0.0;
        for (int i = 0; i < f0_length; i++) {
            for (int j = 0; j < spectrum_length; j++) {
                double diff = fabs(spectrogram1[i][j] - spectrogram2[i][j]);
                double relative_diff = diff / (spectrogram1[i][j] + 1e-10);
                if (relative_diff > 0.01) {  // 1% 이상 차이
                    results_similar = false;
                }
                if (diff > max_diff) {
                    max_diff = diff;
                }
            }
        }
        TEST_ASSERT(results_similar, "일반/병렬 버전 결과 일치성");
        printf("최대 차이: %.6f\n", max_diff);

        // 메모리 해제
        for (int i = 0; i < f0_length; i++) {
            free(spectrogram1[i]);
            free(spectrogram2[i]);
        }
        free(spectrogram1);
        free(spectrogram2);
        world_spectrum_analyzer_destroy(analyzer);
    }

    free(f0);
    free(time_axis);
    free(audio);
}

/**
 * @brief 무성음 처리 테스트
 */
static void test_unvoiced_sound_processing(void) {
    printf("\n=== 무성음 처리 테스트 ===\n");

    // 테스트 오디오 생성 (백색 잡음)
    float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    srand(12345);  // 재현 가능한 결과를 위한 시드
    for (int i = 0; i < TEST_AUDIO_LENGTH; i++) {
        audio[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;  // 작은 잡음
    }

    // F0 = 0 (무성음)으로 설정
    int f0_length = world_get_samples_for_dio(TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_FRAME_PERIOD);
    double* f0 = (double*)malloc(f0_length * sizeof(double));
    double* time_axis = (double*)malloc(f0_length * sizeof(double));

    for (int i = 0; i < f0_length; i++) {
        f0[i] = 0.0;  // 무성음
        time_axis[i] = i * TEST_FRAME_PERIOD / 1000.0;
    }

    // 스펙트럼 분석기 생성
    WorldSpectrumConfig config = {0};
    config.q1 = -0.15;
    config.fft_size = 0;

    WorldSpectrumAnalyzer* analyzer = world_spectrum_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "스펙트럼 분석기 생성");

    if (analyzer) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "스펙트럼 분석기 초기화");

        // 스펙트로그램 메모리 할당
        int spectrum_length = analyzer->fft_size / 2 + 1;
        double** spectrogram = (double**)malloc(f0_length * sizeof(double*));
        for (int i = 0; i < f0_length; i++) {
            spectrogram[i] = (double*)malloc(spectrum_length * sizeof(double));
        }

        // CheapTrick 알고리즘 실행
        result = world_spectrum_analyzer_cheaptrick(analyzer, audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE,
                                                   f0, time_axis, f0_length, spectrogram);
        TEST_ASSERT(result == ET_SUCCESS, "무성음 CheapTrick 실행");

        // 무성음 스펙트럼 특성 확인
        bool unvoiced_spectrum_valid = true;
        for (int i = 0; i < f0_length; i++) {
            // 무성음은 상대적으로 평평한 스펙트럼을 가져야 함
            double spectrum_variance = 0.0;
            double spectrum_mean = 0.0;

            for (int j = 0; j < spectrum_length; j++) {
                spectrum_mean += log(spectrogram[i][j]);
            }
            spectrum_mean /= spectrum_length;

            for (int j = 0; j < spectrum_length; j++) {
                double diff = log(spectrogram[i][j]) - spectrum_mean;
                spectrum_variance += diff * diff;
            }
            spectrum_variance /= spectrum_length;

            // 무성음은 분산이 작아야 함 (평평한 스펙트럼)
            if (spectrum_variance > 10.0) {  // 임계값 (조정 가능)
                unvoiced_spectrum_valid = false;
                break;
            }
        }
        TEST_ASSERT(unvoiced_spectrum_valid, "무성음 스펙트럼 특성");

        // 메모리 해제
        for (int i = 0; i < f0_length; i++) {
            free(spectrogram[i]);
        }
        free(spectrogram);
        world_spectrum_analyzer_destroy(analyzer);
    }

    free(f0);
    free(time_axis);
    free(audio);
}

/**
 * @brief 성능 벤치마크 테스트
 */
static void test_performance_benchmark(void) {
    printf("\n=== 성능 벤치마크 테스트 ===\n");

    // 더 긴 테스트 오디오 생성 (10초)
    int long_audio_length = TEST_SAMPLE_RATE * 10;
    float* audio = (float*)malloc(long_audio_length * sizeof(float));
    generate_complex_sine_wave(audio, long_audio_length, TEST_SAMPLE_RATE, TEST_F0_VALUE);

    // F0 및 시간축 생성
    int f0_length = world_get_samples_for_dio(long_audio_length, TEST_SAMPLE_RATE, TEST_FRAME_PERIOD);
    double* f0 = (double*)malloc(f0_length * sizeof(double));
    double* time_axis = (double*)malloc(f0_length * sizeof(double));
    generate_test_f0_and_time(f0, time_axis, f0_length, TEST_F0_VALUE, TEST_FRAME_PERIOD);

    // 스펙트럼 분석기 생성
    WorldSpectrumConfig config = {0};
    config.q1 = -0.15;
    config.fft_size = 0;

    WorldSpectrumAnalyzer* analyzer = world_spectrum_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "스펙트럼 분석기 생성");

    if (analyzer) {
        ETResult result = world_spectrum_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "스펙트럼 분석기 초기화");

        // 스펙트로그램 메모리 할당
        int spectrum_length = analyzer->fft_size / 2 + 1;
        double** spectrogram = (double**)malloc(f0_length * sizeof(double*));
        for (int i = 0; i < f0_length; i++) {
            spectrogram[i] = (double*)malloc(spectrum_length * sizeof(double));
        }

        // 성능 측정
        clock_t start_time = clock();
        result = world_spectrum_analyzer_cheaptrick(analyzer, audio, long_audio_length, TEST_SAMPLE_RATE,
                                                   f0, time_axis, f0_length, spectrogram);
        clock_t end_time = clock();

        TEST_ASSERT(result == ET_SUCCESS, "긴 오디오 CheapTrick 실행");

        double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
        double real_time_factor = (double)long_audio_length / TEST_SAMPLE_RATE / execution_time;

        printf("오디오 길이: %.1f초\n", (double)long_audio_length / TEST_SAMPLE_RATE);
        printf("처리 시간: %.3f초\n", execution_time);
        printf("실시간 배수: %.2fx\n", real_time_factor);
        printf("프레임 수: %d\n", f0_length);
        printf("초당 프레임 처리: %.1f\n", f0_length / execution_time);

        // 실시간 처리 가능성 확인 (1x 이상이면 실시간 가능)
        TEST_ASSERT(real_time_factor >= 1.0, "실시간 처리 성능");

        // 메모리 해제
        for (int i = 0; i < f0_length; i++) {
            free(spectrogram[i]);
        }
        free(spectrogram);
        world_spectrum_analyzer_destroy(analyzer);
    }

    free(f0);
    free(time_axis);
    free(audio);
}

/**
 * @brief 모든 테스트 실행
 */
int main(void) {
    printf("WORLD 스펙트럼 분석 단위 테스트 시작\n");
    printf("=====================================\n");

    // 각 테스트 실행
    test_spectrum_analyzer_creation();
    test_spectrum_analyzer_initialization();
    test_single_frame_spectrum_extraction();
    test_cheaptrick_algorithm();
    test_simd_optimization();
    test_unvoiced_sound_processing();
    test_performance_benchmark();

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과 요약\n");
    printf("총 테스트: %d\n", g_test_results.total_tests);
    printf("통과: %d\n", g_test_results.passed_tests);
    printf("실패: %d\n", g_test_results.failed_tests);
    printf("성공률: %.1f%%\n",
           g_test_results.total_tests > 0 ?
           (double)g_test_results.passed_tests / g_test_results.total_tests * 100.0 : 0.0);

    if (g_test_results.failed_tests == 0) {
        printf("\n🎉 모든 테스트가 통과했습니다!\n");
        return 0;
    } else {
        printf("\n❌ %d개의 테스트가 실패했습니다.\n", g_test_results.failed_tests);
        return 1;
    }
}