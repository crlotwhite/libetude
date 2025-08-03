/**
 * @file windows_audio_fallback.c
 * @brief Windows 오디오 폴백 관리 시스템
 * @author LibEtude Project
 * @version 1.0.0
 *
 * WASAPI와 DirectSound 간의 자동 폴백 로직 및 상태 관리
 */

#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

/* 오디오 백엔드 타입 */
typedef enum {
    ET_AUDIO_BACKEND_NONE = 0,
    ET_AUDIO_BACKEND_WASAPI,
    ET_AUDIO_BACKEND_DIRECTSOUND
} ETAudioBackendType;

/* 폴백 상태 */
typedef enum {
    ET_FALLBACK_STATE_NONE = 0,
    ET_FALLBACK_STATE_WASAPI_ACTIVE,
    ET_FALLBACK_STATE_DIRECTSOUND_FALLBACK,
    ET_FALLBACK_STATE_FAILED
} ETFallbackState;

/* 폴백 관리 구조체 */
typedef struct {
    ETAudioBackendType current_backend;
    ETFallbackState fallback_state;
    DWORD fallback_attempts;
    DWORD max_fallback_attempts;
    LARGE_INTEGER last_fallback_time;
    LARGE_INTEGER fallback_cooldown; /* 100ms 쿨다운 */
    bool auto_recovery_enabled;
} ETAudioFallbackManager;

/* 전역 폴백 관리자 */
static ETAudioFallbackManager g_fallback_manager = { 0 };

/**
 * @brief 폴백 관리자 초기화
 */
static void _init_fallback_manager(void) {
    if (g_fallback_manager.current_backend != ET_AUDIO_BACKEND_NONE) {
        return; /* 이미 초기화됨 */
    }

    memset(&g_fallback_manager, 0, sizeof(ETAudioFallbackManager));
    g_fallback_manager.current_backend = ET_AUDIO_BACKEND_NONE;
    g_fallback_manager.fallback_state = ET_FALLBACK_STATE_NONE;
    g_fallback_manager.max_fallback_attempts = 3;
    g_fallback_manager.auto_recovery_enabled = true;

    /* 성능 카운터 초기화 */
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    g_fallback_manager.fallback_cooldown.QuadPart = frequency.QuadPart / 10; /* 100ms */

    ET_LOG_INFO("오디오 폴백 관리자 초기화 완료");
}

/**
 * @brief 폴백 쿨다운 확인
 */
static bool _is_fallback_cooldown_expired(void) {
    if (g_fallback_manager.last_fallback_time.QuadPart == 0) {
        return true; /* 첫 번째 폴백 */
    }

    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);

    LARGE_INTEGER elapsed;
    elapsed.QuadPart = current_time.QuadPart - g_fallback_manager.last_fallback_time.QuadPart;

    return elapsed.QuadPart >= g_fallback_manager.fallback_cooldown.QuadPart;
}

/**
 * @brief 폴백 시도 기록
 */
static void _record_fallback_attempt(void) {
    QueryPerformanceCounter(&g_fallback_manager.last_fallback_time);
    g_fallback_manager.fallback_attempts++;

    ET_LOG_INFO("폴백 시도 기록됨 (총 시도: %lu)", g_fallback_manager.fallback_attempts);
}

/**
 * @brief WASAPI 우선 초기화 (폴백 포함)
 */
ETResult et_windows_init_audio_with_fallback(ETAudioDevice* device,
                                           const ETAudioFormat* format) {
    if (!device || !format) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    _init_fallback_manager();

    ET_LOG_INFO("Windows 오디오 초기화 시작 (WASAPI 우선, DirectSound 폴백)");

    /* 폴백 한계 확인 */
    if (g_fallback_manager.fallback_attempts >= g_fallback_manager.max_fallback_attempts) {
        if (!_is_fallback_cooldown_expired()) {
            ET_LOG_WARNING("Fallback attempt limit exceeded, waiting for cooldown");
            ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                        "Fallback attempt limit exceeded");
            return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
        } else {
            /* 쿨다운 만료, 카운터 리셋 */
            g_fallback_manager.fallback_attempts = 0;
            ET_LOG_INFO("Fallback cooldown expired, resetting attempt counter");
        }
    }

    /* WASAPI 초기화 시도 */
    ETResult result = et_audio_init_wasapi_with_fallback(device);

    if (result == ET_SUCCESS) {
        g_fallback_manager.current_backend = ET_AUDIO_BACKEND_WASAPI;
        g_fallback_manager.fallback_state = ET_FALLBACK_STATE_WASAPI_ACTIVE;
        g_fallback_manager.fallback_attempts = 0; /* 성공 시 카운터 리셋 */

        ET_LOG_INFO("WASAPI 초기화 성공");
        return ET_SUCCESS;
    }

    /* WASAPI 실패, DirectSound 폴백 시도 */
    ET_LOG_WARNING("WASAPI initialization failed (error: %d), attempting DirectSound fallback", result);

    _record_fallback_attempt();

    result = et_audio_fallback_to_directsound(device);

    if (result == ET_SUCCESS) {
        g_fallback_manager.current_backend = ET_AUDIO_BACKEND_DIRECTSOUND;
        g_fallback_manager.fallback_state = ET_FALLBACK_STATE_DIRECTSOUND_FALLBACK;

        ET_LOG_INFO("DirectSound 폴백 성공");
        return ET_SUCCESS;
    }

    /* 모든 백엔드 실패 */
    g_fallback_manager.current_backend = ET_AUDIO_BACKEND_NONE;
    g_fallback_manager.fallback_state = ET_FALLBACK_STATE_FAILED;

    ET_LOG_ERROR("모든 오디오 백엔드 초기화 실패");
    ET_SET_ERROR(ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED,
                "Both WASAPI and DirectSound initialization failed");

    return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
}

