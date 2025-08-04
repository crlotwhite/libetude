/**
 * @file test_windows_audio_integration.c
 * @brief Windows 오디오 시스템 통합 테스트
 * @author LibEtude Project
 * @version 1.0.0
 *
 * WASAPI 및 DirectSound 기능 테스트 및 폴백 메커니즘 검증
 * 다양한 오디오 디바이스 환경에서의 호환성 테스트
 * Requirements: 2.1, 2.2, 2.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#ifdef _WIN32
#include "libetude/platform/windows.h"
#include "libetude/audio_io.h"
#include "libetude/error.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 테스트 결과 구조체 */
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
} TestResults;

static TestResults g_test_results = { 0 };

/* 테스트 매크로 */
#define TEST_START(name) \
    do { \
        printf("테스트 시작: %s\n", name); \
        g_test_results.total_tests++; \
    } while(0)

#define TEST_PASS(name) \
    do { \
        printf("  ✓ %s 통과\n", name); \
        g_test_results.passed_tests++; \
    } while(0)

#define TEST_FAIL(name, reason) \
    do { \
        printf("  ✗ %s 실패: %s\n", name, reason); \
        g_test_results.failed_tests++; \
    } while(0)

#define TEST_SKIP(name, reason) \
    do { \
        printf("  ⚠ %s 건너뜀: %s\n", name, reason); \
        g_test_results.skipped_tests++; \
    } while(0)

/* 테스트용 오디오 콜백 */
typedef struct {
    float frequency;
    float phase;
    float sample_rate;
    float amplitude;
    int callback_count;
} TestAudioGenerator;

static void test_audio_callback(float* buffer, uint32_t frame_count, void* user_data) {
    TestAudioGenerator* generator = (TestAudioGenerator*)user_data;

    generator->callback_count++;

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
 * @brief WASAPI 디바이스 열거 및 기본 디바이스 선택 테스트
 * Requirements: 2.1, 2.3
 */
static void test_wasapi_device_enumeration_and_selection(void) {
    TEST_START("WASAPI 디바이스 열거 및 기본 디바이스 선택");

    ETWindowsAudioDevice* devices = NULL;
    uint32_t device_count = 0;

    /* 디바이스 열거 */
    ETResult result = et_windows_enumerate_audio_devices(&devices, &device_count);

    if (result != ET_SUCCESS) {
        TEST_FAIL("디바이스 열거", et_error_string(result));
        return;
    }

    if (device_count == 0) {
        TEST_SKIP("디바이스 선택", "사용 가능한 오디오 디바이스 없음");
        et_windows_free_audio_devices(devices);
        return;
    }

    TEST_PASS("디바이스 열거");
    printf("    발견된 디바이스: %u개\n", device_count);

    /* 기본 디바이스 찾기 */
    bool has_default = false;
    int default_device_index = -1;

    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i].is_default) {
            has_default = true;
            default_device_index = i;
            break;
        }
    }

    if (has_default) {
        TEST_PASS("기본 디바이스 선택");
        printf("    기본 디바이스: %ls\n", devices[default_device_index].friendly_name);
        printf("    샘플레이트: %u Hz\n", devices[default_device_index].sample_rate);
        printf("    채널: %u\n", devices[default_device_index].channels);
    } else {
        TEST_FAIL("기본 디바이스 선택", "기본 디바이스를 찾을 수 없음");
    }

    /* 디바이스 호환성 검사 */
    int compatible_devices = 0;
    for (uint32_t i = 0; i < device_count; i++) {
        if (devices[i].sample_rate >= 44100 && devices[i].channels >= 2) {
            compatible_devices++;
        }
    }

    if (compatible_devices > 0) {
        TEST_PASS("디바이스 호환성 검사");
        printf("    호환 가능한 디바이스: %d개\n", compatible_devices);
    } else {
        TEST_FAIL("디바이스 호환성 검사", "호환 가능한 디바이스 없음");
    }

    et_windows_free_audio_devices(devices);
}

/**
 * @brief WASAPI 초기화 및 포맷 협상 테스트
 * Requirements: 2.1
 */
