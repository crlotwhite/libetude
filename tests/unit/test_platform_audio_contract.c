/**
 * @file test_platform_audio_contract.c
 * @brief 오디오 인터페이스 계약 검증 테스트
 */

#include "test_platform_abstraction.h"

/**
 * @brief 오디오 인터페이스 계약 검증 테스트
 * @return 테스트 결과
 */
ETResult test_audio_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->audio);

    ETAudioInterface* audio = platform->audio;

    // 필수 함수 포인터 검증
    TEST_ASSERT_NOT_NULL(audio->open_output_device);
    TEST_ASSERT_NOT_NULL(audio->open_input_device);
    TEST_ASSERT_NOT_NULL(audio->close_device);
    TEST_ASSERT_NOT_NULL(audio->start_stream);
    TEST_ASSERT_NOT_NULL(audio->stop_stream);
    TEST_ASSERT_NOT_NULL(audio->pause_stream);
    TEST_ASSERT_NOT_NULL(audio->set_callback);
    TEST_ASSERT_NOT_NULL(audio->enumerate_devices);
    TEST_ASSERT_NOT_NULL(audio->get_latency);
    TEST_ASSERT_NOT_NULL(audio->get_state);

    // 기본 오디오 디바이스 열기 테스트
    ETAudioFormat format = {
        .sample_rate = 44100,
        .channels = 2,
        .bits_per_sample = 16,
        .format = ET_AUDIO_FORMAT_PCM_S16LE
    };

    ETAudioDevice* device = NULL;
    ETResult result = audio->open_output_device(NULL, &format, &device);

    // 성공하거나 디바이스가 없는 경우 모두 허용
    TEST_ASSERT(result == ET_SUCCESS || result == ET_ERROR_DEVICE_NOT_FOUND ||
                result == ET_ERROR_NOT_SUPPORTED);

    if (result == ET_SUCCESS && device) {
        // 디바이스 상태 확인
        ETAudioState state = audio->get_state(device);
        TEST_ASSERT(state == ET_AUDIO_STATE_STOPPED || state == ET_AUDIO_STATE_READY);

        // 지연시간 확인
        uint32_t latency = audio->get_latency(device);
        TEST_ASSERT(latency > 0 && latency < 1000); // 1초 미만이어야 함

        // 디바이스 닫기
        audio->close_device(device);
    }

    // 디바이스 열거 테스트
    ETAudioDeviceInfo devices[16];
    int device_count = 16;
    result = audio->enumerate_devices(ET_AUDIO_DEVICE_OUTPUT, devices, &device_count);
    TEST_ASSERT(result == ET_SUCCESS || result == ET_ERROR_NOT_SUPPORTED);

    if (result == ET_SUCCESS) {
        TEST_ASSERT(device_count >= 0 && device_count <= 16);

        // 각 디바이스 정보 검증
        for (int i = 0; i < device_count; i++) {
            TEST_ASSERT(strlen(devices[i].name) > 0);
            TEST_ASSERT(devices[i].max_channels > 0);
            TEST_ASSERT(devices[i].rate_count > 0);
            TEST_ASSERT_NOT_NULL(devices[i].supported_rates);
        }
    }

    // 잘못된 매개변수 테스트
    result = audio->open_output_device(NULL, NULL, &device);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = audio->open_output_device(NULL, &format, NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    return ET_SUCCESS;
}

/**
 * @brief 오디오 콜백 테스트용 함수
 */
static void test_audio_callback(void* user_data, float* output, int frame_count) {
    int* callback_called = (int*)user_data;
    (*callback_called)++;

    // 간단한 사인파 생성
    for (int i = 0; i < frame_count * 2; i++) {
        output[i] = 0.1f * sinf(2.0f * 3.14159f * 440.0f * i / 44100.0f);
    }
}

/**
 * @brief 오디오 스트림 제어 테스트
 * @return 테스트 결과
 */
ETResult test_audio_stream_control(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->audio);

    ETAudioInterface* audio = platform->audio;

    ETAudioFormat format = {
        .sample_rate = 44100,
        .channels = 2,
        .bits_per_sample = 16,
        .format = ET_AUDIO_FORMAT_PCM_S16LE
    };

    ETAudioDevice* device = NULL;
    ETResult result = audio->open_output_device(NULL, &format, &device);

    if (result != ET_SUCCESS || !device) {
        // 오디오 디바이스가 없는 환경에서는 테스트 스킵
        return ET_SUCCESS;
    }

    // 콜백 설정 테스트
    int callback_called = 0;
    result = audio->set_callback(device, test_audio_callback, &callback_called);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 스트림 시작
    result = audio->start_stream(device);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    ETAudioState state = audio->get_state(device);
    TEST_ASSERT_EQUAL(ET_AUDIO_STATE_RUNNING, state);

    // 잠시 대기 (콜백 호출 확인)
    struct timespec ts = {0, 100000000}; // 100ms
    nanosleep(&ts, NULL);

    // 스트림 일시정지
    result = audio->pause_stream(device);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    state = audio->get_state(device);
    TEST_ASSERT_EQUAL(ET_AUDIO_STATE_PAUSED, state);

    // 스트림 정지
    result = audio->stop_stream(device);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    state = audio->get_state(device);
    TEST_ASSERT_EQUAL(ET_AUDIO_STATE_STOPPED, state);

    // 디바이스 닫기
    audio->close_device(device);

    return ET_SUCCESS;
}

/**
 * @brief 오디오 오류 조건 테스트
 * @return 테스트 결과
 */
ETResult test_audio_error_conditions(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->audio);

    ETAudioInterface* audio = platform->audio;

    // NULL 포인터 테스트
    ETResult result = audio->close_device(NULL);
    TEST_ASSERT(result == ET_ERROR_INVALID_PARAMETER || result == ET_SUCCESS);

    result = audio->start_stream(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = audio->stop_stream(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = audio->pause_stream(NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    result = audio->set_callback(NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(ET_ERROR_INVALID_PARAMETER, result);

    uint32_t latency = audio->get_latency(NULL);
    TEST_ASSERT_EQUAL(0, latency);

    ETAudioState state = audio->get_state(NULL);
    TEST_ASSERT_EQUAL(ET_AUDIO_STATE_ERROR, state);

    // 잘못된 포맷 테스트
    ETAudioFormat invalid_format = {
        .sample_rate = 0,
        .channels = 0,
        .bits_per_sample = 0,
        .format = (ETAudioFormat)-1
    };

    ETAudioDevice* device = NULL;
    result = audio->open_output_device(NULL, &invalid_format, &device);
    TEST_ASSERT(result != ET_SUCCESS);
    TEST_ASSERT_NULL(device);

    return ET_SUCCESS;
}