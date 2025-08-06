/**
 * @file audio_common.c
 * @brief 플랫폼 추상화 오디오 공통 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/audio.h"
#include "libetude/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// 오디오 포맷 관련 함수
// ============================================================================

/**
 * @brief 기본 오디오 포맷을 생성합니다
 */
ETAudioFormat et_audio_format_create(uint32_t sample_rate, uint16_t num_channels, uint32_t buffer_size) {
    ETAudioFormat format;
    memset(&format, 0, sizeof(format));

    format.sample_rate = sample_rate;
    format.num_channels = num_channels;
    format.buffer_size = buffer_size;
    format.bit_depth = 32;  // 기본적으로 32비트 float 사용
    format.is_float = true;
    format.frame_size = num_channels * sizeof(float);

    return format;
}

/**
 * @brief 오디오 포맷이 유효한지 검증합니다
 */
bool et_audio_format_validate(const ETAudioFormat* format) {
    if (!format) {
        return false;
    }

    // 샘플링 레이트 검증 (8kHz ~ 192kHz)
    if (format->sample_rate < 8000 || format->sample_rate > 192000) {
        return false;
    }

    // 채널 수 검증 (1 ~ 8채널)
    if (format->num_channels < 1 || format->num_channels > 8) {
        return false;
    }

    // 비트 깊이 검증
    if (format->bit_depth != 16 && format->bit_depth != 24 && format->bit_depth != 32) {
        return false;
    }

    // 버퍼 크기 검증 (64 ~ 8192 프레임)
    if (format->buffer_size < 64 || format->buffer_size > 8192) {
        return false;
    }

    // 프레임 크기 검증
    uint32_t expected_frame_size = format->num_channels * (format->is_float ? sizeof(float) : (format->bit_depth / 8));
    if (format->frame_size != expected_frame_size) {
        return false;
    }

    return true;
}

/**
 * @brief 두 오디오 포맷이 호환되는지 확인합니다
 */
bool et_audio_format_compatible(const ETAudioFormat* format1, const ETAudioFormat* format2) {
    if (!format1 || !format2) {
        return false;
    }

    return (format1->sample_rate == format2->sample_rate &&
            format1->num_channels == format2->num_channels &&
            format1->bit_depth == format2->bit_depth &&
            format1->is_float == format2->is_float);
}

/**
 * @brief 오디오 포맷을 다른 포맷으로 변환합니다
 */
ETResult et_audio_format_convert(const ETAudioFormat* src_format,
                                const ETAudioFormat* dst_format,
                                const void* src_buffer,
                                void* dst_buffer,
                                uint32_t num_frames) {
    if (!src_format || !dst_format || !src_buffer || !dst_buffer) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "포인터가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (num_frames == 0) {
        return ET_SUCCESS;
    }

    // 현재는 간단한 변환만 지원 (float <-> int16)
    if (src_format->is_float && !dst_format->is_float && dst_format->bit_depth == 16) {
        // float -> int16 변환
        const float* src = (const float*)src_buffer;
        int16_t* dst = (int16_t*)dst_buffer;

        uint32_t total_samples = num_frames * src_format->num_channels;
        for (uint32_t i = 0; i < total_samples; i++) {
            float sample = src[i];
            // 클리핑
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            // 변환
            dst[i] = (int16_t)(sample * 32767.0f);
        }
    }
    else if (!src_format->is_float && src_format->bit_depth == 16 && dst_format->is_float) {
        // int16 -> float 변환
        const int16_t* src = (const int16_t*)src_buffer;
        float* dst = (float*)dst_buffer;

        uint32_t total_samples = num_frames * src_format->num_channels;
        for (uint32_t i = 0; i < total_samples; i++) {
            dst[i] = (float)src[i] / 32767.0f;
        }
    }
    else if (et_audio_format_compatible(src_format, dst_format)) {
        // 호환되는 포맷이면 단순 복사
        size_t bytes_to_copy = num_frames * src_format->frame_size;
        memcpy(dst_buffer, src_buffer, bytes_to_copy);
    }
    else {
        ET_SET_ERROR(ET_ERROR_NOT_IMPLEMENTED, "지원되지 않는 포맷 변환입니다");
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 오디오 버퍼 관리 함수
// ============================================================================

/**
 * @brief 오디오 버퍼를 생성합니다
 */
ETAudioBuffer* et_audio_buffer_create(uint32_t size, uint16_t channels, bool is_circular) {
    if (size == 0 || channels == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "잘못된 버퍼 크기 또는 채널 수");
        return NULL;
    }

    ETAudioBuffer* buffer = (ETAudioBuffer*)malloc(sizeof(ETAudioBuffer));
    if (!buffer) {
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "버퍼 구조체 메모리 할당 실패");
        return NULL;
    }

    buffer->data = (float*)calloc(size * channels, sizeof(float));
    if (!buffer->data) {
        free(buffer);
        ET_SET_ERROR(ET_ERROR_OUT_OF_MEMORY, "버퍼 데이터 메모리 할당 실패");
        return NULL;
    }

    buffer->size = size;
    buffer->channels = channels;
    buffer->write_pos = 0;
    buffer->read_pos = 0;
    buffer->available = 0;
    buffer->is_full = false;
    buffer->is_circular = is_circular;

    return buffer;
}

