/**
 * @file linux_audio.c
 * @brief Linux ALSA 오디오 시스템 구현 - 플랫폼 추상화 레이어
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/platform/audio.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif
#include <pthread.h>

#ifdef HAVE_ALSA
// ============================================================================
// Linux ALSA 구현체 내부 구조체
// ============================================================================

/**
 * @brief Linux ALSA 오디오 디바이스 구조체
 */
typedef struct {
    snd_pcm_t* pcm_handle;           /**< ALSA PCM 핸들 */
    snd_pcm_hw_params_t* hw_params;  /**< 하드웨어 파라미터 */
    ETAudioFormat format;            /**< 오디오 포맷 */
    ETAudioDeviceType type;          /**< 디바이스 타입 */
    ETAudioState state;              /**< 디바이스 상태 */
    ETAudioCallback callback;        /**< 오디오 콜백 */
    void* user_data;                 /**< 사용자 데이터 */
    pthread_t audio_thread;          /**< 오디오 처리 스레드 */
    bool thread_running;             /**< 스레드 실행 상태 */
    char device_name[256];           /**< 디바이스 이름 */
    uint32_t latency_ms;             /**< 지연시간 (밀리초) */
} ETLinuxAudioDevice;

/**
 * @brief ALSA 오류 코드를 공통 오류 코드로 매핑하는 구조체
 */
typedef struct {
    int alsa_error;                  /**< ALSA 오류 코드 */
    ETResult common_error;           /**< 공통 오류 코드 */
    const char* description;         /**< 오류 설명 */
} ETAlsaErrorMapping;

// ALSA 오류 매핑 테이블
static const ETAlsaErrorMapping alsa_error_mappings[] = {
    { -ENODEV,    ET_ERROR_DEVICE_NOT_FOUND,  "디바이스를 찾을 수 없음" },
    { -EBUSY,     ET_ERROR_DEVICE_BUSY,       "디바이스가 사용 중" },
    { -EINVAL,    ET_ERROR_INVALID_ARGUMENT,  "잘못된 인수" },
    { -ENOMEM,    ET_ERROR_OUT_OF_MEMORY,     "메모리 부족" },
    { -EPERM,     ET_ERROR_ACCESS_DENIED,     "접근 권한 없음" },
    { -EIO,       ET_ERROR_IO,                "입출력 오류" },
    { -EPIPE,     ET_ERROR_UNDERRUN,          "언더런/오버런" },
    { -ESTRPIPE,  ET_ERROR_DEVICE_SUSPENDED,  "디바이스 일시정지" },
    { -EAGAIN,    ET_ERROR_WOULD_BLOCK,       "블로킹 방지" },
    { -ENOTTY,    ET_ERROR_NOT_SUPPORTED,     "지원되지 않는 기능" }
};

// ============================================================================
// ALSA 오류 처리 및 유틸리티 함수
// ============================================================================

/**
 * @brief ALSA 오류 코드를 공통 오류 코드로 매핑
 * @param alsa_error ALSA 오류 코드
 * @return 공통 오류 코드
 */
static ETResult linux_map_alsa_error(int alsa_error) {
    if (alsa_error >= 0) {
        return ET_SUCCESS;
    }

    // 매핑 테이블에서 검색
    for (size_t i = 0; i < sizeof(alsa_error_mappings) / sizeof(alsa_error_mappings[0]); i++) {
        if (alsa_error_mappings[i].alsa_error == alsa_error) {
            return alsa_error_mappings[i].common_error;
        }
    }

    // 매핑되지 않은 오류는 일반 하드웨어 오류로 처리
    return ET_ERROR_HARDWARE;
}

/**
 * @brief ALSA 오류 설명 가져오기
 * @param alsa_error ALSA 오류 코드
 * @return 오류 설명 문자열
 */
static const char* linux_get_alsa_error_description(int alsa_error) {
    // 매핑 테이블에서 설명 검색
    for (size_t i = 0; i < sizeof(alsa_error_mappings) / sizeof(alsa_error_mappings[0]); i++) {
        if (alsa_error_mappings[i].alsa_error == alsa_error) {
            return alsa_error_mappings[i].description;
        }
    }

    // ALSA 기본 오류 문자열 반환
    return snd_strerror(alsa_error);
}

/**
 * @brief ALSA PCM 상태를 공통 오디오 상태로 변환
 * @param alsa_state ALSA PCM 상태
 * @return 공통 오디오 상태
 */
