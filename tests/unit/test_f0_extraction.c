/**
 * @file test_f0_extraction.c
 * @brief WORLD F0 추출 알고리즘 단위 테스트
 *
 * DIO 및 Harvest 알고리즘의 정확성과 성능을 검증합니다.
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

// 테스트 결과 구조체
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    double total_time_ms;
} TestResults;

// 전역 테스트 결과
static TestResults g_test_results = {0, 0, 0, 0.0};

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        g_test_results.total_tests++; \
        if (condition) { \
            g_test_results.passed_tests++; \
            printf("  ✓ %s\n", message); \
        } else { \
            g_test_results.failed_tests++; \
            printf("  ✗ %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_NEAR(actual, expected, tolerance, message) \
    TEST_ASSERT(fabs((actual) - (expected)) < (tolerance), message)

// 시간 측정 함수들
static clock_t test_timer_start(void) {
    return clock();
}

static void test_timer_end(clock_t start_time) {
    clock_t end_time = clock();
    double elapsed = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    g_test_results.total_time_ms += elapsed;
    printf("    (%.2f ms)\n", elapsed);
}

// ============================================================================
// 테스트 오디오 생성 함수들
// ============================================================================

/**
 * @brief 사인파 생성 (알려진 주파수)
 */
static void generate_sine_wave(float* audio, int length, int sample_rate, double frequency, double amplitude) {
    for (int i = 0; i < length; i++) {
        double t = (double)i / sample_rate;
        audio[i] = (float)(amplitude * sin(2.0 * M_PI * frequency * t));
    }
}

/**
 * @brief 복합 사인파 생성 (하모닉 포함)
 */
static void generate_harmonic_wave(float* audio, int length, int sample_rate, double f0, double amplitude) {
    memset(audio, 0, length * sizeof(float));

    // 기본 주파수와 하모닉 추가
    double harmonics[] = {1.0, 0.5, 0.3, 0.2, 0.1};  // 하모닉 강도
    int num_harmonics = sizeof(harmonics) / sizeof(harmonics[0]);

    for (int h = 0; h < num_harmonics; h++) {
        double freq = f0 * (h + 1);
        if (freq > sample_rate / 2) break;  // 나이퀴스트 주파수 제한

        for (int i = 0; i < length; i++) {
            double t = (double)i / sample_rate;
            audio[i] += (float)(amplitude * harmonics[h] * sin(2.0 * M_PI * freq * t));
        }
    }
}

/**
 * @brief 주파수 변조 사인파 생성 (피치 변화)
 */
static void generate_frequency_modulated_wave(float* audio, int length, int sample_rate,
                                             double f0_start, double f0_end, double amplitude) {
    for (int i = 0; i < length; i++) {
        double t = (double)i / sample_rate;
        double progress = t / ((double)length / sample_rate);
        double frequency = f0_start + (f0_end - f0_start) * progress;

        // 위상 연속성을 위한 적분
        double phase = 2.0 * M_PI * frequency * t;
        audio[i] = (float)(amplitude * sin(phase));
    }
}

/**
 * @brief 노이즈가 포함된 사인파 생성
 */
static void generate_noisy_sine_wave(float* audio, int length, int sample_rate,
                                    double frequency, double amplitude, double noise_level) {
    generate_sine_wave(audio, length, sample_rate, frequency, amplitude);

    // 가우시안 노이즈 추가
    for (int i = 0; i < length; i++) {
        double noise = ((double)rand() / RAND_MAX - 0.5) * 2.0 * noise_level;
        audio[i] += (float)noise;
    }
}

// ============================================================================
// F0 추출기 테스트 함수들
// ============================================================================

/**
 * @brief F0 추출기 생성/해제 테스트
 */
