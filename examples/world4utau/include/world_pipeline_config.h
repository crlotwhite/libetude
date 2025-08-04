/**
 * @file world_pipeline_config.h
 * @brief WORLD 파이프라인 설정 시스템 인터페이스
 *
 * 오디오, WORLD 알고리즘, 그래프 최적화 설정을 통합 관리하는 시스템을 제공합니다.
 */

#ifndef WORLD4UTAU_WORLD_PIPELINE_CONFIG_H
#define WORLD4UTAU_WORLD_PIPELINE_CONFIG_H

#include <libetude/types.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 설정 카테고리별 구조체 정의
// =============================================================================

/**
 * @brief 오디오 처리 설정
 */
typedef struct {
    int sample_rate;                 /**< 샘플링 레이트 (Hz) */
    int frame_size;                  /**< 프레임 크기 (samples) */
    int buffer_size;                 /**< 버퍼 크기 (samples) */
    int bit_depth;                   /**< 비트 깊이 (16, 24, 32) */
    int channel_count;               /**< 채널 수 */
    bool enable_dithering;           /**< 디더링 활성화 */
    float input_gain;                /**< 입력 게인 (dB) */
    float output_gain;               /**< 출력 게인 (dB) */
    bool enable_dc_removal;          /**< DC 제거 필터 활성화 */
    bool enable_anti_aliasing;       /**< 안티 앨리어싱 필터 활성화 */
} WorldAudioConfig;

/**
 * @brief F0 추출 설정
 */
typedef struct {
    double frame_period;             /**< 프레임 주기 (ms) */
    double f0_floor;                 /**< 최소 F0 (Hz) */
    double f0_ceil;                  /**< 최대 F0 (Hz) */
    int algorithm;                   /**< 알고리즘 (0: DIO, 1: Harvest) */
    double channels_in_octave;       /**< 옥타브당 채널 수 */
    double target_fs;                /**< 목표 샘플링 레이트 */
    bool enable_refinement;          /**< F0 정제 활성화 */
    double speed;                    /**< 처리 속도 (1.0 = 기본) */
    bool allow_range_extension;      /**< 범위 확장 허용 */
    double threshold;                /**< 임계값 */
} WorldF0Config;

/**
 * @brief 스펙트럼 분석 설정
 */
typedef struct {
    double q1;                       /**< CheapTrick Q1 파라미터 */
    int fft_size;                    /**< FFT 크기 */
    bool enable_power_spectrum;      /**< 파워 스펙트럼 사용 */
    double frequency_interval;       /**< 주파수 간격 */
    int frequency_bins;              /**< 주파수 빈 수 */
    bool enable_spectral_smoothing;  /**< 스펙트럼 스무딩 활성화 */
    double smoothing_factor;         /**< 스무딩 팩터 */
    bool enable_preemphasis;         /**< 프리엠퍼시스 활성화 */
    double preemphasis_coefficient;  /**< 프리엠퍼시스 계수 */
} WorldSpectrumConfig;

/**
 * @brief 비주기성 분석 설정
 */
typedef struct {
    double threshold;                /**< D4C 임계값 */
    int frequency_bands;             /**< 주파수 대역 수 */
    bool enable_band_aperiodicity;   /**< 대역별 비주기성 활성화 */
    double window_length;            /**< 윈도우 길이 (ms) */
    bool enable_adaptive_windowing;  /**< 적응적 윈도잉 활성화 */
    double noise_floor;              /**< 노이즈 플로어 (dB) */
    bool enable_spectral_recovery;   /**< 스펙트럼 복원 활성화 */
} WorldAperiodicityConfig;

/**
 * @brief 음성 합성 설정
 */
typedef struct {
    int sample_rate;                 /**< 샘플링 레이트 (Hz) */
    double frame_period;             /**< 프레임 주기 (ms) */
    bool enable_postfilter;          /**< 후처리 필터 활성화 */
    double postfilter_coefficient;   /**< 후처리 필터 계수 */
    bool enable_pitch_adaptive_spectral_smoothing; /**< 피치 적응 스펙트럼 스무딩 */
    bool enable_seed_signals;        /**< 시드 신호 사용 */
    double synthesis_speed;          /**< 합성 속도 배율 */
    bool enable_overlap_add;         /**< 오버랩 애드 활성화 */
    int overlap_length;              /**< 오버랩 길이 (samples) */
} WorldSynthesisConfig;

/**
 * @brief 그래프 최적화 설정
 */
