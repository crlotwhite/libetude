/**
 * @file test_audio_io.c
 * @brief 오디오 I/O 시스템 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "libetude/audio_io.h"
#include "libetude/error.h"
#include "libetude/memory.h"

// 테스트 헬퍼 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return 0; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return 1; \
    } while(0)

// 테스트용 전역 변수
static float* g_test_buffer = NULL;
static int g_test_frames_received = 0;
static bool g_callback_called = false;

/**
 * @brief 테스트용 오디오 콜백 함수
 */
static void test_audio_callback(float* buffer, int num_frames, void* user_data) {
    g_callback_called = true;
    g_test_frames_received = num_frames;

    // 테스트용 사인파 생성
    static float phase = 0.0f;
    float frequency = 440.0f; // A4 음
    float sample_rate = 44100.0f;

    for (int i = 0; i < num_frames; i++) {
        float sample = sinf(phase * 2.0f * M_PI);
        buffer[i] = sample * 0.5f; // 볼륨 조절

        phase += frequency / sample_rate;
        if (phase >= 1.0f) {
            phase -= 1.0f;
        }
    }

    // 사용자 데이터 테스트
    if (user_data) {
        int* counter = (int*)user_data;
        (*counter)++;
    }
}

/**
 * @brief 오디오 포맷 생성 테스트
 */
static int test_audio_format_create(void) {
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);

    TEST_ASSERT(format.sample_rate == 44100, "Sample rate mismatch");
    TEST_ASSERT(format.num_channels == 2, "Channel count mismatch");
    TEST_ASSERT(format.buffer_size == 1024, "Buffer size mismatch");
    TEST_ASSERT(format.bit_depth == 32, "Bit depth should be 32 for float");
    TEST_ASSERT(format.frame_size == 2 * sizeof(float), "Frame size calculation error");

    TEST_PASS();
}

/**
 * @brief 오디오 버퍼 생성/해제 테스트
 */
static int test_audio_buffer_create_destroy(void) {
    ETAudioBuffer* buffer = et_audio_buffer_create(1024, 2);

    TEST_ASSERT(buffer != NULL, "Buffer creation failed");
    TEST_ASSERT(buffer->size == 1024, "Buffer size mismatch");
    TEST_ASSERT(buffer->data != NULL, "Buffer data is NULL");
    TEST_ASSERT(buffer->write_pos == 0, "Initial write position should be 0");
    TEST_ASSERT(buffer->read_pos == 0, "Initial read position should be 0");
    TEST_ASSERT(buffer->available == 0, "Initial available should be 0");
    TEST_ASSERT(buffer->is_full == false, "Initial is_full should be false");

    et_audio_buffer_destroy(buffer);

    // NULL 포인터 테스트
    et_audio_buffer_destroy(NULL); // 크래시하지 않아야 함

    TEST_PASS();
}

/**
 * @brief 오디오 버퍼 쓰기/읽기 테스트
 */
static int test_audio_buffer_write_read(void) {
    ETAudioBuffer* buffer = et_audio_buffer_create(10, 1);
    TEST_ASSERT(buffer != NULL, "Buffer creation failed");

    // 테스트 데이터 준비
    float test_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float read_data[10];

    // 데이터 쓰기 테스트
    uint32_t written = et_audio_buffer_write(buffer, test_data, 5);
    TEST_ASSERT(written == 5, "Write count mismatch");
    TEST_ASSERT(buffer->available == 5, "Available count after write");
    TEST_ASSERT(buffer->write_pos == 5, "Write position after write");

    // 데이터 읽기 테스트
    uint32_t read = et_audio_buffer_read(buffer, read_data, 3);
    TEST_ASSERT(read == 3, "Read count mismatch");
    TEST_ASSERT(buffer->available == 2, "Available count after read");
    TEST_ASSERT(buffer->read_pos == 3, "Read position after read");

    // 읽은 데이터 검증
    TEST_ASSERT(read_data[0] == 1.0f, "Read data[0] mismatch");
    TEST_ASSERT(read_data[1] == 2.0f, "Read data[1] mismatch");
    TEST_ASSERT(read_data[2] == 3.0f, "Read data[2] mismatch");

    et_audio_buffer_destroy(buffer);
    TEST_PASS();
}

/**
 * @brief 오디오 버퍼 링 버퍼 동작 테스트
 */
