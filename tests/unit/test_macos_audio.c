/**
 * @file test_macos_audio.c
 * @brief macOS CoreAudio 구현체 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "libetude/platform/audio.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"

#ifdef __APPLE__

// macOS 오디오 인터페이스 가져오기 함수 선언
extern ETAudioInterface* et_create_macos_audio_interface(void);
extern void et_destroy_macos_audio_interface(ETAudioInterface* interface);

// 테스트용 콜백 함수
static int test_audio_callback(float* buffer, int num_frames, void* user_data) {
    // 간단한 사인파 생성 (테스트용)
    static float phase = 0.0f;
    float frequency = 440.0f; // A4 음
    float sample_rate = 44100.0f;

    for (int i = 0; i < num_frames; i++) {
        float sample = 0.1f * sinf(2.0f * M_PI * frequency * phase / sample_rate);
        buffer[i * 2] = sample;     // 왼쪽 채널
        buffer[i * 2 + 1] = sample; // 오른쪽 채널
        phase += 1.0f;
        if (phase >= sample_rate) phase -= sample_rate;
    }

    return 0; // 계속 처리
}

/**
 * @brief macOS 오디오 인터페이스 기본 테스트
 */
void test_macos_audio_interface_basic(void) {
    printf("macOS 오디오 인터페이스 기본 테스트 시작...\n");

    ETAudioInterface* interface = et_create_macos_audio_interface();
    assert(interface != NULL);

    // 인터페이스 함수들이 모두 설정되어 있는지 확인
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

    et_destroy_macos_audio_interface(interface);

    printf("✓ macOS 오디오 인터페이스 기본 테스트 통과\n");
}

/**
 * @brief macOS 오디오 디바이스 열거 테스트
 */
void test_macos_audio_device_enumeration(void) {
    printf("macOS 오디오 디바이스 열거 테스트 시작...\n");

    ETAudioInterface* interface = et_create_macos_audio_interface();
    assert(interface != NULL);

    // 출력 디바이스 열거
    ETAudioDeviceInfo output_devices[10];
    int output_count = 10;

    ETResult result = interface->enumerate_devices(ET_AUDIO_DEVICE_OUTPUT, output_devices, &output_count);
    if (result == ET_SUCCESS) {
        printf("✓ 출력 디바이스 %d개 발견\n", output_count);

        for (int i = 0; i < output_count; i++) {
            printf("  - %s (ID: %s, 채널: %d, 기본: %s)\n",
                   output_devices[i].name,
                   output_devices[i].id,
                   output_devices[i].max_channels,
                   output_devices[i].is_default ? "예" : "아니오");
        }
    } else {
        printf("⚠ 출력 디바이스 열거 실패: %d\n", result);
    }

    // 입력 디바이스 열거
    ETAudioDeviceInfo input_devices[10];
    int input_count = 10;

    result = interface->enumerate_devices(ET_AUDIO_DEVICE_INPUT, input_devices, &input_count);
    if (result == ET_SUCCESS) {
        printf("✓ 입력 디바이스 %d개 발견\n", input_count);

        for (int i = 0; i < input_count; i++) {
            printf("  - %s (ID: %s, 채널: %d, 기본: %s)\n",
                   input_devices[i].name,
                   input_devices[i].id,
                   input_devices[i].max_channels,
                   input_devices[i].is_default ? "예" : "아니오");

            // 지원 샘플 레이트 해제
            if (input_devices[i].supported_rates) {
                free(input_devices[i].supported_rates);
            }
        }
    } else {
        printf("⚠ 입력 디바이스 열거 실패: %d\n", result);
    }

    // 지원 샘플 레이트 해제
    for (int i = 0; i < output_count; i++) {
        if (output_devices[i].supported_rates) {
            free(output_devices[i].supported_rates);
        }
    }

    et_destroy_macos_audio_interface(interface);

    printf("✓ macOS 오디오 디바이스 열거 테스트 완료\n");
}

/**
 * @brief macOS 오디오 포맷 지원 테스트
 */
