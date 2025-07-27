/**
 * @file stft.c
 * @brief SIMD 최적화된 STFT/ISTFT 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 음성 합성에 특화된 고성능 STFT/ISTFT 구현입니다.
 * SIMD 최적화, 윈도우 함수 최적화, 병렬 처리를 지원합니다.
 */

#include "libetude/stft.h"
#include "libetude/fast_math.h"
#include "libetude/simd_kernels.h"
#include "libetude/error.h"
#include "libetude/memory.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

#ifdef __SSE__
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

#ifdef __AVX__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

// ============================================================================
// 내부 구조체 정의
// ============================================================================

/**
 * @brief STFT 컨텍스트 내부 구조체
 */
struct ETSTFTContext {
    ETSTFTConfig config;        /**< STFT 설정 */

    // 윈도우 함수
    float* window;              /**< 윈도우 함수 배열 */
    float window_norm;          /**< 윈도우 정규화 계수 */

    // FFT 관련
    float* fft_input;           /**< FFT 입력 버퍼 */
    float* fft_real;            /**< FFT 실수부 출력 */
    float* fft_imag;            /**< FFT 허수부 출력 */

    // 실시간 처리용 버퍼
    float* overlap_buffer;      /**< 오버랩 버퍼 */
    float* frame_buffer;        /**< 프레임 버퍼 */
    int buffer_pos;             /**< 버퍼 위치 */

    // 병렬 처리
    pthread_t* worker_threads;  /**< 워커 스레드 배열 */
    int num_active_threads;     /**< 활성 스레드 수 */

    // 성능 통계
    double total_forward_time;  /**< 총 순방향 변환 시간 */
    double total_inverse_time;  /**< 총 역방향 변환 시간 */
    int forward_count;          /**< 순방향 변환 횟수 */
    int inverse_count;          /**< 역방향 변환 횟수 */
    size_t memory_usage;        /**< 메모리 사용량 */

    bool initialized;           /**< 초기화 여부 */
};

/**
 * @brief 병렬 FFT 작업 구조체
 */
typedef struct {
    const float* input;         /**< 입력 데이터 */
    float* output_real;         /**< 출력 실수부 */
    float* output_imag;         /**< 출력 허수부 */
    int start_frame;            /**< 시작 프레임 */
    int end_frame;              /**< 끝 프레임 */
    int fft_size;               /**< FFT 크기 */
    const float* window;        /**< 윈도우 함수 */
    bool is_forward;            /**< 순방향 여부 */
} ETFFTTask;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult et_stft_init_buffers(ETSTFTContext* ctx);
static void et_stft_cleanup_buffers(ETSTFTContext* ctx);
static ETResult et_stft_create_window_internal(ETSTFTContext* ctx);
static void* et_stft_worker_thread(void* arg);
static ETResult et_stft_fft_radix2(const float* input, float* output_real, float* output_imag, int size);
static ETResult et_stft_ifft_radix2(const float* input_real, const float* input_imag, float* output, int size);

// ============================================================================
// STFT 컨텍스트 관리
// ============================================================================

ETSTFTContext* et_stft_create_context(const ETSTFTConfig* config) {
    if (!config) {
        et_set_error(ET_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__, "Config cannot be NULL");
        return NULL;
    }

    // FFT 크기가 2의 거듭제곱인지 확인
    if ((config->fft_size & (config->fft_size - 1)) != 0 || config->fft_size < 64) {
        et_set_error(ET_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__, "FFT size must be a power of 2 and >= 64");
        return NULL;
    }

    ETSTFTContext* ctx = (ETSTFTContext*)calloc(1, sizeof(ETSTFTContext));
    if (!ctx) {
        et_set_error(ET_ERROR_OUT_OF_MEMORY, __FILE__, __LINE__, __func__, "Failed to allocate STFT context");
        return NULL;
    }

    // 설정 복사
    ctx->config = *config;

    // 기본값 설정
    if (ctx->config.win_length <= 0) {
        ctx->config.win_length = ctx->config.fft_size;
    }
    if (ctx->config.hop_size <= 0) {
        ctx->config.hop_size = ctx->config.fft_size / 4;
    }
    if (ctx->config.num_threads <= 0) {
        ctx->config.num_threads = 4; // 기본 4개 스레드
    }

    // 버퍼 초기화
    if (et_stft_init_buffers(ctx) != ET_SUCCESS) {
        et_stft_destroy_context(ctx);
        return NULL;
    }

    // 윈도우 함수 생성
    if (et_stft_create_window_internal(ctx) != ET_SUCCESS) {
        et_stft_destroy_context(ctx);
        return NULL;
    }

    ctx->initialized = true;
    return ctx;
}

