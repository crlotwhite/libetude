/**
 * @file test_voice_parameter_control.c
 * @brief 음성 파라미터 제어 기능 단위 테스트
 *
 * UTAU 피치 벤드, 볼륨, 모듈레이션, 타이밍 제어 기능을 테스트합니다.
 * 요구사항 3.1, 3.2, 3.3, 3.4를 검증합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>

// 테스트할 함수들을 포함
#include "voice_parameter_functions.c"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 테스트 상수
#define TEST_SAMPLE_RATE 44100
#define TEST_F0_LENGTH 100
#define TEST_FFT_SIZE 2048
#define TEST_FRAME_PERIOD 5.0
#define TEST_TARGET_PITCH 220.0f  // A3
#define EPSILON 1e-6

// 테스트 결과 카운터
static int tests_passed = 0;
static int tests_failed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_EQ(expected, actual, tolerance, message) \
    do { \
        if (fabs((expected) - (actual)) < (tolerance)) { \
            printf("✓ %s (expected: %.6f, actual: %.6f)\n", message, expected, actual); \
            tests_passed++; \
        } else { \
            printf("✗ %s (expected: %.6f, actual: %.6f, diff: %.6f)\n", \
                   message, expected, actual, fabs((expected) - (actual))); \
            tests_failed++; \
        } \
    } while(0)

// 테스트용 WorldParameters 생성
static WorldParameters* create_test_world_parameters(void) {
    WorldParameters* params = (WorldParameters*)malloc(sizeof(WorldParameters));
    if (!params) {
        return NULL;
    }

    // 기본 파라미터 초기화
    params->sample_rate = TEST_SAMPLE_RATE;
    params->audio_length = TEST_SAMPLE_RATE * TEST_F0_LENGTH * TEST_FRAME_PERIOD / 1000;
    params->frame_period = TEST_FRAME_PERIOD;
    params->f0_length = TEST_F0_LENGTH;
    params->fft_size = TEST_FFT_SIZE;
    params->owns_memory = true;
    params->mem_pool = NULL;

    // F0 배열 할당 및 초기화
    params->f0 = (double*)malloc(TEST_F0_LENGTH * sizeof(double));
    params->time_axis = (double*)malloc(TEST_F0_LENGTH * sizeof(double));

    if (!params->f0 || !params->time_axis) {
        free(params->f0);
        free(params->time_axis);
        free(params);
        return NULL;
    }

    // 테스트용 F0 데이터 생성 (기본 피치)
    for (int i = 0; i < TEST_F0_LENGTH; i++) {
        params->f0[i] = TEST_TARGET_PITCH;  // 모든 프레임을 유성음으로 설정
        params->time_axis[i] = i * TEST_FRAME_PERIOD / 1000.0;  // 시간축
    }

    // 스펙트로그램 배열 할당
    int spectrum_length = TEST_FFT_SIZE / 2 + 1;
    params->spectrogram = (double**)malloc(TEST_F0_LENGTH * sizeof(double*));
    params->aperiodicity = (double**)malloc(TEST_F0_LENGTH * sizeof(double*));

    if (!params->spectrogram || !params->aperiodicity) {
        free(params->f0);
        free(params->time_axis);
        free(params->spectrogram);
        free(params->aperiodicity);
        free(params);
        return NULL;
    }

    // 각 프레임별 스펙트럼 할당 및 초기화
    for (int frame = 0; frame < TEST_F0_LENGTH; frame++) {
        params->spectrogram[frame] = (double*)malloc(spectrum_length * sizeof(double));
        params->aperiodicity[frame] = (double*)malloc(spectrum_length * sizeof(double));

        if (!params->spectrogram[frame] || !params->aperiodicity[frame]) {
            // 할당 실패시 정리
            for (int j = 0; j < frame; j++) {
                free(params->spectrogram[j]);
                free(params->aperiodicity[j]);
            }
            free(params->f0);
            free(params->time_axis);
            free(params->spectrogram);
            free(params->aperiodicity);
            free(params);
            return NULL;
        }

        // 테스트용 스펙트로그램 데이터 생성 (단위 스펙트럼)
        for (int freq = 0; freq < spectrum_length; freq++) {
            params->spectrogram[frame][freq] = 1.0;  // 단위 크기
            params->aperiodicity[frame][freq] = 0.1;  // 기본 비주기성
        }
    }

    return params;
}

// 테스트용 WorldParameters 해제
static void destroy_test_world_parameters(WorldParameters* params) {
    if (!params) {
        return;
    }

    // 스펙트로그램 배열 해제
    if (params->spectrogram) {
        for (int i = 0; i < params->f0_length; i++) {
            free(params->spectrogram[i]);
        }
        free(params->spectrogram);
    }

    // 비주기성 배열 해제
    if (params->aperiodicity) {
        for (int i = 0; i < params->f0_length; i++) {
            free(params->aperiodicity[i]);
        }
        free(params->aperiodicity);
    }

    // 기본 배열들 해제
    free(params->f0);
    free(params->time_axis);
    free(params);
}

// ============================================================================
// 피치 벤드 테스트
// ============================================================================

void test_apply_pitch_bend_basic(void) {
    printf("\n=== 피치 벤드 기본 기능 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // 테스트용 피치 벤드 데이터 (100센트 상승)
    float pitch_bend[] = {0.0f, 50.0f, 100.0f, 50.0f, 0.0f};
    int pitch_bend_length = 5;

    // 피치 벤드 적용
    ETResult result = apply_pitch_bend(params, pitch_bend, pitch_bend_length, TEST_TARGET_PITCH);
    TEST_ASSERT(result == ET_SUCCESS, "피치 벤드 적용 성공");

    // 결과 검증 - 중간 지점에서 최대 피치 변화 확인
    int mid_frame = TEST_F0_LENGTH / 2;

    // 허용 오차 내에서 피치 변화 확인
    TEST_ASSERT(params->f0[mid_frame] > TEST_TARGET_PITCH * 1.05,
                "피치 벤드로 인한 F0 상승 확인");

    destroy_test_world_parameters(params);
}

void test_apply_pitch_bend_edge_cases(void) {
    printf("\n=== 피치 벤드 경계 조건 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // NULL 포인터 테스트
    ETResult result = apply_pitch_bend(NULL, NULL, 0, 0.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "NULL 파라미터 처리");

    // 빈 피치 벤드 데이터 테스트
    float empty_bend[] = {0.0f};
    result = apply_pitch_bend(params, empty_bend, 0, TEST_TARGET_PITCH);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "빈 피치 벤드 데이터 처리");

    // 극한 피치 벤드 값 테스트 (+1200센트)
    float extreme_bend[] = {1200.0f};
    result = apply_pitch_bend(params, extreme_bend, 1, TEST_TARGET_PITCH);
    TEST_ASSERT(result == ET_SUCCESS, "극한 피치 벤드 값 처리");

    // F0 범위 제한 확인
    TEST_ASSERT(params->f0[0] <= 1000.0, "F0 상한 제한 확인");
    TEST_ASSERT(params->f0[0] >= 50.0, "F0 하한 제한 확인");

    destroy_test_world_parameters(params);
}

void test_interpolate_pitch_bend(void) {
    printf("\n=== 피치 벤드 보간 테스트 ===\n");

    // 원본 피치 벤드 데이터
    float original_bend[] = {0.0f, 100.0f, 0.0f};
    int original_length = 3;
    int target_length = 5;

    float* interpolated_bend = (float*)malloc(target_length * sizeof(float));
    TEST_ASSERT(interpolated_bend != NULL, "보간 버퍼 할당");

    // 보간 수행
    ETResult result = interpolate_pitch_bend(original_bend, original_length,
                                           interpolated_bend, target_length);
    TEST_ASSERT(result == ET_SUCCESS, "피치 벤드 보간 성공");

    // 보간 결과 검증
    TEST_ASSERT_FLOAT_EQ(0.0f, interpolated_bend[0], EPSILON, "시작점 보간 정확성");
    TEST_ASSERT_FLOAT_EQ(0.0f, interpolated_bend[target_length-1], EPSILON, "끝점 보간 정확성");
    TEST_ASSERT(interpolated_bend[target_length/2] > 50.0f, "중간점 보간 값 확인");

    free(interpolated_bend);
}

void test_cents_conversion(void) {
    printf("\n=== 센트 변환 함수 테스트 ===\n");

    // 센트를 주파수 비율로 변환 테스트
    double ratio_octave = cents_to_frequency_ratio(1200.0f);  // 1 옥타브
    TEST_ASSERT_FLOAT_EQ(2.0, ratio_octave, EPSILON, "1200센트 = 2배 주파수");

    double ratio_semitone = cents_to_frequency_ratio(100.0f);  // 반음
    TEST_ASSERT_FLOAT_EQ(pow(2.0, 1.0/12.0), ratio_semitone, EPSILON, "100센트 = 반음");

    double ratio_zero = cents_to_frequency_ratio(0.0f);
    TEST_ASSERT_FLOAT_EQ(1.0, ratio_zero, EPSILON, "0센트 = 변화 없음");

    // 주파수 비율을 센트로 변환 테스트
    float cents_octave = frequency_ratio_to_cents(2.0);
    TEST_ASSERT_FLOAT_EQ(1200.0f, cents_octave, EPSILON, "2배 주파수 = 1200센트");

    float cents_semitone = frequency_ratio_to_cents(pow(2.0, 1.0/12.0));
    TEST_ASSERT_FLOAT_EQ(100.0f, cents_semitone, 0.1f, "반음 = 100센트");

    float cents_zero = frequency_ratio_to_cents(1.0);
    TEST_ASSERT_FLOAT_EQ(0.0f, cents_zero, EPSILON, "변화 없음 = 0센트");
}

// ============================================================================
// 볼륨 제어 테스트
// ============================================================================

void test_apply_volume_control_basic(void) {
    printf("\n=== 볼륨 제어 기본 기능 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // 원본 스펙트럼 값 저장
    double original_spectrum = params->spectrogram[0][0];

    // 50% 볼륨 적용
    float volume = 0.5f;
    ETResult result = apply_volume_control(params, volume);
    TEST_ASSERT(result == ET_SUCCESS, "볼륨 제어 적용 성공");

    // 볼륨 변화 확인
    double expected_spectrum = original_spectrum * volume;
    TEST_ASSERT_FLOAT_EQ(expected_spectrum, params->spectrogram[0][0], EPSILON,
                        "스펙트럼 크기 변화 확인");

    // 모든 프레임에 적용되었는지 확인
    bool all_frames_modified = true;
    int spectrum_length = params->fft_size / 2 + 1;
    for (int frame = 0; frame < params->f0_length && all_frames_modified; frame++) {
        for (int freq = 0; freq < spectrum_length; freq++) {
            if (fabs(params->spectrogram[frame][freq] - volume) > EPSILON) {
                all_frames_modified = false;
                break;
            }
        }
    }
    TEST_ASSERT(all_frames_modified, "모든 프레임에 볼륨 적용 확인");

    destroy_test_world_parameters(params);
}

void test_apply_volume_control_edge_cases(void) {
    printf("\n=== 볼륨 제어 경계 조건 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // NULL 포인터 테스트
    ETResult result = apply_volume_control(NULL, 1.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "NULL 파라미터 처리");

    // 음수 볼륨 테스트
    result = apply_volume_control(params, -0.5f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "음수 볼륨 처리");

    // 과도한 볼륨 테스트
    result = apply_volume_control(params, 3.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "과도한 볼륨 처리");

    // 0 볼륨 테스트 (무음)
    result = apply_volume_control(params, 0.0f);
    TEST_ASSERT(result == ET_SUCCESS, "0 볼륨 처리 성공");

    // 최소값 제한 확인
    TEST_ASSERT(params->spectrogram[0][0] >= 1e-10, "스펙트럼 최소값 제한 확인");

    destroy_test_world_parameters(params);
}

// ============================================================================
// 모듈레이션 테스트
// ============================================================================

void test_apply_modulation_basic(void) {
    printf("\n=== 모듈레이션 기본 기능 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // 원본 F0 값 저장
    double original_f0 = params->f0[0];

    // 모듈레이션 적용 (50% 깊이, 5Hz 속도)
    float modulation_depth = 0.5f;
    float modulation_rate = 5.0f;
    ETResult result = apply_modulation(params, modulation_depth, modulation_rate);
    TEST_ASSERT(result == ET_SUCCESS, "모듈레이션 적용 성공");

    // F0 변화 확인 (일부 프레임에서는 변화가 있어야 함)
    bool f0_changed = false;
    for (int i = 0; i < params->f0_length; i++) {
        if (fabs(params->f0[i] - original_f0) > EPSILON) {
            f0_changed = true;
            break;
        }
    }
    TEST_ASSERT(f0_changed, "모듈레이션으로 인한 F0 변화 확인");

    // F0 범위 제한 확인
    for (int i = 0; i < params->f0_length; i++) {
        TEST_ASSERT(params->f0[i] >= 50.0 && params->f0[i] <= 1000.0,
                   "모듈레이션 후 F0 범위 제한 확인");
    }

    destroy_test_world_parameters(params);
}

void test_apply_modulation_edge_cases(void) {
    printf("\n=== 모듈레이션 경계 조건 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // NULL 포인터 테스트
    ETResult result = apply_modulation(NULL, 0.5f, 5.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "NULL 파라미터 처리");

    // 잘못된 모듈레이션 깊이 테스트
    result = apply_modulation(params, -0.1f, 5.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "음수 모듈레이션 깊이 처리");

    result = apply_modulation(params, 1.5f, 5.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "과도한 모듈레이션 깊이 처리");

    // 잘못된 모듈레이션 속도 테스트
    result = apply_modulation(params, 0.5f, 0.05f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "너무 느린 모듈레이션 속도 처리");

    result = apply_modulation(params, 0.5f, 25.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "너무 빠른 모듈레이션 속도 처리");

    destroy_test_world_parameters(params);
}

// ============================================================================
// 타이밍 제어 테스트
// ============================================================================

void test_apply_timing_control_basic(void) {
    printf("\n=== 타이밍 제어 기본 기능 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // 원본 값들 저장
    double original_time_axis = params->time_axis[1];
    double original_frame_period = params->frame_period;
    int original_audio_length = params->audio_length;

    // 2배 속도 적용
    float time_scale = 2.0f;
    ETResult result = apply_timing_control(params, time_scale);
    TEST_ASSERT(result == ET_SUCCESS, "타이밍 제어 적용 성공");

    // 시간축 변화 확인
    double expected_time_axis = original_time_axis / time_scale;
    TEST_ASSERT_FLOAT_EQ(expected_time_axis, params->time_axis[1], EPSILON,
                        "시간축 스케일링 확인");

    // 프레임 주기 변화 확인
    double expected_frame_period = original_frame_period / time_scale;
    TEST_ASSERT_FLOAT_EQ(expected_frame_period, params->frame_period, EPSILON,
                        "프레임 주기 스케일링 확인");

    // 오디오 길이 변화 확인
    int expected_audio_length = (int)(original_audio_length / time_scale);
    TEST_ASSERT(params->audio_length == expected_audio_length,
               "오디오 길이 스케일링 확인");

    destroy_test_world_parameters(params);
}

void test_apply_timing_control_edge_cases(void) {
    printf("\n=== 타이밍 제어 경계 조건 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // NULL 포인터 테스트
    ETResult result = apply_timing_control(NULL, 1.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "NULL 파라미터 처리");

    // 잘못된 시간 스케일 테스트
    result = apply_timing_control(params, 0.05f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "너무 느린 시간 스케일 처리");

    result = apply_timing_control(params, 5.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "너무 빠른 시간 스케일 처리");

    // 0 또는 음수 시간 스케일 테스트
    result = apply_timing_control(params, 0.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "0 시간 스케일 처리");

    result = apply_timing_control(params, -1.0f);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "음수 시간 스케일 처리");

    destroy_test_world_parameters(params);
}

// ============================================================================
// 통합 테스트
// ============================================================================

void test_combined_parameter_control(void) {
    printf("\n=== 통합 파라미터 제어 테스트 ===\n");

    WorldParameters* params = create_test_world_parameters();
    TEST_ASSERT(params != NULL, "WorldParameters 생성");

    // 피치 벤드 적용
    float pitch_bend[] = {0.0f, 100.0f, 0.0f};
    ETResult result = apply_pitch_bend(params, pitch_bend, 3, TEST_TARGET_PITCH);
    TEST_ASSERT(result == ET_SUCCESS, "피치 벤드 적용");

    // 볼륨 제어 적용
    result = apply_volume_control(params, 0.8f);
    TEST_ASSERT(result == ET_SUCCESS, "볼륨 제어 적용");

    // 모듈레이션 적용
    result = apply_modulation(params, 0.3f, 6.0f);
    TEST_ASSERT(result == ET_SUCCESS, "모듈레이션 적용");

    // 타이밍 제어 적용
    result = apply_timing_control(params, 1.2f);
    TEST_ASSERT(result == ET_SUCCESS, "타이밍 제어 적용");

    // 모든 제어가 적용된 후 파라미터 유효성 확인
    TEST_ASSERT(params->f0[0] > 0.0, "F0 유효성 확인");
    TEST_ASSERT(params->spectrogram[0][0] > 0.0, "스펙트럼 유효성 확인");
    TEST_ASSERT(params->frame_period > 0.0, "프레임 주기 유효성 확인");

    destroy_test_world_parameters(params);
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main(void) {
    printf("=== 음성 파라미터 제어 단위 테스트 시작 ===\n");

    // 피치 벤드 테스트
    test_apply_pitch_bend_basic();
    test_apply_pitch_bend_edge_cases();
    test_interpolate_pitch_bend();
    test_cents_conversion();

    // 볼륨 제어 테스트
    test_apply_volume_control_basic();
    test_apply_volume_control_edge_cases();

    // 모듈레이션 테스트
    test_apply_modulation_basic();
    test_apply_modulation_edge_cases();

    // 타이밍 제어 테스트
    test_apply_timing_control_basic();
    test_apply_timing_control_edge_cases();

    // 통합 테스트
    test_combined_parameter_control();

    // 테스트 결과 출력
    printf("\n=== 테스트 결과 ===\n");
    printf("통과: %d\n", tests_passed);
    printf("실패: %d\n", tests_failed);
    printf("총 테스트: %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("✓ 모든 테스트가 통과했습니다!\n");
        return 0;
    } else {
        printf("✗ %d개의 테스트가 실패했습니다.\n", tests_failed);
        return 1;
    }
}