/**
 * @file avx_kernels.c
 * @brief AVX SIMD 커널 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/kernel_registry.h"
#include <string.h>
#include <math.h>

#ifdef LIBETUDE_HAVE_AVX
#include <immintrin.h> // AVX
#endif

// ============================================================================
// AVX 벡터 연산 커널
// ============================================================================

#ifdef LIBETUDE_HAVE_AVX
/**
 * @brief 벡터 덧셈 (AVX 구현)
 */
void avx_vector_add(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vresult = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = a[i] + b[i];
    }
}

/**
 * @brief 벡터 곱셈 (AVX 구현)
 */
void avx_vector_mul(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vresult = _mm256_mul_ps(va, vb);
        _mm256_storeu_ps(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = a[i] * b[i];
    }
}

/**
 * @brief 벡터 스칼라 곱셈 (AVX 구현)
 */
void avx_vector_scale(const float* input, float scale, float* result, size_t size) {
    size_t i = 0;
    __m256 vscale = _mm256_set1_ps(scale);

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        __m256 vresult = _mm256_mul_ps(vinput, vscale);
        _mm256_storeu_ps(result + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        result[i] = input[i] * scale;
    }
}

// ============================================================================
// AVX 활성화 함수 커널
// ============================================================================

/**
 * @brief ReLU 활성화 함수 (AVX 구현)
 */
void avx_relu(const float* input, float* output, size_t size) {
    size_t i = 0;
    __m256 vzero = _mm256_setzero_ps();

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        __m256 vresult = _mm256_max_ps(vinput, vzero);
        _mm256_storeu_ps(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }
}

/**
 * @brief 행렬 곱셈 (AVX 구현)
 *
 * 간단한 AVX 최적화 버전의 행렬 곱셈
 * A: m x k, B: k x n, C: m x n
 */
void avx_matrix_mul(const float* a, const float* b, float* c,
                   size_t m, size_t n, size_t k) {
    // 행렬 C를 0으로 초기화
    memset(c, 0, m * n * sizeof(float));

    // 행렬 곱셈 수행
    for (size_t i = 0; i < m; i++) {
        for (size_t l = 0; l < k; l++) {
            const float a_val = a[i * k + l];
            const __m256 va = _mm256_set1_ps(a_val);

            size_t j = 0;
            // 8개 요소씩 AVX 벡터 처리
            for (; j + 7 < n; j += 8) {
                __m256 vb = _mm256_loadu_ps(&b[l * n + j]);
                __m256 vc = _mm256_loadu_ps(&c[i * n + j]);
                __m256 vresult = _mm256_fmadd_ps(va, vb, vc); // AVX2에서만 사용 가능
                _mm256_storeu_ps(&c[i * n + j], vresult);
            }

            // 나머지 요소 처리
            for (; j < n; j++) {
                c[i * n + j] += a[i * k + l] * b[l * n + j];
            }
        }
    }
}

#else
// AVX를 지원하지 않는 경우 SSE 또는 CPU 구현으로 대체
extern void sse_vector_add(const float* a, const float* b, float* result, size_t size);
extern void sse_vector_mul(const float* a, const float* b, float* result, size_t size);
extern void sse_vector_scale(const float* input, float scale, float* result, size_t size);
extern void sse_relu(const float* input, float* output, size_t size);
extern void cpu_matrix_mul(const float* a, const float* b, float* c, size_t m, size_t n, size_t k);

#define avx_vector_add sse_vector_add
#define avx_vector_mul sse_vector_mul
#define avx_vector_scale sse_vector_scale
#define avx_relu sse_relu
#define avx_matrix_mul cpu_matrix_mul
#endif

// ============================================================================
// AVX 커널 등록 함수
// ============================================================================

/**
 * @brief AVX 커널들을 등록합니다
 */
void register_avx_kernels(void) {
#ifdef LIBETUDE_HAVE_AVX
    // 벡터 연산 커널 등록
    KernelInfo kernel_info;

    // 벡터 덧셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_avx";
    kernel_info.kernel_func = (void*)avx_vector_add;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256; // 256개 이상의 요소에서 최적화 효과
    kernel_info.performance_score = 4.0f; // CPU 대비 예상 성능 점수
    kernel_registry_register(&kernel_info);

    // 벡터 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_mul_avx";
    kernel_info.kernel_func = (void*)avx_vector_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 4.0f;
    kernel_registry_register(&kernel_info);

    // 벡터 스칼라 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_scale_avx";
    kernel_info.kernel_func = (void*)avx_vector_scale;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 4.0f;
    kernel_registry_register(&kernel_info);

    // 활성화 함수 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_relu_avx";
    kernel_info.kernel_func = (void*)avx_relu;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 4.0f;
    kernel_registry_register(&kernel_info);

    // 행렬 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "matmul_avx";
    kernel_info.kernel_func = (void*)avx_matrix_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 128; // 행렬 크기 기준
    kernel_info.performance_score = 4.0f;
    kernel_registry_register(&kernel_info);
#endif
}