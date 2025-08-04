/**
 * @file windows_simd.h
 * @brief Windows 특화 SIMD 최적화 헤더
 *
 * Windows 환경에서 CPU 기능 감지 및 SIMD 최적화된 연산을 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_WINDOWS_SIMD_H
#define LIBETUDE_PLATFORM_WINDOWS_SIMD_H

#ifdef _WIN32

#include <stdbool.h>
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Windows CPU 기능 정보 구조체
 */
typedef struct {
    bool has_sse;           ///< SSE 지원 여부
    bool has_sse2;          ///< SSE2 지원 여부
    bool has_sse41;         ///< SSE4.1 지원 여부
    bool has_avx;           ///< AVX 지원 여부
    bool has_avx2;          ///< AVX2 지원 여부
    bool has_avx512f;       ///< AVX-512 Foundation 지원 여부
    bool has_avx512dq;      ///< AVX-512 DQ 지원 여부
    bool has_avx512bw;      ///< AVX-512 BW 지원 여부
    bool has_avx512vl;      ///< AVX-512 VL 지원 여부
} ETWindowsCPUFeatures;

// CPU 기능 감지
/**
 * @brief Windows에서 CPU 기능을 감지합니다
 *
 * @return ETWindowsCPUFeatures CPU 기능 정보 구조체
 */
ETWindowsCPUFeatures et_windows_detect_cpu_features(void);

/**
 * @brief CPU 기능 정보를 문자열로 반환합니다
 *
 * @param features CPU 기능 구조체
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 */
void et_windows_cpu_features_to_string(const ETWindowsCPUFeatures* features,
                                     char* buffer, size_t buffer_size);

// 행렬 연산
/**
 * @brief AVX2를 사용한 단정밀도 행렬 곱셈 (C = A * B)
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_avx2(const float* a, const float* b,
                                        float* c, int m, int n, int k);

/**
 * @brief AVX-512를 사용한 단정밀도 행렬 곱셈 (C = A * B)
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_avx512(const float* a, const float* b,
                                          float* c, int m, int n, int k);

/**
 * @brief 기본 행렬 곱셈 구현 (SIMD 미지원 시 폴백)
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_fallback(const float* a, const float* b,
                                            float* c, int m, int n, int k);

/**
 * @brief 최적의 SIMD 구현을 자동 선택하여 행렬 곱셈 수행
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_auto(const float* a, const float* b,
                                        float* c, int m, int n, int k);

// 벡터 연산
/**
 * @brief AVX2를 사용한 벡터 덧셈 (c = a + b)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param c 출력 벡터 C
 * @param size 벡터 크기
 */
void et_windows_simd_vector_add_avx2(const float* a, const float* b,
                                   float* c, int size);

/**
 * @brief AVX-512를 사용한 벡터 덧셈 (c = a + b)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param c 출력 벡터 C
 * @param size 벡터 크기
 */
void et_windows_simd_vector_add_avx512(const float* a, const float* b,
                                     float* c, int size);

/**
 * @brief 기본 벡터 덧셈 구현 (SIMD 미지원 시 폴백)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param c 출력 벡터 C
 * @param size 벡터 크기
 */
void et_windows_simd_vector_add_fallback(const float* a, const float* b,
                                       float* c, int size);

/**
 * @brief AVX2를 사용한 벡터 내적 계산
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param size 벡터 크기
 * @return float 내적 결과
 */
float et_windows_simd_vector_dot_avx2(const float* a, const float* b, int size);

/**
 * @brief 기본 벡터 내적 구현 (SIMD 미지원 시 폴백)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param size 벡터 크기
 * @return float 내적 결과
 */
float et_windows_simd_vector_dot_fallback(const float* a, const float* b, int size);

// 모듈 관리
/**
 * @brief Windows SIMD 모듈 초기화
 *
 * @return ETResult 초기화 결과
 */
ETResult et_windows_simd_init(void);

/**
 * @brief Windows SIMD 모듈 정리
 */
void et_windows_simd_finalize(void);

#ifdef __cplusplus
}
#endif

#endif // _WIN32

#endif // LIBETUDE_PLATFORM_WINDOWS_SIMD_H