/**
 * @brief 오디오 버퍼를 해제합니다
 */
void et_audio_buffer_destroy(ETAudioBuffer* buffer) {
    if (buffer) {
        if (buffer->data) {
            free(buffer->data);
        }
        free(buffer);
    }
}

/**
 * @brief 오디오 버퍼에 데이터를 씁니다
 */
uint32_t et_audio_buffer_write(ETAudioBuffer* buffer, const float* data, uint32_t num_frames) {
    if (!buffer || !data || num_frames == 0) {
        return 0;
    }

    uint32_t frames_written = 0;
    uint32_t available_space = et_audio_buffer_available_space(buffer);
    uint32_t frames_to_write = (num_frames > available_space) ? available_space : num_frames;

    for (uint32_t i = 0; i < frames_to_write; i++) {
        for (uint16_t ch = 0; ch < buffer->channels; ch++) {
            buffer->data[buffer->write_pos * buffer->channels + ch] =
                data[i * buffer->channels + ch];
        }

        buffer->write_pos++;
        if (buffer->is_circular && buffer->write_pos >= buffer->size) {
            buffer->write_pos = 0;
        }

        frames_written++;
    }

    buffer->available += frames_written;
    if (buffer->available >= buffer->size) {
        buffer->is_full = true;
        buffer->available = buffer->size;
    }

    return frames_written;
}

/**
 * @brief 오디오 버퍼에서 데이터를 읽습니다
 */
uint32_t et_audio_buffer_read(ETAudioBuffer* buffer, float* data, uint32_t num_frames) {
    if (!buffer || !data || num_frames == 0) {
        return 0;
    }

    uint32_t frames_read = 0;
    uint32_t available_data = et_audio_buffer_available_data(buffer);
    uint32_t frames_to_read = (num_frames > available_data) ? available_data : num_frames;

    for (uint32_t i = 0; i < frames_to_read; i++) {
        for (uint16_t ch = 0; ch < buffer->channels; ch++) {
            data[i * buffer->channels + ch] =
                buffer->data[buffer->read_pos * buffer->channels + ch];
        }

        buffer->read_pos++;
        if (buffer->is_circular && buffer->read_pos >= buffer->size) {
            buffer->read_pos = 0;
        }

        frames_read++;
    }

    buffer->available -= frames_read;
    buffer->is_full = false;

    return frames_read;
}

/**
 * @brief 오디오 버퍼를 리셋합니다
 */
