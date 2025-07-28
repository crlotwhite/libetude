/**
 * @file vocoder.h
 * @brief LibEtude 보코더 인터페이스
 *
 * 이 파일은 LibEtude의 보코더 시스템을 정의합니다.
 * 그래프 기반 보코더 통합, 실시간 최적화, 품질/성능 트레이드오프 조정을 제공합니다.
 */

#ifndef LIBETUDE_VOCODER_H
#define LIBETUDE_VOCODER_H

#include "graph.h"
#include "tensor.h"
#include "memory.h"
#include "lef_format.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 보코더 품질 모드
typedef enum {
    ET_VOCODER_QUALITY_DRAFT = 0,     // 초안 품질 (최고 성능)
    ET_VOCODER_QUALITY_NORMAL = 1,    // 일반 품질 (균형)
    ET_VOCODER_QUALITY_HIGH = 2,      // 고품질 (낮은 성능)
    ET_VOCODER_QUALITY_ULTRA = 3      // 최고 품질 (최저 성능)
} ETVocoderQuality;

// 보코더 실행 모드
typedef enum {
    ET_VOCODER_MODE_BATCH = 0,        // 배치 처리 모드
    ET_VOCODER_MODE_STREAMING = 1,    // 스트리밍 모드
    ET_VOCODER_MODE_REALTIME = 2      // 실시간 모드
} ETVocoderMode;

// 보코더 최적화 플래그
typedef enum {
    ET_VOCODER_OPT_NONE = 0,
    ET_VOCODER_OPT_MEMORY = 1 << 0,       // 메모리 최적화
    ET_VOCODER_OPT_SPEED = 1 << 1,        // 속도 최적화
    ET_VOCODER_OPT_QUALITY = 1 << 2,      // 품질 최적화
    ET_VOCODER_OPT_POWER = 1 << 3,        // 전력 최적화 (모바일용)
    ET_VOCODER_OPT_CACHE = 1 << 4,        // 캐시 최적화
    ET_VOCODER_OPT_SIMD = 1 << 5,         // SIMD 최적화
    ET_VOCODER_OPT_GPU = 1 << 6,          // GPU 가속
    ET_VOCODER_OPT_ALL = 0xFFFFFFFF
} ETVocoderOptFlags;

// 보코더 설정 구조체
typedef struct {
    // 기본 오디오 설정
    int sample_rate;                      // 샘플링 레이트 (Hz)
    int mel_channels;                     // Mel 채널 수
    int hop_length;                       // Hop 길이
    int win_length;                       // 윈도우 길이

    // 품질 및 성능 설정
    ETVocoderQuality quality;             // 품질 모드
    ETVocoderMode mode;                   // 실행 모드
    ETVocoderOptFlags opt_flags;          // 최적화 플래그

    // 실시간 처리 설정
    int chunk_size;                       // 청크 크기 (스트리밍용)
    int lookahead_frames;                 // 미리보기 프레임 수
    int max_latency_ms;                   // 최대 지연 시간 (ms)

    // 메모리 설정
    size_t buffer_size;                   // 내부 버퍼 크기
    bool use_memory_pool;                 // 메모리 풀 사용 여부

    // GPU 설정
    bool enable_gpu;                      // GPU 가속 활성화
    int gpu_device_id;                    // GPU 디바이스 ID

    // 고급 설정
    float quality_scale;                  // 품질 스케일 (0.1 ~ 2.0)
    float speed_scale;                    // 속도 스케일 (0.5 ~ 2.0)
    bool enable_postfilter;               // 후처리 필터 활성화
    bool enable_noise_shaping;            // 노이즈 셰이핑 활성화
} ETVocoderConfig;

