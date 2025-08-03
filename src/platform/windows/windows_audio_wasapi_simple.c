/**
 * @file windows_audio_wasapi_simple.c
 * @brief Windows WASAPI 오디오 백엔드 간단 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

/* WASAPI 기본 구현 */
ETResult et_audio_init_wasapi_with_fallback(ETAudioDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("WASAPI initialization attempt...");

    /* 현재는 기본 구현으로 DirectSound 폴백 */
    ET_LOG_WARNING("WASAPI implementation incomplete, fallback to DirectSound");
    return et_audio_fallback_to_directsound(device);
}

/* 디바이스 열거 기본 구현 */
ETResult et_windows_enumerate_audio_devices(ETWindowsAudioDevice** devices,
                                          uint32_t* device_count) {
    if (!devices || !device_count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    /* 기본 디바이스 하나만 반환 */
    ETWindowsAudioDevice* device_array = (ETWindowsAudioDevice*)malloc(sizeof(ETWindowsAudioDevice));
    if (!device_array) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(device_array, 0, sizeof(ETWindowsAudioDevice));

    /* 기본 디바이스 정보 설정 */
    wcscpy_s(device_array->device_id, 256, L"default");
    wcscpy_s(device_array->friendly_name, 256, L"Default Audio Device");
    device_array->sample_rate = 44100;
    device_array->channels = 2;
    device_array->bits_per_sample = 32;
    device_array->is_default = true;
    device_array->supports_exclusive = false;

    *devices = device_array;
    *device_count = 1;

    ET_LOG_INFO("Returning 1 default audio device");
    return ET_SUCCESS;
}

void et_windows_free_audio_devices(ETWindowsAudioDevice* devices) {
    if (devices) {
        free(devices);
    }
}

ETResult et_windows_init_wasapi_device(const wchar_t* device_id,
                                     const ETAudioFormat* format,
                                     ETWASAPIContext* context) {
    if (!device_id || !format || !context) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(context, 0, sizeof(ETWASAPIContext));

    ET_LOG_INFO("WASAPI device initialization (basic implementation)");
    return ET_SUCCESS;
}

void et_windows_cleanup_wasapi_context(ETWASAPIContext* context) {
    if (!context) {
        return;
    }

    memset(context, 0, sizeof(ETWASAPIContext));
}

void et_windows_wasapi_cleanup(void) {
    ET_LOG_INFO("WASAPI cleanup complete");
}

#endif /* _WIN32 */