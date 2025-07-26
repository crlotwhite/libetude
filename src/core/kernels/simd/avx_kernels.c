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
 * @brief 빠른 exp 근사 함수 (AVX용)
 */
static inline __m256 avx_fast_exp(__m256 x) {
    // 입력 범위 제한 (-10 ~ 10)
    const __m256 vmin = _mm256_set1_ps(-10.0f);
    const __m256 vmax = _mm256_set1_ps(10.0f);
    x = _mm256_max_ps(_mm256_min_ps(x, vmax), vmin);

    // 테일러 급수 계수들
    const __m256 c1 = _mm256_set1_ps(1.0f);
    const __m256 c2 = _mm256_set1_ps(1.0f);
    const __m256 c3 = _mm256_set1_ps(0.5f);        // 1/2!
    const __m256 c4 = _mm256_set1_ps(0.16666667f); // 1/3!
    const __m256 c5 = _mm256_set1_ps(0.04166667f); // 1/4!

    // x의 거듭제곱 계산
    __m256 x2 = _mm256_mul_ps(x, x);
    __m256 x3 = _mm256_mul_ps(x2, x);
    __m256 x4 = _mm256_mul_ps(x3, x);

    // 다항식 계산: 1 + x + x²/2 + x³/6 + x⁴/24
    __m256 result = c1;
#ifdef LIBETUDE_HAVE_AVX2
    result = _mm256_fmadd_ps(c2, x, result);
    result = _mm256_fmadd_ps(c3, x2, result);
    result = _mm256_fmadd_ps(c4, x3, result);
    result = _mm256_fmadd_ps(c5, x4, result);
#else
    result = _mm256_add_ps(result, _mm256_mul_ps(c2, x));
    result = _mm256_add_ps(result, _mm256_mul_ps(c3, x2));
    result = _mm256_add_ps(result, _mm256_mul_ps(c4, x3));
    result = _mm256_add_ps(result, _mm256_mul_ps(c5, x4));
#endif

    return result;
}

/**
 * @brief 빠른 tanh 근사 함수 (AVX용)
 */
static inline __m256 avx_fast_tanh(__m256 x) {
    // 입력 범위 제한
    const __m256 vmin = _mm256_set1_ps(-5.0f);
    const __m256 vmax = _mm256_set1_ps(5.0f);
    x = _mm256_max_ps(_mm256_min_ps(x, vmax), vmin);

    const __m256 c27 = _mm256_set1_ps(27.0f);
    const __m256 c9 = _mm256_set1_ps(9.0f);

    __m256 x2 = _mm256_mul_ps(x, x);

    // 분자: x * (27 + x²)
    __m256 numerator = _mm256_mul_ps(x, _mm256_add_ps(c27, x2));

    // 분모: 27 + 9*x²
#ifdef LIBETUDE_HAVE_AVX2
    __m256 denominator = _mm256_fmadd_ps(c9, x2, c27);
#else
    __m256 denominator = _mm256_add_ps(c27, _mm256_mul_ps(c9, x2));
#endif

    return _mm256_div_ps(numerator, denominator);
}

/**
 * @brief Sigmoid 활성화 함수 (AVX 구현)
 */
void avx_sigmoid(const float* input, float* output, size_t size) {
    size_t i = 0;
    const __m256 vone = _mm256_set1_ps(1.0f);
    const __m256 vneg_one = _mm256_set1_ps(-1.0f);

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        __m256 vneg_input = _mm256_mul_ps(vinput, vneg_one);

        // 빠른 exp(-x) 근사 계산
        __m256 vexp_neg_x = avx_fast_exp(vneg_input);

        // 1 + exp(-x)
        __m256 vdenom = _mm256_add_ps(vone, vexp_neg_x);

        // 1 / (1 + exp(-x))
        __m256 vresult = _mm256_div_ps(vone, vdenom);
        _mm256_storeu_ps(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = 1.0f / (1.0f + expf(-input[i]));
    }
}

/**
 * @brief Tanh 활성화 함수 (AVX 구현)
 */
void avx_tanh(const float* input, float* output, size_t size) {
    size_t i = 0;

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        __m256 vresult = avx_fast_tanh(vinput);
        _mm256_storeu_ps(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = tanhf(input[i]);
    }
}

/**
 * @brief GELU 활성화 함수 (AVX 구현)
 *
 * GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x^3)))
 */
