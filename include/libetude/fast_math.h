/**
 * @file fast_math.h
 * @brief FastApprox 기반 고속 수학 함수 라이브러리
 *
 * 음성 합성에 최적화된 고속 근사 수학 함수들을 제공합니다.
 * FastApprox 알고리즘을 기반으로 하여 정확도와 성능의 균형을 맞춥니다.
 */

#ifndef LIBETUDE_FAST_MATH_H
#define LIBETUDE_FAST_MATH_H

#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 고속 지수 함수 (FastApprox 기반)
 *
 * 표준 exp() 함수보다 빠른 근사 구현입니다.
 * 음성 합성에서 활성화 함수 계산에 사용됩니다.
 *
 * @param x 입력값
 * @return e^x의 근사값
 */
float et_fast_exp(float x);

/**
 * @brief 고속 자연로그 함수 (FastApprox 기반)
 *
 * 표준 log() 함수보다 빠른 근사 구현입니다.
 *
 * @param x 입력값 (x > 0)
 * @return ln(x)의 근사값
 */
float et_fast_log(float x);

/**
 * @brief 고속 밑이 2인 로그 함수
 *
 * @param x 입력값 (x > 0)
 * @return log2(x)의 근사값
 */
float et_fast_log2(float x);

/**
 * @brief 고속 밑이 10인 로그 함수
 *
 * @param x 입력값 (x > 0)
 * @return log10(x)의 근사값
 */
float et_fast_log10(float x);

/**
 * @brief 고속 거듭제곱 함수
 *
 * @param base 밑
 * @param exponent 지수
 * @return base^exponent의 근사값
 */
float et_fast_pow(float base, float exponent);

/**
 * @brief 고속 사인 함수 (룩업 테이블 기반)
 *
 * 룩업 테이블과 선형 보간을 사용한 고속 구현입니다.
 *
 * @param x 입력값 (라디안)
 * @return sin(x)의 근사값
 */
float et_fast_sin(float x);

/**
 * @brief 고속 코사인 함수 (룩업 테이블 기반)
 *
 * @param x 입력값 (라디안)
 * @return cos(x)의 근사값
 */
float et_fast_cos(float x);

/**
 * @brief 고속 탄젠트 함수
 *
 * @param x 입력값 (라디안)
 * @return tan(x)의 근사값
 */
float et_fast_tan(float x);

/**
 * @brief 고속 아크탄젠트 함수
 *
 * @param x 입력값
 * @return atan(x)의 근사값
 */
float et_fast_atan(float x);

/**
 * @brief 고속 아크탄젠트2 함수
 *
 * @param y y 좌표
 * @param x x 좌표
 * @return atan2(y, x)의 근사값
 */
float et_fast_atan2(float y, float x);

/**
 * @brief 고속 하이퍼볼릭 탄젠트 함수 (활성화 함수)
 *
 * 신경망의 활성화 함수로 자주 사용되는 tanh 함수의 고속 구현입니다.
 *
 * @param x 입력값
 * @return tanh(x)의 근사값
 */
float et_fast_tanh(float x);

/**
 * @brief 고속 시그모이드 함수 (활성화 함수)
 *
 * 신경망의 활성화 함수로 사용되는 sigmoid 함수의 고속 구현입니다.
 *
 * @param x 입력값
 * @return 1/(1+e^(-x))의 근사값
 */
float et_fast_sigmoid(float x);

/**
 * @brief 고속 GELU 활성화 함수
 *
 * Gaussian Error Linear Unit 활성화 함수의 고속 구현입니다.
 *
 * @param x 입력값
 * @return GELU(x)의 근사값
 */
float et_fast_gelu(float x);

/**
 * @brief 고속 역제곱근 함수
 *
 * 1/sqrt(x)의 고속 구현입니다. 정규화 연산에 사용됩니다.
 *
 * @param x 입력값 (x > 0)
 * @return 1/sqrt(x)의 근사값
 */