static ETAudioState linux_map_alsa_state(snd_pcm_state_t alsa_state) {
    switch (alsa_state) {
        case SND_PCM_STATE_RUNNING:
            return ET_AUDIO_STATE_RUNNING;
        case SND_PCM_STATE_PAUSED:
            return ET_AUDIO_STATE_PAUSED;
        case SND_PCM_STATE_OPEN:
        case SND_PCM_STATE_SETUP:
        case SND_PCM_STATE_PREPARED:
        case SND_PCM_STATE_DRAINING:
            return ET_AUDIO_STATE_STOPPED;
        case SND_PCM_STATE_XRUN:
        case SND_PCM_STATE_SUSPENDED:
        case SND_PCM_STATE_DISCONNECTED:
        default:
            return ET_AUDIO_STATE_ERROR;
    }
}

/**
 * @brief ALSA 포맷을 공통 포맷으로 변환
 * @param alsa_format ALSA 포맷
 * @param et_format 공통 포맷 (출력)
 * @return 성공시 true, 실패시 false
 */
static bool linux_convert_alsa_format(snd_pcm_format_t alsa_format, ETAudioFormat* et_format) {
    switch (alsa_format) {
        case SND_PCM_FORMAT_FLOAT:
            et_format->bit_depth = 32;
            et_format->is_float = true;
            return true;
        case SND_PCM_FORMAT_S16_LE:
            et_format->bit_depth = 16;
            et_format->is_float = false;
            return true;
        case SND_PCM_FORMAT_S24_LE:
            et_format->bit_depth = 24;
            et_format->is_float = false;
            return true;
        case SND_PCM_FORMAT_S32_LE:
            et_format->bit_depth = 32;
            et_format->is_float = false;
            return true;
        default:
            return false;
    }
}

/**
 * @brief 공통 포맷을 ALSA 포맷으로 변환
 * @param et_format 공통 포맷
 * @return ALSA 포맷 (실패시 SND_PCM_FORMAT_UNKNOWN)
 */
static snd_pcm_format_t linux_convert_to_alsa_format(const ETAudioFormat* et_format) {
    if (et_format->is_float && et_format->bit_depth == 32) {
        return SND_PCM_FORMAT_FLOAT;
    } else if (!et_format->is_float) {
        switch (et_format->bit_depth) {
            case 16: return SND_PCM_FORMAT_S16_LE;
            case 24: return SND_PCM_FORMAT_S24_LE;
            case 32: return SND_PCM_FORMAT_S32_LE;
        }
    }
    return SND_PCM_FORMAT_UNKNOWN;
}

// ============================================================================
// ALSA PCM 설정 함수
// ============================================================================

