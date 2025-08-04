/**
 * @file simd_kernels.c
 * @brief SIMD 커널 통합 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 다양한 SIMD 커널들을 통합하여 관리하는 인터페이스를 제공합니다.
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/kernel_registry.h"
#include <string.h>
#include <math.h>

// ============================================================================
// 외부 커널 함수 선언
// ============================================================================

// SSE 커널 함수들
extern void register_sse_kernels(void);
extern void sse_vector_add(const float* a, const float* b, float* result, size_t size);
extern void sse_vector_mul(const float* a, const float* b, float* result, size_t size);
extern void sse_vector_scale(const float* input, float scale, float* result, size_t size);
extern float sse_vector_dot(const float* a, const float* b, size_t size);
extern void sse_gemm(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);
extern void sse_relu(const float* input, float* output, size_t size);
extern void sse_sigmoid(const float* input, float* output, size_t size);
extern void sse_tanh(const float* input, float* output, size_t size);
extern void sse_gelu(const float* input, float* output, size_t size);

// AVX 커널 함수들
extern void register_avx_kernels(void);
extern void avx_vector_add(const float* a, const float* b, float* result, size_t size);
extern void avx_vector_mul(const float* a, const float* b, float* result, size_t size);
extern void avx_vector_scale(const float* input, float scale, float* result, size_t size);
extern float avx_vector_dot(const float* a, const float* b, size_t size);
extern void avx_gemm(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);
extern void avx_gemm_blocked(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);
extern void avx_relu(const float* input, float* output, size_t size);
extern void avx_sigmoid(const float* input, float* output, size_t size);
extern void avx_tanh(const float* input, float* output, size_t size);
extern void avx_gelu(const float* input, float* output, size_t size);

// NEON 커널 함수들
extern void register_neon_kernels(void);
extern void neon_vector_add(const float* a, const float* b, float* result, size_t size);
extern void neon_vector_mul(const float* a, const float* b, float* result, size_t size);
extern void neon_vector_scale(const float* input, float scale, float* result, size_t size);
extern float neon_vector_dot(const float* a, const float* b, size_t size);
extern void neon_gemm(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);
extern void neon_gemm_blocked(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);
extern void neon_relu(const float* input, float* output, size_t size);
extern void neon_sigmoid(const float* input, float* output, size_t size);
extern void neon_tanh(const float* input, float* output, size_t size);
extern void neon_gelu(const float* input, float* output, size_t size);
extern void neon_vector_add_power_efficient(const float* a, const float* b, float* result, size_t size);
extern void neon_gemm_memory_optimized(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);

// CPU 기본 커널 함수들 (fallback)
extern void register_cpu_kernels(void);

// BF16 SIMD 커널 함수들
extern void sse_float32_to_bfloat16(const float* input, uint16_t* output, size_t size);
extern void sse_bfloat16_to_float32(const uint16_t* input, float* output, size_t size);
extern void sse_bfloat16_vector_add(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);
extern void sse_bfloat16_vector_mul(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);

extern void avx_float32_to_bfloat16(const float* input, uint16_t* output, size_t size);
extern void avx_bfloat16_to_float32(const uint16_t* input, float* output, size_t size);
extern void avx_bfloat16_vector_add(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);
extern void avx_bfloat16_vector_mul(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);
extern void avx_bfloat16_gemm(const uint16_t* a, const uint16_t* b, uint16_t* c, size_t m, size_t n, size_t k);

extern void neon_float32_to_bfloat16(const float* input, uint16_t* output, size_t size);
extern void neon_bfloat16_to_float32(const uint16_t* input, float* output, size_t size);
extern void neon_bfloat16_vector_add(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);
extern void neon_bfloat16_vector_mul(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);

// ============================================================================
// 고수준 SIMD 인터페이스 함수들
// ============================================================================

/**
 * @brief 최적화된 벡터 덧셈
 *
 * 현재 하드웨어에 최적화된 벡터 덧셈 커널을 자동 선택하여 실행합니다.
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
void simd_vector_add_optimal(const float* a, const float* b, float* result, size_t size) {
    // 커널 레지스트리에서 최적 커널 선택
    VectorAddKernel kernel = (VectorAddKernel)kernel_registry_select_optimal("vector_add", size);

    if (kernel) {
        kernel(a, b, result, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            result[i] = a[i] + b[i];
        }
    }
}

/**
 * @brief 최적화된 벡터 곱셈
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
void simd_vector_mul_optimal(const float* a, const float* b, float* result, size_t size) {
    VectorMulKernel kernel = (VectorMulKernel)kernel_registry_select_optimal("vector_mul", size);

    if (kernel) {
        kernel(a, b, result, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            result[i] = a[i] * b[i];
        }
    }
}

/**
 * @brief 최적화된 벡터 스칼라 곱셈
 *
 * @param input 입력 벡터
 * @param scale 스칼라 값
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
void simd_vector_scale_optimal(const float* input, float scale, float* result, size_t size) {
    typedef void (*VectorScaleKernel)(const float*, float, float*, size_t);
    VectorScaleKernel kernel = (VectorScaleKernel)kernel_registry_select_optimal("vector_scale", size);

    if (kernel) {
        kernel(input, scale, result, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            result[i] = input[i] * scale;
        }
    }
}

/**
 * @brief 최적화된 벡터 내적
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param size 벡터 크기
 * @return 내적 결과
 */