static int test_audio_buffer_ring_behavior(void) {
    ETAudioBuffer* buffer = et_audio_buffer_create(5, 1);
    TEST_ASSERT(buffer != NULL, "Buffer creation failed");

    float test_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
    float read_data[10];

    // 버퍼를 가득 채우기
    uint32_t written = et_audio_buffer_write(buffer, test_data, 5);
    TEST_ASSERT(written == 5, "Should write all 5 frames");
    TEST_ASSERT(buffer->is_full == true, "Buffer should be full");

    // 가득 찬 버퍼에 더 쓰기 시도
    written = et_audio_buffer_write(buffer, &test_data[5], 2);
    TEST_ASSERT(written == 0, "Should not write to full buffer");

    // 일부 읽기
    uint32_t read = et_audio_buffer_read(buffer, read_data, 2);
    TEST_ASSERT(read == 2, "Should read 2 frames");
    TEST_ASSERT(buffer->is_full == false, "Buffer should not be full after read");

    // 링 버퍼 동작 테스트 (wrap around)
    written = et_audio_buffer_write(buffer, &test_data[5], 2);
    TEST_ASSERT(written == 2, "Should write 2 frames after read");

    et_audio_buffer_destroy(buffer);
    TEST_PASS();
}

/**
 * @brief 오디오 버퍼 리셋 테스트
 */
static int test_audio_buffer_reset(void) {
    ETAudioBuffer* buffer = et_audio_buffer_create(10, 1);
    TEST_ASSERT(buffer != NULL, "Buffer creation failed");

    float test_data[] = {1.0f, 2.0f, 3.0f};

    // 데이터 쓰기
    et_audio_buffer_write(buffer, test_data, 3);
    TEST_ASSERT(buffer->available == 3, "Should have 3 frames available");

    // 리셋
    et_audio_buffer_reset(buffer);
    TEST_ASSERT(buffer->write_pos == 0, "Write position should be 0 after reset");
    TEST_ASSERT(buffer->read_pos == 0, "Read position should be 0 after reset");
    TEST_ASSERT(buffer->available == 0, "Available should be 0 after reset");
    TEST_ASSERT(buffer->is_full == false, "is_full should be false after reset");

    et_audio_buffer_destroy(buffer);
    TEST_PASS();
}

/**
 * @brief 오디오 버퍼 공간/데이터 조회 테스트
 */
static int test_audio_buffer_available_queries(void) {
    ETAudioBuffer* buffer = et_audio_buffer_create(10, 1);
    TEST_ASSERT(buffer != NULL, "Buffer creation failed");

    // 초기 상태
    TEST_ASSERT(et_audio_buffer_available_space(buffer) == 10, "Initial available space");
    TEST_ASSERT(et_audio_buffer_available_data(buffer) == 0, "Initial available data");

    // 데이터 쓰기 후
    float test_data[] = {1.0f, 2.0f, 3.0f};
    et_audio_buffer_write(buffer, test_data, 3);

    TEST_ASSERT(et_audio_buffer_available_space(buffer) == 7, "Available space after write");
    TEST_ASSERT(et_audio_buffer_available_data(buffer) == 3, "Available data after write");

    // 데이터 읽기 후
    float read_data[2];
    et_audio_buffer_read(buffer, read_data, 2);

    TEST_ASSERT(et_audio_buffer_available_space(buffer) == 9, "Available space after read");
    TEST_ASSERT(et_audio_buffer_available_data(buffer) == 1, "Available data after read");

    et_audio_buffer_destroy(buffer);
    TEST_PASS();
}

/**
 * @brief 오디오 디바이스 생성 테스트 (모의 테스트)
 */
static int test_audio_device_creation(void) {
    ETAudioFormat format = et_audio_format_create(44100, 2, 1024);

    // 출력 디바이스 생성 시도 (실제 하드웨어가 없을 수 있으므로 실패해도 OK)
    ETAudioDevice* device = et_audio_open_output_device(NULL, &format);

    if (device != NULL) {
        // 디바이스가 성공적으로 생성된 경우
        TEST_ASSERT(et_audio_get_state(device) == ET_AUDIO_STATE_STOPPED, "Initial state should be stopped");

        // 콜백 설정 테스트
        int callback_counter = 0;
        ETResult result = et_audio_set_callback(device, test_audio_callback, &callback_counter);
        TEST_ASSERT(result == ET_SUCCESS, "Callback setting should succeed");

        et_audio_close_device(device);
    }

    // NULL 포인터 테스트
    et_audio_close_device(NULL); // 크래시하지 않아야 함

    TEST_PASS();
}

/**
 * @brief 오디오 유틸리티 함수 테스트
 */