/**
 * @brief ALSA PCM 하드웨어 파라미터 설정
 * @param device Linux 오디오 디바이스
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult linux_setup_alsa_hw_params(ETLinuxAudioDevice* device) {
    int err;
    snd_pcm_hw_params_t* hw_params = NULL;

    // 하드웨어 파라미터 할당
    err = snd_pcm_hw_params_malloc(&hw_params);
    if (err < 0) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "ALSA 하드웨어 파라미터 할당 실패: %s",
                     linux_get_alsa_error_description(err));
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기본 파라미터 설정
    err = snd_pcm_hw_params_any(device->pcm_handle, hw_params);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 기본 파라미터 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 접근 방식 설정 (인터리브드)
    err = snd_pcm_hw_params_set_access(device->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 접근 방식 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 포맷 설정
    snd_pcm_format_t alsa_format = linux_convert_to_alsa_format(&device->format);
    if (alsa_format == SND_PCM_FORMAT_UNKNOWN) {
        ET_SET_ERROR(ET_ERROR_NOT_SUPPORTED, "지원되지 않는 오디오 포맷");
        snd_pcm_hw_params_free(hw_params);
        return ET_ERROR_NOT_SUPPORTED;
    }

    err = snd_pcm_hw_params_set_format(device->pcm_handle, hw_params, alsa_format);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 포맷 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 채널 수 설정
    err = snd_pcm_hw_params_set_channels(device->pcm_handle, hw_params, device->format.num_channels);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 채널 수 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 샘플 레이트 설정
    unsigned int rate = device->format.sample_rate;
    err = snd_pcm_hw_params_set_rate_near(device->pcm_handle, hw_params, &rate, 0);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 샘플 레이트 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 실제 설정된 샘플 레이트 업데이트
    device->format.sample_rate = rate;

    // 버퍼 크기 설정
    snd_pcm_uframes_t buffer_size = device->format.buffer_size;
    err = snd_pcm_hw_params_set_buffer_size_near(device->pcm_handle, hw_params, &buffer_size);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 버퍼 크기 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 실제 설정된 버퍼 크기 업데이트
    device->format.buffer_size = buffer_size;

    // 피리어드 크기 설정
    snd_pcm_uframes_t period_size = buffer_size / 4;
    err = snd_pcm_hw_params_set_period_size_near(device->pcm_handle, hw_params, &period_size, 0);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 피리어드 크기 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 파라미터 적용
    err = snd_pcm_hw_params(device->pcm_handle, hw_params);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 하드웨어 파라미터 적용 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(hw_params);
        return linux_map_alsa_error(err);
    }

    // 지연시간 계산 (피리어드 크기 기반)
    device->latency_ms = (period_size * 1000) / device->format.sample_rate;

    // 하드웨어 파라미터 저장
    device->hw_params = hw_params;

    return ET_SUCCESS;
}

/**
 * @brief ALSA PCM 소프트웨어 파라미터 설정
 * @param device Linux 오디오 디바이스
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult linux_setup_alsa_sw_params(ETLinuxAudioDevice* device) {
    snd_pcm_sw_params_t* sw_params = NULL;
    int err;

    // 소프트웨어 파라미터 할당
    err = snd_pcm_sw_params_malloc(&sw_params);
    if (err < 0) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "ALSA 소프트웨어 파라미터 할당 실패: %s",
                     linux_get_alsa_error_description(err));
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 현재 소프트웨어 파라미터 가져오기
    err = snd_pcm_sw_params_current(device->pcm_handle, sw_params);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 현재 소프트웨어 파라미터 가져오기 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_sw_params_free(sw_params);
        return linux_map_alsa_error(err);
    }

    // 버퍼 크기 가져오기
    snd_pcm_uframes_t buffer_size;
    err = snd_pcm_hw_params_get_buffer_size(device->hw_params, &buffer_size);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 버퍼 크기 가져오기 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_sw_params_free(sw_params);
        return linux_map_alsa_error(err);
    }

    // 시작 임계값 설정 (버퍼의 절반)
    snd_pcm_uframes_t start_threshold = buffer_size / 2;
    err = snd_pcm_sw_params_set_start_threshold(device->pcm_handle, sw_params, start_threshold);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 시작 임계값 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_sw_params_free(sw_params);
        return linux_map_alsa_error(err);
    }

    // 정지 임계값 설정 (버퍼 크기)
    err = snd_pcm_sw_params_set_stop_threshold(device->pcm_handle, sw_params, buffer_size);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 정지 임계값 설정 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_sw_params_free(sw_params);
        return linux_map_alsa_error(err);
    }

    // 파라미터 적용
    err = snd_pcm_sw_params(device->pcm_handle, sw_params);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 소프트웨어 파라미터 적용 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_sw_params_free(sw_params);
        return linux_map_alsa_error(err);
    }

    snd_pcm_sw_params_free(sw_params);
    return ET_SUCCESS;
}

// ============================================================================
// ALSA 디바이스 열거 및 정보 조회 함수
// ============================================================================

/**
 * @brief ALSA 디바이스 정보 수집
 * @param device_name 디바이스 이름
 * @param device_type 디바이스 타입
 * @param device_info 디바이스 정보 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult linux_get_alsa_device_info(const char* device_name,
                                          ETAudioDeviceType device_type,
                                          ETAudioDeviceInfo* device_info) {
    snd_pcm_t* pcm_handle = NULL;
    snd_pcm_hw_params_t* hw_params = NULL;
    int err;

    // 디바이스 열기 (정보 조회용)
    snd_pcm_stream_t stream = (device_type == ET_AUDIO_DEVICE_OUTPUT) ?
                              SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE;

    err = snd_pcm_open(&pcm_handle, device_name, stream, SND_PCM_NONBLOCK);
    if (err < 0) {
        return linux_map_alsa_error(err);
    }

    // 하드웨어 파라미터 할당
    err = snd_pcm_hw_params_malloc(&hw_params);
    if (err < 0) {
        snd_pcm_close(pcm_handle);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기본 파라미터 가져오기
    err = snd_pcm_hw_params_any(pcm_handle, hw_params);
    if (err < 0) {
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_close(pcm_handle);
        return linux_map_alsa_error(err);
    }

    // 디바이스 정보 설정
    strncpy(device_info->name, device_name, sizeof(device_info->name) - 1);
    device_info->name[sizeof(device_info->name) - 1] = '\0';

    strncpy(device_info->id, device_name, sizeof(device_info->id) - 1);
    device_info->id[sizeof(device_info->id) - 1] = '\0';

    device_info->type = device_type;

    // 최대 채널 수 가져오기
    unsigned int max_channels;
    err = snd_pcm_hw_params_get_channels_max(hw_params, &max_channels);
    device_info->max_channels = (err >= 0) ? max_channels : 2;

    // 지원 샘플 레이트 조회 (일반적인 레이트들 테스트)
    static const uint32_t test_rates[] = {8000, 11025, 16000, 22050, 44100, 48000, 88200, 96000, 192000};
    static const int num_test_rates = sizeof(test_rates) / sizeof(test_rates[0]);

    device_info->supported_rates = malloc(num_test_rates * sizeof(uint32_t));
    device_info->rate_count = 0;

    if (device_info->supported_rates) {
        for (int i = 0; i < num_test_rates; i++) {
            unsigned int rate = test_rates[i];
            err = snd_pcm_hw_params_test_rate(pcm_handle, hw_params, rate, 0);
            if (err >= 0) {
                device_info->supported_rates[device_info->rate_count++] = rate;
            }
        }
    }

    // 기본 디바이스 여부 확인 (간단한 휴리스틱)
    device_info->is_default = (strcmp(device_name, "default") == 0) ||
                             (strstr(device_name, "default") != NULL);

    // 지연시간 추정 (일반적인 값)
    device_info->min_latency = 5;   // 5ms
    device_info->max_latency = 100; // 100ms

    // 정리
    snd_pcm_hw_params_free(hw_params);
    snd_pcm_close(pcm_handle);

    return ET_SUCCESS;
}

/**
 * @brief ALSA 디바이스 열거 구현
 * @param type 디바이스 타입
 * @param devices 디바이스 정보 배열 (출력)
 * @param count 디바이스 개수 (입출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult linux_enumerate_devices_impl(ETAudioDeviceType type,
                                            ETAudioDeviceInfo* devices,
                                            int* count) {
    void** hints = NULL;
    int device_count = 0;
    int max_devices = *count;

    // 디바이스 힌트 가져오기
    int err = snd_device_name_hint(-1, "pcm", &hints);
    if (err != 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 디바이스 힌트 가져오기 실패: %s",
                     linux_get_alsa_error_description(err));
        *count = 0;
        return linux_map_alsa_error(err);
    }

    // 힌트 순회
    void** hint = hints;
    while (*hint && device_count < max_devices) {
        char* name = snd_device_name_get_hint(*hint, "NAME");
        char* desc = snd_device_name_get_hint(*hint, "DESC");
        char* io = snd_device_name_get_hint(*hint, "IOID");

        // 디바이스 타입 확인
        bool device_matches = false;
        if (io == NULL) {
            // IO 타입이 명시되지 않은 경우 양방향으로 간주
            device_matches = true;
        } else if (type == ET_AUDIO_DEVICE_OUTPUT && strcmp(io, "Output") == 0) {
            device_matches = true;
        } else if (type == ET_AUDIO_DEVICE_INPUT && strcmp(io, "Input") == 0) {
            device_matches = true;
        }

        // 유효한 디바이스인 경우 정보 수집
        if (name && device_matches && strlen(name) > 0) {
            // null, pulse 등의 가상 디바이스는 제외
            if (strcmp(name, "null") != 0 && strstr(name, "pulse") == NULL) {
                ETResult result = linux_get_alsa_device_info(name, type, &devices[device_count]);
                if (result == ET_SUCCESS) {
                    // 설명이 있으면 이름에 추가
                    if (desc && strlen(desc) > 0) {
                        char temp_name[256];
                        snprintf(temp_name, sizeof(temp_name), "%s (%s)", name, desc);
                        strncpy(devices[device_count].name, temp_name, sizeof(devices[device_count].name) - 1);
                        devices[device_count].name[sizeof(devices[device_count].name) - 1] = '\0';
                    }
                    device_count++;
                }
            }
        }

        // 메모리 해제
        if (name) free(name);
        if (desc) free(desc);
        if (io) free(io);

        hint++;
    }

    // 힌트 해제
    snd_device_name_free_hint(hints);

    *count = device_count;
    return ET_SUCCESS;
}

// ============================================================================
// ALSA 오류 복구 및 스레드 처리 함수
// ============================================================================

/**
 * @brief ALSA XRUN 및 오류 복구
 * @param device Linux 오디오 디바이스
 * @param err ALSA 오류 코드
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult linux_recover_alsa_error(ETLinuxAudioDevice* device, int err) {
    if (err == -EPIPE) {
        // 언더런/오버런 복구
        ET_LOG_WARNING("ALSA 언더런/오버런 발생, 복구 시도");
        err = snd_pcm_prepare(device->pcm_handle);
        if (err < 0) {
            ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 언더런 복구 실패: %s",
                         linux_get_alsa_error_description(err));
            device->state = ET_AUDIO_STATE_ERROR;
            return linux_map_alsa_error(err);
        }
        return ET_SUCCESS;
    } else if (err == -ESTRPIPE) {
        // 서스펜드 복구
        ET_LOG_WARNING("ALSA 서스펜드 상태 감지, 복구 시도");
        while ((err = snd_pcm_resume(device->pcm_handle)) == -EAGAIN) {
            usleep(100000); // 100ms 대기
        }

        if (err < 0) {
            // 재개 실패시 재준비
            err = snd_pcm_prepare(device->pcm_handle);
            if (err < 0) {
                ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 서스펜드 복구 실패: %s",
                             linux_get_alsa_error_description(err));
                device->state = ET_AUDIO_STATE_ERROR;
                return linux_map_alsa_error(err);
            }
        }
        return ET_SUCCESS;
    }

    // 기타 오류는 매핑하여 반환
    return linux_map_alsa_error(err);
}

/**
 * @brief 오디오 처리 스레드 함수
 * @param arg Linux 오디오 디바이스 포인터
 * @return NULL
 */