// 보코더 컨텍스트 구조체
typedef struct {
    // 기본 설정
    ETVocoderConfig config;               // 보코더 설정

    // 그래프 및 모델
    ETGraph* vocoder_graph;               // 보코더 계산 그래프
    LEFModel* vocoder_model;              // 보코더 모델

    // 메모리 관리
    ETMemoryPool* mem_pool;               // 메모리 풀

    // 텐서 버퍼 (재사용용)
    ETTensor* input_buffer;               // 입력 버퍼 (Mel 스펙트로그램)
    ETTensor* output_buffer;              // 출력 버퍼 (오디오)
    ETTensor* temp_buffers[4];            // 임시 버퍼들

    // 스트리밍 상태
    bool is_streaming;                    // 스트리밍 상태
    int current_frame;                    // 현재 프레임 인덱스
    float* overlap_buffer;                // 오버랩 버퍼
    size_t overlap_size;                  // 오버랩 크기

    // 성능 통계
    uint64_t total_frames_processed;      // 처리된 총 프레임 수
    uint64_t total_processing_time_us;    // 총 처리 시간 (마이크로초)
    float avg_processing_time_ms;         // 평균 처리 시간 (밀리초)
    float peak_processing_time_ms;        // 최대 처리 시간 (밀리초)

    // 품질 메트릭
    float current_quality_score;          // 현재 품질 점수 (0.0 ~ 1.0)
    float avg_quality_score;              // 평균 품질 점수

    // 내부 상태
    bool initialized;                     // 초기화 상태
    pthread_mutex_t mutex;                // 스레드 안전성용 뮤텍스
} ETVocoderContext;

// 보코더 성능 통계
typedef struct {
    uint64_t frames_processed;            // 처리된 프레임 수
    uint64_t total_processing_time_us;    // 총 처리 시간 (마이크로초)
    float avg_processing_time_ms;         // 평균 처리 시간 (밀리초)
    float peak_processing_time_ms;        // 최대 처리 시간 (밀리초)
    float min_processing_time_ms;         // 최소 처리 시간 (밀리초)
    float realtime_factor;                // 실시간 팩터 (1.0 = 실시간)

    // 품질 메트릭
    float avg_quality_score;              // 평균 품질 점수
    float min_quality_score;              // 최소 품질 점수
    float max_quality_score;              // 최대 품질 점수

    // 메모리 사용량
    size_t peak_memory_usage;             // 최대 메모리 사용량
    size_t current_memory_usage;          // 현재 메모리 사용량

    // 오류 통계
    uint32_t num_errors;                  // 오류 발생 횟수
    uint32_t num_warnings;                // 경고 발생 횟수
} ETVocoderStats;

// =============================================================================
// 보코더 생성 및 관리 함수
// =============================================================================

/**
 * @brief 기본 보코더 설정 생성
 * @return 기본 설정 구조체
 */
ETVocoderConfig et_vocoder_default_config(void);

/**
 * @brief 보코더 컨텍스트 생성
 * @param model_path 보코더 모델 파일 경로
 * @param config 보코더 설정 (NULL이면 기본값 사용)
 * @return 생성된 보코더 컨텍스트, 실패시 NULL
 */
ETVocoderContext* et_create_vocoder(const char* model_path, const ETVocoderConfig* config);

/**
 * @brief 메모리에서 보코더 컨텍스트 생성
 * @param model_data 모델 데이터
 * @param model_size 모델 데이터 크기
 * @param config 보코더 설정 (NULL이면 기본값 사용)
 * @return 생성된 보코더 컨텍스트, 실패시 NULL
 */
ETVocoderContext* et_create_vocoder_from_memory(const void* model_data, size_t model_size,
                                               const ETVocoderConfig* config);

/**
 * @brief 보코더 컨텍스트 소멸
 * @param ctx 보코더 컨텍스트
 */
void et_destroy_vocoder(ETVocoderContext* ctx);

/**
 * @brief 보코더 설정 업데이트
 * @param ctx 보코더 컨텍스트
 * @param config 새로운 설정
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_update_config(ETVocoderContext* ctx, const ETVocoderConfig* config);

// =============================================================================
// 보코더 추론 함수
// =============================================================================

/**
 * @brief Mel 스펙트로그램을 오디오로 변환 (배치 모드)
 * @param ctx 보코더 컨텍스트
 * @param mel_spec Mel 스펙트로그램 텐서 [time_frames, mel_channels]
 * @param audio 출력 오디오 버퍼
 * @param audio_len 출력 오디오 길이 (입력시 버퍼 크기, 출력시 실제 길이)
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_mel_to_audio(ETVocoderContext* ctx, const ETTensor* mel_spec,
                           float* audio, int* audio_len);

/**
 * @brief Mel 스펙트로그램을 오디오 텐서로 변환
 * @param ctx 보코더 컨텍스트
 * @param mel_spec Mel 스펙트로그램 텐서 [time_frames, mel_channels]
 * @param audio_tensor 출력 오디오 텐서 (NULL이면 새로 생성)
 * @return 출력 오디오 텐서, 실패시 NULL
 */