float simd_vector_dot_optimal(const float* a, const float* b, size_t size) {
    typedef float (*VectorDotKernel)(const float*, const float*, size_t);
    VectorDotKernel kernel = (VectorDotKernel)kernel_registry_select_optimal("vector_dot", size);

    if (kernel) {
        return kernel(a, b, size);
    } else {
        // fallback: 기본 구현
        float sum = 0.0f;
        for (size_t i = 0; i < size; i++) {
            sum += a[i] * b[i];
        }
        return sum;
    }
}

/**
 * @brief 최적화된 행렬 곱셈 (GEMM)
 *
 * @param a 행렬 A (m x k)
 * @param b 행렬 B (k x n)
 * @param c 결과 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void simd_gemm_optimal(const float* a, const float* b, float* c,
                       size_t m, size_t n, size_t k) {
    // 행렬 크기를 기준으로 최적 커널 선택
    size_t matrix_size = m * n * k;
    MatMulKernel kernel = (MatMulKernel)kernel_registry_select_optimal("gemm", matrix_size);

    if (kernel) {
        kernel(a, b, c, m, n, k);
    } else {
        // fallback: 기본 구현
        memset(c, 0, m * n * sizeof(float));
        for (size_t i = 0; i < m; i++) {
            for (size_t j = 0; j < n; j++) {
                for (size_t l = 0; l < k; l++) {
                    c[i * n + j] += a[i * k + l] * b[l * n + j];
                }
            }
        }
    }
}

/**
 * @brief 최적화된 ReLU 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
void simd_relu_optimal(const float* input, float* output, size_t size) {
    ActivationKernel kernel = (ActivationKernel)kernel_registry_select_optimal("activation_relu", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            output[i] = input[i] > 0.0f ? input[i] : 0.0f;
        }
    }
}

/**
 * @brief 최적화된 Sigmoid 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
void simd_sigmoid_optimal(const float* input, float* output, size_t size) {
    ActivationKernel kernel = (ActivationKernel)kernel_registry_select_optimal("activation_sigmoid", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            output[i] = 1.0f / (1.0f + expf(-input[i]));
        }
    }
}

/**
 * @brief 최적화된 Tanh 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
void simd_tanh_optimal(const float* input, float* output, size_t size) {
    ActivationKernel kernel = (ActivationKernel)kernel_registry_select_optimal("activation_tanh", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            output[i] = tanhf(input[i]);
        }
    }
}

/**
 * @brief 최적화된 GELU 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
void simd_gelu_optimal(const float* input, float* output, size_t size) {
    ActivationKernel kernel = (ActivationKernel)kernel_registry_select_optimal("activation_gelu", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현
        const float sqrt_2_over_pi = 0.7978845608f;
        const float coeff = 0.044715f;

        for (size_t i = 0; i < size; i++) {
            float x = input[i];
            float x3 = x * x * x;
            float inner = sqrt_2_over_pi * (x + coeff * x3);
            output[i] = 0.5f * x * (1.0f + tanhf(inner));
        }
    }
}

/**
 * @brief 최적화된 소프트맥스 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
void simd_softmax_optimal(const float* input, float* output, size_t size) {
    typedef void (*SoftmaxKernel)(const float*, float*, size_t);
    SoftmaxKernel kernel = (SoftmaxKernel)kernel_registry_select_optimal("softmax", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현 (수치적으로 안정한 소프트맥스)
        // 1. 최댓값 찾기 (수치 안정성을 위해)
        float max_val = input[0];
        for (size_t i = 1; i < size; i++) {
            if (input[i] > max_val) {
                max_val = input[i];
            }
        }

        // 2. exp(x - max) 계산 및 합계 구하기
        float sum = 0.0f;
        for (size_t i = 0; i < size; i++) {
            output[i] = expf(input[i] - max_val);
            sum += output[i];
        }

        // 3. 정규화
        float inv_sum = 1.0f / sum;
        for (size_t i = 0; i < size; i++) {
            output[i] *= inv_sum;
        }
    }
}

/**
 * @brief 최적화된 레이어 정규화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 * @param epsilon 수치 안정성을 위한 작은 값
 */