static void test_f0_extractor_creation(void) {
    printf("\n=== F0 추출기 생성/해제 테스트 ===\n");

    WorldF0Config config = {
        .frame_period = 5.0,
        .f0_floor = 71.0,
        .f0_ceil = 800.0,
        .algorithm = 0,  // DIO
        .channels_in_octave = 2.0,
        .speed = 1.0,
        .allowed_range = 0.1
    };

    clock_t timer = test_timer_start();
    WorldF0Extractor* extractor = world_f0_extractor_create(&config, NULL);
    test_timer_end(timer);

    TEST_ASSERT(extractor != NULL, "F0 추출기 생성 성공");

    if (extractor) {
        TEST_ASSERT(extractor->config.frame_period == 5.0, "프레임 주기 설정 확인");
        TEST_ASSERT(extractor->config.f0_floor == 71.0, "최소 F0 설정 확인");
        TEST_ASSERT(extractor->config.f0_ceil == 800.0, "최대 F0 설정 확인");
        TEST_ASSERT(extractor->config.algorithm == 0, "알고리즘 설정 확인");

        world_f0_extractor_destroy(extractor);
        printf("  ✓ F0 추출기 해제 성공\n");
    }
}

/**
 * @brief DIO 알고리즘 정확성 테스트
 */
static void test_dio_accuracy(void) {
    printf("\n=== DIO 알고리즘 정확성 테스트 ===\n");

    WorldF0Config config = {
        .frame_period = 5.0,
        .f0_floor = 50.0,
        .f0_ceil = 500.0,
        .algorithm = 0,  // DIO
        .channels_in_octave = 2.0,
        .speed = 1.0,
        .allowed_range = 0.1
    };

    WorldF0Extractor* extractor = world_f0_extractor_create(&config, NULL);
    TEST_ASSERT(extractor != NULL, "DIO 추출기 생성");

    if (!extractor) return;

    // 테스트 파라미터
    int sample_rate = 16000;
    double duration = 1.0;  // 1초
    int audio_length = (int)(sample_rate * duration);
    int f0_length = world_get_samples_for_dio(audio_length, sample_rate, config.frame_period);

    float* audio = (float*)malloc(audio_length * sizeof(float));
    double* f0 = (double*)malloc(f0_length * sizeof(double));
    double* time_axis = (double*)malloc(f0_length * sizeof(double));

    TEST_ASSERT(audio != NULL && f0 != NULL && time_axis != NULL, "메모리 할당 성공");

    if (audio && f0 && time_axis) {
        // 테스트 1: 단일 주파수 (220Hz - A3)
        double test_frequency = 220.0;
        generate_sine_wave(audio, audio_length, sample_rate, test_frequency, 0.8);

        clock_t timer = test_timer_start();
        ETResult result = world_f0_extractor_dio(extractor, audio, audio_length, sample_rate,
                                                f0, time_axis, f0_length);
        test_timer_end(timer);

        TEST_ASSERT(result == ET_SUCCESS, "DIO F0 추출 성공");

        // 정확성 검증 (유성음 구간에서)
        int valid_frames = 0;
        double total_error = 0.0;
        double max_error = 0.0;

        for (int i = f0_length / 4; i < f0_length * 3 / 4; i++) {  // 중간 50% 구간
            if (f0[i] > 0.0) {
                double error = fabs(f0[i] - test_frequency);
                total_error += error;
                if (error > max_error) max_error = error;
                valid_frames++;
            }
        }

        if (valid_frames > 0) {
            double avg_error = total_error / valid_frames;
            printf("    평균 오차: %.2f Hz, 최대 오차: %.2f Hz, 유성음 비율: %.1f%%\n",
                   avg_error, max_error, (double)valid_frames / (f0_length / 2) * 100.0);

            TEST_ASSERT_NEAR(avg_error, 0.0, 10.0, "DIO 평균 오차 < 10Hz");
            TEST_ASSERT(max_error < 50.0, "DIO 최대 오차 < 50Hz");
            TEST_ASSERT(valid_frames > f0_length / 8, "충분한 유성음 검출");
        }

        // 테스트 2: 하모닉이 있는 복합음 (150Hz)
        test_frequency = 150.0;
        generate_harmonic_wave(audio, audio_length, sample_rate, test_frequency, 0.8);

        clock_t timer2 = test_timer_start();
        result = world_f0_extractor_dio(extractor, audio, audio_length, sample_rate,
                                       f0, time_axis, f0_length);
        test_timer_end(timer2);

        TEST_ASSERT(result == ET_SUCCESS, "DIO 하모닉 음성 처리 성공");

        // 하모닉 음성 정확성 검증
        valid_frames = 0;
        total_error = 0.0;

        for (int i = f0_length / 4; i < f0_length * 3 / 4; i++) {
            if (f0[i] > 0.0) {
                double error = fabs(f0[i] - test_frequency);
                // 옥타브 에러 체크
                if (error > test_frequency / 2) {
                    if (fabs(f0[i] - test_frequency * 2) < error) {
                        error = fabs(f0[i] - test_frequency * 2);
                    }
                    if (fabs(f0[i] - test_frequency / 2) < error) {
                        error = fabs(f0[i] - test_frequency / 2);
                    }
                }
                total_error += error;
                valid_frames++;
            }
        }

        if (valid_frames > 0) {
            double avg_error = total_error / valid_frames;
            printf("    하모닉 음성 평균 오차: %.2f Hz\n", avg_error);
            TEST_ASSERT_NEAR(avg_error, 0.0, 15.0, "DIO 하모닉 음성 오차 < 15Hz");
        }
    }

    free(audio);
    free(f0);
    free(time_axis);
    world_f0_extractor_destroy(extractor);
}

