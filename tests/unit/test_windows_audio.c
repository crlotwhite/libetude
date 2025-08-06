/**
 * @file test_windows_audio.c
 * @brief Windows 오디오 구현체 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/audio.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32

// Windows 오디오 인터페이스 함수 선언
extern ETAudioInterface* et_get_windows_audio_interface(void);
extern ETResult et_windows_audio_initialize(void);
extern void et_windows_audio_cleanup(void);

/**
 * @brief 기본 초기화 테스트
 */
static void test_windows_audio_initialization(void) {
    printf("Windows 오디오 초기화 테스트...\n");

    ETResult result = et_windows_audio_initialize();
    assert(result == ET_SUCCESS || result == ET_ERROR_HARDWARE);

    ETAudioInterface* interface = et_get_windows_audio_interface();
    assert(interface != NULL);

    // 인터페이스 함수들이 NULL이 아닌지 확인
    assert(interface->open_output_device != NULL);
    assert(interface->open_input_device != NULL);
    assert(interface->close_device != NULL);
    assert(interface->start_stream != NULL);
    assert(interface->stop_stream != NULL);
    assert(interface->pause_stream != NULL);
    assert(interface->set_callback != NULL);
    assert(interface->enumerate_devices != NULL);
    assert(interface->get_latency != NULL);
    assert(interface->get_state != NULL);
    assert(interface->is_format_supported != NULL);
    assert(interface->get_supported_formats != NULL);

    et_windows_audio_cleanup();
    printf("✓ Windows 오디오 초기화 테스트 통과\n");
}

/**
 * @brief 디바이스 열거 테스트
 */
static void test_device_enumeration(void) {
    printf("디바이스 열거 테스트...\n");

    ETResult result = et_windows_audio_initialize();
    if (result != ET_SUCCESS) {
        printf("⚠ Windows 오디오 초기화 실패, 테스트 건너뜀\n");
        return;
    }

    ETAudioInterface* interface = et_get_windows_audio_interface();
    assert(interface != NULL);

    // 출력 디바이스 열거
    ETAudioDeviceInfo output_devices[10];
    int output_count = 10;
    result = interface->enumerate_devices(ET_AUDIO_DEVICE_OUTPUT, output_devices, &output_count);

    if (result == ET_SUCCESS) {
        printf("  출력 디바이스 %d개 발견\n", output_count);
        for (int i = 0; i < output_count; i++) {
            printf("    [%d] %s (ID: %s)\n", i, output_devices[i].name, output_devices[i].id);
        }
    } else {
        printf("  출력 디바이스 열거 실패: %d\n", result);
    }

    // 입력 디바이스 열거
    ETAudioDeviceInfo input_devices[10];
    int input_count = 10;
    result = interface->enumerate_devices(ET_AUDIO_DEVICE_INPUT, input_devices, &input_count);

    if (result == ET_SUCCESS) {
        printf("  입력 디바이스 %d개 발견\n", input_count);
        for (int i = 0; i < input_count; i++) {
            printf("    [%d] %s (ID: %s)\n", i, input_devices[i].name, input_devices[i].id);
        }
    } else {
        printf("  입력 디바이스 열거 실패: %d\n", result);
    }

    et_windows_audio_cleanup();
    printf("✓ 디바이스 열거 테스트 완료\n");
}

/**
 * @brief 포맷 지원 테스트
 */