void simd_layer_norm_optimal(const float* input, float* output, size_t size, float epsilon) {
    typedef void (*LayerNormKernel)(const float*, float*, size_t, float);
    LayerNormKernel kernel = (LayerNormKernel)kernel_registry_select_optimal("layer_norm", size);

    if (kernel) {
        kernel(input, output, size, epsilon);
    } else {
        // fallback: 기본 구현
        // 1. 평균 계산
        float mean = 0.0f;
        for (size_t i = 0; i < size; i++) {
            mean += input[i];
        }
        mean /= (float)size;

        // 2. 분산 계산
        float variance = 0.0f;
        for (size_t i = 0; i < size; i++) {
            float diff = input[i] - mean;
            variance += diff * diff;
        }
        variance /= (float)size;

        // 3. 정규화
        float inv_std = 1.0f / sqrtf(variance + epsilon);
        for (size_t i = 0; i < size; i++) {
            output[i] = (input[i] - mean) * inv_std;
        }
    }
}

/**
 * @brief 최적화된 배치 정규화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 * @param mean 평균값
 * @param variance 분산값
 * @param gamma 스케일 파라미터
 * @param beta 시프트 파라미터
 * @param epsilon 수치 안정성을 위한 작은 값
 */
void simd_batch_norm_optimal(const float* input, float* output, size_t size,
                            float mean, float variance, float gamma, float beta, float epsilon) {
    typedef void (*BatchNormKernel)(const float*, float*, size_t, float, float, float, float, float);
    BatchNormKernel kernel = (BatchNormKernel)kernel_registry_select_optimal("batch_norm", size);

    if (kernel) {
        kernel(input, output, size, mean, variance, gamma, beta, epsilon);
    } else {
        // fallback: 기본 구현
        float inv_std = 1.0f / sqrtf(variance + epsilon);
        for (size_t i = 0; i < size; i++) {
            output[i] = gamma * (input[i] - mean) * inv_std + beta;
        }
    }
}

// ============================================================================
// 음성 합성 특화 SIMD 인터페이스 함수들
// ============================================================================

/**
 * @brief 최적화된 Mel 필터뱅크 적용
 */
