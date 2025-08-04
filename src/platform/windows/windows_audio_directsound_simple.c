/**
 * @file windows_audio_directsound_simple.c
 * @brief Windows DirectSound 오디오 백엔드 간단 구현
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

/* DirectSound 기본 구현 */
ETResult et_audio_fallback_to_directsound(ETAudioDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("DirectSound 폴백 모드로 전환");

    /* 현재는 기본 구현만 제공 */
    ET_LOG_WARNING("DirectSound implementation incomplete");
    return ET_SUCCESS;
}

ETResult et_windows_start_directsound_device(ETDirectSoundDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("DirectSound device start (basic implementation)");
    return ET_SUCCESS;
}

ETResult et_windows_stop_directsound_device(ETDirectSoundDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("DirectSound device stop (basic implementation)");
    return ET_SUCCESS;
}

void et_windows_cleanup_directsound_device(ETDirectSoundDevice* device) {
    if (!device) {
        return;
    }

    ET_LOG_INFO("DirectSound device cleanup (basic implementation)");
}

void et_windows_directsound_cleanup(void) {
    ET_LOG_INFO("DirectSound cleanup complete");
}

#endif /* _WIN32 */