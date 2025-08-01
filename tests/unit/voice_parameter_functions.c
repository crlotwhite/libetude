/**
 * @file voice_parameter_functions.c
 * @brief 음성 파라미터 제어 함수들 (테스트용)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// libetude 타입 정의 (테스트용)
typedef enum {
    ET_SUCCESS = 0,
    ET_ERROR_INVALID_ARGUMENT = -1,
    ET_ERROR_INVALID_STATE = -2,
    ET_ERROR_OUT_OF_MEMORY = -3
} ETResult;

typedef struct ETMemoryPool ETMemoryPool;

// 함수 선언
ETResult interpolate_pitch_bend(const float* pitch_bend, int pitch_bend_length,
                               float* interpolated_bend, int target_length);
double cents_to_frequency_ratio(float cents);
float frequency_ratio_to_cents(double ratio);

// WorldParameters 구조체 정의
typedef struct WorldParameters {
    int sample_rate;
    int audio_length;
    double frame_period;
    int f0_length;
    int fft_size;
    double* f0;
    double* time_axis;
    double** spectrogram;
    double** aperiodicity;
    bool owns_memory;
    ETMemoryPool* mem_pool;
} WorldParameters;

// ============================================================================
// 음성 파라미터 제어 함수들
// ============================================================================

ETResult apply_pitch_bend(WorldParameters* params,
                         const float* pitch_bend, int pitch_bend_length,
                         float target_pitch) {
    if (!params || !pitch_bend || pitch_bend_length <= 0 || target_pitch <= 0.0f) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!params->f0 || params->f0_length <= 0) {
        return ET_ERROR_INVALID_STATE;
    }

    // 피치 벤드 데이터를 F0 프레임 수에 맞게 보간
    float* interpolated_bend = (float*)malloc(params->f0_length * sizeof(float));
    if (!interpolated_bend) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 피치 벤드 보간
    ETResult result = interpolate_pitch_bend(pitch_bend, pitch_bend_length,
                                           interpolated_bend, params->f0_length);
    if (result != ET_SUCCESS) {
        free(interpolated_bend);
        return result;
    }

    // 각 F0 프레임에 피치 벤드 적용
    for (int i = 0; i < params->f0_length; i++) {
        if (params->f0[i] > 0.0) {  // 유성음 프레임만 처리
            // 센트를 주파수 비율로 변환
            double frequency_ratio = cents_to_frequency_ratio(interpolated_bend[i]);

            // 기본 F0에 피치 벤드 적용
            params->f0[i] = target_pitch * frequency_ratio;

            // F0 범위 제한 (일반적인 음성 범위)
            if (params->f0[i] < 50.0) {
                params->f0[i] = 50.0;
            } else if (params->f0[i] > 1000.0) {
                params->f0[i] = 1000.0;
            }
        }
    }

    // 메모리 해제
    free(interpolated_bend);
    return ET_SUCCESS;
}

ETResult apply_volume_control(WorldParameters* params, float volume) {
    if (!params || volume < 0.0f || volume > 2.0f) {  // 2.0까지 허용 (200% 볼륨)
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!params->spectrogram || params->f0_length <= 0 || params->fft_size <= 0) {
        return ET_ERROR_INVALID_STATE;
    }

    int spectrum_length = params->fft_size / 2 + 1;
    double volume_db = 20.0 * log10(volume);  // 볼륨을 dB로 변환
    double volume_linear = pow(10.0, volume_db / 20.0);  // 선형 스케일로 변환

    // 모든 프레임의 스펙트럼에 볼륨 적용
    for (int frame = 0; frame < params->f0_length; frame++) {
        for (int freq = 0; freq < spectrum_length; freq++) {
            params->spectrogram[frame][freq] *= volume_linear;

            // 스펙트럼 값이 너무 작아지지 않도록 제한
            if (params->spectrogram[frame][freq] < 1e-10) {
                params->spectrogram[frame][freq] = 1e-10;
            }
        }
    }

    return ET_SUCCESS;
}

ETResult apply_modulation(WorldParameters* params,
                         float modulation_depth, float modulation_rate) {
    if (!params || modulation_depth < 0.0f || modulation_depth > 1.0f ||
        modulation_rate < 0.1f || modulation_rate > 20.0f) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!params->f0 || !params->time_axis || params->f0_length <= 0) {
        return ET_ERROR_INVALID_STATE;
    }

    // 모듈레이션 깊이를 센트로 변환 (최대 50센트)
    float max_modulation_cents = 50.0f * modulation_depth;

    // 각 F0 프레임에 모듈레이션 적용
    for (int i = 0; i < params->f0_length; i++) {
        if (params->f0[i] > 0.0) {  // 유성음 프레임만 처리
            // 사인파 기반 모듈레이션 계산
            double time_sec = params->time_axis[i];
            double modulation_phase = 2.0 * M_PI * modulation_rate * time_sec;
            double modulation_cents = max_modulation_cents * sin(modulation_phase);

            // 센트를 주파수 비율로 변환하여 적용
            double frequency_ratio = cents_to_frequency_ratio((float)modulation_cents);
            params->f0[i] *= frequency_ratio;

            // F0 범위 제한
            if (params->f0[i] < 50.0) {
                params->f0[i] = 50.0;
            } else if (params->f0[i] > 1000.0) {
                params->f0[i] = 1000.0;
            }
        }
    }

    return ET_SUCCESS;
}

ETResult apply_timing_control(WorldParameters* params, float time_scale) {
    if (!params || time_scale <= 0.1f || time_scale > 3.0f) {  // 0.1x ~ 3.0x 속도 허용
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!params->time_axis || params->f0_length <= 0) {
        return ET_ERROR_INVALID_STATE;
    }

    // 시간축 스케일링
    for (int i = 0; i < params->f0_length; i++) {
        params->time_axis[i] /= time_scale;
    }

    // 프레임 주기도 조정
    params->frame_period /= time_scale;

    // 오디오 길이도 조정
    params->audio_length = (int)(params->audio_length / time_scale);

    return ET_SUCCESS;
}

ETResult interpolate_pitch_bend(const float* pitch_bend, int pitch_bend_length,
                               float* interpolated_bend, int target_length) {
    if (!pitch_bend || !interpolated_bend || pitch_bend_length <= 0 || target_length <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (pitch_bend_length == target_length) {
        // 길이가 같으면 단순 복사
        memcpy(interpolated_bend, pitch_bend, target_length * sizeof(float));
        return ET_SUCCESS;
    }

    // 선형 보간을 사용하여 리샘플링
    double scale_factor = (double)(pitch_bend_length - 1) / (double)(target_length - 1);

    for (int i = 0; i < target_length; i++) {
        double source_index = i * scale_factor;
        int index_low = (int)floor(source_index);
        int index_high = index_low + 1;
        double fraction = source_index - index_low;

        if (index_high >= pitch_bend_length) {
            // 마지막 값 사용
            interpolated_bend[i] = pitch_bend[pitch_bend_length - 1];
        } else {
            // 선형 보간
            interpolated_bend[i] = (float)((1.0 - fraction) * pitch_bend[index_low] +
                                          fraction * pitch_bend[index_high]);
        }
    }

    return ET_SUCCESS;
}

double cents_to_frequency_ratio(float cents) {
    // 1200 센트 = 1 옥타브 = 2배 주파수
    // 주파수 비율 = 2^(cents/1200)
    return pow(2.0, cents / 1200.0);
}

float frequency_ratio_to_cents(double ratio) {
    // 센트 = 1200 * log2(ratio)
    if (ratio <= 0.0) {
        return 0.0f;
    }
    return (float)(1200.0 * log2(ratio));
}