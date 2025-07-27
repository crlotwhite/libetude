/**
 * @file audio_io.h
 * @brief 오디오 I/O 시스템 헤더
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 오디오 API 추상화와 콜백 기반 스트리밍을 제공합니다.
 */

#ifndef LIBETUDE_AUDIO_IO_H
#define LIBETUDE_AUDIO_IO_H

#include "libetude/types.h"
#include "libetude/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 오디오 포맷 구조체
 */
typedef struct {
    uint32_t sample_rate;    /**< 샘플링 레이트 (Hz) */
    uint16_t bit_depth;      /**< 비트 깊이 (16, 24, 32) */
    uint16_t num_channels;   /**< 채널 수 (1=모노, 2=스테레오) */
    uint32_t frame_size;     /**< 프레임 크기 (바이트) */
    uint32_t buffer_size;    /**< 버퍼 크기 (프레임 수) */
} ETAudioFormat;

/**
 * @brief 오디오 디바이스 타입
 */
typedef enum {
    ET_AUDIO_DEVICE_OUTPUT = 0,  /**< 출력 디바이스 */
    ET_AUDIO_DEVICE_INPUT = 1    /**< 입력 디바이스 */
} ETAudioDeviceType;

/**
 * @brief 오디오 디바이스 상태
 */
typedef enum {
    ET_AUDIO_STATE_STOPPED = 0,  /**< 정지 상태 */
    ET_AUDIO_STATE_RUNNING = 1,  /**< 실행 상태 */
    ET_AUDIO_STATE_PAUSED = 2    /**< 일시정지 상태 */
} ETAudioState;

/**
 * @brief 오디오 콜백 함수 타입
 * @param buffer 오디오 버퍼 (float 배열)
 * @param num_frames 프레임 수
 * @param user_data 사용자 데이터
 */
typedef void (*ETAudioCallback)(float* buffer, int num_frames, void* user_data);

/**
 * @brief 오디오 디바이스 구조체 (불투명 타입)
 */
typedef struct ETAudioDevice ETAudioDevice;

/**
 * @brief 오디오 버퍼 구조체
 */
typedef struct {
    float* data;             /**< 버퍼 데이터 */
    uint32_t size;           /**< 버퍼 크기 (프레임 수) */
    uint32_t write_pos;      /**< 쓰기 위치 */
    uint32_t read_pos;       /**< 읽기 위치 */
    uint32_t available;      /**< 사용 가능한 프레임 수 */
    bool is_full;            /**< 버퍼 가득참 여부 */
} ETAudioBuffer;

/**
 * @brief 기본 오디오 포맷 생성
 * @param sample_rate 샘플링 레이트
 * @param num_channels 채널 수
 * @param buffer_size 버퍼 크기
 * @return 오디오 포맷 구조체
 */
ETAudioFormat et_audio_format_create(uint32_t sample_rate, uint16_t num_channels, uint32_t buffer_size);

/**
 * @brief 출력 디바이스 열기
 * @param device_name 디바이스 이름 (NULL이면 기본 디바이스)
 * @param format 오디오 포맷
 * @return 오디오 디바이스 포인터 (실패시 NULL)
 */
ETAudioDevice* et_audio_open_output_device(const char* device_name, const ETAudioFormat* format);

/**
 * @brief 입력 디바이스 열기
 * @param device_name 디바이스 이름 (NULL이면 기본 디바이스)
 * @param format 오디오 포맷
 * @return 오디오 디바이스 포인터 (실패시 NULL)
 */
ETAudioDevice* et_audio_open_input_device(const char* device_name, const ETAudioFormat* format);

/**
 * @brief 오디오 디바이스 닫기
 * @param device 오디오 디바이스
 */
void et_audio_close_device(ETAudioDevice* device);

/**
 * @brief 오디오 콜백 설정
 * @param device 오디오 디바이스
 * @param callback 콜백 함수
 * @param user_data 사용자 데이터
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_audio_set_callback(ETAudioDevice* device, ETAudioCallback callback, void* user_data);

/**
 * @brief 오디오 스트림 시작
 * @param device 오디오 디바이스
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_audio_start(ETAudioDevice* device);

/**
 * @brief 오디오 스트림 정지
 * @param device 오디오 디바이스
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_audio_stop(ETAudioDevice* device);

/**
 * @brief 오디오 스트림 일시정지
 * @param device 오디오 디바이스
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult et_audio_pause(ETAudioDevice* device);

/**
 * @brief 오디오 디바이스 상태 조회
 * @param device 오디오 디바이스
 * @return 디바이스 상태
 */
ETAudioState et_audio_get_state(const ETAudioDevice* device);

/**
 * @brief 오디오 지연시간 조회
 * @param device 오디오 디바이스
 * @return 지연시간 (밀리초)
 */
uint32_t et_audio_get_latency(const ETAudioDevice* device);

/**
 * @brief 오디오 버퍼 생성
 * @param size 버퍼 크기 (프레임 수)
 * @param num_channels 채널 수
 * @return 오디오 버퍼 포인터 (실패시 NULL)
 */
ETAudioBuffer* et_audio_buffer_create(uint32_t size, uint16_t num_channels);

/**
 * @brief 오디오 버퍼 해제
 * @param buffer 오디오 버퍼
 */
void et_audio_buffer_destroy(ETAudioBuffer* buffer);

/**
 * @brief 오디오 버퍼에 데이터 쓰기
 * @param buffer 오디오 버퍼
 * @param data 쓸 데이터
 * @param num_frames 프레임 수
 * @return 실제로 쓴 프레임 수
 */
uint32_t et_audio_buffer_write(ETAudioBuffer* buffer, const float* data, uint32_t num_frames);

/**
 * @brief 오디오 버퍼에서 데이터 읽기
 * @param buffer 오디오 버퍼
 * @param data 읽을 데이터 버퍼
 * @param num_frames 프레임 수
 * @return 실제로 읽은 프레임 수
 */
uint32_t et_audio_buffer_read(ETAudioBuffer* buffer, float* data, uint32_t num_frames);

/**
 * @brief 오디오 버퍼 리셋
 * @param buffer 오디오 버퍼
 */
void et_audio_buffer_reset(ETAudioBuffer* buffer);

/**
 * @brief 오디오 버퍼 사용 가능한 공간 조회
 * @param buffer 오디오 버퍼
 * @return 사용 가능한 프레임 수
 */
uint32_t et_audio_buffer_available_space(const ETAudioBuffer* buffer);

/**
 * @brief 오디오 버퍼 사용 가능한 데이터 조회
 * @param buffer 오디오 버퍼
 * @return 사용 가능한 데이터 프레임 수
 */
uint32_t et_audio_buffer_available_data(const ETAudioBuffer* buffer);

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
 * @brief 오디오 시스템 정리
 */
void et_audio_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_AUDIO_IO_H