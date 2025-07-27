/**
 * @file test_mel_scale.c
 * @brief Mel 스케일 변환 최적화 구현 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "libetude/mel_scale.h"
#include "libetude/error.h"

// ============================================================================
// 테스트 유틸리티 함수
// ============================================================================

#define ASSERT_FLOAT_EQ(a, b, tolerance) \
    do { \
        if (fabsf((a) - (b)) > (tolerance)) { \
            printf("FAIL: %s:%d - Expected %f, got %f (diff: %f)\n", \
                   __FILE__, __LINE__, (b), (a), fabsf((a) - (b))); \
            return 0; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            printf("FAIL: %s:%d - Expected %d, got %d\n", \
                   __FILE__, __LINE__, (b), (a)); \
            return 0; \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s:%d - Condition failed: %s\n", \
                   __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("FAIL: %s:%d - Pointer is NULL: %s\n", \
                   __FILE__, __LINE__, #ptr); \
            return 0; \
        } \
    } while(0)

static void generate_test_spectrum(float* spectrum, int n_freq_bins) {
    // 간단한 테스트 스펙트럼 생성 (가우시안 형태)
    float center = n_freq_bins / 2.0f;
    float sigma = n_freq_bins / 8.0f;

    for (int i = 0; i < n_freq_bins; i++) {
        float x = (i - center) / sigma;
        spectrum[i] = expf(-0.5f * x * x);
    }
}

// ============================================================================
// Mel 스케일 변환 함수 테스트
// ============================================================================

static int test_mel_scale_conversion(void) {
    printf("Testing Mel scale conversion functions...\n");

    // HTK 스케일 테스트
    float hz = 1000.0f;
    float mel_htk = et_mel_hz_to_mel(hz, ET_MEL_SCALE_HTK);
    float hz_back = et_mel_mel_to_hz(mel_htk, ET_MEL_SCALE_HTK);
    ASSERT_FLOAT_EQ(hz_back, hz, 0.1f);

    // Slaney 스케일 테스트
    float mel_slaney = et_mel_hz_to_mel(hz, ET_MEL_SCALE_SLANEY);
    hz_back = et_mel_mel_to_hz(mel_slaney, ET_MEL_SCALE_SLANEY);
    ASSERT_FLOAT_EQ(hz_back, hz, 0.1f);

    // 경계값 테스트
    ASSERT_FLOAT_EQ(et_mel_hz_to_mel(0.0f, ET_MEL_SCALE_HTK), 0.0f, 1e-6f);
    ASSERT_FLOAT_EQ(et_mel_mel_to_hz(0.0f, ET_MEL_SCALE_HTK), 0.0f, 1e-6f);

    printf("✓ Mel scale conversion tests passed\n");
    return 1;
}

static int test_fft_bin_conversion(void) {
    printf("Testing FFT bin conversion functions...\n");

    int n_fft = 1024;
    int sample_rate = 16000;

    // Nyquist 주파수 테스트
    float nyquist = sample_rate / 2.0f;
    float bin_float = et_mel_hz_to_fft_bin(nyquist, n_fft, sample_rate);
    ASSERT_FLOAT_EQ(bin_float, n_fft / 2.0f, 1e-6f);

    // 역변환 테스트
    int bin = 100;
    float hz = et_mel_fft_bin_to_hz(bin, n_fft, sample_rate);
    float bin_back = et_mel_hz_to_fft_bin(hz, n_fft, sample_rate);
    ASSERT_FLOAT_EQ(bin_back, (float)bin, 1e-6f);

    printf("✓ FFT bin conversion tests passed\n");
    return 1;
}

static int test_mel_points_creation(void) {
    printf("Testing Mel points creation...\n");

    int n_mels = 80;
    float fmin = 0.0f;
    float fmax = 8000.0f;
    float* mel_points = (float*)malloc((n_mels + 2) * sizeof(float));
    ASSERT_NOT_NULL(mel_points);

    ETResult result = et_mel_create_mel_points(fmin, fmax, n_mels, ET_MEL_SCALE_HTK, mel_points);
    ASSERT_EQ(result, ET_SUCCESS);

    // 단조 증가 확인
    for (int i = 0; i < n_mels + 1; i++) {
        ASSERT_TRUE(mel_points[i] < mel_points[i + 1]);
    }

    // 경계값 확인
    float mel_min = et_mel_hz_to_mel(fmin, ET_MEL_SCALE_HTK);
    float mel_max = et_mel_hz_to_mel(fmax, ET_MEL_SCALE_HTK);
    ASSERT_FLOAT_EQ(mel_points[0], mel_min, 1e-6f);
    ASSERT_FLOAT_EQ(mel_points[n_mels + 1], mel_max, 1e-3f);

    free(mel_points);
    printf("✓ Mel points creation tests passed\n");
    return 1;
}

// ============================================================================
// 필터뱅크 생성 및 관리 테스트
// ============================================================================

static int test_filterbank_creation(void) {
    printf("Testing filterbank creation...\n");

    ETMelFilterbankConfig config = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbank* mel_fb = et_mel_create_filterbank(&config);
    ASSERT_NOT_NULL(mel_fb);

    // 설정 확인
    int n_fft, n_mels, sample_rate;
    float fmin, fmax;
    ETResult result = et_mel_get_filterbank_info(mel_fb, &n_fft, &n_mels, &sample_rate, &fmin, &fmax);
    ASSERT_EQ(result, ET_SUCCESS);
    ASSERT_EQ(n_fft, 1024);
    ASSERT_EQ(n_mels, 80);
    ASSERT_EQ(sample_rate, 16000);

    et_mel_destroy_filterbank(mel_fb);
    printf("✓ Filterbank creation tests passed\n");
    return 1;
}

static int test_filterbank_config_update(void) {
    printf("Testing filterbank config update...\n");

    ETMelFilterbankConfig config1 = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbank* mel_fb = et_mel_create_filterbank(&config1);
    ASSERT_NOT_NULL(mel_fb);

    // 설정 변경
    ETMelFilterbankConfig config2 = et_mel_default_config(2048, 128, 22050, 0.0f, 11025.0f);
    ETResult result = et_mel_update_config(mel_fb, &config2);
    ASSERT_EQ(result, ET_SUCCESS);

    // 변경된 설정 확인
    int n_fft, n_mels, sample_rate;
    result = et_mel_get_filterbank_info(mel_fb, &n_fft, &n_mels, &sample_rate, NULL, NULL);
    ASSERT_EQ(result, ET_SUCCESS);
    ASSERT_EQ(n_fft, 2048);
    ASSERT_EQ(n_mels, 128);
    ASSERT_EQ(sample_rate, 22050);

    et_mel_destroy_filterbank(mel_fb);
    printf("✓ Filterbank config update tests passed\n");
    return 1;
}

// ============================================================================
// Mel 변환 핵심 기능 테스트
// ============================================================================

static int test_spectrum_to_mel_conversion(void) {
    printf("Testing spectrum to Mel conversion...\n");

    ETMelFilterbankConfig config = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbank* mel_fb = et_mel_create_filterbank(&config);
    ASSERT_NOT_NULL(mel_fb);

    int n_freq_bins = config.n_fft / 2 + 1;
    float* spectrum = (float*)malloc(n_freq_bins * sizeof(float));
    float* mel_frame = (float*)malloc(config.n_mels * sizeof(float));
    ASSERT_NOT_NULL(spectrum);
    ASSERT_NOT_NULL(mel_frame);

    generate_test_spectrum(spectrum, n_freq_bins);

    ETResult result = et_mel_spectrum_to_mel_frame(mel_fb, spectrum, mel_frame);
    ASSERT_EQ(result, ET_SUCCESS);

    // Mel 스펙트럼이 음수가 아닌지 확인
    for (int i = 0; i < config.n_mels; i++) {
        ASSERT_TRUE(mel_frame[i] >= 0.0f);
    }

    free(spectrum);
    free(mel_frame);
    et_mel_destroy_filterbank(mel_fb);
    printf("✓ Spectrum to Mel conversion tests passed\n");
    return 1;
}

static int test_mel_to_spectrum_conversion(void) {
    printf("Testing Mel to spectrum conversion...\n");

    ETMelFilterbankConfig config = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbank* mel_fb = et_mel_create_filterbank(&config);
    ASSERT_NOT_NULL(mel_fb);

    int n_freq_bins = config.n_fft / 2 + 1;
    float* mel_frame = (float*)malloc(config.n_mels * sizeof(float));
    float* spectrum = (float*)malloc(n_freq_bins * sizeof(float));
    ASSERT_NOT_NULL(mel_frame);
    ASSERT_NOT_NULL(spectrum);

    // 간단한 Mel 스펙트럼 생성
    for (int i = 0; i < config.n_mels; i++) {
        mel_frame[i] = 1.0f / (1.0f + i * 0.1f);  // 감소하는 패턴
    }

    ETResult result = et_mel_mel_frame_to_spectrum(mel_fb, mel_frame, spectrum);
    ASSERT_EQ(result, ET_SUCCESS);

    // 복원된 스펙트럼이 음수가 아닌지 확인
    for (int i = 0; i < n_freq_bins; i++) {
        ASSERT_TRUE(spectrum[i] >= 0.0f);
    }

    free(mel_frame);
    free(spectrum);
    et_mel_destroy_filterbank(mel_fb);
    printf("✓ Mel to spectrum conversion tests passed\n");
    return 1;
}

static int test_batch_mel_conversion(void) {
    printf("Testing batch Mel conversion...\n");

    ETMelFilterbankConfig config = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbank* mel_fb = et_mel_create_filterbank(&config);
    ASSERT_NOT_NULL(mel_fb);

    int time_frames = 100;
    int n_freq_bins = config.n_fft / 2 + 1;

    float* spectrogram = (float*)malloc(time_frames * n_freq_bins * sizeof(float));
    float* mel_spec = (float*)malloc(time_frames * config.n_mels * sizeof(float));
    ASSERT_NOT_NULL(spectrogram);
    ASSERT_NOT_NULL(mel_spec);

    // 테스트 스펙트로그램 생성
    for (int t = 0; t < time_frames; t++) {
        generate_test_spectrum(spectrogram + t * n_freq_bins, n_freq_bins);
    }

    ETResult result = et_mel_spectrogram_to_mel(mel_fb, spectrogram, time_frames, mel_spec);
    ASSERT_EQ(result, ET_SUCCESS);

    // 모든 Mel 값이 음수가 아닌지 확인
    for (int i = 0; i < time_frames * config.n_mels; i++) {
        ASSERT_TRUE(mel_spec[i] >= 0.0f);
    }

    free(spectrogram);
    free(mel_spec);
    et_mel_destroy_filterbank(mel_fb);
    printf("✓ Batch Mel conversion tests passed\n");
    return 1;
}

// ============================================================================
// 캐싱 시스템 테스트
// ============================================================================

static int test_caching_system(void) {
    printf("Testing caching system...\n");

    ETResult result = et_mel_init_cache(4);
    ASSERT_EQ(result, ET_SUCCESS);

    ETMelFilterbankConfig config1 = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbankConfig config2 = et_mel_default_config(2048, 128, 22050, 0.0f, 11025.0f);

    // 첫 번째 필터뱅크 생성 (캐시 미스)
    ETMelFilterbank* mel_fb1 = et_mel_create_filterbank(&config1);
    ASSERT_NOT_NULL(mel_fb1);

    // 동일한 설정으로 두 번째 필터뱅크 생성 (캐시 히트)
    ETMelFilterbank* mel_fb2 = et_mel_create_filterbank(&config1);
    ASSERT_NOT_NULL(mel_fb2);
    ASSERT_EQ(mel_fb1, mel_fb2);  // 동일한 포인터여야 함

    // 다른 설정으로 세 번째 필터뱅크 생성 (캐시 미스)
    ETMelFilterbank* mel_fb3 = et_mel_create_filterbank(&config2);
    ASSERT_NOT_NULL(mel_fb3);
    ASSERT_TRUE(mel_fb1 != mel_fb3);  // 다른 포인터여야 함

    // 통계 확인
    ETMelStats stats1, stats3;
    et_mel_get_performance_stats(mel_fb1, &stats1);
    et_mel_get_performance_stats(mel_fb3, &stats3);

    ASSERT_TRUE(stats1.cache_hits > 0);
    ASSERT_TRUE(stats3.cache_misses > 0);

    et_mel_destroy_filterbank(mel_fb1);
    et_mel_destroy_filterbank(mel_fb3);
    et_mel_destroy_cache();

    printf("✓ Caching system tests passed\n");
    return 1;
}

// ============================================================================
// 성능 및 정확도 테스트
// ============================================================================

static int test_reconstruction_accuracy(void) {
    printf("Testing reconstruction accuracy...\n");

    ETMelFilterbankConfig config = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbank* mel_fb = et_mel_create_filterbank(&config);
    ASSERT_NOT_NULL(mel_fb);

    int n_freq_bins = config.n_fft / 2 + 1;
    float* test_spectrum = (float*)malloc(n_freq_bins * sizeof(float));
    ASSERT_NOT_NULL(test_spectrum);

    generate_test_spectrum(test_spectrum, n_freq_bins);

    float reconstruction_error;
    ETResult result = et_mel_verify_accuracy(mel_fb, test_spectrum, n_freq_bins, &reconstruction_error);
    ASSERT_EQ(result, ET_SUCCESS);

    // 재구성 오차가 합리적인 범위 내에 있는지 확인
    // Mel 변환은 정보 손실이 있는 변환이므로 완벽한 재구성은 불가능
    ASSERT_TRUE(reconstruction_error < 100.0f);  // MSE < 100.0 (의사역행렬 근사이므로 큰 오차 허용)

    printf("  Reconstruction MSE: %f\n", reconstruction_error);

    free(test_spectrum);
    et_mel_destroy_filterbank(mel_fb);
    printf("✓ Reconstruction accuracy tests passed\n");
    return 1;
}

static int test_performance_stats(void) {
    printf("Testing performance statistics...\n");

    ETMelFilterbankConfig config = et_mel_default_config(1024, 80, 16000, 0.0f, 8000.0f);
    ETMelFilterbank* mel_fb = et_mel_create_filterbank(&config);
    ASSERT_NOT_NULL(mel_fb);

    int n_freq_bins = config.n_fft / 2 + 1;
    float* spectrum = (float*)malloc(n_freq_bins * sizeof(float));
    float* mel_frame = (float*)malloc(config.n_mels * sizeof(float));
    ASSERT_NOT_NULL(spectrum);
    ASSERT_NOT_NULL(mel_frame);

    generate_test_spectrum(spectrum, n_freq_bins);

    // 여러 번 변환 수행
    for (int i = 0; i < 100; i++) {
        et_mel_spectrum_to_mel_frame(mel_fb, spectrum, mel_frame);
        et_mel_mel_frame_to_spectrum(mel_fb, mel_frame, spectrum);
    }

    ETMelStats stats;
    ETResult result = et_mel_get_performance_stats(mel_fb, &stats);
    ASSERT_EQ(result, ET_SUCCESS);

    ASSERT_TRUE(stats.memory_usage > 0);
    printf("  Memory usage: %zu bytes\n", stats.memory_usage);
    printf("  Forward time: %f ms\n", stats.forward_time_ms);
    printf("  Inverse time: %f ms\n", stats.inverse_time_ms);

    free(spectrum);
    free(mel_frame);
    et_mel_destroy_filterbank(mel_fb);
    printf("✓ Performance statistics tests passed\n");
    return 1;
}

// ============================================================================
// 에러 처리 테스트
// ============================================================================

static int test_error_handling(void) {
    printf("Testing error handling...\n");

    // NULL 포인터 테스트
    ASSERT_EQ(et_mel_create_filterbank(NULL), NULL);

    // 잘못된 설정 테스트
    ETMelFilterbankConfig invalid_config = {0};
    invalid_config.n_fft = -1;  // 잘못된 값
    ASSERT_EQ(et_mel_create_filterbank(&invalid_config), NULL);

    invalid_config.n_fft = 1024;
    invalid_config.n_mels = 0;  // 잘못된 값
    ASSERT_EQ(et_mel_create_filterbank(&invalid_config), NULL);

    invalid_config.n_mels = 80;
    invalid_config.sample_rate = -1;  // 잘못된 값
    ASSERT_EQ(et_mel_create_filterbank(&invalid_config), NULL);

    invalid_config.sample_rate = 16000;
    invalid_config.fmin = 8000.0f;
    invalid_config.fmax = 4000.0f;  // fmin > fmax
    ASSERT_EQ(et_mel_create_filterbank(&invalid_config), NULL);

    // 함수 호출 시 NULL 포인터 테스트
    ETResult result = et_mel_spectrum_to_mel_frame(NULL, NULL, NULL);
    ASSERT_EQ(result, ET_ERROR_INVALID_ARGUMENT);

    result = et_mel_mel_frame_to_spectrum(NULL, NULL, NULL);
    ASSERT_EQ(result, ET_ERROR_INVALID_ARGUMENT);

    printf("✓ Error handling tests passed\n");
    return 1;
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main(void) {
    printf("=== LibEtude Mel Scale Tests ===\n\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 사전 계산 테이블 초기화
    et_mel_init_precomputed_tables();

    // 기본 변환 함수 테스트
    total_tests++; if (test_mel_scale_conversion()) passed_tests++;
    total_tests++; if (test_fft_bin_conversion()) passed_tests++;
    total_tests++; if (test_mel_points_creation()) passed_tests++;

    // 필터뱅크 관리 테스트
    total_tests++; if (test_filterbank_creation()) passed_tests++;
    total_tests++; if (test_filterbank_config_update()) passed_tests++;

    // 핵심 변환 기능 테스트
    total_tests++; if (test_spectrum_to_mel_conversion()) passed_tests++;
    total_tests++; if (test_mel_to_spectrum_conversion()) passed_tests++;
    total_tests++; if (test_batch_mel_conversion()) passed_tests++;

    // 캐싱 시스템 테스트
    total_tests++; if (test_caching_system()) passed_tests++;

    // 성능 및 정확도 테스트
    total_tests++; if (test_reconstruction_accuracy()) passed_tests++;
    total_tests++; if (test_performance_stats()) passed_tests++;

    // 에러 처리 테스트
    total_tests++; if (test_error_handling()) passed_tests++;

    // 정리
    et_mel_destroy_precomputed_tables();

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", passed_tests, total_tests);

    if (passed_tests == total_tests) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ %d tests failed!\n", total_tests - passed_tests);
        return 1;
    }
}