void avx_gelu(const float* input, float* output, size_t size) {
    size_t i = 0;
    const float sqrt_2_over_pi = 0.7978845608f;
    const float coeff = 0.044715f;

    // AVX 상수 설정
    const __m256 vhalf = _mm256_set1_ps(0.5f);
    const __m256 vone = _mm256_set1_ps(1.0f);
    const __m256 vsqrt_2_over_pi = _mm256_set1_ps(sqrt_2_over_pi);
    const __m256 vcoeff = _mm256_set1_ps(coeff);

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        // 입력 로드
        __m256 vx = _mm256_loadu_ps(input + i);

        // x^2 계산
        __m256 vx2 = _mm256_mul_ps(vx, vx);

        // x^3 계산
        __m256 vx3 = _mm256_mul_ps(vx2, vx);

        // coeff * x^3 계산
        __m256 vcoeff_x3 = _mm256_mul_ps(vcoeff, vx3);

        // x + coeff * x^3 계산
        __m256 vsum = _mm256_add_ps(vx, vcoeff_x3);

        // sqrt(2/π) * (x + coeff * x^3) 계산
        __m256 vinner = _mm256_mul_ps(vsqrt_2_over_pi, vsum);

        // tanh(inner) 계산 - 최적화된 tanh 함수 사용
        __m256 vtanh = avx_fast_tanh(vinner);

        // 1 + tanh(inner) 계산
        __m256 vone_plus_tanh = _mm256_add_ps(vone, vtanh);

        // 0.5 * x * (1 + tanh(inner)) 계산
        __m256 vresult = _mm256_mul_ps(vhalf, _mm256_mul_ps(vx, vone_plus_tanh));

        // 결과 저장
        _mm256_storeu_ps(output + i, vresult);
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
 * @brief 소프트맥스 함수 (AVX 구현)
 */
void avx_softmax(const float* input, float* output, size_t size) {
    size_t i = 0;

    // 1. 최댓값 찾기 (수치 안정성을 위해)
    float max_val = input[0];
    for (i = 1; i < size; i++) {
        if (input[i] > max_val) {
            max_val = input[i];
        }
    }

    __m256 vmax = _mm256_set1_ps(max_val);
    __m256 vsum = _mm256_setzero_ps();

    // 2. exp(x - max) 계산 및 합계 구하기 (AVX 벡터화)
    i = 0;
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        __m256 vshifted = _mm256_sub_ps(vinput, vmax);

        // 빠른 exp 근사 사용
        __m256 vexp = avx_fast_exp(vshifted);
        _mm256_storeu_ps(output + i, vexp);
        vsum = _mm256_add_ps(vsum, vexp);
    }

    // 벡터 내 요소들을 합산
    __m128 vlow = _mm256_castps256_ps128(vsum);
    __m128 vhigh = _mm256_extractf128_ps(vsum, 1);
    __m128 vsum_128 = _mm_add_ps(vlow, vhigh);

    float sum_array[4];
    _mm_storeu_ps(sum_array, vsum_128);
    float sum = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3];

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }

    // 3. 정규화 (AVX 벡터화)
    __m256 vinv_sum = _mm256_set1_ps(1.0f / sum);
    i = 0;
    for (; i + 7 < size; i += 8) {
        __m256 voutput = _mm256_loadu_ps(output + i);
        __m256 vnormalized = _mm256_mul_ps(voutput, vinv_sum);
        _mm256_storeu_ps(output + i, vnormalized);
    }

    // 나머지 요소 처리
    float inv_sum = 1.0f / sum;
    for (; i < size; i++) {
        output[i] *= inv_sum;
    }
}

/**
 * @brief 레이어 정규화 함수 (AVX 구현)
 */
void avx_layer_norm(const float* input, float* output, size_t size, float epsilon) {
    size_t i = 0;

    // 1. 평균 계산 (AVX 벡터화)
    __m256 vsum = _mm256_setzero_ps();
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        vsum = _mm256_add_ps(vsum, vinput);
    }

    // 벡터 내 요소들을 합산
    __m128 vlow = _mm256_castps256_ps128(vsum);
    __m128 vhigh = _mm256_extractf128_ps(vsum, 1);
    __m128 vsum_128 = _mm_add_ps(vlow, vhigh);

    float sum_array[4];
    _mm_storeu_ps(sum_array, vsum_128);
    float sum = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3];

    // 나머지 요소 처리
    for (; i < size; i++) {
        sum += input[i];
    }

    float mean = sum / (float)size;
    __m256 vmean = _mm256_set1_ps(mean);

    // 2. 분산 계산 (AVX 벡터화)
    __m256 vvar_sum = _mm256_setzero_ps();
    i = 0;
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        __m256 vdiff = _mm256_sub_ps(vinput, vmean);
        __m256 vdiff_sq = _mm256_mul_ps(vdiff, vdiff);
        vvar_sum = _mm256_add_ps(vvar_sum, vdiff_sq);
    }

    // 벡터 내 요소들을 합산
    vlow = _mm256_castps256_ps128(vvar_sum);
    vhigh = _mm256_extractf128_ps(vvar_sum, 1);
    vsum_128 = _mm_add_ps(vlow, vhigh);

    _mm_storeu_ps(sum_array, vsum_128);
    float var_sum = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3];

    // 나머지 요소 처리
    for (; i < size; i++) {
        float diff = input[i] - mean;
        var_sum += diff * diff;
    }

    float variance = var_sum / (float)size;
    float inv_std = 1.0f / sqrtf(variance + epsilon);
    __m256 vinv_std = _mm256_set1_ps(inv_std);

    // 3. 정규화 (AVX 벡터화)
    i = 0;
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);
        __m256 vdiff = _mm256_sub_ps(vinput, vmean);
        __m256 vnormalized = _mm256_mul_ps(vdiff, vinv_std);
        _mm256_storeu_ps(output + i, vnormalized);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = (input[i] - mean) * inv_std;
    }
}

