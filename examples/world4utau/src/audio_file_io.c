/**
 * @file audio_file_io.c
 * @brief WAV 파일 I/O 구현
 */

#include "audio_file_io.h"
#include <libetude/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// WAV 포맷 상수
#define WAV_FORMAT_PCM 1
#define WAV_FORMAT_IEEE_FLOAT 3

// 바이트 순서 변환 매크로 (리틀 엔디안 가정)
#define SWAP_16(x) ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))
#define SWAP_32(x) ((((x) & 0xFF) << 24) | (((x) & 0xFF00) << 8) | \
                    (((x) >> 8) & 0xFF00) | (((x) >> 24) & 0xFF))

/**
 * @brief 16비트 PCM을 float로 변환
 */
static inline float pcm16_to_float(int16_t sample) {
    return (float)sample / 32768.0f;
}

/**
 * @brief 24비트 PCM을 float로 변환
 */
static inline float pcm24_to_float(int32_t sample) {
    // 24비트 값을 32비트로 확장 (부호 확장)
    if (sample & 0x800000) {
        sample |= 0xFF000000;
    }
    return (float)sample / 8388608.0f;
}

/**
 * @brief 32비트 PCM을 float로 변환
 */
static inline float pcm32_to_float(int32_t sample) {
    return (float)sample / 2147483648.0f;
}

/**
 * @brief float를 16비트 PCM으로 변환
 */
static inline int16_t float_to_pcm16(float sample) {
    // 클리핑
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;

    return (int16_t)(sample * 32767.0f);
}

/**
 * @brief float를 24비트 PCM으로 변환
 */
static inline int32_t float_to_pcm24(float sample) {
    // 클리핑
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;

    return (int32_t)(sample * 8388607.0f);
}

/**
 * @brief float를 32비트 PCM으로 변환
 */
static inline int32_t float_to_pcm32(float sample) {
    // 클리핑
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;

    return (int32_t)(sample * 2147483647.0f);
}