ETTensor* et_vocoder_mel_to_audio_tensor(ETVocoderContext* ctx, const ETTensor* mel_spec,
                                        ETTensor* audio_tensor);

/**
 * @brief 스트리밍 모드 시작
 * @param ctx 보코더 컨텍스트
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_start_streaming(ETVocoderContext* ctx);

/**
 * @brief 스트리밍 모드에서 Mel 청크 처리
 * @param ctx 보코더 컨텍스트
 * @param mel_chunk Mel 스펙트로그램 청크 [chunk_frames, mel_channels]
 * @param audio_chunk 출력 오디오 청크 버퍼
 * @param chunk_len 출력 청크 길이 (입력시 버퍼 크기, 출력시 실제 길이)
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_process_chunk(ETVocoderContext* ctx, const ETTensor* mel_chunk,
                            float* audio_chunk, int* chunk_len);

/**
 * @brief 스트리밍 모드 종료
 * @param ctx 보코더 컨텍스트
 * @param final_audio 최종 오디오 버퍼 (남은 데이터)
 * @param final_len 최종 오디오 길이
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_stop_streaming(ETVocoderContext* ctx, float* final_audio, int* final_len);

// =============================================================================
// 품질/성능 트레이드오프 조정 함수
// =============================================================================

/**
 * @brief 품질 모드 설정
 * @param ctx 보코더 컨텍스트
 * @param quality 품질 모드
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_set_quality(ETVocoderContext* ctx, ETVocoderQuality quality);

/**
 * @brief 실행 모드 설정
 * @param ctx 보코더 컨텍스트
 * @param mode 실행 모드
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_set_mode(ETVocoderContext* ctx, ETVocoderMode mode);

/**
 * @brief 최적화 플래그 설정
 * @param ctx 보코더 컨텍스트
 * @param opt_flags 최적화 플래그
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_set_optimization(ETVocoderContext* ctx, ETVocoderOptFlags opt_flags);

/**
 * @brief 품질/성능 균형 조정
 * @param ctx 보코더 컨텍스트
 * @param quality_weight 품질 가중치 (0.0 ~ 1.0)
 * @param speed_weight 속도 가중치 (0.0 ~ 1.0)
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_balance_quality_speed(ETVocoderContext* ctx, float quality_weight, float speed_weight);

/**
 * @brief 적응형 품질 조정 활성화/비활성화
 * @param ctx 보코더 컨텍스트
 * @param enable 활성화 여부
 * @param target_latency_ms 목표 지연 시간 (ms)
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_enable_adaptive_quality(ETVocoderContext* ctx, bool enable, int target_latency_ms);

// =============================================================================
// 실시간 최적화 함수
// =============================================================================

/**
 * @brief 실시간 처리 모드 활성화
 * @param ctx 보코더 컨텍스트
 * @param max_latency_ms 최대 허용 지연 시간 (ms)
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_enable_realtime(ETVocoderContext* ctx, int max_latency_ms);

/**
 * @brief 버퍼 크기 최적화
 * @param ctx 보코더 컨텍스트
 * @param target_latency_ms 목표 지연 시간 (ms)
 * @return 최적화된 버퍼 크기, 실패시 -1
 */
int et_vocoder_optimize_buffer_size(ETVocoderContext* ctx, int target_latency_ms);

/**
 * @brief 미리보기 프레임 수 조정
 * @param ctx 보코더 컨텍스트
 * @param lookahead_frames 미리보기 프레임 수
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_set_lookahead(ETVocoderContext* ctx, int lookahead_frames);

/**
 * @brief 청크 크기 최적화
 * @param ctx 보코더 컨텍스트
 * @param target_latency_ms 목표 지연 시간 (ms)
 * @return 최적화된 청크 크기, 실패시 -1
 */
