/**
 * @file neon_kernels.c
 * @brief ARM NEON SIMD 커널 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * ARM NEON을 사용한 고성능 벡터 연산 및 활성화 함수 구현
 * 모바일 환경에 최적화된 커널들을 제공합니다.
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/kernel_registry.h"
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef LIBETUDE_HAVE_NEON
#include <arm_neon.h>
#endif

// ============================================================================
// NEON 벡터 연산 커널
// ============================================================================

#ifdef LIBETUDE_HAVE_NEON

/**
 * @brief 벡터 덧셈 (NEON 구현)
 *
 * ARM NEON을 사용하여 4개의 float를 동시에 처리합니다.
 * 모바일 환경에서 배터리 효율성을 고려한 최적화를 적용했습니다.
 */
void neon_vector_add(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        float32x4_t vresult = vaddq_f32(va, vb);
        vst1q_f32(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = a[i] + b[i];
    }
}

/**
 * @brief 벡터 곱셈 (NEON 구현)
 */
void neon_vector_mul(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        float32x4_t vresult = vmulq_f32(va, vb);
        vst1q_f32(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = a[i] * b[i];
    }
}

/**
 * @brief 벡터 스칼라 곱셈 (NEON 구현)
 */
void neon_vector_scale(const float* input, float scale, float* result, size_t size) {
    size_t i = 0;
    float32x4_t vscale = vdupq_n_f32(scale);

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vresult = vmulq_f32(vinput, vscale);
        vst1q_f32(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = input[i] * scale;
    }
}

/**
 * @brief 벡터 내적 (NEON 구현)
 *
 * NEON의 multiply-accumulate 명령어를 활용하여 효율적인 내적 계산
 */