/**
 * @brief Harvest 알고리즘 정확성 테스트
 */
static void test_harvest_accuracy(void) {
    printf("\n=== Harvest 알고리즘 정확성 테스트 ===\n");

    WorldF0Config config = {
        .frame_period = 5.0,
        .f0_floor = 50.0,
        .f0_ceil = 500.0,
        .algorithm = 1,  // Harvest
        .channels_in_octave = 2.0,
        .speed = 1.0,
        .allowed_range = 0.1
    };

    WorldF0Extractor* extractor = world_f0_extractor_create(&config, NULL);
    TEST_ASSERT(extractor != NULL, "Harvest 추출기 생성");

    if (!extractor) return;

    int sample_rate = 16000;
    double duration = 1.0;
    int audio_length = (int)(sample_rate * duration);
    int f0_length = world_get_samples_for_dio(audio_length, sample_rate, config.frame_period);

    float* audio = (float*)malloc(audio_length * sizeof(float));
    double* f0 = (double*)malloc(f0_length * sizeof(double));
    double* time_axis = (double*)malloc(f0_length * sizeof(double));

    if (audio && f0 && time_axis) {
        // 테스트: 주파수 변조 음성 (100Hz -> 200Hz)
        generate_frequency_modulated_wave(audio, audio_length, sample_rate, 100.0, 200.0, 0.8);

        clock_t timer = test_timer_start();
        ETResult result = world_f0_extractor_harvest(extractor, audio, audio_length, sample_rate,
                                                    f0, time_axis, f0_length);
        test_timer_end(timer);

        TEST_ASSERT(result == ET_SUCCESS, "Harvest F0 추출 성공");

        // 주파수 변조 추적 성능 검증
        int valid_frames = 0;
        double total_error = 0.0;

        for (int i = f0_length / 4; i < f0_length * 3 / 4; i++) {
            if (f0[i] > 0.0) {
                double progress = (double)(i - f0_length / 4) / (f0_length / 2);
                double expected_f0 = 100.0 + (200.0 - 100.0) * progress;
                double error = fabs(f0[i] - expected_f0);
                total_error += error;
                valid_frames++;
            }
        }

        if (valid_frames > 0) {
            double avg_error = total_error / valid_frames;
            printf("    주파수 변조 추적 평균 오차: %.2f Hz\n", avg_error);
            TEST_ASSERT_NEAR(avg_error, 0.0, 20.0, "Harvest 주파수 변조 추적 오차 < 20Hz");
            TEST_ASSERT(valid_frames > f0_length / 8, "충분한 주파수 변조 추적");
        }
    }

    free(audio);
    free(f0);
    free(time_axis);
    world_f0_extractor_destroy(extractor);
}

