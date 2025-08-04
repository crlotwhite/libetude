/**
 * @file test_windows_audio_compatibility.c
 * @brief Windows 오디오 디바이스 호환성 테스트
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 다양한 오디오 디바이스 환경에서의 호환성 테스트
 * Requirements: 2.1, 2.2, 2.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"

/* 테스트 통계 */
typedef struct {
    int total_devices_tested;
    int compatible_devices;
    int wasapi_compatible;
    int directsound_compatible;
    int exclusive_mode_supported;
    int shared_mode_supported;
} CompatibilityStats;

static CompatibilityStats g_stats = { 0 };

/**
 * @brief 디바이스 호환성 정보 구조체
 */
typedef struct {
    wchar_t device_name[256];
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    bool wasapi_compatible;
    bool directsound_compatible;
    bool exclusive_mode_support;
    bool shared_mode_support;
    float min_latency_ms;
    float max_latency_ms;
    char error_message[512];
} DeviceCompatibilityInfo;

/**
 * @brief 단일 디바이스 WASAPI 호환성 테스트
 */
static bool test_device_wasapi_compatibility(const ETWindowsAudioDevice* device,
                                           DeviceCompatibilityInfo* info) {
    printf("  WASAPI 호환성 테스트: %ls\n", device->friendly_name);

    wcscpy_s(info->device_name, 256, device->friendly_name);
    info->sample_rate = device->sample_rate;
    info->channels = device->channels;
    info->bits_per_sample = device->bits_per_sample;

    /* 공유 모드 테스트 */
    ETWASAPIContext context;
    ETAudioFormat format = et_audio_format_create(device->sample_rate, device->channels, 1024);

    ETResult result = et_windows_init_wasapi_device(device->device_id, &format, &context);

    if (result == ET_SUCCESS) {
        info->wasapi_compatible = true;
        info->shared_mode_support = true;
        g_stats.wasapi_compatible++;
        g_stats.shared_mode_supported++;

        /* 지연 시간 측정 */
        info->min_latency_ms = (float)format.buffer_size / format.sample_rate * 1000.0f;
        info->max_latency_ms = info->min_latency_ms * 2.0f; /* 더블 버퍼링 고려 */

        printf("    ✓ 공유 모드 호환 (지연시간: %.2f-%.2f ms)\n",
               info->min_latency_ms, info->max_latency_ms);

        /* 독점 모드 테스트 */
        if (device->supports_exclusive) {
            bool exclusive_result = et_windows_test_exclusive_mode(&context);
            if (exclusive_result) {
                info->exclusive_mode_support = true;
                g_stats.exclusive_mode_supported++;
                printf("    ✓ 독점 모드 지원\n");
            } else {
                printf("    ⚠ 독점 모드 미지원\n");
            }
        }

        et_windows_cleanup_wasapi_context(&context);
        return true;
    } else {
        info->wasapi_compatible = false;
        snprintf(info->error_message, sizeof(info->error_message),
                "WASAPI 초기화 실패: %s", et_error_string(result));
        printf("    ✗ WASAPI 비호환: %s\n", et_error_string(result));
        return false;
    }
}

/**
 * @brief 단일 디바이스 DirectSound 호환성 테스트
 */
static bool test_device_directsound_compatibility(const ETWindowsAudioDevice* device,
                                                DeviceCompatibilityInfo* info) {
    printf("  DirectSound 호환성 테스트: %ls\n", device->friendly_name);

    /* DirectSound는 일반적으로 더 관대한 호환성을 가짐 */
    ETAudioDevice audio_device;
    ETResult result = et_audio_fallback_to_directsound(&audio_device);

    if (result == ET_SUCCESS) {
        info->directsound_compatible = true;
        g_stats.directsound_compatible++;

        /* DirectSound 특정 설정 테스트 */
        ETDirectSoundDevice* ds_device = (ETDirectSoundDevice*)audio_device.platform_data;
        if (ds_device) {
            /* 버퍼 크기 및 지연 시간 확인 */
            DWORD buffer_size;
            result = et_windows_get_directsound_buffer_size(ds_device, &buffer_size);
            if (result == ET_SUCCESS) {
                float latency_ms = (float)buffer_size / (device->sample_rate * device->channels * 2) * 1000.0f;
                printf("    ✓ DirectSound 호환 (버퍼: %lu 바이트, 지연시간: ~%.2f ms)\n",
                       buffer_size, latency_ms);
            }
        }

        et_audio_cleanup(&audio_device);
        return true;
    } else {
        info->directsound_compatible = false;
        printf("    ✗ DirectSound 비호환: %s\n", et_error_string(result));
        return false;
    }
}

/**
 * @brief 디바이스별 포맷 호환성 테스트
 */
