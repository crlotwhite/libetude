/**
 * @file dsp_block_factory.h
 * @brief DSP 블록 팩토리 인터페이스
 *
 * 설정 기반으로 DSP 블록을 생성하고 관리하는 팩토리 패턴 인터페이스를 제공합니다.
 */

#ifndef WORLD4UTAU_DSP_BLOCK_FACTORY_H
#define WORLD4UTAU_DSP_BLOCK_FACTORY_H

#include "world_dsp_blocks.h"
#include <libetude/config.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 블록 설정 구조체들
// =============================================================================

/**
 * @brief 오디오 입력 블록 설정
 */
typedef struct {
    char name[64];                  ///< 블록 이름
    const float* audio_buffer;      ///< 오디오 데이터
    int audio_length;               ///< 오디오 길이
    int sample_rate;                ///< 샘플링 레이트
    int frame_size;                 ///< 프레임 크기
    bool auto_calculate_frame_size; ///< 프레임 크기 자동 계산
    double frame_period_ms;         ///< 프레임 주기 (자동 계산용)
} AudioInputBlockConfig;

/**
 * @brief F0 추출 블록 설정
 */
typedef struct {
    char name[64];                  ///< 블록 이름
    F0ExtractionConfig f0_config;   ///< F0 추출 설정
    bool use_default_config;        ///< 기본 설정 사용 여부
} F0ExtractionBlockConfig;

/**
 * @brief 스펙트럼 분석 블록 설정
 */
typedef struct {
    char name[64];                  ///< 블록 이름
    SpectrumConfig spectrum_config; ///< 스펙트럼 분석 설정
    bool use_default_config;        ///< 기본 설정 사용 여부
} SpectrumAnalysisBlockConfig;

/**
 * @brief 비주기성 분석 블록 설정
 */
typedef struct {
    char name[64];                  ///< 블록 이름
    AperiodicityConfig aperiodicity_config; ///< 비주기성 분석 설정
    bool use_default_config;        ///< 기본 설정 사용 여부
} AperiodicityAnalysisBlockConfig;

/**
 * @brief 파라미터 병합 블록 설정
 */
typedef struct {
    char name[64];                  ///< 블록 이름
    int frame_count;                ///< 프레임 수
    int fft_size;                   ///< FFT 크기
    bool auto_calculate_frame_count; ///< 프레임 수 자동 계산
    int audio_length;               ///< 오디오 길이 (자동 계산용)
    double frame_period_ms;         ///< 프레임 주기 (자동 계산용)
    int sample_rate;                ///< 샘플링 레이트 (자동 계산용)
} ParameterMergeBlockConfig;

/**
 * @brief 음성 합성 블록 설정
 */
typedef struct {
    char name[64];                  ///< 블록 이름
    SynthesisConfig synthesis_config; ///< 합성 설정
    bool use_default_config;        ///< 기본 설정 사용 여부
} SynthesisBlockConfig;

/**
 * @brief 오디오 출력 블록 설정
 */
typedef struct {
    char name[64];                  ///< 블록 이름
    int buffer_size;                ///< 버퍼 크기
    int sample_rate;                ///< 샘플링 레이트
    char output_filename[256];      ///< 출력 파일명
    bool enable_file_output;        ///< 파일 출력 활성화
    bool auto_calculate_buffer_size; ///< 버퍼 크기 자동 계산
    double max_duration_sec;        ///< 최대 지속 시간 (자동 계산용)
} AudioOutputBlockConfig;

/**
 * @brief 블록 타입별 설정 통합 구조체
 */
typedef struct {
    DSPBlockType block_type;        ///< 블록 타입
    union {
        AudioInputBlockConfig audio_input;
        F0ExtractionBlockConfig f0_extraction;
        SpectrumAnalysisBlockConfig spectrum_analysis;
        AperiodicityAnalysisBlockConfig aperiodicity_analysis;
        ParameterMergeBlockConfig parameter_merge;
        SynthesisBlockConfig synthesis;
        AudioOutputBlockConfig audio_output;
    } config;                       ///< 타입별 설정
} DSPBlockConfig;

// =============================================================================
// DSP 블록 팩토리 구조체
// =============================================================================

/**
 * @brief DSP 블록 팩토리 구조체
 */
