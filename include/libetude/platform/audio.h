/**
 * @file audio.h
 * @brief 플랫폼 추상화 오디오 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 오디오 API 차이를 숨기고 일관된 인터페이스를 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_AUDIO_H
#define LIBETUDE_PLATFORM_AUDIO_H

#include "libetude/platform/common.h"
#include "libetude/types.h"
#include "libetude/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 오디오 관련 타입 정의
// ============================================================================

/**
 * @brief 오디오 디바이스 타입
 */
typedef enum {
    ET_AUDIO_DEVICE_OUTPUT = 0,  /**< 출력 디바이스 */
    ET_AUDIO_DEVICE_INPUT = 1,   /**< 입력 디바이스 */
    ET_AUDIO_DEVICE_DUPLEX = 2   /**< 양방향 디바이스 */
} ETAudioDeviceType;

/**
 * @brief 오디오 디바이스 상태
 */
typedef enum {
    ET_AUDIO_STATE_STOPPED = 0,  /**< 정지 상태 */
    ET_AUDIO_STATE_RUNNING = 1,  /**< 실행 상태 */
    ET_AUDIO_STATE_PAUSED = 2,   /**< 일시정지 상태 */
    ET_AUDIO_STATE_ERROR = 3     /**< 오류 상태 */
} ETAudioState;

/**
 * @brief 오디오 포맷 구조체
 */
typedef struct {
    uint32_t sample_rate;        /**< 샘플링 레이트 (Hz) */
    uint16_t bit_depth;          /**< 비트 깊이 (16, 24, 32) */
    uint16_t num_channels;       /**< 채널 수 (1=모노, 2=스테레오) */
    uint32_t frame_size;         /**< 프레임 크기 (바이트) */
    uint32_t buffer_size;        /**< 버퍼 크기 (프레임 수) */
    bool is_float;               /**< float 포맷 여부 */
} ETAudioFormat;

/**
 * @brief 오디오 디바이스 정보 구조체
 */
typedef struct {
    char name[256];              /**< 디바이스 이름 */
    char id[128];                /**< 디바이스 ID */
    ETAudioDeviceType type;      /**< 디바이스 타입 */
    uint32_t max_channels;       /**< 최대 채널 수 */
    uint32_t* supported_rates;   /**< 지원 샘플 레이트 배열 */
    int rate_count;              /**< 샘플 레이트 개수 */
    bool is_default;             /**< 기본 디바이스 여부 */
    uint32_t min_latency;        /**< 최소 지연시간 (밀리초) */
    uint32_t max_latency;        /**< 최대 지연시간 (밀리초) */
} ETAudioDeviceInfo;

/**
 * @brief 오디오 콜백 함수 타입
 * @param buffer 오디오 버퍼 (float 배열)
 * @param num_frames 프레임 수
 * @param user_data 사용자 데이터
 * @return 계속 처리하려면 0, 중단하려면 음수
 */
typedef int (*ETAudioCallback)(float* buffer, int num_frames, void* user_data);

/**
 * @brief 오디오 디바이스 구조체 (불투명 타입)
 */
typedef struct ETAudioDevice ETAudioDevice;

// ============================================================================
// 오디오 인터페이스 구조체
// ============================================================================

/**
 * @brief 플랫폼 추상화 오디오 인터페이스
 */
typedef struct ETAudioInterface {
    // 디바이스 관리
    ETResult (*open_output_device)(const char* device_name, const ETAudioFormat* format, ETAudioDevice** device);
    ETResult (*open_input_device)(const char* device_name, const ETAudioFormat* format, ETAudioDevice** device);
    void (*close_device)(ETAudioDevice* device);

    // 스트림 제어
    ETResult (*start_stream)(ETAudioDevice* device);
    ETResult (*stop_stream)(ETAudioDevice* device);
    ETResult (*pause_stream)(ETAudioDevice* device);

    // 콜백 관리
    ETResult (*set_callback)(ETAudioDevice* device, ETAudioCallback callback, void* user_data);

    // 디바이스 정보
    ETResult (*enumerate_devices)(ETAudioDeviceType type, ETAudioDeviceInfo* devices, int* count);
    uint32_t (*get_latency)(const ETAudioDevice* device);
    ETAudioState (*get_state)(const ETAudioDevice* device);

    // 포맷 지원 확인
    bool (*is_format_supported)(const char* device_name, const ETAudioFormat* format);
    ETResult (*get_supported_formats)(const char* device_name, ETAudioFormat* formats, int* count);

    // 플랫폼별 확장 데이터
    void* platform_data;
} ETAudioInterface;

// ============================================================================
// 오디오 버퍼 관리 구조체
// ============================================================================

/**
 * @brief 오디오 버퍼 구조체
 */
typedef struct {
    float* data;                 /**< 버퍼 데이터 */
    uint32_t size;               /**< 버퍼 크기 (프레임 수) */
    uint32_t channels;           /**< 채널 수 */
    uint32_t write_pos;          /**< 쓰기 위치 */
    uint32_t read_pos;           /**< 읽기 위치 */
    uint32_t available;          /**< 사용 가능한 프레임 수 */
    bool is_full;                /**< 버퍼 가득참 여부 */
    bool is_circular;            /**< 순환 버퍼 여부 */
} ETAudioBuffer;

// ============================================================================
// 공통 함수 선언
// ============================================================================

