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
#include <libetude/audio_io.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// libetude 에러 코드 확장 (누락된 코드들)
#ifndef ET_ERROR_MEMORY_ALLOCATION
#define ET_ERROR_MEMORY_ALLOCATION -100
#endif

#ifndef ET_ERROR_INITIALIZATION_FAILED
#define ET_ERROR_INITIALIZATION_FAILED -101
#endif

#ifndef ET_ERROR_BUFFER_TOO_SMALL
#define ET_ERROR_BUFFER_TOO_SMALL -102
#endif

#ifndef ET_ERROR_FILE_NOT_FOUND
#define ET_ERROR_FILE_NOT_FOUND -103
#endif

#ifndef ET_ERROR_FILE_READ
#define ET_ERROR_FILE_READ -104
#endif

#ifndef ET_ERROR_FILE_WRITE
#define ET_ERROR_FILE_WRITE -105
#endif

#ifndef ET_ERROR_INVALID_FORMAT
#define ET_ERROR_INVALID_FORMAT -106
#endif

#ifndef ET_ERROR_COMPRESSION_FAILED
#define ET_ERROR_COMPRESSION_FAILED -107
#endif

#ifndef ET_ERROR_DECOMPRESSION_FAILED
#define ET_ERROR_DECOMPRESSION_FAILED -108
#endif

// 전방 선언
typedef struct WorldAnalysisEngine WorldAnalysisEngine;
typedef struct WorldSynthesisEngine WorldSynthesisEngine;
typedef struct WorldParameters WorldParameters;
typedef struct WorldF0Extractor WorldF0Extractor;
typedef struct WorldSpectrumAnalyzer WorldSpectrumAnalyzer;
typedef struct WorldAperiodicityAnalyzer WorldAperiodicityAnalyzer;
typedef struct WorldMemoryManager WorldMemoryManager;
typedef struct WorldCache WorldCache;

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
 * @brief WORLD 스펙트럼 분석기 구조체
 */
typedef struct WorldSpectrumAnalyzer {
    // 설정
    WorldSpectrumConfig config;  /**< 스펙트럼 분석 설정 */

    // libetude 통합
    ETSTFTContext* stft_ctx;     /**< STFT 컨텍스트 */
    ETMemoryPool* mem_pool;      /**< 메모리 풀 */

    // 내부 버퍼
    double* window_buffer;       /**< 윈도우 함수 버퍼 */
    double* fft_input_buffer;    /**< FFT 입력 버퍼 */
    double* fft_output_buffer;   /**< FFT 출력 버퍼 */
    double* magnitude_buffer;    /**< 크기 스펙트럼 버퍼 */
    double* phase_buffer;        /**< 위상 스펙트럼 버퍼 */
    double* smoothed_spectrum;   /**< 평활화된 스펙트럼 버퍼 */

    // CheapTrick 전용 버퍼
    double* liftering_buffer;    /**< 리프터링 버퍼 */
    double* cepstrum_buffer;     /**< 켑스트럼 버퍼 */
    double* envelope_buffer;     /**< 엔벨로프 버퍼 */

    // 버퍼 크기
    int fft_size;                /**< FFT 크기 */
    int window_size;             /**< 윈도우 크기 */
    size_t buffer_size;          /**< 버퍼 크기 */

    // 상태 정보
    bool is_initialized;         /**< 초기화 상태 */
    int last_sample_rate;        /**< 마지막 처리한 샘플링 레이트 */
    double last_q1;              /**< 마지막 사용한 Q1 파라미터 */
} WorldSpectrumAnalyzer;

/**
 * @brief WORLD 비주기성 분석기 구조체
 */
typedef struct WorldAperiodicityAnalyzer {
    // 설정
    WorldAperiodicityConfig config;  /**< 비주기성 분석 설정 */

    // libetude 통합
    ETSTFTContext* stft_ctx;         /**< STFT 컨텍스트 */
    ETMemoryPool* mem_pool;          /**< 메모리 풀 */

    // 내부 버퍼
    double* window_buffer;           /**< 윈도우 함수 버퍼 */
    double* fft_input_buffer;        /**< FFT 입력 버퍼 */
    double* fft_output_buffer;       /**< FFT 출력 버퍼 */
    double* magnitude_buffer;        /**< 크기 스펙트럼 버퍼 */
    double* phase_buffer;            /**< 위상 스펙트럼 버퍼 */
    double* power_spectrum_buffer;   /**< 파워 스펙트럼 버퍼 */

    // D4C 전용 버퍼
    double* static_group_delay;      /**< 정적 그룹 지연 버퍼 */
    double* smoothed_group_delay;    /**< 평활화된 그룹 지연 버퍼 */
    double* coarse_aperiodicity;     /**< 거친 비주기성 버퍼 */
    double* refined_aperiodicity;    /**< 정제된 비주기성 버퍼 */
    double* frequency_axis;          /**< 주파수축 버퍼 */

    // 대역별 분석 버퍼
    double** band_aperiodicity;      /**< 대역별 비주기성 [num_bands][spectrum_length] */
    double* band_boundaries;         /**< 대역 경계 주파수 */
    int num_bands;                   /**< 분석 대역 수 */

    // 버퍼 크기
    int fft_size;                    /**< FFT 크기 */
    int window_size;                 /**< 윈도우 크기 */
    int spectrum_length;             /**< 스펙트럼 길이 (fft_size/2+1) */
    size_t buffer_size;              /**< 버퍼 크기 */

    // 상태 정보
    bool is_initialized;             /**< 초기화 상태 */
    int last_sample_rate;            /**< 마지막 처리한 샘플링 레이트 */
    double last_threshold;           /**< 마지막 사용한 임계값 */
} WorldAperiodicityAnalyzer;