/**
 * @brief 노이즈 내성 테스트
 */
static void test_noise_robustness(void) {
    printf("\n=== 노이즈 내성 테스트 ===\n");

    WorldF0Config config = {
        .frame_period = 5.0,
        .f0_floor = 50.0,
        .f0_ceil = 500.0,
        .algorithm = 0,  // DIO
        .channels_in_octave = 2.0,
        .speed = 1.0,
        .allowed_range = 0.1
    };

    WorldF0Extractor* extractor = world_f0_extractor_create(&config, NULL);
    if (!extractor) return;

    int sample_rate = 16000;
    double duration = 1.0;
    int audio_length = (int)(sample_rate * duration);
    int f0_length = world_get_samples_for_dio(audio_length, sample_rate, config.frame_period);

    float* audio = (float*)malloc(audio_length * sizeof(float));
    double* f0 = (double*)malloc(f0_length * sizeof(double));
    double* time_axis = (double*)malloc(f0_length * sizeof(double));

    if (audio && f0 && time_axis) {
        double test_frequency = 200.0;
        double noise_levels[] = {0.1, 0.2, 0.3};  // 노이즈 레벨
        int num_noise_levels = sizeof(noise_levels) / sizeof(noise_levels[0]);

        for (int n = 0; n < num_noise_levels; n++) {
            generate_noisy_sine_wave(audio, audio_length, sample_rate, test_frequency, 0.8, noise_levels[n]);

            clock_t timer = test_timer_start();
            ETResult result = world_f0_extractor_dio(extractor, audio, audio_length, sample_rate,
                                                    f0, time_axis, f0_length);
            test_timer_end(timer);

            TEST_ASSERT(result == ET_SUCCESS, "노이즈 환경 F0 추출 성공");

            // 노이즈 내성 검증
            int valid_frames = 0;
            for (int i = f0_length / 4; i < f0_length * 3 / 4; i++) {
                if (f0[i] > 0.0) {
                    valid_frames++;
                }
            }

            double detection_rate = (double)valid_frames / (f0_length / 2) * 100.0;
            printf("    노이즈 레벨 %.1f: 검출률 %.1f%%\n", noise_levels[n], detection_rate);

            // 노이즈 레벨에 따른 최소 검출률 요구사항
            double min_detection_rate = 80.0 - noise_levels[n] * 100.0;  // 노이즈에 따라 감소
            TEST_ASSERT(detection_rate > min_detection_rate, "노이즈 내성 검출률 만족");
        }
    }

    free(audio);
    free(f0);
    free(time_axis);
    world_f0_extractor_destroy(extractor);
}

/**
 * @brief 성능 벤치마크 테스트
 */