int et_vocoder_optimize_chunk_size(ETVocoderContext* ctx, int target_latency_ms);

// =============================================================================
// 성능 모니터링 및 통계 함수
// =============================================================================

/**
 * @brief 보코더 성능 통계 조회
 * @param ctx 보코더 컨텍스트
 * @param stats 통계 정보를 저장할 구조체
 */
void et_vocoder_get_stats(ETVocoderContext* ctx, ETVocoderStats* stats);

/**
 * @brief 성능 통계 리셋
 * @param ctx 보코더 컨텍스트
 */
void et_vocoder_reset_stats(ETVocoderContext* ctx);

/**
 * @brief 현재 처리 시간 측정 시작
 * @param ctx 보코더 컨텍스트
 */
void et_vocoder_start_timing(ETVocoderContext* ctx);

/**
 * @brief 현재 처리 시간 측정 종료
 * @param ctx 보코더 컨텍스트
 * @return 처리 시간 (마이크로초)
 */
uint64_t et_vocoder_end_timing(ETVocoderContext* ctx);

/**
 * @brief 실시간 팩터 계산
 * @param ctx 보코더 컨텍스트
 * @return 실시간 팩터 (1.0 = 실시간, >1.0 = 실시간보다 빠름)
 */
float et_vocoder_get_realtime_factor(ETVocoderContext* ctx);

/**
 * @brief 품질 점수 계산
 * @param ctx 보코더 컨텍스트
 * @param reference_audio 참조 오디오 (NULL이면 내부 메트릭 사용)
 * @param generated_audio 생성된 오디오
 * @param audio_len 오디오 길이
 * @return 품질 점수 (0.0 ~ 1.0)
 */
float et_vocoder_compute_quality_score(ETVocoderContext* ctx, const float* reference_audio,
                                      const float* generated_audio, int audio_len);

// =============================================================================
// 유틸리티 함수
// =============================================================================

/**
 * @brief 보코더 설정 유효성 검사
 * @param config 보코더 설정
 * @return 유효하면 true, 아니면 false
 */
bool et_vocoder_validate_config(const ETVocoderConfig* config);

/**
 * @brief 보코더 컨텍스트 유효성 검사
 * @param ctx 보코더 컨텍스트
 * @return 유효하면 true, 아니면 false
 */
bool et_vocoder_validate_context(const ETVocoderContext* ctx);

/**
 * @brief 보코더 정보 출력
 * @param ctx 보코더 컨텍스트
 */
void et_vocoder_print_info(const ETVocoderContext* ctx);

/**
 * @brief 보코더 성능 리포트 출력
 * @param ctx 보코더 컨텍스트
 * @param output_file 출력 파일 (NULL이면 stdout)
 */
void et_vocoder_print_performance_report(const ETVocoderContext* ctx, const char* output_file);

/**
 * @brief 권장 설정 계산
 * @param sample_rate 샘플링 레이트
 * @param target_latency_ms 목표 지연 시간 (ms)
 * @param quality_preference 품질 선호도 (0.0 = 속도 우선, 1.0 = 품질 우선)
 * @param config 계산된 설정을 저장할 구조체
 * @return 성공시 0, 실패시 음수 에러 코드
 */
int et_vocoder_compute_recommended_config(int sample_rate, int target_latency_ms,
                                         float quality_preference, ETVocoderConfig* config);

/**
 * @brief 메모리 사용량 추정
 * @param config 보코더 설정
 * @return 예상 메모리 사용량 (바이트)
 */
size_t et_vocoder_estimate_memory_usage(const ETVocoderConfig* config);

/**
 * @brief 처리 시간 추정
 * @param config 보코더 설정
 * @param mel_frames Mel 프레임 수
 * @return 예상 처리 시간 (마이크로초)
 */
uint64_t et_vocoder_estimate_processing_time(const ETVocoderConfig* config, int mel_frames);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_VOCODER_H