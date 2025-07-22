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
// 음성 합성 특화 SIMD 함수들
// ============================================================================

/**
 * @brief 최적화된 Mel 필터뱅크 적용
 *
 * 스펙트로그램에 Mel 필터뱅크를 적용하여 Mel 스펙트로그램을 생성합니다.
 *
 * @param spectrogram 입력 스펙트로그램 (n_frames x n_fft)
 * @param mel_filters Mel 필터뱅크 (n_mels x n_fft)
 * @param mel_output 출력 Mel 스펙트로그램 (n_frames x n_mels)
 * @param n_fft FFT 크기
 * @param n_mels Mel 빈 개수
 * @param n_frames 프레임 개수
 */
LIBETUDE_API void simd_apply_mel_filterbank_optimal(const float* spectrogram, const float* mel_filters,
                                                   float* mel_output, size_t n_fft, size_t n_mels, size_t n_frames);

/**
 * @brief 최적화된 윈도우 함수 적용
 *
 * 입력 신호에 윈도우 함수를 적용합니다.
 *
 * @param input 입력 신호
 * @param window 윈도우 함수 (Hann, Hamming 등)
 * @param output 출력 신호
 * @param size 신호 크기
 */
LIBETUDE_API void simd_apply_window_optimal(const float* input, const float* window, float* output, size_t size);

/**
 * @brief 최적화된 복소수 곱셈
 *
 * 두 복소수 배열의 곱셈을 수행합니다.
 *
 * @param a_real 첫 번째 복소수 배열의 실수부
 * @param a_imag 첫 번째 복소수 배열의 허수부
 * @param b_real 두 번째 복소수 배열의 실수부
 * @param b_imag 두 번째 복소수 배열의 허수부
 * @param result_real 결과 복소수 배열의 실수부
 * @param result_imag 결과 복소수 배열의 허수부
 * @param size 배열 크기
 */
LIBETUDE_API void simd_complex_multiply_optimal(const float* a_real, const float* a_imag,
                                               const float* b_real, const float* b_imag,
                                               float* result_real, float* result_imag, size_t size);

/**
 * @brief 최적화된 복소수 크기 계산
 *
 * 복소수 배열의 크기(magnitude)를 계산합니다.
 *
 * @param real 복소수 배열의 실수부
 * @param imag 복소수 배열의 허수부
 * @param magnitude 출력 크기 배열
 * @param size 배열 크기
 */
LIBETUDE_API void simd_complex_magnitude_optimal(const float* real, const float* imag, float* magnitude, size_t size);

/**
 * @brief 최적화된 로그 스펙트럼 계산
 *
 * 스펙트럼 크기에 로그를 적용하여 로그 스펙트럼을 계산합니다.
 *
 * @param magnitude 입력 스펙트럼 크기
 * @param log_spectrum 출력 로그 스펙트럼
 * @param size 배열 크기
 * @param epsilon 로그 계산 시 0 방지를 위한 작은 값
 */
LIBETUDE_API void simd_log_spectrum_optimal(const float* magnitude, float* log_spectrum, size_t size, float epsilon);

/**
 * @brief 배터리 효율적인 벡터 덧셈
 *
 * 모바일 환경에서 배터리 사용량을 최소화하는 벡터 덧셈입니다.
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_vector_add_power_efficient(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 적응형 성능 조절 벡터 덧셈
 *
 * 시스템 성능에 따라 처리 방식을 동적으로 조절하는 벡터 덧셈입니다.
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_vector_add_adaptive(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 저전력 행렬 곱셈
 *
 * 모바일 환경에서 배터리 수명을 고려한 행렬 곱셈입니다.
 *
 * @param a 행렬 A (m x k)
 * @param b 행렬 B (k x n)
 * @param c 결과 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
LIBETUDE_API void simd_gemm_low_power(const float* a, const float* b, float* c,
                                     size_t m, size_t n, size_t k);

/**
 * @brief 온도 인식 적응형 벡터 덧셈
 *
 * 시스템 온도에 따라 처리 강도를 조절하는 벡터 덧셈입니다.
 *
 * @param a 첫 번째 벡터
 * @param b 두 번째 벡터
 * @param result 결과 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_vector_add_thermal_aware(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 모바일 특화 피치 시프팅
 *
 * 음성 합성에서 피치 조절을 위한 모바일 최적화 함수입니다.
 *
 * @param input 입력 오디오 신호
 * @param output 출력 오디오 신호
 * @param size 신호 크기
 * @param pitch_factor 피치 변경 비율 (1.0 = 원본, 2.0 = 2배 높음)
 */
LIBETUDE_API void simd_pitch_shift_mobile(const float* input, float* output, size_t size, float pitch_factor);

/**
 * @brief 모바일 최적화된 스펙트럴 엔벨로프 조정
 *
 * 음성 합성에서 음색 조절을 위한 스펙트럴 엔벨로프 조정입니다.
 *
 * @param magnitude 입력 스펙트럼 크기
 * @param envelope 적용할 엔벨로프
 * @param output 출력 스펙트럼
 * @param size 스펙트럼 크기
 */
LIBETUDE_API void simd_spectral_envelope_mobile(const float* magnitude, const float* envelope,
                                               float* output, size_t size);

/**
 * @brief 모바일용 실시간 노이즈 게이트
 *
 * 음성 합성 후처리에서 배경 노이즈 제거를 위한 함수입니다.
 *
 * @param input 입력 오디오 신호
 * @param output 출력 오디오 신호
 * @param size 신호 크기
 * @param threshold 노이즈 게이트 임계값
 * @param ratio 게이트 비율 (0.0 = 완전 차단, 1.0 = 통과)
 */
LIBETUDE_API void simd_noise_gate_mobile(const float* input, float* output, size_t size,
                                        float threshold, float ratio);

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