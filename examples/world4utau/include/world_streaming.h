/**
 * @file world_streaming.h
 * @brief WORLD 파이프라인 스트리밍 처리 인터페이스
 *
 * 실시간 스트리밍을 위한 청크 단위 처리와 콜백 기반 비동기 처리 시스템을 제공합니다.
 */

#ifndef WORLD4UTAU_WORLD_STREAMING_H
#define WORLD4UTAU_WORLD_STREAMING_H

#include <libetude/types.h>
#include <libetude/memory.h>
#include <libetude/task_scheduler.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 타입 정의
// =============================================================================

/**
 * @brief 스트리밍 상태
 */
typedef enum {
    WORLD_STREAM_STATE_IDLE,         /**< 유휴 상태 */
    WORLD_STREAM_STATE_INITIALIZING, /**< 초기화 중 */
    WORLD_STREAM_STATE_BUFFERING,    /**< 버퍼링 중 */
    WORLD_STREAM_STATE_STREAMING,    /**< 스트리밍 중 */
    WORLD_STREAM_STATE_PAUSED,       /**< 일시 정지 */
    WORLD_STREAM_STATE_STOPPING,     /**< 중지 중 */
    WORLD_STREAM_STATE_ERROR         /**< 오류 */
} WorldStreamState;

/**
 * @brief 스트리밍 모드
 */
typedef enum {
    WORLD_STREAM_MODE_REALTIME,      /**< 실시간 모드 */
    WORLD_STREAM_MODE_BUFFERED,      /**< 버퍼링 모드 */
    WORLD_STREAM_MODE_ADAPTIVE       /**< 적응형 모드 */
} WorldStreamMode;

/**
 * @brief 오디오 청크 구조체
 */
typedef struct {
    float* audio_data;               /**< 오디오 데이터 */
    int frame_count;                 /**< 프레임 수 */
    int channel_count;               /**< 채널 수 */
    int sample_rate;                 /**< 샘플링 레이트 */
    double timestamp;                /**< 타임스탬프 */
    uint64_t sequence_number;        /**< 시퀀스 번호 */
    bool is_final;                   /**< 마지막 청크 여부 */
} WorldAudioChunk;

/**
 * @brief 스트리밍 콜백 타입들
 */
typedef void (*WorldStreamAudioCallback)(const WorldAudioChunk* chunk, void* user_data);
typedef void (*WorldStreamProgressCallback)(float progress, const char* stage, void* user_data);
typedef void (*WorldStreamErrorCallback)(ETResult error, const char* message, void* user_data);
typedef void (*WorldStreamStateCallback)(WorldStreamState old_state, WorldStreamState new_state, void* user_data);

/**
 * @brief 스트리밍 설정
 */
typedef struct {
    // 기본 설정
    WorldStreamMode mode;            /**< 스트리밍 모드 */
    int chunk_size;                  /**< 청크 크기 (프레임) */
    int buffer_count;                /**< 버퍼 개수 */
    int sample_rate;                 /**< 샘플링 레이트 */
    int channel_count;               /**< 채널 수 */

    // 지연 시간 설정
    double target_latency_ms;        /**< 목표 지연 시간 (ms) */
    double max_latency_ms;           /**< 최대 허용 지연 시간 (ms) */

    // 품질 설정
    bool enable_quality_adaptation;  /**< 품질 적응 활성화 */
    float quality_threshold;         /**< 품질 임계값 */

    // 버퍼링 설정
    int min_buffer_size;             /**< 최소 버퍼 크기 */
    int max_buffer_size;             /**< 최대 버퍼 크기 */
    double buffer_timeout_ms;        /**< 버퍼 타임아웃 (ms) */

    // 스레드 설정
    int processing_thread_count;     /**< 처리 스레드 수 */
    bool enable_thread_affinity;     /**< 스레드 친화성 활성화 */

    // 콜백 설정
    WorldStreamAudioCallback audio_callback;     /**< 오디오 콜백 */
    WorldStreamProgressCallback progress_callback; /**< 진행 상황 콜백 */
    WorldStreamErrorCallback error_callback;     /**< 오류 콜백 */
    WorldStreamStateCallback state_callback;     /**< 상태 변경 콜백 */
    void* callback_user_data;        /**< 콜백 사용자 데이터 */
} WorldStreamConfig;