void simd_apply_mel_filterbank_optimal(const float* spectrogram, const float* mel_filters,
                                      float* mel_output, size_t n_fft, size_t n_mels, size_t n_frames) {
    typedef void (*MelFilterbankKernel)(const float*, const float*, float*, size_t, size_t, size_t);
    size_t total_size = n_fft * n_mels * n_frames;
    MelFilterbankKernel kernel = (MelFilterbankKernel)kernel_registry_select_optimal("mel_filterbank", total_size);

    if (kernel) {
        kernel(spectrogram, mel_filters, mel_output, n_fft, n_mels, n_frames);
    } else {
        // fallback: 기본 구현
        for (size_t mel = 0; mel < n_mels; mel++) {
            const float* filter = &mel_filters[mel * n_fft];
            for (size_t frame = 0; frame < n_frames; frame++) {
                const float* spec_frame = &spectrogram[frame * n_fft];
                float sum = 0.0f;
                for (size_t i = 0; i < n_fft; i++) {
                    sum += spec_frame[i] * filter[i];
                }
                mel_output[frame * n_mels + mel] = sum;
            }
        }
    }
}

/**
 * @brief 최적화된 윈도우 함수 적용
 */
void simd_apply_window_optimal(const float* input, const float* window, float* output, size_t size) {
    typedef void (*WindowKernel)(const float*, const float*, float*, size_t);
    WindowKernel kernel = (WindowKernel)kernel_registry_select_optimal("window_function", size);

    if (kernel) {
        kernel(input, window, output, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            output[i] = input[i] * window[i];
        }
    }
}

/**
 * @brief 최적화된 복소수 곱셈
 */
void simd_complex_multiply_optimal(const float* a_real, const float* a_imag,
                                  const float* b_real, const float* b_imag,
                                  float* result_real, float* result_imag, size_t size) {
    typedef void (*ComplexMulKernel)(const float*, const float*, const float*, const float*, float*, float*, size_t);
    ComplexMulKernel kernel = (ComplexMulKernel)kernel_registry_select_optimal("complex_multiply", size);

    if (kernel) {
        kernel(a_real, a_imag, b_real, b_imag, result_real, result_imag, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            float ac = a_real[i] * b_real[i];
            float bd = a_imag[i] * b_imag[i];
            float ad = a_real[i] * b_imag[i];
            float bc = a_imag[i] * b_real[i];
            result_real[i] = ac - bd;
            result_imag[i] = ad + bc;
        }
    }
}

/**
 * @brief 최적화된 복소수 크기 계산
 */
void simd_complex_magnitude_optimal(const float* real, const float* imag, float* magnitude, size_t size) {
    typedef void (*ComplexMagKernel)(const float*, const float*, float*, size_t);
    ComplexMagKernel kernel = (ComplexMagKernel)kernel_registry_select_optimal("complex_magnitude", size);

    if (kernel) {
        kernel(real, imag, magnitude, size);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
        }
    }
}

/**
 * @brief 최적화된 로그 스펙트럼 계산
 */
void simd_log_spectrum_optimal(const float* magnitude, float* log_spectrum, size_t size, float epsilon) {
    typedef void (*LogSpectrumKernel)(const float*, float*, size_t, float);
    LogSpectrumKernel kernel = (LogSpectrumKernel)kernel_registry_select_optimal("log_spectrum", size);

    if (kernel) {
        kernel(magnitude, log_spectrum, size, epsilon);
    } else {
        // fallback: 기본 구현
        for (size_t i = 0; i < size; i++) {
            log_spectrum[i] = logf(magnitude[i] + epsilon);
        }
    }
}

/**
 * @brief 배터리 효율적인 벡터 덧셈
 */
void simd_vector_add_power_efficient(const float* a, const float* b, float* result, size_t size) {
    VectorAddKernel kernel = (VectorAddKernel)kernel_registry_select_optimal("vector_add_power_efficient", size);

    if (kernel) {
        kernel(a, b, result, size);
    } else {
        // fallback: 기본 구현 (작은 청크로 처리)
        const size_t CHUNK_SIZE = 64;
        for (size_t i = 0; i < size; i += CHUNK_SIZE) {
            size_t chunk_end = (i + CHUNK_SIZE < size) ? i + CHUNK_SIZE : size;
            for (size_t j = i; j < chunk_end; j++) {
                result[j] = a[j] + b[j];
            }
        }
    }
}

/**
 * @brief 적응형 성능 조절 벡터 덧셈
 */