typedef struct {
    ETMemoryPool* mem_pool;         ///< 메모리 풀

    // 기본 설정들
    F0ExtractionConfig default_f0_config;
    SpectrumConfig default_spectrum_config;
    AperiodicityConfig default_aperiodicity_config;
    SynthesisConfig default_synthesis_config;

    // 팩토리 통계
    int blocks_created;             ///< 생성된 블록 수
    int blocks_destroyed;           ///< 해제된 블록 수

    // 에러 정보
    char last_error[256];           ///< 마지막 에러 메시지
} DSPBlockFactory;

// =============================================================================
// DSP 블록 팩토리 기본 함수들
// =============================================================================

/**
 * @brief DSP 블록 팩토리 생성
 *
 * @param mem_pool 메모리 풀
 * @return DSPBlockFactory* 생성된 팩토리 (실패 시 NULL)
 */
DSPBlockFactory* dsp_block_factory_create(ETMemoryPool* mem_pool);

/**
 * @brief DSP 블록 팩토리 해제
 *
 * @param factory 해제할 팩토리
 */
void dsp_block_factory_destroy(DSPBlockFactory* factory);

/**
 * @brief 팩토리 기본 설정 초기화
 *
 * @param factory 팩토리
 * @param sample_rate 기본 샘플링 레이트
 * @param frame_period_ms 기본 프레임 주기
 * @param fft_size 기본 FFT 크기
 * @return ETResult 초기화 결과
 */
ETResult dsp_block_factory_initialize_defaults(DSPBlockFactory* factory,
                                              int sample_rate,
                                              double frame_period_ms,
                                              int fft_size);

/**
 * @brief 팩토리 에러 메시지 가져오기
 *
 * @param factory 팩토리
 * @return const char* 에러 메시지
 */
const char* dsp_block_factory_get_error(DSPBlockFactory* factory);

/**
 * @brief 팩토리 통계 출력
 *
 * @param factory 팩토리
 */
void dsp_block_factory_print_stats(DSPBlockFactory* factory);

// =============================================================================
// 블록 생성 함수들
// =============================================================================

/**
 * @brief 설정 기반 DSP 블록 생성
 *
 * @param factory 팩토리
 * @param config 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_block(DSPBlockFactory* factory,
                                         const DSPBlockConfig* config);

/**
 * @brief 오디오 입력 블록 생성
 *
 * @param factory 팩토리
 * @param config 오디오 입력 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_audio_input(DSPBlockFactory* factory,
                                               const AudioInputBlockConfig* config);

/**
 * @brief F0 추출 블록 생성
 *
 * @param factory 팩토리
 * @param config F0 추출 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_f0_extraction(DSPBlockFactory* factory,
                                                 const F0ExtractionBlockConfig* config);

/**
 * @brief 스펙트럼 분석 블록 생성
 *
 * @param factory 팩토리
 * @param config 스펙트럼 분석 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_spectrum_analysis(DSPBlockFactory* factory,
                                                    const SpectrumAnalysisBlockConfig* config);

/**
 * @brief 비주기성 분석 블록 생성
 *
 * @param factory 팩토리
 * @param config 비주기성 분석 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_aperiodicity_analysis(DSPBlockFactory* factory,
                                                        const AperiodicityAnalysisBlockConfig* config);

/**
 * @brief 파라미터 병합 블록 생성
 *
 * @param factory 팩토리
 * @param config 파라미터 병합 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_parameter_merge(DSPBlockFactory* factory,
                                                   const ParameterMergeBlockConfig* config);

/**
 * @brief 음성 합성 블록 생성
 *
 * @param factory 팩토리
 * @param config 음성 합성 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_synthesis(DSPBlockFactory* factory,
                                            const SynthesisBlockConfig* config);

/**
 * @brief 오디오 출력 블록 생성
 *
 * @param factory 팩토리
 * @param config 오디오 출력 블록 설정
 * @return DSPBlock* 생성된 블록 (실패 시 NULL)
 */
DSPBlock* dsp_block_factory_create_audio_output(DSPBlockFactory* factory,
                                                const AudioOutputBlockConfig* config);

/**
 * @brief DSP 블록 해제 (팩토리를 통한 해제)
 *
 * @param factory 팩토리
 * @param block 해제할 블록
 */
void dsp_block_factory_destroy_block(DSPBlockFactory* factory, DSPBlock* block);

// =============================================================================
// 설정 헬퍼 함수들
// =============================================================================

/**
 * @brief 오디오 입력 블록 설정 초기화
 *
 * @param config 설정 구조체
 * @param name 블록 이름
 * @param audio_buffer 오디오 데이터
 * @param audio_length 오디오 길이
 * @param sample_rate 샘플링 레이트
 */