/**
 * @brief WORLD F0 추출기 구조체
 */
typedef struct WorldF0Extractor {
    // 설정
    WorldF0Config config;        /**< F0 추출 설정 */

    // libetude 통합
    ETSTFTContext* stft_ctx;     /**< STFT 컨텍스트 */
    ETMemoryPool* mem_pool;      /**< 메모리 풀 */

    // 내부 버퍼
    double* work_buffer;         /**< 작업용 버퍼 */
    double* filtered_signal;     /**< 필터링된 신호 버퍼 */
    double* decimated_signal;    /**< 다운샘플링된 신호 버퍼 */
    size_t buffer_size;          /**< 버퍼 크기 */

    // DIO 알고리즘 전용 버퍼
    double* dio_f0_candidates;   /**< DIO F0 후보 버퍼 */
    double* dio_f0_scores;       /**< DIO F0 점수 버퍼 */
    int dio_candidates_count;    /**< DIO 후보 개수 */

    // Harvest 알고리즘 전용 버퍼
    double* harvest_f0_map;      /**< Harvest F0 맵 버퍼 */
    double* harvest_reliability; /**< Harvest 신뢰도 버퍼 */

    // 상태 정보
    bool is_initialized;         /**< 초기화 상태 */
    int last_sample_rate;        /**< 마지막 처리한 샘플링 레이트 */
    int last_audio_length;       /**< 마지막 처리한 오디오 길이 */
} WorldF0Extractor;

/**
 * @brief WORLD 분석 엔진 구조체
 */
typedef struct WorldAnalysisEngine {
    // 설정
    WorldAnalysisConfig config;

    // 분석기들
    WorldF0Extractor* f0_extractor;                 /**< F0 추출기 */
    WorldSpectrumAnalyzer* spectrum_analyzer;       /**< 스펙트럼 분석기 */
    WorldAperiodicityAnalyzer* aperiodicity_analyzer; /**< 비주기성 분석기 */

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
    ETMemoryPool* mem_pool;         /**< 메모리 풀 */

    // 내부 버퍼
    double* synthesis_buffer;       /**< 합성용 버퍼 */
    size_t synthesis_buffer_size;   /**< 합성용 버퍼 크기 */

    // 실시간 처리용 버퍼
    float* realtime_output_buffer;  /**< 실시간 출력 버퍼 */
    double* overlap_buffer;         /**< 오버랩 버퍼 */
    int realtime_buffer_size;       /**< 실시간 버퍼 크기 */
    int overlap_buffer_size;        /**< 오버랩 버퍼 크기 */

    // 실시간 상태 정보
    const WorldParameters* current_params; /**< 현재 처리 중인 파라미터 */
    int current_frame_index;        /**< 현재 프레임 인덱스 */
    int samples_processed;          /**< 처리된 샘플 수 */
    int chunk_size;                 /**< 청크 크기 */
    bool realtime_mode;             /**< 실시간 모드 플래그 */

    // 성능 최적화 정보
    double last_processing_time_ms; /**< 마지막 처리 시간 */
    int optimization_level;         /**< 최적화 레벨 (0-3) */
    bool enable_lookahead;          /**< 룩어헤드 처리 활성화 */

    // 상태 정보
    bool is_initialized;            /**< 초기화 상태 */
} WorldSynthesisEngine;

/**
 * @brief WORLD 스트리밍 오디오 콜백 함수 타입
 *
 * @param audio_data 오디오 데이터 포인터
 * @param sample_count 샘플 수
 * @param user_data 사용자 데이터
 * @return true 계속 처리, false 중단
 */
typedef bool (*WorldAudioStreamCallback)(const float* audio_data, int sample_count, void* user_data);

/**
 * @brief WORLD 메모리 풀 타입
 */
typedef enum {
    WORLD_MEMORY_POOL_ANALYSIS = 0,    /**< 분석용 메모리 풀 */
    WORLD_MEMORY_POOL_SYNTHESIS = 1,   /**< 합성용 메모리 풀 */
    WORLD_MEMORY_POOL_CACHE = 2,       /**< 캐시용 메모리 풀 */
    WORLD_MEMORY_POOL_COUNT = 3        /**< 메모리 풀 개수 */
} WorldMemoryPoolType;

/**
 * @brief WORLD 전용 메모리 관리자 구조체
 *
 * libetude 메모리 풀을 활용하여 WORLD 알고리즘에 최적화된 메모리 관리를 제공합니다.
 */
typedef struct WorldMemoryManager {
    // libetude 메모리 풀들
    ETMemoryPool* analysis_pool;     /**< 분석용 메모리 풀 */
    ETMemoryPool* synthesis_pool;    /**< 합성용 메모리 풀 */
    ETMemoryPool* cache_pool;        /**< 캐시용 메모리 풀 */

    // 메모리 풀 설정
    size_t analysis_pool_size;       /**< 분석용 풀 크기 (바이트) */
    size_t synthesis_pool_size;      /**< 합성용 풀 크기 (바이트) */
    size_t cache_pool_size;          /**< 캐시용 풀 크기 (바이트) */

    // 메모리 사용량 통계
    size_t analysis_allocated;       /**< 분석용 할당된 메모리 (바이트) */
    size_t synthesis_allocated;      /**< 합성용 할당된 메모리 (바이트) */
    size_t cache_allocated;          /**< 캐시용 할당된 메모리 (바이트) */
    size_t peak_analysis_usage;      /**< 분석용 피크 사용량 (바이트) */
    size_t peak_synthesis_usage;     /**< 합성용 피크 사용량 (바이트) */
    size_t peak_cache_usage;         /**< 캐시용 피크 사용량 (바이트) */

    // 할당 통계
    int total_allocations;           /**< 총 할당 횟수 */
    int total_deallocations;         /**< 총 해제 횟수 */
    int active_allocations;          /**< 활성 할당 개수 */

    // 성능 최적화 설정
    bool enable_memory_alignment;    /**< 메모리 정렬 사용 여부 */
    bool enable_pool_preallocation;  /**< 풀 사전 할당 사용 여부 */
    int alignment_size;              /**< 메모리 정렬 크기 (바이트) */

    // 상태 정보
    bool is_initialized;             /**< 초기화 상태 */
    bool enable_statistics;          /**< 통계 수집 사용 여부 */
} WorldMemoryManager;