static int test_audio_utility_functions(void) {
    float test_buffer[] = {-2.0f, -0.5f, 0.0f, 0.5f, 2.0f};
    int num_samples = 5;

    // 클리핑 테스트
    et_audio_clip_buffer(test_buffer, num_samples);
    TEST_ASSERT(test_buffer[0] == -1.0f, "Clipping negative overflow");
    TEST_ASSERT(test_buffer[1] == -0.5f, "Clipping normal negative");
    TEST_ASSERT(test_buffer[2] == 0.0f, "Clipping zero");
    TEST_ASSERT(test_buffer[3] == 0.5f, "Clipping normal positive");
    TEST_ASSERT(test_buffer[4] == 1.0f, "Clipping positive overflow");

    // 볼륨 적용 테스트
    float volume_buffer[] = {-1.0f, 0.0f, 1.0f};
    et_audio_apply_volume(volume_buffer, 3, 0.5f);
    TEST_ASSERT(volume_buffer[0] == -0.5f, "Volume negative");
    TEST_ASSERT(volume_buffer[1] == 0.0f, "Volume zero");
    TEST_ASSERT(volume_buffer[2] == 0.5f, "Volume positive");

    // 믹싱 테스트
    float dest[] = {1.0f, 0.0f, -1.0f};
    float src[] = {0.0f, 1.0f, 1.0f};
    et_audio_mix_buffers(dest, src, 3, 0.5f);
    TEST_ASSERT(dest[0] == 0.5f, "Mix test 1");
    TEST_ASSERT(dest[1] == 0.5f, "Mix test 2");
    TEST_ASSERT(dest[2] == 0.0f, "Mix test 3");

    // 페이드 테스트
    float fade_buffer[] = {1.0f, 1.0f, 1.0f, 1.0f};
    et_audio_fade_buffer(fade_buffer, 4, true); // 페이드 인
    TEST_ASSERT(fade_buffer[0] == 0.0f, "Fade in start");
    TEST_ASSERT(fade_buffer[3] == 1.0f, "Fade in end");

    TEST_PASS();
}

/**
 * @brief 에러 처리 테스트
 */
static int test_audio_error_handling(void) {
    // 잘못된 파라미터로 버퍼 생성
    ETAudioBuffer* buffer = et_audio_buffer_create(0, 1);
    TEST_ASSERT(buffer == NULL, "Should fail with zero size");

    buffer = et_audio_buffer_create(1024, 0);
    TEST_ASSERT(buffer == NULL, "Should fail with zero channels");

    // NULL 포인터 테스트
    TEST_ASSERT(et_audio_buffer_write(NULL, NULL, 0) == 0, "NULL buffer write");
    TEST_ASSERT(et_audio_buffer_read(NULL, NULL, 0) == 0, "NULL buffer read");
    TEST_ASSERT(et_audio_buffer_available_space(NULL) == 0, "NULL buffer space");
    TEST_ASSERT(et_audio_buffer_available_data(NULL) == 0, "NULL buffer data");

    // 디바이스 에러 테스트
    TEST_ASSERT(et_audio_set_callback(NULL, NULL, NULL) == ET_ERROR_INVALID_ARGUMENT, "NULL device callback");
    TEST_ASSERT(et_audio_start(NULL) == ET_ERROR_INVALID_ARGUMENT, "NULL device start");
    TEST_ASSERT(et_audio_stop(NULL) == ET_ERROR_INVALID_ARGUMENT, "NULL device stop");
    TEST_ASSERT(et_audio_pause(NULL) == ET_ERROR_INVALID_ARGUMENT, "NULL device pause");
    TEST_ASSERT(et_audio_get_state(NULL) == ET_AUDIO_STATE_STOPPED, "NULL device state");
    TEST_ASSERT(et_audio_get_latency(NULL) == 0, "NULL device latency");

    TEST_PASS();
}

/**
 * @brief 모든 테스트 실행
 */
int main(void) {
    printf("=== LibEtude Audio I/O Tests ===\n");

    // 메모리 시스템 초기화 (현재는 표준 malloc 사용)

    int passed = 0;
    int total = 0;

    // 테스트 실행
    total++; if (test_audio_format_create()) passed++;
    total++; if (test_audio_buffer_create_destroy()) passed++;
    total++; if (test_audio_buffer_write_read()) passed++;
    total++; if (test_audio_buffer_ring_behavior()) passed++;
    total++; if (test_audio_buffer_reset()) passed++;
    total++; if (test_audio_buffer_available_queries()) passed++;
    total++; if (test_audio_device_creation()) passed++;
    total++; if (test_audio_utility_functions()) passed++;
    total++; if (test_audio_error_handling()) passed++;

    // 결과 출력
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", passed, total);

    if (passed == total) {
        printf("All tests passed!\n");
    } else {
        printf("Some tests failed!\n");
    }

    // 메모리 시스템 정리 (현재는 표준 malloc 사용)

    return (passed == total) ? 0 : 1;
}