float et_fast_inv_sqrt(float x);

/**
 * @brief 고속 제곱근 함수
 *
 * @param x 입력값 (x >= 0)
 * @return sqrt(x)의 근사값
 */
float et_fast_sqrt(float x);

/**
 * @brief 벡터화된 지수 함수
 *
 * 배열의 모든 요소에 대해 지수 함수를 적용합니다.
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_exp_vec(const float* input, float* output, size_t size);

/**
 * @brief 벡터화된 로그 함수
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_log_vec(const float* input, float* output, size_t size);

/**
 * @brief 벡터화된 tanh 함수
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_tanh_vec(const float* input, float* output, size_t size);

/**
 * @brief 벡터화된 sigmoid 함수
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_sigmoid_vec(const float* input, float* output, size_t size);

/**
 * @brief 고속 수학 함수 라이브러리 초기화
 *
 * 룩업 테이블을 생성하고 초기화합니다.
 *
 * @return 성공 시 0, 실패 시 음수
 */
int et_fast_math_init(void);

/**
 * @brief 고속 수학 함수 라이브러리 정리
 *
 * 할당된 메모리를 해제합니다.
 */
void et_fast_math_cleanup(void);

// =============================================================================
// 음성 특화 수학 함수들 (Voice-Specific Math Functions)
// =============================================================================

/**
 * @brief Hz를 Mel 스케일로 변환
 *
 * 인간의 청각 특성을 반영한 Mel 스케일 변환을 수행합니다.
 * mel = 2595 * log10(1 + hz / 700)
 *
 * @param hz 주파수 (Hz)
 * @return Mel 스케일 값
 */
float et_hz_to_mel(float hz);

/**
 * @brief Mel 스케일을 Hz로 변환
 *
 * Mel 스케일을 다시 주파수로 변환합니다.
 * hz = 700 * (10^(mel / 2595) - 1)
 *
 * @param mel Mel 스케일 값
 * @return 주파수 (Hz)
 */
float et_mel_to_hz(float mel);

/**
 * @brief Mel 필터뱅크 생성
 *
 * 주어진 파라미터로 Mel 필터뱅크를 생성합니다.
 *
 * @param n_fft FFT 크기
 * @param n_mels Mel 필터 개수
 * @param sample_rate 샘플링 레이트
 * @param fmin 최소 주파수 (Hz)
 * @param fmax 최대 주파수 (Hz)
 * @param mel_filters 출력 필터뱅크 배열 [n_mels * (n_fft/2 + 1)]
 * @return 성공 시 0, 실패 시 음수
 */
int et_create_mel_filterbank(int n_fft, int n_mels, float sample_rate,
                            float fmin, float fmax, float* mel_filters);

/**
 * @brief 스펙트로그램을 Mel 스펙트로그램으로 변환
 *
 * @param spectrogram 입력 스펙트로그램 [n_frames * n_freqs]
 * @param mel_filters Mel 필터뱅크 [n_mels * n_freqs]
 * @param mel_spectrogram 출력 Mel 스펙트로그램 [n_frames * n_mels]
 * @param n_frames 시간 프레임 수
 * @param n_freqs 주파수 빈 수
 * @param n_mels Mel 필터 수
 */
void et_spectrogram_to_mel(const float* spectrogram, const float* mel_filters,
                          float* mel_spectrogram, int n_frames, int n_freqs, int n_mels);

/**
 * @brief 피치 시프팅을 위한 주파수 스케일링
 *
 * 주어진 피치 시프트 비율에 따라 주파수를 스케일링합니다.
 *
 * @param frequency 원본 주파수 (Hz)
 * @param pitch_shift 피치 시프트 비율 (1.0 = 원본, 2.0 = 한 옥타브 위)
 * @return 시프트된 주파수 (Hz)
 */
float et_pitch_shift_frequency(float frequency, float pitch_shift);