/**
 * @brief 기본 오디오 포맷을 생성합니다
 * @param sample_rate 샘플링 레이트
 * @param num_channels 채널 수
 * @param buffer_size 버퍼 크기
 * @return 오디오 포맷 구조체
 */
ETAudioFormat et_audio_format_create(uint32_t sample_rate, uint16_t num_channels, uint32_t buffer_size);

/**
 * @brief 오디오 포맷이 유효한지 검증합니다
 * @param format 검증할 오디오 포맷
 * @return 유효하면 true, 아니면 false
 */
bool et_audio_format_validate(const ETAudioFormat* format);

/**
 * @brief 두 오디오 포맷이 호환되는지 확인합니다
 * @param format1 첫 번째 포맷
 * @param format2 두 번째 포맷
 * @return 호환되면 true, 아니면 false
 */
bool et_audio_format_compatible(const ETAudioFormat* format1, const ETAudioFormat* format2);

/**
 * @brief 오디오 포맷을 다른 포맷으로 변환합니다
 * @param src_format 소스 포맷
 * @param dst_format 대상 포맷
 * @param src_buffer 소스 버퍼
 * @param dst_buffer 대상 버퍼
 * @param num_frames 프레임 수
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_audio_format_convert(const ETAudioFormat* src_format,
                                const ETAudioFormat* dst_format,
                                const void* src_buffer,
                                void* dst_buffer,
                                uint32_t num_frames);

// ============================================================================
// 오디오 버퍼 관리 함수
// ============================================================================

/**
 * @brief 오디오 버퍼를 생성합니다
 * @param size 버퍼 크기 (프레임 수)
 * @param channels 채널 수
 * @param is_circular 순환 버퍼 여부
 * @return 오디오 버퍼 포인터 (실패시 NULL)
 */
ETAudioBuffer* et_audio_buffer_create(uint32_t size, uint16_t channels, bool is_circular);

/**
 * @brief 오디오 버퍼를 해제합니다
 * @param buffer 오디오 버퍼
 */
void et_audio_buffer_destroy(ETAudioBuffer* buffer);

/**
 * @brief 오디오 버퍼에 데이터를 씁니다
 * @param buffer 오디오 버퍼
 * @param data 쓸 데이터
 * @param num_frames 프레임 수
 * @return 실제로 쓴 프레임 수
 */
uint32_t et_audio_buffer_write(ETAudioBuffer* buffer, const float* data, uint32_t num_frames);

/**
 * @brief 오디오 버퍼에서 데이터를 읽습니다
 * @param buffer 오디오 버퍼
 * @param data 읽을 데이터 버퍼
 * @param num_frames 프레임 수
 * @return 실제로 읽은 프레임 수
 */
uint32_t et_audio_buffer_read(ETAudioBuffer* buffer, float* data, uint32_t num_frames);

/**
 * @brief 오디오 버퍼를 리셋합니다
 * @param buffer 오디오 버퍼
 */
void et_audio_buffer_reset(ETAudioBuffer* buffer);

/**
 * @brief 오디오 버퍼의 사용 가능한 공간을 조회합니다
 * @param buffer 오디오 버퍼
 * @return 사용 가능한 프레임 수
 */
uint32_t et_audio_buffer_available_space(const ETAudioBuffer* buffer);

/**
 * @brief 오디오 버퍼의 사용 가능한 데이터를 조회합니다
 * @param buffer 오디오 버퍼
 * @return 사용 가능한 데이터 프레임 수
 */
uint32_t et_audio_buffer_available_data(const ETAudioBuffer* buffer);

// ============================================================================
// 오디오 처리 유틸리티 함수
// ============================================================================

/**
 * @brief 오디오 버퍼 클리핑 (범위: -1.0 ~ 1.0)
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 */
void et_audio_clip_buffer(float* buffer, uint32_t num_samples);

/**
 * @brief 오디오 버퍼 볼륨 조절
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 * @param volume 볼륨 (0.0 ~ 1.0)
 */
void et_audio_apply_volume(float* buffer, uint32_t num_samples, float volume);

/**
 * @brief 오디오 버퍼 믹싱
 * @param dest 대상 버퍼
 * @param src 소스 버퍼
 * @param num_samples 샘플 수
 * @param mix_ratio 믹싱 비율 (0.0 ~ 1.0)
 */
void et_audio_mix_buffers(float* dest, const float* src, uint32_t num_samples, float mix_ratio);

/**
 * @brief 오디오 버퍼 페이드 인/아웃
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 * @param fade_in true면 페이드 인, false면 페이드 아웃
 */
void et_audio_fade_buffer(float* buffer, uint32_t num_samples, bool fade_in);

/**
 * @brief 오디오 버퍼 무음 처리
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 */
void et_audio_silence_buffer(float* buffer, uint32_t num_samples);

/**
 * @brief 오디오 버퍼 RMS 레벨 계산
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 * @return RMS 레벨 (0.0 ~ 1.0)
 */
float et_audio_calculate_rms(const float* buffer, uint32_t num_samples);

/**
 * @brief 오디오 버퍼 피크 레벨 계산
 * @param buffer 오디오 버퍼
 * @param num_samples 샘플 수
 * @return 피크 레벨 (0.0 ~ 1.0)
 */
float et_audio_calculate_peak(const float* buffer, uint32_t num_samples);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_AUDIO_H