typedef struct {
    bool enable_node_fusion;         /**< 노드 융합 최적화 */
    bool enable_memory_reuse;        /**< 메모리 재사용 최적화 */
    bool enable_simd_optimization;   /**< SIMD 최적화 */
    bool enable_parallel_execution;  /**< 병렬 실행 최적화 */
    int max_thread_count;            /**< 최대 스레드 수 */
    bool enable_cache_optimization;  /**< 캐시 최적화 */
    bool enable_dead_code_elimination; /**< 데드 코드 제거 */
    bool enable_constant_folding;    /**< 상수 폴딩 */
    double optimization_level;       /**< 최적화 레벨 (0.0-1.0) */
    size_t memory_budget;            /**< 메모리 예산 (bytes) */
} WorldGraphOptimizationConfig;

/**
 * @brief 메모리 관리 설정
 */
typedef struct {
    size_t memory_pool_size;         /**< 메모리 풀 크기 (bytes) */
    size_t analysis_pool_size;       /**< 분석용 메모리 풀 크기 */
    size_t synthesis_pool_size;      /**< 합성용 메모리 풀 크기 */
    size_t cache_pool_size;          /**< 캐시용 메모리 풀 크기 */
    bool enable_memory_tracking;     /**< 메모리 추적 활성화 */
    bool enable_leak_detection;      /**< 메모리 누수 감지 활성화 */
    double gc_threshold;             /**< 가비지 컬렉션 임계값 */
    bool enable_memory_compression;  /**< 메모리 압축 활성화 */
} WorldMemoryConfig;

/**
 * @brief 성능 모니터링 설정
 */
typedef struct {
    bool enable_profiling;           /**< 프로파일링 활성화 */
    bool enable_timing_analysis;     /**< 타이밍 분석 활성화 */
    bool enable_memory_profiling;    /**< 메모리 프로파일링 활성화 */
    bool enable_cpu_profiling;       /**< CPU 프로파일링 활성화 */
    bool enable_gpu_profiling;       /**< GPU 프로파일링 활성화 */
    int profiling_interval_ms;       /**< 프로파일링 간격 (ms) */
    char profile_output_dir[256];    /**< 프로파일 출력 디렉토리 */
    bool enable_realtime_monitoring; /**< 실시간 모니터링 활성화 */
} WorldPerformanceConfig;

/**
 * @brief 디버깅 설정
 */
typedef struct {
    bool enable_debug_output;        /**< 디버그 출력 활성화 */
    bool enable_verbose_logging;     /**< 상세 로깅 활성화 */
    bool enable_intermediate_dumps;  /**< 중간 결과 덤프 활성화 */
    bool enable_graph_visualization; /**< 그래프 시각화 활성화 */
    char debug_output_dir[256];      /**< 디버그 출력 디렉토리 */
    char log_file_path[256];         /**< 로그 파일 경로 */
    int log_level;                   /**< 로그 레벨 (0-5) */
    bool enable_assertion_checks;    /**< 어서션 체크 활성화 */
} WorldDebugConfig;

/**
 * @brief 통합 파이프라인 설정
 */
typedef struct {
    // 핵심 설정 카테고리
    WorldAudioConfig audio;          /**< 오디오 처리 설정 */
    WorldF0Config f0;                /**< F0 추출 설정 */
    WorldSpectrumConfig spectrum;    /**< 스펙트럼 분석 설정 */
    WorldAperiodicityConfig aperiodicity; /**< 비주기성 분석 설정 */
    WorldSynthesisConfig synthesis;  /**< 음성 합성 설정 */

    // 시스템 설정 카테고리
    WorldGraphOptimizationConfig optimization; /**< 그래프 최적화 설정 */
    WorldMemoryConfig memory;        /**< 메모리 관리 설정 */
    WorldPerformanceConfig performance; /**< 성능 모니터링 설정 */
    WorldDebugConfig debug;          /**< 디버깅 설정 */

    // 메타 정보
    char config_name[64];            /**< 설정 이름 */
    char config_version[16];         /**< 설정 버전 */
    char description[256];           /**< 설정 설명 */
    double creation_time;            /**< 생성 시간 */
    double modification_time;        /**< 수정 시간 */
} WorldPipelineConfiguration;

// =============================================================================
// 설정 프리셋 정의
// =============================================================================

/**
 * @brief 설정 프리셋 타입
 */
typedef enum {
    WORLD_CONFIG_PRESET_DEFAULT,     /**< 기본 설정 */
    WORLD_CONFIG_PRESET_HIGH_QUALITY, /**< 고품질 설정 */
    WORLD_CONFIG_PRESET_FAST,        /**< 고속 처리 설정 */
    WORLD_CONFIG_PRESET_LOW_LATENCY, /**< 저지연 설정 */
    WORLD_CONFIG_PRESET_LOW_MEMORY,  /**< 저메모리 설정 */
    WORLD_CONFIG_PRESET_REALTIME,    /**< 실시간 처리 설정 */
    WORLD_CONFIG_PRESET_BATCH,       /**< 배치 처리 설정 */
    WORLD_CONFIG_PRESET_DEBUG,       /**< 디버깅 설정 */
    WORLD_CONFIG_PRESET_CUSTOM       /**< 사용자 정의 설정 */
} WorldConfigPreset;

