/**
 * @file world_engine.h
 * @brief WORLD 알고리즘 엔진 인터페이스 헤더 파일
 *
 * WORLD 보코더 알고리즘의 분석 및 합성 엔진을 정의합니다.
 * libetude의 최적화된 DSP 기능과 통합된 WORLD 구현을 제공합니다.
 */

#ifndef WORLD4UTAU_WORLD_ENGINE_H
#define WORLD4UTAU_WORLD_ENGINE_H

#include <libetude/api.h>
#include <libetude/types.h>
#include <libetude/error.h>
#include <libetude/stft.h>
#include <libetude/vocoder.h>
#include <libetude/memory.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 전방 선언
typedef struct WorldAnalysisEngine WorldAnalysisEngine;
typedef struct WorldSynthesisEngine WorldSynthesisEngine;
typedef struct WorldParameters WorldParameters;

/**
 * @brief WORLD 파라미터 구조체
 *
 * WORLD 보코더 알고리즘의 분석 결과를 저장하는 구조체입니다.
 * F0, 스펙트럼, 비주기성 정보를 포함합니다.
 */
typedef struct WorldParameters {
    // 기본 정보
    int sample_rate;             /**< 샘플링 레이트 (Hz) */
    int audio_length;            /**< 오디오 길이 (샘플) */
    double frame_period;         /**< 프레임 주기 (ms) */
    int f0_length;               /**< F0 배열 길이 */
    int fft_size;                /**< FFT 크기 */

    // F0 정보
    double* f0;                  /**< F0 배열 (Hz) */
    double* time_axis;           /**< 시간축 배열 (초) */

    // 스펙트럼 정보
    double** spectrogram;        /**< 스펙트로그램 [f0_length][fft_size/2+1] */

    // 비주기성 정보
    double** aperiodicity;       /**< 비주기성 [f0_length][fft_size/2+1] */

    // 메모리 관리
    bool owns_memory;            /**< 메모리 소유권 플래그 */
    ETMemoryPool* mem_pool;      /**< 메모리 풀 참조 */
} WorldParameters;

/**
 * @brief WORLD F0 추출 설정
 */
typedef struct {
    double frame_period;         /**< 프레임 주기 (ms, 기본값: 5.0) */
    double f0_floor;             /**< 최소 F0 (Hz, 기본값: 71.0) */
    double f0_ceil;              /**< 최대 F0 (Hz, 기본값: 800.0) */
    int algorithm;               /**< 알고리즘 (0: DIO, 1: Harvest) */
    double channels_in_octave;   /**< 옥타브당 채널 수 (기본값: 2.0) */
    double speed;                /**< 처리 속도 (1: 정확, 높을수록 빠름) */
    double allowed_range;        /**< 허용 범위 (기본값: 0.1) */
} WorldF0Config;

/**
 * @brief WORLD 스펙트럼 분석 설정
 */
typedef struct {
    double q1;                   /**< CheapTrick Q1 파라미터 (기본값: -0.15) */
    int fft_size;                /**< FFT 크기 (0: 자동 계산) */
} WorldSpectrumConfig;

/**
 * @brief WORLD 비주기성 분석 설정
 */
typedef struct {
    double threshold;            /**< D4C 임계값 (기본값: 0.85) */
} WorldAperiodicityConfig;

/**
 * @brief WORLD 분석 엔진 설정
 */
typedef struct {
    WorldF0Config f0_config;                    /**< F0 추출 설정 */
    WorldSpectrumConfig spectrum_config;        /**< 스펙트럼 분석 설정 */
    WorldAperiodicityConfig aperiodicity_config; /**< 비주기성 분석 설정 */

    // libetude 통합 설정
    bool enable_simd_optimization;              /**< SIMD 최적화 사용 여부 */
    bool enable_gpu_acceleration;               /**< GPU 가속 사용 여부 */
    size_t memory_pool_size;                    /**< 메모리 풀 크기 (바이트) */
} WorldAnalysisConfig;