/**
 * @brief WORLD 캐시 엔트리 구조체
 */
typedef struct {
    char file_hash[64];              /**< 파일 해시 (SHA-256) */
    WorldParameters* params;         /**< 분석 결과 */
    uint64_t timestamp;              /**< 캐시 생성 시간 (Unix timestamp) */
    uint64_t file_size;              /**< 원본 파일 크기 (바이트) */
    uint32_t sample_rate;            /**< 샘플링 레이트 */
    uint32_t audio_length;           /**< 오디오 길이 (샘플) */
    bool is_valid;                   /**< 유효성 플래그 */
    bool is_compressed;              /**< 압축 여부 */
    size_t compressed_size;          /**< 압축된 크기 (바이트) */
} WorldCacheEntry;

/**
 * @brief WORLD 캐시 시스템 구조체
 *
 * 분석 결과를 파일 기반으로 캐싱하여 재분석을 방지합니다.
 */
typedef struct WorldCache {
    // 캐시 엔트리 관리
    WorldCacheEntry* entries;        /**< 캐시 엔트리 배열 */
    int max_entries;                 /**< 최대 엔트리 수 */
    int current_count;               /**< 현재 엔트리 수 */
    int next_index;                  /**< 다음 삽입 인덱스 (순환) */

    // 캐시 디렉토리 설정
    char cache_dir[512];             /**< 캐시 디렉토리 경로 */
    char index_file_path[512];       /**< 인덱스 파일 경로 */

    // 캐시 정책 설정
    uint64_t max_cache_age_seconds;  /**< 최대 캐시 유지 시간 (초) */
    size_t max_cache_size_bytes;     /**< 최대 캐시 크기 (바이트) */
    size_t current_cache_size;       /**< 현재 캐시 크기 (바이트) */
    bool enable_compression;         /**< 압축 사용 여부 */
    bool enable_auto_cleanup;        /**< 자동 정리 사용 여부 */

    // 캐시 통계
    int cache_hits;                  /**< 캐시 히트 횟수 */
    int cache_misses;                /**< 캐시 미스 횟수 */
    int cache_evictions;             /**< 캐시 제거 횟수 */
    double total_load_time_ms;       /**< 총 로드 시간 (밀리초) */
    double total_save_time_ms;       /**< 총 저장 시간 (밀리초) */

    // 메모리 관리
    WorldMemoryManager* memory_manager; /**< 메모리 관리자 참조 */

    // 상태 정보
    bool is_initialized;             /**< 초기화 상태 */
    bool is_dirty;                   /**< 인덱스 파일 업데이트 필요 여부 */
} WorldCache;

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
// WORLD F0 추출기 함수들
// ============================================================================

/**
 * @brief WORLD F0 추출기 생성
 *
 * @param config F0 추출 설정
 * @param mem_pool 메모리 풀 (NULL이면 내부에서 생성)
 * @return 생성된 F0 추출기 포인터, 실패시 NULL
 */
WorldF0Extractor* world_f0_extractor_create(const WorldF0Config* config, ETMemoryPool* mem_pool);

/**
 * @brief WORLD F0 추출기 해제
 *
 * @param extractor 해제할 F0 추출기
 */
void world_f0_extractor_destroy(WorldF0Extractor* extractor);

/**
 * @brief F0 추출기 초기화
 *
 * @param extractor F0 추출기
 * @param sample_rate 샘플링 레이트
 * @param audio_length 오디오 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_f0_extractor_initialize(WorldF0Extractor* extractor, int sample_rate, int audio_length);

/**
 * @brief DIO 알고리즘을 사용한 F0 추출
 *
 * @param extractor F0 추출기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 결과 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_f0_extractor_dio(WorldF0Extractor* extractor,
                               const float* audio, int audio_length, int sample_rate,
                               double* f0, double* time_axis, int f0_length);

/**
 * @brief Harvest 알고리즘을 사용한 F0 추출
 *
 * @param extractor F0 추출기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 결과 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_f0_extractor_harvest(WorldF0Extractor* extractor,
                                   const float* audio, int audio_length, int sample_rate,
                                   double* f0, double* time_axis, int f0_length);

/**
 * @brief F0 추출 (설정된 알고리즘 사용)
 *
 * @param extractor F0 추출기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 결과 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_f0_extractor_extract(WorldF0Extractor* extractor,
                                   const float* audio, int audio_length, int sample_rate,
                                   double* f0, double* time_axis, int f0_length);

// ============================================================================
// WORLD 스펙트럼 분석기 함수들
// ============================================================================

/**
 * @brief WORLD 스펙트럼 분석기 생성
 *
 * @param config 스펙트럼 분석 설정
 * @param mem_pool 메모리 풀 (NULL이면 내부에서 생성)
 * @return 생성된 스펙트럼 분석기 포인터, 실패시 NULL
 */
WorldSpectrumAnalyzer* world_spectrum_analyzer_create(const WorldSpectrumConfig* config, ETMemoryPool* mem_pool);

/**
 * @brief WORLD 스펙트럼 분석기 해제
 *
 * @param analyzer 해제할 스펙트럼 분석기
 */