float neon_vector_dot(const float* a, const float* b, size_t size) {
    size_t i = 0;
    float32x4_t vsum = vdupq_n_f32(0.0f);

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        // multiply-accumulate: vsum += va * vb
        vsum = vmlaq_f32(vsum, va, vb);
    }

    // 벡터 내 요소들을 합산 (horizontal add)
    float32x2_t vsum_low = vget_low_f32(vsum);
    float32x2_t vsum_high = vget_high_f32(vsum);
    float32x2_t vsum_pair = vadd_f32(vsum_low, vsum_high);
    float sum = vget_lane_f32(vsum_pair, 0) + vget_lane_f32(vsum_pair, 1);

    // 나머지 요소 처리
    for (; i < size; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

/**
 * @brief 간단한 GEMM (General Matrix Multiply) - NEON 구현
 *
 * 모바일 환경에 최적화된 행렬 곱셈 구현
 * 캐시 효율성과 배터리 사용량을 고려한 블록 단위 처리
 */
void neon_gemm(const float* a, const float* b, float* c,
               size_t m, size_t n, size_t k) {
    // 행렬 C를 0으로 초기화
    memset(c, 0, m * n * sizeof(float));

    // 행렬 곱셈 수행 - 캐시 친화적 순서
    for (size_t i = 0; i < m; i++) {
        for (size_t l = 0; l < k; l++) {
            const float a_val = a[i * k + l];
            const float32x4_t va = vdupq_n_f32(a_val);

            size_t j = 0;
            // 4개 요소씩 NEON 벡터 처리
            for (; j + 3 < n; j += 4) {
                float32x4_t vb = vld1q_f32(&b[l * n + j]);
                float32x4_t vc = vld1q_f32(&c[i * n + j]);
                // multiply-accumulate: vc += va * vb
                float32x4_t vresult = vmlaq_f32(vc, va, vb);
                vst1q_f32(&c[i * n + j], vresult);
            }

            // 나머지 요소 처리
            for (; j < n; j++) {
                c[i * n + j] += a[i * k + l] * b[l * n + j];
            }
        }
    }
}

/**
 * @brief 블록 단위 GEMM (NEON 구현)
 *
 * 큰 행렬에 대해 캐시 효율성을 높이는 블록 단위 처리
 * 모바일 환경의 제한된 캐시 크기를 고려한 최적화
 */
void neon_gemm_blocked(const float* a, const float* b, float* c,
                       size_t m, size_t n, size_t k) {
    const size_t BLOCK_SIZE = 64; // 모바일 환경에 적합한 블록 크기

    // 행렬 C를 0으로 초기화
    memset(c, 0, m * n * sizeof(float));

    // 블록 단위로 행렬 곱셈 수행
    for (size_t ii = 0; ii < m; ii += BLOCK_SIZE) {
        for (size_t jj = 0; jj < n; jj += BLOCK_SIZE) {
            for (size_t kk = 0; kk < k; kk += BLOCK_SIZE) {
                // 블록 경계 계산
                size_t i_end = (ii + BLOCK_SIZE < m) ? ii + BLOCK_SIZE : m;
                size_t j_end = (jj + BLOCK_SIZE < n) ? jj + BLOCK_SIZE : n;
                size_t k_end = (kk + BLOCK_SIZE < k) ? kk + BLOCK_SIZE : k;

                // 블록 내 행렬 곱셈
                for (size_t i = ii; i < i_end; i++) {
                    for (size_t l = kk; l < k_end; l++) {
                        const float a_val = a[i * k + l];
                        const float32x4_t va = vdupq_n_f32(a_val);

                        size_t j = jj;
                        // 4개 요소씩 NEON 벡터 처리
                        for (; j + 3 < j_end; j += 4) {
                            float32x4_t vb = vld1q_f32(&b[l * n + j]);
                            float32x4_t vc = vld1q_f32(&c[i * n + j]);
                            float32x4_t vresult = vmlaq_f32(vc, va, vb);
                            vst1q_f32(&c[i * n + j], vresult);
                        }

                        // 나머지 요소 처리
                        for (; j < j_end; j++) {
                            c[i * n + j] += a[i * k + l] * b[l * n + j];
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// NEON 활성화 함수 커널
// ============================================================================

/**
 * @brief ReLU 활성화 함수 (NEON 구현)
 */
void neon_relu(const float* input, float* output, size_t size) {
    size_t i = 0;
    float32x4_t vzero = vdupq_n_f32(0.0f);

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vresult = vmaxq_f32(vinput, vzero);
        vst1q_f32(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }
}

/**
 * @brief 빠른 exp 근사 함수 (NEON용)
 *
 * 모바일 환경에 최적화된 exp 근사 함수
 * 정확도와 성능의 균형을 맞춘 구현
 */
static inline float32x4_t neon_fast_exp(float32x4_t x) {
    // 입력 범위 제한 (-10 ~ 10)
    const float32x4_t vmin = vdupq_n_f32(-10.0f);
    const float32x4_t vmax = vdupq_n_f32(10.0f);
    x = vmaxq_f32(vminq_f32(x, vmax), vmin);

    // 테일러 급수 계수들
    const float32x4_t c1 = vdupq_n_f32(1.0f);
    const float32x4_t c2 = vdupq_n_f32(1.0f);
    const float32x4_t c3 = vdupq_n_f32(0.5f);        // 1/2!
    const float32x4_t c4 = vdupq_n_f32(0.16666667f); // 1/3!
    const float32x4_t c5 = vdupq_n_f32(0.04166667f); // 1/4!

    // x의 거듭제곱 계산
    float32x4_t x2 = vmulq_f32(x, x);
    float32x4_t x3 = vmulq_f32(x2, x);
    float32x4_t x4 = vmulq_f32(x3, x);

    // 다항식 계산: 1 + x + x²/2 + x³/6 + x⁴/24
    float32x4_t result = c1;
    result = vmlaq_f32(result, c2, x);   // result += c2 * x
    result = vmlaq_f32(result, c3, x2);  // result += c3 * x2
    result = vmlaq_f32(result, c4, x3);  // result += c4 * x3
    result = vmlaq_f32(result, c5, x4);  // result += c5 * x4

    return result;
}

/**
 * @brief 빠른 tanh 근사 함수 (NEON용)
 *
 * tanh(x) ≈ x * (27 + x²) / (27 + 9*x²) (Padé 근사)
 * 모바일 환경에서 배터리 효율성을 고려한 구현
 */
static inline float32x4_t neon_fast_tanh(float32x4_t x) {
    // 입력 범위 제한
    const float32x4_t vmin = vdupq_n_f32(-5.0f);
    const float32x4_t vmax = vdupq_n_f32(5.0f);
    x = vmaxq_f32(vminq_f32(x, vmax), vmin);

    const float32x4_t c27 = vdupq_n_f32(27.0f);
    const float32x4_t c9 = vdupq_n_f32(9.0f);

    float32x4_t x2 = vmulq_f32(x, x);

    // 분자: x * (27 + x²)
    float32x4_t numerator = vmulq_f32(x, vaddq_f32(c27, x2));

    // 분모: 27 + 9*x²
    float32x4_t denominator = vmlaq_f32(c27, c9, x2); // 27 + 9*x²

    // 나눗셈 근사 (Newton-Raphson 1회 반복)
    float32x4_t inv_denom = vrecpeq_f32(denominator);
    inv_denom = vmulq_f32(vrecpsq_f32(denominator, inv_denom), inv_denom);

    return vmulq_f32(numerator, inv_denom);
}

/**
 * @brief Sigmoid 활성화 함수 (NEON 구현)
 * sigmoid(x) = 1 / (1 + exp(-x))
 */
void neon_sigmoid(const float* input, float* output, size_t size) {
    size_t i = 0;
    const float32x4_t vone = vdupq_n_f32(1.0f);
    const float32x4_t vneg_one = vdupq_n_f32(-1.0f);

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vneg_input = vmulq_f32(vinput, vneg_one);

        // 빠른 exp(-x) 근사 계산
        float32x4_t vexp_neg_x = neon_fast_exp(vneg_input);

        // 1 + exp(-x)
        float32x4_t vdenom = vaddq_f32(vone, vexp_neg_x);

        // 1 / (1 + exp(-x)) - 역수 근사 사용
        float32x4_t inv_denom = vrecpeq_f32(vdenom);
        inv_denom = vmulq_f32(vrecpsq_f32(vdenom, inv_denom), inv_denom);

        vst1q_f32(output + i, inv_denom);
    }

    // 나머지 요소 처리 (표준 라이브러리 사용)
    for (; i < size; i++) {
        output[i] = 1.0f / (1.0f + expf(-input[i]));
    }
}

/**
 * @brief Tanh 활성화 함수 (NEON 구현)
 */
void neon_tanh(const float* input, float* output, size_t size) {
    size_t i = 0;

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vresult = neon_fast_tanh(vinput);
        vst1q_f32(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = tanhf(input[i]);
    }
}

/**
 * @brief GELU 활성화 함수 (NEON 구현)
 * GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x^3)))
 *
 * 모바일 환경에 최적화된 GELU 구현
 */
void neon_gelu(const float* input, float* output, size_t size) {
    size_t i = 0;
    const float sqrt_2_over_pi = 0.7978845608f; // sqrt(2/π)
    const float coeff = 0.044715f;

    const float32x4_t vsqrt_2_over_pi = vdupq_n_f32(sqrt_2_over_pi);
    const float32x4_t vcoeff = vdupq_n_f32(coeff);
    const float32x4_t vhalf = vdupq_n_f32(0.5f);
    const float32x4_t vone = vdupq_n_f32(1.0f);

    // 4개 요소씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t x = vld1q_f32(input + i);

        // x^3 계산
        float32x4_t x2 = vmulq_f32(x, x);
        float32x4_t x3 = vmulq_f32(x2, x);

        // sqrt(2/π) * (x + 0.044715 * x^3)
        float32x4_t inner = vmlaq_f32(x, vcoeff, x3); // x + coeff * x3
        inner = vmulq_f32(vsqrt_2_over_pi, inner);

        // tanh(inner)
        float32x4_t tanh_inner = neon_fast_tanh(inner);

        // 0.5 * x * (1 + tanh(inner))
        float32x4_t one_plus_tanh = vaddq_f32(vone, tanh_inner);
        float32x4_t result = vmulq_f32(vhalf, x);
        result = vmulq_f32(result, one_plus_tanh);

        vst1q_f32(output + i, result);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        float x = input[i];
        float x3 = x * x * x;
        float inner = sqrt_2_over_pi * (x + coeff * x3);
        output[i] = 0.5f * x * (1.0f + tanhf(inner));
    }
}

/**
 * @brief 소프트맥스 함수 (NEON 구현)
 */
void neon_softmax(const float* input, float* output, size_t size) {
    size_t i = 0;

    // 1. 최댓값 찾기 (수치 안정성을 위해)
    float max_val = input[0];
    for (i = 1; i < size; i++) {
        if (input[i] > max_val) {
            max_val = input[i];
        }
    }

    float32x4_t vmax = vdupq_n_f32(max_val);
    float32x4_t vsum = vdupq_n_f32(0.0f);

    // 2. exp(x - max) 계산 및 합계 구하기 (NEON 벡터화)
    i = 0;
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vshifted = vsubq_f32(vinput, vmax);

        // 빠른 exp 근사 사용
        float32x4_t vexp = neon_fast_exp(vshifted);
        vst1q_f32(output + i, vexp);
        vsum = vaddq_f32(vsum, vexp);
    }

    // 벡터 내 요소들을 합산
    float32x2_t vsum_low = vget_low_f32(vsum);
    float32x2_t vsum_high = vget_high_f32(vsum);
    float32x2_t vsum_pair = vadd_f32(vsum_low, vsum_high);
    float sum = vget_lane_f32(vsum_pair, 0) + vget_lane_f32(vsum_pair, 1);

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }

    // 3. 정규화 (NEON 벡터화)
    float32x4_t vinv_sum = vdupq_n_f32(1.0f / sum);
    i = 0;
    for (; i + 3 < size; i += 4) {
        float32x4_t voutput = vld1q_f32(output + i);
        float32x4_t vnormalized = vmulq_f32(voutput, vinv_sum);
        vst1q_f32(output + i, vnormalized);
    }

    // 나머지 요소 처리
    float inv_sum = 1.0f / sum;
    for (; i < size; i++) {
        output[i] *= inv_sum;
    }
}

/**
 * @brief 레이어 정규화 함수 (NEON 구현)
 */
void neon_layer_norm(const float* input, float* output, size_t size, float epsilon) {
    size_t i = 0;

    // 1. 평균 계산 (NEON 벡터화)
    float32x4_t vsum = vdupq_n_f32(0.0f);
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        vsum = vaddq_f32(vsum, vinput);
    }

    // 벡터 내 요소들을 합산
    float32x2_t vsum_low = vget_low_f32(vsum);
    float32x2_t vsum_high = vget_high_f32(vsum);
    float32x2_t vsum_pair = vadd_f32(vsum_low, vsum_high);
    float sum = vget_lane_f32(vsum_pair, 0) + vget_lane_f32(vsum_pair, 1);

    // 나머지 요소 처리
    for (; i < size; i++) {
        sum += input[i];
    }

    float mean = sum / (float)size;
    float32x4_t vmean = vdupq_n_f32(mean);

    // 2. 분산 계산 (NEON 벡터화)
    float32x4_t vvar_sum = vdupq_n_f32(0.0f);
    i = 0;
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vdiff = vsubq_f32(vinput, vmean);
        float32x4_t vdiff_sq = vmulq_f32(vdiff, vdiff);
        vvar_sum = vaddq_f32(vvar_sum, vdiff_sq);
    }

    // 벡터 내 요소들을 합산
    vsum_low = vget_low_f32(vvar_sum);
    vsum_high = vget_high_f32(vvar_sum);
    vsum_pair = vadd_f32(vsum_low, vsum_high);
    float var_sum = vget_lane_f32(vsum_pair, 0) + vget_lane_f32(vsum_pair, 1);

    // 나머지 요소 처리
    for (; i < size; i++) {
        float diff = input[i] - mean;
        var_sum += diff * diff;
    }

    float variance = var_sum / (float)size;
    float inv_std = 1.0f / sqrtf(variance + epsilon);
    float32x4_t vinv_std = vdupq_n_f32(inv_std);

    // 3. 정규화 (NEON 벡터화)
    i = 0;
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vdiff = vsubq_f32(vinput, vmean);
        float32x4_t vnormalized = vmulq_f32(vdiff, vinv_std);
        vst1q_f32(output + i, vnormalized);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = (input[i] - mean) * inv_std;
    }
}

/**
 * @brief 배치 정규화 함수 (NEON 구현)
 */
void neon_batch_norm(const float* input, float* output, size_t size,
                    float mean, float variance, float gamma, float beta, float epsilon) {
    size_t i = 0;
    float inv_std = 1.0f / sqrtf(variance + epsilon);

    float32x4_t vmean = vdupq_n_f32(mean);
    float32x4_t vinv_std = vdupq_n_f32(inv_std);
    float32x4_t vgamma = vdupq_n_f32(gamma);
    float32x4_t vbeta = vdupq_n_f32(beta);

    // NEON 벡터화된 배치 정규화
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);

        // (input - mean) * inv_std
        float32x4_t vnormalized = vmulq_f32(vsubq_f32(vinput, vmean), vinv_std);

        // gamma * normalized + beta
        float32x4_t vresult = vmlaq_f32(vbeta, vgamma, vnormalized);

        vst1q_f32(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = gamma * (input[i] - mean) * inv_std + beta;
    }
}

