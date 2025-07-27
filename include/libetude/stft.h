/**
 * @file stft.h
 * @brief STFT/ISTFT 최적화 구현 헤더
 * @author LibEtude Project
 * @version 1.0.0
 *
 * SIMD 최적화된 STFT/ISTFT 구현과 윈도우 함수, 병렬 처리를 제공합니다.
 */

#ifndef LIBETUDE_STFT_H
#define LIBETUDE_STFT_H

#include "libetude/types.h"
#include "libetude/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// STFT 컨텍스트 및 설정
// ============================================================================

/**
 * @brief STFT 윈도우 타입
 */
typedef enum {
    ET_WINDOW_HANN = 0,      /**< Hann 윈도우 */
    ET_WINDOW_HAMMING = 1,   /**< Hamming 윈도우 */
    ET_WINDOW_BLACKMAN = 2,  /**< Blackman 윈도우 */
    ET_WINDOW_RECTANGULAR = 3 /**< 사각 윈도우 */
} ETWindowType;

/**
 * @brief STFT 처리 모드
 */
typedef enum {
    ET_STFT_MODE_NORMAL = 0,    /**< 일반 모드 */
    ET_STFT_MODE_REALTIME = 1,  /**< 실시간 모드 */
    ET_STFT_MODE_BATCH = 2      /**< 배치 모드 */
} ETSTFTMode;

/**
 * @brief STFT 컨텍스트 구조체 (불투명 타입)
 */
typedef struct ETSTFTContext ETSTFTContext;

/**
 * @brief STFT 설정 구조체
 */
typedef struct {
    int fft_size;           /**< FFT 크기 (2의 거듭제곱) */
    int hop_size;           /**< Hop 크기 (일반적으로 fft_size/4) */
    int win_length;         /**< 윈도우 길이 (일반적으로 fft_size와 동일) */
    ETWindowType window_type; /**< 윈도우 타입 */
    ETSTFTMode mode;        /**< 처리 모드 */
    bool enable_simd;       /**< SIMD 최적화 활성화 */
    bool enable_parallel;   /**< 병렬 처리 활성화 */
    int num_threads;        /**< 병렬 처리 스레드 수 (0 = 자동) */
} ETSTFTConfig;

// ============================================================================
// STFT 컨텍스트 관리
// ============================================================================

/**
 * @brief STFT 컨텍스트 생성
 *
 * @param config STFT 설정
 * @return STFT 컨텍스트 포인터 (실패시 NULL)
 */
ETSTFTContext* et_stft_create_context(const ETSTFTConfig* config);

/**
 * @brief STFT 컨텍스트 해제
 *
 * @param ctx STFT 컨텍스트
 */
void et_stft_destroy_context(ETSTFTContext* ctx);

/**
 * @brief 기본 STFT 설정 생성
 *
 * @param fft_size FFT 크기
 * @param hop_size Hop 크기
 * @param window_type 윈도우 타입
 * @return STFT 설정 구조체
 */
ETSTFTConfig et_stft_default_config(int fft_size, int hop_size, ETWindowType window_type);

