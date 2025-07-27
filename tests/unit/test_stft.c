/**
 * @file test_stft.c
 * @brief STFT/ISTFT 최적화 구현 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#include "libetude/stft.h"
#include "libetude/fast_math.h"
#include "libetude/error.h"

// 테스트 유틸리티
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

// 테스트 상수
#define TEST_SAMPLE_RATE 22050
#define TEST_FFT_SIZE 1024
#define TEST_HOP_SIZE 256
#define TEST_AUDIO_LENGTH 4096
#define TEST_TOLERANCE 1e-5f

// 테스트 신호 생성
static void generate_test_signal(float* signal, int length, float frequency, float sample_rate) {
    for (int i = 0; i < length; i++) {
        signal[i] = sinf(2.0f * M_PI * frequency * i / sample_rate);
    }
}

// 배열 비교 함수
static int compare_arrays(const float* a, const float* b, int size, float tolerance) {
    for (int i = 0; i < size; i++) {
        if (fabsf(a[i] - b[i]) > tolerance) {
            return 0;
        }
    }
    return 1;
}

// ============================================================================
// 기본 기능 테스트
// ============================================================================

static int test_stft_context_creation() {
    ETSTFTConfig config = et_stft_default_config(TEST_FFT_SIZE, TEST_HOP_SIZE, ET_WINDOW_HANN);

    ETSTFTContext* ctx = et_stft_create_context(&config);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    et_stft_destroy_context(ctx);
    TEST_PASS();
}

static int test_stft_invalid_parameters() {
    // NULL 설정으로 컨텍스트 생성 시도
    ETSTFTContext* ctx = et_stft_create_context(NULL);
    TEST_ASSERT(ctx == NULL, "Should fail with NULL config");

    // 잘못된 FFT 크기
    ETSTFTConfig config = et_stft_default_config(1000, TEST_HOP_SIZE, ET_WINDOW_HANN); // 2의 거듭제곱이 아님
    ctx = et_stft_create_context(&config);
    TEST_ASSERT(ctx == NULL, "Should fail with invalid FFT size");

    TEST_PASS();
}

static int test_window_functions() {
    int window_size = 512;
    float* window = (float*)malloc(window_size * sizeof(float));

    // Hann 윈도우 테스트
    ETResult result = et_stft_create_window(ET_WINDOW_HANN, window_size, window);
    TEST_ASSERT(result == ET_SUCCESS, "Hann window creation failed");
    TEST_ASSERT(window[0] == 0.0f, "Hann window should start with 0");
    TEST_ASSERT(window[window_size-1] == 0.0f, "Hann window should end with 0");
    TEST_ASSERT(window[window_size/2] > 0.9f, "Hann window peak should be near 1");

    // Hamming 윈도우 테스트
    result = et_stft_create_window(ET_WINDOW_HAMMING, window_size, window);
    TEST_ASSERT(result == ET_SUCCESS, "Hamming window creation failed");
    TEST_ASSERT(window[0] > 0.0f, "Hamming window should not start with 0");

    // 사각 윈도우 테스트
    result = et_stft_create_window(ET_WINDOW_RECTANGULAR, window_size, window);
    TEST_ASSERT(result == ET_SUCCESS, "Rectangular window creation failed");
    for (int i = 0; i < window_size; i++) {
        TEST_ASSERT(window[i] == 1.0f, "Rectangular window should be all 1s");
    }

    free(window);
    TEST_PASS();
}

static int test_stft_forward_basic() {
    ETSTFTConfig config = et_stft_default_config(TEST_FFT_SIZE, TEST_HOP_SIZE, ET_WINDOW_HANN);
    ETSTFTContext* ctx = et_stft_create_context(&config);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    // 테스트 신호 생성 (440Hz 사인파)
    float* audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    generate_test_signal(audio, TEST_AUDIO_LENGTH, 440.0f, TEST_SAMPLE_RATE);

    // STFT 수행
    int n_frames = et_stft_calculate_frames(TEST_AUDIO_LENGTH, TEST_FFT_SIZE, TEST_HOP_SIZE);
    int freq_bins = TEST_FFT_SIZE / 2 + 1;

    float* magnitude = (float*)malloc(n_frames * freq_bins * sizeof(float));
    float* phase = (float*)malloc(n_frames * freq_bins * sizeof(float));

    int actual_frames;
    ETResult result = et_stft_forward(ctx, audio, TEST_AUDIO_LENGTH, magnitude, phase, &actual_frames);

    TEST_ASSERT(result == ET_SUCCESS, "STFT forward failed");
    TEST_ASSERT(actual_frames == n_frames, "Frame count mismatch");

    // 440Hz 주변에서 피크 확인
    int expected_bin = (int)(440.0f * TEST_FFT_SIZE / TEST_SAMPLE_RATE);
    float max_magnitude = 0.0f;
    int max_bin = 0;

    for (int bin = expected_bin - 2; bin <= expected_bin + 2; bin++) {
        if (bin >= 0 && bin < freq_bins && magnitude[bin] > max_magnitude) {
            max_magnitude = magnitude[bin];
            max_bin = bin;
        }
    }

    TEST_ASSERT(abs(max_bin - expected_bin) <= 2, "Peak frequency detection failed");
    TEST_ASSERT(max_magnitude > 0.1f, "Peak magnitude too low");

    free(audio);
    free(magnitude);
    free(phase);
    et_stft_destroy_context(ctx);
    TEST_PASS();
}

static int test_stft_inverse_basic() {
    ETSTFTConfig config = et_stft_default_config(TEST_FFT_SIZE, TEST_HOP_SIZE, ET_WINDOW_HANN);
    ETSTFTContext* ctx = et_stft_create_context(&config);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    // 테스트 신호 생성
    float* original_audio = (float*)malloc(TEST_AUDIO_LENGTH * sizeof(float));
    generate_test_signal(original_audio, TEST_AUDIO_LENGTH, 440.0f, TEST_SAMPLE_RATE);

    // STFT 수행
    int n_frames = et_stft_calculate_frames(TEST_AUDIO_LENGTH, TEST_FFT_SIZE, TEST_HOP_SIZE);
    int freq_bins = TEST_FFT_SIZE / 2 + 1;

    float* magnitude = (float*)malloc(n_frames * freq_bins * sizeof(float));
    float* phase = (float*)malloc(n_frames * freq_bins * sizeof(float));

    int actual_frames;
    ETResult result = et_stft_forward(ctx, original_audio, TEST_AUDIO_LENGTH, magnitude, phase, &actual_frames);
    TEST_ASSERT(result == ET_SUCCESS, "STFT forward failed");

    // ISTFT 수행
    int reconstructed_length;
    float* reconstructed_audio = (float*)malloc(TEST_AUDIO_LENGTH * 2 * sizeof(float)); // 여유 공간

    result = et_stft_inverse(ctx, magnitude, phase, actual_frames, reconstructed_audio, &reconstructed_length);
    TEST_ASSERT(result == ET_SUCCESS, "STFT inverse failed");

    // 복원 품질 확인 (중앙 부분만 비교, 경계 효과 제외)
    int start_offset = TEST_FFT_SIZE / 2;
    int compare_length = TEST_AUDIO_LENGTH - TEST_FFT_SIZE;

    if (compare_length > 0) {
        int similar = compare_arrays(original_audio + start_offset,
                                   reconstructed_audio + start_offset,
                                   compare_length, 0.1f);
        TEST_ASSERT(similar, "Reconstruction quality too low");
    }

    free(original_audio);
    free(reconstructed_audio);
    free(magnitude);
    free(phase);
    et_stft_destroy_context(ctx);
    TEST_PASS();
}

// ============================================================================
// 실시간 처리 테스트
// ============================================================================

static int test_stft_streaming() {
    ETSTFTConfig config = et_stft_default_config(TEST_FFT_SIZE, TEST_HOP_SIZE, ET_WINDOW_HANN);
    config.mode = ET_STFT_MODE_REALTIME;

    ETSTFTContext* ctx = et_stft_create_context(&config);
    TEST_ASSERT(ctx != NULL, "Streaming context creation failed");

    // 청크 크기
    int chunk_size = TEST_HOP_SIZE;
    int freq_bins = TEST_FFT_SIZE / 2 + 1;

    float* audio_chunk = (float*)malloc(chunk_size * sizeof(float));
    float* magnitude = (float*)malloc(freq_bins * sizeof(float));
    float* phase = (float*)malloc(freq_bins * sizeof(float));
    float* output_chunk = (float*)malloc(chunk_size * sizeof(float));

    // 여러 청크 처리
    for (int chunk = 0; chunk < 10; chunk++) {
        // 테스트 청크 생성
        generate_test_signal(audio_chunk, chunk_size, 440.0f, TEST_SAMPLE_RATE);

        // 스트리밍 STFT
        ETResult result = et_stft_forward_streaming(ctx, audio_chunk, chunk_size, magnitude, phase);
        TEST_ASSERT(result == ET_SUCCESS, "Streaming STFT forward failed");

        // 스트리밍 ISTFT
        int output_size;
        result = et_stft_inverse_streaming(ctx, magnitude, phase, output_chunk, &output_size);
        TEST_ASSERT(result == ET_SUCCESS, "Streaming STFT inverse failed");
        TEST_ASSERT(output_size == chunk_size, "Output chunk size mismatch");
    }

    free(audio_chunk);
    free(magnitude);
    free(phase);
    free(output_chunk);
    et_stft_destroy_context(ctx);
    TEST_PASS();
}

// ============================================================================
// 성능 테스트
// ============================================================================

static int test_stft_performance() {
    ETSTFTConfig config = et_stft_default_config(TEST_FFT_SIZE, TEST_HOP_SIZE, ET_WINDOW_HANN);
    config.enable_simd = true;
    config.enable_parallel = true;

    ETSTFTContext* ctx = et_stft_create_context(&config);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    // 긴 테스트 신호 생성
    int long_audio_length = 44100; // 1초
    float* audio = (float*)malloc(long_audio_length * sizeof(float));
    generate_test_signal(audio, long_audio_length, 440.0f, TEST_SAMPLE_RATE);

    int n_frames = et_stft_calculate_frames(long_audio_length, TEST_FFT_SIZE, TEST_HOP_SIZE);
    int freq_bins = TEST_FFT_SIZE / 2 + 1;

    float* magnitude = (float*)malloc(n_frames * freq_bins * sizeof(float));
    float* phase = (float*)malloc(n_frames * freq_bins * sizeof(float));

    // 성능 측정
    clock_t start_time = clock();

    int actual_frames;
    ETResult result = et_stft_forward(ctx, audio, long_audio_length, magnitude, phase, &actual_frames);

    clock_t end_time = clock();
    double elapsed_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;

    TEST_ASSERT(result == ET_SUCCESS, "Performance test STFT failed");

    printf("STFT Performance: %.2f ms for %d samples (%.2fx realtime)\n",
           elapsed_time, long_audio_length,
           (long_audio_length / (double)TEST_SAMPLE_RATE * 1000.0) / elapsed_time);

    // 성능 통계 확인
    float forward_time, inverse_time;
    size_t memory_usage;
    result = et_stft_get_performance_stats(ctx, &forward_time, &inverse_time, &memory_usage);
    TEST_ASSERT(result == ET_SUCCESS, "Performance stats retrieval failed");

    printf("Average forward time: %.2f ms\n", forward_time);
    printf("Memory usage: %zu bytes\n", memory_usage);

    free(audio);
    free(magnitude);
    free(phase);
    et_stft_destroy_context(ctx);
    TEST_PASS();
}

// ============================================================================
// SIMD 최적화 테스트
// ============================================================================

static int test_simd_optimizations() {
    int size = 1024;
    float* real; posix_memalign((void**)&real, 32, size * sizeof(float));
    float* imag; posix_memalign((void**)&imag, 32, size * sizeof(float));
    float* magnitude; posix_memalign((void**)&magnitude, 32, size * sizeof(float));
    float* phase; posix_memalign((void**)&phase, 32, size * sizeof(float));
    float* window; posix_memalign((void**)&window, 32, size * sizeof(float));
    float* input; posix_memalign((void**)&input, 32, size * sizeof(float));
    float* output; posix_memalign((void**)&output, 32, size * sizeof(float));

    // 테스트 데이터 생성
    for (int i = 0; i < size; i++) {
        real[i] = sinf(2.0f * M_PI * i / size);
        imag[i] = cosf(2.0f * M_PI * i / size);
        input[i] = sinf(2.0f * M_PI * i / size);
    }

    // Hann 윈도우 생성
    et_stft_create_window(ET_WINDOW_HANN, size, window);

    // SIMD 크기 계산 테스트
    clock_t start_time = clock();
    et_stft_magnitude_simd(real, imag, magnitude, size);
    clock_t end_time = clock();

    double simd_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;

    // 일반 구현과 비교
    start_time = clock();
    for (int i = 0; i < size; i++) {
        magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }
    end_time = clock();

    double scalar_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;

    printf("SIMD magnitude calculation: %.3f ms\n", simd_time);
    printf("Scalar magnitude calculation: %.3f ms\n", scalar_time);

    if (scalar_time > 0) {
        printf("SIMD speedup: %.2fx\n", scalar_time / simd_time);
    }

    // 윈도우 적용 SIMD 테스트
    et_stft_apply_window_simd(input, window, output, size);

    // 결과 검증
    for (int i = 0; i < 10; i++) {
        float expected = input[i] * window[i];
        TEST_ASSERT(fabsf(output[i] - expected) < 1e-6f, "SIMD window application failed");
    }

    free(real);
    free(imag);
    free(magnitude);
    free(phase);
    free(window);
    free(input);
    free(output);

    TEST_PASS();
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main() {
    printf("=== LibEtude STFT/ISTFT Tests ===\n\n");

    int passed = 0;
    int total = 0;

    // 기본 기능 테스트
    total++; if (test_stft_context_creation()) passed++;
    total++; if (test_stft_invalid_parameters()) passed++;
    total++; if (test_window_functions()) passed++;
    total++; if (test_stft_forward_basic()) passed++;
    total++; if (test_stft_inverse_basic()) passed++;

    // 실시간 처리 테스트
    total++; if (test_stft_streaming()) passed++;

    // 성능 테스트
    total++; if (test_stft_performance()) passed++;

    // SIMD 최적화 테스트
    total++; if (test_simd_optimizations()) passed++;

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", passed, total);

    if (passed == total) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed!\n");
        return 1;
    }
}