/**
 * @brief 스트리밍 통계
 */
typedef struct {
    // 처리 통계
    uint64_t total_chunks_processed; /**< 처리된 총 청크 수 */
    uint64_t total_frames_processed; /**< 처리된 총 프레임 수 */
    double total_processing_time;    /**< 총 처리 시간 (초) */
    double average_chunk_time;       /**< 평균 청크 처리 시간 (초) */

    // 지연 시간 통계
    double current_latency_ms;       /**< 현재 지연 시간 (ms) */
    double average_latency_ms;       /**< 평균 지연 시간 (ms) */
    double max_latency_ms;           /**< 최대 지연 시간 (ms) */

    // 버퍼 통계
    int current_buffer_level;        /**< 현재 버퍼 레벨 */
    int max_buffer_level;            /**< 최대 버퍼 레벨 */
    uint64_t buffer_underruns;       /**< 버퍼 언더런 횟수 */
    uint64_t buffer_overruns;        /**< 버퍼 오버런 횟수 */

    // 품질 통계
    float current_quality;           /**< 현재 품질 */
    float average_quality;           /**< 평균 품질 */
    uint64_t quality_adaptations;    /**< 품질 적응 횟수 */

    // 오류 통계
    uint64_t total_errors;           /**< 총 오류 수 */
    uint64_t dropped_chunks;         /**< 드롭된 청크 수 */
} WorldStreamStats;

/**
 * @brief 스트리밍 컨텍스트
 */
typedef struct {
    // 설정
    WorldStreamConfig config;        /**< 스트리밍 설정 */

    // 상태
    WorldStreamState state;          /**< 현재 상태 */
    bool is_active;                  /**< 활성 상태 */
    bool should_stop;                /**< 중지 요청 플래그 */

    // 버퍼 관리
    WorldAudioChunk** chunk_buffers; /**< 청크 버퍼 배열 */
    int buffer_read_index;           /**< 읽기 인덱스 */
    int buffer_write_index;          /**< 쓰기 인덱스 */
    int buffer_count;                /**< 현재 버퍼 수 */

    // 스레드 관리
    ETTaskScheduler* task_scheduler; /**< 작업 스케줄러 */
    void** processing_threads;       /**< 처리 스레드 배열 */

    // 동기화
    void* buffer_mutex;              /**< 버퍼 뮤텍스 */
    void* state_mutex;               /**< 상태 뮤텍스 */
    void* condition_var;             /**< 조건 변수 */

    // 통계
    WorldStreamStats stats;          /**< 스트리밍 통계 */
    double start_time;               /**< 시작 시간 */
    double last_chunk_time;          /**< 마지막 청크 시간 */

    // 메모리 관리
    ETMemoryPool* mem_pool;          /**< 메모리 풀 */

    // 오류 처리
    ETResult last_error;             /**< 마지막 오류 코드 */
    char error_message[256];         /**< 오류 메시지 */

    // 품질 적응
    float current_quality_level;     /**< 현재 품질 레벨 */
    double last_quality_check_time;  /**< 마지막 품질 체크 시간 */
} WorldStreamContext;

// =============================================================================
// 스트리밍 컨텍스트 관리
// =============================================================================

/**
 * @brief 기본 스트리밍 설정 생성
 *
 * @return WorldStreamConfig 기본 설정
 */
WorldStreamConfig world_stream_config_default(void);

/**
 * @brief 스트리밍 컨텍스트 생성
 *
 * @param config 스트리밍 설정
 * @return WorldStreamContext* 생성된 컨텍스트 (실패 시 NULL)
 */
WorldStreamContext* world_stream_context_create(const WorldStreamConfig* config);

/**
 * @brief 스트리밍 컨텍스트 해제
 *
 * @param context 해제할 컨텍스트
 */
void world_stream_context_destroy(WorldStreamContext* context);

/**
 * @brief 스트리밍 초기화
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 초기화 결과
 */
ETResult world_stream_initialize(WorldStreamContext* context);

/**
 * @brief 스트리밍 정리
 *
 * @param context 스트리밍 컨텍스트
 */
void world_stream_cleanup(WorldStreamContext* context);

// =============================================================================
// 스트리밍 제어
// =============================================================================

/**
 * @brief 스트리밍 시작
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 시작 결과
 */
ETResult world_stream_start(WorldStreamContext* context);

