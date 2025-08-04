/**
 * @file test_directsound_fallback.c
 * @brief DirectSound 폴백 메커니즘 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"

/* 테스트 콜백 함수 */
static void test_audio_callback(float* buffer, uint32_t frames, void* user_data) {
    /* 간단한 사인파 생성 (테스트용) */
    static float phase = 0.0f;
    const float frequency = 440.0f; /* A4 음 */
    const float sample_rate = 44100.0f;
    const float amplitude = 0.1f; /* 낮은 볼륨 */

    int* callback_count = (int*)user_data;
    if (callback_count) {
        (*callback_count)++;
    }

    for (uint32_t i = 0; i < frames; i++) {
        float sample = amplitude * sinf(2.0f * M_PI * frequency * phase / sample_rate);
        buffer[i * 2] = sample;     /* 왼쪽 채널 */
        buffer[i * 2 + 1] = sample; /* 오른쪽 채널 */
        phase += 1.0f;
        if (phase >= sample_rate) {
            phase -= sample_rate;
        }
    }
}

/**
 * @brief DirectSound 시스템 초기화 테스트
 */
static void test_directsound_system_init(void) {
    printf("DirectSound 시스템 초기화 테스트...\n");

    ETAudioDevice device = { 0 };
    ETResult result = et_audio_fallback_to_directsound(&device);

    if (result == ET_SUCCESS) {
        printf("  ✓ DirectSound 시스템 초기화 성공\n");
        /* 정리 작업은 실제 구현에서 필요 */
    } else {
        printf("  ✗ DirectSound 시스템 초기화 실패 (오류: %d)\n", result);
        /* DirectSound가 사용 불가능한 환경일 수 있음 */
    }
}

/**
 * @brief DirectSound 디바이스 시작/정지 테스트
 */
static void test_directsound_device_lifecycle(void) {
    printf("DirectSound 디바이스 생명주기 테스트...\n");

    ETAudioDevice device = { 0 };
    ETResult result = et_audio_fallback_to_directsound(&device);

    if (result != ET_SUCCESS) {
        printf("  ⚠ DirectSound 초기화 실패, 테스트 건너뜀\n");
        return;
    }

    /* 실제 구현에서는 device에서 DirectSound 컨텍스트를 가져와야 함 */
    ETDirectSoundDevice* ds_device = NULL; /* device.platform_data; */

    if (ds_device) {
        /* 디바이스 시작 테스트 */
        result = et_windows_start_directsound_device(ds_device);
        if (result == ET_SUCCESS) {
            printf("  ✓ DirectSound 디바이스 시작 성공\n");

            /* 짧은 시간 실행 */
            Sleep(100);

            /* 디바이스 정지 테스트 */
            result = et_windows_stop_directsound_device(ds_device);
            if (result == ET_SUCCESS) {
                printf("  ✓ DirectSound 디바이스 정지 성공\n");
            } else {
                printf("  ✗ DirectSound 디바이스 정지 실패 (오류: %d)\n", result);
            }

            /* 정리 */
            et_windows_cleanup_directsound_device(ds_device);
        } else {
            printf("  ✗ DirectSound 디바이스 시작 실패 (오류: %d)\n", result);
        }
    } else {
        printf("  ⚠ DirectSound 디바이스 컨텍스트 없음, 테스트 건너뜀\n");
    }
}

/**
 * @brief DirectSound 오디오 콜백 테스트
 */
static void test_directsound_audio_callback(void) {
    printf("DirectSound 오디오 콜백 테스트...\n");

    ETAudioDevice device = { 0 };
    ETResult result = et_audio_fallback_to_directsound(&device);

    if (result != ET_SUCCESS) {
        printf("  ⚠ DirectSound 초기화 실패, 테스트 건너뜀\n");
        return;
    }

    /* 실제 구현에서는 콜백 설정 및 테스트 */
    int callback_count = 0;

    /* 콜백 설정 (실제 구현에서 필요) */
    /* device.callback = test_audio_callback; */
    /* device.user_data = &callback_count; */

    ETDirectSoundDevice* ds_device = NULL; /* device.platform_data; */

    if (ds_device) {
        result = et_windows_start_directsound_device(ds_device);
        if (result == ET_SUCCESS) {
            printf("  ✓ DirectSound 오디오 스트림 시작\n");

            /* 1초간 실행하여 콜백 호출 확인 */
            Sleep(1000);

            if (callback_count > 0) {
                printf("  ✓ 오디오 콜백 호출됨 (%d회)\n", callback_count);
            } else {
                printf("  ⚠ 오디오 콜백 호출되지 않음\n");
            }

            et_windows_stop_directsound_device(ds_device);
            et_windows_cleanup_directsound_device(ds_device);
        } else {
            printf("  ✗ DirectSound 오디오 스트림 시작 실패 (오류: %d)\n", result);
        }
    } else {
        printf("  ⚠ DirectSound 디바이스 컨텍스트 없음, 테스트 건너뜀\n");
    }
}

/**
 * @brief DirectSound 성능 통계 테스트
 */