static void* linux_audio_thread(void* arg) {
    ETLinuxAudioDevice* device = (ETLinuxAudioDevice*)arg;
    float* buffer = NULL;
    snd_pcm_sframes_t frames_per_period;
    int err;

    // 피리어드 크기 가져오기
    snd_pcm_uframes_t period_size;
    err = snd_pcm_hw_params_get_period_size(device->hw_params, &period_size, NULL);
    if (err < 0) {
        ET_LOG_ERROR("피리어드 크기 가져오기 실패: %s", linux_get_alsa_error_description(err));
        device->state = ET_AUDIO_STATE_ERROR;
        return NULL;
    }

    frames_per_period = period_size;

    // 오디오 버퍼 할당
    size_t buffer_size = frames_per_period * device->format.num_channels * sizeof(float);
    buffer = (float*)malloc(buffer_size);
    if (!buffer) {
        ET_LOG_ERROR("오디오 버퍼 할당 실패");
        device->state = ET_AUDIO_STATE_ERROR;
        return NULL;
    }

    ET_LOG_INFO("Linux 오디오 스레드 시작 (피리어드 크기: %ld)", frames_per_period);

    // 메인 오디오 루프
    while (device->thread_running) {
        // 콜백이 설정되어 있고 실행 상태인 경우에만 처리
        if (device->callback && device->state == ET_AUDIO_STATE_RUNNING) {
            // 콜백 호출
            int callback_result = device->callback(buffer, frames_per_period, device->user_data);
            if (callback_result < 0) {
                ET_LOG_WARNING("오디오 콜백에서 중단 요청");
                break;
            }

            // ALSA에 데이터 쓰기 (출력) 또는 읽기 (입력)
            if (device->type == ET_AUDIO_DEVICE_OUTPUT) {
                err = snd_pcm_writei(device->pcm_handle, buffer, frames_per_period);
            } else {
                err = snd_pcm_readi(device->pcm_handle, buffer, frames_per_period);
                // 입력의 경우 읽은 데이터로 콜백 다시 호출
                if (err > 0) {
                    device->callback(buffer, err, device->user_data);
                }
            }

            // 오류 처리
            if (err < 0) {
                ETResult recover_result = linux_recover_alsa_error(device, err);
                if (recover_result != ET_SUCCESS) {
                    ET_LOG_ERROR("ALSA 오류 복구 실패, 스레드 종료");
                    break;
                }
            } else if (err != frames_per_period) {
                ET_LOG_WARNING("예상과 다른 프레임 수 처리: %d (예상: %ld)", err, frames_per_period);
            }
        } else {
            // 콜백이 없거나 정지 상태인 경우 대기
            usleep(10000); // 10ms 대기
        }
    }

    // 정리
    free(buffer);
    ET_LOG_INFO("Linux 오디오 스레드 종료");
    return NULL;
}

