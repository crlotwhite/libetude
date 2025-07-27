/**
 * @file macos_audio.c
 * @brief macOS 오디오 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

/**
 * @brief macOS 오디오 시스템 초기화
 */
void macos_audio_init(void) {
    // macOS에서는 별도 초기화가 필요하지 않음
    // AudioQueue와 AudioUnit은 필요시 자동으로 초기화됨
}

/**
 * @brief macOS 오디오 시스템 정리
 */
void macos_audio_finalize(void) {
    // 시스템 레벨 정리 작업
}

/**
 * @brief 기본 출력 디바이스 ID 조회
 * @return 디바이스 ID (실패시 kAudioObjectUnknown)
 */
AudioDeviceID macos_get_default_output_device(void) {
    AudioDeviceID device_id = kAudioObjectUnknown;
    UInt32 size = sizeof(device_id);

    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &property_address,
                                               0, NULL,
                                               &size, &device_id);

    if (status != noErr) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to get default output device");
        return kAudioObjectUnknown;
    }

    return device_id;
}

/**
 * @brief 기본 입력 디바이스 ID 조회
 * @return 디바이스 ID (실패시 kAudioObjectUnknown)
 */
AudioDeviceID macos_get_default_input_device(void) {
    AudioDeviceID device_id = kAudioObjectUnknown;
    UInt32 size = sizeof(device_id);

    AudioObjectPropertyAddress property_address = {
        kAudioHardwarePropertyDefaultInputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                               &property_address,
                                               0, NULL,
                                               &size, &device_id);

    if (status != noErr) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to get default input device");
        return kAudioObjectUnknown;
    }

    return device_id;
}

/**
 * @brief 디바이스 샘플 레이트 설정
 * @param device_id 디바이스 ID
 * @param sample_rate 샘플 레이트
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult macos_set_device_sample_rate(AudioDeviceID device_id, Float64 sample_rate) {
    AudioObjectPropertyAddress property_address = {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectSetPropertyData(device_id,
                                               &property_address,
                                               0, NULL,
                                               sizeof(sample_rate), &sample_rate);

    if (status != noErr) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to set device sample rate");
        return ET_ERROR_HARDWARE;
    }

    return ET_SUCCESS;
}

/**
 * @brief 디바이스 버퍼 크기 설정
 * @param device_id 디바이스 ID
 * @param buffer_size 버퍼 크기 (프레임 수)
 * @return 성공시 ET_SUCCESS, 실패시 에러 코드
 */
ETResult macos_set_device_buffer_size(AudioDeviceID device_id, UInt32 buffer_size) {
    AudioObjectPropertyAddress property_address = {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectSetPropertyData(device_id,
                                               &property_address,
                                               0, NULL,
                                               sizeof(buffer_size), &buffer_size);

    if (status != noErr) {
        et_set_error(ET_ERROR_HARDWARE, __FILE__, __LINE__, __func__, "Failed to set device buffer size");
        return ET_ERROR_HARDWARE;
    }

    return ET_SUCCESS;
}

/**
 * @brief 오디오 디바이스 지연시간 조회
 * @param device_id 디바이스 ID
 * @param is_input 입력 디바이스 여부
 * @return 지연시간 (프레임 수)
 */
UInt32 macos_get_device_latency(AudioDeviceID device_id, bool is_input) {
    UInt32 latency = 0;
    UInt32 size = sizeof(latency);

    AudioObjectPropertyAddress property_address = {
        kAudioDevicePropertyLatency,
        is_input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(device_id,
                                               &property_address,
                                               0, NULL,
                                               &size, &latency);

    if (status != noErr) {
        return 0; // 기본값 반환
    }

    return latency;
}

#else
// macOS가 아닌 플랫폼에서는 빈 구현
void macos_audio_init(void) {}
void macos_audio_finalize(void) {}
#endif