static void test_device_format_compatibility(const ETWindowsAudioDevice* device) {
    printf("  포맷 호환성 테스트: %ls\n", device->friendly_name);

    /* 테스트할 포맷들 */
    struct {
        uint32_t sample_rate;
        uint16_t channels;
        const char* description;
    } test_formats[] = {
        { 44100, 1, "44.1kHz 모노" },
        { 44100, 2, "44.1kHz 스테레오" },
        { 48000, 2, "48kHz 스테레오" },
        { 96000, 2, "96kHz 스테레오" },
        { 192000, 2, "192kHz 스테레오" }
    };

    int compatible_formats = 0;
    int total_formats = sizeof(test_formats) / sizeof(test_formats[0]);

    for (int i = 0; i < total_formats; i++) {
        ETWASAPIContext context;
        ETAudioFormat format = et_audio_format_create(
            test_formats[i].sample_rate,
            test_formats[i].channels,
            1024
        );

        ETResult result = et_windows_init_wasapi_device(device->device_id, &format, &context);

        if (result == ET_SUCCESS) {
            compatible_formats++;
            printf("    ✓ %s\n", test_formats[i].description);
            et_windows_cleanup_wasapi_context(&context);
        } else {
            printf("    ✗ %s: %s\n", test_formats[i].description, et_error_string(result));
        }
    }

    printf("    호환 포맷: %d/%d\n", compatible_formats, total_formats);
}

/**
 * @brief 디바이스 성능 특성 테스트
 */
static void test_device_performance_characteristics(const ETWindowsAudioDevice* device) {
    printf("  성능 특성 테스트: %ls\n", device->friendly_name);

    ETWASAPIContext context;
    ETAudioFormat format = et_audio_format_create(device->sample_rate, device->channels, 1024);

    ETResult result = et_windows_init_wasapi_device(device->device_id, &format, &context);

    if (result == ET_SUCCESS) {
        /* 최소 버퍼 크기 테스트 */
        uint32_t min_buffer_sizes[] = { 64, 128, 256, 512, 1024, 2048 };
        uint32_t supported_min_buffer = 0;

        for (int i = 0; i < 6; i++) {
            ETAudioFormat test_format = et_audio_format_create(
                device->sample_rate, device->channels, min_buffer_sizes[i]);

            ETWASAPIContext test_context;
            ETResult test_result = et_windows_init_wasapi_device(device->device_id, &test_format, &test_context);

            if (test_result == ET_SUCCESS) {
                supported_min_buffer = min_buffer_sizes[i];
                et_windows_cleanup_wasapi_context(&test_context);
                break;
            }
        }

        if (supported_min_buffer > 0) {
            float min_latency = (float)supported_min_buffer / device->sample_rate * 1000.0f;
            printf("    최소 버퍼 크기: %u 프레임 (%.2f ms)\n", supported_min_buffer, min_latency);
        }

        /* CPU 사용률 추정 */
        double estimated_cpu_usage = (double)format.buffer_size / format.sample_rate * 100.0;
        printf("    예상 CPU 사용률: %.2f%%\n", estimated_cpu_usage);

        et_windows_cleanup_wasapi_context(&context);
    }
}

/**
 * @brief USB 오디오 디바이스 특별 테스트
 */
static void test_usb_audio_device_compatibility(const ETWindowsAudioDevice* device) {
    /* USB 디바이스인지 확인 (디바이스 ID에서 USB 문자열 검색) */
    if (wcsstr(device->device_id, L"USB") != NULL) {
        printf("  USB 오디오 디바이스 감지: %ls\n", device->friendly_name);

        /* USB 특화 테스트 */
        ETWASAPIContext context;
        ETAudioFormat format = et_audio_format_create(device->sample_rate, device->channels, 1024);

        ETResult result = et_windows_init_wasapi_device(device->device_id, &format, &context);

        if (result == ET_SUCCESS) {
            /* USB 디바이스의 일반적인 문제점 테스트 */

            /* 1. 클럭 동기화 테스트 */
            bool clock_sync_stable = et_windows_test_usb_clock_stability(&context);
            if (clock_sync_stable) {
                printf("    ✓ USB 클럭 동기화 안정\n");
            } else {
                printf("    ⚠ USB 클럭 동기화 불안정\n");
            }

            /* 2. 전력 관리 호환성 테스트 */
            bool power_mgmt_compatible = et_windows_test_usb_power_management(&context);
            if (power_mgmt_compatible) {
                printf("    ✓ USB 전력 관리 호환\n");
            } else {
                printf("    ⚠ USB 전력 관리 문제 가능성\n");
            }

            /* 3. 고해상도 오디오 지원 테스트 */
            if (device->sample_rate >= 96000) {
                printf("    ✓ 고해상도 오디오 지원 (%u Hz)\n", device->sample_rate);
            }

            et_windows_cleanup_wasapi_context(&context);
        }
    }
}

/**
 * @brief Bluetooth 오디오 디바이스 특별 테스트
 */