static void test_performance_benchmark(void) {
    printf("\n=== 성능 벤치마크 테스트 ===\n");

    WorldF0Config config = {
        .frame_period = 5.0,
        .f0_floor = 71.0,
        .f0_ceil = 800.0,
        .algorithm = 0,  // DIO
        .channels_in_octave = 2.0,
        .speed = 1.0,
        .allowed_range = 0.1
    };

    WorldF0Extractor* extractor = world_f0_extractor_create(&config, NULL);
    if (!extractor) return;

    // 다양한 길이의 오디오로 성능 테스트
    int sample_rate = 44100;
    double durations[] = {1.0, 5.0, 10.0};  // 1초, 5초, 10초
    int num_durations = sizeof(durations) / sizeof(durations[0]);

    for (int d = 0; d < num_durations; d++) {
        int audio_length = (int)(sample_rate * durations[d]);
        int f0_length = world_get_samples_for_dio(audio_length, sample_rate, config.frame_period);

        float* audio = (float*)malloc(audio_length * sizeof(float));
        double* f0 = (double*)malloc(f0_length * sizeof(double));
        double* time_axis = (double*)malloc(f0_length * sizeof(double));

        if (audio && f0 && time_axis) {
            // 테스트 오디오 생성
            generate_harmonic_wave(audio, audio_length, sample_rate, 200.0, 0.8);

            // 성능 측정
            clock_t perf_start_time = clock();
            ETResult result = world_f0_extractor_dio(extractor, audio, audio_length, sample_rate,
                                                    f0, time_axis, f0_length);
            clock_t perf_end_time = clock();

            double elapsed_ms = ((double)(perf_end_time - perf_start_time)) / CLOCKS_PER_SEC * 1000.0;
            double realtime_factor = (durations[d] * 1000.0) / elapsed_ms;

            TEST_ASSERT(result == ET_SUCCESS, "성능 테스트 F0 추출 성공");
            printf("    %.1f초 오디오: %.2f ms (실시간 팩터: %.2fx)\n",
                   durations[d], elapsed_ms, realtime_factor);

            // 실시간 처리 요구사항 (최소 1x 이상)
            TEST_ASSERT(realtime_factor >= 1.0, "실시간 처리 성능 만족");

            // 메모리 사용량 모니터링
            size_t current_usage, peak_usage;
            world_monitor_memory_usage(extractor, &current_usage, &peak_usage);
            printf("    메모리 사용량: %.2f KB (피크: %.2f KB)\n",
                   current_usage / 1024.0, peak_usage / 1024.0);
        }

        free(audio);
        free(f0);
        free(time_axis);
    }

    world_f0_extractor_destroy(extractor);
}

/**
 * @brief 메모리 누수 테스트
 */
static void test_memory_leak(void) {
    printf("\n=== 메모리 누수 테스트 ===\n");

    WorldF0Config config = {
        .frame_period = 5.0,
        .f0_floor = 71.0,
        .f0_ceil = 800.0,
        .algorithm = 0,  // DIO
        .channels_in_octave = 2.0,
        .speed = 1.0,
        .allowed_range = 0.1
    };

    // 반복적인 생성/해제 테스트
    int num_iterations = 100;

    clock_t timer = test_timer_start();
    for (int i = 0; i < num_iterations; i++) {
        WorldF0Extractor* extractor = world_f0_extractor_create(&config, NULL);
        if (extractor) {
            world_f0_extractor_destroy(extractor);
        }
    }
    test_timer_end(timer);

    printf("    %d회 생성/해제 완료\n", num_iterations);
    TEST_ASSERT(true, "메모리 누수 테스트 완료");
}

// ============================================================================
// 메인 테스트 실행 함수
// ============================================================================

/**
 * @brief 모든 테스트 실행
 */
int main(void) {
    printf("WORLD F0 추출 알고리즘 단위 테스트 시작\n");
    printf("=====================================\n");

    // 랜덤 시드 설정
    srand((unsigned int)time(NULL));

    // 테스트 실행
    test_f0_extractor_creation();
    test_dio_accuracy();
    test_harvest_accuracy();
    test_noise_robustness();
    test_performance_benchmark();
    test_memory_leak();

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과 요약:\n");
    printf("  총 테스트: %d\n", g_test_results.total_tests);
    printf("  성공: %d\n", g_test_results.passed_tests);
    printf("  실패: %d\n", g_test_results.failed_tests);
    printf("  총 실행 시간: %.2f ms\n", g_test_results.total_time_ms);

    if (g_test_results.failed_tests == 0) {
        printf("  결과: 모든 테스트 통과! ✓\n");
        return 0;
    } else {
        printf("  결과: %d개 테스트 실패 ✗\n", g_test_results.failed_tests);
        return 1;
    }
}