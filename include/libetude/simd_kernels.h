/**
 * @file simd_kernels.h
 * @brief SIMD 최적화 커널 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 하드웨어별 SIMD 최적화된 커널들의 고수준 인터페이스를 제공합니다.
 */

#ifndef LIBETUDE_SIMD_KERNELS_H
#define LIBETUDE_SIMD_KERNELS_H

#include "libetude/config.h"
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 고수준 SIMD 인터페이스 함수
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
LIBETUDE_API void simd_vector_add_optimal(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 최적화된 벡터 곱셈
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_vector_mul_optimal(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 최적화된 벡터 스칼라 곱셈
 *
 * @param input 입력 벡터
 * @param scale 스칼라 값
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_vector_scale_optimal(const float* input, float scale, float* result, size_t size);

/**
 * @brief 최적화된 벡터 내적
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param size 벡터 크기
 * @return 내적 결과
 */
LIBETUDE_API float simd_vector_dot_optimal(const float* a, const float* b, size_t size);

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
LIBETUDE_API void simd_gemm_optimal(const float* a, const float* b, float* c,
                                   size_t m, size_t n, size_t k);

/**
 * @brief 최적화된 ReLU 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_relu_optimal(const float* input, float* output, size_t size);

/**
 * @brief 최적화된 Sigmoid 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_sigmoid_optimal(const float* input, float* output, size_t size);

/**
 * @brief 최적화된 Tanh 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_tanh_optimal(const float* input, float* output, size_t size);

/**
 * @brief 최적화된 GELU 활성화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_gelu_optimal(const float* input, float* output, size_t size);

// ============================================================================
// SIMD 커널 시스템 관리 함수
// ============================================================================

/**
 * @brief SIMD 커널 시스템을 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode simd_kernels_init(void);

/**
 * @brief SIMD 커널 시스템을 정리합니다
 */
LIBETUDE_API void simd_kernels_finalize(void);

/**
 * @brief 현재 사용 가능한 SIMD 기능을 반환합니다
 *
 * @return SIMD 기능 플래그
 */
LIBETUDE_API uint32_t simd_kernels_get_features(void);

/**
 * @brief SIMD 커널 정보를 출력합니다 (디버그용)
 */
LIBETUDE_API void simd_kernels_print_info(void);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_SIMD_KERNELS_H