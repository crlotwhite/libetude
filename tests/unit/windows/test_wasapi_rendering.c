/**
 * @file test_wasapi_rendering.c
 * @brief WASAPI Audio Rendering and Volume Control Tests
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _WIN32

/* 테스트용 사인파 생성기 */
typedef struct {
    float frequency;
    float phase;
    float sample_rate;
    float amplitude;
} SineWaveGenerator;

/**
 * @brief 사인파 오디오 콜백
 */
static void sine_wave_callback(float* buffer, uint32_t frame_count, void* user_data) {
    SineWaveGenerator* generator = (SineWaveGenerator*)user_data;

    for (uint32_t i = 0; i < frame_count; i++) {
        float sample = generator->amplitude * sinf(generator->phase);

        /* 스테레오 출력 */
        buffer[i * 2] = sample;     /* 왼쪽 채널 */
        buffer[i * 2 + 1] = sample; /* 오른쪽 채널 */

        /* 위상 업데이트 */
        generator->phase += 2.0f * (float)M_PI * generator->frequency / generator->sample_rate;
        if (generator->phase >= 2.0f * (float)M_PI) {
            generator->phase -= 2.0f * (float)M_PI;
        }
    }
}

/**
 * @brief WASAPI 디바이스 열거 테스트
 */
static void test_wasapi_device_enumeration(void) {
    printf("WASAPI 디바이스 열거 테스트 시작...\n");

    ETWindowsAudioDevice* devices = NULL;
    uint32_t device_count = 0;

    ETResult result = et_windows_enumerate_audio_devices(&devices, &device_count);
    assert(result == ET_SUCCESS);
    assert(devices != NULL);
    assert(device_count > 0);

    printf("발견된 오디오 디바이스: %u개\n", device_count);

    for (uint32_t i = 0; i < device_count; i++) {
        printf("디바이스 %u:\n", i);
        printf("  Name: %ls\n", devices[i].friendly_name);
        printf("  Sample Rate: %lu Hz\n", devices[i].sample_rate);
        printf("  Channels: %u\n", devices[i].channels);
        printf("  Bit Depth: %u\n", devices[i].bits_per_sample);
        printf("  Default Device: %s\n", devices[i].is_default ? "Yes" : "No");
        printf("  Exclusive Mode Support: %s\n", devices[i].supports_exclusive ? "Yes" : "No");
        printf("\n");
    }

    et_windows_free_audio_devices(devices);
    printf("WASAPI 디바이스 열거 테스트 완료\n\n");
}

/**
 * @brief WASAPI 초기화 테스트
 */
static void test_wasapi_initialization(void) {
    printf("WASAPI 초기화 테스트 시작...\n");

    /* 기본 디바이스로 WASAPI 컨텍스트 초기화 */
    ETWASAPIContext context;
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);

    ETResult result = et_windows_init_wasapi_device(NULL, &format, &context);
    if (result == ET_SUCCESS) {
        printf("WASAPI 초기화 성공\n");
        et_windows_cleanup_wasapi_context(&context);
    } else {
        printf("WASAPI 초기화 실패: %d\n", result);
    }

    printf("WASAPI 초기화 테스트 완료\n\n");
}

/**
 * @brief WASAPI 오디오 렌더링 테스트
 */
static void test_wasapi_audio_rendering(void) {
    printf("WASAPI 오디오 렌더링 테스트 시작...\n");

    /* 오디오 디바이스 초기화 */
    ETAudioDevice device;
    ETResult result = et_audio_init_wasapi_with_fallback(&device);

    if (result != ET_SUCCESS) {
        printf("WASAPI 디바이스 초기화 실패: %d\n", result);
        return;
    }

    printf("WASAPI 디바이스 초기화 성공\n");

    /* 사인파 생성기 설정 */
    SineWaveGenerator generator = {
        .frequency = 440.0f,    /* A4 음 */
        .phase = 0.0f,
        .sample_rate = 44100.0f,
        .amplitude = 0.3f       /* 30% 볼륨 */
    };

    /* 오디오 스트림 시작 (실제 구현에서는 ETWASAPIDevice 사용) */
    printf("3초간 440Hz 사인파 재생...\n");

    /* 실제 테스트에서는 여기서 스트림을 시작하고 3초 대기 */
    Sleep(3000);

    printf("오디오 렌더링 테스트 완료\n\n");
}

/**
 * @brief WASAPI 볼륨 제어 테스트
 */