static void test_wasapi_initialization_and_format_negotiation(void) {
    TEST_START("WASAPI 초기화 및 포맷 협상");

    ETWASAPIContext context;
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);

    /* 기본 디바이스로 초기화 */
    ETResult result = et_windows_init_wasapi_device(NULL, &format, &context);

    if (result == ET_SUCCESS) {
        TEST_PASS("WASAPI 초기화");

        /* 포맷 검증 */
        if (context.format.sample_rate > 0 && context.format.num_channels > 0) {
            TEST_PASS("포맷 협상");
            printf("    협상된 포맷: %u Hz, %u 채널, %u 프레임 버퍼\n",
                   context.format.sample_rate, context.format.num_channels, context.format.buffer_size);
        } else {
            TEST_FAIL("포맷 협상", "잘못된 오디오 포맷");
        }

        /* 독점 모드 지원 확인 */
        bool supports_exclusive = et_windows_wasapi_supports_exclusive_mode(&context);
        if (supports_exclusive) {
            TEST_PASS("독점 모드 지원 확인");
        } else {
            printf("    ⚠ 독점 모드 미지원 (공유 모드만 사용 가능)\n");
        }

        et_windows_cleanup_wasapi_context(&context);
    } else {
        TEST_FAIL("WASAPI 초기화", et_error_string(result));
    }
}

/**
 * @brief WASAPI 오디오 렌더링 및 세션 관리 테스트
 * Requirements: 2.1, 2.2
 */
static void test_wasapi_audio_rendering_and_session_management(void) {
    TEST_START("WASAPI 오디오 렌더링 및 세션 관리");

    ETAudioDevice device;
    ETResult result = et_audio_init_wasapi_with_fallback(&device);

    if (result != ET_SUCCESS) {
        TEST_FAIL("오디오 디바이스 초기화", et_error_string(result));
        return;
    }

    TEST_PASS("오디오 디바이스 초기화");

    /* 테스트 오디오 생성기 설정 */
    TestAudioGenerator generator = {
        .frequency = 440.0f,    /* A4 음 */
        .phase = 0.0f,
        .sample_rate = 44100.0f,
        .amplitude = 0.1f,      /* 낮은 볼륨 */
        .callback_count = 0
    };

    /* 오디오 콜백 설정 */
    result = et_audio_set_callback(&device, test_audio_callback, &generator);
    if (result == ET_SUCCESS) {
        TEST_PASS("오디오 콜백 설정");
    } else {
        TEST_FAIL("오디오 콜백 설정", et_error_string(result));
        et_audio_cleanup(&device);
        return;
    }

    /* 오디오 스트림 시작 */
    result = et_audio_start(&device);
    if (result == ET_SUCCESS) {
        TEST_PASS("오디오 스트림 시작");

        /* 2초간 재생하여 콜백 호출 확인 */
        Sleep(2000);

        if (generator.callback_count > 0) {
            TEST_PASS("오디오 콜백 호출");
            printf("    콜백 호출 횟수: %d\n", generator.callback_count);
        } else {
            TEST_FAIL("오디오 콜백 호출", "콜백이 호출되지 않음");
        }

        /* 오디오 스트림 정지 */
        result = et_audio_stop(&device);
        if (result == ET_SUCCESS) {
            TEST_PASS("오디오 스트림 정지");
        } else {
            TEST_FAIL("오디오 스트림 정지", et_error_string(result));
        }
    } else {
        TEST_FAIL("오디오 스트림 시작", et_error_string(result));
    }

    /* 볼륨 제어 테스트 */
    float test_volumes[] = { 0.0f, 0.5f, 1.0f };
    for (int i = 0; i < 3; i++) {
        result = et_audio_set_volume(&device, test_volumes[i]);
        if (result == ET_SUCCESS) {
            float current_volume;
            result = et_audio_get_volume(&device, &current_volume);
            if (result == ET_SUCCESS && fabs(current_volume - test_volumes[i]) < 0.01f) {
                printf("    ✓ 볼륨 설정/확인: %.1f%%\n", test_volumes[i] * 100.0f);
            }
        }
    }

    et_audio_cleanup(&device);
}

/**
 * @brief DirectSound 폴백 메커니즘 테스트
 * Requirements: 2.1
 */
static void test_directsound_fallback_mechanism(void) {
    TEST_START("DirectSound 폴백 메커니즘");

    /* WASAPI 실패 시뮬레이션을 위해 잘못된 설정 사용 */
    ETAudioDevice device;
    ETAudioFormat invalid_format = et_audio_format_create(192000, 8, 64); /* 극단적인 설정 */

    /* 통합 폴백 시스템 테스트 */
    ETResult result = et_windows_init_audio_with_fallback(&device, &invalid_format);

    if (result == ET_SUCCESS) {
        TEST_PASS("폴백 시스템 초기화");

        /* 현재 사용 중인 백엔드 확인 */
        ETAudioBackendType backend_type;
        result = et_windows_get_current_audio_backend(&device, &backend_type);

        if (result == ET_SUCCESS) {
            const char* backend_name = (backend_type == ET_AUDIO_BACKEND_WASAPI) ? "WASAPI" :
                                     (backend_type == ET_AUDIO_BACKEND_DIRECTSOUND) ? "DirectSound" : "알 수 없음";
            printf("    현재 백엔드: %s\n", backend_name);

            if (backend_type == ET_AUDIO_BACKEND_DIRECTSOUND) {
                TEST_PASS("DirectSound 폴백 성공");
            } else {
                printf("    ⚠ WASAPI가 성공했으므로 폴백이 발생하지 않음\n");
            }
        }

        et_audio_cleanup(&device);
    } else {
        TEST_FAIL("폴백 시스템 초기화", et_error_string(result));
    }

    /* DirectSound 직접 테스트 */
    result = et_audio_fallback_to_directsound(&device);
    if (result == ET_SUCCESS) {
        TEST_PASS("DirectSound 직접 초기화");
        et_audio_cleanup(&device);
    } else {
        TEST_FAIL("DirectSound 직접 초기화", et_error_string(result));
    }
}

