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

    // 활성화 함수 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_relu_sse";
    kernel_info.kernel_func = (void*)sse_relu;
    kernel_info.simd_features = LIBETUDE_SIMD_SSE2;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.5f;
    kernel_registry_register(&kernel_info);
#endif
}