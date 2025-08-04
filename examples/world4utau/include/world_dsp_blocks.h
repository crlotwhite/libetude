/**
 * @file world_dsp_blocks.h
 * @brief WORLD 전용 DSP 블록 인터페이스
 *
 * WORLD 알고리즘의 각 처리 단계를 DSP 블록으로 구현한 인터페이스를 제공합니다.
 */

#ifndef WORLD4UTAU_WORLD_DSP_BLOCKS_H
#define WORLD4UTAU_WORLD_DSP_BLOCKS_H

#include "dsp_blocks.h"
#include "world_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// WORLD DSP 블록 데이터 구조체들
// =============================================================================

/**
 * @brief 오디오 입력 블록 데이터
 */
typedef struct {
    float* audio_buffer;            ///< 오디오 데이터 버퍼
    int audio_length;               ///< 오디오 길이
    int sample_rate;                ///< 샘플링 레이트
    int current_position;           ///< 현재 읽기 위치
    int frame_size;                 ///< 프레임 크기
} AudioInputBlockData;

/**
 * @brief F0 추출 블록 데이터
 */
typedef struct {
    WorldF0Extractor* extractor;    ///< F0 추출기
    F0ExtractionConfig config;      ///< F0 추출 설정
    float* input_buffer;            ///< 입력 오디오 버퍼
    double* f0_output;              ///< F0 출력 버퍼
    double* time_axis;              ///< 시간축 버퍼
    int frame_count;                ///< 프레임 수
    int current_frame;              ///< 현재 처리 프레임
} F0ExtractionBlockData;

/**
 * @brief 스펙트럼 분석 블록 데이터
 */
typedef struct {
    WorldSpectrumAnalyzer* analyzer; ///< 스펙트럼 분석기
    SpectrumConfig config;          ///< 스펙트럼 분석 설정
    float* input_buffer;            ///< 입력 오디오 버퍼
    double* f0_input;               ///< F0 입력 버퍼
    double** spectrum_output;       ///< 스펙트럼 출력 버퍼
    int frame_count;                ///< 프레임 수
    int fft_size;                   ///< FFT 크기
} SpectrumAnalysisBlockData;

/**
 * @brief 비주기성 분석 블록 데이터
 */
typedef struct {
    WorldAperiodicityAnalyzer* analyzer; ///< 비주기성 분석기
    AperiodicityConfig config;      ///< 비주기성 분석 설정
    float* input_buffer;            ///< 입력 오디오 버퍼
    double* f0_input;               ///< F0 입력 버퍼
    double** aperiodicity_output;   ///< 비주기성 출력 버퍼
    int frame_count;                ///< 프레임 수
    int fft_size;                   ///< FFT 크기
} AperiodicityAnalysisBlockData;

/**
 * @brief 파라미터 병합 블록 데이터
 */
typedef struct {
    WorldParameters* world_params;  ///< WORLD 파라미터
    double* f0_input;               ///< F0 입력 버퍼
    double** spectrum_input;        ///< 스펙트럼 입력 버퍼
    double** aperiodicity_input;    ///< 비주기성 입력 버퍼
    int frame_count;                ///< 프레임 수
    int fft_size;                   ///< FFT 크기
    bool is_merged;                 ///< 병합 완료 플래그
} ParameterMergeBlockData;

/**
 * @brief 음성 합성 블록 데이터
 */
typedef struct {
    WorldSynthesisEngine* engine;   ///< 합성 엔진
    SynthesisConfig config;         ///< 합성 설정
    WorldParameters* input_params;  ///< 입력 WORLD 파라미터
    float* audio_output;            ///< 출력 오디오 버퍼
    int output_length;              ///< 출력 길이
    int sample_rate;                ///< 샘플링 레이트
} SynthesisBlockData;

/**
 * @brief 오디오 출력 블록 데이터
 */
typedef struct {
    float* audio_buffer;            ///< 오디오 출력 버퍼
    int buffer_size;                ///< 버퍼 크기
    int sample_rate;                ///< 샘플링 레이트
    char output_filename[256];      ///< 출력 파일명
    bool write_to_file;             ///< 파일 쓰기 플래그
} AudioOutputBlockData;

// =============================================================================
// WORLD DSP 블록 생성 함수들
// =============================================================================