// =============================================================================
// 설정 관리 함수들
// =============================================================================

/**
 * @brief 기본 설정 생성
 *
 * @return WorldPipelineConfiguration 기본 설정
 */
WorldPipelineConfiguration world_pipeline_config_create_default(void);

/**
 * @brief 프리셋 기반 설정 생성
 *
 * @param preset 프리셋 타입
 * @return WorldPipelineConfiguration 프리셋 설정
 */
WorldPipelineConfiguration world_pipeline_config_create_preset(WorldConfigPreset preset);

/**
 * @brief 설정 복사
 *
 * @param src 소스 설정
 * @param dst 대상 설정
 * @return ETResult 복사 결과
 */
ETResult world_pipeline_config_copy(const WorldPipelineConfiguration* src,
                                   WorldPipelineConfiguration* dst);

/**
 * @brief 설정 검증
 *
 * @param config 검증할 설정
 * @return bool 검증 결과 (true: 유효, false: 무효)
 */
bool world_pipeline_config_validate(const WorldPipelineConfiguration* config);

/**
 * @brief 설정 정규화 (유효하지 않은 값들을 유효한 범위로 조정)
 *
 * @param config 정규화할 설정
 * @return ETResult 정규화 결과
 */
ETResult world_pipeline_config_normalize(WorldPipelineConfiguration* config);

// =============================================================================
// 설정 파일 I/O
// =============================================================================

/**
 * @brief 설정을 파일에서 로드
 *
 * @param filename 설정 파일 경로
 * @param config 로드된 설정 (출력)
 * @return ETResult 로드 결과
 */
ETResult world_pipeline_config_load_from_file(const char* filename,
                                              WorldPipelineConfiguration* config);

/**
 * @brief 설정을 파일에 저장
 *
 * @param config 저장할 설정
 * @param filename 설정 파일 경로
 * @return ETResult 저장 결과
 */
ETResult world_pipeline_config_save_to_file(const WorldPipelineConfiguration* config,
                                            const char* filename);

/**
 * @brief JSON 형식으로 설정 로드
 *
 * @param json_string JSON 문자열
 * @param config 로드된 설정 (출력)
 * @return ETResult 로드 결과
 */
ETResult world_pipeline_config_load_from_json(const char* json_string,
                                              WorldPipelineConfiguration* config);

/**
 * @brief JSON 형식으로 설정 저장
 *
 * @param config 저장할 설정
 * @param json_buffer JSON 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @return ETResult 저장 결과
 */
ETResult world_pipeline_config_save_to_json(const WorldPipelineConfiguration* config,
                                            char* json_buffer, size_t buffer_size);

// =============================================================================
// 설정 비교 및 병합
// =============================================================================

/**
 * @brief 두 설정 비교
 *
 * @param config1 첫 번째 설정
 * @param config2 두 번째 설정
 * @return bool 동일 여부 (true: 동일, false: 다름)
 */
bool world_pipeline_config_compare(const WorldPipelineConfiguration* config1,
                                   const WorldPipelineConfiguration* config2);

/**
 * @brief 설정 병합 (config2의 값으로 config1 업데이트)
 *
 * @param config1 기본 설정 (업데이트됨)
 * @param config2 오버라이드 설정
 * @return ETResult 병합 결과
 */
ETResult world_pipeline_config_merge(WorldPipelineConfiguration* config1,
                                     const WorldPipelineConfiguration* config2);

/**
 * @brief 설정 차이점 계산
 *
 * @param config1 첫 번째 설정
 * @param config2 두 번째 설정
 * @param diff_buffer 차이점 설명 버퍼
 * @param buffer_size 버퍼 크기
 * @return ETResult 계산 결과
 */
ETResult world_pipeline_config_diff(const WorldPipelineConfiguration* config1,
                                    const WorldPipelineConfiguration* config2,
                                    char* diff_buffer, size_t buffer_size);

// =============================================================================
// 설정 카테고리별 관리 함수들
// =============================================================================

/**
 * @brief 오디오 설정 기본값 생성
 *
 * @return WorldAudioConfig 기본 오디오 설정
 */
WorldAudioConfig world_audio_config_default(void);

/**
 * @brief F0 설정 기본값 생성
 *
 * @return WorldF0Config 기본 F0 설정
 */
