#include "libetude/compression.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// 음성 특화 압축을 위한 상수들
#define MEL_FREQUENCY_BANDS 80
#define TEMPORAL_WINDOW_SIZE 256
#define PERCEPTUAL_THRESHOLD 0.001f
#define QUANTIZATION_LEVELS 256

// DCT 변환을 위한 코사인 테이블 (간단한 구현)
static float cosine_table[MEL_FREQUENCY_BANDS][MEL_FREQUENCY_BANDS];
static bool cosine_table_initialized = false;

/**
 * DCT 코사인 테이블 초기화
 */
static void init_cosine_table() {
    if (cosine_table_initialized) {
        return;
    }

    for (int i = 0; i < MEL_FREQUENCY_BANDS; i++) {
        for (int j = 0; j < MEL_FREQUENCY_BANDS; j++) {
            cosine_table[i][j] = cosf((float)M_PI * i * (j + 0.5f) / MEL_FREQUENCY_BANDS);
        }
    }
    cosine_table_initialized = true;
}

/**
 * 1D DCT 변환 (Type-II)
 */
static void dct_transform(const float* input, float* output, int size) {
    for (int i = 0; i < size; i++) {
        output[i] = 0.0f;
        for (int j = 0; j < size; j++) {
            float coeff = (i == 0) ? sqrtf(1.0f / size) : sqrtf(2.0f / size);
            output[i] += coeff * input[j] * cosf((float)M_PI * i * (j + 0.5f) / size);
        }
    }
}

/**
 * 1D IDCT 변환 (역변환)
 */
static void idct_transform(const float* input, float* output, int size) {
    for (int i = 0; i < size; i++) {
        output[i] = 0.0f;
        for (int j = 0; j < size; j++) {
            float coeff = (j == 0) ? sqrtf(1.0f / size) : sqrtf(2.0f / size);
            output[i] += coeff * input[j] * cosf((float)M_PI * j * (i + 0.5f) / size);
        }
    }
}

/**
 * 지각적 가중치 계산 (Mel 주파수 기반)
 */
static float calculate_perceptual_weight(int mel_bin, int total_bins) {
    // 낮은 주파수에 더 높은 가중치 부여 (인간의 청각 특성)
    float normalized_freq = (float)mel_bin / total_bins;
    return 1.0f + 2.0f * expf(-normalized_freq * 3.0f);
}

/**
 * 시간적 상관관계를 이용한 예측 압축
 */
static int temporal_prediction_compress(const float* input, int size, int window_size,
                                      float* residuals, float* predictors) {
    if (!input || !residuals || !predictors || size < window_size) {
        return -1;
    }

    // 첫 번째 윈도우는 그대로 저장
    memcpy(residuals, input, window_size * sizeof(float));
    memset(predictors, 0, (size / window_size) * sizeof(float));

    // 나머지 윈도우들에 대해 예측 압축 수행
    for (int i = window_size; i < size; i += window_size) {
        int window_idx = i / window_size;
        int remaining = (size - i < window_size) ? (size - i) : window_size;

        // 이전 윈도우를 기반으로 예측값 계산
        float predictor = 0.0f;
        for (int j = 0; j < window_size && i - window_size + j >= 0; j++) {
            predictor += input[i - window_size + j];
        }
        predictor /= window_size;
        predictors[window_idx] = predictor;

        // 잔차 계산
        for (int j = 0; j < remaining; j++) {
            residuals[i + j] = input[i + j] - predictor;
        }
    }

    return 0;
}

/**
 * 양자화 및 엔트로피 부호화
 */
