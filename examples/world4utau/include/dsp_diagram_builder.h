/**
 * @file dsp_diagram_builder.h
 * @brief DSP 블록 다이어그램 빌더 인터페이스
 *
 * DSP 블록들을 연결하여 다이어그램을 구성하는 빌더 패턴 인터페이스를 제공합니다.
 */

#ifndef WORLD4UTAU_DSP_DIAGRAM_BUILDER_H
#define WORLD4UTAU_DSP_DIAGRAM_BUILDER_H

#include "dsp_block_diagram.h"
#include "world_dsp_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// DSP 다이어그램 빌더 구조체
// =============================================================================

/**
 * @brief DSP 다이어그램 빌더 구조체
 */
typedef struct {
    DSPBlockDiagram* diagram;       ///< 구성 중인 다이어그램
    ETMemoryPool* mem_pool;         ///< 메모리 풀

    // 빌더 상태
    bool is_building;               ///< 빌드 진행 중 플래그
    char error_message[256];        ///< 에러 메시지

    // 블록 ID 추적 (빠른 접근을 위해)
    int audio_input_block_id;       ///< 오디오 입력 블록 ID
    int f0_extraction_block_id;     ///< F0 추출 블록 ID
    int spectrum_analysis_block_id; ///< 스펙트럼 분석 블록 ID
    int aperiodicity_analysis_block_id; ///< 비주기성 분석 블록 ID
    int parameter_merge_block_id;   ///< 파라미터 병합 블록 ID
    int synthesis_block_id;         ///< 합성 블록 ID
    int audio_output_block_id;      ///< 오디오 출력 블록 ID

    // 설정 정보
    int sample_rate;                ///< 샘플링 레이트
    int audio_length;               ///< 오디오 길이
    double frame_period;            ///< 프레임 주기 (ms)
    int fft_size;                   ///< FFT 크기
} DSPDiagramBuilder;

/**
 * @brief WORLD 처리 파이프라인 설정
 */
typedef struct {
    // 오디오 설정
    int sample_rate;                ///< 샘플링 레이트
    int audio_length;               ///< 오디오 길이
    double frame_period;            ///< 프레임 주기 (ms)
    int fft_size;                   ///< FFT 크기

    // F0 추출 설정
    F0ExtractionConfig f0_config;

    // 스펙트럼 분석 설정
    SpectrumConfig spectrum_config;

    // 비주기성 분석 설정
    AperiodicityConfig aperiodicity_config;

    // 합성 설정
    SynthesisConfig synthesis_config;

    // 출력 설정
    char output_filename[256];      ///< 출력 파일명
    bool enable_file_output;        ///< 파일 출력 활성화
} WorldPipelineConfig;

// =============================================================================
// DSP 다이어그램 빌더 기본 함수들
// =============================================================================

/**
 * @brief DSP 다이어그램 빌더 생성
 *
 * @param name 다이어그램 이름
 * @param max_blocks 최대 블록 수
 * @param max_connections 최대 연결 수
 * @param mem_pool 메모리 풀
 * @return DSPDiagramBuilder* 생성된 빌더 (실패 시 NULL)
 */
DSPDiagramBuilder* dsp_diagram_builder_create(const char* name, int max_blocks,
                                             int max_connections, ETMemoryPool* mem_pool);

/**
 * @brief DSP 다이어그램 빌더 해제
 *
 * @param builder 해제할 빌더
 */
void dsp_diagram_builder_destroy(DSPDiagramBuilder* builder);

/**
 * @brief 빌드 시작
 *
 * @param builder 빌더
 * @return ETResult 시작 결과
 */
ETResult dsp_diagram_builder_begin(DSPDiagramBuilder* builder);

/**
 * @brief 빌드 완료 및 다이어그램 반환
 *
 * @param builder 빌더
 * @return DSPBlockDiagram* 완성된 다이어그램 (실패 시 NULL)
 */
DSPBlockDiagram* dsp_diagram_builder_finish(DSPDiagramBuilder* builder);