void world_spectrum_analyzer_destroy(WorldSpectrumAnalyzer* analyzer);

/**
 * @brief 스펙트럼 분석기 초기화
 *
 * @param analyzer 스펙트럼 분석기
 * @param sample_rate 샘플링 레이트
 * @param fft_size FFT 크기 (0이면 자동 계산)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_spectrum_analyzer_initialize(WorldSpectrumAnalyzer* analyzer, int sample_rate, int fft_size);

/**
 * @brief CheapTrick 알고리즘을 사용한 스펙트럼 분석
 *
 * @param analyzer 스펙트럼 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @param spectrogram 스펙트로그램 결과
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_spectrum_analyzer_cheaptrick(WorldSpectrumAnalyzer* analyzer,
                                           const float* audio, int audio_length, int sample_rate,
                                           const double* f0, const double* time_axis, int f0_length,
                                           double** spectrogram);

/**
 * @brief F0 적응형 스펙트럼 분석
 *
 * @param analyzer 스펙트럼 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param center_sample 중심 샘플 위치
 * @param f0_value F0 값 (Hz)
 * @param sample_rate 샘플링 레이트
 * @param spectrum 출력 스펙트럼 배열
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_spectrum_analyzer_extract_frame(WorldSpectrumAnalyzer* analyzer,
                                              const float* audio, int audio_length,
                                              int center_sample, double f0_value,
                                              int sample_rate, double* spectrum);

/**
 * @brief 스펙트럼 엔벨로프 평활화
 *
 * @param analyzer 스펙트럼 분석기
 * @param raw_spectrum 원본 스펙트럼
 * @param smoothed_spectrum 평활화된 스펙트럼
 * @param spectrum_length 스펙트럼 길이
 * @param f0_value F0 값 (Hz)
 * @param sample_rate 샘플링 레이트
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_spectrum_analyzer_smooth_envelope(WorldSpectrumAnalyzer* analyzer,
                                                const double* raw_spectrum,
                                                double* smoothed_spectrum,
                                                int spectrum_length,
                                                double f0_value, int sample_rate);

/**
 * @brief SIMD 최적화된 스펙트럼 분석 (병렬 처리)
 *
 * @param analyzer 스펙트럼 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @param spectrogram 스펙트로그램 결과
 * @param num_threads 사용할 스레드 수 (0이면 자동)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_spectrum_analyzer_cheaptrick_parallel(WorldSpectrumAnalyzer* analyzer,
                                                    const float* audio, int audio_length, int sample_rate,
                                                    const double* f0, const double* time_axis, int f0_length,
                                                    double** spectrogram, int num_threads);

/**
 * @brief SIMD 최적화된 켑스트럼 평활화
 *
 * @param analyzer 스펙트럼 분석기
 * @param magnitude_spectrum 크기 스펙트럼
 * @param smoothed_spectrum 평활화된 스펙트럼
 * @param spectrum_length 스펙트럼 길이
 * @param f0_value F0 값 (Hz)
 * @param sample_rate 샘플링 레이트
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_spectrum_analyzer_cepstral_smoothing_simd(WorldSpectrumAnalyzer* analyzer,
                                                        const double* magnitude_spectrum,
                                                        double* smoothed_spectrum,
                                                        int spectrum_length,
                                                        double f0_value, int sample_rate);

// ============================================================================
// WORLD 비주기성 분석기 함수들
// ============================================================================

/**
 * @brief WORLD 비주기성 분석기 생성
 *
 * @param config 비주기성 분석 설정
 * @param mem_pool 메모리 풀 (NULL이면 내부에서 생성)
 * @return 생성된 비주기성 분석기 포인터, 실패시 NULL
 */
WorldAperiodicityAnalyzer* world_aperiodicity_analyzer_create(const WorldAperiodicityConfig* config, ETMemoryPool* mem_pool);

/**
 * @brief WORLD 비주기성 분석기 해제
 *
 * @param analyzer 해제할 비주기성 분석기
 */
void world_aperiodicity_analyzer_destroy(WorldAperiodicityAnalyzer* analyzer);