/**
 * @brief 세미톤을 주파수 비율로 변환
 *
 * 음악적 세미톤 단위를 주파수 비율로 변환합니다.
 *
 * @param semitones 세미톤 수 (양수: 높은 음, 음수: 낮은 음)
 * @return 주파수 비율 (1.0 = 원본)
 */
float et_semitones_to_ratio(float semitones);

/**
 * @brief 주파수 비율을 세미톤으로 변환
 *
 * @param ratio 주파수 비율
 * @return 세미톤 수
 */
float et_ratio_to_semitones(float ratio);

/**
 * @brief 윈도우 함수 - 해밍 윈도우
 *
 * 오디오 신호 처리에 사용되는 해밍 윈도우를 생성합니다.
 *
 * @param window 출력 윈도우 배열
 * @param size 윈도우 크기
 */
void et_hamming_window(float* window, int size);

/**
 * @brief 윈도우 함수 - 한 윈도우
 *
 * 오디오 신호 처리에 사용되는 한 윈도우를 생성합니다.
 *
 * @param window 출력 윈도우 배열
 * @param size 윈도우 크기
 */
void et_hann_window(float* window, int size);

/**
 * @brief 윈도우 함수 - 블랙만 윈도우
 *
 * 오디오 신호 처리에 사용되는 블랙만 윈도우를 생성합니다.
 *
 * @param window 출력 윈도우 배열
 * @param size 윈도우 크기
 */
void et_blackman_window(float* window, int size);

/**
 * @brief 오디오 신호의 RMS (Root Mean Square) 계산
 *
 * 오디오 신호의 에너지를 측정하는 RMS 값을 계산합니다.
 *
 * @param signal 입력 신호
 * @param size 신호 크기
 * @return RMS 값
 */
float et_audio_rms(const float* signal, int size);

/**
 * @brief 오디오 신호 정규화
 *
 * 오디오 신호를 지정된 최대값으로 정규화합니다.
 *
 * @param signal 입출력 신호
 * @param size 신호 크기
 * @param target_max 목표 최대값 (일반적으로 1.0)
 */
void et_normalize_audio(float* signal, int size, float target_max);

/**
 * @brief 오디오 신호의 피크 값 찾기
 *
 * @param signal 입력 신호
 * @param size 신호 크기
 * @return 절댓값 기준 최대값
 */
float et_find_peak(const float* signal, int size);

/**
 * @brief 선형 보간
 *
 * 두 값 사이의 선형 보간을 수행합니다.
 *
 * @param a 시작값
 * @param b 끝값
 * @param t 보간 비율 (0.0 ~ 1.0)
 * @return 보간된 값
 */
float et_lerp(float a, float b, float t);

/**
 * @brief 코사인 보간
 *
 * 두 값 사이의 부드러운 코사인 보간을 수행합니다.
 *
 * @param a 시작값
 * @param b 끝값
 * @param t 보간 비율 (0.0 ~ 1.0)
 * @return 보간된 값
 */
float et_cosine_interp(float a, float b, float t);

/**
 * @brief 3차 스플라인 보간
 *
 * 4개 점을 사용한 3차 스플라인 보간을 수행합니다.
 *
 * @param p0 첫 번째 점
 * @param p1 두 번째 점
 * @param p2 세 번째 점
 * @param p3 네 번째 점
 * @param t 보간 비율 (0.0 ~ 1.0, p1과 p2 사이)
 * @return 보간된 값
 */
float et_cubic_interp(float p0, float p1, float p2, float p3, float t);

/**
 * @brief dB를 선형 스케일로 변환
 *
 * @param db dB 값
 * @return 선형 스케일 값
 */
float et_db_to_linear(float db);

/**
 * @brief 선형 스케일을 dB로 변환
 *
 * @param linear 선형 스케일 값
 * @return dB 값
 */
float et_linear_to_db(float linear);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_FAST_MATH_H