// ============================================================================
// 플랫폼 추상화 인터페이스 구현
// ============================================================================

/**
 * @brief Linux 출력 디바이스 열기
 */
static ETResult linux_open_output_device(const char* device_name,
                                        const ETAudioFormat* format,
                                        ETAudioDevice** device) {
    if (!device_name || !format || !device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Linux 오디오 디바이스 할당
    ETLinuxAudioDevice* linux_device = (ETLinuxAudioDevice*)calloc(1, sizeof(ETLinuxAudioDevice));
    if (!linux_device) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기본 정보 설정
    linux_device->format = *format;
    linux_device->type = ET_AUDIO_DEVICE_OUTPUT;
    linux_device->state = ET_AUDIO_STATE_STOPPED;
    linux_device->thread_running = false;
    strncpy(linux_device->device_name, device_name, sizeof(linux_device->device_name) - 1);

    // ALSA PCM 디바이스 열기
    int err = snd_pcm_open(&linux_device->pcm_handle, device_name,
                          SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 출력 디바이스 열기 실패 (%s): %s",
                     device_name, linux_get_alsa_error_description(err));
        free(linux_device);
        return linux_map_alsa_error(err);
    }

    // 하드웨어 파라미터 설정
    ETResult result = linux_setup_alsa_hw_params(linux_device);
    if (result != ET_SUCCESS) {
        snd_pcm_close(linux_device->pcm_handle);
        free(linux_device);
        return result;
    }

    // 소프트웨어 파라미터 설정
    result = linux_setup_alsa_sw_params(linux_device);
    if (result != ET_SUCCESS) {
        snd_pcm_hw_params_free(linux_device->hw_params);
        snd_pcm_close(linux_device->pcm_handle);
        free(linux_device);
        return result;
    }

    // PCM 준비
    err = snd_pcm_prepare(linux_device->pcm_handle);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA PCM 준비 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(linux_device->hw_params);
        snd_pcm_close(linux_device->pcm_handle);
        free(linux_device);
        return linux_map_alsa_error(err);
    }

    *device = (ETAudioDevice*)linux_device;
    return ET_SUCCESS;
}