void dsp_block_factory_init_audio_input_config(AudioInputBlockConfig* config,
                                               const char* name,
                                               const float* audio_buffer,
                                               int audio_length,
                                               int sample_rate);

/**
 * @brief F0 추출 블록 설정 초기화
 *
 * @param config 설정 구조체
 * @param name 블록 이름
 * @param use_default 기본 설정 사용 여부
 */
void dsp_block_factory_init_f0_extraction_config(F0ExtractionBlockConfig* config,
                                                 const char* name,
                                                 bool use_default);

/**
 * @brief 스펙트럼 분석 블록 설정 초기화
 *
 * @param config 설정 구조체
 * @param name 블록 이름
 * @param use_default 기본 설정 사용 여부
 */
void dsp_block_factory_init_spectrum_analysis_config(SpectrumAnalysisBlockConfig* config,
                                                     const char* name,
                                                     bool use_default);

/**
 * @brief 비주기성 분석 블록 설정 초기화
 *
 * @param config 설정 구조체
 * @param name 블록 이름
 * @param use_default 기본 설정 사용 여부
 */
void dsp_block_factory_init_aperiodicity_analysis_config(AperiodicityAnalysisBlockConfig* config,
                                                         const char* name,
                                                         bool use_default);

/**
 * @brief 파라미터 병합 블록 설정 초기화
 *
 * @param config 설정 구조체
 * @param name 블록 이름
 * @param frame_count 프레임 수
 * @param fft_size FFT 크기
 */
void dsp_block_factory_init_parameter_merge_config(ParameterMergeBlockConfig* config,
                                                   const char* name,
                                                   int frame_count,
                                                   int fft_size);

/**
 * @brief 음성 합성 블록 설정 초기화
 *
 * @param config 설정 구조체
 * @param name 블록 이름
 * @param use_default 기본 설정 사용 여부
 */
void dsp_block_factory_init_synthesis_config(SynthesisBlockConfig* config,
                                             const char* name,
                                             bool use_default);

/**
 * @brief 오디오 출력 블록 설정 초기화
 *
 * @param config 설정 구조체
 * @param name 블록 이름
 * @param sample_rate 샘플링 레이트
 * @param output_filename 출력 파일명
 */
void dsp_block_factory_init_audio_output_config(AudioOutputBlockConfig* config,
                                                const char* name,
                                                int sample_rate,
                                                const char* output_filename);

// =============================================================================
// 배치 생성 함수들
// =============================================================================

/**
 * @brief 여러 블록을 배치로 생성
 *
 * @param factory 팩토리
 * @param configs 블록 설정 배열
 * @param config_count 설정 배열 크기
 * @param blocks 생성된 블록 배열 (출력)
 * @param max_blocks 블록 배열 최대 크기
 * @return int 생성된 블록 수 (실패 시 -1)
 */
int dsp_block_factory_create_blocks_batch(DSPBlockFactory* factory,
                                          const DSPBlockConfig* configs,
                                          int config_count,
                                          DSPBlock** blocks,
                                          int max_blocks);

/**
 * @brief 여러 블록을 배치로 해제
 *
 * @param factory 팩토리
 * @param blocks 해제할 블록 배열
 * @param block_count 블록 수
 */
void dsp_block_factory_destroy_blocks_batch(DSPBlockFactory* factory,
                                            DSPBlock** blocks,
                                            int block_count);

// =============================================================================
// 설정 파일 지원 함수들
// =============================================================================

/**
 * @brief JSON 설정 파일에서 블록 설정 로드
 *
 * @param factory 팩토리
 * @param config_filename 설정 파일명
 * @param configs 로드된 설정 배열 (출력)
 * @param max_configs 설정 배열 최대 크기
 * @return int 로드된 설정 수 (실패 시 -1)
 */
int dsp_block_factory_load_config_from_json(DSPBlockFactory* factory,
                                            const char* config_filename,
                                            DSPBlockConfig* configs,
                                            int max_configs);

/**
 * @brief 블록 설정을 JSON 파일로 저장
 *
 * @param factory 팩토리
 * @param config_filename 설정 파일명
 * @param configs 저장할 설정 배열
 * @param config_count 설정 수
 * @return ETResult 저장 결과
 */
ETResult dsp_block_factory_save_config_to_json(DSPBlockFactory* factory,
                                               const char* config_filename,
                                               const DSPBlockConfig* configs,
                                               int config_count);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_DSP_BLOCK_FACTORY_H