/**
 * @brief WORLD 분석 엔진 구조체
 */
typedef struct WorldAnalysisEngine {
    // 설정
    WorldAnalysisConfig config;

    // libetude 통합
    ETSTFTContext* stft_ctx;     /**< STFT 컨텍스트 */
    ETMemoryPool* mem_pool;      /**< 메모리 풀 */

    // 내부 버퍼
    double* work_buffer;         /**< 작업용 버퍼 */
    size_t work_buffer_size;     /**< 작업용 버퍼 크기 */

    // 상태 정보
    bool is_initialized;         /**< 초기화 상태 */
    int last_sample_rate;        /**< 마지막 처리한 샘플링 레이트 */
} WorldAnalysisEngine;

/**
 * @brief WORLD 합성 엔진 설정
 */
typedef struct {
    int sample_rate;             /**< 샘플링 레이트 (Hz) */
    double frame_period;         /**< 프레임 주기 (ms) */
    bool enable_postfilter;      /**< 후처리 필터 사용 여부 */

    // libetude 통합 설정
    bool enable_simd_optimization;  /**< SIMD 최적화 사용 여부 */
    bool enable_gpu_acceleration;   /**< GPU 가속 사용 여부 */
    size_t memory_pool_size;        /**< 메모리 풀 크기 (바이트) */
} WorldSynthesisConfig;

/**
 * @brief WORLD 합성 엔진 구조체
 */
typedef struct WorldSynthesisEngine {
    // 설정
    WorldSynthesisConfig config;

    // libetude 통합
    ETVocoderContext* vocoder_ctx;  /**< 보코더 컨텍스트 */
    ETAudioBuffer* output_buffer;   /**< 출력 버퍼 */
    ETMemoryPool* mem_pool;         /**< 메모리 풀 */

    // 내부 버퍼
    double* synthesis_buffer;       /**< 합성용 버퍼 */
    size_t synthesis_buffer_size;   /**< 합성용 버퍼 크기 */

    // 상태 정보
    bool is_initialized;            /**< 초기화 상태 */
} WorldSynthesisEngine;

/**
 * @brief 스트리밍 오디오 콜백 함수 타입
 *
 * @param audio_data 오디오 데이터 포인터
 * @param sample_count 샘플 수
 * @param user_data 사용자 데이터
 * @return true 계속 처리, false 중단
 */
typedef bool (*AudioStreamCallback)(const float* audio_data, int sample_count, void* user_data);

// ============================================================================
// WorldParameters 관리 함수들
// ============================================================================

/**
 * @brief WorldParameters 생성
 *
 * @param f0_length F0 배열 길이
 * @param fft_size FFT 크기
 * @param pool 메모리 풀 (NULL이면 기본 할당자 사용)
 * @return 생성된 WorldParameters 포인터, 실패시 NULL
 */
WorldParameters* world_parameters_create(int f0_length, int fft_size, ETMemoryPool* pool);

/**
 * @brief WorldParameters 해제
 *
 * @param params 해제할 WorldParameters
 */
void world_parameters_destroy(WorldParameters* params);

/**
 * @brief WorldParameters 복사
 *
 * @param src 원본 WorldParameters
 * @param dst 대상 WorldParameters
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_parameters_copy(const WorldParameters* src, WorldParameters* dst);

/**
 * @brief WorldParameters 초기화
 *
 * @param params 초기화할 WorldParameters
 * @param sample_rate 샘플링 레이트
 * @param audio_length 오디오 길이
 * @param frame_period 프레임 주기
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_parameters_init(WorldParameters* params, int sample_rate,
                              int audio_length, double frame_period);

// ============================================================================
// WORLD 분석 엔진 함수들
// ============================================================================

/**
 * @brief WORLD 분석 엔진 생성
 *
 * @param config 분석 엔진 설정
 * @return 생성된 분석 엔진 포인터, 실패시 NULL
 */
