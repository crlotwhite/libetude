/**
 * @file test_wasapi_device_enumeration.c
 * @brief WASAPI 디바이스 열거 기능 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include "libetude/platform/windows.h"
#include "libetude/error.h"

/**
 * @brief WASAPI 디바이스 열거 기본 테스트
 */
static void test_wasapi_device_enumeration_basic(void) {
    printf("WASAPI 디바이스 열거 기본 테스트 시작...\n");

    ETWindowsAudioDevice* devices = NULL;
    uint32_t device_count = 0;

    /* 디바이스 열거 */
    ETResult result = et_windows_enumerate_audio_devices(&devices, &device_count);

    if (result == ET_SUCCESS) {
        printf("✓ 디바이스 열거 성공: %u개 디바이스 발견\n", device_count);

        /* 각 디바이스 정보 출력 */
        for (uint32_t i = 0; i < device_count; i++) {
            printf("  디바이스 %u:\n", i + 1);
            printf("    이름: %ls\n", devices[i].friendly_name);
            printf("    샘플레이트: %u Hz\n", devices[i].sample_rate);
            printf("    채널: %u\n", devices[i].channels);
            printf("    비트깊이: %u\n", devices[i].bits_per_sample);
            printf("    기본 디바이스: %s\n", devices[i].is_default ? "예" : "아니오");
            printf("    독점 모드 지원: %s\n", devices[i].supports_exclusive ? "예" : "아니오");
            printf("\n");
        }

        /* 기본 디바이스가 있는지 확인 */
        bool has_default = false;
        for (uint32_t i = 0; i < device_count; i++) {
            if (devices[i].is_default) {
                has_default = true;
                break;
            }
        }

        if (has_default) {
            printf("✓ 기본 디바이스 발견\n");
        } else {
            printf("⚠ 기본 디바이스 없음\n");
        }

        /* 메모리 해제 */
        et_windows_free_audio_devices(devices);

    } else {
        printf("✗ 디바이스 열거 실패: %s\n", et_error_string(result));

        const ETError* error = et_get_last_error();
        if (error) {
            printf("  오류 메시지: %s\n", error->message);
        }
    }

    printf("WASAPI 디바이스 열거 기본 테스트 완료\n\n");
}

/**
 * @brief WASAPI 컨텍스트 초기화 테스트
 */
static void test_wasapi_context_initialization(void) {
    printf("WASAPI 컨텍스트 초기화 테스트 시작...\n");

    /* 먼저 디바이스 목록 가져오기 */
    ETWindowsAudioDevice* devices = NULL;
    uint32_t device_count = 0;

    ETResult result = et_windows_enumerate_audio_devices(&devices, &device_count);
    if (result != ET_SUCCESS || device_count == 0) {
        printf("✗ 테스트용 디바이스를 찾을 수 없음\n");
        return;
    }

    /* 첫 번째 디바이스로 테스트 */
    ETWASAPIContext context;
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);

    result = et_windows_init_wasapi_device(devices[0].device_id, &format, &context);

    if (result == ET_SUCCESS) {
        printf("✓ WASAPI 컨텍스트 초기화 성공\n");
        printf("  디바이스: %ls\n", devices[0].friendly_name);
        printf("  포맷: %u Hz, %u 채널\n", format.sample_rate, format.num_channels);

        /* 컨텍스트 정리 */
        et_windows_cleanup_wasapi_context(&context);
        printf("✓ WASAPI 컨텍스트 정리 완료\n");

    } else {
        printf("✗ WASAPI 컨텍스트 초기화 실패: %s\n", et_error_string(result));

        const ETError* error = et_get_last_error();
        if (error) {
            printf("  오류 메시지: %s\n", error->message);
        }
    }

    /* 메모리 해제 */
    et_windows_free_audio_devices(devices);

    printf("WASAPI 컨텍스트 초기화 테스트 완료\n\n");
}

/**
 * @brief 잘못된 매개변수 테스트
 */
static void test_invalid_parameters(void) {
    printf("잘못된 매개변수 테스트 시작...\n");

    /* NULL 포인터 테스트 */
    ETResult result = et_windows_enumerate_audio_devices(NULL, NULL);
    if (result == ET_ERROR_INVALID_PARAMETER) {
        printf("✓ NULL 포인터 검사 통과\n");
    } else {
        printf("✗ NULL 포인터 검사 실패\n");
    }

    /* 잘못된 디바이스 ID 테스트 */
    ETWASAPIContext context;
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);

    result = et_windows_init_wasapi_device(L"invalid_device_id", &format, &context);
    if (result != ET_SUCCESS) {
        printf("✓ 잘못된 디바이스 ID 검사 통과\n");
    } else {
        printf("✗ 잘못된 디바이스 ID 검사 실패\n");
        et_windows_cleanup_wasapi_context(&context);
    }

    printf("잘못된 매개변수 테스트 완료\n\n");
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== WASAPI 디바이스 열거 테스트 시작 ===\n\n");

    /* Windows 플랫폼 초기화 */
    ETWindowsPlatformConfig config = et_windows_create_default_config();
    config.audio.prefer_wasapi = true;

    ETResult result = et_windows_init(&config);
    if (result != ET_SUCCESS) {
        printf("✗ Windows 플랫폼 초기화 실패: %s\n", et_error_string(result));
        return 1;
    }

    printf("✓ Windows 플랫폼 초기화 완료\n\n");

    /* 테스트 실행 */
    test_wasapi_device_enumeration_basic();
    test_wasapi_context_initialization();
    test_invalid_parameters();

    /* 정리 */
    et_windows_wasapi_cleanup();
    et_windows_finalize();

    printf("=== WASAPI 디바이스 열거 테스트 완료 ===\n");
    return 0;
}

#else /* !_WIN32 */

int main(void) {
    printf("이 테스트는 Windows에서만 실행됩니다.\n");
    return 0;
}

#endif /* _WIN32 */