/**
 * @file mel_scale.h
 * @brief Mel 스케일 변환 최적화 구현 헤더
 * @author LibEtude Project
 * @version 1.0.0
 *
 * SIMD 최적화된 Mel 스케일 변환과 필터뱅크, 캐싱 최적화를 제공합니다.
 */

#ifndef LIBETUDE_MEL_SCALE_H
#define LIBETUDE_MEL_SCALE_H

#include "libetude/types.h"
#include "libetude/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Mel 스케일 변환 상수 및 타입
// ============================================================================

/**
 * @brief Mel 스케일 변환 타입
 */
typedef enum {
    ET_MEL_SCALE_HTK = 0,      /**< HTK 스타일 Mel 스케일 */
    ET_MEL_SCALE_SLANEY = 1    /**< Slaney 스타일 Mel 스케일 */
} ETMelScaleType;

/**
 * @brief Mel 필터뱅크 컨텍스트 구조체 (불투명 타입)
 */
typedef struct ETMelFilterbank ETMelFilterbank;

/**
 * @brief Mel 필터뱅크 설정 구조체
 */
typedef struct {
    int n_fft;                  /**< FFT 크기 */
    int n_mels;                 /**< Mel 필터 수 */
    float fmin;                 /**< 최소 주파수 (Hz) */
    float fmax;                 /**< 최대 주파수 (Hz) */
    int sample_rate;            /**< 샘플링 레이트 (Hz) */
    ETMelScaleType scale_type;  /**< Mel 스케일 타입 */
    bool enable_simd;           /**< SIMD 최적화 활성화 */
    bool enable_caching;        /**< 캐싱 최적화 활성화 */
    bool normalize;             /**< 필터 정규화 여부 */
} ETMelFilterbankConfig;

/**
 * @brief Mel 변환 통계 구조체
 */
typedef struct {
    float forward_time_ms;      /**< 순방향 변환 평균 시간 (ms) */
    float inverse_time_ms;      /**< 역방향 변환 평균 시간 (ms) */
    size_t memory_usage;        /**< 메모리 사용량 (bytes) */
    int cache_hits;             /**< 캐시 히트 수 */
    int cache_misses;           /**< 캐시 미스 수 */
} ETMelStats;

// ============================================================================
// Mel 필터뱅크 관리
// ============================================================================

/**
 * @brief Mel 필터뱅크 생성
 *
 * @param config Mel 필터뱅크 설정
 * @return Mel 필터뱅크 포인터 (실패시 NULL)
 */
ETMelFilterbank* et_mel_create_filterbank(const ETMelFilterbankConfig* config);

/**
 * @brief Mel 필터뱅크 해제
 *
 * @param mel_fb Mel 필터뱅크
 */
void et_mel_destroy_filterbank(ETMelFilterbank* mel_fb);

/**
 * @brief 기본 Mel 필터뱅크 설정 생성
 *
 * @param n_fft FFT 크기
 * @param n_mels Mel 필터 수
 * @param sample_rate 샘플링 레이트
 * @param fmin 최소 주파수 (0이면 자동 설정)
 * @param fmax 최대 주파수 (0이면 자동 설정)
 * @return Mel 필터뱅크 설정 구조체
 */
ETMelFilterbankConfig et_mel_default_config(int n_fft, int n_mels, int sample_rate,
                                           float fmin, float fmax);