void simd_vector_add_adaptive(const float* a, const float* b, float* result, size_t size) {
    VectorAddKernel kernel = (VectorAddKernel)kernel_registry_select_optimal("vector_add_adaptive", size);

    if (kernel) {
        kernel(a, b, result, size);
    } else {
        // fallback: 기본 구현
        simd_vector_add_optimal(a, b, result, size);
    }
}

// ============================================================================
// SIMD 커널 초기화 및 정리 함수
// ============================================================================

/**
 * @brief SIMD 커널 시스템을 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LibEtudeErrorCode simd_kernels_init(void) {
    // 커널 레지스트리 초기화
    LibEtudeErrorCode result = kernel_registry_init();
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    // 하드웨어 기능 확인 및 적절한 커널 등록은 kernel_registry_init()에서 처리됨

    return LIBETUDE_SUCCESS;
}

/**
 * @brief SIMD 커널 시스템을 정리합니다
 */
void simd_kernels_finalize(void) {
    kernel_registry_finalize();
}

/**
 * @brief 현재 사용 가능한 SIMD 기능을 반환합니다
 *
 * @return SIMD 기능 플래그
 */
uint32_t simd_kernels_get_features(void) {
    return kernel_registry_get_hardware_features();
}

/**
 * @brief SIMD 커널 정보를 출력합니다 (디버그용)
 */
void simd_kernels_print_info(void) {
    kernel_registry_print_info();
}

// ============================================================================
// BF16 양자화 SIMD 최적화 함수들
// ============================================================================

/**
 * @brief SIMD 최적화된 float32 to BF16 변환
 */
void simd_float32_to_bfloat16_optimal(const float* input, uint16_t* output, size_t size) {
    typedef void (*Float32ToBF16Kernel)(const float*, uint16_t*, size_t);
    Float32ToBF16Kernel kernel = (Float32ToBF16Kernel)kernel_registry_select_optimal("float32_to_bfloat16", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현 (스칼라 버전)
        for (size_t i = 0; i < size; i++) {
            union {
                float f;
                uint32_t i;
            } converter;

            converter.f = input[i];

            // 반올림을 위한 바이어스 추가
            uint32_t bias = 0x00007FFF + ((converter.i >> 16) & 1);

            // 상위 16비트 추출 (반올림 적용)
            output[i] = (uint16_t)((converter.i + bias) >> 16);
        }
    }
}

/**
 * @brief SIMD 최적화된 BF16 to float32 변환
 */
void simd_bfloat16_to_float32_optimal(const uint16_t* input, float* output, size_t size) {
    typedef void (*BF16ToFloat32Kernel)(const uint16_t*, float*, size_t);
    BF16ToFloat32Kernel kernel = (BF16ToFloat32Kernel)kernel_registry_select_optimal("bfloat16_to_float32", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현 (스칼라 버전)
        for (size_t i = 0; i < size; i++) {
            union {
                float f;
                uint32_t i;
            } converter;

            // BF16은 float32의 상위 16비트만 사용
            // 하위 16비트는 0으로 채움
            converter.i = ((uint32_t)input[i]) << 16;
            output[i] = converter.f;
        }
    }
}

/**
 * @brief SIMD 최적화된 BF16 벡터 덧셈
 */
void simd_bfloat16_vector_add_optimal(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size) {
    typedef void (*BF16VectorAddKernel)(const uint16_t*, const uint16_t*, uint16_t*, size_t);
    BF16VectorAddKernel kernel = (BF16VectorAddKernel)kernel_registry_select_optimal("bfloat16_vector_add", size);

    if (kernel) {
        kernel(a, b, result, size);
    } else {
        // fallback: 기본 구현 (BF16 -> float32 -> 연산 -> BF16)
        for (size_t i = 0; i < size; i++) {
            // BF16을 float32로 변환
            union { float f; uint32_t i; } conv_a, conv_b, conv_result;
            conv_a.i = ((uint32_t)a[i]) << 16;
            conv_b.i = ((uint32_t)b[i]) << 16;

            // float32로 덧셈 수행
            conv_result.f = conv_a.f + conv_b.f;

            // 결과를 BF16으로 변환
            uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
            result[i] = (uint16_t)((conv_result.i + bias) >> 16);
        }
    }
}