/**
 * @brief 스트리밍 중지
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 중지 결과
 */
ETResult world_stream_stop(WorldStreamContext* context);

/**
 * @brief 스트리밍 일시 정지
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 일시 정지 결과
 */
ETResult world_stream_pause(WorldStreamContext* context);

/**
 * @brief 스트리밍 재개
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 재개 결과
 */
ETResult world_stream_resume(WorldStreamContext* context);

/**
 * @brief 스트리밍 재시작
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 재시작 결과
 */
ETResult world_stream_restart(WorldStreamContext* context);

// =============================================================================
// 청크 처리
// =============================================================================

/**
 * @brief 오디오 청크 생성
 *
 * @param frame_count 프레임 수
 * @param channel_count 채널 수
 * @param sample_rate 샘플링 레이트
 * @param mem_pool 메모리 풀
 * @return WorldAudioChunk* 생성된 청크 (실패 시 NULL)
 */
WorldAudioChunk* world_audio_chunk_create(int frame_count, int channel_count,
                                          int sample_rate, ETMemoryPool* mem_pool);

/**
 * @brief 오디오 청크 해제
 *
 * @param chunk 해제할 청크
 */
void world_audio_chunk_destroy(WorldAudioChunk* chunk);

/**
 * @brief 오디오 청크 복사
 *
 * @param src 소스 청크
 * @param dst 대상 청크
 * @return ETResult 복사 결과
 */
ETResult world_audio_chunk_copy(const WorldAudioChunk* src, WorldAudioChunk* dst);

/**
 * @brief 입력 오디오를 청크로 분할
 *
 * @param context 스트리밍 컨텍스트
 * @param input_audio 입력 오디오 데이터
 * @param input_length 입력 길이 (프레임)
 * @return ETResult 분할 결과
 */
ETResult world_stream_push_audio(WorldStreamContext* context,
                                const float* input_audio, int input_length);

/**
 * @brief 처리된 청크 가져오기
 *
 * @param context 스트리밍 컨텍스트
 * @param chunk 출력 청크 (출력)
 * @return ETResult 가져오기 결과
 */
ETResult world_stream_pop_chunk(WorldStreamContext* context, WorldAudioChunk** chunk);

// =============================================================================
// 버퍼 관리
// =============================================================================

/**
 * @brief 버퍼 레벨 조회
 *
 * @param context 스트리밍 컨텍스트
 * @return int 현재 버퍼 레벨
 */
int world_stream_get_buffer_level(WorldStreamContext* context);

/**
 * @brief 버퍼 가용 공간 조회
 *
 * @param context 스트리밍 컨텍스트
 * @return int 가용 버퍼 공간
 */
int world_stream_get_buffer_space(WorldStreamContext* context);

/**
 * @brief 버퍼 플러시
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 플러시 결과
 */
ETResult world_stream_flush_buffers(WorldStreamContext* context);

/**
 * @brief 버퍼 크기 조정
 *
 * @param context 스트리밍 컨텍스트
 * @param new_buffer_count 새로운 버퍼 개수
 * @return ETResult 조정 결과
 */
ETResult world_stream_resize_buffers(WorldStreamContext* context, int new_buffer_count);

// =============================================================================
// 상태 및 통계
// =============================================================================

/**
 * @brief 스트리밍 상태 조회
 *
 * @param context 스트리밍 컨텍스트
 * @return WorldStreamState 현재 상태
 */
WorldStreamState world_stream_get_state(WorldStreamContext* context);

/**
 * @brief 스트리밍 활성 상태 확인
 *
 * @param context 스트리밍 컨텍스트
 * @return bool 활성 상태 여부
 */
bool world_stream_is_active(WorldStreamContext* context);

/**
 * @brief 현재 지연 시간 조회
 *
 * @param context 스트리밍 컨텍스트
 * @return double 현재 지연 시간 (ms)
 */
double world_stream_get_current_latency(WorldStreamContext* context);

/**
 * @brief 스트리밍 통계 조회
 *
 * @param context 스트리밍 컨텍스트
 * @return const WorldStreamStats* 통계 정보 (NULL 시 실패)
 */
const WorldStreamStats* world_stream_get_stats(WorldStreamContext* context);

/**
 * @brief 통계 초기화
 *
 * @param context 스트리밍 컨텍스트
 * @return ETResult 초기화 결과
 */