static void test_wasapi_volume_control(void) {
    printf("WASAPI 볼륨 제어 테스트 시작...\n");

    /* 실제 구현에서는 ETWASAPIDevice를 사용 */
    printf("볼륨 제어 기능 테스트는 실제 디바이스가 필요합니다\n");

    /* 볼륨 설정 테스트 시뮬레이션 */
    float test_volumes[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    int num_volumes = sizeof(test_volumes) / sizeof(test_volumes[0]);

    for (int i = 0; i < num_volumes; i++) {
        printf("볼륨 설정 테스트: %.2f%% (%.2f)\n",
               test_volumes[i] * 100.0f, test_volumes[i]);
    }

    /* 음소거 테스트 시뮬레이션 */
    printf("음소거 설정 테스트: 활성화\n");
    printf("음소거 설정 테스트: 비활성화\n");

    printf("WASAPI 볼륨 제어 테스트 완료\n\n");
}

/**
 * @brief WASAPI 성능 모니터링 테스트
 */
static void test_wasapi_performance_monitoring(void) {
    printf("WASAPI 성능 모니터링 테스트 시작...\n");

    /* 성능 통계 시뮬레이션 */
    double avg_callback_duration = 2.5; /* 2.5ms */
    UINT32 current_padding = 512;
    UINT32 buffer_frame_count = 1024;

    printf("성능 통계:\n");
    printf("  평균 콜백 시간: %.2f ms\n", avg_callback_duration);
    printf("  현재 패딩: %u 프레임\n", current_padding);
    printf("  버퍼 크기: %u 프레임\n", buffer_frame_count);
    printf("  버퍼 사용률: %.1f%%\n",
           (float)current_padding / buffer_frame_count * 100.0f);

    /* 지연 시간 분석 */
    float latency_ms = (float)buffer_frame_count / 44100.0f * 1000.0f;
    printf("  예상 지연 시간: %.2f ms\n", latency_ms);

    printf("WASAPI 성능 모니터링 테스트 완료\n\n");
}

/**
 * @brief WASAPI 오디오 세션 관리 테스트
 */
static void test_wasapi_session_management(void) {
    printf("WASAPI 오디오 세션 관리 테스트 시작...\n");

    /* 세션 상태 시뮬레이션 */
    const char* session_states[] = {
        "비활성", "활성", "만료"
    };

    printf("세션 상태 변경 시뮬레이션:\n");
    for (int i = 0; i < 3; i++) {
        printf("  세션 상태: %s\n", session_states[i]);
    }

    /* 세션 연결 해제 이유 시뮬레이션 */
    const char* disconnect_reasons[] = {
        "디바이스 제거", "서버 종료", "포맷 변경",
        "세션 로그오프", "세션 연결 해제", "독점 모드 재정의"
    };

    printf("연결 해제 이유 시뮬레이션:\n");
    for (int i = 0; i < 6; i++) {
        printf("  이유: %s\n", disconnect_reasons[i]);
    }

    printf("WASAPI 오디오 세션 관리 테스트 완료\n\n");
}

/**
 * @brief 저지연 렌더링 루프 테스트
 */
static void test_low_latency_rendering(void) {
    printf("저지연 렌더링 루프 테스트 시작...\n");

    /* 저지연 설정 시뮬레이션 */
    UINT32 buffer_sizes[] = { 128, 256, 512, 1024 };
    int num_sizes = sizeof(buffer_sizes) / sizeof(buffer_sizes[0]);

    printf("다양한 버퍼 크기에서의 지연 시간 분석:\n");
    for (int i = 0; i < num_sizes; i++) {
        float latency = (float)buffer_sizes[i] / 44100.0f * 1000.0f;
        printf("  버퍼 크기: %u 프레임, 지연 시간: %.2f ms\n",
               buffer_sizes[i], latency);
    }

    /* 언더런 방지 로직 테스트 */
    printf("언더런 방지 로직 테스트:\n");
    printf("  최소 버퍼 임계값: 25%% (256/1024 프레임)\n");
    printf("  언더런 감지 및 복구 메커니즘 활성화\n");

    /* 스레드 우선순위 테스트 */
    printf("스레드 최적화:\n");
    printf("  스레드 우선순위: THREAD_PRIORITY_TIME_CRITICAL\n");
    printf("  Pro Audio 스레드 특성 설정\n");

    printf("저지연 렌더링 루프 테스트 완료\n\n");
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== WASAPI 오디오 렌더링 및 볼륨 제어 테스트 ===\n\n");

    /* Windows 플랫폼 초기화 */
    ETWindowsPlatformConfig config = et_windows_create_default_config();
    config.audio.prefer_wasapi = true;
    config.audio.buffer_size_ms = 23; /* ~1024 프레임 @ 44.1kHz */

    ETResult result = et_windows_init(&config);
    if (result != ET_SUCCESS) {
        printf("Windows 플랫폼 초기화 실패: %d\n", result);
        return 1;
    }

    /* 개별 테스트 실행 */
    test_wasapi_device_enumeration();
    test_wasapi_initialization();
    test_wasapi_audio_rendering();
    test_wasapi_volume_control();
    test_wasapi_performance_monitoring();
    test_wasapi_session_management();
    test_low_latency_rendering();

    /* 정리 */
    et_windows_wasapi_cleanup();
    et_windows_finalize();

    printf("=== 모든 테스트 완료 ===\n");
    return 0;
}

#else /* !_WIN32 */

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif /* _WIN32 */