/**
 * @brief 오디오 디바이스 변경 감지 테스트
 * Requirements: 2.3
 */
static void test_audio_device_change_detection(void) {
    TEST_START("오디오 디바이스 변경 감지");

    ETAudioDevice device;
    ETResult result = et_audio_init_wasapi_with_fallback(&device);

    if (result != ET_SUCCESS) {
        TEST_SKIP("디바이스 변경 감지", "오디오 디바이스 초기화 실패");
        return;
    }

    /* 디바이스 변경 콜백 설정 */
    result = et_windows_set_device_change_callback(&device, NULL, NULL);
    if (result == ET_SUCCESS) {
        TEST_PASS("디바이스 변경 콜백 설정");
    } else {
        TEST_FAIL("디바이스 변경 콜백 설정", et_error_string(result));
    }

    /* 현재 디바이스 상태 확인 */
    bool is_device_available;
    result = et_windows_check_device_availability(&device, &is_device_available);
    if (result == ET_SUCCESS) {
        if (is_device_available) {
            TEST_PASS("디바이스 가용성 확인");
        } else {
            TEST_FAIL("디바이스 가용성 확인", "디바이스를 사용할 수 없음");
        }
    } else {
        TEST_FAIL("디바이스 가용성 확인", et_error_string(result));
    }

    et_audio_cleanup(&device);
}

/**
 * @brief 다양한 오디오 포맷 호환성 테스트
 * Requirements: 2.1, 2.2
 */
static void test_audio_format_compatibility(void) {
    TEST_START("다양한 오디오 포맷 호환성");

    /* 테스트할 오디오 포맷들 */
    struct {
        uint32_t sample_rate;
        uint16_t channels;
        uint32_t buffer_size;
        const char* description;
    } test_formats[] = {
        { 44100, 2, 1024, "CD 품질 (44.1kHz 스테레오)" },
        { 48000, 2, 1024, "DVD 품질 (48kHz 스테레오)" },
        { 96000, 2, 512,  "고해상도 (96kHz 스테레오)" },
        { 44100, 1, 2048, "모노 (44.1kHz)" },
        { 22050, 2, 4096, "저품질 (22.05kHz)" }
    };

    int compatible_formats = 0;
    int total_formats = sizeof(test_formats) / sizeof(test_formats[0]);

    for (int i = 0; i < total_formats; i++) {
        ETAudioFormat format = et_audio_format_create(
            test_formats[i].sample_rate,
            test_formats[i].channels,
            test_formats[i].buffer_size
        );

        ETWASAPIContext context;
        ETResult result = et_windows_init_wasapi_device(NULL, &format, &context);

        if (result == ET_SUCCESS) {
            compatible_formats++;
            printf("    ✓ %s 호환\n", test_formats[i].description);
            et_windows_cleanup_wasapi_context(&context);
        } else {
            printf("    ✗ %s 비호환: %s\n", test_formats[i].description, et_error_string(result));
        }
    }

    if (compatible_formats > 0) {
        TEST_PASS("오디오 포맷 호환성");
        printf("    호환 가능한 포맷: %d/%d\n", compatible_formats, total_formats);
    } else {
        TEST_FAIL("오디오 포맷 호환성", "호환 가능한 포맷 없음");
    }
}

/**
 * @brief 오디오 성능 및 지연 시간 테스트
 * Requirements: 2.2
 */
