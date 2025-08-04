/**
 * @file linux_audio.c
 * @brief Linux ALSA 오디오 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"

#ifdef __linux__
#include <alsa/asoundlib.h>
#include <pthread.h>

/**
 * @brief Linux 오디오 시스템 초기화
 */
void linux_audio_init(void) {
    // ALSA는 별도 전역 초기화가 필요하지 않음
}

/**
 * @brief Linux 오디오 시스템 정리
 */
void linux_audio_finalize(void) {
    // ALSA 정리 작업
}

/**
 * @brief ALSA 에러 코드를 문자열로 변환
 * @param error ALSA 에러 코드
 * @return 에러 문자열
 */
static const char* linux_alsa_error_string(int error) {
    return snd_strerror(error);
}

/**
 * @brief ALSA PCM 디바이스 설정
 * @param pcm_handle PCM 핸들
 * @param format 오디오 포맷
 * @param hw_params 하드웨어 파라미터 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult linux_setup_alsa_device(snd_pcm_t* pcm_handle,
                                const ETAudioFormat* format,
                                snd_pcm_hw_params_t** hw_params) {
    int err;

    // 하드웨어 파라미터 할당
    err = snd_pcm_hw_params_malloc(hw_params);
    if (err < 0) {
        et_set_error(ET_ERROR_OUT_OF_MEMORY, "Failed to allocate hw params", __FILE__, __LINE__, __func__);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기본 파라미터 설정
    err = snd_pcm_hw_params_any(pcm_handle, *hw_params);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    // 접근 방식 설정 (인터리브드)
    err = snd_pcm_hw_params_set_access(pcm_handle, *hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    // 포맷 설정 (32비트 float)
    err = snd_pcm_hw_params_set_format(pcm_handle, *hw_params, SND_PCM_FORMAT_FLOAT);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    // 채널 수 설정
    err = snd_pcm_hw_params_set_channels(pcm_handle, *hw_params, format->num_channels);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    // 샘플 레이트 설정
    unsigned int rate = format->sample_rate;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle, *hw_params, &rate, 0);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    // 버퍼 크기 설정
    snd_pcm_uframes_t buffer_size = format->buffer_size;
    err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, *hw_params, &buffer_size);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    // 피리어드 크기 설정
    snd_pcm_uframes_t period_size = buffer_size / 4;
    err = snd_pcm_hw_params_set_period_size_near(pcm_handle, *hw_params, &period_size, 0);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    // 파라미터 적용
    err = snd_pcm_hw_params(pcm_handle, *hw_params);
    if (err < 0) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "%s", linux_alsa_error_string(err));
        snd_pcm_hw_params_free(*hw_params);
        return ET_ERROR_HARDWARE;
    }

    return ET_SUCCESS;
}

/**
 * @brief ALSA 소프트웨어 파라미터 설정
 * @param pcm_handle PCM 핸들
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult linux_setup_alsa_sw_params(snd_pcm_t* pcm_handle) {
    snd_pcm_sw_params_t* sw_params;
    int err;

    // 소프트웨어 파라미터 할당
    err = snd_pcm_sw_params_malloc(&sw_params);
    if (err < 0) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 현재 소프트웨어 파라미터 가져오기
    err = snd_pcm_sw_params_current(pcm_handle, sw_params);
    if (err < 0) {
        snd_pcm_sw_params_free(sw_params);
        return ET_ERROR_HARDWARE;
    }

    // 시작 임계값 설정
    snd_pcm_uframes_t start_threshold;
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_current(pcm_handle, hw_params);
    snd_pcm_hw_params_get_buffer_size(hw_params, &start_threshold);

    err = snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, start_threshold / 2);
    if (err < 0) {
        snd_pcm_sw_params_free(sw_params);
        return ET_ERROR_HARDWARE;
    }

    // 파라미터 적용
    err = snd_pcm_sw_params(pcm_handle, sw_params);
    snd_pcm_sw_params_free(sw_params);

    return (err < 0) ? ET_ERROR_HARDWARE : ET_SUCCESS;
}

/**
 * @brief ALSA 디바이스 목록 조회
 * @param device_type 디바이스 타입
 * @param device_names 디바이스 이름 배열 (출력)
 * @param max_devices 최대 디바이스 수
 * @return 실제 디바이스 수
 */
int linux_enumerate_alsa_devices(ETAudioDeviceType device_type,
                                char device_names[][256],
                                int max_devices) {
    void** hints;
    int device_count = 0;

    // 디바이스 힌트 가져오기
    const char* interface = (device_type == ET_AUDIO_DEVICE_OUTPUT) ? "pcm" : "pcm";
    int err = snd_device_name_hint(-1, interface, &hints);

    if (err != 0) {
        return 0;
    }

    // 힌트 순회
    void** hint = hints;
    while (*hint && device_count < max_devices) {
        char* name = snd_device_name_get_hint(*hint, "NAME");
        char* desc = snd_device_name_get_hint(*hint, "DESC");
        char* io = snd_device_name_get_hint(*hint, "IOID");

        // 디바이스 타입 확인
        bool is_output = (device_type == ET_AUDIO_DEVICE_OUTPUT);
        bool device_matches = (io == NULL) ||
                             (is_output && strcmp(io, "Output") == 0) ||
                             (!is_output && strcmp(io, "Input") == 0);

        if (name && device_matches) {
            strncpy(device_names[device_count], name, 255);
            device_names[device_count][255] = '\0';
            device_count++;
        }

        if (name) free(name);
        if (desc) free(desc);
        if (io) free(io);

        hint++;
    }

    snd_device_name_free_hint(hints);
    return device_count;
}

/**
 * @brief ALSA PCM 상태 문자열 변환
 * @param state PCM 상태
 * @return 상태 문자열
 */
const char* linux_alsa_state_string(snd_pcm_state_t state) {
    switch (state) {
        case SND_PCM_STATE_OPEN: return "Open";
        case SND_PCM_STATE_SETUP: return "Setup";
        case SND_PCM_STATE_PREPARED: return "Prepared";
        case SND_PCM_STATE_RUNNING: return "Running";
        case SND_PCM_STATE_XRUN: return "Xrun";
        case SND_PCM_STATE_DRAINING: return "Draining";
        case SND_PCM_STATE_PAUSED: return "Paused";
        case SND_PCM_STATE_SUSPENDED: return "Suspended";
        case SND_PCM_STATE_DISCONNECTED: return "Disconnected";
        default: return "Unknown";
    }
}

/**
 * @brief ALSA XRUN 복구
 * @param pcm_handle PCM 핸들
 * @param err 에러 코드
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult linux_alsa_recover_xrun(snd_pcm_t* pcm_handle, int err) {
    if (err == -EPIPE) {
        // 언더런/오버런 복구
        err = snd_pcm_prepare(pcm_handle);
        if (err < 0) {
            et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Cannot recover from underrun");
            return ET_ERROR_HARDWARE;
        }
    } else if (err == -ESTRPIPE) {
        // 서스펜드 복구
        while ((err = snd_pcm_resume(pcm_handle)) == -EAGAIN) {
            usleep(100000); // 100ms 대기
        }

        if (err < 0) {
            err = snd_pcm_prepare(pcm_handle);
            if (err < 0) {
                et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Cannot recover from suspend");
                return ET_ERROR_HARDWARE;
            }
        }
    }

    return ET_SUCCESS;
}

#else
// Linux가 아닌 플랫폼에서는 빈 구현
void linux_audio_init(void) {}
void linux_audio_finalize(void) {}
#endif