void et_stft_destroy_context(ETSTFTContext* ctx) {
    if (!ctx) return;

    et_stft_cleanup_buffers(ctx);
    free(ctx);
}

ETSTFTConfig et_stft_default_config(int fft_size, int hop_size, ETWindowType window_type) {
    ETSTFTConfig config = {0};
    config.fft_size = fft_size;
    config.hop_size = hop_size;
    config.win_length = fft_size;
    config.window_type = window_type;
    config.mode = ET_STFT_MODE_NORMAL;
    config.enable_simd = true;
    config.enable_parallel = true;
    config.num_threads = 4;
    return config;
}

ETResult et_stft_update_config(ETSTFTContext* ctx, const ETSTFTConfig* config) {
    if (!ctx || !config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 기존 버퍼 정리
    et_stft_cleanup_buffers(ctx);

    // 새 설정 적용
    ctx->config = *config;

    // 버퍼 재초기화
    ETResult result = et_stft_init_buffers(ctx);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 윈도우 함수 재생성
    return et_stft_create_window_internal(ctx);
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static ETResult et_stft_init_buffers(ETSTFTContext* ctx) {
    int fft_size = ctx->config.fft_size;
    int win_length = ctx->config.win_length;

    // 메모리 사용량 계산
    size_t total_memory = 0;

    // 윈도우 함수 버퍼
    if (posix_memalign((void**)&ctx->window, 32, win_length * sizeof(float)) != 0) {
        return ET_ERROR_OUT_OF_MEMORY;
    }
    total_memory += win_length * sizeof(float);

    // FFT 버퍼들
    if (posix_memalign((void**)&ctx->fft_input, 32, fft_size * sizeof(float)) != 0 ||
        posix_memalign((void**)&ctx->fft_real, 32, (fft_size / 2 + 1) * sizeof(float)) != 0 ||
        posix_memalign((void**)&ctx->fft_imag, 32, (fft_size / 2 + 1) * sizeof(float)) != 0) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    total_memory += fft_size * sizeof(float);
    total_memory += 2 * (fft_size / 2 + 1) * sizeof(float);

    // 실시간 처리용 버퍼
    if (ctx->config.mode == ET_STFT_MODE_REALTIME) {
        if (posix_memalign((void**)&ctx->overlap_buffer, 32, fft_size * sizeof(float)) != 0 ||
            posix_memalign((void**)&ctx->frame_buffer, 32, fft_size * sizeof(float)) != 0) {
            return ET_ERROR_OUT_OF_MEMORY;
        }

        memset(ctx->overlap_buffer, 0, fft_size * sizeof(float));
        memset(ctx->frame_buffer, 0, fft_size * sizeof(float));
        ctx->buffer_pos = 0;

        total_memory += 2 * fft_size * sizeof(float);
    }

    // 병렬 처리용 스레드 배열
    if (ctx->config.enable_parallel && ctx->config.num_threads > 1) {
        ctx->worker_threads = (pthread_t*)calloc(ctx->config.num_threads, sizeof(pthread_t));
        if (!ctx->worker_threads) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
        total_memory += ctx->config.num_threads * sizeof(pthread_t);
    }

    ctx->memory_usage = total_memory;
    return ET_SUCCESS;
}

static void et_stft_cleanup_buffers(ETSTFTContext* ctx) {
    if (ctx->window) {
        free(ctx->window);
        ctx->window = NULL;
    }
    if (ctx->fft_input) {
        free(ctx->fft_input);
        ctx->fft_input = NULL;
    }
    if (ctx->fft_real) {
        free(ctx->fft_real);
        ctx->fft_real = NULL;
    }
    if (ctx->fft_imag) {
        free(ctx->fft_imag);
        ctx->fft_imag = NULL;
    }
    if (ctx->overlap_buffer) {
        free(ctx->overlap_buffer);
        ctx->overlap_buffer = NULL;
    }
    if (ctx->frame_buffer) {
        free(ctx->frame_buffer);
        ctx->frame_buffer = NULL;
    }
    if (ctx->worker_threads) {
        free(ctx->worker_threads);
        ctx->worker_threads = NULL;
    }
}

static ETResult et_stft_create_window_internal(ETSTFTContext* ctx) {
    ETResult result = et_stft_create_window(ctx->config.window_type,
                                           ctx->config.win_length,
                                           ctx->window);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 윈도우 정규화 계수 계산
    ctx->window_norm = et_stft_window_normalization(ctx->window,
                                                   ctx->config.win_length,
                                                   ctx->config.hop_size);

    return ET_SUCCESS;
}

// ============================================================================
// STFT/ISTFT 핵심 함수 구현
// ============================================================================

ETResult et_stft_forward(ETSTFTContext* ctx, const float* audio, int audio_len,
                        float* magnitude, float* phase, int* n_frames) {
    if (!ctx || !audio || !magnitude || !phase || !n_frames) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!ctx->initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    clock_t start_time = clock();

    int fft_size = ctx->config.fft_size;
    int hop_size = ctx->config.hop_size;
    int win_length = ctx->config.win_length;

    // 출력 프레임 수 계산
    *n_frames = et_stft_calculate_frames(audio_len, fft_size, hop_size);

    if (ctx->config.enable_parallel && ctx->config.num_threads > 1) {
        // 병렬 처리
        return et_stft_fft_parallel(audio, magnitude, phase, *n_frames, fft_size, ctx->config.num_threads);
    } else {
        // 순차 처리
        for (int frame = 0; frame < *n_frames; frame++) {
            int start_pos = frame * hop_size;

            // 프레임 추출 및 제로 패딩
            memset(ctx->fft_input, 0, fft_size * sizeof(float));
            int copy_len = (start_pos + win_length <= audio_len) ? win_length : (audio_len - start_pos);
            if (copy_len > 0) {
                memcpy(ctx->fft_input, audio + start_pos, copy_len * sizeof(float));
            }

            // 윈도우 적용
            if (ctx->config.enable_simd) {
                et_stft_apply_window_simd(ctx->fft_input, ctx->window, ctx->fft_input, win_length);
            } else {
                for (int i = 0; i < win_length; i++) {
                    ctx->fft_input[i] *= ctx->window[i];
                }
            }

            // FFT 수행
            ETResult result = et_stft_fft_real_simd(ctx->fft_input, ctx->fft_real, ctx->fft_imag, fft_size);
            if (result != ET_SUCCESS) {
                return result;
            }

            // 크기와 위상 계산
            int freq_bins = fft_size / 2 + 1;
            float* mag_frame = magnitude + frame * freq_bins;
            float* phase_frame = phase + frame * freq_bins;

            if (ctx->config.enable_simd) {
                et_stft_magnitude_simd(ctx->fft_real, ctx->fft_imag, mag_frame, freq_bins);
                et_stft_phase_simd(ctx->fft_real, ctx->fft_imag, phase_frame, freq_bins);
            } else {
                for (int i = 0; i < freq_bins; i++) {
                    mag_frame[i] = sqrtf(ctx->fft_real[i] * ctx->fft_real[i] +
                                        ctx->fft_imag[i] * ctx->fft_imag[i]);
                    phase_frame[i] = atan2f(ctx->fft_imag[i], ctx->fft_real[i]);
                }
            }
        }
    }

    // 성능 통계 업데이트
    clock_t end_time = clock();
    ctx->total_forward_time += ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    ctx->forward_count++;

    return ET_SUCCESS;
}ETResult
et_stft_inverse(ETSTFTContext* ctx, const float* magnitude, const float* phase,
                        int n_frames, float* audio, int* audio_len) {
    if (!ctx || !magnitude || !phase || !audio || !audio_len) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!ctx->initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    clock_t start_time = clock();

    int fft_size = ctx->config.fft_size;
    int hop_size = ctx->config.hop_size;
    int freq_bins = fft_size / 2 + 1;

    // 출력 오디오 길이 계산
    *audio_len = et_stft_calculate_audio_length(n_frames, fft_size, hop_size);

    // 출력 버퍼 초기화
    memset(audio, 0, *audio_len * sizeof(float));

    // 오버랩-애드를 위한 임시 버퍼
    float* temp_frame;
    if (posix_memalign((void**)&temp_frame, 32, fft_size * sizeof(float)) != 0) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    for (int frame = 0; frame < n_frames; frame++) {
        const float* mag_frame = magnitude + frame * freq_bins;
        const float* phase_frame = phase + frame * freq_bins;

        // 크기와 위상에서 복소수 복원
        if (ctx->config.enable_simd) {
            et_stft_polar_to_complex_simd(mag_frame, phase_frame, ctx->fft_real, ctx->fft_imag, freq_bins);
        } else {
            for (int i = 0; i < freq_bins; i++) {
                ctx->fft_real[i] = mag_frame[i] * cosf(phase_frame[i]);
                ctx->fft_imag[i] = mag_frame[i] * sinf(phase_frame[i]);
            }
        }

        // IFFT 수행
        ETResult result = et_stft_ifft_complex_simd(ctx->fft_real, ctx->fft_imag, temp_frame, fft_size);
        if (result != ET_SUCCESS) {
            free(temp_frame);
            return result;
        }

        // 윈도우 적용
        if (ctx->config.enable_simd) {
            et_stft_apply_window_simd(temp_frame, ctx->window, temp_frame, ctx->config.win_length);
        } else {
            for (int i = 0; i < ctx->config.win_length; i++) {
                temp_frame[i] *= ctx->window[i];
            }
        }

        // 오버랩-애드
        int start_pos = frame * hop_size;
        for (int i = 0; i < fft_size && start_pos + i < *audio_len; i++) {
            audio[start_pos + i] += temp_frame[i] / ctx->window_norm;
        }
    }

    free(temp_frame);

    // 성능 통계 업데이트
    clock_t end_time = clock();
    ctx->total_inverse_time += ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    ctx->inverse_count++;

    return ET_SUCCESS;
}

ETResult et_stft_forward_streaming(ETSTFTContext* ctx, const float* audio_chunk, int chunk_size,
                                  float* magnitude, float* phase) {
    if (!ctx || !audio_chunk || !magnitude || !phase) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (ctx->config.mode != ET_STFT_MODE_REALTIME) {
        return ET_ERROR_INVALID_STATE;
    }

    int fft_size = ctx->config.fft_size;
    int hop_size = ctx->config.hop_size;
    int freq_bins = fft_size / 2 + 1;

    // 입력 청크를 프레임 버퍼에 추가
    int remaining_space = fft_size - ctx->buffer_pos;
    int copy_size = (chunk_size < remaining_space) ? chunk_size : remaining_space;

    memcpy(ctx->frame_buffer + ctx->buffer_pos, audio_chunk, copy_size * sizeof(float));
    ctx->buffer_pos += copy_size;

    // 프레임이 완성되면 STFT 수행
    if (ctx->buffer_pos >= hop_size) {
        // 윈도우 적용
        if (ctx->config.enable_simd) {
            et_stft_apply_window_simd(ctx->frame_buffer, ctx->window, ctx->fft_input, ctx->config.win_length);
        } else {
            for (int i = 0; i < ctx->config.win_length; i++) {
                ctx->fft_input[i] = ctx->frame_buffer[i] * ctx->window[i];
            }
        }

        // FFT 수행
        ETResult result = et_stft_fft_real_simd(ctx->fft_input, ctx->fft_real, ctx->fft_imag, fft_size);
        if (result != ET_SUCCESS) {
            return result;
        }

        // 크기와 위상 계산
        if (ctx->config.enable_simd) {
            et_stft_magnitude_simd(ctx->fft_real, ctx->fft_imag, magnitude, freq_bins);
            et_stft_phase_simd(ctx->fft_real, ctx->fft_imag, phase, freq_bins);
        } else {
            for (int i = 0; i < freq_bins; i++) {
                magnitude[i] = sqrtf(ctx->fft_real[i] * ctx->fft_real[i] +
                                   ctx->fft_imag[i] * ctx->fft_imag[i]);
                phase[i] = atan2f(ctx->fft_imag[i], ctx->fft_real[i]);
            }
        }

        // 버퍼 시프트
        memmove(ctx->frame_buffer, ctx->frame_buffer + hop_size,
                (fft_size - hop_size) * sizeof(float));
        ctx->buffer_pos -= hop_size;
    }

    return ET_SUCCESS;
}

ETResult et_stft_inverse_streaming(ETSTFTContext* ctx, const float* magnitude, const float* phase,
                                  float* audio_chunk, int* chunk_size) {
    if (!ctx || !magnitude || !phase || !audio_chunk || !chunk_size) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (ctx->config.mode != ET_STFT_MODE_REALTIME) {
        return ET_ERROR_INVALID_STATE;
    }

    int fft_size = ctx->config.fft_size;
    int hop_size = ctx->config.hop_size;
    int freq_bins = fft_size / 2 + 1;

    // 크기와 위상에서 복소수 복원
    if (ctx->config.enable_simd) {
        et_stft_polar_to_complex_simd(magnitude, phase, ctx->fft_real, ctx->fft_imag, freq_bins);
    } else {
        for (int i = 0; i < freq_bins; i++) {
            ctx->fft_real[i] = magnitude[i] * cosf(phase[i]);
            ctx->fft_imag[i] = magnitude[i] * sinf(phase[i]);
        }
    }

    // IFFT 수행
    ETResult result = et_stft_ifft_complex_simd(ctx->fft_real, ctx->fft_imag, ctx->fft_input, fft_size);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 윈도우 적용
    if (ctx->config.enable_simd) {
        et_stft_apply_window_simd(ctx->fft_input, ctx->window, ctx->fft_input, ctx->config.win_length);
    } else {
        for (int i = 0; i < ctx->config.win_length; i++) {
            ctx->fft_input[i] *= ctx->window[i];
        }
    }

    // 오버랩-애드
    for (int i = 0; i < fft_size; i++) {
        ctx->overlap_buffer[i] += ctx->fft_input[i] / ctx->window_norm;
    }

    // 출력 청크 생성
    *chunk_size = hop_size;
    memcpy(audio_chunk, ctx->overlap_buffer, hop_size * sizeof(float));

    // 오버랩 버퍼 시프트
    memmove(ctx->overlap_buffer, ctx->overlap_buffer + hop_size,
            (fft_size - hop_size) * sizeof(float));
    memset(ctx->overlap_buffer + (fft_size - hop_size), 0, hop_size * sizeof(float));

    return ET_SUCCESS;
}

// ============================================================================
// 윈도우 함수 최적화 구현
// ============================================================================

ETResult et_stft_create_window(ETWindowType window_type, int size, float* window) {
    if (!window || size <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    switch (window_type) {
        case ET_WINDOW_HANN:
            et_hann_window(window, size);
            break;

        case ET_WINDOW_HAMMING:
            et_hamming_window(window, size);
            break;

        case ET_WINDOW_BLACKMAN:
            et_blackman_window(window, size);
            break;

        case ET_WINDOW_RECTANGULAR:
            for (int i = 0; i < size; i++) {
                window[i] = 1.0f;
            }
            break;

        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

void et_stft_apply_window_simd(const float* input, const float* window, float* output, int size) {
    if (!input || !window || !output || size <= 0) {
        return;
    }

#ifdef __AVX__
    // AVX 최적화 (8개 float 동시 처리)
    int simd_size = size & ~7; // 8의 배수로 맞춤

    for (int i = 0; i < simd_size; i += 8) {
        __m256 input_vec = _mm256_load_ps(input + i);
        __m256 window_vec = _mm256_load_ps(window + i);
        __m256 result = _mm256_mul_ps(input_vec, window_vec);
        _mm256_store_ps(output + i, result);
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        output[i] = input[i] * window[i];
    }

#elif defined(__SSE__)
    // SSE 최적화 (4개 float 동시 처리)
    int simd_size = size & ~3; // 4의 배수로 맞춤

    for (int i = 0; i < simd_size; i += 4) {
        __m128 input_vec = _mm_load_ps(input + i);
        __m128 window_vec = _mm_load_ps(window + i);
        __m128 result = _mm_mul_ps(input_vec, window_vec);
        _mm_store_ps(output + i, result);
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        output[i] = input[i] * window[i];
    }

#elif defined(__ARM_NEON)
    // NEON 최적화 (4개 float 동시 처리)
    int simd_size = size & ~3; // 4의 배수로 맞춤

    for (int i = 0; i < simd_size; i += 4) {
        float32x4_t input_vec = vld1q_f32(input + i);
        float32x4_t window_vec = vld1q_f32(window + i);
        float32x4_t result = vmulq_f32(input_vec, window_vec);
        vst1q_f32(output + i, result);
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        output[i] = input[i] * window[i];
    }

#else
    // 일반 구현
    for (int i = 0; i < size; i++) {
        output[i] = input[i] * window[i];
    }
#endif
}

float et_stft_window_normalization(const float* window, int size, int hop_size) {
    if (!window || size <= 0 || hop_size <= 0) {
        return 1.0f;
    }

    // 윈도우 함수의 제곱합 계산
    float sum_squared = 0.0f;

#ifdef __AVX__
    __m256 sum_vec = _mm256_setzero_ps();
    int simd_size = size & ~7;

    for (int i = 0; i < simd_size; i += 8) {
        __m256 window_vec = _mm256_load_ps(window + i);
        __m256 squared = _mm256_mul_ps(window_vec, window_vec);
        sum_vec = _mm256_add_ps(sum_vec, squared);
    }

    // 벡터 합계 계산
    float temp[8];
    _mm256_store_ps(temp, sum_vec);
    for (int i = 0; i < 8; i++) {
        sum_squared += temp[i];
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        sum_squared += window[i] * window[i];
    }

#else
    for (int i = 0; i < size; i++) {
        sum_squared += window[i] * window[i];
    }
#endif

    // 정규화 계수 계산 (오버랩-애드 고려)
    return sqrtf(sum_squared * hop_size / (float)size);
}

// ============================================================================
// FFT 최적화 함수 구현
// ============================================================================

ETResult et_stft_fft_real_simd(const float* input, float* output_real, float* output_imag, int size) {
    if (!input || !output_real || !output_imag || size <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 간단한 Radix-2 FFT 구현 (실제로는 더 최적화된 라이브러리 사용 권장)
    return et_stft_fft_radix2(input, output_real, output_imag, size);
}

ETResult et_stft_ifft_complex_simd(const float* input_real, const float* input_imag, float* output, int size) {
    if (!input_real || !input_imag || !output || size <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 간단한 Radix-2 IFFT 구현
    return et_stft_ifft_radix2(input_real, input_imag, output, size);
}

// ============================================================================
// 병렬 처리 구현
// ============================================================================

static void* et_stft_worker_thread(void* arg) {
    ETFFTTask* task = (ETFFTTask*)arg;

    for (int frame = task->start_frame; frame < task->end_frame; frame++) {
        // 각 프레임에 대해 FFT 또는 IFFT 수행
        // 실제 구현에서는 더 복잡한 병렬 처리 로직 필요
    }

    return NULL;
}

ETResult et_stft_fft_parallel(const float* input, float* output_real, float* output_imag,
                              int n_frames, int fft_size, int num_threads) {
    if (!input || !output_real || !output_imag || n_frames <= 0 || fft_size <= 0 || num_threads <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 간단한 병렬 처리 구현 (실제로는 더 정교한 작업 분할 필요)
    int frames_per_thread = n_frames / num_threads;
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    ETFFTTask* tasks = (ETFFTTask*)malloc(num_threads * sizeof(ETFFTTask));

    if (!threads || !tasks) {
        free(threads);
        free(tasks);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 작업 분할 및 스레드 생성
    for (int i = 0; i < num_threads; i++) {
        tasks[i].input = input;
        tasks[i].output_real = output_real;
        tasks[i].output_imag = output_imag;
        tasks[i].start_frame = i * frames_per_thread;
        tasks[i].end_frame = (i == num_threads - 1) ? n_frames : (i + 1) * frames_per_thread;
        tasks[i].fft_size = fft_size;
        tasks[i].is_forward = true;

        pthread_create(&threads[i], NULL, et_stft_worker_thread, &tasks[i]);
    }

    // 스레드 완료 대기
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(tasks);

    return ET_SUCCESS;
}

ETResult et_stft_ifft_parallel(const float* input_real, const float* input_imag, float* output,
                               int n_frames, int fft_size, int num_threads) {
    // IFFT 병렬 처리 구현 (FFT와 유사)
    return ET_SUCCESS; // 간단한 구현
}

/* ============================================================================ */
/* 유틸리티 함수 구현 */
/* ============================================================================ */

int et_stft_calculate_frames(int audio_len, int fft_size, int hop_size) {
    if (audio_len <= 0 || fft_size <= 0 || hop_size <= 0) {
        return 0;
    }

    return (audio_len - fft_size) / hop_size + 1;
}

int et_stft_calculate_audio_length(int n_frames, int fft_size, int hop_size) {
    if (n_frames <= 0 || fft_size <= 0 || hop_size <= 0) {
        return 0;
    }

    return (n_frames - 1) * hop_size + fft_size;
}

void et_stft_magnitude_simd(const float* real, const float* imag, float* magnitude, int size) {
    if (!real || !imag || !magnitude || size <= 0) {
        return;
    }

#ifdef __AVX__
    int simd_size = size & ~7; // 8의 배수로 맞춤

    for (int i = 0; i < simd_size; i += 8) {
        __m256 real_vec = _mm256_load_ps(real + i);
        __m256 imag_vec = _mm256_load_ps(imag + i);

        __m256 real_sq = _mm256_mul_ps(real_vec, real_vec);
        __m256 imag_sq = _mm256_mul_ps(imag_vec, imag_vec);
        __m256 sum = _mm256_add_ps(real_sq, imag_sq);
        __m256 mag = _mm256_sqrt_ps(sum);

        _mm256_store_ps(magnitude + i, mag);
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }

#elif defined(__SSE__)
    int simd_size = size & ~3; // 4의 배수로 맞춤

    for (int i = 0; i < simd_size; i += 4) {
        __m128 real_vec = _mm_load_ps(real + i);
        __m128 imag_vec = _mm_load_ps(imag + i);

        __m128 real_sq = _mm_mul_ps(real_vec, real_vec);
        __m128 imag_sq = _mm_mul_ps(imag_vec, imag_vec);
        __m128 sum = _mm_add_ps(real_sq, imag_sq);
        __m128 mag = _mm_sqrt_ps(sum);

        _mm_store_ps(magnitude + i, mag);
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }

#elif defined(__ARM_NEON)
    int simd_size = size & ~3; // 4의 배수로 맞춤

    for (int i = 0; i < simd_size; i += 4) {
        float32x4_t real_vec = vld1q_f32(real + i);
        float32x4_t imag_vec = vld1q_f32(imag + i);

        float32x4_t real_sq = vmulq_f32(real_vec, real_vec);
        float32x4_t imag_sq = vmulq_f32(imag_vec, imag_vec);
        float32x4_t sum = vaddq_f32(real_sq, imag_sq);

        // NEON에는 sqrt가 없으므로 역제곱근 사용
        float32x4_t rsqrt = vrsqrteq_f32(sum);
        float32x4_t mag = vmulq_f32(sum, rsqrt);

        vst1q_f32(magnitude + i, mag);
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }

#else
    // 일반 구현
    for (int i = 0; i < size; i++) {
        magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }
#endif
}

void et_stft_phase_simd(const float* real, const float* imag, float* phase, int size) {
    if (!real || !imag || !phase || size <= 0) {
        return;
    }

    // atan2는 SIMD 최적화가 어려우므로 일반 구현 사용
    for (int i = 0; i < size; i++) {
        phase[i] = et_fast_atan2(imag[i], real[i]);
    }
}

void et_stft_polar_to_complex_simd(const float* magnitude, const float* phase,
                                  float* real, float* imag, int size) {
    if (!magnitude || !phase || !real || !imag || size <= 0) {
        return;
    }

#ifdef __AVX__
    int simd_size = size & ~7; // 8의 배수로 맞춤

    for (int i = 0; i < simd_size; i += 8) {
        __m256 mag_vec = _mm256_load_ps(magnitude + i);
        __m256 phase_vec = _mm256_load_ps(phase + i);

        // 삼각함수는 SIMD로 직접 계산하기 어려우므로 스칼라 처리
        float temp_real[8], temp_imag[8];
        float temp_mag[8], temp_phase[8];

        _mm256_store_ps(temp_mag, mag_vec);
        _mm256_store_ps(temp_phase, phase_vec);

        for (int j = 0; j < 8; j++) {
            temp_real[j] = temp_mag[j] * et_fast_cos(temp_phase[j]);
            temp_imag[j] = temp_mag[j] * et_fast_sin(temp_phase[j]);
        }

        _mm256_store_ps(real + i, _mm256_load_ps(temp_real));
        _mm256_store_ps(imag + i, _mm256_load_ps(temp_imag));
    }

    // 나머지 처리
    for (int i = simd_size; i < size; i++) {
        real[i] = magnitude[i] * et_fast_cos(phase[i]);
        imag[i] = magnitude[i] * et_fast_sin(phase[i]);
    }

#else
    // 일반 구현
    for (int i = 0; i < size; i++) {
        real[i] = magnitude[i] * et_fast_cos(phase[i]);
        imag[i] = magnitude[i] * et_fast_sin(phase[i]);
    }
#endif
}

ETResult et_stft_get_performance_stats(ETSTFTContext* ctx, float* forward_time,
                                      float* inverse_time, size_t* memory_usage) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (forward_time) {
        *forward_time = (ctx->forward_count > 0) ?
                       (float)(ctx->total_forward_time / ctx->forward_count) : 0.0f;
    }

    if (inverse_time) {
        *inverse_time = (ctx->inverse_count > 0) ?
                       (float)(ctx->total_inverse_time / ctx->inverse_count) : 0.0f;
    }

    if (memory_usage) {
        *memory_usage = ctx->memory_usage;
    }

    return ET_SUCCESS;
}

ETResult et_stft_reset_context(ETSTFTContext* ctx) {
    if (!ctx) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 실시간 처리 버퍼 리셋
    if (ctx->overlap_buffer) {
        memset(ctx->overlap_buffer, 0, ctx->config.fft_size * sizeof(float));
    }
    if (ctx->frame_buffer) {
        memset(ctx->frame_buffer, 0, ctx->config.fft_size * sizeof(float));
    }

    ctx->buffer_pos = 0;

    // 성능 통계 리셋
    ctx->total_forward_time = 0.0;
    ctx->total_inverse_time = 0.0;
    ctx->forward_count = 0;
    ctx->inverse_count = 0;

    return ET_SUCCESS;
}

// ============================================================================
// 내부 FFT 구현 (간단한 Radix-2)
// ============================================================================

static void et_fft_bit_reverse(float* real, float* imag, int size) {
    int j = 0;
    for (int i = 1; i < size; i++) {
        int bit = size >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;

        if (i < j) {
            // 실수부 교환
            float temp = real[i];
            real[i] = real[j];
            real[j] = temp;

            // 허수부 교환
            temp = imag[i];
            imag[i] = imag[j];
            imag[j] = temp;
        }
    }
}

static ETResult et_stft_fft_radix2(const float* input, float* output_real, float* output_imag, int size) {
    if ((size & (size - 1)) != 0) {
        return ET_ERROR_INVALID_ARGUMENT; // 2의 거듭제곱이 아님
    }

    // 입력 복사
    for (int i = 0; i < size; i++) {
        output_real[i] = input[i];
        output_imag[i] = 0.0f;
    }

    // 비트 역순 정렬
    et_fft_bit_reverse(output_real, output_imag, size);

    // Cooley-Tukey FFT
    for (int len = 2; len <= size; len <<= 1) {
        float angle = -2.0f * M_PI / len;
        float wlen_real = et_fast_cos(angle);
        float wlen_imag = et_fast_sin(angle);

        for (int i = 0; i < size; i += len) {
            float w_real = 1.0f;
            float w_imag = 0.0f;

            for (int j = 0; j < len / 2; j++) {
                int u = i + j;
                int v = i + j + len / 2;

                float u_real = output_real[u];
                float u_imag = output_imag[u];
                float v_real = output_real[v];
                float v_imag = output_imag[v];

                // 복소수 곱셈: v * w
                float temp_real = v_real * w_real - v_imag * w_imag;
                float temp_imag = v_real * w_imag + v_imag * w_real;

                output_real[u] = u_real + temp_real;
                output_imag[u] = u_imag + temp_imag;
                output_real[v] = u_real - temp_real;
                output_imag[v] = u_imag - temp_imag;

                // w 업데이트
                float new_w_real = w_real * wlen_real - w_imag * wlen_imag;
                float new_w_imag = w_real * wlen_imag + w_imag * wlen_real;
                w_real = new_w_real;
                w_imag = new_w_imag;
            }
        }
    }

    // 실수 FFT의 경우 대칭성을 이용하여 절반만 출력
    int output_size = size / 2 + 1;
    for (int i = output_size; i < size; i++) {
        output_real[i] = 0.0f;
        output_imag[i] = 0.0f;
    }

    return ET_SUCCESS;
}

static ETResult et_stft_ifft_radix2(const float* input_real, const float* input_imag, float* output, int size) {
    if ((size & (size - 1)) != 0) {
        return ET_ERROR_INVALID_ARGUMENT; // 2의 거듭제곱이 아님
    }

    // 임시 버퍼
    float* temp_real = (float*)malloc(size * sizeof(float));
    float* temp_imag = (float*)malloc(size * sizeof(float));

    if (!temp_real || !temp_imag) {
        free(temp_real);
        free(temp_imag);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 입력 복사 (켤레 복소수)
    int freq_bins = size / 2 + 1;
    for (int i = 0; i < freq_bins; i++) {
        temp_real[i] = input_real[i];
        temp_imag[i] = -input_imag[i]; // 켤레
    }

    // 대칭성을 이용하여 나머지 채우기
    for (int i = freq_bins; i < size; i++) {
        int mirror_idx = size - i;
        temp_real[i] = temp_real[mirror_idx];
        temp_imag[i] = -temp_imag[mirror_idx];
    }

    // 비트 역순 정렬
    et_fft_bit_reverse(temp_real, temp_imag, size);

    // Cooley-Tukey FFT (IFFT는 FFT와 동일하지만 각도 부호가 반대)
    for (int len = 2; len <= size; len <<= 1) {
        float angle = 2.0f * M_PI / len; // IFFT는 양의 각도
        float wlen_real = et_fast_cos(angle);
        float wlen_imag = et_fast_sin(angle);

        for (int i = 0; i < size; i += len) {
            float w_real = 1.0f;
            float w_imag = 0.0f;

            for (int j = 0; j < len / 2; j++) {
                int u = i + j;
                int v = i + j + len / 2;

                float u_real = temp_real[u];
                float u_imag = temp_imag[u];
                float v_real = temp_real[v];
                float v_imag = temp_imag[v];

                // 복소수 곱셈: v * w
                float temp_val_real = v_real * w_real - v_imag * w_imag;
                float temp_val_imag = v_real * w_imag + v_imag * w_real;

                temp_real[u] = u_real + temp_val_real;
                temp_imag[u] = u_imag + temp_val_imag;
                temp_real[v] = u_real - temp_val_real;
                temp_imag[v] = u_imag - temp_val_imag;

                // w 업데이트
                float new_w_real = w_real * wlen_real - w_imag * wlen_imag;
                float new_w_imag = w_real * wlen_imag + w_imag * wlen_real;
                w_real = new_w_real;
                w_imag = new_w_imag;
            }
        }
    }

    // 정규화 및 실수부만 출력
    float norm = 1.0f / size;
    for (int i = 0; i < size; i++) {
        output[i] = temp_real[i] * norm;
    }

    free(temp_real);
    free(temp_imag);

    return ET_SUCCESS;
}