/**
 * @brief SIMD 최적화된 BF16 벡터 곱셈
 */
void simd_bfloat16_vector_mul_optimal(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size) {
    typedef void (*BF16VectorMulKernel)(const uint16_t*, const uint16_t*, uint16_t*, size_t);
    BF16VectorMulKernel kernel = (BF16VectorMulKernel)kernel_registry_select_optimal("bfloat16_vector_mul", size);

    if (kernel) {
        kernel(a, b, result, size);
    } else {
        // fallback: 기본 구현 (BF16 -> float32 -> 연산 -> BF16)
        for (size_t i = 0; i < size; i++) {
            // BF16을 float32로 변환
            union { float f; uint32_t i; } conv_a, conv_b, conv_result;
            conv_a.i = ((uint32_t)a[i]) << 16;
            conv_b.i = ((uint32_t)b[i]) << 16;

            // float32로 곱셈 수행
            conv_result.f = conv_a.f * conv_b.f;

            // 결과를 BF16으로 변환
            uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
            result[i] = (uint16_t)((conv_result.i + bias) >> 16);
        }
    }
}

/**
 * @brief SIMD 최적화된 BF16 행렬 곱셈
 */
void simd_bfloat16_gemm_optimal(const uint16_t* a, const uint16_t* b, uint16_t* c,
                               size_t m, size_t n, size_t k) {
    typedef void (*BF16GemmKernel)(const uint16_t*, const uint16_t*, uint16_t*, size_t, size_t, size_t);
    size_t matrix_size = m * n * k;
    BF16GemmKernel kernel = (BF16GemmKernel)kernel_registry_select_optimal("bfloat16_gemm", matrix_size);

    if (kernel) {
        kernel(a, b, c, m, n, k);
    } else {
        // fallback: 기본 구현 (BF16 -> float32 -> GEMM -> BF16)
        // 결과 행렬 초기화
        memset(c, 0, m * n * sizeof(uint16_t));

        for (size_t i = 0; i < m; i++) {
            for (size_t j = 0; j < n; j++) {
                float sum = 0.0f;

                for (size_t l = 0; l < k; l++) {
                    // BF16을 float32로 변환하여 연산
                    union { float f; uint32_t i; } conv_a, conv_b;
                    conv_a.i = ((uint32_t)a[i * k + l]) << 16;
                    conv_b.i = ((uint32_t)b[l * n + j]) << 16;

                    sum += conv_a.f * conv_b.f;
                }

                // 결과를 BF16으로 변환
                union { float f; uint32_t i; } conv_result;
                conv_result.f = sum;
                uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
                c[i * n + j] = (uint16_t)((conv_result.i + bias) >> 16);
            }
        }
    }
}

/**
 * @brief SIMD 최적화된 BF16 활성화 함수 (ReLU)
 */
void simd_bfloat16_relu_optimal(const uint16_t* input, uint16_t* output, size_t size) {
    typedef void (*BF16ReluKernel)(const uint16_t*, uint16_t*, size_t);
    BF16ReluKernel kernel = (BF16ReluKernel)kernel_registry_select_optimal("bfloat16_relu", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현
        const uint16_t zero_bf16 = 0x0000; // BF16 형태의 0.0

        for (size_t i = 0; i < size; i++) {
            // BF16을 float32로 변환하여 비교
            union { float f; uint32_t i; } conv;
            conv.i = ((uint32_t)input[i]) << 16;

            if (conv.f > 0.0f) {
                output[i] = input[i]; // 양수면 그대로
            } else {
                output[i] = zero_bf16; // 음수면 0
            }
        }
    }
}

/**
 * @brief SIMD 최적화된 BF16 활성화 함수 (GELU)
 */