static void test_format_support(void) {
    printf("포맷 지원 테스트...\n");

    ETResult result = et_windows_audio_initialize();
    if (result != ET_SUCCESS) {
        printf("⚠ Windows 오디오 초기화 실패, 테스트 건너뜀\n");
        return;
    }

    ETAudioInterface* interface = et_get_windows_audio_interface();
    assert(interface != NULL);

    // 기본 포맷들 테스트
    ETAudioFormat formats[] = {
        {44100, 16, 2, 4, 1024, false},    // CD 품질
        {48000, 16, 2, 4, 1024, false},    // DAT 품질
        {48000, 24, 2, 6, 1024, false},    // 고품질
        {96000, 24, 2, 6, 1024, false},    // 고해상도
        {44100, 32, 2, 8, 1024, true},     // Float32
        {8000, 8, 1, 1, 512, false},       // 낮은 품질 (지원되지 않을 수 있음)
        {192000, 32, 8, 32, 2048, true}    // 매우 높은 품질 (지원되지 않을 수 있음)
    };

    const char* format_names[] = {
        "CD 품질 (44.1kHz, 16bit, 스테레오)",
        "DAT 품질 (48kHz, 16bit, 스테레오)",
        "고품질 (48kHz, 24bit, 스테레오)",
        "고해상도 (96kHz, 24bit, 스테레오)",
        "Float32 (44.1kHz, 32bit, 스테레오)",
        "낮은 품질 (8kHz, 8bit, 모노)",
        "매우 높은 품질 (192kHz, 32bit, 8채널)"
    };

    int format_count = sizeof(formats) / sizeof(formats[0]);

    for (int i = 0; i < format_count; i++) {
        bool supported = interface->is_format_supported("default", &formats[i]);
        printf("  %s: %s\n", format_names[i], supported ? "지원됨" : "지원되지 않음");
    }

    // 지원되는 포맷 목록 조회
    ETAudioFormat supported_formats[20];
    int supported_count = 20;
    result = interface->get_supported_formats("default", supported_formats, &supported_count);

    if (result == ET_SUCCESS) {
        printf("  지원되는 포맷 %d개:\n", supported_count);
        for (int i = 0; i < supported_count; i++) {
            printf("    %dHz, %dbit, %dch, %s\n",
                   supported_formats[i].sample_rate,
                   supported_formats[i].bit_depth,
                   supported_formats[i].num_channels,
                   supported_formats[i].is_float ? "float" : "int");
        }
    }

    et_windows_audio_cleanup();
    printf("✓ 포맷 지원 테스트 완료\n");
}

/**
 * @brief 디바이스 열기/닫기 테스트
 */
static void test_device_open_close(void) {
    printf("디바이스 열기/닫기 테스트...\n");

    ETResult result = et_windows_audio_initialize();
    if (result != ET_SUCCESS) {
        printf("⚠ Windows 오디오 초기화 실패, 테스트 건너뜀\n");
        return;
    }

    ETAudioInterface* interface = et_get_windows_audio_interface();
    assert(interface != NULL);

    // 기본 출력 디바이스 열기
    ETAudioFormat format = {44100, 16, 2, 4, 1024, false};
    ETAudioDevice* device = NULL;

    result = interface->open_output_device("default", &format, &device);
    if (result == ET_SUCCESS) {
        printf("  출력 디바이스 열기 성공\n");

        // 상태 확인
        ETAudioState state = interface->get_state(device);
        assert(state == ET_AUDIO_STATE_STOPPED);
        printf("  초기 상태: 정지됨\n");

        // 지연시간 확인
        uint32_t latency = interface->get_latency(device);
        printf("  지연시간: %u ms\n", latency);

        // 디바이스 닫기
        interface->close_device(device);
        printf("  출력 디바이스 닫기 완료\n");
    } else {
        printf("  출력 디바이스 열기 실패: %d\n", result);
    }

    // 기본 입력 디바이스 열기 (있는 경우)
    device = NULL;
    result = interface->open_input_device("default", &format, &device);
    if (result == ET_SUCCESS) {
        printf("  입력 디바이스 열기 성공\n");

        ETAudioState state = interface->get_state(device);
        assert(state == ET_AUDIO_STATE_STOPPED);

        interface->close_device(device);
        printf("  입력 디바이스 닫기 완료\n");
    } else {
        printf("  입력 디바이스 열기 실패 또는 없음: %d\n", result);
    }

    et_windows_audio_cleanup();
    printf("✓ 디바이스 열기/닫기 테스트 완료\n");
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== Windows 오디오 구현체 테스트 시작 ===\n\n");

    test_windows_audio_initialization();
    printf("\n");

    test_device_enumeration();
    printf("\n");

    test_format_support();
    printf("\n");

    test_device_open_close();
    printf("\n");

    printf("=== 모든 테스트 완료 ===\n");
    return 0;
}

#else
// Windows가 아닌 플랫폼에서는 빈 테스트
int main(void) {
    printf("Windows 플랫폼이 아니므로 테스트를 건너뜁니다.\n");
    return 0;
}
#endif