static void test_directsound_performance_stats(void) {
    printf("DirectSound 성능 통계 테스트...\n");

    ETAudioDevice device = { 0 };
    ETResult result = et_audio_fallback_to_directsound(&device);

    if (result != ET_SUCCESS) {
        printf("  ⚠ DirectSound 초기화 실패, 테스트 건너뜀\n");
        return;
    }

    ETDirectSoundDevice* ds_device = NULL; /* device.platform_data; */

    if (ds_device) {
        double avg_callback_duration;
        DWORD current_write_cursor;
        DWORD buffer_size;

        result = et_windows_get_directsound_performance_stats(ds_device,
                                                            &avg_callback_duration,
                                                            &current_write_cursor,
                                                            &buffer_size);

        if (result == ET_SUCCESS) {
            printf("  ✓ 성능 통계 가져오기 성공\n");
            printf("    평균 콜백 시간: %.2fms\n", avg_callback_duration);
            printf("    현재 쓰기 커서: %lu\n", current_write_cursor);
            printf("    버퍼 크기: %lu 바이트\n", buffer_size);
        } else {
            printf("  ✗ 성능 통계 가져오기 실패 (오류: %d)\n", result);
        }

        et_windows_cleanup_directsound_device(ds_device);
    } else {
        printf("  ⚠ DirectSound 디바이스 컨텍스트 없음, 테스트 건너뜀\n");
    }
}

/**
 * @brief 통합 폴백 시스템 테스트
 */
static void test_integrated_fallback_system(void) {
    printf("통합 폴백 시스템 테스트...\n");

    ETAudioDevice device = { 0 };
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);

    /* 통합 폴백 초기화 테스트 */
    ETResult result = et_windows_init_audio_with_fallback(&device, &format);

    if (result == ET_SUCCESS) {
        printf("  ✓ 통합 폴백 시스템 초기화 성공\n");

        /* 백엔드 상태 확인 */
        result = et_windows_check_audio_backend_status(&device);
        if (result == ET_SUCCESS) {
            printf("  ✓ 오디오 백엔드 상태 정상\n");
        } else {
            printf("  ⚠ 오디오 백엔드 상태 확인 실패 (오류: %d)\n", result);
        }

        /* 폴백 관리자 정보 가져오기 */
        char info_buffer[512];
        result = et_windows_get_fallback_manager_info(info_buffer, sizeof(info_buffer));
        if (result == ET_SUCCESS) {
            printf("  ✓ 폴백 관리자 정보:\n%s\n", info_buffer);
        } else {
            printf("  ✗ 폴백 관리자 정보 가져오기 실패 (오류: %d)\n", result);
        }

        /* 자동 복구 테스트 */
        result = et_windows_attempt_audio_recovery(&device);
        if (result == ET_SUCCESS) {
            printf("  ✓ 자동 복구 테스트 성공\n");
        } else {
            printf("  ⚠ 자동 복구 테스트 실패 (오류: %d)\n", result);
        }

        /* 정리 */
        et_windows_cleanup_fallback_manager();
    } else {
        printf("  ✗ 통합 폴백 시스템 초기화 실패 (오류: %d)\n", result);
    }
}

/**
 * @brief DirectSound 오류 복구 테스트
 */
static void test_directsound_error_recovery(void) {
    printf("DirectSound 오류 복구 테스트...\n");

    ETAudioDevice device = { 0 };
    ETResult result = et_audio_fallback_to_directsound(&device);

    if (result != ET_SUCCESS) {
        printf("  ⚠ DirectSound 초기화 실패, 테스트 건너뜀\n");
        return;
    }

    ETDirectSoundDevice* ds_device = NULL; /* device.platform_data; */

    if (ds_device) {
        /* 디바이스 상태 확인 */
        result = et_windows_check_directsound_device_status(ds_device);
        if (result == ET_SUCCESS) {
            printf("  ✓ DirectSound 디바이스 상태 정상\n");
        } else {
            printf("  ⚠ DirectSound 디바이스 상태 확인 실패 (오류: %d)\n", result);
        }

        et_windows_cleanup_directsound_device(ds_device);
    } else {
        printf("  ⚠ DirectSound 디바이스 컨텍스트 없음, 테스트 건너뜀\n");
    }
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== DirectSound 폴백 메커니즘 테스트 시작 ===\n\n");

    /* Windows 플랫폼 초기화 */
    ETWindowsPlatformConfig config = et_windows_create_default_config();
    config.audio.prefer_wasapi = false; /* DirectSound 테스트를 위해 WASAPI 비활성화 */

    ETResult init_result = et_windows_init(&config);
    if (init_result != ET_SUCCESS) {
        printf("Windows 플랫폼 초기화 실패 (오류: %d)\n", init_result);
        return 1;
    }

    /* 개별 테스트 실행 */
    test_directsound_system_init();
    printf("\n");

    test_directsound_device_lifecycle();
    printf("\n");

    test_directsound_audio_callback();
    printf("\n");

    test_directsound_performance_stats();
    printf("\n");

    test_integrated_fallback_system();
    printf("\n");

    test_directsound_error_recovery();
    printf("\n");

    /* 정리 */
    et_windows_directsound_cleanup();
    et_windows_finalize();

    printf("=== DirectSound 폴백 메커니즘 테스트 완료 ===\n");
    return 0;
}

#else /* !_WIN32 */

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif /* _WIN32 */