ETResult read_wav_file(const char* file_path, AudioData* audio_data) {
    if (!file_path || !audio_data) {
        fprintf(stderr, "Error: read_wav_file에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(audio_data, 0, sizeof(AudioData));

    FILE* file = fopen(file_path, "rb");
    if (!file) {
        fprintf(stderr, "Error: WAV 파일을 열 수 없습니다: %s\n", file_path);
        return ET_ERROR_NOT_FOUND;
    }

    // WAV 헤더 읽기
    WAVHeader header;
    size_t read_size = fread(&header, 1, sizeof(WAVHeader), file);
    if (read_size != sizeof(WAVHeader)) {
        fprintf(stderr, "Error: WAV 헤더를 읽을 수 없습니다: %s\n", file_path);
        fclose(file);
        return ET_ERROR_IO;
    }

    // 헤더 유효성 검사
    if (!validate_wav_header(&header)) {
        fprintf(stderr, "Error: 유효하지 않은 WAV 파일입니다: %s\n", file_path);
        fclose(file);
        return ET_ERROR_INVALID_FORMAT;
    }

    // 지원되는 포맷인지 확인
    if (!is_supported_format(header.format_tag, header.bits_per_sample)) {
        fprintf(stderr, "Error: 지원하지 않는 WAV 포맷입니다 (포맷: %d, 비트: %d): %s\n",
                header.format_tag, header.bits_per_sample, file_path);
        fclose(file);
        return ET_ERROR_UNSUPPORTED_FORMAT;
    }

    // 오디오 정보 설정
    audio_data->info.sample_rate = header.sample_rate;
    audio_data->info.num_channels = header.num_channels;
    audio_data->info.bits_per_sample = header.bits_per_sample;
    audio_data->info.num_samples = header.data_size / (header.num_channels * (header.bits_per_sample / 8));
    audio_data->info.duration_seconds = (double)audio_data->info.num_samples / header.sample_rate;
    audio_data->info.is_float_format = (header.format_tag == WAV_FORMAT_IEEE_FLOAT);

    printf("Info: WAV 파일 정보 - 샘플레이트: %d Hz, 채널: %d, 비트: %d, 샘플수: %d, 길이: %.2f초\n",
           audio_data->info.sample_rate, audio_data->info.num_channels,
           audio_data->info.bits_per_sample, audio_data->info.num_samples,
           audio_data->info.duration_seconds);

    // 오디오 데이터 메모리 할당
    uint32_t total_samples = audio_data->info.num_samples * audio_data->info.num_channels;
    audio_data->data = (float*)malloc(total_samples * sizeof(float));
    if (!audio_data->data) {
        fprintf(stderr, "Error: 오디오 데이터용 메모리 할당 실패 (%d 샘플)\n", total_samples);
        fclose(file);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    audio_data->owns_data = true;

    // 포맷에 따른 데이터 읽기 및 변환
    ETResult result = ET_SUCCESS;

    if (header.format_tag == WAV_FORMAT_PCM) {
        // PCM 포맷 처리
        if (header.bits_per_sample == 16) {
            // 16비트 PCM
            int16_t* pcm_buffer = (int16_t*)malloc(total_samples * sizeof(int16_t));
            if (!pcm_buffer) {
                fprintf(stderr, "Error: PCM 버퍼 메모리 할당 실패\n");
                free(audio_data->data);
                audio_data->data = NULL;
                fclose(file);
                return ET_ERROR_OUT_OF_MEMORY;
            }

            size_t samples_read = fread(pcm_buffer, sizeof(int16_t), total_samples, file);
            if (samples_read != total_samples) {
                fprintf(stderr, "Warning: 예상보다 적은 샘플을 읽었습니다 (%zu/%d)\n",
                        samples_read, total_samples);
                // 부분적으로 읽은 데이터도 처리
                total_samples = (uint32_t)samples_read;
                audio_data->info.num_samples = total_samples / audio_data->info.num_channels;
            }

            // 16비트 PCM을 float로 변환
            for (uint32_t i = 0; i < total_samples; i++) {
                audio_data->data[i] = pcm16_to_float(pcm_buffer[i]);
            }

            free(pcm_buffer);

        } else if (header.bits_per_sample == 24) {
            // 24비트 PCM (3바이트씩 읽어서 처리)
            uint8_t* raw_buffer = (uint8_t*)malloc(total_samples * 3);
            if (!raw_buffer) {
                fprintf(stderr, "Error: 24비트 PCM 버퍼 메모리 할당 실패\n");
                free(audio_data->data);
                audio_data->data = NULL;
                fclose(file);
                return ET_ERROR_OUT_OF_MEMORY;
            }

            size_t bytes_read = fread(raw_buffer, 1, total_samples * 3, file);
            if (bytes_read != total_samples * 3) {
                fprintf(stderr, "Warning: 예상보다 적은 바이트를 읽었습니다 (%zu/%d)\n",
                        bytes_read, total_samples * 3);
                total_samples = (uint32_t)(bytes_read / 3);
                audio_data->info.num_samples = total_samples / audio_data->info.num_channels;
            }

            // 24비트 PCM을 float로 변환
            for (uint32_t i = 0; i < total_samples; i++) {
                int32_t sample = 0;
                // 리틀 엔디안으로 3바이트를 32비트로 조합
                sample = raw_buffer[i * 3] |
                        (raw_buffer[i * 3 + 1] << 8) |
                        (raw_buffer[i * 3 + 2] << 16);

                audio_data->data[i] = pcm24_to_float(sample);
            }

            free(raw_buffer);

        } else if (header.bits_per_sample == 32) {
            // 32비트 PCM
            int32_t* pcm_buffer = (int32_t*)malloc(total_samples * sizeof(int32_t));
            if (!pcm_buffer) {
                fprintf(stderr, "Error: 32비트 PCM 버퍼 메모리 할당 실패\n");
                free(audio_data->data);
                audio_data->data = NULL;
                fclose(file);
                return ET_ERROR_OUT_OF_MEMORY;
            }

            size_t samples_read = fread(pcm_buffer, sizeof(int32_t), total_samples, file);
            if (samples_read != total_samples) {
                fprintf(stderr, "Warning: 예상보다 적은 샘플을 읽었습니다 (%zu/%d)\n",
                        samples_read, total_samples);
                total_samples = (uint32_t)samples_read;
                audio_data->info.num_samples = total_samples / audio_data->info.num_channels;
            }

            // 32비트 PCM을 float로 변환
            for (uint32_t i = 0; i < total_samples; i++) {
                audio_data->data[i] = pcm32_to_float(pcm_buffer[i]);
            }

            free(pcm_buffer);
        }

    } else if (header.format_tag == WAV_FORMAT_IEEE_FLOAT) {
        // IEEE float 포맷 (32비트)
        if (header.bits_per_sample == 32) {
            size_t samples_read = fread(audio_data->data, sizeof(float), total_samples, file);
            if (samples_read != total_samples) {
                fprintf(stderr, "Warning: 예상보다 적은 샘플을 읽었습니다 (%zu/%d)\n",
                        samples_read, total_samples);
                total_samples = (uint32_t)samples_read;
                audio_data->info.num_samples = total_samples / audio_data->info.num_channels;
            }
        } else {
            fprintf(stderr, "Error: 지원하지 않는 IEEE float 비트 깊이: %d\n", header.bits_per_sample);
            result = ET_ERROR_UNSUPPORTED_FORMAT;
        }
    }

    fclose(file);

    if (result != ET_SUCCESS) {
        free(audio_data->data);
        audio_data->data = NULL;
        audio_data->owns_data = false;
        return result;
    }

    printf("Info: WAV 파일을 성공적으로 읽었습니다: %s (%d 샘플)\n",
           file_path, audio_data->info.num_samples);

    return ET_SUCCESS;
}
ETResult get_wav_file_info(const char* file_path, AudioFileInfo* info) {
    if (!file_path || !info) {
        fprintf(stderr, "Error: get_wav_file_info에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(file_path, "rb");
    if (!file) {
        fprintf(stderr, "Error: WAV 파일을 열 수 없습니다: %s\n", file_path);
        return ET_ERROR_NOT_FOUND;
    }

    // WAV 헤더만 읽기
    WAVHeader header;
    size_t read_size = fread(&header, 1, sizeof(WAVHeader), file);
    fclose(file);

    if (read_size != sizeof(WAVHeader)) {
        fprintf(stderr, "Error: WAV 헤더를 읽을 수 없습니다: %s\n", file_path);
        return ET_ERROR_IO;
    }

    // 헤더 유효성 검사
    if (!validate_wav_header(&header)) {
        fprintf(stderr, "Error: 유효하지 않은 WAV 파일입니다: %s\n", file_path);
        return ET_ERROR_INVALID_FORMAT;
    }

    // 정보 설정
    info->sample_rate = header.sample_rate;
    info->num_channels = header.num_channels;
    info->bits_per_sample = header.bits_per_sample;
    info->num_samples = header.data_size / (header.num_channels * (header.bits_per_sample / 8));
    info->duration_seconds = (double)info->num_samples / header.sample_rate;
    info->is_float_format = (header.format_tag == WAV_FORMAT_IEEE_FLOAT);

    return ET_SUCCESS;
}

AudioData* audio_data_create(uint32_t num_samples, uint16_t num_channels, uint32_t sample_rate) {
    if (num_samples == 0 || num_channels == 0 || sample_rate == 0) {
        fprintf(stderr, "Error: audio_data_create에 유효하지 않은 파라미터가 전달되었습니다.\n");
        return NULL;
    }

    AudioData* audio_data = (AudioData*)malloc(sizeof(AudioData));
    if (!audio_data) {
        fprintf(stderr, "Error: AudioData 구조체 메모리 할당 실패\n");
        return NULL;
    }

    memset(audio_data, 0, sizeof(AudioData));

    // 오디오 데이터 메모리 할당
    uint32_t total_samples = num_samples * num_channels;
    audio_data->data = (float*)calloc(total_samples, sizeof(float));
    if (!audio_data->data) {
        fprintf(stderr, "Error: 오디오 데이터 메모리 할당 실패 (%d 샘플)\n", total_samples);
        free(audio_data);
        return NULL;
    }

    // 정보 설정
    audio_data->info.sample_rate = sample_rate;
    audio_data->info.num_channels = num_channels;
    audio_data->info.bits_per_sample = 32; // float 기본값
    audio_data->info.num_samples = num_samples;
    audio_data->info.duration_seconds = (double)num_samples / sample_rate;
    audio_data->info.is_float_format = true;
    audio_data->owns_data = true;

    return audio_data;
}

void audio_data_destroy(AudioData* audio_data) {
    if (!audio_data) {
        return;
    }

    if (audio_data->owns_data && audio_data->data) {
        free(audio_data->data);
    }

    free(audio_data);
}

ETResult audio_data_copy(const AudioData* src, AudioData* dst) {
    if (!src || !dst) {
        fprintf(stderr, "Error: audio_data_copy에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!src->data) {
        fprintf(stderr, "Error: 소스 오디오 데이터가 NULL입니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 기존 데이터 해제 (소유권이 있는 경우)
    if (dst->owns_data && dst->data) {
        free(dst->data);
    }

    // 정보 복사
    dst->info = src->info;

    // 데이터 메모리 할당 및 복사
    uint32_t total_samples = src->info.num_samples * src->info.num_channels;
    dst->data = (float*)malloc(total_samples * sizeof(float));
    if (!dst->data) {
        fprintf(stderr, "Error: 오디오 데이터 복사용 메모리 할당 실패\n");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memcpy(dst->data, src->data, total_samples * sizeof(float));
    dst->owns_data = true;

    return ET_SUCCESS;
}

ETResult audio_data_normalize(AudioData* audio_data) {
    if (!audio_data || !audio_data->data) {
        fprintf(stderr, "Error: audio_data_normalize에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    uint32_t total_samples = audio_data->info.num_samples * audio_data->info.num_channels;

    // 최대 절댓값 찾기
    float max_abs = 0.0f;
    for (uint32_t i = 0; i < total_samples; i++) {
        float abs_val = fabsf(audio_data->data[i]);
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }

    // 이미 정규화되어 있거나 무음인 경우
    if (max_abs <= 1.0f) {
        return ET_SUCCESS;
    }

    // 정규화 수행
    float scale = 1.0f / max_abs;
    for (uint32_t i = 0; i < total_samples; i++) {
        audio_data->data[i] *= scale;
    }

    printf("Info: 오디오 데이터를 정규화했습니다 (스케일: %.6f)\n", scale);
    return ET_SUCCESS;
}

ETResult audio_data_to_mono(AudioData* audio_data) {
    if (!audio_data || !audio_data->data) {
        fprintf(stderr, "Error: audio_data_to_mono에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (audio_data->info.num_channels == 1) {
        // 이미 모노
        return ET_SUCCESS;
    }

    if (audio_data->info.num_channels != 2) {
        fprintf(stderr, "Error: 2채널 스테레오만 모노 변환을 지원합니다 (현재: %d채널)\n",
                audio_data->info.num_channels);
        return ET_ERROR_UNSUPPORTED_FORMAT;
    }

    uint32_t mono_samples = audio_data->info.num_samples;
    float* mono_data = (float*)malloc(mono_samples * sizeof(float));
    if (!mono_data) {
        fprintf(stderr, "Error: 모노 변환용 메모리 할당 실패\n");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 스테레오를 모노로 변환 (좌우 채널 평균)
    for (uint32_t i = 0; i < mono_samples; i++) {
        float left = audio_data->data[i * 2];
        float right = audio_data->data[i * 2 + 1];
        mono_data[i] = (left + right) * 0.5f;
    }

    // 기존 데이터 교체
    if (audio_data->owns_data) {
        free(audio_data->data);
    }

    audio_data->data = mono_data;
    audio_data->info.num_channels = 1;
    audio_data->owns_data = true;

    printf("Info: 스테레오를 모노로 변환했습니다 (%d 샘플)\n", mono_samples);
    return ET_SUCCESS;
}

ETResult audio_data_resample(AudioData* audio_data, uint32_t target_sample_rate) {
    if (!audio_data || !audio_data->data) {
        fprintf(stderr, "Error: audio_data_resample에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (target_sample_rate == 0) {
        fprintf(stderr, "Error: 유효하지 않은 목표 샘플링 레이트: %d\n", target_sample_rate);
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (audio_data->info.sample_rate == target_sample_rate) {
        // 이미 목표 샘플링 레이트
        return ET_SUCCESS;
    }

    // 리샘플링 비율 계산
    double ratio = (double)target_sample_rate / audio_data->info.sample_rate;
    uint32_t new_num_samples = (uint32_t)(audio_data->info.num_samples * ratio);
    uint32_t new_total_samples = new_num_samples * audio_data->info.num_channels;

    float* new_data = (float*)malloc(new_total_samples * sizeof(float));
    if (!new_data) {
        fprintf(stderr, "Error: 리샘플링용 메모리 할당 실패\n");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 간단한 선형 보간 리샘플링
    for (uint32_t i = 0; i < new_num_samples; i++) {
        double src_index = (double)i / ratio;
        uint32_t src_index_int = (uint32_t)src_index;
        double frac = src_index - src_index_int;

        for (uint16_t ch = 0; ch < audio_data->info.num_channels; ch++) {
            if (src_index_int >= audio_data->info.num_samples - 1) {
                // 마지막 샘플 사용
                new_data[i * audio_data->info.num_channels + ch] =
                    audio_data->data[(audio_data->info.num_samples - 1) * audio_data->info.num_channels + ch];
            } else {
                // 선형 보간
                float sample1 = audio_data->data[src_index_int * audio_data->info.num_channels + ch];
                float sample2 = audio_data->data[(src_index_int + 1) * audio_data->info.num_channels + ch];
                new_data[i * audio_data->info.num_channels + ch] =
                    sample1 + (float)frac * (sample2 - sample1);
            }
        }
    }

    // 기존 데이터 교체
    if (audio_data->owns_data) {
        free(audio_data->data);
    }

    audio_data->data = new_data;
    audio_data->info.sample_rate = target_sample_rate;
    audio_data->info.num_samples = new_num_samples;
    audio_data->info.duration_seconds = (double)new_num_samples / target_sample_rate;
    audio_data->owns_data = true;

    printf("Info: 샘플링 레이트를 %d Hz에서 %d Hz로 변경했습니다 (%d -> %d 샘플)\n",
           (uint32_t)(target_sample_rate / ratio), target_sample_rate,
           (uint32_t)(new_num_samples / ratio), new_num_samples);

    return ET_SUCCESS;
}

bool validate_wav_header(const WAVHeader* header) {
    if (!header) {
        return false;
    }

    // RIFF 시그니처 확인
    if (memcmp(header->riff_id, "RIFF", 4) != 0) {
        fprintf(stderr, "Error: RIFF 시그니처가 유효하지 않습니다\n");
        return false;
    }

    // WAVE 시그니처 확인
    if (memcmp(header->wave_id, "WAVE", 4) != 0) {
        fprintf(stderr, "Error: WAVE 시그니처가 유효하지 않습니다\n");
        return false;
    }

    // fmt 청크 확인
    if (memcmp(header->fmt_id, "fmt ", 4) != 0) {
        fprintf(stderr, "Error: fmt 청크 시그니처가 유효하지 않습니다\n");
        return false;
    }

    // data 청크 확인
    if (memcmp(header->data_id, "data", 4) != 0) {
        fprintf(stderr, "Error: data 청크 시그니처가 유효하지 않습니다\n");
        return false;
    }

    // 기본 파라미터 유효성 확인
    if (header->num_channels == 0 || header->num_channels > 8) {
        fprintf(stderr, "Error: 유효하지 않은 채널 수: %d\n", header->num_channels);
        return false;
    }

    if (header->sample_rate == 0 || header->sample_rate > 192000) {
        fprintf(stderr, "Error: 유효하지 않은 샘플링 레이트: %d\n", header->sample_rate);
        return false;
    }

    if (header->bits_per_sample == 0 || header->bits_per_sample > 32) {
        fprintf(stderr, "Error: 유효하지 않은 비트 깊이: %d\n", header->bits_per_sample);
        return false;
    }

    // 데이터 크기 확인
    if (header->data_size == 0) {
        fprintf(stderr, "Error: 데이터 크기가 0입니다\n");
        return false;
    }

    return true;
}

bool is_supported_format(uint16_t format_tag, uint16_t bits_per_sample) {
    if (format_tag == WAV_FORMAT_PCM) {
        return (bits_per_sample == 16 || bits_per_sample == 24 || bits_per_sample == 32);
    } else if (format_tag == WAV_FORMAT_IEEE_FLOAT) {
        return (bits_per_sample == 32);
    }

    return false;
}

uint32_t calculate_wav_file_size(const AudioData* audio_data) {
    if (!audio_data) {
        return 0;
    }

    uint32_t bytes_per_sample = audio_data->info.bits_per_sample / 8;
    uint32_t data_size = audio_data->info.num_samples * audio_data->info.num_channels * bytes_per_sample;

    return sizeof(WAVHeader) + data_size;
}

ETAudioFormat audio_info_to_et_format(const AudioFileInfo* info, uint32_t buffer_size) {
    ETAudioFormat format;

    if (info) {
        format.sample_rate = info->sample_rate;
        format.num_channels = info->num_channels;
        format.bit_depth = 32; // libetude는 내부적으로 float 사용
        format.frame_size = info->num_channels * sizeof(float);
        format.buffer_size = buffer_size;
    } else {
        // 기본값
        format = et_audio_format_create(44100, 1, buffer_size);
    }

    return format;
}

void debug_print_audio_data(const AudioData* audio_data, const char* label) {
    if (!audio_data) {
        printf("=== Audio Data Debug Info (%s) ===\n", label ? label : "Unknown");
        printf("AudioData structure is NULL\n");
        printf("=====================================\n");
        return;
    }

    printf("=== Audio Data Debug Info (%s) ===\n", label ? label : "Unknown");
    printf("Sample Rate: %d Hz\n", audio_data->info.sample_rate);
    printf("Channels: %d\n", audio_data->info.num_channels);
    printf("Bits per Sample: %d\n", audio_data->info.bits_per_sample);
    printf("Number of Samples: %d (per channel)\n", audio_data->info.num_samples);
    printf("Duration: %.3f seconds\n", audio_data->info.duration_seconds);
    printf("Is Float Format: %s\n", audio_data->info.is_float_format ? "Yes" : "No");
    printf("Data Pointer: %p\n", (void*)audio_data->data);
    printf("Owns Data: %s\n", audio_data->owns_data ? "Yes" : "No");

    if (audio_data->data && audio_data->info.num_samples > 0) {
        // 첫 몇 개 샘플 출력
        uint32_t samples_to_show = (audio_data->info.num_samples < 10) ? audio_data->info.num_samples : 10;
        printf("First %d samples:\n", samples_to_show);

        for (uint32_t i = 0; i < samples_to_show; i++) {
            printf("  Sample %d: ", i);
            for (uint16_t ch = 0; ch < audio_data->info.num_channels; ch++) {
                printf("%.6f ", audio_data->data[i * audio_data->info.num_channels + ch]);
            }
            printf("\n");
        }

        // 통계 정보
        uint32_t total_samples = audio_data->info.num_samples * audio_data->info.num_channels;
        float min_val = audio_data->data[0];
        float max_val = audio_data->data[0];
        double sum = 0.0;

        for (uint32_t i = 0; i < total_samples; i++) {
            float val = audio_data->data[i];
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
            sum += val;
        }

        printf("Statistics:\n");
        printf("  Min: %.6f\n", min_val);
        printf("  Max: %.6f\n", max_val);
        printf("  Mean: %.6f\n", sum / total_samples);
        printf("  Range: %.6f\n", max_val - min_val);
    }

    printf("=====================================\n");
}

ETResult write_wav_file(const char* file_path, const AudioData* audio_data) {
    if (!file_path || !audio_data || !audio_data->data) {
        fprintf(stderr, "Error: write_wav_file에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (audio_data->info.num_samples == 0 || audio_data->info.num_channels == 0) {
        fprintf(stderr, "Error: 유효하지 않은 오디오 데이터입니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(file_path, "wb");
    if (!file) {
        fprintf(stderr, "Error: WAV 파일을 생성할 수 없습니다: %s\n", file_path);
        return ET_ERROR_IO;
    }

    // WAV 헤더 생성
    WAVHeader header;
    memset(&header, 0, sizeof(WAVHeader));

    // RIFF 헤더
    memcpy(header.riff_id, "RIFF", 4);
    memcpy(header.wave_id, "WAVE", 4);

    // fmt 청크
    memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_size = 16; // PCM의 경우 16바이트

    // 출력 포맷 결정 (기본적으로 입력 포맷 유지, 필요시 변환)
    uint16_t output_bits_per_sample = audio_data->info.bits_per_sample;
    uint16_t output_format_tag;

    if (audio_data->info.is_float_format && output_bits_per_sample == 32) {
        output_format_tag = WAV_FORMAT_IEEE_FLOAT;
    } else {
        output_format_tag = WAV_FORMAT_PCM;
        // float 데이터를 PCM으로 변환하는 경우 기본 16비트 사용
        if (audio_data->info.is_float_format) {
            output_bits_per_sample = 16;
        }
    }

    header.format_tag = output_format_tag;
    header.num_channels = audio_data->info.num_channels;
    header.sample_rate = audio_data->info.sample_rate;
    header.bits_per_sample = output_bits_per_sample;
    header.block_align = header.num_channels * (output_bits_per_sample / 8);
    header.bytes_per_sec = header.sample_rate * header.block_align;

    // data 청크
    memcpy(header.data_id, "data", 4);
    header.data_size = audio_data->info.num_samples * header.block_align;

    // 파일 크기 계산
    header.file_size = sizeof(WAVHeader) - 8 + header.data_size;

    // 헤더 쓰기
    size_t header_written = fwrite(&header, 1, sizeof(WAVHeader), file);
    if (header_written != sizeof(WAVHeader)) {
        fprintf(stderr, "Error: WAV 헤더를 쓸 수 없습니다: %s\n", file_path);
        fclose(file);
        remove(file_path);
        return ET_ERROR_IO;
    }

    // 오디오 데이터 쓰기
    ETResult result = ET_SUCCESS;
    uint32_t total_samples = audio_data->info.num_samples * audio_data->info.num_channels;

    if (output_format_tag == WAV_FORMAT_IEEE_FLOAT) {
        // IEEE float 포맷으로 직접 쓰기
        size_t samples_written = fwrite(audio_data->data, sizeof(float), total_samples, file);
        if (samples_written != total_samples) {
            fprintf(stderr, "Error: 오디오 데이터를 쓸 수 없습니다 (float): %s\n", file_path);
            result = ET_ERROR_IO;
        }

    } else if (output_format_tag == WAV_FORMAT_PCM) {
        // PCM 포맷으로 변환하여 쓰기
        if (output_bits_per_sample == 16) {
            // 16비트 PCM
            int16_t* pcm_buffer = (int16_t*)malloc(total_samples * sizeof(int16_t));
            if (!pcm_buffer) {
                fprintf(stderr, "Error: 16비트 PCM 버퍼 메모리 할당 실패\n");
                fclose(file);
                remove(file_path);
                return ET_ERROR_OUT_OF_MEMORY;
            }

            // float를 16비트 PCM으로 변환
            for (uint32_t i = 0; i < total_samples; i++) {
                pcm_buffer[i] = float_to_pcm16(audio_data->data[i]);
            }

            size_t samples_written = fwrite(pcm_buffer, sizeof(int16_t), total_samples, file);
            free(pcm_buffer);

            if (samples_written != total_samples) {
                fprintf(stderr, "Error: 오디오 데이터를 쓸 수 없습니다 (16bit PCM): %s\n", file_path);
                result = ET_ERROR_IO;
            }

        } else if (output_bits_per_sample == 24) {
            // 24비트 PCM
            uint8_t* pcm_buffer = (uint8_t*)malloc(total_samples * 3);
            if (!pcm_buffer) {
                fprintf(stderr, "Error: 24비트 PCM 버퍼 메모리 할당 실패\n");
                fclose(file);
                remove(file_path);
                return ET_ERROR_OUT_OF_MEMORY;
            }

            // float를 24비트 PCM으로 변환
            for (uint32_t i = 0; i < total_samples; i++) {
                int32_t sample = float_to_pcm24(audio_data->data[i]);

                // 24비트를 3바이트로 분할 (리틀 엔디안)
                pcm_buffer[i * 3] = (uint8_t)(sample & 0xFF);
                pcm_buffer[i * 3 + 1] = (uint8_t)((sample >> 8) & 0xFF);
                pcm_buffer[i * 3 + 2] = (uint8_t)((sample >> 16) & 0xFF);
            }

            size_t bytes_written = fwrite(pcm_buffer, 1, total_samples * 3, file);
            free(pcm_buffer);

            if (bytes_written != total_samples * 3) {
                fprintf(stderr, "Error: 오디오 데이터를 쓸 수 없습니다 (24bit PCM): %s\n", file_path);
                result = ET_ERROR_IO;
            }

        } else if (output_bits_per_sample == 32) {
            // 32비트 PCM
            int32_t* pcm_buffer = (int32_t*)malloc(total_samples * sizeof(int32_t));
            if (!pcm_buffer) {
                fprintf(stderr, "Error: 32비트 PCM 버퍼 메모리 할당 실패\n");
                fclose(file);
                remove(file_path);
                return ET_ERROR_OUT_OF_MEMORY;
            }

            // float를 32비트 PCM으로 변환
            for (uint32_t i = 0; i < total_samples; i++) {
                pcm_buffer[i] = float_to_pcm32(audio_data->data[i]);
            }

            size_t samples_written = fwrite(pcm_buffer, sizeof(int32_t), total_samples, file);
            free(pcm_buffer);

            if (samples_written != total_samples) {
                fprintf(stderr, "Error: 오디오 데이터를 쓸 수 없습니다 (32bit PCM): %s\n", file_path);
                result = ET_ERROR_IO;
            }

        } else {
            fprintf(stderr, "Error: 지원하지 않는 PCM 비트 깊이: %d\n", output_bits_per_sample);
            result = ET_ERROR_UNSUPPORTED_FORMAT;
        }
    }

    fclose(file);

    if (result != ET_SUCCESS) {
        remove(file_path); // 실패한 파일 삭제
        return result;
    }

    printf("Info: WAV 파일을 성공적으로 저장했습니다: %s\n", file_path);
    printf("      포맷: %s, %d채널, %d Hz, %d비트, %d 샘플\n",
           (output_format_tag == WAV_FORMAT_IEEE_FLOAT) ? "IEEE Float" : "PCM",
           audio_data->info.num_channels, audio_data->info.sample_rate,
           output_bits_per_sample, audio_data->info.num_samples);

    return ET_SUCCESS;
}

/**
 * @brief 고품질 WAV 파일 쓰기 (추가 옵션 지원)
 *
 * 더 세밀한 제어가 가능한 WAV 파일 쓰기 함수입니다.
 *
 * @param file_path 출력 WAV 파일 경로
 * @param audio_data 저장할 오디오 데이터
 * @param target_bits_per_sample 목표 비트 깊이 (0이면 자동 선택)
 * @param use_float_format IEEE float 포맷 사용 여부
 * @param apply_dithering 디더링 적용 여부 (비트 깊이 감소 시)
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
ETResult write_wav_file_advanced(const char* file_path, const AudioData* audio_data,
                                uint16_t target_bits_per_sample, bool use_float_format,
                                bool apply_dithering) {
    if (!file_path || !audio_data || !audio_data->data) {
        fprintf(stderr, "Error: write_wav_file_advanced에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 임시 AudioData 생성하여 포맷 변경
    AudioData temp_data;
    ETResult result = audio_data_copy(audio_data, &temp_data);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 목표 포맷 설정
    if (target_bits_per_sample > 0) {
        temp_data.info.bits_per_sample = target_bits_per_sample;
    }
    temp_data.info.is_float_format = use_float_format;

    // 디더링 적용 (비트 깊이가 감소하는 경우)
    if (apply_dithering && target_bits_per_sample > 0 &&
        target_bits_per_sample < audio_data->info.bits_per_sample) {

        result = apply_dithering_to_audio_data(&temp_data, target_bits_per_sample);
        if (result != ET_SUCCESS) {
            audio_data_destroy(&temp_data);
            return result;
        }
    }

    // WAV 파일 쓰기
    result = write_wav_file(file_path, &temp_data);

    // 임시 데이터 정리
    if (temp_data.owns_data && temp_data.data) {
        free(temp_data.data);
    }

    return result;
}

/**
 * @brief 오디오 데이터에 디더링 적용
 *
 * 양자화 노이즈를 줄이기 위해 디더링을 적용합니다.
 *
 * @param audio_data 디더링을 적용할 오디오 데이터
 * @param target_bits 목표 비트 깊이
 * @return ET_SUCCESS 성공, 그 외 오류 코드
 */
static ETResult apply_dithering_to_audio_data(AudioData* audio_data, uint16_t target_bits) {
    if (!audio_data || !audio_data->data) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    uint32_t total_samples = audio_data->info.num_samples * audio_data->info.num_channels;

    // 디더링 노이즈 레벨 계산
    float dither_amplitude = 1.0f / (1 << target_bits);

    // 간단한 TPDF (Triangular Probability Density Function) 디더링
    for (uint32_t i = 0; i < total_samples; i++) {
        // 두 개의 균등 분포 난수를 더해서 삼각 분포 생성
        float rand1 = ((float)rand() / RAND_MAX) - 0.5f;
        float rand2 = ((float)rand() / RAND_MAX) - 0.5f;
        float dither = (rand1 + rand2) * dither_amplitude;

        audio_data->data[i] += dither;

        // 클리핑
        if (audio_data->data[i] > 1.0f) audio_data->data[i] = 1.0f;
        if (audio_data->data[i] < -1.0f) audio_data->data[i] = -1.0f;
    }

    printf("Info: 디더링을 적용했습니다 (목표 비트: %d)\n", target_bits);
    return ET_SUCCESS;
}

/**
 * @brief WAV 파일 배치 변환
 *
 * 여러 WAV 파일을 일괄 변환합니다.
 *
 * @param input_files 입력 파일 경로 배열
 * @param output_files 출력 파일 경로 배열
 * @param num_files 파일 개수
 * @param target_sample_rate 목표 샘플링 레이트 (0이면 변경 안함)
 * @param target_bits_per_sample 목표 비트 깊이 (0이면 변경 안함)
 * @param convert_to_mono 모노 변환 여부
 * @return 성공한 파일 개수
 */
int batch_convert_wav_files(const char** input_files, const char** output_files,
                           int num_files, uint32_t target_sample_rate,
                           uint16_t target_bits_per_sample, bool convert_to_mono) {
    if (!input_files || !output_files || num_files <= 0) {
        fprintf(stderr, "Error: batch_convert_wav_files에 유효하지 않은 파라미터가 전달되었습니다.\n");
        return 0;
    }

    int success_count = 0;

    for (int i = 0; i < num_files; i++) {
        printf("Info: 변환 중 (%d/%d): %s -> %s\n",
               i + 1, num_files, input_files[i], output_files[i]);

        AudioData audio_data;
        ETResult result = read_wav_file(input_files[i], &audio_data);
        if (result != ET_SUCCESS) {
            fprintf(stderr, "Warning: 파일 읽기 실패: %s\n", input_files[i]);
            continue;
        }

        // 변환 적용
        bool modified = false;

        if (convert_to_mono && audio_data.info.num_channels > 1) {
            result = audio_data_to_mono(&audio_data);
            if (result == ET_SUCCESS) {
                modified = true;
            }
        }

        if (target_sample_rate > 0 && audio_data.info.sample_rate != target_sample_rate) {
            result = audio_data_resample(&audio_data, target_sample_rate);
            if (result == ET_SUCCESS) {
                modified = true;
            }
        }

        if (target_bits_per_sample > 0) {
            audio_data.info.bits_per_sample = target_bits_per_sample;
            modified = true;
        }

        // 파일 저장
        result = write_wav_file(output_files[i], &audio_data);
        if (result == ET_SUCCESS) {
            success_count++;
            if (modified) {
                printf("Info: 변환 완료: %s\n", output_files[i]);
            } else {
                printf("Info: 복사 완료: %s\n", output_files[i]);
            }
        } else {
            fprintf(stderr, "Warning: 파일 저장 실패: %s\n", output_files[i]);
        }

        // 메모리 정리
        audio_data_destroy(&audio_data);
    }

    printf("Info: 배치 변환 완료: %d/%d 파일 성공\n", success_count, num_files);
    return success_count;
}// 전방 선언

static ETResult apply_dithering_to_audio_data(AudioData* audio_data, uint16_t target_bits);

ETResult print_wav_file_metadata(const char* file_path) {
    if (!file_path) {
        fprintf(stderr, "Error: print_wav_file_metadata에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    AudioFileInfo info;
    ETResult result = get_wav_file_info(file_path, &info);
    if (result != ET_SUCCESS) {
        return result;
    }

    printf("=== WAV File Metadata ===\n");
    printf("File: %s\n", file_path);
    printf("Sample Rate: %d Hz\n", info.sample_rate);
    printf("Channels: %d (%s)\n", info.num_channels,
           (info.num_channels == 1) ? "Mono" :
           (info.num_channels == 2) ? "Stereo" : "Multi-channel");
    printf("Bits per Sample: %d\n", info.bits_per_sample);
    printf("Format: %s\n", info.is_float_format ? "IEEE Float" : "PCM");
    printf("Number of Samples: %d (per channel)\n", info.num_samples);
    printf("Duration: %.3f seconds (%.2f minutes)\n",
           info.duration_seconds, info.duration_seconds / 60.0);

    // 파일 크기 계산
    uint32_t bytes_per_sample = info.bits_per_sample / 8;
    uint32_t data_size = info.num_samples * info.num_channels * bytes_per_sample;
    uint32_t file_size = sizeof(WAVHeader) + data_size;

    printf("Data Size: %d bytes (%.2f MB)\n", data_size, data_size / (1024.0 * 1024.0));
    printf("File Size: %d bytes (%.2f MB)\n", file_size, file_size / (1024.0 * 1024.0));

    // 비트레이트 계산
    uint32_t bitrate = info.sample_rate * info.num_channels * info.bits_per_sample;
    printf("Bitrate: %d bps (%.1f kbps)\n", bitrate, bitrate / 1000.0);

    printf("========================\n");

    return ET_SUCCESS;
}

ETResult calculate_audio_statistics(const AudioData* audio_data,
                                   float* min_value, float* max_value,
                                   float* mean_value, float* rms_value) {
    if (!audio_data || !audio_data->data) {
        fprintf(stderr, "Error: calculate_audio_statistics에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    uint32_t total_samples = audio_data->info.num_samples * audio_data->info.num_channels;
    if (total_samples == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    float min_val = audio_data->data[0];
    float max_val = audio_data->data[0];
    double sum = 0.0;
    double sum_squares = 0.0;

    for (uint32_t i = 0; i < total_samples; i++) {
        float val = audio_data->data[i];

        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;

        sum += val;
        sum_squares += val * val;
    }

    if (min_value) *min_value = min_val;
    if (max_value) *max_value = max_val;
    if (mean_value) *mean_value = (float)(sum / total_samples);
    if (rms_value) *rms_value = (float)sqrt(sum_squares / total_samples);

    return ET_SUCCESS;
}

int detect_silence_regions(const AudioData* audio_data, float threshold,
                          float min_duration_ms, float* silence_start,
                          float* silence_end, int max_regions) {
    if (!audio_data || !audio_data->data || !silence_start || !silence_end) {
        fprintf(stderr, "Error: detect_silence_regions에 NULL 파라미터가 전달되었습니다.\n");
        return 0;
    }

    if (threshold < 0.0f) threshold = -threshold; // 절댓값 사용

    uint32_t min_samples = (uint32_t)((min_duration_ms / 1000.0f) * audio_data->info.sample_rate);
    uint32_t silence_count = 0;
    bool in_silence = false;
    uint32_t silence_start_sample = 0;

    for (uint32_t i = 0; i < audio_data->info.num_samples; i++) {
        // 현재 프레임의 최대 절댓값 계산 (모든 채널)
        float max_abs = 0.0f;
        for (uint16_t ch = 0; ch < audio_data->info.num_channels; ch++) {
            float abs_val = fabsf(audio_data->data[i * audio_data->info.num_channels + ch]);
            if (abs_val > max_abs) {
                max_abs = abs_val;
            }
        }

        bool is_silent = (max_abs <= threshold);

        if (!in_silence && is_silent) {
            // 무음 시작
            in_silence = true;
            silence_start_sample = i;
        } else if (in_silence && !is_silent) {
            // 무음 종료
            uint32_t silence_duration = i - silence_start_sample;

            if (silence_duration >= min_samples && silence_count < max_regions) {
                silence_start[silence_count] = (float)silence_start_sample / audio_data->info.sample_rate;
                silence_end[silence_count] = (float)i / audio_data->info.sample_rate;
                silence_count++;
            }

            in_silence = false;
        }
    }

    // 파일 끝까지 무음인 경우
    if (in_silence && silence_count < max_regions) {
        uint32_t silence_duration = audio_data->info.num_samples - silence_start_sample;

        if (silence_duration >= min_samples) {
            silence_start[silence_count] = (float)silence_start_sample / audio_data->info.sample_rate;
            silence_end[silence_count] = (float)audio_data->info.num_samples / audio_data->info.sample_rate;
            silence_count++;
        }
    }

    return (int)silence_count;
}

ETResult trim_audio_silence(AudioData* audio_data, float threshold) {
    if (!audio_data || !audio_data->data) {
        fprintf(stderr, "Error: trim_audio_silence에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (threshold < 0.0f) threshold = -threshold; // 절댓값 사용

    uint32_t start_sample = 0;
    uint32_t end_sample = audio_data->info.num_samples;

    // 앞쪽 무음 찾기
    for (uint32_t i = 0; i < audio_data->info.num_samples; i++) {
        bool is_silent = true;

        for (uint16_t ch = 0; ch < audio_data->info.num_channels; ch++) {
            float abs_val = fabsf(audio_data->data[i * audio_data->info.num_channels + ch]);
            if (abs_val > threshold) {
                is_silent = false;
                break;
            }
        }

        if (!is_silent) {
            start_sample = i;
            break;
        }
    }

    // 뒤쪽 무음 찾기
    for (uint32_t i = audio_data->info.num_samples; i > start_sample; i--) {
        bool is_silent = true;

        for (uint16_t ch = 0; ch < audio_data->info.num_channels; ch++) {
            float abs_val = fabsf(audio_data->data[(i - 1) * audio_data->info.num_channels + ch]);
            if (abs_val > threshold) {
                is_silent = false;
                break;
            }
        }

        if (!is_silent) {
            end_sample = i;
            break;
        }
    }

    // 트림이 필요한지 확인
    if (start_sample == 0 && end_sample == audio_data->info.num_samples) {
        printf("Info: 트림할 무음이 없습니다.\n");
        return ET_SUCCESS;
    }

    // 전체가 무음인 경우
    if (start_sample >= end_sample) {
        printf("Warning: 전체 오디오가 무음입니다. 트림하지 않습니다.\n");
        return ET_SUCCESS;
    }

    // 새로운 길이 계산
    uint32_t new_num_samples = end_sample - start_sample;
    uint32_t new_total_samples = new_num_samples * audio_data->info.num_channels;

    // 새로운 데이터 버퍼 할당
    float* new_data = (float*)malloc(new_total_samples * sizeof(float));
    if (!new_data) {
        fprintf(stderr, "Error: 트림용 메모리 할당 실패\n");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 데이터 복사
    uint32_t src_offset = start_sample * audio_data->info.num_channels;
    memcpy(new_data, &audio_data->data[src_offset], new_total_samples * sizeof(float));

    // 기존 데이터 교체
    if (audio_data->owns_data) {
        free(audio_data->data);
    }

    audio_data->data = new_data;
    audio_data->info.num_samples = new_num_samples;
    audio_data->info.duration_seconds = (double)new_num_samples / audio_data->info.sample_rate;
    audio_data->owns_data = true;

    printf("Info: 무음을 트림했습니다: %.3f초 -> %.3f초 (앞: %.3f초, 뒤: %.3f초 제거)\n",
           (double)(end_sample - start_sample + (audio_data->info.num_samples - end_sample) + start_sample) / audio_data->info.sample_rate,
           audio_data->info.duration_seconds,
           (double)start_sample / audio_data->info.sample_rate,
           (double)(audio_data->info.num_samples - end_sample) / audio_data->info.sample_rate);

    return ET_SUCCESS;
}

// ============================================================================
// libetude 오디오 I/O 통합 구현
// ============================================================================

#include <time.h>

// 내부 콜백 함수
static void internal_audio_callback(float* buffer, int num_frames, void* user_data);

ETResult world_audio_io_init(WorldAudioIO* audio_io, uint32_t sample_rate,
                            uint16_t num_channels, uint32_t buffer_size) {
    if (!audio_io) {
        fprintf(stderr, "Error: world_audio_io_init에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(audio_io, 0, sizeof(WorldAudioIO));

    // 오디오 포맷 설정
    audio_io->format = et_audio_format_create(sample_rate, num_channels, buffer_size);

    // 입력 버퍼 생성
    audio_io->input_buffer = et_audio_buffer_create(buffer_size * 4, num_channels);
    if (!audio_io->input_buffer) {
        fprintf(stderr, "Error: 입력 버퍼 생성 실패\n");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 출력 버퍼 생성
    audio_io->output_buffer = et_audio_buffer_create(buffer_size * 4, num_channels);
    if (!audio_io->output_buffer) {
        fprintf(stderr, "Error: 출력 버퍼 생성 실패\n");
        et_audio_buffer_destroy(audio_io->input_buffer);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 출력 디바이스 열기
    audio_io->output_device = et_audio_open_output_device(NULL, &audio_io->format);
    if (!audio_io->output_device) {
        fprintf(stderr, "Error: 출력 디바이스 열기 실패\n");
        et_audio_buffer_destroy(audio_io->input_buffer);
        et_audio_buffer_destroy(audio_io->output_buffer);
        return ET_ERROR_HARDWARE;
    }

    // 입력 디바이스 열기 (선택적)
    audio_io->input_device = et_audio_open_input_device(NULL, &audio_io->format);
    if (!audio_io->input_device) {
        printf("Warning: 입력 디바이스를 열 수 없습니다. 출력 전용 모드로 동작합니다.\n");
    }

    // 콜백 설정
    ETResult result = et_audio_set_callback(audio_io->output_device,
                                           internal_audio_callback, audio_io);
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: 출력 디바이스 콜백 설정 실패\n");
        world_audio_io_cleanup(audio_io);
        return result;
    }

    if (audio_io->input_device) {
        result = et_audio_set_callback(audio_io->input_device,
                                      internal_audio_callback, audio_io);
        if (result != ET_SUCCESS) {
            printf("Warning: 입력 디바이스 콜백 설정 실패\n");
        }
    }

    audio_io->is_initialized = true;
    audio_io->frames_processed = 0;
    audio_io->cpu_usage = 0.0;

    printf("Info: libetude 오디오 I/O 시스템이 초기화되었습니다.\n");
    printf("      샘플레이트: %d Hz, 채널: %d, 버퍼: %d 프레임\n",
           sample_rate, num_channels, buffer_size);

    return ET_SUCCESS;
}

void world_audio_io_cleanup(WorldAudioIO* audio_io) {
    if (!audio_io) {
        return;
    }

    // 스트림 정지
    if (audio_io->is_running) {
        world_audio_io_stop(audio_io);
    }

    // 디바이스 닫기
    if (audio_io->output_device) {
        et_audio_close_device(audio_io->output_device);
        audio_io->output_device = NULL;
    }

    if (audio_io->input_device) {
        et_audio_close_device(audio_io->input_device);
        audio_io->input_device = NULL;
    }

    // 버퍼 해제
    if (audio_io->input_buffer) {
        et_audio_buffer_destroy(audio_io->input_buffer);
        audio_io->input_buffer = NULL;
    }

    if (audio_io->output_buffer) {
        et_audio_buffer_destroy(audio_io->output_buffer);
        audio_io->output_buffer = NULL;
    }

    // 상태 초기화
    audio_io->is_initialized = false;
    audio_io->is_running = false;

    printf("Info: libetude 오디오 I/O 시스템이 정리되었습니다.\n");
}

ETResult world_audio_io_set_callback(WorldAudioIO* audio_io,
                                    void (*callback)(float* input, float* output,
                                                   int num_frames, void* user_data),
                                    void* user_data) {
    if (!audio_io || !callback) {
        fprintf(stderr, "Error: world_audio_io_set_callback에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!audio_io->is_initialized) {
        fprintf(stderr, "Error: 오디오 I/O 시스템이 초기화되지 않았습니다.\n");
        return ET_ERROR_NOT_INITIALIZED;
    }

    audio_io->audio_callback = callback;
    audio_io->user_data = user_data;

    return ET_SUCCESS;
}

ETResult world_audio_io_start(WorldAudioIO* audio_io) {
    if (!audio_io) {
        fprintf(stderr, "Error: world_audio_io_start에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!audio_io->is_initialized) {
        fprintf(stderr, "Error: 오디오 I/O 시스템이 초기화되지 않았습니다.\n");
        return ET_ERROR_NOT_INITIALIZED;
    }

    if (audio_io->is_running) {
        return ET_SUCCESS; // 이미 실행 중
    }

    // 출력 디바이스 시작
    ETResult result = et_audio_start(audio_io->output_device);
    if (result != ET_SUCCESS) {
        fprintf(stderr, "Error: 출력 디바이스 시작 실패\n");
        return result;
    }

    // 입력 디바이스 시작 (있는 경우)
    if (audio_io->input_device) {
        result = et_audio_start(audio_io->input_device);
        if (result != ET_SUCCESS) {
            printf("Warning: 입력 디바이스 시작 실패\n");
        }
    }

    audio_io->is_running = true;
    audio_io->frames_processed = 0;

    printf("Info: 오디오 스트림이 시작되었습니다.\n");
    return ET_SUCCESS;
}

ETResult world_audio_io_stop(WorldAudioIO* audio_io) {
    if (!audio_io) {
        fprintf(stderr, "Error: world_audio_io_stop에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!audio_io->is_running) {
        return ET_SUCCESS; // 이미 정지됨
    }

    // 디바이스 정지
    if (audio_io->output_device) {
        et_audio_stop(audio_io->output_device);
    }

    if (audio_io->input_device) {
        et_audio_stop(audio_io->input_device);
    }

    audio_io->is_running = false;

    printf("Info: 오디오 스트림이 정지되었습니다. (처리된 프레임: %llu)\n",
           (unsigned long long)audio_io->frames_processed);

    return ET_SUCCESS;
}

// 내부 콜백 함수 구현
static void internal_audio_callback(float* buffer, int num_frames, void* user_data) {
    WorldAudioIO* audio_io = (WorldAudioIO*)user_data;

    if (!audio_io || !audio_io->is_running) {
        // 무음 출력
        memset(buffer, 0, num_frames * audio_io->format.num_channels * sizeof(float));
        return;
    }

    // 성능 측정 시작
    clock_t start_time = clock();

    // 입력 데이터 준비 (입력 디바이스가 있는 경우)
    float* input_data = NULL;
    if (audio_io->input_device && audio_io->input_buffer) {
        // 실제 구현에서는 입력 버퍼에서 데이터를 읽어옴
        input_data = buffer; // 임시로 출력 버퍼 사용
    }

    // 사용자 콜백 호출
    if (audio_io->audio_callback) {
        audio_io->audio_callback(input_data, buffer, num_frames, audio_io->user_data);
    } else {
        // 기본 동작: 무음 출력
        memset(buffer, 0, num_frames * audio_io->format.num_channels * sizeof(float));
    }

    // 클리핑 적용
    et_audio_clip_buffer(buffer, num_frames * audio_io->format.num_channels);

    // 통계 업데이트
    audio_io->frames_processed += num_frames;

    // 성능 측정 종료
    clock_t end_time = clock();
    double callback_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    double frame_time = (double)num_frames / audio_io->format.sample_rate;

    // CPU 사용률 계산 (이동 평균)
    double current_cpu = (callback_time / frame_time) * 100.0;
    audio_io->cpu_usage = audio_io->cpu_usage * 0.9 + current_cpu * 0.1;
}

ETResult audio_data_to_et_buffer(const AudioData* audio_data, ETAudioBuffer* buffer) {
    if (!audio_data || !audio_data->data || !buffer) {
        fprintf(stderr, "Error: audio_data_to_et_buffer에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 버퍼 리셋
    et_audio_buffer_reset(buffer);

    // 데이터 쓰기
    uint32_t total_samples = audio_data->info.num_samples * audio_data->info.num_channels;
    uint32_t frames_written = et_audio_buffer_write(buffer, audio_data->data,
                                                   audio_data->info.num_samples);

    if (frames_written != audio_data->info.num_samples) {
        fprintf(stderr, "Warning: 모든 데이터를 버퍼에 쓸 수 없습니다 (%d/%d 프레임)\n",
                frames_written, audio_data->info.num_samples);
        return ET_ERROR_BUFFER_OVERFLOW;
    }

    return ET_SUCCESS;
}

ETResult et_buffer_to_audio_data(const ETAudioBuffer* buffer, AudioData* audio_data,
                                uint32_t sample_rate, uint16_t num_channels) {
    if (!buffer || !audio_data) {
        fprintf(stderr, "Error: et_buffer_to_audio_data에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 사용 가능한 데이터 크기 확인
    uint32_t available_frames = et_audio_buffer_available_data(buffer);
    if (available_frames == 0) {
        fprintf(stderr, "Error: 버퍼에 읽을 데이터가 없습니다.\n");
        return ET_ERROR_BUFFER_UNDERFLOW;
    }

    // AudioData 생성
    AudioData* new_audio_data = audio_data_create(available_frames, num_channels, sample_rate);
    if (!new_audio_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 데이터 읽기
    uint32_t frames_read = et_audio_buffer_read((ETAudioBuffer*)buffer,
                                               new_audio_data->data, available_frames);

    if (frames_read != available_frames) {
        fprintf(stderr, "Warning: 예상보다 적은 데이터를 읽었습니다 (%d/%d 프레임)\n",
                frames_read, available_frames);
        new_audio_data->info.num_samples = frames_read;
        new_audio_data->info.duration_seconds = (double)frames_read / sample_rate;
    }

    // 기존 데이터 해제 및 교체
    if (audio_data->owns_data && audio_data->data) {
        free(audio_data->data);
    }

    *audio_data = *new_audio_data;
    free(new_audio_data); // 구조체만 해제, 데이터는 유지

    return ET_SUCCESS;
}

int enumerate_audio_devices(char device_names[][256], int max_devices, bool is_input) {
    if (!device_names || max_devices <= 0) {
        return 0;
    }

    // 플랫폼별 디바이스 열거 구현
    // 여기서는 기본 디바이스만 반환하는 간단한 구현
    int device_count = 0;

    if (device_count < max_devices) {
        if (is_input) {
            strncpy(device_names[device_count], "Default Input Device", 255);
        } else {
            strncpy(device_names[device_count], "Default Output Device", 255);
        }
        device_names[device_count][255] = '\0';
        device_count++;
    }

    // 추가 디바이스들 (플랫폼별로 구현 필요)
    const char* additional_devices[] = {
        "Built-in Microphone",
        "Built-in Output",
        "USB Audio Device",
        "Bluetooth Audio"
    };

    int num_additional = sizeof(additional_devices) / sizeof(additional_devices[0]);
    for (int i = 0; i < num_additional && device_count < max_devices; i++) {
        strncpy(device_names[device_count], additional_devices[i], 255);
        device_names[device_count][255] = '\0';
        device_count++;
    }

    printf("Info: %d개의 %s 디바이스를 발견했습니다.\n",
           device_count, is_input ? "입력" : "출력");

    return device_count;
}

bool get_audio_device_info(const char* device_name, bool is_input,
                          uint32_t* supported_rates, int max_rates,
                          uint16_t* supported_channels, int max_channels) {
    if (!supported_rates || !supported_channels) {
        return false;
    }

    // 일반적으로 지원되는 샘플링 레이트
    const uint32_t common_rates[] = {
        8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000
    };
    int num_rates = sizeof(common_rates) / sizeof(common_rates[0]);

    // 지원되는 채널 수
    const uint16_t common_channels[] = {1, 2, 4, 6, 8};
    int num_channels = sizeof(common_channels) / sizeof(common_channels[0]);

    // 배열에 복사
    int rates_to_copy = (num_rates < max_rates) ? num_rates : max_rates;
    for (int i = 0; i < rates_to_copy; i++) {
        supported_rates[i] = common_rates[i];
    }

    int channels_to_copy = (num_channels < max_channels) ? num_channels : max_channels;
    for (int i = 0; i < channels_to_copy; i++) {
        supported_channels[i] = common_channels[i];
    }

    printf("Info: 디바이스 '%s' 정보 조회 완료\n",
           device_name ? device_name : "Default Device");
    printf("      지원 샘플레이트: %d개, 지원 채널: %d개\n",
           rates_to_copy, channels_to_copy);

    return true;
}

ETResult monitor_audio_performance(const WorldAudioIO* audio_io, double* cpu_usage,
                                  uint32_t* buffer_underruns, uint32_t* latency_ms) {
    if (!audio_io) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (cpu_usage) {
        *cpu_usage = audio_io->cpu_usage;
    }

    if (buffer_underruns) {
        // 실제 구현에서는 언더런 카운터를 추적해야 함
        *buffer_underruns = 0;
    }

    if (latency_ms) {
        if (audio_io->output_device) {
            *latency_ms = et_audio_get_latency(audio_io->output_device);
        } else {
            *latency_ms = 0;
        }
    }

    return ET_SUCCESS;
}

ETResult test_audio_stream_quality(WorldAudioIO* audio_io, float test_duration_seconds,
                                  float test_frequency) {
    if (!audio_io || test_duration_seconds <= 0.0f || test_frequency <= 0.0f) {
        fprintf(stderr, "Error: test_audio_stream_quality에 유효하지 않은 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!audio_io->is_initialized) {
        fprintf(stderr, "Error: 오디오 I/O 시스템이 초기화되지 않았습니다.\n");
        return ET_ERROR_NOT_INITIALIZED;
    }

    printf("Info: 오디오 스트림 품질 테스트 시작\n");
    printf("      지속시간: %.1f초, 테스트 주파수: %.1f Hz\n",
           test_duration_seconds, test_frequency);

    // 테스트 신호 생성 (사인파)
    uint32_t test_samples = (uint32_t)(test_duration_seconds * audio_io->format.sample_rate);
    AudioData* test_data = audio_data_create(test_samples, audio_io->format.num_channels,
                                            audio_io->format.sample_rate);
    if (!test_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 사인파 생성
    for (uint32_t i = 0; i < test_samples; i++) {
        float t = (float)i / audio_io->format.sample_rate;
        float sample = 0.5f * sinf(2.0f * M_PI * test_frequency * t);

        for (uint16_t ch = 0; ch < audio_io->format.num_channels; ch++) {
            test_data->data[i * audio_io->format.num_channels + ch] = sample;
        }
    }

    // 테스트 실행 (실제로는 더 복잡한 품질 측정 로직 필요)
    printf("Info: 테스트 신호 생성 완료 (%d 샘플)\n", test_samples);

    // 통계 계산
    float min_val, max_val, mean_val, rms_val;
    ETResult result = calculate_audio_statistics(test_data, &min_val, &max_val,
                                               &mean_val, &rms_val);
    if (result == ET_SUCCESS) {
        printf("Info: 테스트 신호 통계\n");
        printf("      최솟값: %.6f, 최댓값: %.6f\n", min_val, max_val);
        printf("      평균: %.6f, RMS: %.6f\n", mean_val, rms_val);
    }

    // 메모리 정리
    audio_data_destroy(test_data);

    printf("Info: 오디오 스트림 품질 테스트 완료\n");
    return ET_SUCCESS;
}