WorldF0Config world_f0_config_default(void);

/**
 * @brief 스펙트럼 설정 기본값 생성
 *
 * @return WorldSpectrumConfig 기본 스펙트럼 설정
 */
WorldSpectrumConfig world_spectrum_config_default(void);

/**
 * @brief 비주기성 설정 기본값 생성
 *
 * @return WorldAperiodicityConfig 기본 비주기성 설정
 */
WorldAperiodicityConfig world_aperiodicity_config_default(void);

/**
 * @brief 합성 설정 기본값 생성
 *
 * @return WorldSynthesisConfig 기본 합성 설정
 */
WorldSynthesisConfig world_synthesis_config_default(void);

/**
 * @brief 그래프 최적화 설정 기본값 생성
 *
 * @return WorldGraphOptimizationConfig 기본 최적화 설정
 */
WorldGraphOptimizationConfig world_graph_optimization_config_default(void);

/**
 * @brief 메모리 설정 기본값 생성
 *
 * @return WorldMemoryConfig 기본 메모리 설정
 */
WorldMemoryConfig world_memory_config_default(void);

/**
 * @brief 성능 설정 기본값 생성
 *
 * @return WorldPerformanceConfig 기본 성능 설정
 */
WorldPerformanceConfig world_performance_config_default(void);

/**
 * @brief 디버깅 설정 기본값 생성
 *
 * @return WorldDebugConfig 기본 디버깅 설정
 */
WorldDebugConfig world_debug_config_default(void);

// =============================================================================
// 설정 검증 함수들
// =============================================================================

/**
 * @brief 오디오 설정 검증
 *
 * @param config 검증할 오디오 설정
 * @return bool 검증 결과
 */
bool world_audio_config_validate(const WorldAudioConfig* config);

/**
 * @brief F0 설정 검증
 *
 * @param config 검증할 F0 설정
 * @return bool 검증 결과
 */
bool world_f0_config_validate(const WorldF0Config* config);

/**
 * @brief 스펙트럼 설정 검증
 *
 * @param config 검증할 스펙트럼 설정
 * @return bool 검증 결과
 */
bool world_spectrum_config_validate(const WorldSpectrumConfig* config);

/**
 * @brief 비주기성 설정 검증
 *
 * @param config 검증할 비주기성 설정
 * @return bool 검증 결과
 */
bool world_aperiodicity_config_validate(const WorldAperiodicityConfig* config);

/**
 * @brief 합성 설정 검증
 *
 * @param config 검증할 합성 설정
 * @return bool 검증 결과
 */
bool world_synthesis_config_validate(const WorldSynthesisConfig* config);

/**
 * @brief 그래프 최적화 설정 검증
 *
 * @param config 검증할 최적화 설정
 * @return bool 검증 결과
 */
bool world_graph_optimization_config_validate(const WorldGraphOptimizationConfig* config);

/**
 * @brief 메모리 설정 검증
 *
 * @param config 검증할 메모리 설정
 * @return bool 검증 결과
 */
bool world_memory_config_validate(const WorldMemoryConfig* config);

/**
 * @brief 성능 설정 검증
 *
 * @param config 검증할 성능 설정
 * @return bool 검증 결과
 */
bool world_performance_config_validate(const WorldPerformanceConfig* config);

/**
 * @brief 디버깅 설정 검증
 *
 * @param config 검증할 디버깅 설정
 * @return bool 검증 결과
 */
bool world_debug_config_validate(const WorldDebugConfig* config);

// =============================================================================
// 유틸리티 함수들
// =============================================================================

/**
 * @brief 설정 정보 출력
 *
 * @param config 출력할 설정
 */
void world_pipeline_config_print(const WorldPipelineConfiguration* config);

/**
 * @brief 설정 요약 정보 출력
 *
 * @param config 출력할 설정
 */
void world_pipeline_config_print_summary(const WorldPipelineConfiguration* config);

/**
 * @brief 프리셋 이름 조회
 *
 * @param preset 프리셋 타입
 * @return const char* 프리셋 이름
 */
const char* world_config_preset_get_name(WorldConfigPreset preset);

/**
 * @brief 프리셋 설명 조회
 *
 * @param preset 프리셋 타입
 * @return const char* 프리셋 설명
 */
const char* world_config_preset_get_description(WorldConfigPreset preset);

/**
 * @brief 설정 해시 계산 (설정 변경 감지용)
 *
 * @param config 해시를 계산할 설정
 * @return uint64_t 설정 해시값
 */
uint64_t world_pipeline_config_hash(const WorldPipelineConfiguration* config);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_WORLD_PIPELINE_CONFIG_H