/**
 * @brief 빌더 에러 메시지 가져오기
 *
 * @param builder 빌더
 * @return const char* 에러 메시지
 */
const char* dsp_diagram_builder_get_error(DSPDiagramBuilder* builder);

// =============================================================================
// 블록 추가 함수들
// =============================================================================

/**
 * @brief 오디오 입력 블록 추가
 *
 * @param builder 빌더
 * @param name 블록 이름
 * @param audio_buffer 오디오 데이터
 * @param audio_length 오디오 길이
 * @param sample_rate 샘플링 레이트
 * @param frame_size 프레임 크기
 * @return ETResult 추가 결과
 */
ETResult dsp_diagram_builder_add_audio_input(DSPDiagramBuilder* builder,
                                            const char* name,
                                            const float* audio_buffer,
                                            int audio_length,
                                            int sample_rate,
                                            int frame_size);

/**
 * @brief F0 추출 블록 추가
 *
 * @param builder 빌더
 * @param name 블록 이름
 * @param config F0 추출 설정
 * @return ETResult 추가 결과
 */
ETResult dsp_diagram_builder_add_f0_extraction(DSPDiagramBuilder* builder,
                                              const char* name,
                                              const F0ExtractionConfig* config);

/**
 * @brief 스펙트럼 분석 블록 추가
 *
 * @param builder 빌더
 * @param name 블록 이름
 * @param config 스펙트럼 분석 설정
 * @return ETResult 추가 결과
 */
ETResult dsp_diagram_builder_add_spectrum_analysis(DSPDiagramBuilder* builder,
                                                  const char* name,
                                                  const SpectrumConfig* config);

/**
 * @brief 비주기성 분석 블록 추가
 *
 * @param builder 빌더
 * @param name 블록 이름
 * @param config 비주기성 분석 설정
 * @return ETResult 추가 결과
 */
ETResult dsp_diagram_builder_add_aperiodicity_analysis(DSPDiagramBuilder* builder,
                                                      const char* name,
                                                      const AperiodicityConfig* config);

/**
 * @brief 파라미터 병합 블록 추가
 *
 * @param builder 빌더
 * @param name 블록 이름
 * @param frame_count 프레임 수
 * @param fft_size FFT 크기
 * @return ETResult 추가 결과
 */
ETResult dsp_diagram_builder_add_parameter_merge(DSPDiagramBuilder* builder,
                                                const char* name,
                                                int frame_count,
                                                int fft_size);

/**
 * @brief 음성 합성 블록 추가
 *
 * @param builder 빌더
 * @param name 블록 이름
 * @param config 합성 설정
 * @return ETResult 추가 결과
 */
ETResult dsp_diagram_builder_add_synthesis(DSPDiagramBuilder* builder,
                                          const char* name,
                                          const SynthesisConfig* config);

/**
 * @brief 오디오 출력 블록 추가
 *
 * @param builder 빌더
 * @param name 블록 이름
 * @param buffer_size 버퍼 크기
 * @param sample_rate 샘플링 레이트
 * @param output_filename 출력 파일명 (NULL이면 파일 쓰기 안함)
 * @return ETResult 추가 결과
 */
ETResult dsp_diagram_builder_add_audio_output(DSPDiagramBuilder* builder,
                                             const char* name,
                                             int buffer_size,
                                             int sample_rate,
                                             const char* output_filename);

// =============================================================================
// 연결 함수들
// =============================================================================

/**
 * @brief 블록 간 연결 (블록 이름 사용)
 *
 * @param builder 빌더
 * @param source_block_name 소스 블록 이름
 * @param source_port_index 소스 포트 인덱스
 * @param dest_block_name 대상 블록 이름
 * @param dest_port_index 대상 포트 인덱스
 * @return ETResult 연결 결과
 */
ETResult dsp_diagram_builder_connect_by_name(DSPDiagramBuilder* builder,
                                            const char* source_block_name,
                                            int source_port_index,
                                            const char* dest_block_name,
                                            int dest_port_index);

