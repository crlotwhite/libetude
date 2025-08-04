/**
 * @file test_aperiodicity_analysis.c
 * @brief WORLD 비주기성 분석 단위 테스트
 *
 * D4C 알고리즘의 정확성과 성능을 검증하는 테스트 스위트입니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

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

// 테스트 설정
#define TEST_SAMPLE_RATE 44100
#define TEST_AUDIO_LENGTH 4410  // 0.1초
#define TEST_F0_VALUE 220.0     // A3
#define TEST_FRAME_PERIOD 5.0   // 5ms
#define TOLERANCE 1e-6

// 테스트 결과 카운터
static int tests_passed = 0;
static int tests_failed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("✗ %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_NEAR(actual, expected, tolerance, message) \
    do { \
        if (fabs((actual) - (expected)) < (tolerance)) { \
            tests_passed++; \
            printf("✓ %s (%.6f ≈ %.6f)\n", message, actual, expected); \
        } else { \
            tests_failed++; \
            printf("✗ %s (%.6f ≠ %.6f, diff: %.6f)\n", message, actual, expected, fabs((actual) - (expected))); \
        } \
    } while(0)

/**
 * @brief 테스트용 합성 오디오 생성
 */
static void generate_test_audio(float* audio, int length, int sample_rate, double f0, double aperiodicity_level) {
    if (!audio) return;

    double phase = 0.0;
    double phase_increment = 2.0 * M_PI * f0 / sample_rate;

    for (int i = 0; i < length; i++) {
        // 기본 사인파
        double periodic_component = sin(phase);

        // 비주기성 성분 (노이즈)
        double noise = ((double)rand() / RAND_MAX - 0.5) * 2.0;
        double aperiodic_component = noise * aperiodicity_level;

        // 합성
        audio[i] = (float)(periodic_component * (1.0 - aperiodicity_level) + aperiodic_component);

        phase += phase_increment;
        if (phase > 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
    }
}

/**
 * @brief 테스트용 복합 오디오 생성 (하모닉 + 노이즈)
 */
static void generate_complex_test_audio(float* audio, int length, int sample_rate, double f0) {
    if (!audio) return;

    memset(audio, 0, length * sizeof(float));

    // 기본 주파수와 하모닉 성분 추가
    for (int harmonic = 1; harmonic <= 5; harmonic++) {
        double freq = f0 * harmonic;
        double amplitude = 1.0 / harmonic; // 하모닉일수록 작은 진폭
        double phase = 0.0;
        double phase_increment = 2.0 * M_PI * freq / sample_rate;

        for (int i = 0; i < length; i++) {
            audio[i] += (float)(amplitude * sin(phase));
            phase += phase_increment;
            if (phase > 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
        }
    }

    // 고주파수 노이즈 추가 (비주기성 시뮬레이션)
    for (int i = 0; i < length; i++) {
        double noise = ((double)rand() / RAND_MAX - 0.5) * 0.1;
        audio[i] += (float)noise;
    }
}

/**
 * @brief 비주기성 분석기 생성 테스트
 */
static void test_aperiodicity_analyzer_creation(void) {
    printf("\n=== 비주기성 분석기 생성 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    // 정상적인 생성
    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "비주기성 분석기 생성 성공");

    if (analyzer) {
        TEST_ASSERT(analyzer->config.threshold == 0.85, "설정값 올바르게 저장됨");
        TEST_ASSERT(!analyzer->is_initialized, "초기 상태는 미초기화");

        world_aperiodicity_analyzer_destroy(analyzer);
        printf("✓ 비주기성 분석기 해제 완료\n");
    }

    // NULL 설정으로 생성 시도
    WorldAperiodicityAnalyzer* null_analyzer = world_aperiodicity_analyzer_create(NULL, NULL);
    TEST_ASSERT(null_analyzer == NULL, "NULL 설정으로 생성 시 NULL 반환");
}

/**
 * @brief 비주기성 분석기 초기화 테스트
 */
static void test_aperiodicity_analyzer_initialization(void) {
    printf("\n=== 비주기성 분석기 초기화 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        // 정상적인 초기화
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");
        TEST_ASSERT(analyzer->is_initialized, "초기화 상태 플래그 설정됨");
        TEST_ASSERT(analyzer->last_sample_rate == TEST_SAMPLE_RATE, "샘플링 레이트 저장됨");
        TEST_ASSERT(analyzer->fft_size > 0, "FFT 크기 자동 계산됨");
        TEST_ASSERT(analyzer->spectrum_length == analyzer->fft_size / 2 + 1, "스펙트럼 길이 올바름");
        TEST_ASSERT(analyzer->num_bands == 5, "대역 수 올바름");

        // 버퍼 할당 확인
        TEST_ASSERT(analyzer->window_buffer != NULL, "윈도우 버퍼 할당됨");
        TEST_ASSERT(analyzer->fft_input_buffer != NULL, "FFT 입력 버퍼 할당됨");
        TEST_ASSERT(analyzer->magnitude_buffer != NULL, "크기 스펙트럼 버퍼 할당됨");
        TEST_ASSERT(analyzer->static_group_delay != NULL, "정적 그룹 지연 버퍼 할당됨");
        TEST_ASSERT(analyzer->band_aperiodicity != NULL, "대역별 비주기성 버퍼 할당됨");

        // 잘못된 파라미터로 초기화 시도
        ETResult invalid_result = world_aperiodicity_analyzer_initialize(analyzer, -1, 0);
        TEST_ASSERT(invalid_result != ET_SUCCESS, "잘못된 샘플링 레이트로 초기화 실패");

        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 그룹 지연 계산 테스트
 */
static void test_group_delay_computation(void) {
    printf("\n=== 그룹 지연 계산 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 1024);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");

        // 테스트용 위상 스펙트럼 생성 (선형 위상)
        int spectrum_length = analyzer->spectrum_length;
        double* magnitude = analyzer->magnitude_buffer;
        double* phase = analyzer->phase_buffer;
        double* group_delay = analyzer->static_group_delay;

        // 선형 위상 생성 (일정한 그룹 지연을 가져야 함)
        double expected_delay = 10.0; // 샘플
        for (int i = 0; i < spectrum_length; i++) {
            magnitude[i] = 1.0; // 균일한 크기
            double freq = (double)i / (spectrum_length - 1) * M_PI;
            phase[i] = -expected_delay * freq; // 선형 위상
        }

        // 그룹 지연 계산
        result = world_aperiodicity_analyzer_compute_static_group_delay(analyzer,
                                                                       magnitude, phase,
                                                                       spectrum_length,
                                                                       group_delay);
        TEST_ASSERT(result == ET_SUCCESS, "그룹 지연 계산 성공");

        // 중간 주파수에서 그룹 지연 확인 (경계 효과 제외)
        double avg_delay = 0.0;
        int count = 0;
        for (int i = spectrum_length / 4; i < 3 * spectrum_length / 4; i++) {
            avg_delay += group_delay[i];
            count++;
        }
        avg_delay /= count;

        TEST_ASSERT_NEAR(avg_delay, expected_delay, 2.0, "선형 위상에서 일정한 그룹 지연");

        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 단일 프레임 비주기성 분석 테스트
 */
static void test_single_frame_aperiodicity_analysis(void) {
    printf("\n=== 단일 프레임 비주기성 분석 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");

        // 테스트 오디오 생성
        float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
        double* aperiodicity = (double*)malloc(analyzer->spectrum_length * sizeof(double));

        if (audio && aperiodicity) {
            // 순수한 사인파 (낮은 비주기성 예상)
            generate_test_audio(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE, 0.0);

            result = world_aperiodicity_analyzer_extract_frame(analyzer,
                                                              audio, TEST_AUDIO_LENGTH,
                                                              TEST_AUDIO_LENGTH / 2, TEST_F0_VALUE,
                                                              TEST_SAMPLE_RATE, aperiodicity);
            TEST_ASSERT(result == ET_SUCCESS, "순수 사인파 비주기성 분석 성공");

            // 낮은 주파수에서 낮은 비주기성 확인
            double low_freq_aperiodicity = 0.0;
            int low_freq_bins = analyzer->spectrum_length / 8;
            for (int i = 1; i < low_freq_bins; i++) {
                low_freq_aperiodicity += aperiodicity[i];
            }
            low_freq_aperiodicity /= (low_freq_bins - 1);

            TEST_ASSERT(low_freq_aperiodicity < 0.5, "순수 사인파에서 낮은 비주기성");
            printf("  순수 사인파 평균 비주기성: %.3f\n", low_freq_aperiodicity);

            // 노이즈가 많은 신호 (높은 비주기성 예상)
            generate_test_audio(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE, 0.8);

            result = world_aperiodicity_analyzer_extract_frame(analyzer,
                                                              audio, TEST_AUDIO_LENGTH,
                                                              TEST_AUDIO_LENGTH / 2, TEST_F0_VALUE,
                                                              TEST_SAMPLE_RATE, aperiodicity);
            TEST_ASSERT(result == ET_SUCCESS, "노이즈 신호 비주기성 분석 성공");

            double noisy_aperiodicity = 0.0;
            for (int i = 1; i < low_freq_bins; i++) {
                noisy_aperiodicity += aperiodicity[i];
            }
            noisy_aperiodicity /= (low_freq_bins - 1);

            TEST_ASSERT(noisy_aperiodicity > low_freq_aperiodicity, "노이즈 신호에서 더 높은 비주기성");
            printf("  노이즈 신호 평균 비주기성: %.3f\n", noisy_aperiodicity);
        }

        free(audio);
        free(aperiodicity);
        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 대역별 비주기성 분석 테스트
 */
static void test_band_aperiodicity_analysis(void) {
    printf("\n=== 대역별 비주기성 분석 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");

        float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
        double* band_aperiodicity = (double*)malloc(analyzer->num_bands * sizeof(double));

        if (audio && band_aperiodicity) {
            // 복합 하모닉 신호 생성
            generate_complex_test_audio(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE);

            result = world_aperiodicity_analyzer_extract_bands(analyzer,
                                                              audio, TEST_AUDIO_LENGTH,
                                                              TEST_AUDIO_LENGTH / 2, TEST_F0_VALUE,
                                                              TEST_SAMPLE_RATE, band_aperiodicity);
            TEST_ASSERT(result == ET_SUCCESS, "대역별 비주기성 분석 성공");

            // 대역별 결과 출력 및 검증
            printf("  대역별 비주기성:\n");
            for (int band = 0; band < analyzer->num_bands; band++) {
                printf("    대역 %d: %.3f\n", band, band_aperiodicity[band]);
                TEST_ASSERT(band_aperiodicity[band] >= 0.0 && band_aperiodicity[band] <= 1.0,
                           "비주기성 값이 유효 범위 내");
            }

            // 일반적으로 고주파수 대역에서 더 높은 비주기성 예상
            TEST_ASSERT(band_aperiodicity[analyzer->num_bands - 1] >= band_aperiodicity[0],
                       "고주파수 대역에서 더 높은 비주기성");
        }

        free(audio);
        free(band_aperiodicity);
        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief D4C 알고리즘 전체 테스트
 */
static void test_d4c_algorithm(void) {
    printf("\n=== D4C 알고리즘 전체 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");

        // 테스트 파라미터 설정
        int f0_length = 20; // 20 프레임
        double frame_period_sec = TEST_FRAME_PERIOD / 1000.0;

        float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
        double* f0 = (double*)malloc(f0_length * sizeof(double));
        double* time_axis = (double*)malloc(f0_length * sizeof(double));
        double** aperiodicity = (double**)malloc(f0_length * sizeof(double*));

        if (audio && f0 && time_axis && aperiodicity) {
            // 메모리 할당
            for (int i = 0; i < f0_length; i++) {
                aperiodicity[i] = (double*)malloc(analyzer->spectrum_length * sizeof(double));
            }

            // 테스트 데이터 생성
            generate_complex_test_audio(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE);

            for (int i = 0; i < f0_length; i++) {
                f0[i] = TEST_F0_VALUE; // 일정한 F0
                time_axis[i] = i * frame_period_sec;
            }

            // D4C 알고리즘 실행
            result = world_aperiodicity_analyzer_d4c(analyzer,
                                                    audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE,
                                                    f0, time_axis, f0_length,
                                                    aperiodicity);
            TEST_ASSERT(result == ET_SUCCESS, "D4C 알고리즘 실행 성공");

            // 결과 검증
            bool all_valid = true;
            double avg_aperiodicity = 0.0;
            int valid_count = 0;

            for (int frame = 0; frame < f0_length; frame++) {
                for (int freq = 0; freq < analyzer->spectrum_length; freq++) {
                    if (aperiodicity[frame][freq] < 0.0 || aperiodicity[frame][freq] > 1.0) {
                        all_valid = false;
                        break;
                    }
                    avg_aperiodicity += aperiodicity[frame][freq];
                    valid_count++;
                }
                if (!all_valid) break;
            }

            TEST_ASSERT(all_valid, "모든 비주기성 값이 유효 범위 내");

            if (valid_count > 0) {
                avg_aperiodicity /= valid_count;
                printf("  전체 평균 비주기성: %.3f\n", avg_aperiodicity);
                TEST_ASSERT(avg_aperiodicity > 0.0 && avg_aperiodicity < 1.0, "평균 비주기성이 합리적 범위");
            }

            // 메모리 해제
            for (int i = 0; i < f0_length; i++) {
                free(aperiodicity[i]);
            }
        }

        free(audio);
        free(f0);
        free(time_axis);
        free(aperiodicity);
        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 최적화된 비주기성 분석 테스트
 */
static void test_optimized_aperiodicity_analysis(void) {
    printf("\n=== 최적화된 비주기성 분석 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");

        float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
        double* aperiodicity_normal = (double*)malloc(analyzer->spectrum_length * sizeof(double));
        double* aperiodicity_optimized = (double*)malloc(analyzer->spectrum_length * sizeof(double));

        if (audio && aperiodicity_normal && aperiodicity_optimized) {
            generate_complex_test_audio(audio, TEST_AUDIO_LENGTH, TEST_SAMPLE_RATE, TEST_F0_VALUE);

            // 일반 버전
            result = world_aperiodicity_analyzer_extract_frame(analyzer,
                                                              audio, TEST_AUDIO_LENGTH,
                                                              TEST_AUDIO_LENGTH / 2, TEST_F0_VALUE,
                                                              TEST_SAMPLE_RATE, aperiodicity_normal);
            TEST_ASSERT(result == ET_SUCCESS, "일반 비주기성 분석 성공");

            // 최적화 버전
            result = world_aperiodicity_analyzer_extract_frame_optimized(analyzer,
                                                                        audio, TEST_AUDIO_LENGTH,
                                                                        TEST_AUDIO_LENGTH / 2, TEST_F0_VALUE,
                                                                        TEST_SAMPLE_RATE, aperiodicity_optimized);
            TEST_ASSERT(result == ET_SUCCESS, "최적화된 비주기성 분석 성공");

            // 결과 비교
            double max_diff = 0.0;
            double avg_diff = 0.0;
            for (int i = 0; i < analyzer->spectrum_length; i++) {
                double diff = fabs(aperiodicity_normal[i] - aperiodicity_optimized[i]);
                if (diff > max_diff) max_diff = diff;
                avg_diff += diff;
            }
            avg_diff /= analyzer->spectrum_length;

            printf("  최대 차이: %.6f, 평균 차이: %.6f\n", max_diff, avg_diff);
            TEST_ASSERT(max_diff < 0.1, "최적화 버전과 일반 버전의 차이가 허용 범위 내");
        }

        free(audio);
        free(aperiodicity_normal);
        free(aperiodicity_optimized);
        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 성능 통계 테스트
 */
static void test_performance_stats(void) {
    printf("\n=== 성능 통계 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        ETResult result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");

        size_t memory_usage = 0;
        double processing_time = 0.0;
        int simd_capability = 0;

        result = world_aperiodicity_analyzer_get_performance_stats(analyzer,
                                                                  &memory_usage,
                                                                  &processing_time,
                                                                  &simd_capability);
        TEST_ASSERT(result == ET_SUCCESS, "성능 통계 조회 성공");
        TEST_ASSERT(memory_usage > 0, "메모리 사용량이 0보다 큼");

        printf("  메모리 사용량: %zu 바이트 (%.2f KB)\n", memory_usage, memory_usage / 1024.0);
        printf("  SIMD 기능: 0x%02X", simd_capability);

        if (simd_capability & 0x02) printf(" SSE2");
        if (simd_capability & 0x04) printf(" AVX");
        if (simd_capability & 0x08) printf(" NEON");
        printf("\n");

        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 에러 처리 테스트
 */
static void test_error_handling(void) {
    printf("\n=== 에러 처리 테스트 ===\n");

    WorldAperiodicityConfig config = {0};
    config.threshold = 0.85;

    WorldAperiodicityAnalyzer* analyzer = world_aperiodicity_analyzer_create(&config, NULL);
    TEST_ASSERT(analyzer != NULL, "분석기 생성 성공");

    if (analyzer) {
        double* dummy_buffer = (double*)malloc(100 * sizeof(double));

        // 초기화 전 사용 시도
        ETResult result = world_aperiodicity_analyzer_extract_frame(analyzer,
                                                                   NULL, 0, 0, 0.0, 0, dummy_buffer);
        TEST_ASSERT(result != ET_SUCCESS, "초기화 전 사용 시 오류 반환");

        // 초기화
        result = world_aperiodicity_analyzer_initialize(analyzer, TEST_SAMPLE_RATE, 0);
        TEST_ASSERT(result == ET_SUCCESS, "분석기 초기화 성공");

        // NULL 포인터 테스트
        result = world_aperiodicity_analyzer_extract_frame(NULL, NULL, 0, 0, 0.0, 0, dummy_buffer);
        TEST_ASSERT(result != ET_SUCCESS, "NULL 분석기로 호출 시 오류 반환");

        result = world_aperiodicity_analyzer_extract_frame(analyzer, NULL, 0, 0, 0.0, 0, dummy_buffer);
        TEST_ASSERT(result != ET_SUCCESS, "NULL 오디오로 호출 시 오류 반환");

        result = world_aperiodicity_analyzer_extract_frame(analyzer, (float*)dummy_buffer, 100, 0, 0.0, TEST_SAMPLE_RATE, NULL);
        TEST_ASSERT(result != ET_SUCCESS, "NULL 출력 버퍼로 호출 시 오류 반환");

        // 잘못된 F0 값
        result = world_aperiodicity_analyzer_extract_frame(analyzer, (float*)dummy_buffer, 100, 50, -1.0, TEST_SAMPLE_RATE, dummy_buffer);
        TEST_ASSERT(result != ET_SUCCESS, "음수 F0로 호출 시 오류 반환");

        free(dummy_buffer);
        world_aperiodicity_analyzer_destroy(analyzer);
    }
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("WORLD 비주기성 분석 단위 테스트 시작\n");
    printf("=====================================\n");

    // libetude 초기화
    ETResult init_result = et_initialize();
    if (init_result != ET_SUCCESS) {
        printf("libetude 초기화 실패: %d\n", init_result);
        return 1;
    }

    // 테스트 실행
    test_aperiodicity_analyzer_creation();
    test_aperiodicity_analyzer_initialization();
    test_group_delay_computation();
    test_single_frame_aperiodicity_analysis();
    test_band_aperiodicity_analysis();
    test_d4c_algorithm();
    test_optimized_aperiodicity_analysis();
    test_performance_stats();
    test_error_handling();

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과: %d개 성공, %d개 실패\n", tests_passed, tests_failed);

    if (tests_failed == 0) {
        printf("✓ 모든 테스트 통과!\n");
    } else {
        printf("✗ %d개 테스트 실패\n", tests_failed);
    }

    // libetude 정리
    et_finalize();

    return (tests_failed == 0) ? 0 : 1;
}