/**
 * @brief 비주기성 분석기 초기화
 *
 * @param analyzer 비주기성 분석기
 * @param sample_rate 샘플링 레이트
 * @param fft_size FFT 크기
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_initialize(WorldAperiodicityAnalyzer* analyzer, int sample_rate, int fft_size);

/**
 * @brief D4C 알고리즘을 사용한 비주기성 분석
 *
 * @param analyzer 비주기성 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @param aperiodicity 비주기성 결과
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_d4c(WorldAperiodicityAnalyzer* analyzer,
                                         const float* audio, int audio_length, int sample_rate,
                                         const double* f0, const double* time_axis, int f0_length,
                                         double** aperiodicity);

/**
 * @brief 단일 프레임 비주기성 분석
 *
 * @param analyzer 비주기성 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param center_sample 중심 샘플 위치
 * @param f0_value F0 값 (Hz)
 * @param sample_rate 샘플링 레이트
 * @param aperiodicity 출력 비주기성 배열
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_extract_frame(WorldAperiodicityAnalyzer* analyzer,
                                                   const float* audio, int audio_length,
                                                   int center_sample, double f0_value,
                                                   int sample_rate, double* aperiodicity);

/**
 * @brief 대역별 비주기성 분석
 *
 * @param analyzer 비주기성 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param center_sample 중심 샘플 위치
 * @param f0_value F0 값 (Hz)
 * @param sample_rate 샘플링 레이트
 * @param band_aperiodicity 대역별 비주기성 결과
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_extract_bands(WorldAperiodicityAnalyzer* analyzer,
                                                   const float* audio, int audio_length,
                                                   int center_sample, double f0_value,
                                                   int sample_rate, double* band_aperiodicity);

/**
 * @brief 정적 그룹 지연 계산
 *
 * @param analyzer 비주기성 분석기
 * @param magnitude_spectrum 크기 스펙트럼
 * @param phase_spectrum 위상 스펙트럼
 * @param spectrum_length 스펙트럼 길이
 * @param static_group_delay 출력 정적 그룹 지연
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_compute_static_group_delay(WorldAperiodicityAnalyzer* analyzer,
                                                               const double* magnitude_spectrum,
                                                               const double* phase_spectrum,
                                                               int spectrum_length,
                                                               double* static_group_delay);

/**
 * @brief 그룹 지연 평활화
 *
 * @param analyzer 비주기성 분석기
 * @param static_group_delay 정적 그룹 지연
 * @param smoothed_group_delay 평활화된 그룹 지연
 * @param spectrum_length 스펙트럼 길이
 * @param f0_value F0 값 (Hz)
 * @param sample_rate 샘플링 레이트
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_smooth_group_delay(WorldAperiodicityAnalyzer* analyzer,
                                                       const double* static_group_delay,
                                                       double* smoothed_group_delay,
                                                       int spectrum_length,
                                                       double f0_value, int sample_rate);

/**
 * @brief 비주기성 추정
 *
 * @param analyzer 비주기성 분석기
 * @param static_group_delay 정적 그룹 지연
 * @param smoothed_group_delay 평활화된 그룹 지연
 * @param spectrum_length 스펙트럼 길이
 * @param aperiodicity 출력 비주기성
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_estimate_aperiodicity(WorldAperiodicityAnalyzer* analyzer,
                                                          const double* static_group_delay,
                                                          const double* smoothed_group_delay,
                                                          int spectrum_length,
                                                          double* aperiodicity);

/**
 * @brief 최적화된 비주기성 분석 (SIMD 및 메모리 최적화)
 *
 * @param analyzer 비주기성 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param center_sample 중심 샘플 위치
 * @param f0_value F0 값 (Hz)
 * @param sample_rate 샘플링 레이트
 * @param aperiodicity 출력 비주기성 배열
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_extract_frame_optimized(WorldAperiodicityAnalyzer* analyzer,
                                                            const float* audio, int audio_length,
                                                            int center_sample, double f0_value,
                                                            int sample_rate, double* aperiodicity);

/**
 * @brief 멀티스레드 비주기성 분석
 *
 * @param analyzer 비주기성 분석기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 배열
 * @param time_axis 시간축 배열
 * @param f0_length F0 배열 길이
 * @param aperiodicity 비주기성 결과
 * @param num_threads 사용할 스레드 수
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_d4c_parallel(WorldAperiodicityAnalyzer* analyzer,
                                                  const float* audio, int audio_length, int sample_rate,
                                                  const double* f0, const double* time_axis, int f0_length,
                                                  double** aperiodicity, int num_threads);

/**
 * @brief 비주기성 분석기 성능 통계 조회
 *
 * @param analyzer 비주기성 분석기
 * @param memory_usage 메모리 사용량 (바이트)
 * @param processing_time_ms 처리 시간 (밀리초)
 * @param simd_capability SIMD 기능 비트마스크
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_aperiodicity_analyzer_get_performance_stats(WorldAperiodicityAnalyzer* analyzer,
                                                          size_t* memory_usage,
                                                          double* processing_time_ms,
                                                          int* simd_capability);

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
                                   WorldAudioStreamCallback callback, void* user_data);

/**
 * @brief 실시간 청크 단위 합성 초기화
 *
 * @param engine 합성 엔진
 * @param params WORLD 파라미터
 * @param chunk_size 청크 크기 (샘플)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_synthesize_realtime_init(WorldSynthesisEngine* engine,
                                       const WorldParameters* params,
                                       int chunk_size);

/**
 * @brief 실시간 청크 단위 합성 처리
 *
 * @param engine 합성 엔진
 * @param output_chunk 출력 청크 버퍼
 * @param chunk_size 청크 크기 (샘플)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_synthesize_realtime_process(WorldSynthesisEngine* engine,
                                          float* output_chunk,
                                          int chunk_size);

/**
 * @brief 실시간 합성 상태 리셋
 *
 * @param engine 합성 엔진
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_synthesize_realtime_reset(WorldSynthesisEngine* engine);

/**
 * @brief 지연 시간 측정 및 최적화
 *
 * @param engine 합성 엔진
 * @param latency_ms 측정된 지연 시간 (밀리초)
 * @param optimization_level 최적화 레벨 (0-3)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_optimize_latency(WorldSynthesisEngine* engine,
                               double* latency_ms,
                               int optimization_level);

// ============================================================================
// 음성 파라미터 제어 함수들
// ============================================================================

/**
 * @brief 피치 벤드 적용
 *
 * UTAU 피치 벤드 데이터를 WORLD F0 파라미터에 적용합니다.
 * 피치 벤드는 시간에 따른 피치 변화를 나타내며, 기본 F0에 곱셈으로 적용됩니다.
 *
 * @param params WORLD 파라미터 (F0가 수정됨)
 * @param pitch_bend 피치 벤드 데이터 배열 (센트 단위, -1200 ~ +1200)
 * @param pitch_bend_length 피치 벤드 데이터 길이
 * @param target_pitch 목표 피치 (Hz, 기준 피치)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult apply_pitch_bend(WorldParameters* params,
                         const float* pitch_bend, int pitch_bend_length,
                         float target_pitch);

/**
 * @brief 볼륨 제어 적용
 *
 * 스펙트럼 크기 조절을 통해 볼륨을 제어합니다.
 * 모든 주파수 성분에 균등하게 적용됩니다.
 *
 * @param params WORLD 파라미터 (스펙트로그램이 수정됨)
 * @param volume 볼륨 레벨 (0.0 ~ 1.0, 1.0이 원본)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult apply_volume_control(WorldParameters* params, float volume);

/**
 * @brief 모듈레이션 효과 적용
 *
 * 비브라토 효과를 위한 F0 모듈레이션을 적용합니다.
 * 사인파 기반의 주기적인 피치 변화를 생성합니다.
 *
 * @param params WORLD 파라미터 (F0가 수정됨)
 * @param modulation_depth 모듈레이션 깊이 (0.0 ~ 1.0)
 * @param modulation_rate 모듈레이션 속도 (Hz, 일반적으로 4-8Hz)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult apply_modulation(WorldParameters* params,
                         float modulation_depth, float modulation_rate);

/**
 * @brief 타이밍 제어 적용
 *
 * 시간 스케일링을 통해 재생 속도를 조절합니다.
 * F0와 스펙트럼의 시간축을 조정합니다.
 *
 * @param params WORLD 파라미터 (시간축이 수정됨)
 * @param time_scale 시간 스케일 팩터 (1.0이 원본, >1.0이 빠름, <1.0이 느림)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult apply_timing_control(WorldParameters* params, float time_scale);

/**
 * @brief 피치 벤드 보간
 *
 * 피치 벤드 데이터를 F0 프레임 수에 맞게 보간합니다.
 * 선형 보간을 사용하여 부드러운 피치 변화를 생성합니다.
 *
 * @param pitch_bend 원본 피치 벤드 데이터
 * @param pitch_bend_length 원본 데이터 길이
 * @param interpolated_bend 보간된 피치 벤드 출력 배열
 * @param target_length 목표 길이 (F0 프레임 수)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult interpolate_pitch_bend(const float* pitch_bend, int pitch_bend_length,
                               float* interpolated_bend, int target_length);

/**
 * @brief 센트를 주파수 비율로 변환
 *
 * 센트 단위의 피치 변화를 주파수 비율로 변환합니다.
 * 1200 센트 = 1 옥타브 = 2배 주파수
 *
 * @param cents 센트 값 (-1200 ~ +1200)
 * @return 주파수 비율 (1.0이 변화 없음)
 */