/**
 * @brief Mel 필터뱅크 설정 업데이트
 *
 * @param mel_fb Mel 필터뱅크
 * @param config 새로운 설정
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_update_config(ETMelFilterbank* mel_fb, const ETMelFilterbankConfig* config);

// ============================================================================
// Mel 스케일 변환 핵심 함수
// ============================================================================

/**
 * @brief 스펙트로그램을 Mel 스펙트로그램으로 변환 (최적화된 구현)
 *
 * 선형 주파수 스펙트로그램을 Mel 스케일 스펙트로그램으로 변환합니다.
 * SIMD 최적화와 캐싱을 지원합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @param spectrogram 입력 스펙트로그램 [time_frames * (n_fft/2 + 1)]
 * @param time_frames 시간 프레임 수
 * @param mel_spec 출력 Mel 스펙트로그램 [time_frames * n_mels]
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_spectrogram_to_mel(ETMelFilterbank* mel_fb, const float* spectrogram,
                                  int time_frames, float* mel_spec);

/**
 * @brief Mel 스펙트로그램을 스펙트로그램으로 역변환 (최적화된 구현)
 *
 * Mel 스케일 스펙트로그램을 선형 주파수 스펙트로그램으로 역변환합니다.
 * 의사역행렬(pseudo-inverse)을 사용한 근사 복원입니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @param mel_spec 입력 Mel 스펙트로그램 [time_frames * n_mels]
 * @param time_frames 시간 프레임 수
 * @param spectrogram 출력 스펙트로그램 [time_frames * (n_fft/2 + 1)]
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_mel_to_spectrogram(ETMelFilterbank* mel_fb, const float* mel_spec,
                                  int time_frames, float* spectrogram);

/**
 * @brief 단일 프레임 Mel 변환 (실시간 처리용)
 *
 * 단일 스펙트럼 프레임을 Mel 스펙트럼으로 변환합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @param spectrum 입력 스펙트럼 [(n_fft/2 + 1)]
 * @param mel_frame 출력 Mel 프레임 [n_mels]
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_spectrum_to_mel_frame(ETMelFilterbank* mel_fb, const float* spectrum,
                                     float* mel_frame);

/**
 * @brief 단일 프레임 Mel 역변환 (실시간 처리용)
 *
 * 단일 Mel 프레임을 스펙트럼으로 역변환합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @param mel_frame 입력 Mel 프레임 [n_mels]
 * @param spectrum 출력 스펙트럼 [(n_fft/2 + 1)]
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_mel_frame_to_spectrum(ETMelFilterbank* mel_fb, const float* mel_frame,
                                     float* spectrum);

// ============================================================================
// Mel 스케일 변환 함수
// ============================================================================

/**
 * @brief Hz를 Mel로 변환
 *
 * @param hz 주파수 (Hz)
 * @param scale_type Mel 스케일 타입
 * @return Mel 값
 */
float et_mel_hz_to_mel(float hz, ETMelScaleType scale_type);

/**
 * @brief Mel을 Hz로 변환
 *
 * @param mel Mel 값
 * @param scale_type Mel 스케일 타입
 * @return 주파수 (Hz)
 */
float et_mel_mel_to_hz(float mel, ETMelScaleType scale_type);

/**
 * @brief Mel 주파수 포인트 생성
 *
 * 지정된 범위에서 균등하게 분포된 Mel 주파수 포인트를 생성합니다.
 *
 * @param fmin 최소 주파수 (Hz)
 * @param fmax 최대 주파수 (Hz)
 * @param n_mels Mel 필터 수
 * @param scale_type Mel 스케일 타입
 * @param mel_points 출력 Mel 포인트 배열 [n_mels + 2]
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_create_mel_points(float fmin, float fmax, int n_mels,
                                 ETMelScaleType scale_type, float* mel_points);

/**
 * @brief FFT bin을 Hz로 변환
 *
 * @param bin FFT bin 인덱스
 * @param n_fft FFT 크기
 * @param sample_rate 샘플링 레이트
 * @return 주파수 (Hz)
 */
float et_mel_fft_bin_to_hz(int bin, int n_fft, int sample_rate);

/**
 * @brief Hz를 FFT bin으로 변환
 *
 * @param hz 주파수 (Hz)
 * @param n_fft FFT 크기
 * @param sample_rate 샘플링 레이트
 * @return FFT bin 인덱스 (실수)
 */
float et_mel_hz_to_fft_bin(float hz, int n_fft, int sample_rate);

// ============================================================================
// 필터뱅크 생성 및 최적화
// ============================================================================

/**
 * @brief 삼각형 Mel 필터 생성
 *
 * 삼각형 모양의 Mel 필터를 생성합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_create_triangular_filters(ETMelFilterbank* mel_fb);

/**
 * @brief 필터뱅크 정규화
 *
 * 필터뱅크를 정규화하여 에너지 보존을 보장합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_normalize_filterbank(ETMelFilterbank* mel_fb);

/**
 * @brief 희소 필터뱅크 최적화
 *
 * 0이 아닌 값만 저장하는 희소 행렬 형태로 필터뱅크를 최적화합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_optimize_sparse_filterbank(ETMelFilterbank* mel_fb);

/**
 * @brief 의사역행렬 생성
 *
 * Mel 역변환을 위한 의사역행렬을 생성합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_create_pseudo_inverse(ETMelFilterbank* mel_fb);

// ============================================================================
// SIMD 최적화 함수
// ============================================================================

/**
 * @brief SIMD 최적화된 행렬-벡터 곱셈
 *
 * 필터뱅크와 스펙트럼의 행렬-벡터 곱셈을 SIMD로 최적화합니다.
 *
 * @param filters 필터 행렬 [n_mels * n_freq_bins]
 * @param spectrum 입력 스펙트럼 [n_freq_bins]
 * @param mel_frame 출력 Mel 프레임 [n_mels]
 * @param n_mels Mel 필터 수
 * @param n_freq_bins 주파수 bin 수
 */