static int quantize_and_encode(const float* input, int size, float threshold,
                              uint8_t* output, int output_capacity, int* encoded_size) {
    if (!input || !output || !encoded_size) {
        return -1;
    }

    int out_pos = 0;
    *encoded_size = 0;

    // 간단한 양자화 및 RLE 부호화
    for (int i = 0; i < size && out_pos < output_capacity - 2; i++) {
        float val = input[i];

        // 임계값 이하는 0으로 양자화
        if (fabsf(val) < threshold) {
            val = 0.0f;
        }

        // 8비트 양자화
        int quantized = (int)(val * 127.0f);
        if (quantized > 127) quantized = 127;
        if (quantized < -128) quantized = -128;

        uint8_t byte_val = (uint8_t)(quantized + 128);

        // 간단한 RLE: 연속된 같은 값 압축
        int run_length = 1;
        while (i + run_length < size && run_length < 255) {
            float next_val = input[i + run_length];
            if (fabsf(next_val) < threshold) next_val = 0.0f;

            int next_quantized = (int)(next_val * 127.0f);
            if (next_quantized > 127) next_quantized = 127;
            if (next_quantized < -128) next_quantized = -128;

            if ((uint8_t)(next_quantized + 128) != byte_val) break;
            run_length++;
        }

        if (run_length >= 3) {
            // RLE 토큰: [0xFF] [길이] [값]
            if (out_pos + 3 <= output_capacity) {
                output[out_pos++] = 0xFF;
                output[out_pos++] = (uint8_t)run_length;
                output[out_pos++] = byte_val;
                i += run_length - 1;
            } else {
                break;
            }
        } else {
            // 일반 값
            output[out_pos++] = byte_val;
        }
    }

    *encoded_size = out_pos;
    return 0;
}

// ============================================================================
// 음성 특화 압축 함수 구현
// ============================================================================

/**
 * 음성 특화 압축 컨텍스트 생성
 */
CompressionContext* voice_compression_create_context(const VoiceCompressionParams* params) {
    if (!params) {
        return NULL;
    }

    CompressionContext* ctx = compression_create_context(COMPRESSION_VOICE_OPTIMIZED, 6);
    if (!ctx) {
        return NULL;
    }

    // 음성 특화 파라미터 복사
    VoiceCompressionParams* voice_params = (VoiceCompressionParams*)ctx->internal_context;
    *voice_params = *params;

    // DCT 테이블 초기화
    init_cosine_table();

    return ctx;
}

/**
 * Mel 스펙트로그램 가중치 압축
 */
int voice_compress_mel_weights(const float* mel_weights, size_t size,
                              const VoiceCompressionParams* params,
                              void* output, size_t output_capacity,
                              size_t* compressed_size) {
    if (!mel_weights || !params || !output || !compressed_size || size == 0) {
        return COMPRESSION_ERROR_INVALID_ARGUMENT;
    }

    *compressed_size = 0;
    uint8_t* out_buffer = (uint8_t*)output;
    size_t out_pos = 0;

    // 임시 버퍼 할당
    float* dct_coeffs = (float*)malloc(size * sizeof(float));
    float* weighted_coeffs = (float*)malloc(size * sizeof(float));

    if (!dct_coeffs || !weighted_coeffs) {
        free(dct_coeffs);
        free(weighted_coeffs);
        return COMPRESSION_ERROR_OUT_OF_MEMORY;
    }

    // Mel 주파수 도메인에서 DCT 변환 수행
    int mel_bins = (int)sqrtf((float)size);  // 대략적인 Mel 빈 수 추정
    if (mel_bins > MEL_FREQUENCY_BANDS) mel_bins = MEL_FREQUENCY_BANDS;

    // 블록 단위로 DCT 변환
    for (size_t i = 0; i < size; i += mel_bins) {
        int block_size = (size - i < (size_t)mel_bins) ? (int)(size - i) : mel_bins;

        // DCT 변환
        dct_transform(&mel_weights[i], &dct_coeffs[i], block_size);

        // 지각적 가중치 적용
        for (int j = 0; j < block_size; j++) {
            float weight = calculate_perceptual_weight(j, block_size);
            weighted_coeffs[i + j] = dct_coeffs[i + j] * weight * params->mel_frequency_weight;
        }
    }

    // 양자화 및 부호화
    int encoded_size;
    float threshold = PERCEPTUAL_THRESHOLD / params->quality_threshold;

    int result = quantize_and_encode(weighted_coeffs, (int)size, threshold,
                                   &out_buffer[out_pos], (int)(output_capacity - out_pos),
                                   &encoded_size);

    if (result == 0) {
        out_pos += encoded_size;
        *compressed_size = out_pos;
    }

    free(dct_coeffs);
    free(weighted_coeffs);

    return (result == 0) ? COMPRESSION_SUCCESS : COMPRESSION_ERROR_COMPRESSION_FAILED;
}

/**
 * 어텐션 가중치 압축 (시간적 상관관계 활용)
 */