/**
 * @brief STFT 컨텍스트 설정 업데이트
 *
 * @param ctx STFT 컨텍스트
 * @param config 새로운 설정
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_update_config(ETSTFTContext* ctx, const ETSTFTConfig* config);

// ============================================================================
// STFT/ISTFT 핵심 함수
// ============================================================================

/**
 * @brief STFT 변환 (최적화된 구현)
 *
 * 입력 오디오 신호를 시간-주파수 도메인으로 변환합니다.
 * SIMD 최적화와 병렬 처리를 지원합니다.
 *
 * @param ctx STFT 컨텍스트
 * @param audio 입력 오디오 신호
 * @param audio_len 오디오 신호 길이
 * @param magnitude 출력 크기 스펙트럼 [n_frames * (fft_size/2 + 1)]
 * @param phase 출력 위상 스펙트럼 [n_frames * (fft_size/2 + 1)]
 * @param n_frames 출력 프레임 수 포인터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_forward(ETSTFTContext* ctx, const float* audio, int audio_len,
                        float* magnitude, float* phase, int* n_frames);

/**
 * @brief ISTFT 변환 (최적화된 구현)
 *
 * 시간-주파수 도메인 데이터를 오디오 신호로 역변환합니다.
 * SIMD 최적화와 병렬 처리를 지원합니다.
 *
 * @param ctx STFT 컨텍스트
 * @param magnitude 입력 크기 스펙트럼 [n_frames * (fft_size/2 + 1)]
 * @param phase 입력 위상 스펙트럼 [n_frames * (fft_size/2 + 1)]
 * @param n_frames 입력 프레임 수
 * @param audio 출력 오디오 신호
 * @param audio_len 출력 오디오 신호 길이 포인터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_inverse(ETSTFTContext* ctx, const float* magnitude, const float* phase,
                        int n_frames, float* audio, int* audio_len);

/**
 * @brief 실시간 STFT 변환 (스트리밍)
 *
 * 실시간 처리를 위한 청크 단위 STFT 변환입니다.
 *
 * @param ctx STFT 컨텍스트
 * @param audio_chunk 입력 오디오 청크
 * @param chunk_size 청크 크기
 * @param magnitude 출력 크기 스펙트럼
 * @param phase 출력 위상 스펙트럼
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_forward_streaming(ETSTFTContext* ctx, const float* audio_chunk, int chunk_size,
                                  float* magnitude, float* phase);

/**
 * @brief 실시간 ISTFT 변환 (스트리밍)
 *
 * 실시간 처리를 위한 청크 단위 ISTFT 변환입니다.
 *
 * @param ctx STFT 컨텍스트
 * @param magnitude 입력 크기 스펙트럼
 * @param phase 입력 위상 스펙트럼
 * @param audio_chunk 출력 오디오 청크
 * @param chunk_size 청크 크기 포인터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_inverse_streaming(ETSTFTContext* ctx, const float* magnitude, const float* phase,
                                  float* audio_chunk, int* chunk_size);

// ============================================================================
// 윈도우 함수 최적화
// ============================================================================

/**
 * @brief 최적화된 윈도우 함수 생성
 *
 * @param window_type 윈도우 타입
 * @param size 윈도우 크기
 * @param window 출력 윈도우 배열
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_create_window(ETWindowType window_type, int size, float* window);

/**
 * @brief SIMD 최적화된 윈도우 적용
 *
 * 입력 신호에 윈도우 함수를 적용합니다.
 *
 * @param input 입력 신호
 * @param window 윈도우 함수
 * @param output 출력 신호
 * @param size 신호 크기
 */
void et_stft_apply_window_simd(const float* input, const float* window, float* output, int size);

/**
 * @brief 윈도우 함수 정규화 계수 계산
 *
 * ISTFT 복원을 위한 윈도우 정규화 계수를 계산합니다.
 *
 * @param window 윈도우 함수
 * @param size 윈도우 크기
 * @param hop_size Hop 크기
 * @return 정규화 계수
 */
float et_stft_window_normalization(const float* window, int size, int hop_size);

// ============================================================================
// FFT 최적화 함수
// ============================================================================