void et_mel_matvec_simd(const float* filters, const float* spectrum, float* mel_frame,
                       int n_mels, int n_freq_bins);

/**
 * @brief SIMD 최적화된 희소 행렬-벡터 곱셈
 *
 * 희소 필터뱅크와 스펙트럼의 곱셈을 SIMD로 최적화합니다.
 *
 * @param sparse_filters 희소 필터 데이터
 * @param indices 인덱스 배열
 * @param indptr 포인터 배열
 * @param spectrum 입력 스펙트럼
 * @param mel_frame 출력 Mel 프레임
 * @param n_mels Mel 필터 수
 */
void et_mel_sparse_matvec_simd(const float* sparse_filters, const int* indices,
                              const int* indptr, const float* spectrum,
                              float* mel_frame, int n_mels);

/**
 * @brief SIMD 최적화된 배치 Mel 변환
 *
 * 여러 프레임을 한 번에 처리하는 배치 Mel 변환입니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @param spectrogram 입력 스펙트로그램 [time_frames * n_freq_bins]
 * @param mel_spec 출력 Mel 스펙트로그램 [time_frames * n_mels]
 * @param time_frames 시간 프레임 수
 * @param n_freq_bins 주파수 bin 수
 * @param n_mels Mel 필터 수
 */
void et_mel_batch_transform_simd(ETMelFilterbank* mel_fb, const float* spectrogram,
                                float* mel_spec, int time_frames, int n_freq_bins, int n_mels);

// ============================================================================
// 캐싱 및 사전 계산 최적화
// ============================================================================

/**
 * @brief 필터뱅크 캐시 초기화
 *
 * 자주 사용되는 필터뱅크 설정을 캐시합니다.
 *
 * @param cache_size 캐시 크기
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_init_cache(int cache_size);

/**
 * @brief 필터뱅크 캐시 해제
 */
void et_mel_destroy_cache(void);

/**
 * @brief 캐시에서 필터뱅크 조회
 *
 * @param config 필터뱅크 설정
 * @return 캐시된 필터뱅크 (없으면 NULL)
 */
ETMelFilterbank* et_mel_get_cached_filterbank(const ETMelFilterbankConfig* config);

/**
 * @brief 필터뱅크를 캐시에 저장
 *
 * @param config 필터뱅크 설정
 * @param mel_fb 필터뱅크
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_cache_filterbank(const ETMelFilterbankConfig* config, ETMelFilterbank* mel_fb);

/**
 * @brief 사전 계산된 상수 테이블 초기화
 *
 * Mel 변환에 필요한 상수들을 사전 계산하여 테이블로 저장합니다.
 *
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_init_precomputed_tables(void);

/**
 * @brief 사전 계산된 상수 테이블 해제
 */
void et_mel_destroy_precomputed_tables(void);

// ============================================================================
// 유틸리티 및 통계 함수
// ============================================================================

/**
 * @brief Mel 필터뱅크 정보 조회
 *
 * @param mel_fb Mel 필터뱅크
 * @param n_fft FFT 크기 포인터
 * @param n_mels Mel 필터 수 포인터
 * @param sample_rate 샘플링 레이트 포인터
 * @param fmin 최소 주파수 포인터
 * @param fmax 최대 주파수 포인터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_get_filterbank_info(ETMelFilterbank* mel_fb, int* n_fft, int* n_mels,
                                   int* sample_rate, float* fmin, float* fmax);

/**
 * @brief Mel 변환 성능 통계 조회
 *
 * @param mel_fb Mel 필터뱅크
 * @param stats 성능 통계 구조체
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_get_performance_stats(ETMelFilterbank* mel_fb, ETMelStats* stats);

/**
 * @brief Mel 필터뱅크 리셋
 *
 * 성능 통계와 캐시를 리셋합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_reset_filterbank(ETMelFilterbank* mel_fb);

/**
 * @brief Mel 필터 시각화 데이터 생성
 *
 * 디버깅 및 시각화를 위한 필터 응답 데이터를 생성합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @param filter_responses 출력 필터 응답 [n_mels * n_freq_bins]
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_get_filter_responses(ETMelFilterbank* mel_fb, float* filter_responses);

/**
 * @brief Mel 스케일 변환 정확도 검증
 *
 * 순방향-역방향 변환의 정확도를 검증합니다.
 *
 * @param mel_fb Mel 필터뱅크
 * @param test_spectrum 테스트 스펙트럼
 * @param n_freq_bins 주파수 bin 수
 * @param reconstruction_error 재구성 오차 포인터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_mel_verify_accuracy(ETMelFilterbank* mel_fb, const float* test_spectrum,
                               int n_freq_bins, float* reconstruction_error);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_MEL_SCALE_H