/**
 * @brief 배치 정규화 함수 (AVX 구현)
 */
void avx_batch_norm(const float* input, float* output, size_t size,
                   float mean, float variance, float gamma, float beta, float epsilon) {
    size_t i = 0;
    float inv_std = 1.0f / sqrtf(variance + epsilon);

    __m256 vmean = _mm256_set1_ps(mean);
    __m256 vinv_std = _mm256_set1_ps(inv_std);
    __m256 vgamma = _mm256_set1_ps(gamma);
    __m256 vbeta = _mm256_set1_ps(beta);

    // AVX 벡터화된 배치 정규화
    for (; i + 7 < size; i += 8) {
        __m256 vinput = _mm256_loadu_ps(input + i);

        // (input - mean) * inv_std
        __m256 vnormalized = _mm256_mul_ps(_mm256_sub_ps(vinput, vmean), vinv_std);

        // gamma * normalized + beta
#ifdef LIBETUDE_HAVE_AVX2
        __m256 vresult = _mm256_fmadd_ps(vgamma, vnormalized, vbeta);
#else
        __m256 vresult = _mm256_add_ps(_mm256_mul_ps(vgamma, vnormalized), vbeta);
#endif

        _mm256_storeu_ps(output + i, vresult);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        output[i] = gamma * (input[i] - mean) * inv_std + beta;
    }
}

/**
 * @brief 벡터 내적 (AVX 구현)
 *
 * 이 구현은 다음과 같은 최적화 기법을 사용합니다:
 * 1. AVX 명령어를 사용한 8개 요소 병렬 처리
 * 2. 여러 누적 레지스터를 사용하여 명령어 수준 병렬성 향상
 * 3. 수평 합산 최적화
 * 4. AVX2에서는 FMA 명령어 활용
 */