/**
 * @brief SIMD 최적화된 실수 FFT
 *
 * @param input 입력 실수 신호
 * @param output_real 출력 실수부
 * @param output_imag 출력 허수부
 * @param size FFT 크기 (2의 거듭제곱)
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_fft_real_simd(const float* input, float* output_real, float* output_imag, int size);

/**
 * @brief SIMD 최적화된 복소수 IFFT
 *
 * @param input_real 입력 실수부
 * @param input_imag 입력 허수부
 * @param output 출력 실수 신호
 * @param size FFT 크기 (2의 거듭제곱)
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_ifft_complex_simd(const float* input_real, const float* input_imag, float* output, int size);

/**
 * @brief 병렬 FFT 처리
 *
 * 여러 프레임을 병렬로 FFT 처리합니다.
 *
 * @param input 입력 신호 배열 [n_frames * fft_size]
 * @param output_real 출력 실수부 배열 [n_frames * (fft_size/2 + 1)]
 * @param output_imag 출력 허수부 배열 [n_frames * (fft_size/2 + 1)]
 * @param n_frames 프레임 수
 * @param fft_size FFT 크기
 * @param num_threads 스레드 수
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_fft_parallel(const float* input, float* output_real, float* output_imag,
                              int n_frames, int fft_size, int num_threads);

/**
 * @brief 병렬 IFFT 처리
 *
 * 여러 프레임을 병렬로 IFFT 처리합니다.
 *
 * @param input_real 입력 실수부 배열 [n_frames * (fft_size/2 + 1)]
 * @param input_imag 입력 허수부 배열 [n_frames * (fft_size/2 + 1)]
 * @param output 출력 신호 배열 [n_frames * fft_size]
 * @param n_frames 프레임 수
 * @param fft_size FFT 크기
 * @param num_threads 스레드 수
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_ifft_parallel(const float* input_real, const float* input_imag, float* output,
                               int n_frames, int fft_size, int num_threads);

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * @brief STFT 출력 크기 계산
 *
 * 주어진 오디오 길이와 설정에 대한 STFT 출력 크기를 계산합니다.
 *
 * @param audio_len 오디오 길이
 * @param fft_size FFT 크기
 * @param hop_size Hop 크기
 * @return 출력 프레임 수
 */
int et_stft_calculate_frames(int audio_len, int fft_size, int hop_size);

/**
 * @brief ISTFT 출력 크기 계산
 *
 * 주어진 프레임 수와 설정에 대한 ISTFT 출력 크기를 계산합니다.
 *
 * @param n_frames 프레임 수
 * @param fft_size FFT 크기
 * @param hop_size Hop 크기
 * @return 출력 오디오 길이
 */
int et_stft_calculate_audio_length(int n_frames, int fft_size, int hop_size);

/**
 * @brief 스펙트럼 크기 계산 (SIMD 최적화)
 *
 * 복소수 스펙트럼에서 크기를 계산합니다.
 *
 * @param real 실수부 배열
 * @param imag 허수부 배열
 * @param magnitude 출력 크기 배열
 * @param size 배열 크기
 */
void et_stft_magnitude_simd(const float* real, const float* imag, float* magnitude, int size);

/**
 * @brief 스펙트럼 위상 계산 (SIMD 최적화)
 *
 * 복소수 스펙트럼에서 위상을 계산합니다.
 *
 * @param real 실수부 배열
 * @param imag 허수부 배열
 * @param phase 출력 위상 배열
 * @param size 배열 크기
 */
void et_stft_phase_simd(const float* real, const float* imag, float* phase, int size);

/**
 * @brief 크기와 위상에서 복소수 복원 (SIMD 최적화)
 *
 * 크기와 위상 정보에서 복소수 스펙트럼을 복원합니다.
 *
 * @param magnitude 크기 배열
 * @param phase 위상 배열
 * @param real 출력 실수부 배열
 * @param imag 출력 허수부 배열
 * @param size 배열 크기
 */
void et_stft_polar_to_complex_simd(const float* magnitude, const float* phase,
                                  float* real, float* imag, int size);

/**
 * @brief STFT 성능 통계 조회
 *
 * @param ctx STFT 컨텍스트
 * @param forward_time 순방향 변환 평균 시간 (ms)
 * @param inverse_time 역방향 변환 평균 시간 (ms)
 * @param memory_usage 메모리 사용량 (bytes)
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_get_performance_stats(ETSTFTContext* ctx, float* forward_time,
                                      float* inverse_time, size_t* memory_usage);

/**
 * @brief STFT 컨텍스트 리셋
 *
 * 실시간 처리를 위한 내부 버퍼를 리셋합니다.
 *
 * @param ctx STFT 컨텍스트
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_stft_reset_context(ETSTFTContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_STFT_H