ETResult world_stream_reset_stats(WorldStreamContext* context);

// =============================================================================
// 품질 적응
// =============================================================================

/**
 * @brief 품질 레벨 설정
 *
 * @param context 스트리밍 컨텍스트
 * @param quality_level 품질 레벨 (0.0-1.0)
 * @return ETResult 설정 결과
 */
ETResult world_stream_set_quality_level(WorldStreamContext* context, float quality_level);

/**
 * @brief 현재 품질 레벨 조회
 *
 * @param context 스트리밍 컨텍스트
 * @return float 현재 품질 레벨
 */
float world_stream_get_quality_level(WorldStreamContext* context);

/**
 * @brief 자동 품질 적응 활성화/비활성화
 *
 * @param context 스트리밍 컨텍스트
 * @param enable 활성화 여부
 * @return ETResult 설정 결과
 */
ETResult world_stream_enable_quality_adaptation(WorldStreamContext* context, bool enable);

// =============================================================================
// 콜백 관리
// =============================================================================

/**
 * @brief 오디오 콜백 설정
 *
 * @param context 스트리밍 컨텍스트
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return ETResult 설정 결과
 */
ETResult world_stream_set_audio_callback(WorldStreamContext* context,
                                        WorldStreamAudioCallback callback,
                                        void* user_data);

/**
 * @brief 진행 상황 콜백 설정
 *
 * @param context 스트리밍 컨텍스트
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return ETResult 설정 결과
 */
ETResult world_stream_set_progress_callback(WorldStreamContext* context,
                                           WorldStreamProgressCallback callback,
                                           void* user_data);

/**
 * @brief 오류 콜백 설정
 *
 * @param context 스트리밍 컨텍스트
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return ETResult 설정 결과
 */
ETResult world_stream_set_error_callback(WorldStreamContext* context,
                                        WorldStreamErrorCallback callback,
                                        void* user_data);

/**
 * @brief 상태 변경 콜백 설정
 *
 * @param context 스트리밍 컨텍스트
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return ETResult 설정 결과
 */
ETResult world_stream_set_state_callback(WorldStreamContext* context,
                                        WorldStreamStateCallback callback,
                                        void* user_data);

// =============================================================================
// 디버깅 및 진단
// =============================================================================

/**
 * @brief 스트리밍 상태 덤프
 *
 * @param context 스트리밍 컨텍스트
 * @param filename 출력 파일명
 * @return ETResult 덤프 결과
 */
ETResult world_stream_dump_state(WorldStreamContext* context, const char* filename);

/**
 * @brief 스트리밍 정보 출력
 *
 * @param context 스트리밍 컨텍스트
 */
void world_stream_print_info(WorldStreamContext* context);

/**
 * @brief 스트리밍 통계 출력
 *
 * @param context 스트리밍 컨텍스트
 */
void world_stream_print_stats(WorldStreamContext* context);

// =============================================================================
// 유틸리티 함수
// =============================================================================

/**
 * @brief 스트리밍 설정 검증
 *
 * @param config 검증할 설정
 * @return bool 검증 결과
 */
bool world_stream_config_validate(const WorldStreamConfig* config);

/**
 * @brief 스트리밍 설정 복사
 *
 * @param src 소스 설정
 * @param dst 대상 설정
 * @return ETResult 복사 결과
 */
ETResult world_stream_config_copy(const WorldStreamConfig* src, WorldStreamConfig* dst);

/**
 * @brief 최적 청크 크기 계산
 *
 * @param sample_rate 샘플링 레이트
 * @param target_latency_ms 목표 지연 시간 (ms)
 * @return int 최적 청크 크기
 */
int world_stream_calculate_optimal_chunk_size(int sample_rate, double target_latency_ms);

/**
 * @brief 최적 버퍼 개수 계산
 *
 * @param chunk_size 청크 크기
 * @param target_latency_ms 목표 지연 시간 (ms)
 * @param max_latency_ms 최대 지연 시간 (ms)
 * @return int 최적 버퍼 개수
 */
int world_stream_calculate_optimal_buffer_count(int chunk_size,
                                               double target_latency_ms,
                                               double max_latency_ms);

#ifdef __cplusplus
}
#endif

#endif // WORLD4UTAU_WORLD_STREAMING_H