WorldAnalysisEngine* world_analysis_create(const WorldAnalysisConfig* config);

/**
 * @brief WORLD 분석 엔진 해제
 *
 * @param engine 해제할 분석 엔진
 */
void world_analysis_destroy(WorldAnalysisEngine* engine);

/**
 * @brief 음성 분석 수행
 *
 * F0, 스펙트럼, 비주기성을 모두 분석합니다.
 *
 * @param engine 분석 엔진
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param params 분석 결과를 저장할 WorldParameters
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_analyze_audio(WorldAnalysisEngine* engine,
                            const float* audio, int audio_length, int sample_rate,
                            WorldParameters* params);

/**
 * @brief F0 추출
 *
 * @param engine 분석 엔진
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 결과 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_extract_f0(WorldAnalysisEngine* engine,
                         const float* audio, int audio_length, int sample_rate,
                         double* f0, double* time_axis, int f0_length);

/**
 * @brief 스펙트럼 분석
 *
 * @param engine 분석 엔진
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @param spectrogram 스펙트로그램 결과
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_analyze_spectrum(WorldAnalysisEngine* engine,
                               const float* audio, int audio_length, int sample_rate,
                               const double* f0, const double* time_axis, int f0_length,
                               double** spectrogram);

/**
 * @brief 비주기성 분석
 *
 * @param engine 분석 엔진
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @param aperiodicity 비주기성 결과
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_analyze_aperiodicity(WorldAnalysisEngine* engine,
                                   const float* audio, int audio_length, int sample_rate,
                                   const double* f0, const double* time_axis, int f0_length,
                                   double** aperiodicity);

// ============================================================================
// WORLD 합성 엔진 함수들
// ============================================================================

/**
 * @brief WORLD 합성 엔진 생성
 *
 * @param config 합성 엔진 설정
 * @return 생성된 합성 엔진 포인터, 실패시 NULL
 */
WorldSynthesisEngine* world_synthesis_create(const WorldSynthesisConfig* config);

/**
 * @brief WORLD 합성 엔진 해제
 *
 * @param engine 해제할 합성 엔진
 */
void world_synthesis_destroy(WorldSynthesisEngine* engine);

/**
 * @brief 음성 합성 수행
 *
 * @param engine 합성 엔진
 * @param params WORLD 파라미터
 * @param output_audio 출력 오디오 버퍼
 * @param output_length 출력 오디오 길이 (입력시 버퍼 크기, 출력시 실제 길이)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_synthesize_audio(WorldSynthesisEngine* engine,
                               const WorldParameters* params,
                               float* output_audio, int* output_length);

/**
 * @brief 실시간 스트리밍 합성
 *
 * @param engine 합성 엔진
 * @param params WORLD 파라미터
 * @param callback 오디오 스트림 콜백 함수
 * @param user_data 사용자 데이터
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_synthesize_streaming(WorldSynthesisEngine* engine,
                                   const WorldParameters* params,
                                   AudioStreamCallback callback, void* user_data);

// ============================================================================
// 유틸리티 함수들
// ============================================================================

/**
 * @brief 기본 분석 설정 가져오기
 *
 * @param config 설정을 저장할 구조체
 */
void world_get_default_analysis_config(WorldAnalysisConfig* config);

/**
 * @brief 기본 합성 설정 가져오기
 *
 * @param config 설정을 저장할 구조체
 */
void world_get_default_synthesis_config(WorldSynthesisConfig* config);

/**
 * @brief FFT 크기 계산
 *
 * @param sample_rate 샘플링 레이트
 * @return 권장 FFT 크기
 */
int world_get_fft_size_for_cheaptrick(int sample_rate);

/**
 * @brief F0 길이 계산
 *
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param frame_period 프레임 주기 (ms)
 * @return F0 배열 길이
 */
int world_get_samples_for_dio(int audio_length, int sample_rate, double frame_period);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_WORLD_ENGINE_H