void simd_bfloat16_gelu_optimal(const uint16_t* input, uint16_t* output, size_t size) {
    typedef void (*BF16GeluKernel)(const uint16_t*, uint16_t*, size_t);
    BF16GeluKernel kernel = (BF16GeluKernel)kernel_registry_select_optimal("bfloat16_gelu", size);

    if (kernel) {
        kernel(input, output, size);
    } else {
        // fallback: 기본 구현
        const float sqrt_2_over_pi = 0.7978845608f;
        const float coeff = 0.044715f;

        for (size_t i = 0; i < size; i++) {
            // BF16을 float32로 변환
            union { float f; uint32_t i; } conv_input, conv_output;
            conv_input.i = ((uint32_t)input[i]) << 16;

            // GELU 계산
            float x = conv_input.f;
            float x3 = x * x * x;
            float inner = sqrt_2_over_pi * (x + coeff * x3);
            conv_output.f = 0.5f * x * (1.0f + tanhf(inner));

            // 결과를 BF16으로 변환
            uint32_t bias = 0x00007FFF + ((conv_output.i >> 16) & 1);
            output[i] = (uint16_t)((conv_output.i + bias) >> 16);
        }
    }
}

/**
 * @brief 적응형 BF16 양자화 임계값 계산
 */
float simd_bfloat16_adaptive_threshold(const float* input, size_t size, float quantile) {
    if (!input || size == 0 || quantile < 0.0f || quantile > 1.0f) {
        return 1.0f; // 기본값
    }

    // 절댓값의 히스토그램을 사용한 분위수 계산
    #define HISTOGRAM_BINS 1000
    float histogram[HISTOGRAM_BINS] = {0};

    // 최댓값 찾기
    float max_abs = 0.0f;
    for (size_t i = 0; i < size; i++) {
        float abs_val = fabsf(input[i]);
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }

    if (max_abs == 0.0f) {
        return 1.0f; // 모든 값이 0인 경우
    }

    // 히스토그램 생성
    float bin_width = max_abs / HISTOGRAM_BINS;
    for (size_t i = 0; i < size; i++) {
        float abs_val = fabsf(input[i]);
        int bin = (int)(abs_val / bin_width);
        if (bin >= HISTOGRAM_BINS) bin = HISTOGRAM_BINS - 1;
        histogram[bin] += 1.0f;
    }

    // 분위수 계산
    float target_count = quantile * size;
    float cumulative = 0.0f;

    for (int i = 0; i < HISTOGRAM_BINS; i++) {
        cumulative += histogram[i];
        if (cumulative >= target_count) {
            return (i + 1) * bin_width;
        }
    }

    return max_abs; // fallback
}

// ============================================================================
// 테스트용 호환성 함수들 구현 (Legacy API)
// ============================================================================

/**
 * @brief SIMD 커널 시스템 초기화 (테스트 호환성)
 */
LibEtudeErrorCode et_init_simd_kernels(void) {
    return simd_kernels_init();
}

/**
 * @brief SIMD 커널 시스템 정리 (테스트 호환성)
 */
void et_cleanup_simd_kernels(void) {
    simd_kernels_finalize();
}

/**
 * @brief 벡터 덧셈 (테스트 호환성)
 */
void et_simd_vector_add(const float* a, const float* b, float* result, size_t size) {
    simd_vector_add_optimal(a, b, result, size);
}

/**
 * @brief 벡터 곱셈 (테스트 호환성)
 */
void et_simd_vector_mul(const float* a, const float* b, float* result, size_t size) {
    simd_vector_mul_optimal(a, b, result, size);
}

/**
 * @brief 벡터 내적 (테스트 호환성)
 */
float et_simd_dot_product(const float* a, const float* b, size_t size) {
    return simd_vector_dot_optimal(a, b, size);
}

/**
 * @brief 행렬-벡터 곱셈 (테스트 호환성)
 */
void et_simd_matrix_vector_mul(const float* matrix, const float* vector, float* result, size_t rows, size_t cols) {
    // 행렬-벡터 곱셈: result = matrix * vector
    // matrix는 row-major 형태 (rows x cols)
    // vector는 cols 크기
    // result는 rows 크기

    for (size_t i = 0; i < rows; i++) {
        result[i] = simd_vector_dot_optimal(&matrix[i * cols], vector, cols);
    }
}