/**
 * @brief Linux 입력 디바이스 열기
 */
static ETResult linux_open_input_device(const char* device_name,
                                       const ETAudioFormat* format,
                                       ETAudioDevice** device) {
    if (!device_name || !format || !device) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Linux 오디오 디바이스 할당
    ETLinuxAudioDevice* linux_device = (ETLinuxAudioDevice*)calloc(1, sizeof(ETLinuxAudioDevice));
    if (!linux_device) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기본 정보 설정
    linux_device->format = *format;
    linux_device->type = ET_AUDIO_DEVICE_INPUT;
    linux_device->state = ET_AUDIO_STATE_STOPPED;
    linux_device->thread_running = false;
    strncpy(linux_device->device_name, device_name, sizeof(linux_device->device_name) - 1);

    // ALSA PCM 디바이스 열기
    int err = snd_pcm_open(&linux_device->pcm_handle, device_name,
                          SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 입력 디바이스 열기 실패 (%s): %s",
                     device_name, linux_get_alsa_error_description(err));
        free(linux_device);
        return linux_map_alsa_error(err);
    }

    // 하드웨어 파라미터 설정
    ETResult result = linux_setup_alsa_hw_params(linux_device);
    if (result != ET_SUCCESS) {
        snd_pcm_close(linux_device->pcm_handle);
        free(linux_device);
        return result;
    }

    // 소프트웨어 파라미터 설정
    result = linux_setup_alsa_sw_params(linux_device);
    if (result != ET_SUCCESS) {
        snd_pcm_hw_params_free(linux_device->hw_params);
        snd_pcm_close(linux_device->pcm_handle);
        free(linux_device);
        return result;
    }

    // PCM 준비
    err = snd_pcm_prepare(linux_device->pcm_handle);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA PCM 준비 실패: %s",
                     linux_get_alsa_error_description(err));
        snd_pcm_hw_params_free(linux_device->hw_params);
        snd_pcm_close(linux_device->pcm_handle);
        free(linux_device);
        return linux_map_alsa_error(err);
    }

    *device = (ETAudioDevice*)linux_device;
    return ET_SUCCESS;
}

/**
 * @brief Linux 오디오 디바이스 닫기
 */
static void linux_close_device(ETAudioDevice* device) {
    if (!device) return;

    ETLinuxAudioDevice* linux_device = (ETLinuxAudioDevice*)device;

    // 스트림 정지
    if (linux_device->state == ET_AUDIO_STATE_RUNNING) {
        linux_device->thread_running = false;
        if (linux_device->audio_thread) {
            pthread_join(linux_device->audio_thread, NULL);
        }
    }

    // ALSA 리소스 정리
    if (linux_device->hw_params) {
        snd_pcm_hw_params_free(linux_device->hw_params);
    }

    if (linux_device->pcm_handle) {
        snd_pcm_close(linux_device->pcm_handle);
    }

    // 지원 샘플 레이트 배열 해제 (디바이스 열거에서 할당된 경우)
    // 이 구현에서는 디바이스 구조체에 직접 할당하지 않으므로 생략

    free(linux_device);
}