static void test_bluetooth_audio_device_compatibility(const ETWindowsAudioDevice* device) {
    /* Bluetooth 디바이스인지 확인 */
    if (wcsstr(device->device_id, L"BTHENUM") != NULL ||
        wcsstr(device->friendly_name, L"Bluetooth") != NULL) {
        printf("  Bluetooth 오디오 디바이스 감지: %ls\n", device->friendly_name);

        ETWASAPIContext context;
        ETAudioFormat format = et_audio_format_create(device->sample_rate, device->channels, 2048); /* 더 큰 버퍼 */

        ETResult result = et_windows_init_wasapi_device(device->device_id, &format, &context);

        if (result == ET_SUCCESS) {
            /* Bluetooth 특화 테스트 */

            /* 1. 지연 시간 측정 (Bluetooth는 일반적으로 높은 지연 시간) */
            float bt_latency = (float)format.buffer_size / format.sample_rate * 1000.0f;
            printf("    Bluetooth 지연 시간: %.2f ms\n", bt_latency);

            if (bt_latency > 100.0f) {
                printf("    ⚠ 높은 지연 시간 (실시간 애플리케이션에 부적합)\n");
            }

            /* 2. 연결 안정성 테스트 */
            bool connection_stable = et_windows_test_bluetooth_connection_stability(&context);
            if (connection_stable) {
                printf("    ✓ Bluetooth 연결 안정\n");
            } else {
                printf("    ⚠ Bluetooth 연결 불안정 가능성\n");
            }

            /* 3. 코덱 정보 확인 */
            char codec_info[128];
            if (et_windows_get_bluetooth_codec_info(&context, codec_info, sizeof(codec_info)) == ET_SUCCESS) {
                printf("    코덱 정보: %s\n", codec_info);
            }

            et_windows_cleanup_wasapi_context(&context);
        }
    }
}

/**
 * @brief 전체 시스템 호환성 테스트
 */
static void test_system_wide_compatibility(void) {
    printf("=== 전체 시스템 호환성 테스트 ===\n");

    ETWindowsAudioDevice* devices = NULL;
    uint32_t device_count = 0;

    ETResult result = et_windows_enumerate_audio_devices(&devices, &device_count);

    if (result != ET_SUCCESS) {
        printf("✗ 디바이스 열거 실패: %s\n", et_error_string(result));
        return;
    }

    if (device_count == 0) {
        printf("⚠ 사용 가능한 오디오 디바이스가 없습니다.\n");
        return;
    }

    printf("발견된 오디오 디바이스: %u개\n\n", device_count);

    /* 각 디바이스별 호환성 테스트 */
    DeviceCompatibilityInfo* compatibility_info =
        (DeviceCompatibilityInfo*)calloc(device_count, sizeof(DeviceCompatibilityInfo));

    for (uint32_t i = 0; i < device_count; i++) {
        printf("디바이스 %u: %ls\n", i + 1, devices[i].friendly_name);
        printf("  기본 정보: %u Hz, %u 채널, %u 비트\n",
               devices[i].sample_rate, devices[i].channels, devices[i].bits_per_sample);

        g_stats.total_devices_tested++;

        /* WASAPI 호환성 테스트 */
        bool wasapi_compatible = test_device_wasapi_compatibility(&devices[i], &compatibility_info[i]);

        /* DirectSound 호환성 테스트 */
        bool directsound_compatible = test_device_directsound_compatibility(&devices[i], &compatibility_info[i]);

        if (wasapi_compatible || directsound_compatible) {
            g_stats.compatible_devices++;

            /* 추가 테스트 */
            test_device_format_compatibility(&devices[i]);
            test_device_performance_characteristics(&devices[i]);
            test_usb_audio_device_compatibility(&devices[i]);
            test_bluetooth_audio_device_compatibility(&devices[i]);
        }

        printf("\n");
    }

    /* 호환성 보고서 생성 */
    printf("=== 호환성 보고서 ===\n");
    printf("총 테스트된 디바이스: %d\n", g_stats.total_devices_tested);
    printf("호환 가능한 디바이스: %d (%.1f%%)\n",
           g_stats.compatible_devices,
           (float)g_stats.compatible_devices / g_stats.total_devices_tested * 100.0f);
    printf("WASAPI 호환: %d (%.1f%%)\n",
           g_stats.wasapi_compatible,
           (float)g_stats.wasapi_compatible / g_stats.total_devices_tested * 100.0f);
    printf("DirectSound 호환: %d (%.1f%%)\n",
           g_stats.directsound_compatible,
           (float)g_stats.directsound_compatible / g_stats.total_devices_tested * 100.0f);
    printf("독점 모드 지원: %d (%.1f%%)\n",
           g_stats.exclusive_mode_supported,
           (float)g_stats.exclusive_mode_supported / g_stats.total_devices_tested * 100.0f);
    printf("공유 모드 지원: %d (%.1f%%)\n",
           g_stats.shared_mode_supported,
           (float)g_stats.shared_mode_supported / g_stats.total_devices_tested * 100.0f);

    /* 상세 호환성 정보 출력 */
    printf("\n=== 상세 호환성 정보 ===\n");
    for (uint32_t i = 0; i < device_count; i++) {
        printf("디바이스: %ls\n", compatibility_info[i].device_name);
        printf("  WASAPI: %s\n", compatibility_info[i].wasapi_compatible ? "호환" : "비호환");
        printf("  DirectSound: %s\n", compatibility_info[i].directsound_compatible ? "호환" : "비호환");
        printf("  독점 모드: %s\n", compatibility_info[i].exclusive_mode_support ? "지원" : "미지원");
        printf("  공유 모드: %s\n", compatibility_info[i].shared_mode_support ? "지원" : "미지원");
        if (compatibility_info[i].min_latency_ms > 0) {
            printf("  지연 시간: %.2f-%.2f ms\n",
                   compatibility_info[i].min_latency_ms, compatibility_info[i].max_latency_ms);
        }
        if (strlen(compatibility_info[i].error_message) > 0) {
            printf("  오류: %s\n", compatibility_info[i].error_message);
        }
        printf("\n");
    }

    free(compatibility_info);
    et_windows_free_audio_devices(devices);
}