double cents_to_frequency_ratio(float cents);

/**
 * @brief 주파수 비율을 센트로 변환
 *
 * 주파수 비율을 센트 단위로 변환합니다.
 *
 * @param ratio 주파수 비율 (1.0이 변화 없음)
 * @return 센트 값
 */
float frequency_ratio_to_cents(double ratio);

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

// ============================================================================
// DIO 알고리즘 내부 함수들
// ============================================================================

/**
 * @brief DIO F0 추정 메인 함수
 *
 * @param extractor F0 추출기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 결과 배열
 * @param f0_length F0 배열 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_dio_f0_estimation(WorldF0Extractor* extractor, const float* audio,
                                int audio_length, int sample_rate, double* f0, int f0_length);

/**
 * @brief 메디안 필터 적용
 *
 * @param signal 입력 신호
 * @param length 신호 길이
 * @param window_size 윈도우 크기 (홀수)
 */
void world_apply_median_filter(double* signal, int length, int window_size);

// ============================================================================
// Harvest 알고리즘 내부 함수들
// ============================================================================

/**
 * @brief Harvest F0 추정 메인 함수
 *
 * @param extractor F0 추출기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 결과 배열
 * @param f0_length F0 배열 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_harvest_f0_estimation(WorldF0Extractor* extractor, const float* audio,
                                    int audio_length, int sample_rate, double* f0, int f0_length);

/**
 * @brief Harvest 후처리 (연속성 개선)
 *
 * @param f0 F0 배열
 * @param f0_length F0 배열 길이
 * @param f0_floor 최소 F0
 * @param f0_ceil 최대 F0
 */
void world_harvest_postprocess(double* f0, int f0_length, double f0_floor, double f0_ceil);

// ============================================================================
// 성능 최적화 함수들
// ============================================================================

/**
 * @brief 최적화된 DIO F0 추정
 *
 * @param extractor F0 추출기
 * @param audio 입력 오디오 데이터
 * @param audio_length 오디오 길이 (샘플)
 * @param sample_rate 샘플링 레이트
 * @param f0 F0 결과 배열
 * @param f0_length F0 배열 길이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_dio_f0_estimation_optimized(WorldF0Extractor* extractor,
                                          const float* audio, int audio_length, int sample_rate,
                                          double* f0, int f0_length);

// ============================================================================
// WORLD 합성 내부 함수들 (정적 함수 선언)
// ============================================================================

/**
 * @brief 유성음 프레임 합성
 */
static ETResult world_synthesize_voiced_frame(WorldSynthesisEngine* engine,
                                             const double* spectrum,
                                             const double* aperiodicity,
                                             double f0_value,
                                             int sample_rate,
                                             int fft_size,
                                             double* impulse_response,
                                             double* noise_spectrum,
                                             double* periodic_spectrum);

/**
 * @brief 무성음 프레임 합성
 */
static ETResult world_synthesize_unvoiced_frame(WorldSynthesisEngine* engine,
                                               const double* spectrum,
                                               const double* aperiodicity,
                                               int sample_rate,
                                               int fft_size,
                                               double* noise_spectrum);

/**
 * @brief 주기적 임펄스 응답 생성
 */
static ETResult world_generate_periodic_impulse(WorldSynthesisEngine* engine,
                                               const double* periodic_spectrum,
                                               int spectrum_length,
                                               double f0_value,
                                               int sample_rate,
                                               int fft_size,
                                               double* impulse_response);