/**
 * @brief Linux 오디오 스트림 시작
 */
static ETResult linux_start_stream(ETAudioDevice* device) {
    if (!device) return ET_ERROR_INVALID_ARGUMENT;

    ETLinuxAudioDevice* linux_device = (ETLinuxAudioDevice*)device;

    if (linux_device->state == ET_AUDIO_STATE_RUNNING) {
        return ET_SUCCESS; // 이미 실행 중
    }

    // 오디오 스레드 시작
    linux_device->thread_running = true;
    linux_device->state = ET_AUDIO_STATE_RUNNING;

    int err = pthread_create(&linux_device->audio_thread, NULL, linux_audio_thread, linux_device);
    if (err != 0) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "오디오 스레드 생성 실패: %d", err);
        linux_device->thread_running = false;
        linux_device->state = ET_AUDIO_STATE_ERROR;
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

/**
 * @brief Linux 오디오 스트림 정지
 */
static ETResult linux_stop_stream(ETAudioDevice* device) {
    if (!device) return ET_ERROR_INVALID_ARGUMENT;

    ETLinuxAudioDevice* linux_device = (ETLinuxAudioDevice*)device;

    if (linux_device->state != ET_AUDIO_STATE_RUNNING) {
        return ET_SUCCESS; // 이미 정지됨
    }

    // 스레드 정지
    linux_device->thread_running = false;
    if (linux_device->audio_thread) {
        pthread_join(linux_device->audio_thread, NULL);
        linux_device->audio_thread = 0;
    }

    // PCM 정지
    int err = snd_pcm_drop(linux_device->pcm_handle);
    if (err < 0) {
        ET_LOG_WARNING("ALSA PCM 정지 실패: %s", linux_get_alsa_error_description(err));
    }

    // PCM 재준비
    err = snd_pcm_prepare(linux_device->pcm_handle);
    if (err < 0) {
        ET_LOG_WARNING("ALSA PCM 재준비 실패: %s", linux_get_alsa_error_description(err));
    }

    linux_device->state = ET_AUDIO_STATE_STOPPED;
    return ET_SUCCESS;
}

/**
 * @brief Linux 오디오 스트림 일시정지
 */
static ETResult linux_pause_stream(ETAudioDevice* device) {
    if (!device) return ET_ERROR_INVALID_ARGUMENT;

    ETLinuxAudioDevice* linux_device = (ETLinuxAudioDevice*)device;

    if (linux_device->state != ET_AUDIO_STATE_RUNNING) {
        return ET_ERROR_INVALID_STATE;
    }

    // ALSA 일시정지
    int err = snd_pcm_pause(linux_device->pcm_handle, 1);
    if (err < 0) {
        ET_SET_ERROR(linux_map_alsa_error(err), "ALSA 일시정지 실패: %s",
                     linux_get_alsa_error_description(err));
        return linux_map_alsa_error(err);
    }

    linux_device->state = ET_AUDIO_STATE_PAUSED;
    return ET_SUCCESS;
}

/**
 * @brief Linux 오디오 콜백 설정
 */
static ETResult linux_set_callback(ETAudioDevice* device, ETAudioCallback callback, void* user_data) {
    if (!device) return ET_ERROR_INVALID_ARGUMENT;

    ETLinuxAudioDevice* linux_device = (ETLinuxAudioDevice*)device;
    linux_device->callback = callback;
    linux_device->user_data = user_data;

    return ET_SUCCESS;
}

/**
 * @brief Linux 오디오 디바이스 열거
 */
static ETResult linux_enumerate_devices(ETAudioDeviceType type, ETAudioDeviceInfo* devices, int* count) {
    return linux_enumerate_devices_impl(type, devices, count);
}

/**
 * @brief Linux 오디오 디바이스 지연시간 조회
 */
static uint32_t linux_get_latency(const ETAudioDevice* device) {
    if (!device) return 0;

    const ETLinuxAudioDevice* linux_device = (const ETLinuxAudioDevice*)device;
    return linux_device->latency_ms;
}

/**
 * @brief Linux 오디오 디바이스 상태 조회
 */
static ETAudioState linux_get_state(const ETAudioDevice* device) {
    if (!device) return ET_AUDIO_STATE_ERROR;

    const ETLinuxAudioDevice* linux_device = (const ETLinuxAudioDevice*)device;
    return linux_device->state;
}

/**
 * @brief Linux 오디오 포맷 지원 여부 확인
 */
