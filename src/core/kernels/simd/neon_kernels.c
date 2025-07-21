/**
 * @file neon_kernels.c
 * @brief ARM NEON SIMD 커널 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/kernel_registry.h"
#include <string.h>
#include <math.h>

#ifdef LIBETUDE_HAVE_NEON
#include <arm_neon.h>
#endif

// ============================================================================
// NEON 벡터 연산 커널
// ============================================================================

#ifdef LIBETUDE_HAVE_NEON
/**
 * @brief 벡터 덧셈 (NEON 구현)
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
 * @brief Sigmoid 활성화 함수 (NEON 구현)
 *
 * 참고: 완전한 NEON 구현을 위해서는 exp 함수의 NEON 최적화가 필요함
 * 여기서는 간단한 구현만 제공
 */
void neon_sigmoid(const float* input, float* output, size_t size) {
    // 현재는 스칼라 구현과 동일
    for (size_t i = 0; i < size; i++) {
        output[i] = 1.0f / (1.0f + expf(-input[i]));
    }
}

/**
 * @brief Tanh 활성화 함수 (NEON 구현)
 *
 * 참고: 완전한 NEON 구현을 위해서는 tanh 함수의 NEON 최적화가 필요함
 * 여기서는 간단한 구현만 제공
 */
void neon_tanh(const float* input, float* output, size_t size) {
    // 현재는 스칼라 구현과 동일
    for (size_t i = 0; i < size; i++) {
        output[i] = tanhf(input[i]);
    }
}

#else
// NEON을 지원하지 않는 경우 CPU 구현으로 대체
extern void cpu_vector_add(const float* a, const float* b, float* result, size_t size);
extern void cpu_vector_mul(const float* a, const float* b, float* result, size_t size);
extern void cpu_vector_scale(const float* input, float scale, float* result, size_t size);
extern void cpu_relu(const float* input, float* output, size_t size);
extern void cpu_sigmoid(const float* input, float* output, size_t size);
extern void cpu_tanh(const float* input, float* output, size_t size);

#define neon_vector_add cpu_vector_add
#define neon_vector_mul cpu_vector_mul
#define neon_vector_scale cpu_vector_scale
#define neon_relu cpu_relu
#define neon_sigmoid cpu_sigmoid
#define neon_tanh cpu_tanh
#endif

// ============================================================================
// NEON 커널 등록 함수
// ============================================================================

/**
 * @brief NEON 커널들을 등록합니다
 */
void register_neon_kernels(void) {
#ifdef LIBETUDE_HAVE_NEON
    // 벡터 연산 커널 등록
    KernelInfo kernel_info;

    // 벡터 덧셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_add_neon";
    kernel_info.kernel_func = (void*)neon_vector_add;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128; // 128개 이상의 요소에서 최적화 효과
    kernel_info.performance_score = 3.0f; // CPU 대비 예상 성능 점수
    kernel_registry_register(&kernel_info);

    // 벡터 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_mul_neon";
    kernel_info.kernel_func = (void*)neon_vector_mul;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 3.0f;
    kernel_registry_register(&kernel_info);

    // 벡터 스칼라 곱셈 커널
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "vector_scale_neon";
    kernel_info.kernel_func = (void*)neon_vector_scale;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 3.0f;
    kernel_registry_register(&kernel_info);

    // 활성화 함수 커널 등록
    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_relu_neon";
    kernel_info.kernel_func = (void*)neon_relu;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 3.0f;
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_sigmoid_neon";
    kernel_info.kernel_func = (void*)neon_sigmoid;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.0f; // 부분 최적화만 적용됨
    kernel_registry_register(&kernel_info);

    memset(&kernel_info, 0, sizeof(KernelInfo));
    kernel_info.name = "activation_tanh_neon";
    kernel_info.kernel_func = (void*)neon_tanh;
    kernel_info.simd_features = LIBETUDE_SIMD_NEON;
    kernel_info.optimal_size = 128;
    kernel_info.performance_score = 2.0f; // 부분 최적화만 적용됨
    kernel_registry_register(&kernel_info);
#endif
}