/**
 * @brief 현재 오디오 백엔드 상태 확인
 */
ETResult et_windows_check_audio_backend_status(ETAudioDevice* device) {
    if (!device) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    switch (g_fallback_manager.current_backend) {
        case ET_AUDIO_BACKEND_WASAPI:
            /* WASAPI 상태 확인 로직 (구현 필요) */
            /* return et_windows_check_wasapi_device_status(device); */
            return ET_SUCCESS;

        case ET_AUDIO_BACKEND_DIRECTSOUND:
            return et_windows_check_directsound_device_status((ETDirectSoundDevice*)device);

        case ET_AUDIO_BACKEND_NONE:
        default:
            return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }
}

/**
 * @brief 오디오 백엔드 자동 복구 시도
 */
ETResult et_windows_attempt_audio_recovery(ETAudioDevice* device) {
    if (!device || !g_fallback_manager.auto_recovery_enabled) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ET_LOG_INFO("오디오 백엔드 자동 복구 시도");

    /* 현재 백엔드 상태 확인 */
    ETResult status = et_windows_check_audio_backend_status(device);

    if (status == ET_SUCCESS) {
        ET_LOG_INFO("현재 오디오 백엔드 정상 동작 중");
        return ET_SUCCESS;
    }

    /* 복구 시도 */
    switch (g_fallback_manager.current_backend) {
        case ET_AUDIO_BACKEND_WASAPI:
            ET_LOG_INFO("WASAPI 복구 시도 중...");
            /* WASAPI 복구 로직 (구현 필요) */
            /* 복구 실패 시 DirectSound로 폴백 */
            ET_LOG_WARNING("WASAPI 복구 실패, DirectSound로 폴백");
            return et_audio_fallback_to_directsound(device);

        case ET_AUDIO_BACKEND_DIRECTSOUND:
            ET_LOG_INFO("DirectSound 복구 시도 중...");
            /* DirectSound 복구는 내부적으로 처리됨 */
            return ET_SUCCESS;

        default:
            ET_LOG_ERROR("알 수 없는 오디오 백엔드 상태");
            return ET_WINDOWS_ERROR_DIRECTSOUND_FALLBACK_FAILED;
    }
}

/**
 * @brief 폴백 관리자 상태 정보 가져오기
 */
ETResult et_windows_get_fallback_manager_info(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    const char* backend_name = "알 수 없음";
    const char* state_name = "알 수 없음";

    switch (g_fallback_manager.current_backend) {
        case ET_AUDIO_BACKEND_WASAPI: backend_name = "WASAPI"; break;
        case ET_AUDIO_BACKEND_DIRECTSOUND: backend_name = "DirectSound"; break;
        case ET_AUDIO_BACKEND_NONE: backend_name = "없음"; break;
    }

    switch (g_fallback_manager.fallback_state) {
        case ET_FALLBACK_STATE_WASAPI_ACTIVE: state_name = "WASAPI 활성"; break;
        case ET_FALLBACK_STATE_DIRECTSOUND_FALLBACK: state_name = "DirectSound 폴백"; break;
        case ET_FALLBACK_STATE_FAILED: state_name = "실패"; break;
        case ET_FALLBACK_STATE_NONE: state_name = "초기화되지 않음"; break;
    }

    int written = snprintf(buffer, buffer_size,
        "오디오 폴백 관리자 상태:\n"
        "  현재 백엔드: %s\n"
        "  폴백 상태: %s\n"
        "  폴백 시도 횟수: %lu/%lu\n"
        "  자동 복구: %s\n",
        backend_name, state_name,
        g_fallback_manager.fallback_attempts,
        g_fallback_manager.max_fallback_attempts,
        g_fallback_manager.auto_recovery_enabled ? "활성" : "비활성");

    if (written < 0 || (size_t)written >= buffer_size) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    return ET_SUCCESS;
}

/**
 * @brief 자동 복구 활성화/비활성화
 */
void et_windows_set_auto_recovery_enabled(bool enabled) {
    g_fallback_manager.auto_recovery_enabled = enabled;
    ET_LOG_INFO("오디오 자동 복구 %s", enabled ? "활성화" : "비활성화");
}

/**
 * @brief 폴백 관리자 정리
 */
void et_windows_cleanup_fallback_manager(void) {
    memset(&g_fallback_manager, 0, sizeof(ETAudioFallbackManager));
    ET_LOG_INFO("오디오 폴백 관리자 정리 완료");
}

#endif /* _WIN32 */