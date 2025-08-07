/**
 * @file audio_example.c
 * @brief 플랫폼 추상화 레이어 오디오 I/O 예제
 *
 * 이 예제는 ETAudioInterface를 사용하여 크로스 플랫폼 오디오 I/O를 구현하는 방법을 보여줍니다.
 * 사인파 생성기를 통해 오디오 출력을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>

#include <libetude/platform/factory.h>
#include <libetude/platform/audio.h>
#include <libetude/platform/system.h>
#include <libetude/error.h>

// 전역 변수
static ETAudioInterface* g_audio_interface = NULL;
static ETSystemInterface* g_system_interface = NULL;
static ETAudioDevice* g_audio_device = NULL;
static volatile int g_running = 1;

// 사인파 생성기 상태
typedef struct {
    double phase;
    double frequency;
    double amplitude;
    uint32_t sample_rate;
} SineGenerator;

static SineGenerator g_sine_gen = {0};

/**
 * @brief 신호 핸들러 - Ctrl+C로 프로그램 종료
 */
void signal_handler(int sig) {
    printf("\n프로그램을 종료합니다...\n");
    g_running = 0;
}

/**
 * @brief 오디오 콜백 함수 - 사인파 생성
 */
void audio_callback(void* output_buffer, int frame_count, void* user_data) {
    SineGenerator* gen = (SineGenerator*)user_data;
    float* buffer = (float*)output_buffer;

    for (int i = 0; i < frame_count * 2; i += 2) { // 스테레오
        // 사인파 생성
        float sample = (float)(gen->amplitude * sin(gen->phase));

        // 스테레오 출력 (좌우 동일)
        buffer[i] = sample;     // 좌채널
        buffer[i + 1] = sample; // 우채널

        // 위상 업데이트
        gen->phase += 2.0 * M_PI * gen->frequency / gen->sample_rate;
        if (gen->phase >= 2.0 * M_PI) {
            gen->phase -= 2.0 * M_PI;
        }
    }
}

/**
 * @brief 사용 가능한 오디오 디바이스 목록 출력
 */
void list_audio_devices(void) {
    printf("=== 사용 가능한 오디오 디바이스 ===\n");

    ETAudioDeviceInfo devices[16];
    int device_count = 16;

    ETResult result = g_audio_interface->enumerate_devices(
        ET_AUDIO_DEVICE_OUTPUT, devices, &device_count);

    if (result != ET_SUCCESS) {
        printf("디바이스 열거 실패: %d\n", result);
        return;
    }

    printf("출력 디바이스 (%d개):\n", device_count);
    for (int i = 0; i < device_count; i++) {
        printf("  %d: %s%s\n", i, devices[i].name,
               devices[i].is_default ? " (기본)" : "");
        printf("     ID: %s\n", devices[i].id);
        printf("     최대 채널: %u\n", devices[i].max_channels);
        printf("     지원 샘플 레이트: ");
        for (int j = 0; j < devices[i].rate_count; j++) {
            printf("%u ", devices[i].supported_rates[j]);
        }
        printf("\n\n");
    }
}

/**
 * @brief 오디오 디바이스 상태 모니터링
 */
void monitor_audio_device(void) {
    if (!g_audio_device) return;

    ETAudioState state = g_audio_interface->get_state(g_audio_device);
    uint32_t latency = g_audio_interface->get_latency(g_audio_device);

    const char* state_names[] = {
        "정지됨", "실행중", "일시정지", "오류"
    };

    printf("\r상태: %s, 지연시간: %u ms",
           state_names[state], latency);
    fflush(stdout);
}

/**
 * @brief 대화형 주파수 조정
 */
void interactive_frequency_control(void) {
    printf("\n=== 대화형 주파수 조정 ===\n");
    printf("명령어:\n");
    printf("  1-9: 주파수 설정 (100Hz * 숫자)\n");
    printf("  +/-: 주파수 증가/감소\n");
    printf("  v: 볼륨 조정\n");
    printf("  s: 상태 정보 출력\n");
    printf("  q: 종료\n\n");

    char input;
    while (g_running && (input = getchar()) != 'q') {
        switch (input) {
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
                g_sine_gen.frequency = (input - '0') * 100.0;
                printf("주파수: %.1f Hz\n", g_sine_gen.frequency);
                break;

            case '+':
                g_sine_gen.frequency += 50.0;
                if (g_sine_gen.frequency > 2000.0) g_sine_gen.frequency = 2000.0;
                printf("주파수: %.1f Hz\n", g_sine_gen.frequency);
                break;

            case '-':
                g_sine_gen.frequency -= 50.0;
                if (g_sine_gen.frequency < 50.0) g_sine_gen.frequency = 50.0;
                printf("주파수: %.1f Hz\n", g_sine_gen.frequency);
                break;

            case 'v':
                printf("현재 볼륨: %.2f\n", g_sine_gen.amplitude);
                printf("새 볼륨 입력 (0.0-1.0): ");
                double new_volume;
                if (scanf("%lf", &new_volume) == 1) {
                    if (new_volume >= 0.0 && new_volume <= 1.0) {
                        g_sine_gen.amplitude = new_volume;
                        printf("볼륨 설정: %.2f\n", g_sine_gen.amplitude);
                    } else {
                        printf("잘못된 볼륨 값입니다.\n");
                    }
                }
                break;

            case 's':
                monitor_audio_device();
                printf("\n현재 설정:\n");
                printf("  주파수: %.1f Hz\n", g_sine_gen.frequency);
                printf("  볼륨: %.2f\n", g_sine_gen.amplitude);
                printf("  샘플 레이트: %u Hz\n", g_sine_gen.sample_rate);
                break;

            case '\n':
                // 개행 문자 무시
                break;

            default:
                printf("알 수 없는 명령어: %c\n", input);
                break;
        }
    }
}