float avx_vector_dot(const float* a, const float* b, size_t size) {
    size_t i = 0;

    // 여러 누적 레지스터 사용 (명령어 수준 병렬성 향상)
    __m256 vsum0 = _mm256_setzero_ps();
    __m256 vsum1 = _mm256_setzero_ps();
    __m256 vsum2 = _mm256_setzero_ps();
    __m256 vsum3 = _mm256_setzero_ps();

    // 32개 요소씩 처리 (4개 레지스터 * 8개 요소)
    for (; i + 31 < size; i += 32) {
        // 첫 번째 8개 요소
        __m256 va0 = _mm256_loadu_ps(a + i);
        __m256 vb0 = _mm256_loadu_ps(b + i);

        // 두 번째 8개 요소
        __m256 va1 = _mm256_loadu_ps(a + i + 8);
        __m256 vb1 = _mm256_loadu_ps(b + i + 8);

        // 세 번째 8개 요소
        __m256 va2 = _mm256_loadu_ps(a + i + 16);
        __m256 vb2 = _mm256_loadu_ps(b + i + 16);

        // 네 번째 8개 요소
        __m256 va3 = _mm256_loadu_ps(a + i + 24);
        __m256 vb3 = _mm256_loadu_ps(b + i + 24);

#ifdef LIBETUDE_HAVE_AVX2
        // FMA 명령어 사용 (AVX2)
        vsum0 = _mm256_fmadd_ps(va0, vb0, vsum0);
        vsum1 = _mm256_fmadd_ps(va1, vb1, vsum1);
        vsum2 = _mm256_fmadd_ps(va2, vb2, vsum2);
        vsum3 = _mm256_fmadd_ps(va3, vb3, vsum3);
#else
        // 기본 AVX 명령어 사용
        vsum0 = _mm256_add_ps(vsum0, _mm256_mul_ps(va0, vb0));
        vsum1 = _mm256_add_ps(vsum1, _mm256_mul_ps(va1, vb1));
        vsum2 = _mm256_add_ps(vsum2, _mm256_mul_ps(va2, vb2));
        vsum3 = _mm256_add_ps(vsum3, _mm256_mul_ps(va3, vb3));
#endif
    }

    // 8개 요소씩 처리 (나머지)
    for (; i + 7 < size; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
#ifdef LIBETUDE_HAVE_AVX2
        vsum0 = _mm256_fmadd_ps(va, vb, vsum0);
#else
        vsum0 = _mm256_add_ps(vsum0, _mm256_mul_ps(va, vb));
#endif
    }

    // 모든 누적 레지스터 합산
    vsum0 = _mm256_add_ps(vsum0, vsum1);
    vsum2 = _mm256_add_ps(vsum2, vsum3);
    vsum0 = _mm256_add_ps(vsum0, vsum2);

    // 수평 합산 최적화 (AVX에서는 직접적인 수평 합산 명령어가 없음)
    // 상위 128비트와 하위 128비트를 추출하여 합산
    __m128 vlow  = _mm256_castps256_ps128(vsum0);
    __m128 vhigh = _mm256_extractf128_ps(vsum0, 1);

    // 128비트 벡터들을 합산
    __m128 vsum_128 = _mm_add_ps(vlow, vhigh);

    // 4개 요소를 2개씩 합산
    __m128 vshuf = _mm_movehdup_ps(vsum_128); // 복제 상위 2개 요소
    __m128 vsums = _mm_add_ps(vsum_128, vshuf); // 합산

    // 마지막 2개 요소를 합산
    vshuf = _mm_movehl_ps(vshuf, vsums);
    vsums = _mm_add_ss(vsums, vshuf);

    // 최종 결과 추출
    float sum = _mm_cvtss_f32(vsums);

    // 나머지 요소 처리
    for (; i < size; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

/**
 * @brief 행렬 곱셈 (AVX 구현)
 *
 * 간단한 AVX 최적화 버전의 행렬 곱셈
 * A: m x k, B: k x n, C: m x n
 */
void avx_gemm(const float* a, const float* b, float* c,
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
#ifdef LIBETUDE_HAVE_AVX2
                __m256 vresult = _mm256_fmadd_ps(va, vb, vc); // FMA 사용
#else
                __m256 vmul = _mm256_mul_ps(va, vb);
                __m256 vresult = _mm256_add_ps(vmul, vc);
#endif
                _mm256_storeu_ps(&c[i * n + j], vresult);
            }

            // 나머지 요소 처리
            for (; j < n; j++) {
                c[i * n + j] += a[i * k + l] * b[l * n + j];
            }
        }
    }
}

/**
 * @brief 최적화된 GEMM (블록 단위 처리)
 * 캐시 효율성을 위한 블록 단위 행렬 곱셈
 *
 * 이 구현은 다음과 같은 최적화 기법을 사용합니다:
 * 1. 블록 단위 처리로 캐시 지역성 향상
 * 2. 마이크로 커널 접근 방식으로 레지스터 재사용 최대화
 * 3. 데이터 프리페치를 통한 메모리 접근 최적화
 * 4. AVX 명령어를 활용한 벡터화
 */