/**
 * @brief 특수 환경 호환성 테스트
 */
static void test_special_environment_compatibility(void) {
    printf("=== 특수 환경 호환성 테스트 ===\n");

    /* 1. 원격 데스크톱 환경 테스트 */
    if (GetSystemMetrics(SM_REMOTESESSION)) {
        printf("원격 데스크톱 환경 감지\n");

        ETAudioDevice device;
        ETResult result = et_windows_init_audio_with_fallback(&device, NULL);

        if (result == ET_SUCCESS) {
            printf("  ✓ 원격 데스크톱에서 오디오 초기화 성공\n");
            et_audio_cleanup(&device);
        } else {
            printf("  ✗ 원격 데스크톱에서 오디오 초기화 실패: %s\n", et_error_string(result));
        }
    }

    /* 2. 가상 머신 환경 테스트 */
    bool is_virtual_machine = et_windows_detect_virtual_machine();
    if (is_virtual_machine) {
        printf("가상 머신 환경 감지\n");

        ETAudioDevice device;
        ETResult result = et_windows_init_audio_with_fallback(&device, NULL);

        if (result == ET_SUCCESS) {
            printf("  ✓ 가상 머신에서 오디오 초기화 성공\n");
            et_audio_cleanup(&device);
        } else {
            printf("  ✗ 가상 머신에서 오디오 초기화 실패: %s\n", et_error_string(result));
        }
    }

    /* 3. 서버 환경 테스트 */
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        if (osvi.wProductType != VER_NT_WORKSTATION) {
            printf("Windows Server 환경 감지\n");

            /* 서버 환경에서는 오디오 서비스가 비활성화될 수 있음 */
            ETAudioDevice device;
            ETResult result = et_windows_init_audio_with_fallback(&device, NULL);

            if (result == ET_SUCCESS) {
                printf("  ✓ 서버 환경에서 오디오 초기화 성공\n");
                et_audio_cleanup(&device);
            } else {
                printf("  ⚠ 서버 환경에서 오디오 초기화 실패 (예상됨): %s\n", et_error_string(result));
            }
        }
    }
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== Windows 오디오 디바이스 호환성 테스트 ===\n\n");

    /* Windows 플랫폼 초기화 */
    ETWindowsPlatformConfig config = et_windows_create_default_config();
    config.audio.prefer_wasapi = true;

    ETResult result = et_windows_init(&config);
    if (result != ET_SUCCESS) {
        printf("✗ Windows 플랫폼 초기화 실패: %s\n", et_error_string(result));
        return 1;
    }

    printf("✓ Windows 플랫폼 초기화 완료\n\n");

    /* 호환성 테스트 실행 */
    test_system_wide_compatibility();
    printf("\n");

    test_special_environment_compatibility();
    printf("\n");

    /* 최종 결과 */
    if (g_stats.compatible_devices > 0) {
        printf("✓ 호환성 테스트 완료: %d/%d 디바이스 호환\n",
               g_stats.compatible_devices, g_stats.total_devices_tested);
    } else {
        printf("✗ 호환성 테스트 실패: 호환 가능한 디바이스 없음\n");
    }

    /* 정리 */
    et_windows_wasapi_cleanup();
    et_windows_directsound_cleanup();
    et_windows_finalize();

    return (g_stats.compatible_devices > 0) ? 0 : 1;
}

#else /* !_WIN32 */

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif /* _WIN32 */