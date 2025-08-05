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

// SIMD가 비활성화된 경우 모든 함수를 no-op으로 정의하거나 fallback 구현 제공
#if !LIBETUDE_SIMD_ENABLED
#warning "SIMD support is disabled. Using fallback implementations."
#endif

#if LIBETUDE_SIMD_ENABLED
// ============================================================================
// 고수준 SIMD 인터페이스 함수 (SIMD 활성화시에만 사용 가능)
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

/**
 * @brief 최적화된 소프트맥스 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_softmax_optimal(const float* input, float* output, size_t size);

/**
 * @brief 최적화된 레이어 정규화 함수
 *
 * @param input 입력 벡터
 * @param output 출력 벡터
 * @param size 벡터 크기
 * @param epsilon 수치 안정성을 위한 작은 값
 */
LIBETUDE_API void simd_layer_norm_optimal(const float* input, float* output, size_t size, float epsilon);

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
LIBETUDE_API void simd_batch_norm_optimal(const float* input, float* output, size_t size,
                                         float mean, float variance, float gamma, float beta, float epsilon);

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
// BF16 양자화 SIMD 최적화 함수들
// ============================================================================

/**
 * @brief SIMD 최적화된 float32 to BF16 변환
 *
 * 벡터화된 BF16 변환으로 대량의 데이터를 효율적으로 처리합니다.
 *
 * @param input float32 입력 배열
 * @param output BF16 출력 배열 (uint16_t로 저장)
 * @param size 배열 크기
 */
LIBETUDE_API void simd_float32_to_bfloat16_optimal(const float* input, uint16_t* output, size_t size);

/**
 * @brief SIMD 최적화된 BF16 to float32 변환
 *
 * 벡터화된 BF16 역변환으로 대량의 데이터를 효율적으로 처리합니다.
 *
 * @param input BF16 입력 배열 (uint16_t로 저장)
 * @param output float32 출력 배열
 * @param size 배열 크기
 */
LIBETUDE_API void simd_bfloat16_to_float32_optimal(const uint16_t* input, float* output, size_t size);

/**
 * @brief SIMD 최적화된 BF16 벡터 덧셈
 *
 * BF16 형태로 저장된 두 벡터의 덧셈을 수행합니다.
 * 내부적으로 float32로 변환 후 연산하고 다시 BF16으로 변환합니다.
 *
 * @param a 첫 번째 BF16 벡터
 * @param b 두 번째 BF16 벡터
 * @param result 결과 BF16 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_bfloat16_vector_add_optimal(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);

/**
 * @brief SIMD 최적화된 BF16 벡터 곱셈
 *
 * BF16 형태로 저장된 두 벡터의 곱셈을 수행합니다.
 *
 * @param a 첫 번째 BF16 벡터
 * @param b 두 번째 BF16 벡터
 * @param result 결과 BF16 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_bfloat16_vector_mul_optimal(const uint16_t* a, const uint16_t* b, uint16_t* result, size_t size);

/**
 * @brief SIMD 최적화된 BF16 행렬 곱셈
 *
 * BF16 형태로 저장된 행렬들의 곱셈을 수행합니다.
 * 높은 처리량과 메모리 효율성을 제공합니다.
 *
 * @param a 행렬 A (m x k, BF16)
 * @param b 행렬 B (k x n, BF16)
 * @param c 결과 행렬 C (m x n, BF16)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
LIBETUDE_API void simd_bfloat16_gemm_optimal(const uint16_t* a, const uint16_t* b, uint16_t* c,
                                            size_t m, size_t n, size_t k);

/**
 * @brief SIMD 최적화된 BF16 활성화 함수 (ReLU)
 *
 * BF16 데이터에 대한 ReLU 활성화 함수를 적용합니다.
 *
 * @param input 입력 BF16 벡터
 * @param output 출력 BF16 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_bfloat16_relu_optimal(const uint16_t* input, uint16_t* output, size_t size);

/**
 * @brief SIMD 최적화된 BF16 활성화 함수 (GELU)
 *
 * BF16 데이터에 대한 GELU 활성화 함수를 적용합니다.
 * 음성 합성 모델에서 자주 사용되는 활성화 함수입니다.
 *
 * @param input 입력 BF16 벡터
 * @param output 출력 BF16 벡터
 * @param size 벡터 크기
 */