/**
 * @brief 오디오 성능 테스트
 */
void audio_performance_test(void) {
    printf("\n=== 오디오 성능 테스트 ===\n");

    uint64_t start_time, end_time;
    const int test_duration = 5; // 5초
    int callback_count = 0;

    // 콜백 카운터를 위한 사용자 데이터 구조체
    typedef struct {
        SineGenerator* gen;
        int* counter;
    } CallbackData;

    CallbackData callback_data = {&g_sine_gen, &callback_count};

    // 성능 측정용 콜백
    void performance_callback(void* output_buffer, int frame_count, void* user_data) {
        CallbackData* data = (CallbackData*)user_data;
        audio_callback(output_buffer, frame_count, data->gen);
        (*data->counter)++;
    }

    // 콜백 변경
    g_audio_interface->set_callback(g_audio_device, performance_callback, &callback_data);

    g_system_interface->get_high_resolution_time(&start_time);

    printf("성능 테스트 시작 (%d초)...\n", test_duration);

    // 테스트 실행
    for (int i = 0; i < test_duration; i++) {
        g_system_interface->sleep(1000); // 1초 대기
        printf(".");
        fflush(stdout);
    }

    g_system_interface->get_high_resolution_time(&end_time);

    // 원래 콜백으로 복원
    g_audio_interface->set_callback(g_audio_device, audio_callback, &g_sine_gen);

    uint64_t elapsed_ns = end_time - start_time;
    double elapsed_sec = elapsed_ns / 1000000000.0;
    double callbacks_per_sec = callback_count / elapsed_sec;

    printf("\n\n성능 테스트 결과:\n");
    printf("  테스트 시간: %.2f초\n", elapsed_sec);
    printf("  총 콜백 수: %d\n", callback_count);
    printf("  초당 콜백: %.1f\n", callbacks_per_sec);
    printf("  평균 콜백 간격: %.2f ms\n", 1000.0 / callbacks_per_sec);
}

int main(void) {
    printf("=== LibEtude 플랫폼 추상화 레이어 오디오 예제 ===\n\n");

    // 신호 핸들러 설정
    signal(SIGINT, signal_handler);

    // 플랫폼 초기화
    ETResult result = et_platform_initialize();
    if (result != ET_SUCCESS) {
        printf("플랫폼 초기화 실패: %d\n", result);
        return 1;
    }

    // 인터페이스 획득
    g_audio_interface = et_platform_get_audio_interface();
    g_system_interface = et_platform_get_system_interface();

    if (!g_audio_interface || !g_system_interface) {
        printf("인터페이스 획득 실패\n");
        et_platform_cleanup();
        return 1;
    }

    // 사용 가능한 디바이스 목록 출력
    list_audio_devices();

    // 오디오 포맷 설정
    ETAudioFormat format = {
        .sample_rate = 44100,
        .channels = 2,
        .bits_per_sample = 32,
        .format = ET_AUDIO_FORMAT_FLOAT32
    };

    // 사인파 생성기 초기화
    g_sine_gen.phase = 0.0;
    g_sine_gen.frequency = 440.0; // A4 음
    g_sine_gen.amplitude = 0.3;   // 30% 볼륨
    g_sine_gen.sample_rate = format.sample_rate;

    // 오디오 디바이스 열기
    printf("오디오 디바이스를 여는 중...\n");
    result = g_audio_interface->open_output_device(NULL, &format, &g_audio_device);
    if (result != ET_SUCCESS) {
        printf("오디오 디바이스 열기 실패: %d\n", result);
        et_platform_cleanup();
        return 1;
    }

    printf("오디오 디바이스 열기 성공!\n");

    // 콜백 설정
    result = g_audio_interface->set_callback(g_audio_device, audio_callback, &g_sine_gen);
    if (result != ET_SUCCESS) {
        printf("콜백 설정 실패: %d\n", result);
        g_audio_interface->close_device(g_audio_device);
        et_platform_cleanup();
        return 1;
    }

    // 오디오 스트림 시작
    printf("오디오 스트림 시작...\n");
    result = g_audio_interface->start_stream(g_audio_device);
    if (result != ET_SUCCESS) {
        printf("스트림 시작 실패: %d\n", result);
        g_audio_interface->close_device(g_audio_device);
        et_platform_cleanup();
        return 1;
    }

    printf("오디오 재생 시작! (%.1f Hz 사인파)\n", g_sine_gen.frequency);

    // 성능 테스트 실행
    audio_performance_test();

    // 대화형 제어
    interactive_frequency_control();

    // 정리
    printf("\n오디오 스트림 정지 중...\n");
    g_audio_interface->stop_stream(g_audio_device);
    g_audio_interface->close_device(g_audio_device);

    printf("플랫폼 정리 중...\n");
    et_platform_cleanup();

    printf("예제 완료!\n");
    return 0;
}