// ============================================================================
// 모바일 특화 최적화 함수들
// ============================================================================

/**
 * @brief 배터리 효율적인 벡터 연산
 *
 * 모바일 환경에서 배터리 사용량을 최소화하는 벡터 연산
 * 처리량보다 효율성을 우선시하는 구현
 */
void neon_vector_add_power_efficient(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;

    // 작은 청크 단위로 처리하여 열 발생 최소화
    const size_t CHUNK_SIZE = 64;

    while (i < size) {
        size_t chunk_end = (i + CHUNK_SIZE < size) ? i + CHUNK_SIZE : size;

        // 청크 내에서 NEON 벡터 처리
        size_t j = i;
        for (; j + 3 < chunk_end; j += 4) {
            float32x4_t va = vld1q_f32(a + j);
            float32x4_t vb = vld1q_f32(b + j);
            float32x4_t vresult = vaddq_f32(va, vb);
            vst1q_f32(result + j, vresult);
        }

        // 청크 내 나머지 요소 처리
        for (; j < chunk_end; j++) {
            result[j] = a[j] + b[j];
        }

        i = chunk_end;

        // 열 관리를 위한 작은 지연 (필요시)
        // 실제 구현에서는 온도 센서 기반 동적 조절 가능
    }
}

/**
 * @brief 모바일 환경용 저전력 GEMM
 *
 * 배터리 수명을 고려한 행렬 곱셈 구현
 * 작은 블록 크기와 적응형 처리로 전력 소모 최소화
 */