void et_audio_buffer_reset(ETAudioBuffer* buffer) {
    if (!buffer) {
        return;
    }

    buffer->write_pos = 0;
    buffer->read_pos = 0;
    buffer->available = 0;
    buffer->is_full = false;

    // 버퍼 데이터 초기화
    if (buffer->data) {
        memset(buffer->data, 0, buffer->size * buffer->channels * sizeof(float));
    }
}

/**
 * @brief 오디오 버퍼의 사용 가능한 공간을 조회합니다
 */
uint32_t et_audio_buffer_available_space(const ETAudioBuffer* buffer) {
    if (!buffer) {
        return 0;
    }

    return buffer->size - buffer->available;
}

/**
 * @brief 오디오 버퍼의 사용 가능한 데이터를 조회합니다
 */
uint32_t et_audio_buffer_available_data(const ETAudioBuffer* buffer) {
    if (!buffer) {
        return 0;
    }

    return buffer->available;
}

// ============================================================================
// 오디오 처리 유틸리티 함수
// ============================================================================

/**
 * @brief 오디오 버퍼 클리핑 (범위: -1.0 ~ 1.0)
 */
void et_audio_clip_buffer(float* buffer, uint32_t num_samples) {
    if (!buffer || num_samples == 0) {
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        if (buffer[i] > 1.0f) {
            buffer[i] = 1.0f;
        } else if (buffer[i] < -1.0f) {
            buffer[i] = -1.0f;
        }
    }
}

/**
 * @brief 오디오 버퍼 볼륨 조절
 */
void et_audio_apply_volume(float* buffer, uint32_t num_samples, float volume) {
    if (!buffer || num_samples == 0) {
        return;
    }

    // 볼륨 클리핑
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 2.0f) volume = 2.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        buffer[i] *= volume;
    }
}

/**
 * @brief 오디오 버퍼 믹싱
 */
void et_audio_mix_buffers(float* dest, const float* src, uint32_t num_samples, float mix_ratio) {
    if (!dest || !src || num_samples == 0) {
        return;
    }

    // 믹싱 비율 클리핑
    if (mix_ratio < 0.0f) mix_ratio = 0.0f;
    if (mix_ratio > 1.0f) mix_ratio = 1.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        dest[i] = dest[i] * (1.0f - mix_ratio) + src[i] * mix_ratio;
    }
}

/**
 * @brief 오디오 버퍼 페이드 인/아웃
 */
void et_audio_fade_buffer(float* buffer, uint32_t num_samples, bool fade_in) {
    if (!buffer || num_samples == 0) {
        return;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        float factor;
        if (fade_in) {
            factor = (float)i / (float)(num_samples - 1);
        } else {
            factor = (float)(num_samples - 1 - i) / (float)(num_samples - 1);
        }
        buffer[i] *= factor;
    }
}

/**
 * @brief 오디오 버퍼 무음 처리
 */
void et_audio_silence_buffer(float* buffer, uint32_t num_samples) {
    if (!buffer || num_samples == 0) {
        return;
    }

    memset(buffer, 0, num_samples * sizeof(float));
}

/**
 * @brief 오디오 버퍼 RMS 레벨 계산
 */
float et_audio_calculate_rms(const float* buffer, uint32_t num_samples) {
    if (!buffer || num_samples == 0) {
        return 0.0f;
    }

    double sum = 0.0;
    for (uint32_t i = 0; i < num_samples; i++) {
        sum += (double)buffer[i] * (double)buffer[i];
    }

    return (float)sqrt(sum / (double)num_samples);
}

/**
 * @brief 오디오 버퍼 피크 레벨 계산
 */
float et_audio_calculate_peak(const float* buffer, uint32_t num_samples) {
    if (!buffer || num_samples == 0) {
        return 0.0f;
    }

    float peak = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        float abs_sample = fabsf(buffer[i]);
        if (abs_sample > peak) {
            peak = abs_sample;
        }
    }

    return peak;
}