/**
 * @brief 노이즈 성분 추가
 */
static ETResult world_add_noise_component(WorldSynthesisEngine* engine,
                                         const double* noise_spectrum,
                                         int spectrum_length,
                                         int fft_size,
                                         double* impulse_response);

/**
 * @brief 백색 노이즈 신호 생성
 */
static ETResult world_generate_noise_signal(WorldSynthesisEngine* engine,
                                           const double* noise_spectrum,
                                           int spectrum_length,
                                           int fft_size,
                                           double* noise_signal);

/**
 * @brief 오버랩-애드를 통한 프레임 합성
 */
static ETResult world_overlap_add_frame(WorldSynthesisEngine* engine,
                                       const double* frame_signal,
                                       int frame_length,
                                       int center_sample,
                                       float* output_audio,
                                       int output_length);

/**
 * @brief 최소 위상 계산
 */
static ETResult world_compute_minimum_phase(WorldSynthesisEngine* engine,
                                           const double* magnitude,
                                           double* phase,
                                           int spectrum_length);

/**
 * @brief 실수 FFT 수행
 */
static ETResult world_fft_real(WorldSynthesisEngine* engine,
                              const double* input,
                              int input_length,
                              double* magnitude,
                              double* phase,
                              int spectrum_length);

/**
 * @brief 실수 IFFT 수행
 */
static ETResult world_ifft_real(WorldSynthesisEngine* engine,
                               const double* magnitude,
                               const double* phase,
                               int spectrum_length,
                               double* output,
                               int output_length);

/**
 * @brief 청크 버퍼에 프레임 오버랩-애드
 */
static ETResult world_overlap_add_frame_to_chunk(WorldSynthesisEngine* engine,
                                                const double* frame_signal,
                                                int frame_length,
                                                int relative_center,
                                                float* chunk_buffer,
                                                int chunk_length);

/**
 * @brief 고속 프레임 합성 (최적화된 버전)
 */
static ETResult world_synthesize_frame_fast(WorldSynthesisEngine* engine,
                                           const double* spectrum,
                                           const double* aperiodicity,
                                           double f0_value,
                                           int sample_rate,
                                           int fft_size,
                                           double* impulse_response);

/**
 * @brief 실시간 청크용 오버랩-애드 (최적화된 버전)
 */
static ETResult world_overlap_add_frame_to_chunk_realtime(WorldSynthesisEngine* engine,
                                                         const double* frame_signal,
                                                         int frame_length,
                                                         int relative_center,
                                                         float* chunk_buffer,
                                                         int chunk_length);

/**
 * @brief 실시간 성능 모니터링
 */
ETResult world_monitor_realtime_performance(WorldSynthesisEngine* engine,
                                           double* cpu_usage_percent,
                                           double* memory_usage_mb,
                                           double* latency_ms);

/**
 * @brief 적응적 최적화 레벨 조정
 */
ETResult world_adaptive_optimization(WorldSynthesisEngine* engine,
                                    double target_latency_ms);

/**
 * @brief 경량 후처리 (성능 최적화)
 *
 * @param f0 F0 배열
 * @param f0_length F0 배열 길이
 */
void world_apply_lightweight_postprocess(double* f0, int f0_length);

/**
 * @brief 메모리 사용량 모니터링
 *
 * @param extractor F0 추출기
 * @param current_usage 현재 메모리 사용량 (바이트)
 * @param peak_usage 피크 메모리 사용량 (바이트)
 */
void world_monitor_memory_usage(WorldF0Extractor* extractor, size_t* current_usage, size_t* peak_usage);

/**
 * @brief 스펙트럼 분석기에서 SIMD 최적화 활성화/비활성화
 *
 * @param analyzer 스펙트럼 분석기
 * @param enable SIMD 최적화 사용 여부
 */
void world_spectrum_analyzer_set_simd_optimization(WorldSpectrumAnalyzer* analyzer, bool enable);

/**
 * @brief 현재 시스템에서 사용 가능한 SIMD 기능 확인
 *
 * @return SIMD 기능 비트마스크 (0x01: SSE2, 0x02: AVX, 0x04: NEON)
 */
int world_spectrum_analyzer_get_simd_capabilities(void);

// ============================================================================
// WORLD 메모리 관리자 함수들
// ============================================================================

/**
 * @brief WORLD 메모리 관리자 생성
 *
 * @param analysis_size 분석용 메모리 풀 크기 (바이트)
 * @param synthesis_size 합성용 메모리 풀 크기 (바이트)
 * @param cache_size 캐시용 메모리 풀 크기 (바이트)
 * @return 생성된 메모리 관리자 포인터, 실패시 NULL
 */
WorldMemoryManager* world_memory_manager_create(size_t analysis_size,
                                                size_t synthesis_size,
                                                size_t cache_size);

/**
 * @brief WORLD 메모리 관리자 해제
 *
 * @param manager 해제할 메모리 관리자
 */
void world_memory_manager_destroy(WorldMemoryManager* manager);

/**
 * @brief 메모리 할당 (풀 기반)
 *
 * @param manager 메모리 관리자
 * @param size 할당할 크기 (바이트)
 * @param pool_type 메모리 풀 타입
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* world_memory_alloc(WorldMemoryManager* manager, size_t size, WorldMemoryPoolType pool_type);

/**
 * @brief 메모리 해제 (풀 기반)
 *
 * @param manager 메모리 관리자
 * @param ptr 해제할 메모리 포인터
 * @param pool_type 메모리 풀 타입
 */
void world_memory_free(WorldMemoryManager* manager, void* ptr, WorldMemoryPoolType pool_type);