void avx_gemm_blocked(const float* a, const float* b, float* c,
                      size_t m, size_t n, size_t k) {
    // 블록 크기 최적화 (L1/L2 캐시 크기에 맞춤)
    const size_t mc = 64;  // A 행렬 블록 행 크기
    const size_t kc = 64;  // A 행렬 블록 열 크기 / B 행렬 블록 행 크기
    const size_t nc = 128; // B 행렬 블록 열 크기

    // 마이크로 커널 크기 (레지스터 최적화)
    const size_t mr = 4;   // 내부 루프에서 한 번에 처리할 A 행 수
    const size_t nr = 16;  // 내부 루프에서 한 번에 처리할 B 열 수

    // 행렬 C를 0으로 초기화
    memset(c, 0, m * n * sizeof(float));

    // 임시 패킹 버퍼 (정렬된 메모리 접근을 위함)
    // 32바이트 정렬로 AVX 최적화
    float* a_packed = NULL;
    float* b_packed = NULL;

    // 메모리 할당 시도
    const size_t a_size = mc * kc * sizeof(float);
    const size_t b_size = kc * nc * sizeof(float);

#ifdef _WIN32
    a_packed = (float*)_aligned_malloc(a_size, 32);
    b_packed = (float*)_aligned_malloc(b_size, 32);
#else
    if (posix_memalign((void**)&a_packed, 32, a_size) != 0) a_packed = NULL;
    if (posix_memalign((void**)&b_packed, 32, b_size) != 0) b_packed = NULL;
#endif

    if (!a_packed || !b_packed) {
        // 메모리 할당 실패 시 정리 후 폴백
#ifdef _WIN32
        if (a_packed) _aligned_free(a_packed);
        if (b_packed) _aligned_free(b_packed);
#else
        if (a_packed) free(a_packed);
        if (b_packed) free(b_packed);
#endif
        avx_gemm(a, b, c, m, n, k);
        return;
    }

    // 3중 블록 루프
    for (size_t i = 0; i < m; i += mc) {
        size_t ib = LIBETUDE_MIN(mc, m - i);

        for (size_t p = 0; p < k; p += kc) {
            size_t pb = LIBETUDE_MIN(kc, k - p);

            // A 행렬 블록 패킹 (연속 메모리 접근을 위한 재배치)
            for (size_t ii = 0; ii < ib; ii++) {
                for (size_t pp = 0; pp < pb; pp++) {
                    a_packed[ii * pb + pp] = a[(i + ii) * k + (p + pp)];
                }
            }

            for (size_t j = 0; j < n; j += nc) {
                size_t jb = LIBETUDE_MIN(nc, n - j);

                // B 행렬 블록 패킹
                for (size_t pp = 0; pp < pb; pp++) {
                    for (size_t jj = 0; jj < jb; jj++) {
                        b_packed[pp * jb + jj] = b[(p + pp) * n + (j + jj)];
                    }
                }

                // 마이크로 커널 블록 계산
                for (size_t ii = 0; ii < ib; ii += mr) {
                    size_t i_limit = LIBETUDE_MIN(mr, ib - ii);

                    for (size_t jj = 0; jj < jb; jj += nr) {
                        size_t j_limit = LIBETUDE_MIN(nr, jb - jj);

                        // 마이크로 커널 (최내부 루프)
                        // 여기서 실제 계산이 이루어짐
                        for (size_t iii = 0; iii < i_limit; iii++) {
                            for (size_t pp = 0; pp < pb; pp++) {
                                // A 행렬 요소 로드 및 브로드캐스트
                                float a_val = a_packed[(ii + iii) * pb + pp];
                                __m256 va = _mm256_set1_ps(a_val);

                                // 8개 요소씩 처리 (AVX 레지스터 크기)
                                for (size_t jjj = 0; jjj < j_limit; jjj += 8) {
                                    if (jjj + 8 <= j_limit) {
                                        // B 행렬 요소 로드
                                        __m256 vb = _mm256_loadu_ps(&b_packed[pp * jb + jj + jjj]);

                                        // C 행렬 요소 로드
                                        __m256 vc = _mm256_loadu_ps(&c[(i + ii + iii) * n + (j + jj + jjj)]);

                                        // 계산 및 저장
#ifdef LIBETUDE_HAVE_AVX2
                                        // FMA 명령어 사용 (AVX2)
                                        vc = _mm256_fmadd_ps(va, vb, vc);
#else
                                        // 기본 AVX 명령어 사용
                                        __m256 vmul = _mm256_mul_ps(va, vb);
                                        vc = _mm256_add_ps(vc, vmul);
#endif
                                        _mm256_storeu_ps(&c[(i + ii + iii) * n + (j + jj + jjj)], vc);
                                    } else {
                                        // 나머지 요소 스칼라 처리
                                        for (size_t j_rem = 0; j_rem < j_limit - jjj; j_rem++) {
                                            c[(i + ii + iii) * n + (j + jj + jjj + j_rem)] +=
                                                a_val * b_packed[pp * jb + jj + jjj + j_rem];
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 임시 버퍼 해제
    free(a_packed);
    free(b_packed);
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

    // 벡터 내적 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_dot_avx";
    kernel_info.kernel_func = (void*)avx_vector_dot;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 4.0f;
    kernel_registry_register(&kernel_info);

    // GEMM 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "gemm_avx";
    kernel_info.kernel_func = (void*)avx_gemm;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 128; // 행렬 크기 기준
    kernel_info.performance_score = 4.0f;
    kernel_registry_register(&kernel_info);

    // 블록 단위 GEMM 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "gemm_blocked_avx";
    kernel_info.kernel_func = (void*)avx_gemm_blocked;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256; // 더 큰 행렬에서 효과적
    kernel_info.performance_score = 4.5f;
    kernel_registry_register(&kernel_info);

    // 추가 활성화 함수들
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_sigmoid_avx";
    kernel_info.kernel_func = (void*)avx_sigmoid;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 3.5f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_tanh_avx";
    kernel_info.kernel_func = (void*)avx_tanh;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 3.5f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_gelu_avx";
    kernel_info.kernel_func = (void*)avx_gelu;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 3.0f;
    kernel_registry_register(&kernel_info);

    // 소프트맥스 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "softmax_avx";
    kernel_info.kernel_func = (void*)avx_softmax;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 3.5f;
    kernel_registry_register(&kernel_info);

    // 레이어 정규화 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "layer_norm_avx";
    kernel_info.kernel_func = (void*)avx_layer_norm;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 3.8f;
    kernel_registry_register(&kernel_info);

    // 배치 정규화 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "batch_norm_avx";
    kernel_info.kernel_func = (void*)avx_batch_norm;
    kernel_info.simd_features = LIBETUDE_SIMD_AVX;
    kernel_info.optimal_size = 256;
    kernel_info.performance_score = 4.0f;
    kernel_registry_register(&kernel_info);
#endif
}
//
 ============================================================================
// AVX BF16 양자화 최적화 커널
// ============================================================================

#ifdef LIBETUDE_HAVE_AVX
/**
 * @brief AVX 최적화된 float32 to BF16 변환
 */
void avx_float32_to_bfloat16(const float* input, uint16_t* output, size_t size) {
    size_t i = 0;

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        __m256 vfloat = _mm256_loadu_ps(input + i);
        __m256i vint = _mm256_castps_si256(vfloat);

        // 반올림을 위한 바이어스 추가
        __m256i bias = _mm256_set1_epi32(0x00007FFF);
        __m256i round_bit = _mm256_and_si256(_mm256_srli_epi32(vint, 16), _mm256_set1_epi32(1));
        bias = _mm256_add_epi32(bias, round_bit);

        // 바이어스 추가 후 상위 16비트 추출
        vint = _mm256_add_epi32(vint, bias);
        vint = _mm256_srli_epi32(vint, 16);

        // 32비트에서 16비트로 패킹
        __m256i packed = _mm256_packus_epi32(vint, vint);

        // 결과 저장 (128비트씩 두 번)
        _mm_storeu_si128((__m128i*)(output + i), _mm256_extracti128_si256(packed, 0));
        _mm_storeu_si128((__m128i*)(output + i + 4), _mm256_extracti128_si256(packed, 1));
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        union { float f; uint32_t i; } converter;
        converter.f = input[i];
        uint32_t bias = 0x00007FFF + ((converter.i >> 16) & 1);
        output[i] = (uint16_t)((converter.i + bias) >> 16);
    }
}

/**
 * @brief AVX 최적화된 BF16 to float32 변환
 */
void avx_bfloat16_to_float32(const uint16_t* input, float* output, size_t size) {
    size_t i = 0;

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        // 16비트 값들을 로드
        __m128i vbf16 = _mm_loadu_si128((__m128i*)(input + i));

        // 16비트를 32비트로 확장
        __m256i vint32 = _mm256_cvtepu16_epi32(vbf16);

        // 16비트 왼쪽 시프트 (BF16 -> float32)
        vint32 = _mm256_slli_epi32(vint32, 16);

        // 정수를 float로 재해석
        __m256 vfloat = _mm256_castsi256_ps(vint32);

        // 결과 저장
        _mm256_storeu_ps(output + i, vfloat);
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        union { float f; uint32_t i; } converter;
        converter.i = ((uint32_t)input[i]) << 16;
        output[i] = converter.f;
    }
}

/**
 * @brief AVX 최적화된 BF16 벡터 덧셈
 */
void avx_bfloat16_vector_add(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size) {
    size_t i = 0;

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        // BF16 값들을 로드
        __m128i va_bf16 = _mm_loadu_si128((__m128i*)(a + i));
        __m128i vb_bf16 = _mm_loadu_si128((__m128i*)(b + i));

        // 16비트를 32비트로 확장
        __m256i va_int32 = _mm256_cvtepu16_epi32(va_bf16);
        __m256i vb_int32 = _mm256_cvtepu16_epi32(vb_bf16);

        // 16비트 왼쪽 시프트 (BF16 -> float32)
        va_int32 = _mm256_slli_epi32(va_int32, 16);
        vb_int32 = _mm256_slli_epi32(vb_int32, 16);

        // 정수를 float로 재해석
        __m256 va_float = _mm256_castsi256_ps(va_int32);
        __m256 vb_float = _mm256_castsi256_ps(vb_int32);

        // float32 덧셈 수행
        __m256 vresult_float = _mm256_add_ps(va_float, vb_float);

        // float32를 BF16으로 변환
        __m256i vresult_int = _mm256_castps_si256(vresult_float);

        // 반올림을 위한 바이어스 추가
        __m256i bias = _mm256_set1_epi32(0x00007FFF);
        __m256i round_bit = _mm256_and_si256(_mm256_srli_epi32(vresult_int, 16), _mm256_set1_epi32(1));
        bias = _mm256_add_epi32(bias, round_bit);

        // 바이어스 추가 후 상위 16비트 추출
        vresult_int = _mm256_add_epi32(vresult_int, bias);
        vresult_int = _mm256_srli_epi32(vresult_int, 16);

        // 32비트에서 16비트로 패킹
        __m256i packed = _mm256_packus_epi32(vresult_int, vresult_int);

        // 결과 저장
        _mm_storeu_si128((__m128i*)(result + i), _mm256_extracti128_si256(packed, 0));
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        union { float f; uint32_t i; } conv_a, conv_b, conv_result;
        conv_a.i = ((uint32_t)a[i]) << 16;
        conv_b.i = ((uint32_t)b[i]) << 16;
        conv_result.f = conv_a.f + conv_b.f;
        uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
        result[i] = (uint16_t)((conv_result.i + bias) >> 16);
    }
}

/**
 * @brief AVX 최적화된 BF16 벡터 곱셈
 */
void avx_bfloat16_vector_mul(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size) {
    size_t i = 0;

    // 8개 요소씩 AVX 벡터 처리
    for (; i + 7 < size; i += 8) {
        // BF16 값들을 로드
        __m128i va_bf16 = _mm_loadu_si128((__m128i*)(a + i));
        __m128i vb_bf16 = _mm_loadu_si128((__m128i*)(b + i));

        // 16비트를 32비트로 확장
        __m256i va_int32 = _mm256_cvtepu16_epi32(va_bf16);
        __m256i vb_int32 = _mm256_cvtepu16_epi32(vb_bf16);

        // 16비트 왼쪽 시프트 (BF16 -> float32)
        va_int32 = _mm256_slli_epi32(va_int32, 16);
        vb_int32 = _mm256_slli_epi32(vb_int32, 16);

        // 정수를 float로 재해석
        __m256 va_float = _mm256_castsi256_ps(va_int32);
        __m256 vb_float = _mm256_castsi256_ps(vb_int32);

        // float32 곱셈 수행
        __m256 vresult_float = _mm256_mul_ps(va_float, vb_float);

        // float32를 BF16으로 변환
        __m256i vresult_int = _mm256_castps_si256(vresult_float);

        // 반올림을 위한 바이어스 추가
        __m256i bias = _mm256_set1_epi32(0x00007FFF);
        __m256i round_bit = _mm256_and_si256(_mm256_srli_epi32(vresult_int, 16), _mm256_set1_epi32(1));
        bias = _mm256_add_epi32(bias, round_bit);

        // 바이어스 추가 후 상위 16비트 추출
        vresult_int = _mm256_add_epi32(vresult_int, bias);
        vresult_int = _mm256_srli_epi32(vresult_int, 16);

        // 32비트에서 16비트로 패킹
        __m256i packed = _mm256_packus_epi32(vresult_int, vresult_int);

        // 결과 저장
        _mm_storeu_si128((__m128i*)(result + i), _mm256_extracti128_si256(packed, 0));
    }

    // 나머지 요소 처리
    for (; i < size; i++) {
        union { float f; uint32_t i; } conv_a, conv_b, conv_result;
        conv_a.i = ((uint32_t)a[i]) << 16;
        conv_b.i = ((uint32_t)b[i]) << 16;
        conv_result.f = conv_a.f * conv_b.f;
        uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
        result[i] = (uint16_t)((conv_result.i + bias) >> 16);
    }
}

/**
 * @brief AVX 최적화된 BF16 행렬 곱셈 (간단한 구현)
 */
void avx_bfloat16_gemm(const uint16_t* a, const uint16_t* b, uint16_t* c,
                      size_t m, size_t n, size_t k) {
    // 결과 행렬 초기화
    memset(c, 0, m * n * sizeof(uint16_t));

    // 블록 크기 설정 (캐시 효율성을 위해)
    const size_t BLOCK_SIZE = 64;

    for (size_t i = 0; i < m; i += BLOCK_SIZE) {
        for (size_t j = 0; j < n; j += BLOCK_SIZE) {
            for (size_t l = 0; l < k; l += BLOCK_SIZE) {
                // 블록 경계 계산
                size_t i_end = (i + BLOCK_SIZE < m) ? i + BLOCK_SIZE : m;
                size_t j_end = (j + BLOCK_SIZE < n) ? j + BLOCK_SIZE : n;
                size_t l_end = (l + BLOCK_SIZE < k) ? l + BLOCK_SIZE : k;

                // 블록 내 행렬 곱셈
                for (size_t ii = i; ii < i_end; ii++) {
                    for (size_t jj = j; jj < j_end; jj++) {
                        __m256 sum_vec = _mm256_setzero_ps();
                        size_t ll = l;

                        // AVX로 8개씩 처리
                        for (; ll + 7 < l_end; ll += 8) {
                            // A 행렬의 BF16 값들을 float32로 변환
                            __m128i va_bf16 = _mm_loadu_si128((__m128i*)(a + ii * k + ll));
                            __m256i va_int32 = _mm256_cvtepu16_epi32(va_bf16);
                            va_int32 = _mm256_slli_epi32(va_int32, 16);
                            __m256 va_float = _mm256_castsi256_ps(va_int32);

                            // B 행렬의 BF16 값들을 float32로 변환
                            uint16_t b_values[8];
                            for (int idx = 0; idx < 8; idx++) {
                                b_values[idx] = b[(ll + idx) * n + jj];
                            }
                            __m128i vb_bf16 = _mm_loadu_si128((__m128i*)b_values);
                            __m256i vb_int32 = _mm256_cvtepu16_epi32(vb_bf16);
                            vb_int32 = _mm256_slli_epi32(vb_int32, 16);
                            __m256 vb_float = _mm256_castsi256_ps(vb_int32);

                            // 곱셈 및 누적
                            __m256 prod = _mm256_mul_ps(va_float, vb_float);
                            sum_vec = _mm256_add_ps(sum_vec, prod);
                        }

                        // 벡터 합계를 스칼라로 축소
                        float sum_array[8];
                        _mm256_storeu_ps(sum_array, sum_vec);
                        float partial_sum = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3] +
                                          sum_array[4] + sum_array[5] + sum_array[6] + sum_array[7];

                        // 나머지 요소들 처리
                        for (; ll < l_end; ll++) {
                            union { float f; uint32_t i; } conv_a, conv_b;
                            conv_a.i = ((uint32_t)a[ii * k + ll]) << 16;
                            conv_b.i = ((uint32_t)b[ll * n + jj]) << 16;
                            partial_sum += conv_a.f * conv_b.f;
                        }

                        // 기존 값에 누적하고 BF16으로 변환
                        union { float f; uint32_t i; } conv_c, conv_result;
                        conv_c.i = ((uint32_t)c[ii * n + jj]) << 16;
                        conv_result.f = conv_c.f + partial_sum;
                        uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
                        c[ii * n + jj] = (uint16_t)((conv_result.i + bias) >> 16);
                    }
                }
            }
        }
    }
}

#else // LIBETUDE_HAVE_AVX가 정의되지 않은 경우

// AVX가 지원되지 않는 경우 더미 함수들
void avx_float32_to_bfloat16(const float* input, uint16_t* output, size_t size) {
    // 기본 구현으로 fallback
    for (size_t i = 0; i < size; i++) {
        union { float f; uint32_t i; } converter;
        converter.f = input[i];
        uint32_t bias = 0x00007FFF + ((converter.i >> 16) & 1);
        output[i] = (uint16_t)((converter.i + bias) >> 16);
    }
}

void avx_bfloat16_to_float32(const uint16_t* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        union { float f; uint32_t i; } converter;
        converter.i = ((uint32_t)input[i]) << 16;
        output[i] = converter.f;
    }
}

void avx_bfloat16_vector_add(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        union { float f; uint32_t i; } conv_a, conv_b, conv_result;
        conv_a.i = ((uint32_t)a[i]) << 16;
        conv_b.i = ((uint32_t)b[i]) << 16;
        conv_result.f = conv_a.f + conv_b.f;
        uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
        result[i] = (uint16_t)((conv_result.i + bias) >> 16);
    }
}

void avx_bfloat16_vector_mul(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        union { float f; uint32_t i; } conv_a, conv_b, conv_result;
        conv_a.i = ((uint32_t)a[i]) << 16;
        conv_b.i = ((uint32_t)b[i]) << 16;
        conv_result.f = conv_a.f * conv_b.f;
        uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
        result[i] = (uint16_t)((conv_result.i + bias) >> 16);
    }
}

void avx_bfloat16_gemm(const uint16_t* a, const uint16_t* b, uint16_t* c,
                      size_t m, size_t n, size_t k) {
    memset(c, 0, m * n * sizeof(uint16_t));
    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++) {
            float sum = 0.0f;
            for (size_t l = 0; l < k; l++) {
                union { float f; uint32_t i; } conv_a, conv_b;
                conv_a.i = ((uint32_t)a[i * k + l]) << 16;
                conv_b.i = ((uint32_t)b[l * n + j]) << 16;
                sum += conv_a.f * conv_b.f;
            }
            union { float f; uint32_t i; } conv_result;
            conv_result.f = sum;
            uint32_t bias = 0x00007FFF + ((conv_result.i >> 16) & 1);
            c[i * n + j] = (uint16_t)((conv_result.i + bias) >> 16);
        }
    }
}

#endif // LIBETUDE_HAVE_AVX