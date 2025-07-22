/**
 * @file sse_kernels.c
 * @brief SSE SIMD 커널 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/kernel_registry.h"
#include <string.h>
#include <math.h>

#ifdef LIBETUDE_HAVE_SSE2
#include <emmintrin.h> // SSE2
#endif

#ifdef LIBETUDE_HAVE_SSE4_1
#include <smmintrin.h> // SSE4.1
#endif

// ============================================================================
// SSE 벡터 연산 커널
// ============================================================================

#ifdef LIBETUDE_HAVE_SSE2
/**
 * @brief 벡터 덧셈 (SSE 구현)
 */
void sse_vector_add(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;

    // 4개 요소씩 SSE 벡터 처리
    for (; i + 3 < size; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        __m128 vresult = _mm_add_ps(va, vb);
        _mm_storeu_ps(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = a[i] + b[i];
    }
}

/**
 * @brief 벡터 곱셈 (SSE 구현)
 */
void sse_vector_mul(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;

    // 4개 요소씩 SSE 벡터 처리
    for (; i + 3 < size; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        __m128 vresult = _mm_mul_ps(va, vb);
        _mm_storeu_ps(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = a[i] * b[i];
    }
}

/**
 * @brief 벡터 스칼라 곱셈 (SSE 구현)
 */
void sse_vector_scale(const float* input, float scale, float* result, size_t size) {
    size_t i = 0;
    __m128 vscale = _mm_set1_ps(scale);

    // 4개 요소씩 SSE 벡터 처리
    for (; i + 3 < size; i += 4) {
        __m128 vinput = _mm_loadu_ps(input + i);
        __m128 vresult = _mm_mul_ps(vinput, vscale);
        _mm_storeu_ps(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = input[i] * scale;
    }
}

/**
 * @brief 벡터 내적 (SSE 구현)
 */
float sse_vector_dot(const float* a, const float* b, size_t size) {
    size_t i = 0;
    __m128 vsum = _mm_setzero_ps();

    // 4개 요소씩 SSE 벡터 처리
    for (; i + 3 < size; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        __m128 vmul = _mm_mul_ps(va, vb);
        vsum = _mm_add_ps(vsum, vmul);
    }

    // 벡터 내 요소들을 합산
    float result[4];
    _mm_storeu_ps(result, vsum);
    float sum = result[0] + result[1] + result[2] + result[3];

    // 나머지 요소 처리
    for (; i < size; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

/**
 * @brief 간단한 GEMM (General Matrix Multiply) - SSE 구현
 * A: m x k, B: k x n, C: m x n
 */
void sse_gemm(const float* a, const float* b, float* c,
              size_t m, size_t n, size_t k) {
    // 행렬 C를 0으로 초기화
    memset(c, 0, m * n * sizeof(float));

    // 행렬 곱셈 수행
    for (size_t i = 0; i < m; i++) {
        for (size_t l = 0; l < k; l++) {
            const float a_val = a[i * k + l];
            const __m128 va = _mm_set1_ps(a_val);

            size_t j = 0;
            // 4개 요소씩 SSE 벡터 처리
            for (; j + 3 < n; j += 4) {
                __m128 vb = _mm_loadu_ps(&b[l * n + j]);
                __m128 vc = _mm_loadu_ps(&c[i * n + j]);
                __m128 vresult = _mm_add_ps(_mm_mul_ps(va, vb), vc);
                _mm_storeu_ps(&c[i * n + j], vresult);
            }

            // 나머지 요소 처리
            for (; j < n; j++) {
                c[i * n + j] += a[i * k + l] * b[l * n + j];
            }
        }
    }
}

// ============================================================================
// SSE 활성화 함수 커널
// ============================================================================

/**
 * @brief ReLU 활성화 함수 (SSE 구현)
 */
void sse_relu(const float* input, float* output, size_t size) {
    size_t i = 0;
    __m128 vzero = _mm_setzero_ps();

    // 4개 요소씩 SSE 벡터 처리
    for (; i + 3 < size; i += 4) {
        __m128 vinput = _mm_loadu_ps(input + i);
        __m128 vresult = _mm_max_ps(vinput, vzero);
        _mm_storeu_ps(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }
}

/**
 * @brief 빠른 exp 근사 함수 (SSE용)
 *
 * 빠른 exp 근사를 위한 다항식 근사
 * exp(x) ≈ 1 + x + x²/2 + x³/6 + x⁴/24 (테일러 급수 근사)
 */
static inline __m128 sse_fast_exp(__m128 x) {
    // 입력 범위 제한 (-10 ~ 10)
    const __m128 vmin = _mm_set1_ps(-10.0f);
    const __m128 vmax = _mm_set1_ps(10.0f);
    x = _mm_max_ps(_mm_min_ps(x, vmax), vmin);

    // 테일러 급수 계수들
    const __m128 c1 = _mm_set1_ps(1.0f);
    const __m128 c2 = _mm_set1_ps(1.0f);
    const __m128 c3 = _mm_set1_ps(0.5f);        // 1/2!
    const __m128 c4 = _mm_set1_ps(0.16666667f); // 1/3!
    const __m128 c5 = _mm_set1_ps(0.04166667f); // 1/4!

    // x의 거듭제곱 계산
    __m128 x2 = _mm_mul_ps(x, x);
    __m128 x3 = _mm_mul_ps(x2, x);
    __m128 x4 = _mm_mul_ps(x3, x);

    // 다항식 계산: 1 + x + x²/2 + x³/6 + x⁴/24
    __m128 result = c1;
    result = _mm_add_ps(result, _mm_mul_ps(c2, x));
    result = _mm_add_ps(result, _mm_mul_ps(c3, x2));
    result = _mm_add_ps(result, _mm_mul_ps(c4, x3));
    result = _mm_add_ps(result, _mm_mul_ps(c5, x4));

    return result;
}

/**
 * @brief 빠른 tanh 근사 함수 (SSE용)
 *
 * tanh(x) ≈ x * (27 + x²) / (27 + 9*x²) (Padé 근사)
 */
static inline __m128 sse_fast_tanh(__m128 x) {
    // 입력 범위 제한
    const __m128 vmin = _mm_set1_ps(-5.0f);
    const __m128 vmax = _mm_set1_ps(5.0f);
    x = _mm_max_ps(_mm_min_ps(x, vmax), vmin);

    const __m128 c27 = _mm_set1_ps(27.0f);
    const __m128 c9 = _mm_set1_ps(9.0f);

    __m128 x2 = _mm_mul_ps(x, x);

    // 분자: x * (27 + x²)
    __m128 numerator = _mm_mul_ps(x, _mm_add_ps(c27, x2));

    // 분모: 27 + 9*x²
    __m128 denominator = _mm_add_ps(c27, _mm_mul_ps(c9, x2));

    return _mm_div_ps(numerator, denominator);
}

/**
 * @brief Sigmoid 활성화 함수 (SSE 구현)
 * sigmoid(x) = 1 / (1 + exp(-x))
 */
void sse_sigmoid(const float* input, float* output, size_t size) {
    size_t i = 0;
    const __m128 vone = _mm_set1_ps(1.0f);
    const __m128 vneg_one = _mm_set1_ps(-1.0f);

    // 4개 요소씩 SSE 벡터 처리
    for (; i + 3 < size; i += 4) {
        __m128 vinput = _mm_loadu_ps(input + i);
        __m128 vneg_input = _mm_mul_ps(vinput, vneg_one);

        // 빠른 exp(-x) 근사 계산
        __m128 vexp_neg_x = sse_fast_exp(vneg_input);

        // 1 + exp(-x)
        __m128 vdenom = _mm_add_ps(vone, vexp_neg_x);

        // 1 / (1 + exp(-x))
        __m128 vresult = _mm_div_ps(vone, vdenom);
        _mm_storeu_ps(output + i, vresult);
    }

    // 나머지 요소 처리 (표준 라이브러리 사용)
    for (; i < size; i++) {
        output[i] = 1.0f / (1.0f + expf(-input[i]));
    }
}

/**
 * @brief Tanh 활성화 함수 (SSE 구현)
 */
void sse_tanh(const float* input, float* output, size_t size) {
    size_t i = 0;

    // 4개 요소씩 SSE 벡터 처리
    for (; i + 3 < size; i += 4) {
        __m128 vinput = _mm_loadu_ps(input + i);
        __m128 vresult = sse_fast_tanh(vinput);
        _mm_storeu_ps(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = tanhf(input[i]);
    }
}

/**
 * @brief GELU 활성화 함수 (SSE 구현)
 * GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x^3)))
 */
void sse_gelu(const float* input, float* output, size_t size) {
    size_t i = 0;
    const float sqrt_2_over_pi = 0.7978845608f; // sqrt(2/π)
    const float coeff = 0.044715f;

    // GELU는 복잡한 함수이므로 스칼라 구현 사용
    for (; i < size; i++) {
        float x = input[i];
        float x3 = x * x * x;
        float inner = sqrt_2_over_pi * (x + coeff * x3);
        output[i] = 0.5f * x * (1.0f + tanhf(inner));
    }
}

#else
// SSE를 지원하지 않는 경우 CPU 구현으로 대체
extern void cpu_vector_add(const float* a, const float* b, float* result, size_t size);
extern void cpu_vector_mul(const float* a, const float* b, float* result, size_t size);
extern void cpu_vector_scale(const float* input, float scale, float* result, size_t size);
extern void cpu_relu(const float* input, float* output, size_t size);

#define sse_vector_add cpu_vector_add
#define sse_vector_mul cpu_vector_mul
#define sse_vector_scale cpu_vector_scale
#define sse_relu cpu_relu
#endif

// ============================================================================
// SSE 커널 등록 함수
// ============================================================================

/**
 * @brief SSE 커널들을 등록합니다
 */
void register_sse_kernels(void) {
#ifdef LIBETUDE_HAVE_SSE2
    // 벡터 연산 커널 등록
    KernelInfo kernel_info;

    // 벡터 덧셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_sse";
    kernel_info.kernel_func = (void*)sse_vector_add;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128; // 128개 이상의 요소에서 최적화 효과
    kernel_info.performance_score = 2.5f; // CPU 대비 예상 성능 점수
    kernel_registry_register(&kernel_info);

    // 벡터 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_mul_sse";
    kernel_info.kernel_func = (void*)sse_vector_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.5f;
    kernel_registry_register(&kernel_info);

    // 벡터 스칼라 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_scale_sse";
    kernel_info.kernel_func = (void*)sse_vector_scale;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.5f;
    kernel_registry_register(&kernel_info);

    // 벡터 내적 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_dot_sse";
    kernel_info.kernel_func = (void*)sse_vector_dot;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.5f;
    kernel_registry_register(&kernel_info);

    // GEMM 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "gemm_sse";
    kernel_info.kernel_func = (void*)sse_gemm;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 64; // 행렬 크기 기준
    kernel_info.performance_score = 2.0f;
    kernel_registry_register(&kernel_info);

    // 활성화 함수 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_relu_sse";
    kernel_info.kernel_func = (void*)sse_relu;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.5f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_sigmoid_sse";
    kernel_info.kernel_func = (void*)sse_sigmoid;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_tanh_sse";
    kernel_info.kernel_func = (void*)sse_tanh;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_gelu_sse";
    kernel_info.kernel_func = (void*)sse_gelu;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 1.8f;
    kernel_registry_register(&kernel_info);
#endif
}