/**
 * @brief 정렬된 메모리 할당
 *
 * @param manager 메모리 관리자
 * @param size 할당할 크기 (바이트)
 * @param alignment 정렬 크기 (바이트)
 * @param pool_type 메모리 풀 타입
 * @return 할당된 메모리 포인터, 실패시 NULL
 */
void* world_memory_alloc_aligned(WorldMemoryManager* manager, size_t size,
                                size_t alignment, WorldMemoryPoolType pool_type);

/**
 * @brief 메모리 풀 리셋
 *
 * @param manager 메모리 관리자
 * @param pool_type 리셋할 메모리 풀 타입
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_memory_pool_reset(WorldMemoryManager* manager, WorldMemoryPoolType pool_type);

/**
 * @brief 메모리 사용량 통계 조회
 *
 * @param manager 메모리 관리자
 * @param pool_type 조회할 메모리 풀 타입
 * @param allocated 현재 할당된 메모리 (바이트)
 * @param peak_usage 피크 사용량 (바이트)
 * @param allocation_count 할당 횟수
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_memory_get_statistics(WorldMemoryManager* manager, WorldMemoryPoolType pool_type,
                                     size_t* allocated, size_t* peak_usage, int* allocation_count);

/**
 * @brief 메모리 풀 사전 할당
 *
 * @param manager 메모리 관리자
 * @param pool_type 사전 할당할 메모리 풀 타입
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_memory_pool_preallocate(WorldMemoryManager* manager, WorldMemoryPoolType pool_type);

/**
 * @brief 메모리 누수 검사
 *
 * @param manager 메모리 관리자
 * @param leaked_bytes 누수된 메모리 크기 (바이트)
 * @param leaked_allocations 누수된 할당 개수
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_memory_check_leaks(WorldMemoryManager* manager,
                                 size_t* leaked_bytes, int* leaked_allocations);

// ============================================================================
// WORLD 캐시 시스템 함수들
// ============================================================================

/**
 * @brief WORLD 캐시 시스템 생성
 *
 * @param cache_dir 캐시 디렉토리 경로
 * @param max_entries 최대 엔트리 수
 * @param memory_manager 메모리 관리자 (NULL이면 내부에서 생성)
 * @return 생성된 캐시 시스템 포인터, 실패시 NULL
 */
WorldCache* world_cache_create(const char* cache_dir, int max_entries,
                              WorldMemoryManager* memory_manager);

/**
 * @brief WORLD 캐시 시스템 해제
 *
 * @param cache 해제할 캐시 시스템
 */
void world_cache_destroy(WorldCache* cache);

/**
 * @brief 캐시에서 분석 결과 조회
 *
 * @param cache 캐시 시스템
 * @param file_path 파일 경로
 * @param params 분석 결과를 저장할 WorldParameters
 * @return true 캐시 히트, false 캐시 미스
 */
bool world_cache_get(WorldCache* cache, const char* file_path, WorldParameters* params);

/**
 * @brief 캐시에 분석 결과 저장
 *
 * @param cache 캐시 시스템
 * @param file_path 파일 경로
 * @param params 저장할 분석 결과
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_set(WorldCache* cache, const char* file_path, const WorldParameters* params);

/**
 * @brief 캐시 정리 (오래된 엔트리 제거)
 *
 * @param cache 캐시 시스템
 * @param max_age_seconds 최대 유지 시간 (초)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_cleanup(WorldCache* cache, uint64_t max_age_seconds);

/**
 * @brief 파일 해시 계산
 *
 * @param file_path 파일 경로
 * @param hash_output 해시 결과 버퍼 (최소 64바이트)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_compute_file_hash(const char* file_path, char* hash_output);

/**
 * @brief 캐시 엔트리 직렬화
 *
 * @param entry 캐시 엔트리
 * @param buffer 출력 버퍼
 * @param buffer_size 버퍼 크기
 * @param written_size 실제 쓰여진 크기
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_serialize_entry(const WorldCacheEntry* entry,
                                    uint8_t* buffer, size_t buffer_size, size_t* written_size);

/**
 * @brief 캐시 엔트리 역직렬화
 *
 * @param buffer 입력 버퍼
 * @param buffer_size 버퍼 크기
 * @param entry 출력 캐시 엔트리
 * @param memory_manager 메모리 관리자
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_deserialize_entry(const uint8_t* buffer, size_t buffer_size,
                                      WorldCacheEntry* entry, WorldMemoryManager* memory_manager);

/**
 * @brief 캐시 인덱스 파일 저장
 *
 * @param cache 캐시 시스템
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_save_index(WorldCache* cache);

/**
 * @brief 캐시 인덱스 파일 로드
 *
 * @param cache 캐시 시스템
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_load_index(WorldCache* cache);

/**
 * @brief 캐시 통계 조회
 *
 * @param cache 캐시 시스템
 * @param hits 캐시 히트 횟수
 * @param misses 캐시 미스 횟수
 * @param hit_ratio 캐시 히트 비율 (0.0 ~ 1.0)
 * @param total_size 총 캐시 크기 (바이트)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_get_statistics(WorldCache* cache, int* hits, int* misses,
                                   double* hit_ratio, size_t* total_size);

/**
 * @brief 캐시 압축 활성화/비활성화
 *
 * @param cache 캐시 시스템
 * @param enable 압축 사용 여부
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_set_compression(WorldCache* cache, bool enable);

/**
 * @brief 캐시 자동 정리 설정
 *
 * @param cache 캐시 시스템
 * @param enable 자동 정리 사용 여부
 * @param max_age_seconds 최대 유지 시간 (초)
 * @param max_size_bytes 최대 캐시 크기 (바이트)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult world_cache_set_auto_cleanup(WorldCache* cache, bool enable,
                                     uint64_t max_age_seconds, size_t max_size_bytes);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_WORLD_ENGINE_H