/**
 * @file cpu_kernels.c
 * @brief 기본 CPU 커널 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 기본 수학 연산 커널
// ============================================================================

/**
 * @brief 벡터 덧셈 (CPU 구현)
 */
void cpu_vector_add(const float* a, const float* b, float* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        result[i] = a[i] + b[i];
    }
}

/**
 * @brief 벡터 곱셈 (CPU 구현)
 */
void cpu_vector_mul(const float* a, const float* b, float* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        result[i] = a[i] * b[i];
    }
}

/**
 * @brief 벡터 스칼라 곱셈 (CPU 구현)
 */
void cpu_vector_scale(const float* input, float scale, float* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        result[i] = input[i] * scale;
    }
}

/**
 * @brief 벡터 내적 (CPU 구현)
 */
float cpu_vector_dot(const float* a, const float* b, size_t size) {
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

// ============================================================================
// 행렬 연산 커널
// ============================================================================

/**
 * @brief 행렬 곱셈 (CPU 구현) - 단순한 3중 루프
 */
void cpu_matrix_mul(const float* a, const float* b, float* c,
                   size_t m, size_t n, size_t k) {
    // C = A * B
    // A: m x k, B: k x n, C: m x n
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (size_t l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

/**
 * @brief 행렬-벡터 곱셈 (CPU 구현)
 */
void cpu_matrix_vector_mul(const float* matrix, const float* vector, float* result,
                          size_t rows, size_t cols) {
    for (size_t i = 0; i < rows; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < cols; j++) {
            sum += matrix[i * cols + j] * vector[j];
        }
        result[i] = sum;
    }
}

// ============================================================================
// 활성화 함수 커널
// ============================================================================

/**
 * @brief ReLU 활성화 함수 (CPU 구현)
 */
void cpu_relu(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }
}

/**
 * @brief Sigmoid 활성화 함수 (CPU 구현)
 */
void cpu_sigmoid(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = 1.0f / (1.0f + expf(-input[i]));
    }
}

/**
 * @brief Tanh 활성화 함수 (CPU 구현)
 */
void cpu_tanh(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = tanhf(input[i]);
    }
}

/**
 * @brief GELU 활성화 함수 (CPU 구현)
 */
void cpu_gelu(const float* input, float* output, size_t size) {
    const float sqrt_2_over_pi = 0.7978845608f; // sqrt(2/π)
    for (size_t i = 0; i < size; i++) {
        float x = input[i];
        float tanh_arg = sqrt_2_over_pi * (x + 0.044715f * x * x * x);
        output[i] = 0.5f * x * (1.0f + tanhf(tanh_arg));
    }
}

// ============================================================================
// 소프트맥스 및 정규화 커널
// ============================================================================

/**
 * @brief 소프트맥스 함수 (CPU 구현)
 */
void cpu_softmax(const float* input, float* output, size_t size) {
    // 수치적 안정성을 위해 최댓값을 빼기
    float max_val = input[0];
    for (size_t i = 1; i < size; i++) {
        if (input[i] > max_val) {
            max_val = input[i];
        }
    }

    // exp 계산 및 합계
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }

    // 정규화
    for (size_t i = 0; i < size; i++) {
        output[i] /= sum;
    }
}

/**
 * @brief 레이어 정규화 (CPU 구현)
 */
void cpu_layer_norm(const float* input, float* output, size_t size,
                   const float* gamma, const float* beta, float epsilon) {
    // 평균 계산
    float mean = 0.0f;
    for (size_t i = 0; i < size; i++) {
        mean += input[i];
    }
    mean /= size;

    // 분산 계산
    float variance = 0.0f;
    for (size_t i = 0; i < size; i++) {
        float diff = input[i] - mean;
        variance += diff * diff;
    }
    variance /= size;

    // 정규화 및 스케일링
    float inv_std = 1.0f / sqrtf(variance + epsilon);
    for (size_t i = 0; i < size; i++) {
        float normalized = (input[i] - mean) * inv_std;
        output[i] = gamma[i] * normalized + beta[i];
    }
}

// ============================================================================
// 음성 특화 DSP 커널
// ============================================================================

/**
 * @brief 윈도우 함수 적용 (Hann 윈도우)
 */
void cpu_apply_hann_window(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        float window = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (size - 1)));
        output[i] = input[i] * window;
    }
}

/**
 * @brief 주파수 빈을 Mel 스케일로 변환
 */
float cpu_hz_to_mel(float hz) {
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

/**
 * @brief Mel 스케일을 주파수 빈으로 변환
 */
float cpu_mel_to_hz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

/**
 * @brief Mel 필터뱅크 적용 (CPU 구현)
 */
void cpu_apply_mel_filterbank(const float* spectrogram, float* mel_spec,
                             const float* mel_filters, size_t n_fft, size_t n_mels,
                             size_t time_frames) {
    for (size_t t = 0; t < time_frames; t++) {
        for (size_t m = 0; m < n_mels; m++) {
            float sum = 0.0f;
            for (size_t f = 0; f < n_fft / 2 + 1; f++) {
                sum += spectrogram[t * (n_fft / 2 + 1) + f] *
                       mel_filters[m * (n_fft / 2 + 1) + f];
            }
            mel_spec[t * n_mels + m] = sum;
        }
    }
}

// ============================================================================
// 커널 등록 함수
// ============================================================================

/**
 * @brief CPU 커널들을 등록합니다
 */
void register_cpu_kernels(void) {
    // 벡터 연산 커널 등록
    KernelInfo kernel_info;

    // 벡터 덧셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_cpu";
    kernel_info.kernel_func = (void*)cpu_vector_add;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0; // 모든 크기에 적합
    kernel_info.performance_score = 1.0f; // 기본 점수
    kernel_registry_register(&kernel_info);

    // 벡터 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_mul_cpu";
    kernel_info.kernel_func = (void*)cpu_vector_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    // 벡터 스칼라 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_scale_cpu";
    kernel_info.kernel_func = (void*)cpu_vector_scale;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    // 행렬 연산 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "matmul_cpu";
    kernel_info.kernel_func = (void*)cpu_matrix_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "matmul_vector_cpu";
    kernel_info.kernel_func = (void*)cpu_matrix_vector_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    // 활성화 함수 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_relu_cpu";
    kernel_info.kernel_func = (void*)cpu_relu;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_sigmoid_cpu";
    kernel_info.kernel_func = (void*)cpu_sigmoid;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_tanh_cpu";
    kernel_info.kernel_func = (void*)cpu_tanh;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_gelu_cpu";
    kernel_info.kernel_func = (void*)cpu_gelu;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    // 소프트맥스 및 정규화 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "softmax_cpu";
    kernel_info.kernel_func = (void*)cpu_softmax;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    // 음성 특화 DSP 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "hann_window_cpu";
    kernel_info.kernel_func = (void*)cpu_apply_hann_window;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "mel_filterbank_cpu";
    kernel_info.kernel_func = (void*)cpu_apply_mel_filterbank;
    kernel_info.simd_features = LIBETUDE_SIMD_NONE;
    kernel_info.optimal_size = 0;
    kernel_info.performance_score = 1.0f;
    kernel_registry_register(&kernel_info);
}