void test_macos_audio_format_support(void) {
    printf("macOS 오디오 포맷 지원 테스트 시작...\n");

    ETAudioInterface* interface = et_create_macos_audio_interface();
    assert(interface != NULL);

    // 일반적인 포맷들 테스트
    ETAudioFormat test_formats[] = {
        {44100, 16, 2, 4, 1024, false},  // 16-bit 스테레오 44.1kHz
        {48000, 16, 2, 4, 1024, false},  // 16-bit 스테레오 48kHz
        {44100, 32, 2, 8, 1024, true},   // 32-bit float 스테레오 44.1kHz
        {48000, 32, 2, 8, 1024, true},   // 32-bit float 스테레오 48kHz
        {44100, 16, 1, 2, 1024, false},  // 16-bit 모노 44.1kHz
        {96000, 24, 2, 6, 1024, false}   // 24-bit 스테레오 96kHz
    };

    int num_formats = sizeof(test_formats) / sizeof(test_formats[0]);
    const char* device_name = NULL; // 기본 디바이스

    printf("기본 디바이스에서 포맷 지원 테스트:\n");

    for (int i = 0; i < num_formats; i++) {
        bool supported = interface->is_format_supported(device_name, &test_formats[i]);
        printf("  - %dHz, %d-bit, %d채널, %s: %s\n",
               test_formats[i].sample_rate,
               test_formats[i].bit_depth,
               test_formats[i].num_channels,
               test_formats[i].is_float ? "float" : "int",
               supported ? "지원됨" : "지원안됨");
    }

    // 지원 포맷 조회
    ETAudioFormat supported_formats[20];
    int format_count = 20;

    ETResult result = interface->get_supported_formats(device_name, supported_formats, &format_count);
    if (result == ET_SUCCESS) {
        printf("✓ 지원되는 포맷 %d개:\n", format_count);
        for (int i = 0; i < format_count && i < 10; i++) { // 처음 10개만 출력
            printf("  - %dHz, %d-bit, %d채널, %s\n",
                   supported_formats[i].sample_rate,
                   supported_formats[i].bit_depth,
                   supported_formats[i].num_channels,
                   supported_formats[i].is_float ? "float" : "int");
        }
        if (format_count > 10) {
            printf("  ... 및 %d개 더\n", format_count - 10);
        }
    } else {
        printf("⚠ 지원 포맷 조회 실패: %d\n", result);
    }

    et_destroy_macos_audio_interface(interface);

    printf("✓ macOS 오디오 포맷 지원 테스트 완료\n");
}

/**
 * @brief macOS 오디오 디바이스 열기/닫기 테스트
 */
void test_macos_audio_device_open_close(void) {
    printf("macOS 오디오 디바이스 열기/닫기 테스트 시작...\n");

    ETAudioInterface* interface = et_create_macos_audio_interface();
    assert(interface != NULL);

    // 기본 오디오 포맷 설정
    ETAudioFormat format = {
        .sample_rate = 44100,
        .bit_depth = 32,
        .num_channels = 2,
        .frame_size = 8,
        .buffer_size = 1024,
        .is_float = true
    };

    // 출력 디바이스 열기
    ETAudioDevice* output_device = NULL;
    ETResult result = interface->open_output_device(NULL, &format, &output_device);

    if (result == ET_SUCCESS) {
        printf("✓ 출력 디바이스 열기 성공\n");

        // 디바이스 상태 확인
        ETAudioState state = interface->get_state(output_device);
        assert(state == ET_AUDIO_STATE_STOPPED);
        printf("✓ 초기 디바이스 상태: 정지됨\n");

        // 지연시간 확인
        uint32_t latency = interface->get_latency(output_device);
        printf("✓ 디바이스 지연시간: %d ms\n", latency);

        // 디바이스 닫기
        interface->close_device(output_device);
        printf("✓ 출력 디바이스 닫기 성공\n");
    } else {
        printf("⚠ 출력 디바이스 열기 실패: %d\n", result);
        const ETError* error = et_get_last_error();
        if (error) {
            printf("  오류 메시지: %s\n", error->message);
        }
    }

    et_destroy_macos_audio_interface(interface);

    printf("✓ macOS 오디오 디바이스 열기/닫기 테스트 완료\n");
}

/**
 * @brief macOS 오디오 스트림 제어 테스트
 */
void test_macos_audio_stream_control(void) {
    printf("macOS 오디오 스트림 제어 테스트 시작...\n");

    ETAudioInterface* interface = et_create_macos_audio_interface();
    assert(interface != NULL);

    // 기본 오디오 포맷 설정
    ETAudioFormat format = {
        .sample_rate = 44100,
        .bit_depth = 32,
        .num_channels = 2,
        .frame_size = 8,
        .buffer_size = 1024,
        .is_float = true
    };

    // 출력 디바이스 열기
    ETAudioDevice* device = NULL;
    ETResult result = interface->open_output_device(NULL, &format, &device);

    if (result != ET_SUCCESS) {
        printf("⚠ 디바이스 열기 실패, 스트림 제어 테스트 건너뜀\n");
        et_destroy_macos_audio_interface(interface);
        return;
    }

    // 콜백 설정
    result = interface->set_callback(device, test_audio_callback, NULL);
    assert(result == ET_SUCCESS);
    printf("✓ 오디오 콜백 설정 완료\n");

    // 스트림 시작
    result = interface->start_stream(device);
    if (result == ET_SUCCESS) {
        printf("✓ 스트림 시작 성공\n");

        // 상태 확인
        ETAudioState state = interface->get_state(device);
        assert(state == ET_AUDIO_STATE_RUNNING);
        printf("✓ 스트림 상태: 실행 중\n");

        // 잠시 재생
        printf("  2초간 테스트 톤 재생...\n");
        sleep(2);

        // 일시정지 테스트
        result = interface->pause_stream(device);
        if (result == ET_SUCCESS) {
            printf("✓ 스트림 일시정지 성공\n");
            state = interface->get_state(device);
            assert(state == ET_AUDIO_STATE_PAUSED);
            sleep(1);

            // 다시 시작
            result = interface->start_stream(device);
            if (result == ET_SUCCESS) {
                printf("✓ 일시정지 후 재시작 성공\n");
                sleep(1);
            }
        } else {
            printf("⚠ 스트림 일시정지 실패: %d\n", result);
        }

        // 스트림 정지
        result = interface->stop_stream(device);
        assert(result == ET_SUCCESS);
        printf("✓ 스트림 정지 성공\n");

        // 상태 확인
        state = interface->get_state(device);
        assert(state == ET_AUDIO_STATE_STOPPED);
        printf("✓ 스트림 상태: 정지됨\n");
    } else {
        printf("⚠ 스트림 시작 실패: %d\n", result);
        const ETError* error = et_get_last_error();
        if (error) {
            printf("  오류 메시지: %s\n", error->message);
        }
    }

    // 디바이스 닫기
    interface->close_device(device);
    et_destroy_macos_audio_interface(interface);

    printf("✓ macOS 오디오 스트림 제어 테스트 완료\n");
}

