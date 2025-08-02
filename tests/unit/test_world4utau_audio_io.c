/**
 * @file test_world4utau_audio_io.c
 * @brief world4utau 오디오 I/O 단위 테스트
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

// world4utau 헤더 포함
#include "../../examples/world4utau/include/audio_file_io.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ PASS: %s\n", message); \
        } else { \
            tests_failed++; \
            printf("✗ FAIL: %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

#define TEST_ASSERT_FLOAT_EQUAL(expected, actual, tolerance, message) \
    TEST_ASSERT(fabs((expected) - (actual)) < (tolerance), message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

// 테스트 데이터 생성 함수
static AudioData* create_test_audio_data(uint32_t num_samples, uint16_t num_channels,
                                        uint32_t sample_rate, float frequency) {
    AudioData* audio_data = audio_data_create(num_samples, num_channels, sample_rate);
    if (!audio_data) {
        return NULL;
    }

    // 사인파 생성
    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        float sample = 0.5f * sinf(2.0f * M_PI * frequency * t);

        for (uint16_t ch = 0; ch < num_channels; ch++) {
            audio_data->data[i * num_channels + ch] = sample;
        }
    }

    return audio_data;
}

// 임시 파일 생성 함수
static char* create_temp_filename(const char* prefix, const char* extension) {
    static char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/%s_test_%d.%s",
             prefix, (int)getpid(), extension);
    return filename;
}

// ============================================================================
// WAV 파일 I/O 테스트
// ============================================================================

void test_wav_file_info() {
    printf("\n=== WAV 파일 정보 조회 테스트 ===\n");

    // 테스트 오디오 데이터 생성
    AudioData* test_data = create_test_audio_data(44100, 2, 44100, 440.0f);
    TEST_ASSERT_NOT_NULL(test_data, "테스트 오디오 데이터 생성");

    // 임시 WAV 파일 생성
    char* temp_file = create_temp_filename("wav_info", "wav");
    ETResult result = write_wav_file(temp_file, test_data);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "WAV 파일 쓰기");

    // 파일 정보 조회
    AudioFileInfo info;
    result = get_wav_file_info(temp_file, &info);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "WAV 파일 정보 조회");
    TEST_ASSERT_EQUAL(44100, info.sample_rate, "샘플링 레이트 확인");
    TEST_ASSERT_EQUAL(2, info.num_channels, "채널 수 확인");
    TEST_ASSERT_EQUAL(44100, info.num_samples, "샘플 수 확인");
    TEST_ASSERT_FLOAT_EQUAL(1.0, info.duration_seconds, 0.001, "재생 시간 확인");

    // 정리
    audio_data_destroy(test_data);
    unlink(temp_file);
}

void test_wav_file_read_write() {
    printf("\n=== WAV 파일 읽기/쓰기 테스트 ===\n");

    // 다양한 포맷으로 테스트
    struct {
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t num_samples;
        float frequency;
    } test_cases[] = {
        {1, 22050, 22050, 220.0f},  // 모노, 22kHz, 1초
        {2, 44100, 44100, 440.0f},  // 스테레오, 44kHz, 1초
        {1, 48000, 24000, 880.0f},  // 모노, 48kHz, 0.5초
        {2, 96000, 48000, 1760.0f}  // 스테레오, 96kHz, 0.5초
    };

    int num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_cases; i++) {
        printf("테스트 케이스 %d: %dch, %dHz, %d샘플\n",
               i + 1, test_cases[i].channels, test_cases[i].sample_rate,
               test_cases[i].num_samples);

        // 원본 데이터 생성
        AudioData* original = create_test_audio_data(
            test_cases[i].num_samples, test_cases[i].channels,
            test_cases[i].sample_rate, test_cases[i].frequency);
        TEST_ASSERT_NOT_NULL(original, "원본 오디오 데이터 생성");

        // WAV 파일로 저장
        char* temp_file = create_temp_filename("readwrite", "wav");
        ETResult result = write_wav_file(temp_file, original);
        TEST_ASSERT_EQUAL(ET_SUCCESS, result, "WAV 파일 쓰기");

        // WAV 파일 읽기
        AudioData loaded;
        result = read_wav_file(temp_file, &loaded);
        TEST_ASSERT_EQUAL(ET_SUCCESS, result, "WAV 파일 읽기");

        // 데이터 비교
        TEST_ASSERT_EQUAL(original->info.sample_rate, loaded.info.sample_rate, "샘플링 레이트 일치");
        TEST_ASSERT_EQUAL(original->info.num_channels, loaded.info.num_channels, "채널 수 일치");
        TEST_ASSERT_EQUAL(original->info.num_samples, loaded.info.num_samples, "샘플 수 일치");

        // 오디오 데이터 비교 (약간의 오차 허용)
        bool data_match = true;
        uint32_t total_samples = original->info.num_samples * original->info.num_channels;
        for (uint32_t j = 0; j < total_samples && data_match; j++) {
            if (fabs(original->data[j] - loaded.data[j]) > 0.001f) {
                data_match = false;
            }
        }
        TEST_ASSERT(data_match, "오디오 데이터 일치");

        // 정리
        audio_data_destroy(original);
        if (loaded.owns_data && loaded.data) {
            free(loaded.data);
        }
        unlink(temp_file);
    }
}

void test_audio_data_operations() {
    printf("\n=== 오디오 데이터 조작 테스트 ===\n");

    // 테스트 데이터 생성 (스테레오)
    AudioData* stereo_data = create_test_audio_data(44100, 2, 44100, 440.0f);
    TEST_ASSERT_NOT_NULL(stereo_data, "스테레오 테스트 데이터 생성");

    // 모노 변환 테스트
    ETResult result = audio_data_to_mono(stereo_data);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "스테레오를 모노로 변환");
    TEST_ASSERT_EQUAL(1, stereo_data->info.num_channels, "모노 변환 후 채널 수 확인");

    // 샘플링 레이트 변환 테스트
    uint32_t original_samples = stereo_data->info.num_samples;
    result = audio_data_resample(stereo_data, 22050);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "샘플링 레이트 변환 (44kHz -> 22kHz)");
    TEST_ASSERT_EQUAL(22050, stereo_data->info.sample_rate, "변환 후 샘플링 레이트 확인");

    // 샘플 수가 대략 절반이 되었는지 확인 (±10% 오차 허용)
    uint32_t expected_samples = original_samples / 2;
    uint32_t actual_samples = stereo_data->info.num_samples;
    bool sample_count_ok = (actual_samples >= expected_samples * 0.9) &&
                          (actual_samples <= expected_samples * 1.1);
    TEST_ASSERT(sample_count_ok, "리샘플링 후 샘플 수 확인");

    // 정규화 테스트
    // 먼저 데이터를 증폭하여 정규화가 필요하도록 만듦
    uint32_t total_samples = stereo_data->info.num_samples * stereo_data->info.num_channels;
    for (uint32_t i = 0; i < total_samples; i++) {
        stereo_data->data[i] *= 2.0f; // 2배 증폭
    }

    result = audio_data_normalize(stereo_data);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "오디오 데이터 정규화");

    // 정규화 후 최댓값이 1.0 이하인지 확인
    float max_val = 0.0f;
    for (uint32_t i = 0; i < total_samples; i++) {
        float abs_val = fabs(stereo_data->data[i]);
        if (abs_val > max_val) {
            max_val = abs_val;
        }
    }
    TEST_ASSERT(max_val <= 1.0f, "정규화 후 최댓값 확인");

    // 정리
    audio_data_destroy(stereo_data);
}

void test_audio_statistics() {
    printf("\n=== 오디오 통계 계산 테스트 ===\n");

    // 알려진 값으로 테스트 데이터 생성
    AudioData* test_data = audio_data_create(1000, 1, 44100);
    TEST_ASSERT_NOT_NULL(test_data, "테스트 데이터 생성");

    // 간단한 패턴으로 데이터 채우기 (-0.5 ~ 0.5)
    for (uint32_t i = 0; i < 1000; i++) {
        test_data->data[i] = (float)i / 1000.0f - 0.5f;
    }

    // 통계 계산
    float min_val, max_val, mean_val, rms_val;
    ETResult result = calculate_audio_statistics(test_data, &min_val, &max_val,
                                               &mean_val, &rms_val);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "오디오 통계 계산");

    // 예상 값과 비교
    TEST_ASSERT_FLOAT_EQUAL(-0.5f, min_val, 0.001f, "최솟값 확인");
    TEST_ASSERT_FLOAT_EQUAL(0.499f, max_val, 0.001f, "최댓값 확인");
    TEST_ASSERT_FLOAT_EQUAL(-0.0005f, mean_val, 0.001f, "평균값 확인");

    // 정리
    audio_data_destroy(test_data);
}

void test_silence_detection() {
    printf("\n=== 무음 감지 테스트 ===\n");

    // 무음이 포함된 테스트 데이터 생성
    AudioData* test_data = audio_data_create(44100, 1, 44100); // 1초
    TEST_ASSERT_NOT_NULL(test_data, "테스트 데이터 생성");

    // 패턴: 0.2초 신호, 0.3초 무음, 0.2초 신호, 0.3초 무음
    for (uint32_t i = 0; i < 44100; i++) {
        float t = (float)i / 44100.0f;

        if ((t >= 0.0f && t < 0.2f) || (t >= 0.5f && t < 0.7f)) {
            // 신호 구간
            test_data->data[i] = 0.5f * sinf(2.0f * M_PI * 440.0f * t);
        } else {
            // 무음 구간
            test_data->data[i] = 0.0f;
        }
    }

    // 무음 감지
    float silence_start[10], silence_end[10];
    int silence_count = detect_silence_regions(test_data, 0.01f, 100.0f, // 100ms 최소 지속시간
                                             silence_start, silence_end, 10);

    TEST_ASSERT(silence_count >= 1, "무음 구간 감지");

    if (silence_count > 0) {
        printf("감지된 무음 구간: %.3f초 - %.3f초\n", silence_start[0], silence_end[0]);
        TEST_ASSERT(silence_start[0] >= 0.15f && silence_start[0] <= 0.25f, "첫 번째 무음 시작 시간");
        TEST_ASSERT(silence_end[0] >= 0.45f && silence_end[0] <= 0.55f, "첫 번째 무음 종료 시간");
    }

    // 무음 트림 테스트
    // 앞뒤에 무음이 있는 데이터 생성
    AudioData* trim_data = audio_data_create(44100, 1, 44100);
    TEST_ASSERT_NOT_NULL(trim_data, "트림 테스트 데이터 생성");

    for (uint32_t i = 0; i < 44100; i++) {
        float t = (float)i / 44100.0f;

        if (t >= 0.2f && t < 0.8f) {
            // 중간 0.6초만 신호
            trim_data->data[i] = 0.5f * sinf(2.0f * M_PI * 440.0f * t);
        } else {
            // 앞뒤 무음
            trim_data->data[i] = 0.0f;
        }
    }

    uint32_t original_samples = trim_data->info.num_samples;
    ETResult result = trim_audio_silence(trim_data, 0.01f);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "무음 트림");
    TEST_ASSERT(trim_data->info.num_samples < original_samples, "트림 후 샘플 수 감소");

    // 정리
    audio_data_destroy(test_data);
    audio_data_destroy(trim_data);
}

// ============================================================================
// libetude 오디오 I/O 통합 테스트
// ============================================================================

void test_libetude_audio_io_init() {
    printf("\n=== libetude 오디오 I/O 초기화 테스트 ===\n");

    WorldAudioIO audio_io;
    ETResult result = world_audio_io_init(&audio_io, 44100, 2, 512);

    // 초기화 성공 여부는 시스템에 따라 다를 수 있음
    if (result == ET_SUCCESS) {
        TEST_ASSERT(audio_io.is_initialized, "오디오 I/O 초기화 상태");
        TEST_ASSERT_EQUAL(44100, audio_io.format.sample_rate, "샘플링 레이트 설정");
        TEST_ASSERT_EQUAL(2, audio_io.format.num_channels, "채널 수 설정");
        TEST_ASSERT_NOT_NULL(audio_io.output_buffer, "출력 버퍼 생성");

        // 정리
        world_audio_io_cleanup(&audio_io);
        TEST_ASSERT(!audio_io.is_initialized, "오디오 I/O 정리 후 상태");
    } else {
        printf("Warning: 오디오 I/O 초기화 실패 (시스템에 오디오 디바이스가 없을 수 있음)\n");
        tests_run++; // 테스트 실행으로 카운트하지만 실패로 처리하지 않음
    }
}

void test_audio_data_buffer_conversion() {
    printf("\n=== AudioData와 ETAudioBuffer 변환 테스트 ===\n");

    // 테스트 데이터 생성
    AudioData* test_data = create_test_audio_data(1024, 2, 44100, 440.0f);
    TEST_ASSERT_NOT_NULL(test_data, "테스트 데이터 생성");

    // ETAudioBuffer 생성
    ETAudioBuffer* buffer = et_audio_buffer_create(2048, 2);
    TEST_ASSERT_NOT_NULL(buffer, "ETAudioBuffer 생성");

    // AudioData를 ETAudioBuffer로 변환
    ETResult result = audio_data_to_et_buffer(test_data, buffer);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "AudioData를 ETAudioBuffer로 변환");

    // 버퍼에 데이터가 있는지 확인
    uint32_t available_data = et_audio_buffer_available_data(buffer);
    TEST_ASSERT_EQUAL(1024, available_data, "버퍼의 사용 가능한 데이터 확인");

    // ETAudioBuffer를 AudioData로 변환
    AudioData converted_data;
    result = et_buffer_to_audio_data(buffer, &converted_data, 44100, 2);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "ETAudioBuffer를 AudioData로 변환");

    // 변환된 데이터 확인
    TEST_ASSERT_EQUAL(44100, converted_data.info.sample_rate, "변환된 샘플링 레이트");
    TEST_ASSERT_EQUAL(2, converted_data.info.num_channels, "변환된 채널 수");
    TEST_ASSERT_EQUAL(1024, converted_data.info.num_samples, "변환된 샘플 수");

    // 데이터 내용 비교 (첫 몇 개 샘플)
    bool data_match = true;
    for (int i = 0; i < 10 && data_match; i++) {
        for (int ch = 0; ch < 2; ch++) {
            float original_val = test_data->data[i * 2 + ch];
            float converted_val = converted_data.data[i * 2 + ch];
            if (fabs(original_val - converted_val) > 0.001f) {
                data_match = false;
                break;
            }
        }
    }
    TEST_ASSERT(data_match, "변환된 데이터 내용 일치");

    // 정리
    audio_data_destroy(test_data);
    et_audio_buffer_destroy(buffer);
    if (converted_data.owns_data && converted_data.data) {
        free(converted_data.data);
    }
}

void test_audio_device_enumeration() {
    printf("\n=== 오디오 디바이스 열거 테스트 ===\n");

    char output_devices[10][256];
    char input_devices[10][256];

    // 출력 디바이스 열거
    int output_count = enumerate_audio_devices(output_devices, 10, false);
    TEST_ASSERT(output_count > 0, "출력 디바이스 발견");

    if (output_count > 0) {
        printf("발견된 출력 디바이스: %s\n", output_devices[0]);
    }

    // 입력 디바이스 열거
    int input_count = enumerate_audio_devices(input_devices, 10, true);
    TEST_ASSERT(input_count > 0, "입력 디바이스 발견");

    if (input_count > 0) {
        printf("발견된 입력 디바이스: %s\n", input_devices[0]);
    }

    // 디바이스 정보 조회
    uint32_t supported_rates[20];
    uint16_t supported_channels[10];

    bool info_result = get_audio_device_info(NULL, false, supported_rates, 20,
                                           supported_channels, 10);
    TEST_ASSERT(info_result, "기본 출력 디바이스 정보 조회");
}

// ============================================================================
// 고급 기능 테스트
// ============================================================================

void test_advanced_wav_operations() {
    printf("\n=== 고급 WAV 파일 기능 테스트 ===\n");

    // 테스트 데이터 생성
    AudioData* test_data = create_test_audio_data(44100, 2, 44100, 440.0f);
    TEST_ASSERT_NOT_NULL(test_data, "테스트 데이터 생성");

    // 고품질 WAV 파일 쓰기 테스트 (24비트)
    char* temp_file_24bit = create_temp_filename("advanced_24bit", "wav");
    ETResult result = write_wav_file_advanced(temp_file_24bit, test_data, 24, false, true);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "24비트 WAV 파일 쓰기 (디더링 적용)");

    // 파일 정보 확인
    AudioFileInfo info;
    result = get_wav_file_info(temp_file_24bit, &info);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "24비트 WAV 파일 정보 조회");
    TEST_ASSERT_EQUAL(24, info.bits_per_sample, "24비트 포맷 확인");

    // IEEE float WAV 파일 쓰기 테스트
    char* temp_file_float = create_temp_filename("advanced_float", "wav");
    result = write_wav_file_advanced(temp_file_float, test_data, 32, true, false);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "IEEE float WAV 파일 쓰기");

    // 파일 정보 확인
    result = get_wav_file_info(temp_file_float, &info);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "IEEE float WAV 파일 정보 조회");
    TEST_ASSERT(info.is_float_format, "IEEE float 포맷 확인");

    // 메타데이터 출력 테스트
    result = print_wav_file_metadata(temp_file_24bit);
    TEST_ASSERT_EQUAL(ET_SUCCESS, result, "WAV 파일 메타데이터 출력");

    // 정리
    audio_data_destroy(test_data);
    unlink(temp_file_24bit);
    unlink(temp_file_float);
}

void test_batch_conversion() {
    printf("\n=== 배치 변환 테스트 ===\n");

    // 여러 테스트 파일 생성
    const int num_files = 3;
    char* input_files[num_files];
    char* output_files[num_files];
    AudioData* test_data[num_files];

    // 다양한 포맷의 테스트 파일 생성
    struct {
        uint16_t channels;
        uint32_t sample_rate;
        float frequency;
    } file_configs[] = {
        {1, 22050, 220.0f},
        {2, 44100, 440.0f},
        {1, 48000, 880.0f}
    };

    for (int i = 0; i < num_files; i++) {
        test_data[i] = create_test_audio_data(
            file_configs[i].sample_rate, file_configs[i].channels,
            file_configs[i].sample_rate, file_configs[i].frequency);
        TEST_ASSERT_NOT_NULL(test_data[i], "배치 테스트 데이터 생성");

        input_files[i] = create_temp_filename("batch_input", "wav");
        output_files[i] = create_temp_filename("batch_output", "wav");

        ETResult result = write_wav_file(input_files[i], test_data[i]);
        TEST_ASSERT_EQUAL(ET_SUCCESS, result, "배치 입력 파일 생성");
    }

    // 배치 변환 실행 (모노로 변환, 44.1kHz로 통일)
    int success_count = batch_convert_wav_files(
        (const char**)input_files, (const char**)output_files, num_files,
        44100, 16, true);

    TEST_ASSERT_EQUAL(num_files, success_count, "배치 변환 성공 개수");

    // 변환 결과 확인
    for (int i = 0; i < num_files; i++) {
        AudioFileInfo info;
        ETResult result = get_wav_file_info(output_files[i], &info);
        TEST_ASSERT_EQUAL(ET_SUCCESS, result, "변환된 파일 정보 조회");
        TEST_ASSERT_EQUAL(1, info.num_channels, "모노 변환 확인");
        TEST_ASSERT_EQUAL(44100, info.sample_rate, "샘플링 레이트 통일 확인");
        TEST_ASSERT_EQUAL(16, info.bits_per_sample, "16비트 변환 확인");
    }

    // 정리
    for (int i = 0; i < num_files; i++) {
        audio_data_destroy(test_data[i]);
        unlink(input_files[i]);
        unlink(output_files[i]);
    }
}

// ============================================================================
// 메인 테스트 실행 함수
// ============================================================================

int main() {
    printf("=== world4utau 오디오 I/O 단위 테스트 시작 ===\n");

    // WAV 파일 I/O 테스트
    test_wav_file_info();
    test_wav_file_read_write();
    test_audio_data_operations();
    test_audio_statistics();
    test_silence_detection();

    // libetude 오디오 I/O 통합 테스트
    test_libetude_audio_io_init();
    test_audio_data_buffer_conversion();
    test_audio_device_enumeration();

    // 고급 기능 테스트
    test_advanced_wav_operations();
    test_batch_conversion();

    // 테스트 결과 출력
    printf("\n=== 테스트 결과 ===\n");
    printf("총 테스트: %d\n", tests_run);
    printf("성공: %d\n", tests_passed);
    printf("실패: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("✓ 모든 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("✗ %d개의 테스트가 실패했습니다.\n", tests_failed);
        return 1;
    }
}