static bool linux_is_format_supported(const char* device_name, const ETAudioFormat* format) {
    if (!device_name || !format) return false;

    snd_pcm_t* pcm_handle = NULL;
    snd_pcm_hw_params_t* hw_params = NULL;
    bool supported = false;
    int err;

    // 디바이스 열기 (테스트용)
    err = snd_pcm_open(&pcm_handle, device_name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err < 0) {
        return false;
    }

    // 하드웨어 파라미터 할당
    err = snd_pcm_hw_params_malloc(&hw_params);
    if (err < 0) {
        snd_pcm_close(pcm_handle);
        return false;
    }

    // 기본 파라미터 설정
    err = snd_pcm_hw_params_any(pcm_handle, hw_params);
    if (err < 0) {
        goto cleanup;
    }

    // 포맷 테스트
    snd_pcm_format_t alsa_format = linux_convert_to_alsa_format(format);
    if (alsa_format == SND_PCM_FORMAT_UNKNOWN) {
        goto cleanup;
    }

    err = snd_pcm_hw_params_test_format(pcm_handle, hw_params, alsa_format);
    if (err < 0) {
        goto cleanup;
    }

    // 채널 수 테스트
    err = snd_pcm_hw_params_test_channels(pcm_handle, hw_params, format->num_channels);
    if (err < 0) {
        goto cleanup;
    }

    // 샘플 레이트 테스트
    err = snd_pcm_hw_params_test_rate(pcm_handle, hw_params, format->sample_rate, 0);
    if (err < 0) {
        goto cleanup;
    }

    supported = true;

cleanup:
    snd_pcm_hw_params_free(hw_params);
    snd_pcm_close(pcm_handle);
    return supported;
}

/**
 * @brief Linux 지원 오디오 포맷 조회
 */
static ETResult linux_get_supported_formats(const char* device_name, ETAudioFormat* formats, int* count) {
    // 간단한 구현: 일반적인 포맷들을 반환
    if (!device_name || !formats || !count) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int max_formats = *count;
    int format_count = 0;

    // 일반적인 포맷들 테스트
    ETAudioFormat test_formats[] = {
        {44100, 16, 2, 4, 1024, false},  // 16-bit 스테레오 44.1kHz
        {48000, 16, 2, 4, 1024, false},  // 16-bit 스테레오 48kHz
        {44100, 32, 2, 8, 1024, true},   // 32-bit float 스테레오 44.1kHz
        {48000, 32, 2, 8, 1024, true},   // 32-bit float 스테레오 48kHz
        {44100, 16, 1, 2, 1024, false},  // 16-bit 모노 44.1kHz
        {48000, 16, 1, 2, 1024, false}   // 16-bit 모노 48kHz
    };

    int num_test_formats = sizeof(test_formats) / sizeof(test_formats[0]);

    for (int i = 0; i < num_test_formats && format_count < max_formats; i++) {
        if (linux_is_format_supported(device_name, &test_formats[i])) {
            formats[format_count++] = test_formats[i];
        }
    }

    *count = format_count;
    return ET_SUCCESS;
}

// ============================================================================
// Linux 오디오 인터페이스 구조체
// ============================================================================

/**
 * @brief Linux ALSA 오디오 인터페이스
 */
static ETAudioInterface linux_audio_interface = {
    .open_output_device = linux_open_output_device,
    .open_input_device = linux_open_input_device,
    .close_device = linux_close_device,
    .start_stream = linux_start_stream,
    .stop_stream = linux_stop_stream,
    .pause_stream = linux_pause_stream,
    .set_callback = linux_set_callback,
    .enumerate_devices = linux_enumerate_devices,
    .get_latency = linux_get_latency,
    .get_state = linux_get_state,
    .is_format_supported = linux_is_format_supported,
    .get_supported_formats = linux_get_supported_formats,
    .platform_data = NULL
};

// ============================================================================
// 공개 함수
// ============================================================================

/**
 * @brief Linux 오디오 인터페이스 가져오기
 * @return Linux 오디오 인터페이스 포인터
 */
const ETAudioInterface* et_get_linux_audio_interface(void) {
    return &linux_audio_interface;
}

#else
// ALSA가 없는 경우 스텁 구현
const ETAudioInterface* et_get_linux_audio_interface(void) {
    ET_LOG_WARNING("ALSA 라이브러리가 없어 Linux 오디오 인터페이스를 사용할 수 없습니다");
    return NULL;
}
#endif // HAVE_ALSA

#else
// Linux가 아닌 플랫폼에서는 NULL 반환
const ETAudioInterface* et_get_linux_audio_interface(void) {
    return NULL;
}
#endif