/**
 * @brief ReLU 활성화 함수 (테스트 호환성)
 */
void et_simd_relu(const float* input, float* output, size_t size) {
    simd_relu_optimal(input, output, size);
}

/**
 * @brief Sigmoid 활성화 함수 (테스트 호환성)
 */
void et_simd_sigmoid(const float* input, float* output, size_t size) {
    simd_sigmoid_optimal(input, output, size);
}

/**
 * @brief Tanh 활성화 함수 (테스트 호환성)
 */
void et_simd_tanh(const float* input, float* output, size_t size) {
    simd_tanh_optimal(input, output, size);
}

/**
 * @brief SSE 지원 여부 확인
 */
bool et_has_sse_support(void) {
    uint32_t features = simd_kernels_get_features();
    return (features & LIBETUDE_SIMD_SSE) != 0 || (features & LIBETUDE_SIMD_SSE2) != 0;
}

/**
 * @brief AVX 지원 여부 확인
 */
bool et_has_avx_support(void) {
    uint32_t features = simd_kernels_get_features();
    return (features & LIBETUDE_SIMD_AVX) != 0;
}

/**
 * @brief NEON 지원 여부 확인
 */
bool et_has_neon_support(void) {
    uint32_t features = simd_kernels_get_features();
    return (features & LIBETUDE_SIMD_NEON) != 0;
}

/**
 * @brief 음성 특화 BF16 양자화 파라미터 튜닝
 */
bool simd_bfloat16_voice_tuning(const float* input, size_t size, bool is_frequency_domain,
                               float* scale_factor, float* bias_factor) {
    if (!input || size == 0 || !scale_factor || !bias_factor) {
        return false;
    }

    // 기본값 설정
    *scale_factor = 1.0f;
    *bias_factor = 0.0f;

    // 통계 계산
    float mean = 0.0f;
    float variance = 0.0f;
    float min_val = input[0];
    float max_val = input[0];

    // 평균 계산
    for (size_t i = 0; i < size; i++) {
        mean += input[i];
        if (input[i] < min_val) min_val = input[i];
        if (input[i] > max_val) max_val = input[i];
    }
    mean /= (float)size;

    // 분산 계산
    for (size_t i = 0; i < size; i++) {
        float diff = input[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)size;

    float std_dev = sqrtf(variance);

    if (is_frequency_domain) {
        // 주파수 도메인: 로그 스케일 특성 고려
        // 낮은 주파수 성분이 더 중요하므로 동적 범위 조정
        float dynamic_range = max_val - min_val;

        if (dynamic_range > 0.0f) {
            // 3-sigma 규칙을 사용하여 스케일 조정
            float three_sigma = 3.0f * std_dev;
            *scale_factor = 1.0f / (three_sigma + 1e-8f);

            // 평균을 중심으로 바이어스 조정
            *bias_factor = -mean * (*scale_factor);
        }
    } else {
        // 시간 도메인: 신호의 동적 범위 보존
        float abs_max = fmaxf(fabsf(min_val), fabsf(max_val));

        if (abs_max > 0.0f) {
            // BF16의 동적 범위를 고려한 스케일링
            // BF16은 약 ±3.4e38 범위를 가지지만, 정밀도는 7비트
            float bf16_safe_range = 65504.0f; // BF16의 안전한 최댓값
            *scale_factor = bf16_safe_range / (abs_max * 1.1f); // 10% 여유

            // 시간 도메인에서는 일반적으로 바이어스 없음
            *bias_factor = 0.0f;
        }
    }

    // 스케일 팩터가 너무 작거나 큰 경우 클램핑
    if (*scale_factor < 1e-6f) *scale_factor = 1e-6f;
    if (*scale_factor > 1e6f) *scale_factor = 1e6f;

    // 바이어스 팩터 클램핑
    if (*bias_factor < -1000.0f) *bias_factor = -1000.0f;
    if (*bias_factor > 1000.0f) *bias_factor = 1000.0f;

    return true;
}