int voice_compress_attention_weights(const float* attention_weights, size_t size,
                                    int num_heads, int seq_length,
                                    const VoiceCompressionParams* params,
                                    void* output, size_t output_capacity,
                                    size_t* compressed_size) {
    if (!attention_weights || !params || !output || !compressed_size ||
        size == 0 || num_heads == 0 || seq_length == 0) {
        return COMPRESSION_ERROR_INVALID_ARGUMENT;
    }

    *compressed_size = 0;
    uint8_t* out_buffer = (uint8_t*)output;
    size_t out_pos = 0;

    // 헤드별로 분리하여 처리
    size_t head_size = size / num_heads;

    for (int head = 0; head < num_heads; head++) {
        const float* head_weights = &attention_weights[head * head_size];

        // 임시 버퍼 할당
        float* residuals = (float*)malloc(head_size * sizeof(float));
        float* predictors = (float*)malloc((head_size / TEMPORAL_WINDOW_SIZE + 1) * sizeof(float));

        if (!residuals || !predictors) {
            free(residuals);
            free(predictors);
            return COMPRESSION_ERROR_OUT_OF_MEMORY;
        }

        // 시간적 예측 압축 적용
        int pred_result = temporal_prediction_compress(head_weights, (int)head_size,
                                                     TEMPORAL_WINDOW_SIZE, residuals, predictors);

        if (pred_result != 0) {
            free(residuals);
            free(predictors);
            return COMPRESSION_ERROR_COMPRESSION_FAILED;
        }

        // 잔차 신호 양자화 및 부호화
        int encoded_size;
        float threshold = PERCEPTUAL_THRESHOLD * params->temporal_correlation;

        int result = quantize_and_encode(residuals, (int)head_size, threshold,
                                       &out_buffer[out_pos], (int)(output_capacity - out_pos),
                                       &encoded_size);

        if (result != 0) {
            free(residuals);
            free(predictors);
            return COMPRESSION_ERROR_COMPRESSION_FAILED;
        }

        out_pos += encoded_size;

        // 예측값들도 저장 (복원을 위해 필요)
        int num_predictors = (int)head_size / TEMPORAL_WINDOW_SIZE;
        if (out_pos + num_predictors * sizeof(float) <= output_capacity) {
            memcpy(&out_buffer[out_pos], predictors, num_predictors * sizeof(float));
            out_pos += num_predictors * sizeof(float);
        }

        free(residuals);
        free(predictors);
    }

    *compressed_size = out_pos;
    return COMPRESSION_SUCCESS;
}

/**
 * 보코더 가중치 압축 (주파수 도메인 최적화)
 */
int voice_compress_vocoder_weights(const float* vocoder_weights, size_t size,
                                  int sample_rate,
                                  const VoiceCompressionParams* params,
                                  void* output, size_t output_capacity,
                                  size_t* compressed_size) {
    if (!vocoder_weights || !params || !output || !compressed_size ||
        size == 0 || sample_rate <= 0) {
        return COMPRESSION_ERROR_INVALID_ARGUMENT;
    }

    *compressed_size = 0;
    uint8_t* out_buffer = (uint8_t*)output;
    size_t out_pos = 0;

    // 주파수 빈 크기 계산 (일반적으로 FFT 크기의 절반 + 1)
    int freq_bins = 1025;  // 2048 FFT의 경우
    if (size % freq_bins != 0) {
        freq_bins = (int)sqrtf((float)size);  // 대략적인 추정
    }

    // 임시 버퍼 할당
    float* freq_domain = (float*)malloc(size * sizeof(float));
    float* compressed_freq = (float*)malloc(size * sizeof(float));

    if (!freq_domain || !compressed_freq) {
        free(freq_domain);
        free(compressed_freq);
        return COMPRESSION_ERROR_OUT_OF_MEMORY;
    }

    // 주파수 도메인 변환 및 압축
    for (size_t i = 0; i < size; i += freq_bins) {
        int block_size = (size - i < (size_t)freq_bins) ? (int)(size - i) : freq_bins;

        // 주파수 도메인 변환 (간단한 DCT 사용)
        dct_transform(&vocoder_weights[i], &freq_domain[i], block_size);

        // 주파수별 중요도 가중치 적용
        for (int j = 0; j < block_size; j++) {
            float freq_hz = (float)j * sample_rate / (2.0f * block_size);

            // 음성 주파수 대역 (80Hz - 8kHz)에 더 높은 가중치
            float importance = 1.0f;
            if (freq_hz >= 80.0f && freq_hz <= 8000.0f) {
                importance = 2.0f;
            } else if (freq_hz > 8000.0f) {
                importance = 0.5f;  // 고주파는 덜 중요
            }

            compressed_freq[i + j] = freq_domain[i + j] * importance;
        }
    }

    // 지각적 임계값 적용하여 양자화
    float threshold = PERCEPTUAL_THRESHOLD;
    if (params->use_perceptual_model) {
        threshold *= params->quality_threshold;
    }

    int encoded_size;
    int result = quantize_and_encode(compressed_freq, (int)size, threshold,
                                   &out_buffer[out_pos], (int)(output_capacity - out_pos),
                                   &encoded_size);

    if (result == 0) {
        out_pos += encoded_size;
        *compressed_size = out_pos;
    }

    free(freq_domain);
    free(compressed_freq);

    return (result == 0) ? COMPRESSION_SUCCESS : COMPRESSION_ERROR_COMPRESSION_FAILED;
}