/**
 * @brief macOS 오디오 오류 처리 테스트
 */
void test_macos_audio_error_handling(void) {
    printf("macOS 오디오 오류 처리 테스트 시작...\n");

    ETAudioInterface* interface = et_create_macos_audio_interface();
    assert(interface != NULL);

    // 잘못된 포맷으로 디바이스 열기 시도
    ETAudioFormat invalid_format = {
        .sample_rate = 0,        // 잘못된 샘플 레이트
        .bit_depth = 32,
        .num_channels = 2,
        .frame_size = 8,
        .buffer_size = 1024,
        .is_float = true
    };

    ETAudioDevice* device = NULL;
    ETResult result = interface->open_output_device(NULL, &invalid_format, &device);

    if (result != ET_SUCCESS) {
        printf("✓ 잘못된 포맷 거부됨 (예상된 동작)\n");
        const ETError* error = et_get_last_error();
        if (error) {
            printf("  오류 메시지: %s\n", error->message);
        }
    } else {
        printf("⚠ 잘못된 포맷이 허용됨 (예상치 못한 동작)\n");
        interface->close_device(device);
    }

    // 존재하지 않는 디바이스 열기 시도
    ETAudioFormat valid_format = {
        .sample_rate = 44100,
        .bit_depth = 32,
        .num_channels = 2,
        .frame_size = 8,
        .buffer_size = 1024,
        .is_float = true
    };

    result = interface->open_output_device("NonExistentDevice12345", &valid_format, &device);

    if (result != ET_SUCCESS) {
        printf("✓ 존재하지 않는 디바이스 거부됨 (예상된 동작)\n");
        const ETError* error = et_get_last_error();
        if (error) {
            printf("  오류 메시지: %s\n", error->message);
        }
    } else {
        printf("⚠ 존재하지 않는 디바이스가 허용됨 (예상치 못한 동작)\n");
        interface->close_device(device);
    }

    // NULL 포인터 테스트
    result = interface->open_output_device(NULL, NULL, &device);
    assert(result != ET_SUCCESS);
    printf("✓ NULL 포맷 포인터 거부됨\n");

    result = interface->open_output_device(NULL, &valid_format, NULL);
    assert(result != ET_SUCCESS);
    printf("✓ NULL 디바이스 포인터 거부됨\n");

    et_destroy_macos_audio_interface(interface);

    printf("✓ macOS 오디오 오류 처리 테스트 완료\n");
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== macOS CoreAudio 구현체 테스트 시작 ===\n\n");

    // 로깅 시스템 초기화
    et_init_logging();
    et_set_log_level(ET_LOG_INFO);

    // 기본 테스트들
    test_macos_audio_interface_basic();
    printf("\n");

    test_macos_audio_device_enumeration();
    printf("\n");

    test_macos_audio_format_support();
    printf("\n");

    test_macos_audio_device_open_close();
    printf("\n");

    test_macos_audio_error_handling();
    printf("\n");

    // 실제 오디오 하드웨어가 있는 경우에만 실행
    printf("실제 오디오 재생 테스트를 실행하시겠습니까? (y/N): ");
    char response;
    if (scanf(" %c", &response) == 1 && (response == 'y' || response == 'Y')) {
        test_macos_audio_stream_control();
        printf("\n");
    } else {
        printf("오디오 재생 테스트 건너뜀\n\n");
    }

    // 로깅 시스템 정리
    et_cleanup_logging();

    printf("=== 모든 테스트 완료 ===\n");
    return 0;
}

#else
// macOS가 아닌 플랫폼에서는 테스트 건너뜀
int main(void) {
    printf("macOS CoreAudio 테스트는 macOS 플랫폼에서만 실행됩니다.\n");
    return 0;
}
#endif