/**
 * @brief 블록 간 연결 (블록 ID 사용)
 *
 * @param builder 빌더
 * @param source_block_id 소스 블록 ID
 * @param source_port_index 소스 포트 인덱스
 * @param dest_block_id 대상 블록 ID
 * @param dest_port_index 대상 포트 인덱스
 * @return ETResult 연결 결과
 */
ETResult dsp_diagram_builder_connect_by_id(DSPDiagramBuilder* builder,
                                          int source_block_id,
                                          int source_port_index,
                                          int dest_block_id,
                                          int dest_port_index);

// =============================================================================
// 고수준 빌더 함수들
// =============================================================================

/**
 * @brief 표준 WORLD 분석 파이프라인 구성
 *
 * @param builder 빌더
 * @param audio_buffer 오디오 데이터
 * @param audio_length 오디오 길이
 * @param config 파이프라인 설정
 * @return ETResult 구성 결과
 */
ETResult dsp_diagram_builder_build_world_analysis_pipeline(DSPDiagramBuilder* builder,
                                                          const float* audio_buffer,
                                                          int audio_length,
                                                          const WorldPipelineConfig* config);

/**
 * @brief 표준 WORLD 합성 파이프라인 구성
 *
 * @param builder 빌더
 * @param world_params WORLD 파라미터
 * @param config 파이프라인 설정
 * @return ETResult 구성 결과
 */
ETResult dsp_diagram_builder_build_world_synthesis_pipeline(DSPDiagramBuilder* builder,
                                                           const WorldParameters* world_params,
                                                           const WorldPipelineConfig* config);

/**
 * @brief 완전한 WORLD 처리 파이프라인 구성 (분석 + 합성)
 *
 * @param builder 빌더
 * @param audio_buffer 오디오 데이터
 * @param audio_length 오디오 길이
 * @param config 파이프라인 설정
 * @return ETResult 구성 결과
 */
ETResult dsp_diagram_builder_build_complete_world_pipeline(DSPDiagramBuilder* builder,
                                                          const float* audio_buffer,
                                                          int audio_length,
                                                          const WorldPipelineConfig* config);

// =============================================================================
// 검증 및 최적화 함수들
// =============================================================================

/**
 * @brief 다이어그램 연결 검증
 *
 * @param builder 빌더
 * @return bool 검증 결과 (true: 유효, false: 무효)
 */
bool dsp_diagram_builder_validate_connections(DSPDiagramBuilder* builder);

/**
 * @brief 데이터 흐름 검증
 *
 * @param builder 빌더
 * @return bool 검증 결과 (true: 유효, false: 무효)
 */
bool dsp_diagram_builder_validate_data_flow(DSPDiagramBuilder* builder);

/**
 * @brief 다이어그램 최적화
 *
 * @param builder 빌더
 * @return ETResult 최적화 결과
 */
ETResult dsp_diagram_builder_optimize(DSPDiagramBuilder* builder);

/**
 * @brief 불필요한 블록 제거
 *
 * @param builder 빌더
 * @return ETResult 제거 결과
 */
ETResult dsp_diagram_builder_remove_unused_blocks(DSPDiagramBuilder* builder);

// =============================================================================
// 디버깅 및 시각화 함수들
// =============================================================================

/**
 * @brief 빌더 상태 출력
 *
 * @param builder 빌더
 */
void dsp_diagram_builder_print_status(DSPDiagramBuilder* builder);

/**
 * @brief 다이어그램을 DOT 형식으로 출력
 *
 * @param builder 빌더
 * @param filename 출력 파일명
 * @return ETResult 출력 결과
 */
ETResult dsp_diagram_builder_export_dot(DSPDiagramBuilder* builder, const char* filename);

/**
 * @brief 빌드 과정 로그 출력
 *
 * @param builder 빌더
 * @param enable_logging 로깅 활성화 여부
 */
void dsp_diagram_builder_set_logging(DSPDiagramBuilder* builder, bool enable_logging);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_DSP_DIAGRAM_BUILDER_H