/**
 * @brief 오디오 입력 블록 생성
 *
 * @param name 블록 이름
 * @param audio_buffer 오디오 데이터
 * @param audio_length 오디오 길이
 * @param sample_rate 샘플링 레이트
 * @param frame_size 프레임 크기
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* create_audio_input_block(const char* name,
                                  const float* audio_buffer, int audio_length,
                                  int sample_rate, int frame_size,
                                  ETMemoryPool* mem_pool);

/**
 * @brief F0 추출 블록 생성
 *
 * @param name 블록 이름
 * @param config F0 추출 설정
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* create_f0_extraction_block(const char* name,
                                    const F0ExtractionConfig* config,
                                    ETMemoryPool* mem_pool);

/**
 * @brief 스펙트럼 분석 블록 생성
 *
 * @param name 블록 이름
 * @param config 스펙트럼 분석 설정
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* create_spectrum_analysis_block(const char* name,
                                        const SpectrumConfig* config,
                                        ETMemoryPool* mem_pool);

/**
 * @brief 비주기성 분석 블록 생성
 *
 * @param name 블록 이름
 * @param config 비주기성 분석 설정
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* create_aperiodicity_analysis_block(const char* name,
                                            const AperiodicityConfig* config,
                                            ETMemoryPool* mem_pool);

/**
 * @brief 파라미터 병합 블록 생성
 *
 * @param name 블록 이름
 * @param frame_count 프레임 수
 * @param fft_size FFT 크기
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* create_parameter_merge_block(const char* name,
                                      int frame_count, int fft_size,
                                      ETMemoryPool* mem_pool);

/**
 * @brief 음성 합성 블록 생성
 *
 * @param name 블록 이름
 * @param config 합성 설정
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* create_synthesis_block(const char* name,
                                const SynthesisConfig* config,
                                ETMemoryPool* mem_pool);

/**
 * @brief 오디오 출력 블록 생성
 *
 * @param name 블록 이름
 * @param buffer_size 버퍼 크기
 * @param sample_rate 샘플링 레이트
 * @param output_filename 출력 파일명 (NULL이면 파일 쓰기 안함)
 * @param mem_pool 메모리 풀
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* create_audio_output_block(const char* name,
                                   int buffer_size, int sample_rate,
                                   const char* output_filename,
                                   ETMemoryPool* mem_pool);

// =============================================================================
// WORLD DSP 블록 처리 함수들 (내부 사용)
// =============================================================================

/**
 * @brief 오디오 입력 블록 처리 함수
 */
ETResult audio_input_block_process(DSPBlock* block, int frame_count);

/**
 * @brief F0 추출 블록 처리 함수
 */
ETResult f0_extraction_block_process(DSPBlock* block, int frame_count);

/**
 * @brief 스펙트럼 분석 블록 처리 함수
 */
ETResult spectrum_analysis_block_process(DSPBlock* block, int frame_count);

/**
 * @brief 비주기성 분석 블록 처리 함수
 */
ETResult aperiodicity_analysis_block_process(DSPBlock* block, int frame_count);

/**
 * @brief 파라미터 병합 블록 처리 함수
 */
ETResult parameter_merge_block_process(DSPBlock* block, int frame_count);

/**
 * @brief 음성 합성 블록 처리 함수
 */
ETResult synthesis_block_process(DSPBlock* block, int frame_count);

/**
 * @brief 오디오 출력 블록 처리 함수
 */
ETResult audio_output_block_process(DSPBlock* block, int frame_count);

// =============================================================================
// WORLD DSP 블록 초기화/정리 함수들 (내부 사용)
// =============================================================================

/**
 * @brief 오디오 입력 블록 초기화 함수
 */
ETResult audio_input_block_initialize(DSPBlock* block);

/**
 * @brief F0 추출 블록 초기화 함수
 */
ETResult f0_extraction_block_initialize(DSPBlock* block);

/**
 * @brief 스펙트럼 분석 블록 초기화 함수
 */
ETResult spectrum_analysis_block_initialize(DSPBlock* block);

/**
 * @brief 비주기성 분석 블록 초기화 함수
 */
ETResult aperiodicity_analysis_block_initialize(DSPBlock* block);

/**
 * @brief 파라미터 병합 블록 초기화 함수
 */
ETResult parameter_merge_block_initialize(DSPBlock* block);

/**
 * @brief 음성 합성 블록 초기화 함수
 */
ETResult synthesis_block_initialize(DSPBlock* block);

/**
 * @brief 오디오 출력 블록 초기화 함수
 */
ETResult audio_output_block_initialize(DSPBlock* block);

/**
 * @brief 오디오 입력 블록 정리 함수
 */
void audio_input_block_cleanup(DSPBlock* block);

/**
 * @brief F0 추출 블록 정리 함수
 */
void f0_extraction_block_cleanup(DSPBlock* block);

/**
 * @brief 스펙트럼 분석 블록 정리 함수
 */
void spectrum_analysis_block_cleanup(DSPBlock* block);

/**
 * @brief 비주기성 분석 블록 정리 함수
 */
void aperiodicity_analysis_block_cleanup(DSPBlock* block);

/**
 * @brief 파라미터 병합 블록 정리 함수
 */
void parameter_merge_block_cleanup(DSPBlock* block);

/**
 * @brief 음성 합성 블록 정리 함수
 */
void synthesis_block_cleanup(DSPBlock* block);

/**
 * @brief 오디오 출력 블록 정리 함수
 */
void audio_output_block_cleanup(DSPBlock* block);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_WORLD_DSP_BLOCKS_H