LIBETUDE_API void simd_bfloat16_gelu_optimal(const uint16_t* input, uint16_t* output, size_t size);

/**
 * @brief 적응형 BF16 양자화 임계값 계산
 *
 * 입력 데이터의 분포를 분석하여 최적의 BF16 양자화 전략을 결정합니다.
 * 음성 합성 특성을 고려한 동적 임계값 조정을 수행합니다.
 *
 * @param input float32 입력 데이터
 * @param size 데이터 크기
 * @param quantile 분위수 (0.0 ~ 1.0, 기본값 0.99)
 * @return 권장 양자화 임계값
 */
LIBETUDE_API float simd_bfloat16_adaptive_threshold(const float* input, size_t size, float quantile);

/**
 * @brief 음성 특화 BF16 양자화 파라미터 튜닝
 *
 * 음성 합성 모델의 특성을 고려하여 BF16 양자화 파라미터를 최적화합니다.
 * 주파수 도메인 특성과 시간 도메인 특성을 모두 고려합니다.
 *
 * @param input float32 입력 데이터 (음성 특징)
 * @param size 데이터 크기
 * @param is_frequency_domain 주파수 도메인 데이터 여부
 * @param scale_factor 출력 스케일 팩터 포인터
 * @param bias_factor 출력 바이어스 팩터 포인터
 * @return 튜닝 성공 여부
 */
LIBETUDE_API bool simd_bfloat16_voice_tuning(const float* input, size_t size, bool is_frequency_domain,
                                             float* scale_factor, float* bias_factor);

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

// ============================================================================
// 테스트용 호환성 함수들 (Legacy API)
// ============================================================================

/**
 * @brief SIMD 커널 시스템 초기화 (테스트 호환성)
 */
LIBETUDE_API LibEtudeErrorCode et_init_simd_kernels(void);

/**
 * @brief SIMD 커널 시스템 정리 (테스트 호환성)
 */
LIBETUDE_API void et_cleanup_simd_kernels(void);

/**
 * @brief 벡터 덧셈 (테스트 호환성)
 */
LIBETUDE_API void et_simd_vector_add(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 벡터 곱셈 (테스트 호환성)
 */
LIBETUDE_API void et_simd_vector_mul(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 벡터 내적 (테스트 호환성)
 */
LIBETUDE_API float et_simd_dot_product(const float* a, const float* b, size_t size);

/**
 * @brief 행렬-벡터 곱셈 (테스트 호환성)
 */
LIBETUDE_API void et_simd_matrix_vector_mul(const float* matrix, const float* vector, float* result, size_t rows, size_t cols);

/**
 * @brief ReLU 활성화 함수 (테스트 호환성)
 */
LIBETUDE_API void et_simd_relu(const float* input, float* output, size_t size);

/**
 * @brief Sigmoid 활성화 함수 (테스트 호환성)
 */
LIBETUDE_API void et_simd_sigmoid(const float* input, float* output, size_t size);

/**
 * @brief Tanh 활성화 함수 (테스트 호환성)
 */
LIBETUDE_API void et_simd_tanh(const float* input, float* output, size_t size);

/**
 * @brief SSE 지원 여부 확인
 */
LIBETUDE_API bool et_has_sse_support(void);

/**
 * @brief AVX 지원 여부 확인
 */
LIBETUDE_API bool et_has_avx_support(void);

/**
 * @brief NEON 지원 여부 확인
 */
LIBETUDE_API bool et_has_neon_support(void);

#else  // !LIBETUDE_SIMD_ENABLED
// SIMD가 비활성화된 경우, SIMD 함수들을 사용할 수 없음
// 이는 컴파일 타임에 오류를 발생시켜 SIMD 함수 사용을 방지함
#endif // LIBETUDE_SIMD_ENABLED

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_SIMD_KERNELS_H