static void test_audio_performance_and_latency(void) {
    TEST_START("오디오 성능 및 지연 시간");

    ETAudioDevice device;
    ETResult result = et_audio_init_wasapi_with_fallback(&device);

    if (result != ET_SUCCESS) {
        TEST_SKIP("성능 테스트", "오디오 디바이스 초기화 실패");
        return;
    }

    /* 성능 통계 가져오기 */
    ETAudioPerformanceStats stats;
    result = et_audio_get_performance_stats(&device, &stats);

    if (result == ET_SUCCESS) {
        TEST_PASS("성능 통계 수집");
        printf("    평균 콜백 시간: %.2f ms\n", stats.avg_callback_duration_ms);
        printf("    최대 콜백 시간: %.2f ms\n", stats.max_callback_duration_ms);
        printf("    버퍼 언더런 횟수: %u\n", stats.underrun_count);
        printf("    예상 지연 시간: %.2f ms\n", stats.estimated_latency_ms);

        /* 지연 시간 검증 */
        if (stats.estimated_latency_ms < 50.0f) {
            TEST_PASS("저지연 성능");
        } else {
            printf("    ⚠ 높은 지연 시간 (%.2f ms)\n", stats.estimated_latency_ms);
        }

        /* 언더런 검증 */
        if (stats.underrun_count == 0) {
            TEST_PASS("언더런 방지");
        } else {
            printf("    ⚠ 언더런 발생 (%u회)\n", stats.underrun_count);
        }
    } else {
        TEST_FAIL("성능 통계 수집", et_error_string(result));
    }

    et_audio_cleanup(&device);
}

/**
 * @brief 오류 처리 및 복구 테스트
 * Requirements: 2.1, 2.3
 */
static void test_error_handling_and_recovery(void) {
    TEST_START("오류 처리 및 복구");

    /* 잘못된 매개변수 테스트 */
    ETResult result = et_windows_enumerate_audio_devices(NULL, NULL);
    if (result == ET_ERROR_INVALID_PARAMETER) {
        TEST_PASS("잘못된 매개변수 검사");
    } else {
        TEST_FAIL("잘못된 매개변수 검사", "예상된 오류가 발생하지 않음");
    }

    /* 잘못된 디바이스 ID 테스트 */
    ETWASAPIContext context;
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);
    result = et_windows_init_wasapi_device(L"invalid_device_id", &format, &context);

    if (result != ET_SUCCESS) {
        TEST_PASS("잘못된 디바이스 ID 처리");
    } else {
        TEST_FAIL("잘못된 디바이스 ID 처리", "예상된 오류가 발생하지 않음");
        et_windows_cleanup_wasapi_context(&context);
    }

    /* 자동 복구 메커니즘 테스트 */
    ETAudioDevice device;
    result = et_windows_init_audio_with_fallback(&device, &format);

    if (result == ET_SUCCESS) {
        /* 복구 시도 */
        result = et_windows_attempt_audio_recovery(&device);
        if (result == ET_SUCCESS) {
            TEST_PASS("자동 복구 메커니즘");
        } else {
            TEST_FAIL("자동 복구 메커니즘", et_error_string(result));
        }

        et_audio_cleanup(&device);
    }
}

/**
 * @brief 테스트 결과 출력
 */
static void print_test_results(void) {
    printf("\n=== 테스트 결과 요약 ===\n");
    printf("총 테스트: %d\n", g_test_results.total_tests);
    printf("통과: %d\n", g_test_results.passed_tests);
    printf("실패: %d\n", g_test_results.failed_tests);
    printf("건너뜀: %d\n", g_test_results.skipped_tests);

    if (g_test_results.failed_tests == 0) {
        printf("✓ 모든 테스트 통과!\n");
    } else {
        printf("✗ %d개 테스트 실패\n", g_test_results.failed_tests);
    }
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== Windows 오디오 시스템 통합 테스트 ===\n\n");

    /* Windows 플랫폼 초기화 */
    ETWindowsPlatformConfig config = et_windows_create_default_config();
    config.audio.prefer_wasapi = true;
    config.audio.buffer_size_ms = 23; /* ~1024 프레임 @ 44.1kHz */
    config.audio.exclusive_mode = false;

    ETResult result = et_windows_init(&config);
    if (result != ET_SUCCESS) {
        printf("✗ Windows 플랫폼 초기화 실패: %s\n", et_error_string(result));
        return 1;
    }

    printf("✓ Windows 플랫폼 초기화 완료\n\n");

    /* 개별 테스트 실행 */
    test_wasapi_device_enumeration_and_selection();
    printf("\n");

    test_wasapi_initialization_and_format_negotiation();
    printf("\n");

    test_wasapi_audio_rendering_and_session_management();
    printf("\n");

    test_directsound_fallback_mechanism();
    printf("\n");

    test_audio_device_change_detection();
    printf("\n");

    test_audio_format_compatibility();
    printf("\n");

    test_audio_performance_and_latency();
    printf("\n");

    test_error_handling_and_recovery();
    printf("\n");

    /* 테스트 결과 출력 */
    print_test_results();

    /* 정리 */
    et_windows_wasapi_cleanup();
    et_windows_directsound_cleanup();
    et_windows_finalize();

    return (g_test_results.failed_tests == 0) ? 0 : 1;
}

#else /* !_WIN32 */

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif /* _WIN32 */