// ============================================================================
// 압축 사전 관리 구현
// ============================================================================

/**
 * 모델별 압축 사전 생성
 */
size_t create_model_compression_dictionary(const void** model_layers,
                                          const size_t* layer_sizes,
                                          size_t num_layers,
                                          void* dict_buffer,
                                          size_t dict_capacity) {
    if (!model_layers || !layer_sizes || !dict_buffer || num_layers == 0) {
        return 0;
    }

    // 모든 레이어에서 공통 패턴 추출
    uint32_t pattern_freq[256] = {0};
    size_t total_bytes = 0;

    // 바이트 패턴 빈도 계산
    for (size_t i = 0; i < num_layers; i++) {
        const uint8_t* layer_data = (const uint8_t*)model_layers[i];
        for (size_t j = 0; j < layer_sizes[i]; j++) {
            pattern_freq[layer_data[j]]++;
            total_bytes++;
        }
    }

    // 빈도 기반 사전 생성
    uint8_t* dict = (uint8_t*)dict_buffer;
    size_t dict_pos = 0;

    // 빈도 순으로 정렬하여 사전에 추가
    for (int threshold = (int)(total_bytes / 1000); threshold > 0 && dict_pos < dict_capacity; threshold /= 2) {
        for (int byte_val = 0; byte_val < 256 && dict_pos < dict_capacity; byte_val++) {
            if (pattern_freq[byte_val] >= (uint32_t)threshold) {
                dict[dict_pos++] = (uint8_t)byte_val;
            }
        }
    }

    return dict_pos;
}

/**
 * 압축 사전을 사용한 압축
 */
int compression_compress_with_dictionary(CompressionContext* ctx,
                                        const void* input, size_t input_size,
                                        void* output, size_t output_capacity,
                                        size_t* compressed_size) {
    if (!ctx || !input || !output || !compressed_size) {
        return COMPRESSION_ERROR_INVALID_ARGUMENT;
    }

    // 사전이 없으면 일반 압축 수행
    if (!ctx->use_dictionary || !ctx->dictionary_data) {
        return compression_compress(ctx, input, input_size, output, output_capacity, compressed_size);
    }

    // 사전 기반 압축 (간단한 구현)
    const uint8_t* src = (const uint8_t*)input;
    uint8_t* dst = (uint8_t*)output;
    const uint8_t* dict = (const uint8_t*)ctx->dictionary_data;

    size_t src_pos = 0;
    size_t dst_pos = 0;

    while (src_pos < input_size && dst_pos < output_capacity - 1) {
        uint8_t current_byte = src[src_pos];

        // 사전에서 바이트 찾기
        bool found_in_dict = false;
        for (size_t i = 0; i < ctx->dictionary_size; i++) {
            if (dict[i] == current_byte) {
                // 사전 인덱스로 압축 (상위 비트 설정)
                dst[dst_pos++] = (uint8_t)(i | 0x80);
                found_in_dict = true;
                break;
            }
        }

        if (!found_in_dict) {
            // 사전에 없으면 원본 바이트 저장
            dst[dst_pos++] = current_byte;
        }

        src_pos++;
    }

    *compressed_size = dst_pos;
    return COMPRESSION_SUCCESS;
}