void neon_gemm_low_power(const float* a, const float* b, float* c,
                         size_t m, size_t n, size_t k) {
    // 매우 작은 블록 크기로 전력 소모 최소화
    const size_t BLOCK_SIZE = 16;

    // 행렬 C를 0으로 초기화
    memset(c, 0, m * n * sizeof(float));

    // 전력 효율적인 순서로 블록 처리
    for (size_t ii = 0; ii < m; ii += BLOCK_SIZE) {
        for (size_t kk = 0; kk < k; kk += BLOCK_SIZE) {
            for (size_t jj = 0; jj < n; jj += BLOCK_SIZE) {
                // 블록 경계 계산
                size_t i_end = (ii + BLOCK_SIZE < m) ? ii + BLOCK_SIZE : m;
                size_t k_end = (kk + BLOCK_SIZE < k) ? kk + BLOCK_SIZE : k;
                size_t j_end = (jj + BLOCK_SIZE < n) ? jj + BLOCK_SIZE : n;

                // 블록 내 행렬 곱셈 - 전력 효율적 구현
                for (size_t i = ii; i < i_end; i++) {
                    for (size_t l = kk; l < k_end; l++) {
                        const float a_val = a[i * k + l];
                        const float32x4_t va = vdupq_n_f32(a_val);

                        size_t j = jj;
                        // 2개씩만 처리하여 전력 소모 감소
                        for (; j + 1 < j_end; j += 2) {
                            float32x2_t vb = vld1_f32(&b[l * n + j]);
                            float32x2_t vc = vld1_f32(&c[i * n + j]);
                            float32x2_t va_low = vget_low_f32(va);
                            float32x2_t vresult = vmla_f32(vc, va_low, vb);
                            vst1_f32(&c[i * n + j], vresult);
                        }

                        // 나머지 요소 처리
                        for (; j < j_end; j++) {
                            c[i * n + j] += a[i * k + l] * b[l * n + j];
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief 온도 인식 적응형 벡터 연산
 *
 * 시스템 온도에 따라 처리 강도를 조절하는 벡터 연산
 * 과열 방지를 위한 동적 성능 조절
 */
void neon_vector_add_thermal_aware(const float* a, const float* b, float* result, size_t size) {
    // 간단한 온도 시뮬레이션 (실제로는 시스템 온도 센서 사용)
    static int thermal_state = 0; // 0: 정상, 1: 주의, 2: 과열
    static size_t operation_count = 0;

    operation_count++;

    // 간단한 온도 상태 시뮬레이션
    if (operation_count % 1000 == 0) {
        thermal_state = (thermal_state + 1) % 3;
    }

    size_t i = 0;
    size_t chunk_size;

    // 온도 상태에 따른 청크 크기 조절
    switch (thermal_state) {
        case 0: // 정상 온도
            chunk_size = 128;
            break;
        case 1: // 주의 온도
            chunk_size = 64;
            break;
        case 2: // 과열 상태
            chunk_size = 32;
            break;
        default:
            chunk_size = 64;
    }

    while (i < size) {
        size_t chunk_end = (i + chunk_size < size) ? i + chunk_size : size;

        // 청크 내에서 NEON 벡터 처리
        size_t j = i;
        for (; j + 3 < chunk_end; j += 4) {
            float32x4_t va = vld1q_f32(a + j);
            float32x4_t vb = vld1q_f32(b + j);
            float32x4_t vresult = vaddq_f32(va, vb);
            vst1q_f32(result + j, vresult);
        }

        // 나머지 요소 처리
        for (; j < chunk_end; j++) {
            result[j] = a[j] + b[j];
        }

        i = chunk_end;

        // 과열 상태에서는 추가 지연
        if (thermal_state == 2) {
            // 실제 구현에서는 더 정교한 지연 메커니즘 사용
            for (volatile int delay = 0; delay < 100; delay++);
        }
    }
}

/**
 * @brief 음성 합성 특화 Mel 필터뱅크 적용 (NEON 구현)
 *
 * 음성 합성에서 자주 사용되는 Mel 스케일 변환을 NEON으로 최적화
 */
void neon_apply_mel_filterbank(const float* spectrogram, const float* mel_filters,
                               float* mel_output, size_t n_fft, size_t n_mels, size_t n_frames) {
    // 각 Mel 빈에 대해 필터 적용
    for (size_t mel = 0; mel < n_mels; mel++) {
        const float* filter = &mel_filters[mel * n_fft];

        for (size_t frame = 0; frame < n_frames; frame++) {
            const float* spec_frame = &spectrogram[frame * n_fft];
            float sum = 0.0f;

            size_t i = 0;
            float32x4_t vsum = vdupq_n_f32(0.0f);

            // 4개씩 NEON 벡터 처리
            for (; i + 3 < n_fft; i += 4) {
                float32x4_t vspec = vld1q_f32(&spec_frame[i]);
                float32x4_t vfilter = vld1q_f32(&filter[i]);
                vsum = vmlaq_f32(vsum, vspec, vfilter);
            }

            // 벡터 내 요소들 합산
            float32x2_t vsum_low = vget_low_f32(vsum);
            float32x2_t vsum_high = vget_high_f32(vsum);
            float32x2_t vsum_pair = vadd_f32(vsum_low, vsum_high);
            sum = vget_lane_f32(vsum_pair, 0) + vget_lane_f32(vsum_pair, 1);

            // 나머지 요소 처리
            for (; i < n_fft; i++) {
                sum += spec_frame[i] * filter[i];
            }

            mel_output[frame * n_mels + mel] = sum;
        }
    }
}

/**
 * @brief 음성 합성용 윈도우 함수 적용 (NEON 구현)
 *
 * Hann, Hamming 등의 윈도우 함수를 NEON으로 최적화하여 적용
 */
void neon_apply_window(const float* input, const float* window, float* output, size_t size) {
    size_t i = 0;

    // 4개씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(&input[i]);
        float32x4_t vwindow = vld1q_f32(&window[i]);
        float32x4_t vresult = vmulq_f32(vinput, vwindow);
        vst1q_f32(&output[i], vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = input[i] * window[i];
    }
}

/**
 * @brief 복소수 곱셈 (NEON 구현)
 *
 * 음성 처리에서 자주 사용되는 복소수 곱셈을 NEON으로 최적화
 * (a + bi) * (c + di) = (ac - bd) + (ad + bc)i
 */
void neon_complex_mul(const float* a_real, const float* a_imag,
                      const float* b_real, const float* b_imag,
                      float* result_real, float* result_imag, size_t size) {
    size_t i = 0;

    // 4개씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t va_real = vld1q_f32(&a_real[i]);
        float32x4_t va_imag = vld1q_f32(&a_imag[i]);
        float32x4_t vb_real = vld1q_f32(&b_real[i]);
        float32x4_t vb_imag = vld1q_f32(&b_imag[i]);

        // 실수부: ac - bd
        float32x4_t vreal = vmulq_f32(va_real, vb_real);
        vreal = vmlsq_f32(vreal, va_imag, vb_imag); // vreal -= va_imag * vb_imag

        // 허수부: ad + bc
        float32x4_t vimag = vmulq_f32(va_real, vb_imag);
        vimag = vmlaq_f32(vimag, va_imag, vb_real); // vimag += va_imag * vb_real

        vst1q_f32(&result_real[i], vreal);
        vst1q_f32(&result_imag[i], vimag);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        float ac = a_real[i] * b_real[i];
        float bd = a_imag[i] * b_imag[i];
        float ad = a_real[i] * b_imag[i];
        float bc = a_imag[i] * b_real[i];

        result_real[i] = ac - bd;
        result_imag[i] = ad + bc;
    }
}

/**
 * @brief 스펙트럼 크기 계산 (NEON 구현)
 *
 * 복소수 스펙트럼에서 크기를 계산: |z| = sqrt(real² + imag²)
 */
void neon_complex_magnitude(const float* real, const float* imag, float* magnitude, size_t size) {
    size_t i = 0;

    // 4개씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vreal = vld1q_f32(&real[i]);
        float32x4_t vimag = vld1q_f32(&imag[i]);

        // real² + imag²
        float32x4_t vreal_sq = vmulq_f32(vreal, vreal);
        float32x4_t vimag_sq = vmulq_f32(vimag, vimag);
        float32x4_t vmag_sq = vaddq_f32(vreal_sq, vimag_sq);

        // sqrt 근사 (Newton-Raphson 방법)
        float32x4_t vmag = vsqrtq_f32(vmag_sq);

        vst1q_f32(&magnitude[i], vmag);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }
}

/**
 * @brief 로그 스펙트럼 계산 (NEON 구현)
 *
 * 음성 처리에서 자주 사용되는 로그 스펙트럼 계산
 * log_spectrum = log(magnitude + epsilon)
 */
void neon_log_spectrum(const float* magnitude, float* log_spectrum, size_t size, float epsilon) {
    size_t i = 0;
    const float32x4_t vepsilon = vdupq_n_f32(epsilon);

    // 4개씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vmag = vld1q_f32(&magnitude[i]);

        // magnitude + epsilon
        float32x4_t vmag_eps = vaddq_f32(vmag, vepsilon);

        // 빠른 log 근사 (음성 처리에서는 정확도보다 속도가 중요)
        // log(x) ≈ (x - 1) - (x - 1)²/2 + (x - 1)³/3 (x가 1 근처일 때)
        // 더 간단한 근사: log(x) ≈ 2 * (x - 1) / (x + 1) (x > 0)

        // 표준 라이브러리 사용 (NEON에는 직접적인 log 명령어가 없음)
        float log_vals[4];
        vst1q_f32(log_vals, vmag_eps);
        for (int j = 0; j < 4; j++) {
            log_vals[j] = logf(log_vals[j]);
        }
        float32x4_t vlog = vld1q_f32(log_vals);

        vst1q_f32(&log_spectrum[i], vlog);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        log_spectrum[i] = logf(magnitude[i] + epsilon);
    }
}

/**
 * @brief 적응형 성능 조절 벡터 연산
 *
 * 모바일 환경의 동적 주파수 조절(DVFS)에 대응하는 적응형 연산
 * CPU 주파수가 낮을 때는 더 작은 청크로 처리
 */
void neon_vector_add_adaptive(const float* a, const float* b, float* result, size_t size) {
    // 간단한 성능 측정을 통한 적응형 청크 크기 결정
    static size_t adaptive_chunk_size = 128; // 초기값

    size_t i = 0;

    while (i < size) {
        size_t chunk_end = (i + adaptive_chunk_size < size) ? i + adaptive_chunk_size : size;

        // 청크 처리 시작 시간 측정 (간단한 방법)
        clock_t start_time = clock();

        // 청크 내에서 NEON 벡터 처리
        size_t j = i;
        for (; j + 3 < chunk_end; j += 4) {
            float32x4_t va = vld1q_f32(a + j);
            float32x4_t vb = vld1q_f32(b + j);
            float32x4_t vresult = vaddq_f32(va, vb);
            vst1q_f32(result + j, vresult);
        }

        // 나머지 요소 처리
        for (; j < chunk_end; j++) {
            result[j] = a[j] + b[j];
        }

        // 처리 시간 측정 및 적응형 조절
#ifdef LIBETUDE_ENABLE_PROFILING
        clock_t end_time = clock();
        double chunk_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
#else
        // 프로파일링이 비활성화된 경우 기본값 사용
        double chunk_time = 0.0005; // 기본 처리 시간 가정
#endif

        // 처리 시간이 너무 길면 청크 크기 감소, 짧으면 증가
        if (chunk_time > 0.001) { // 1ms 이상이면 청크 크기 감소
            adaptive_chunk_size = (adaptive_chunk_size > 32) ? adaptive_chunk_size / 2 : 32;
        } else if (chunk_time < 0.0005) { // 0.5ms 미만이면 청크 크기 증가
            adaptive_chunk_size = (adaptive_chunk_size < 512) ? adaptive_chunk_size * 2 : 512;
        }

        i = chunk_end;
    }
}

/**
 * @brief 메모리 대역폭 최적화된 GEMM
 *
 * 모바일 환경의 제한된 메모리 대역폭을 고려한 행렬 곱셈
 */
void neon_gemm_memory_optimized(const float* a, const float* b, float* c,
                                size_t m, size_t n, size_t k) {
    // 작은 블록 크기로 캐시 미스 최소화
    const size_t BLOCK_SIZE = 32;

    // 행렬 C를 0으로 초기화
    memset(c, 0, m * n * sizeof(float));

    // 메모리 접근 패턴 최적화
    for (size_t kk = 0; kk < k; kk += BLOCK_SIZE) {
        for (size_t ii = 0; ii < m; ii += BLOCK_SIZE) {
            for (size_t jj = 0; jj < n; jj += BLOCK_SIZE) {
                // 블록 경계 계산
                size_t k_end = (kk + BLOCK_SIZE < k) ? kk + BLOCK_SIZE : k;
                size_t i_end = (ii + BLOCK_SIZE < m) ? ii + BLOCK_SIZE : m;
                size_t j_end = (jj + BLOCK_SIZE < n) ? jj + BLOCK_SIZE : n;

                // 블록 내 행렬 곱셈
                for (size_t i = ii; i < i_end; i++) {
                    for (size_t l = kk; l < k_end; l++) {
                        const float a_val = a[i * k + l];
                        const float32x4_t va = vdupq_n_f32(a_val);

                        size_t j = jj;
                        for (; j + 3 < j_end; j += 4) {
                            float32x4_t vb = vld1q_f32(&b[l * n + j]);
                            float32x4_t vc = vld1q_f32(&c[i * n + j]);
                            float32x4_t vresult = vmlaq_f32(vc, va, vb);
                            vst1q_f32(&c[i * n + j], vresult);
                        }

                        for (; j < j_end; j++) {
                            c[i * n + j] += a[i * k + l] * b[l * n + j];
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief 모바일 특화 피치 시프팅 (NEON 구현)
 *
 * 음성 합성에서 피치 조절을 위한 최적화된 함수
 * 모바일 환경에서 실시간 처리 가능한 구현
 */
void neon_pitch_shift_mobile(const float* input, float* output, size_t size,
                            float pitch_factor) {
    // 피치 시프팅을 위한 간단한 보간 구현
    const float32x4_t vpitch_factor = vdupq_n_f32(pitch_factor);
    const float32x4_t vone = vdupq_n_f32(1.0f);

    size_t i = 0;

    // 4개씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        // 인덱스 계산
        float32x4_t vindices = {(float)i, (float)(i+1), (float)(i+2), (float)(i+3)};
        float32x4_t vsrc_indices = vmulq_f32(vindices, vpitch_factor);

        // 정수 부분과 소수 부분 분리
        int32x4_t vint_indices = vcvtq_s32_f32(vsrc_indices);
        float32x4_t vfrac = vsubq_f32(vsrc_indices, vcvtq_f32_s32(vint_indices));

        // 보간을 위한 값들 로드 (경계 검사 포함)
        float samples[4];
        int32_t indices[4];
        float fracs[4];
        vst1q_s32(indices, vint_indices);
        vst1q_f32(fracs, vfrac);

        for (int j = 0; j < 4; j++) {
            int idx = indices[j];
            if (idx >= 0 && idx < (int)size - 1) {
                float frac = fracs[j];
                samples[j] = input[idx] * (1.0f - frac) + input[idx + 1] * frac;
            } else if (idx >= 0 && idx < (int)size) {
                samples[j] = input[idx];
            } else {
                samples[j] = 0.0f;
            }
        }

        float32x4_t vresult = vld1q_f32(samples);
        vst1q_f32(&output[i], vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        float src_idx = (float)i * pitch_factor;
        int int_idx = (int)src_idx;
        float frac = src_idx - int_idx;

        if (int_idx >= 0 && int_idx < (int)size - 1) {
            output[i] = input[int_idx] * (1.0f - frac) + input[int_idx + 1] * frac;
        } else if (int_idx >= 0 && int_idx < (int)size) {
            output[i] = input[int_idx];
        } else {
            output[i] = 0.0f;
        }
    }
}

/**
 * @brief 모바일 최적화된 스펙트럴 엔벨로프 조정
 *
 * 음성 합성에서 음색 조절을 위한 스펙트럴 엔벨로프 조정
 * 배터리 효율성을 고려한 구현
 */
void neon_spectral_envelope_mobile(const float* magnitude, const float* envelope,
                                  float* output, size_t size) {
    size_t i = 0;

    // 4개씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vmag = vld1q_f32(&magnitude[i]);
        float32x4_t venv = vld1q_f32(&envelope[i]);

        // 스펙트럴 엔벨로프 적용 (곱셈)
        float32x4_t vresult = vmulq_f32(vmag, venv);

        vst1q_f32(&output[i], vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = magnitude[i] * envelope[i];
    }
}

/**
 * @brief 모바일용 실시간 노이즈 게이트
 *
 * 음성 합성 후처리에서 배경 노이즈 제거
 * 저전력으로 실시간 처리 가능한 구현
 */
void neon_noise_gate_mobile(const float* input, float* output, size_t size,
                           float threshold, float ratio) {
    const float32x4_t vthreshold = vdupq_n_f32(threshold);
    const float32x4_t vratio = vdupq_n_f32(ratio);

    size_t i = 0;

    // 4개씩 NEON 벡터 처리
    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(&input[i]);

        // 절댓값 계산
        float32x4_t vabs = vabsq_f32(vinput);

        // 임계값과 비교
        uint32x4_t vmask = vcgtq_f32(vabs, vthreshold);

        // 게이트 적용
        float32x4_t vgated = vmulq_f32(vinput, vratio);
        float32x4_t vresult = vbslq_f32(vmask, vinput, vgated);

        vst1q_f32(&output[i], vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        float abs_val = fabsf(input[i]);
        if (abs_val > threshold) {
            output[i] = input[i];
        } else {
            output[i] = input[i] * ratio;
        }
    }
}

#else
// NEON을 지원하지 않는 경우 CPU 구현으로 대체
extern void cpu_vector_add(const float* a, const float* b, float* result, size_t size);
extern void cpu_vector_mul(const float* a, const float* b, float* result, size_t size);
extern void cpu_vector_scale(const float* input, float scale, float* result, size_t size);
extern float cpu_vector_dot(const float* a, const float* b, size_t size);
extern void cpu_gemm(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);
extern void cpu_relu(const float* input, float* output, size_t size);
extern void cpu_sigmoid(const float* input, float* output, size_t size);
extern void cpu_tanh(const float* input, float* output, size_t size);
extern void cpu_gelu(const float* input, float* output, size_t size);

// CPU fallback 구현들
static void cpu_apply_mel_filterbank(const float* spectrogram, const float* mel_filters,
                                     float* mel_output, size_t n_fft, size_t n_mels, size_t n_frames) {
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

static void cpu_apply_window(const float* input, const float* window, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = input[i] * window[i];
    }
}

static void cpu_complex_mul(const float* a_real, const float* a_imag,
                            const float* b_real, const float* b_imag,
                            float* result_real, float* result_imag, size_t size) {
    for (size_t i = 0; i < size; i++) {
        float ac = a_real[i] * b_real[i];
        float bd = a_imag[i] * b_imag[i];
        float ad = a_real[i] * b_imag[i];
        float bc = a_imag[i] * b_real[i];
        result_real[i] = ac - bd;
        result_imag[i] = ad + bc;
    }
}

static void cpu_complex_magnitude(const float* real, const float* imag, float* magnitude, size_t size) {
    for (size_t i = 0; i < size; i++) {
        magnitude[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]);
    }
}

static void cpu_log_spectrum(const float* magnitude, float* log_spectrum, size_t size, float epsilon) {
    for (size_t i = 0; i < size; i++) {
        log_spectrum[i] = logf(magnitude[i] + epsilon);
    }
}

// CPU fallback 함수들 추가
static void cpu_gemm_low_power(const float* a, const float* b, float* c, size_t m, size_t n, size_t k) {
    cpu_gemm(a, b, c, m, n, k); // 기본 GEMM 사용
}

static void cpu_vector_add_thermal_aware(const float* a, const float* b, float* result, size_t size) {
    cpu_vector_add(a, b, result, size); // 기본 벡터 덧셈 사용
}

static void cpu_pitch_shift_mobile(const float* input, float* output, size_t size, float pitch_factor) {
    for (size_t i = 0; i < size; i++) {
        float src_idx = (float)i * pitch_factor;
        int int_idx = (int)src_idx;
        float frac = src_idx - int_idx;

        if (int_idx >= 0 && int_idx < (int)size - 1) {
            output[i] = input[int_idx] * (1.0f - frac) + input[int_idx + 1] * frac;
        } else if (int_idx >= 0 && int_idx < (int)size) {
            output[i] = input[int_idx];
        } else {
            output[i] = 0.0f;
        }
    }
}

static void cpu_spectral_envelope_mobile(const float* magnitude, const float* envelope, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = magnitude[i] * envelope[i];
    }
}

static void cpu_noise_gate_mobile(const float* input, float* output, size_t size, float threshold, float ratio) {
    for (size_t i = 0; i < size; i++) {
        float abs_val = fabsf(input[i]);
        if (abs_val > threshold) {
            output[i] = input[i];
        } else {
            output[i] = input[i] * ratio;
        }
    }
}

#define neon_vector_add cpu_vector_add
#define neon_vector_mul cpu_vector_mul
#define neon_vector_scale cpu_vector_scale
#define neon_vector_dot cpu_vector_dot
#define neon_gemm cpu_gemm
#define neon_gemm_blocked cpu_gemm
#define neon_relu cpu_relu
#define neon_sigmoid cpu_sigmoid
#define neon_tanh cpu_tanh
#define neon_gelu cpu_gelu
#define neon_vector_add_power_efficient cpu_vector_add
#define neon_gemm_memory_optimized cpu_gemm
#define neon_apply_mel_filterbank cpu_apply_mel_filterbank
#define neon_apply_window cpu_apply_window
#define neon_complex_mul cpu_complex_mul
#define neon_complex_magnitude cpu_complex_magnitude
#define neon_log_spectrum cpu_log_spectrum
#define neon_vector_add_adaptive cpu_vector_add
#define neon_gemm_low_power cpu_gemm_low_power
#define neon_vector_add_thermal_aware cpu_vector_add_thermal_aware
#define neon_pitch_shift_mobile cpu_pitch_shift_mobile
#define neon_spectral_envelope_mobile cpu_spectral_envelope_mobile
#define neon_noise_gate_mobile cpu_noise_gate_mobile
#endif

// ============================================================================
// NEON 커널 등록 함수
// ============================================================================

/**
 * @brief NEON 커널들을 등록합니다
 *
 * 모바일 환경에 최적화된 성능 점수와 최적 크기를 설정합니다.
 */
void register_neon_kernels(void) {
#ifdef LIBETUDE_HAVE_NEON
    KernelInfo kernel_info;

    // 벡터 연산 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_neon";
    kernel_info.kernel_func = (void*)neon_vector_add;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64; // 모바일 환경에 적합한 크기
    kernel_info.performance_score = 2.8f; // NEON은 모바일에서 높은 효율성
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_mul_neon";
    kernel_info.kernel_func = (void*)neon_vector_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.8f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_scale_neon";
    kernel_info.kernel_func = (void*)neon_vector_scale;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.8f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_dot_neon";
    kernel_info.kernel_func = (void*)neon_vector_dot;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 3.0f; // 내적은 NEON에서 특히 효율적
    kernel_registry_register(&kernel_info);

    // GEMM 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "gemm_neon";
    kernel_info.kernel_func = (void*)neon_gemm;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 32; // 모바일 환경의 작은 행렬에 최적화
    kernel_info.performance_score = 2.5f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "gemm_blocked_neon";
    kernel_info.kernel_func = (void*)neon_gemm_blocked;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128; // 큰 행렬에 대한 블록 처리
    kernel_info.performance_score = 2.7f;
    kernel_registry_register(&kernel_info);

    // 활성화 함수 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_relu_neon";
    kernel_info.kernel_func = (void*)neon_relu;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 3.0f; // ReLU는 NEON에서 매우 효율적
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_sigmoid_neon";
    kernel_info.kernel_func = (void*)neon_sigmoid;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.3f; // 근사 함수 사용으로 높은 성능
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_tanh_neon";
    kernel_info.kernel_func = (void*)neon_tanh;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.3f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_gelu_neon";
    kernel_info.kernel_func = (void*)neon_gelu;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.1f; // 복잡한 함수이지만 NEON 최적화로 향상
    kernel_registry_register(&kernel_info);

    // 모바일 특화 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_power_efficient_neon";
    kernel_info.kernel_func = (void*)neon_vector_add_power_efficient;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 32; // 배터리 효율성 우선
    kernel_info.performance_score = 2.2f; // 성능보다 효율성 중시
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "gemm_memory_optimized_neon";
    kernel_info.kernel_func = (void*)neon_gemm_memory_optimized;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.6f; // 메모리 효율성 최적화
    kernel_registry_register(&kernel_info);

    // 음성 합성 특화 DSP 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "mel_filterbank_neon";
    kernel_info.kernel_func = (void*)neon_apply_mel_filterbank;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 256; // Mel 필터뱅크에 적합한 크기
    kernel_info.performance_score = 2.9f; // 음성 처리에서 높은 효율성
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "window_function_neon";
    kernel_info.kernel_func = (void*)neon_apply_window;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 3.1f; // 간단한 곱셈이므로 높은 성능
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "complex_multiply_neon";
    kernel_info.kernel_func = (void*)neon_complex_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.7f; // 복소수 연산 최적화
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "complex_magnitude_neon";
    kernel_info.kernel_func = (void*)neon_complex_magnitude;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.5f; // sqrt 연산 포함
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "log_spectrum_neon";
    kernel_info.kernel_func = (void*)neon_log_spectrum;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.0f; // log 연산으로 인한 성능 저하
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_adaptive_neon";
    kernel_info.kernel_func = (void*)neon_vector_add_adaptive;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128; // 적응형 크기 조절
    kernel_info.performance_score = 2.4f; // 적응형 오버헤드 고려
    kernel_registry_register(&kernel_info);

    // 모바일 특화 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "gemm_low_power_neon";
    kernel_info.kernel_func = (void*)neon_gemm_low_power;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 16; // 저전력 모드
    kernel_info.performance_score = 1.8f; // 전력 효율성 우선
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_thermal_aware_neon";
    kernel_info.kernel_func = (void*)neon_vector_add_thermal_aware;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64; // 온도 적응형
    kernel_info.performance_score = 2.3f; // 온도 관리 오버헤드 고려
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "pitch_shift_mobile_neon";
    kernel_info.kernel_func = (void*)neon_pitch_shift_mobile;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 256; // 피치 시프팅에 적합한 크기
    kernel_info.performance_score = 2.2f; // 보간 연산 포함
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "spectral_envelope_mobile_neon";
    kernel_info.kernel_func = (void*)neon_spectral_envelope_mobile;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.9f; // 간단한 곱셈 연산
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "noise_gate_mobile_neon";
    kernel_info.kernel_func = (void*)neon_noise_gate_mobile;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 64;
    kernel_info.performance_score = 2.6f; // 조건부 연산 포함
    kernel_registry_register(&kernel_info);

    // 소프트맥스 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "softmax_neon";
    kernel_info.kernel_func = (void*)neon_softmax;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.0f;
    kernel_registry_register(&kernel_info);

    // 레이어 정규화 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "layer_norm_neon";
    kernel_info.kernel_func = (void*)neon_layer_norm;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.2f;
    kernel_registry_register(&kernel_info);

    // 배치 정규화 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "batch_norm_neon";
    kernel_info.kernel_func = (void*)neon_batch_norm;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.3f;
    kernel_registry_register(&kernel_info);
#endif
}