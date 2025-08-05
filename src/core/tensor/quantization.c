/**
 * @file quantization.c
 * @brief LibEtude 텐서 양자화 구현
 *
 * BF16, INT8, INT4 양자화 및 동적 양자화를 구현합니다.
 * 음성 합성에 최적화된 양자화 전략을 제공합니다.
 */

#include "libetude/tensor.h"
#include "libetude/simd_kernels.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>

// =============================================================================
// BF16 (Brain Float 16) 양자화 구현
// =============================================================================

float et_bfloat16_to_float32(uint16_t bf16_val) {
    // BF16은 float32의 상위 16비트만 사용
    // 하위 16비트는 0으로 채움
    union {
        float f;
        uint32_t i;
    } converter;

    converter.i = ((uint32_t)bf16_val) << 16;
    return converter.f;
}

uint16_t et_float32_to_bfloat16(float float_val) {
    union {
        float f;
        uint32_t i;
    } converter;

    converter.f = float_val;

    // 반올림을 위한 바이어스 추가
    uint32_t bias = 0x00007FFF + ((converter.i >> 16) & 1);

    // 상위 16비트 추출 (반올림 적용)
    return (uint16_t)((converter.i + bias) >> 16);
}

ETTensor* et_quantize_to_bfloat16(const ETTensor* input, ETTensor* output, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32) {
        return NULL;
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_BFLOAT16, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_BFLOAT16 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    const float* input_data = (const float*)input->data;
    uint16_t* output_data = (uint16_t*)output->data;

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            output_data[i] = et_float32_to_bfloat16(input_data[i]);
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 변환
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            float val = et_get_float(input, indices);
            uint16_t bf16_val = et_float32_to_bfloat16(val);

            // 출력 텐서에 설정
            void* out_ptr = et_get_ptr(output, indices);
            if (out_ptr) {
                *(uint16_t*)out_ptr = bf16_val;
            }

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    return output;
}

ETTensor* et_dequantize_from_bfloat16(const ETTensor* input, ETTensor* output, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_BFLOAT16) {
        return NULL;
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_FLOAT32, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_FLOAT32 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    const uint16_t* input_data = (const uint16_t*)input->data;
    float* output_data = (float*)output->data;

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            output_data[i] = et_bfloat16_to_float32(input_data[i]);
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 변환
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            void* in_ptr = et_get_ptr(input, indices);
            if (in_ptr) {
                uint16_t bf16_val = *(uint16_t*)in_ptr;
                float val = et_bfloat16_to_float32(bf16_val);
                et_set_float(output, indices, val);
            }

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    return output;
}

// =============================================================================
// INT8/INT4 양자화 파라미터 계산
// =============================================================================

// 정적 함수 선언
static bool et_compute_minmax_range(const ETTensor* input, float* min_val, float* max_val);
static bool et_compute_percentile_range(const ETTensor* input, float outlier_percentile,
                                       float* min_val, float* max_val);
static bool et_compute_kl_optimal_range(const ETTensor* input, ETDataType target_dtype,
                                       float* min_val, float* max_val);
static bool et_compute_mse_optimal_range(const ETTensor* input, ETDataType target_dtype,
                                        float* min_val, float* max_val);
static bool et_compute_voice_optimal_range(const ETTensor* input, ETDataType target_dtype,
                                          float* min_val, float* max_val);
static int et_float_compare(const void* a, const void* b);
static float et_compute_kl_divergence(const int* histogram, int num_bins, float bin_width,
                                     float min_val, float threshold, int num_quantized_bins);
static float et_compute_quantization_mse(const ETTensor* input, float min_val, float max_val, int num_levels);
static float et_round_to_nearest_even(float x);

// 양자화 오차 분석 구조체
typedef struct {
    float mse;                    // 평균 제곱 오차
    float mae;                    // 평균 절대 오차
    float max_error;              // 최대 오차
    float snr_db;                 // 신호 대 잡음비 (dB)
    float dynamic_range_loss;     // 동적 범위 손실 (%)
} ETQuantizationError;

static bool et_compute_quantization_error(const ETTensor* original, const ETTensor* quantized,
                                         const ETQuantizationParams* params, ETQuantizationError* error);

// 구조체들은 헤더 파일에 정의되어 있음

// 기본 양자화 파라미터 계산 (기존 함수)
bool et_compute_quantization_params(const ETTensor* input, ETDataType target_dtype, ETQuantizationParams* params) {
    ETQuantizationOptions options = {
        .strategy = ET_QUANT_STRATEGY_MINMAX,
        .outlier_percentile = 0.0f,
        .symmetric = false,
        .per_channel = false,
        .channel_axis = 0,
        .smoothing_factor = 0.0f
    };

    return et_compute_quantization_params_advanced(input, target_dtype, params, &options);
}

// 고급 양자화 파라미터 계산
bool et_compute_quantization_params_advanced(const ETTensor* input, ETDataType target_dtype,
                                            ETQuantizationParams* params, const ETQuantizationOptions* options) {
    if (!et_validate_tensor(input) || !params || input->dtype != ET_FLOAT32) {
        return false;
    }

    if (target_dtype != ET_INT8 && target_dtype != ET_INT4) {
        return false;
    }

    // 양자화 범위 설정
    int32_t qmin, qmax;
    if (target_dtype == ET_INT8) {
        qmin = options->symmetric ? -127 : -128;
        qmax = 127;
    } else { // ET_INT4
        qmin = options->symmetric ? -7 : -8;
        qmax = 7;
    }

    float min_val, max_val;

    // 전략에 따른 min/max 계산
    switch (options->strategy) {
        case ET_QUANT_STRATEGY_PERCENTILE:
            if (!et_compute_percentile_range(input, options->outlier_percentile, &min_val, &max_val)) {
                return false;
            }
            break;

        case ET_QUANT_STRATEGY_KL_DIVERGENCE:
            if (!et_compute_kl_optimal_range(input, target_dtype, &min_val, &max_val)) {
                return false;
            }
            break;

        case ET_QUANT_STRATEGY_MSE_OPTIMAL:
            if (!et_compute_mse_optimal_range(input, target_dtype, &min_val, &max_val)) {
                return false;
            }
            break;

        case ET_QUANT_STRATEGY_VOICE_OPTIMIZED:
            if (!et_compute_voice_optimal_range(input, target_dtype, &min_val, &max_val)) {
                return false;
            }
            break;

        default: // ET_QUANT_STRATEGY_MINMAX
            if (!et_compute_minmax_range(input, &min_val, &max_val)) {
                return false;
            }
            break;
    }

    // 대칭 양자화 처리
    if (options->symmetric) {
        float abs_max = fmaxf(fabsf(min_val), fabsf(max_val));
        min_val = -abs_max;
        max_val = abs_max;
    }

    // 스무딩 적용 (이전 값과의 가중 평균)
    if (options->smoothing_factor > 0.0f && options->smoothing_factor <= 1.0f) {
        // 이전 파라미터가 있다면 스무딩 적용 (여기서는 단순화)
        // 실제로는 이전 양자화 파라미터를 저장해야 함
    }

    // 스케일과 제로 포인트 계산
    float scale = (max_val - min_val) / (qmax - qmin);
    if (scale == 0.0f || scale < 1e-8f) {
        scale = 1e-8f; // 수치적 안정성을 위한 최소값
    }

    int32_t zero_point;
    if (options->symmetric) {
        zero_point = 0; // 대칭 양자화에서는 제로 포인트가 0
    } else {
        zero_point = qmin - (int32_t)roundf(min_val / scale);
        // 제로 포인트 클램핑
        if (zero_point < qmin) zero_point = qmin;
        if (zero_point > qmax) zero_point = qmax;
    }

    // 파라미터 설정
    params->scale = scale;
    params->zero_point = zero_point;
    params->min_val = min_val;
    params->max_val = max_val;

    return true;
}

// Min-Max 범위 계산
static bool et_compute_minmax_range(const ETTensor* input, float* min_val, float* max_val) {
    *min_val = FLT_MAX;
    *max_val = -FLT_MAX;

    const float* data = (const float*)input->data;

    if (input->is_contiguous) {
        // 연속 메모리인 경우 빠른 탐색
        for (size_t i = 0; i < input->size; i++) {
            float val = data[i];
            if (val < *min_val) *min_val = val;
            if (val > *max_val) *max_val = val;
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 탐색
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            float val = et_get_float(input, indices);
            if (val < *min_val) *min_val = val;
            if (val > *max_val) *max_val = val;

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    return *min_val != FLT_MAX && *max_val != -FLT_MAX;
}

// 백분위수 기반 범위 계산 (이상치 제거)
static bool et_compute_percentile_range(const ETTensor* input, float outlier_percentile,
                                       float* min_val, float* max_val) {
    if (outlier_percentile < 0.0f || outlier_percentile >= 50.0f) {
        return et_compute_minmax_range(input, min_val, max_val);
    }

    // 모든 값을 배열에 복사 (정렬을 위해)
    float* values = (float*)malloc(input->size * sizeof(float));
    if (!values) {
        return false;
    }

    const float* data = (const float*)input->data;
    if (input->is_contiguous) {
        memcpy(values, data, input->size * sizeof(float));
    } else {
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};
        for (size_t i = 0; i < input->size; i++) {
            values[i] = et_get_float(input, indices);

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    // 퀵 정렬 (간단한 구현)
    qsort(values, input->size, sizeof(float), et_float_compare);

    // 백분위수 계산
    size_t lower_idx = (size_t)(input->size * outlier_percentile / 100.0f);
    size_t upper_idx = input->size - 1 - lower_idx;

    if (lower_idx >= input->size) lower_idx = 0;
    if (upper_idx >= input->size) upper_idx = input->size - 1;

    *min_val = values[lower_idx];
    *max_val = values[upper_idx];

    free(values);
    return true;
}

// float 비교 함수 (qsort용)
static int et_float_compare(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

// KL 발산 최소화 기반 범위 계산
static bool et_compute_kl_optimal_range(const ETTensor* input, ETDataType target_dtype,
                                       float* min_val, float* max_val) {
    // 히스토그램 기반 KL 발산 최소화
    const int num_bins = 2048;
    const int num_candidates = 100;

    // 먼저 전체 범위 계산
    float global_min, global_max;
    if (!et_compute_minmax_range(input, &global_min, &global_max)) {
        return false;
    }

    // 히스토그램 생성
    int* histogram = (int*)calloc(num_bins, sizeof(int));
    if (!histogram) {
        return false;
    }

    float bin_width = (global_max - global_min) / num_bins;
    if (bin_width <= 0.0f) {
        free(histogram);
        *min_val = global_min;
        *max_val = global_max;
        return true;
    }

    // 히스토그램 채우기
    const float* data = (const float*)input->data;
    if (input->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            int bin = (int)((data[i] - global_min) / bin_width);
            if (bin < 0) bin = 0;
            if (bin >= num_bins) bin = num_bins - 1;
            histogram[bin]++;
        }
    } else {
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};
        for (size_t i = 0; i < input->size; i++) {
            float val = et_get_float(input, indices);
            int bin = (int)((val - global_min) / bin_width);
            if (bin < 0) bin = 0;
            if (bin >= num_bins) bin = num_bins - 1;
            histogram[bin]++;

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    // 양자화 레벨 수
    int num_quantized_bins = (target_dtype == ET_INT8) ? 256 : 16;

    // 최적의 임계값 찾기 (간단한 그리드 서치)
    float best_threshold = global_max;
    float min_kl_div = FLT_MAX;

    for (int i = 1; i < num_candidates; i++) {
        float threshold = global_min + (global_max - global_min) * i / num_candidates;
        float kl_div = et_compute_kl_divergence(histogram, num_bins, bin_width, global_min,
                                               threshold, num_quantized_bins);

        if (kl_div < min_kl_div) {
            min_kl_div = kl_div;
            best_threshold = threshold;
        }
    }

    free(histogram);

    *min_val = global_min;
    *max_val = best_threshold;
    return true;
}

// KL 발산 계산
static float et_compute_kl_divergence(const int* histogram, int num_bins, float bin_width,
                                     float min_val, float threshold, int num_quantized_bins) {
    // 원본 분포 정규화
    int total_count = 0;
    for (int i = 0; i < num_bins; i++) {
        total_count += histogram[i];
    }

    if (total_count == 0) return FLT_MAX;

    // 임계값 내의 빈 찾기
    int threshold_bin = (int)((threshold - min_val) / bin_width);
    if (threshold_bin >= num_bins) threshold_bin = num_bins - 1;

    // 양자화된 분포 생성
    float* quantized_dist = (float*)calloc(num_quantized_bins, sizeof(float));
    if (!quantized_dist) return FLT_MAX;

    float quantized_bin_width = (threshold - min_val) / num_quantized_bins;

    for (int i = 0; i <= threshold_bin; i++) {
        float bin_center = min_val + (i + 0.5f) * bin_width;
        int q_bin = (int)((bin_center - min_val) / quantized_bin_width);
        if (q_bin >= num_quantized_bins) q_bin = num_quantized_bins - 1;
        quantized_dist[q_bin] += (float)histogram[i] / total_count;
    }

    // KL 발산 계산
    float kl_div = 0.0f;
    for (int i = 0; i <= threshold_bin; i++) {
        float p = (float)histogram[i] / total_count;
        if (p > 0.0f) {
            float bin_center = min_val + (i + 0.5f) * bin_width;
            int q_bin = (int)((bin_center - min_val) / quantized_bin_width);
            if (q_bin >= num_quantized_bins) q_bin = num_quantized_bins - 1;

            float q = quantized_dist[q_bin];
            if (q > 0.0f) {
                kl_div += p * logf(p / q);
            } else {
                kl_div += p * 10.0f; // 큰 페널티
            }
        }
    }

    free(quantized_dist);
    return kl_div;
}

// MSE 최적화 기반 범위 계산
static bool et_compute_mse_optimal_range(const ETTensor* input, ETDataType target_dtype,
                                        float* min_val, float* max_val) {
    // 전체 범위 계산
    float global_min, global_max;
    if (!et_compute_minmax_range(input, &global_min, &global_max)) {
        return false;
    }

    // 양자화 레벨 수
    int num_levels = (target_dtype == ET_INT8) ? 256 : 16;

    // 그리드 서치로 최적 범위 찾기
    const int num_candidates = 50;
    float best_min = global_min, best_max = global_max;
    float min_mse = FLT_MAX;

    for (int i = 0; i < num_candidates; i++) {
        for (int j = i + 1; j < num_candidates; j++) {
            float test_min = global_min + (global_max - global_min) * i / num_candidates;
            float test_max = global_min + (global_max - global_min) * j / num_candidates;

            float mse = et_compute_quantization_mse(input, test_min, test_max, num_levels);

            if (mse < min_mse) {
                min_mse = mse;
                best_min = test_min;
                best_max = test_max;
            }
        }
    }

    *min_val = best_min;
    *max_val = best_max;
    return true;
}

// 양자화 MSE 계산
static float et_compute_quantization_mse(const ETTensor* input, float min_val, float max_val, int num_levels) {
    float scale = (max_val - min_val) / (num_levels - 1);
    if (scale <= 0.0f) return FLT_MAX;

    float mse = 0.0f;
    const float* data = (const float*)input->data;

    if (input->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            float original = data[i];

            // 클램핑
            if (original < min_val) original = min_val;
            if (original > max_val) original = max_val;

            // 양자화
            int quantized_int = (int)roundf((original - min_val) / scale);
            float quantized = min_val + quantized_int * scale;

            float error = original - quantized;
            mse += error * error;
        }
    } else {
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};
        for (size_t i = 0; i < input->size; i++) {
            float original = et_get_float(input, indices);

            // 클램핑
            if (original < min_val) original = min_val;
            if (original > max_val) original = max_val;

            // 양자화
            int quantized_int = (int)roundf((original - min_val) / scale);
            float quantized = min_val + quantized_int * scale;

            float error = original - quantized;
            mse += error * error;

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    return mse / input->size;
}

// 음성 특화 범위 계산
static bool et_compute_voice_optimal_range(const ETTensor* input, ETDataType target_dtype,
                                          float* min_val, float* max_val) {
    // 음성 신호의 특성을 고려한 양자화 범위 계산

    // 기본 통계 계산
    float mean = 0.0f, variance = 0.0f;
    float global_min, global_max;

    if (!et_compute_minmax_range(input, &global_min, &global_max)) {
        return false;
    }

    const float* data = (const float*)input->data;

    // 평균 계산
    if (input->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            mean += data[i];
        }
    } else {
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};
        for (size_t i = 0; i < input->size; i++) {
            mean += et_get_float(input, indices);

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }
    mean /= input->size;

    // 분산 계산
    if (input->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            float diff = data[i] - mean;
            variance += diff * diff;
        }
    } else {
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};
        for (size_t i = 0; i < input->size; i++) {
            float val = et_get_float(input, indices);
            float diff = val - mean;
            variance += diff * diff;

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }
    variance /= input->size;
    float std_dev = sqrtf(variance);

    // 음성 신호 특성 고려
    // 1. 대부분의 음성 신호는 정규분포에 가까움
    // 2. 99.7% 데이터가 3σ 범위 내에 있음
    // 3. 극단값(클리핑 등)은 제거하는 것이 좋음

    float sigma_multiplier = (target_dtype == ET_INT8) ? 3.5f : 2.5f; // INT4는 더 보수적으로

    *min_val = fmaxf(global_min, mean - sigma_multiplier * std_dev);
    *max_val = fminf(global_max, mean + sigma_multiplier * std_dev);

    // 최소 범위 보장
    if (*max_val - *min_val < 1e-6f) {
        *min_val = global_min;
        *max_val = global_max;
    }

    return true;
}

// =============================================================================
// INT8 양자화 구현
// =============================================================================

// 고급 INT8 양자화 (정밀도 손실 최소화)
ETTensor* et_quantize_to_int8_advanced(const ETTensor* input, ETTensor* output,
                                      const ETQuantizationParams* params,
                                      const ETQuantizationOptions* options, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32) {
        return NULL;
    }

    // 양자화 파라미터 계산 또는 사용
    ETQuantizationParams computed_params;
    if (!params) {
        ETQuantizationOptions default_options = {
            .strategy = ET_QUANT_STRATEGY_VOICE_OPTIMIZED,
            .outlier_percentile = 0.1f,
            .symmetric = false,
            .per_channel = false,
            .channel_axis = 0,
            .smoothing_factor = 0.0f
        };

        const ETQuantizationOptions* use_options = options ? options : &default_options;

        if (!et_compute_quantization_params_advanced(input, ET_INT8, &computed_params, use_options)) {
            return NULL;
        }
        params = &computed_params;
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_INT8, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_INT8 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    const float* input_data = (const float*)input->data;
    int8_t* output_data = (int8_t*)output->data;

    // 정밀도 손실 최소화를 위한 개선된 양자화
    float inv_scale = 1.0f / params->scale;

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            float val = input_data[i];

            // 범위 클램핑 (정밀도 향상)
            if (val < params->min_val) val = params->min_val;
            if (val > params->max_val) val = params->max_val;

            // 개선된 반올림 (banker's rounding)
            float scaled = val * inv_scale + params->zero_point;
            int32_t quantized = (int32_t)et_round_to_nearest_even(scaled);

            // 클램핑
            if (quantized < -128) quantized = -128;
            if (quantized > 127) quantized = 127;

            output_data[i] = (int8_t)quantized;
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 변환
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            float val = et_get_float(input, indices);

            // 범위 클램핑
            if (val < params->min_val) val = params->min_val;
            if (val > params->max_val) val = params->max_val;

            float scaled = val * inv_scale + params->zero_point;
            int32_t quantized = (int32_t)et_round_to_nearest_even(scaled);

            // 클램핑
            if (quantized < -128) quantized = -128;
            if (quantized > 127) quantized = 127;

            // 출력 텐서에 설정
            void* out_ptr = et_get_ptr(output, indices);
            if (out_ptr) {
                *(int8_t*)out_ptr = (int8_t)quantized;
            }

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    return output;
}

ETTensor* et_quantize_to_int8(const ETTensor* input, ETTensor* output, const ETQuantizationParams* params, ETMemoryPool* pool) {
    return et_quantize_to_int8_advanced(input, output, params, NULL, pool);
}

ETTensor* et_dequantize_from_int8(const ETTensor* input, ETTensor* output, const ETQuantizationParams* params, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || !params || input->dtype != ET_INT8) {
        return NULL;
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_FLOAT32, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_FLOAT32 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    const int8_t* input_data = (const int8_t*)input->data;
    float* output_data = (float*)output->data;

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            int8_t quantized = input_data[i];
            float val = params->scale * (quantized - params->zero_point);
            output_data[i] = val;
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 변환
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            void* in_ptr = et_get_ptr(input, indices);
            if (in_ptr) {
                int8_t quantized = *(int8_t*)in_ptr;
                float val = params->scale * (quantized - params->zero_point);
                et_set_float(output, indices, val);
            }

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    return output;
}

// =============================================================================
// 정밀도 향상 유틸리티 함수
// =============================================================================

/**
 * @brief Banker's rounding (round half to even) 구현
 * 정밀도 손실을 최소화하기 위한 반올림 방법
 */
static float et_round_to_nearest_even(float x) {
    float floor_x = floorf(x);
    float frac = x - floor_x;

    if (frac < 0.5f) {
        return floor_x;
    } else if (frac > 0.5f) {
        return floor_x + 1.0f;
    } else {
        // 정확히 0.5인 경우, 짝수로 반올림
        int floor_int = (int)floor_x;
        if (floor_int % 2 == 0) {
            return floor_x;
        } else {
            return floor_x + 1.0f;
        }
    }
}

// ETQuantizationError는 이미 정적 함수 선언 부분에 정의됨

/**
 * @brief 양자화 오차 계산
 */
static bool et_compute_quantization_error(const ETTensor* original, const ETTensor* quantized,
                                          const ETQuantizationParams* params, ETQuantizationError* error) __attribute__((unused));

static bool et_compute_quantization_error(const ETTensor* original, const ETTensor* quantized,
                                          const ETQuantizationParams* params, ETQuantizationError* error) {
    if (!et_validate_tensor(original) || !et_validate_tensor(quantized) || !error) {
        return false;
    }

    if (!et_same_shape(original, quantized) || original->dtype != ET_FLOAT32) {
        return false;
    }

    const float* orig_data = (const float*)original->data;
    float* dequant_data = NULL;

    // 역양자화 수행
    ETTensor* dequantized = NULL;
    if (quantized->dtype == ET_INT8) {
        dequantized = et_dequantize_from_int8(quantized, NULL, params, original->pool);
    } else if (quantized->dtype == ET_INT4) {
        dequantized = et_dequantize_from_int4(quantized, NULL, params, original->pool);
    } else {
        return false;
    }

    if (!dequantized) return false;
    dequant_data = (float*)dequantized->data;

    // 오차 통계 계산
    float sum_squared_error = 0.0f;
    float sum_abs_error = 0.0f;
    float max_error = 0.0f;
    float sum_squared_signal = 0.0f;

    for (size_t i = 0; i < original->size; i++) {
        float orig = orig_data[i];
        float dequant = dequant_data[i];
        float err = fabsf(orig - dequant);

        sum_squared_error += err * err;
        sum_abs_error += err;
        if (err > max_error) max_error = err;
        sum_squared_signal += orig * orig;
    }

    error->mse = sum_squared_error / original->size;
    error->mae = sum_abs_error / original->size;
    error->max_error = max_error;

    // SNR 계산 (dB)
    if (sum_squared_error > 0.0f && sum_squared_signal > 0.0f) {
        error->snr_db = 10.0f * log10f(sum_squared_signal / sum_squared_error);
    } else {
        error->snr_db = INFINITY;
    }

    // 동적 범위 손실 계산
    float orig_range = params->max_val - params->min_val;
    float quant_levels = (quantized->dtype == ET_INT8) ? 256.0f : 16.0f;
    float quant_resolution = orig_range / quant_levels;
    error->dynamic_range_loss = (quant_resolution / orig_range) * 100.0f;

    et_destroy_tensor(dequantized);
    return true;
}

// =============================================================================
// INT4 패킹/언패킹 유틸리티
// =============================================================================

uint8_t et_pack_int4(uint8_t val1, uint8_t val2) {
    // 4비트 값들을 1바이트에 패킹
    // val1은 하위 4비트, val2는 상위 4비트
    return (val1 & 0x0F) | ((val2 & 0x0F) << 4);
}

void et_unpack_int4(uint8_t packed, uint8_t* val1, uint8_t* val2) {
    if (!val1 || !val2) return;

    *val1 = packed & 0x0F;        // 하위 4비트
    *val2 = (packed >> 4) & 0x0F; // 상위 4비트
}

// =============================================================================
// INT4 양자화 구현
// =============================================================================

// 고급 INT4 양자화 (정밀도 손실 최소화)
ETTensor* et_quantize_to_int4_advanced(const ETTensor* input, ETTensor* output,
                                      const ETQuantizationParams* params,
                                      const ETQuantizationOptions* options, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32) {
        return NULL;
    }

    // 양자화 파라미터 계산 또는 사용
    ETQuantizationParams computed_params;
    if (!params) {
        ETQuantizationOptions default_options = {
            .strategy = ET_QUANT_STRATEGY_VOICE_OPTIMIZED,
            .outlier_percentile = 0.2f, // INT4는 더 보수적으로
            .symmetric = true,          // INT4는 대칭 양자화가 더 효과적
            .per_channel = false,
            .channel_axis = 0,
            .smoothing_factor = 0.0f
        };

        const ETQuantizationOptions* use_options = options ? options : &default_options;

        if (!et_compute_quantization_params_advanced(input, ET_INT4, &computed_params, use_options)) {
            return NULL;
        }
        params = &computed_params;
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_INT4, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_INT4 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    const float* input_data = (const float*)input->data;
    uint8_t* output_data = (uint8_t*)output->data;

    float inv_scale = 1.0f / params->scale;

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i += 2) {
            // 첫 번째 값 양자화
            float val1 = input_data[i];
            if (val1 < params->min_val) val1 = params->min_val;
            if (val1 > params->max_val) val1 = params->max_val;

            float scaled1 = val1 * inv_scale + params->zero_point;
            int32_t quantized1 = (int32_t)et_round_to_nearest_even(scaled1);
            if (quantized1 < -8) quantized1 = -8;
            if (quantized1 > 7) quantized1 = 7;
            uint8_t q1 = (uint8_t)(quantized1 + 8); // -8~7을 0~15로 변환

            // 두 번째 값 양자화 (있는 경우)
            uint8_t q2 = 0;
            if (i + 1 < input->size) {
                float val2 = input_data[i + 1];
                if (val2 < params->min_val) val2 = params->min_val;
                if (val2 > params->max_val) val2 = params->max_val;

                float scaled2 = val2 * inv_scale + params->zero_point;
                int32_t quantized2 = (int32_t)et_round_to_nearest_even(scaled2);
                if (quantized2 < -8) quantized2 = -8;
                if (quantized2 > 7) quantized2 = 7;
                q2 = (uint8_t)(quantized2 + 8);
            }

            // 패킹하여 저장
            output_data[i / 2] = et_pack_int4(q1, q2);
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 변환
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i += 2) {
            // 첫 번째 값 양자화
            float val1 = et_get_float(input, indices);
            if (val1 < params->min_val) val1 = params->min_val;
            if (val1 > params->max_val) val1 = params->max_val;

            float scaled1 = val1 * inv_scale + params->zero_point;
            int32_t quantized1 = (int32_t)et_round_to_nearest_even(scaled1);
            if (quantized1 < -8) quantized1 = -8;
            if (quantized1 > 7) quantized1 = 7;
            uint8_t q1 = (uint8_t)(quantized1 + 8);

            // 다음 인덱스로 이동
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }

            // 두 번째 값 양자화 (있는 경우)
            uint8_t q2 = 0;
            if (i + 1 < input->size) {
                float val2 = et_get_float(input, indices);
                if (val2 < params->min_val) val2 = params->min_val;
                if (val2 > params->max_val) val2 = params->max_val;

                float scaled2 = val2 * inv_scale + params->zero_point;
                int32_t quantized2 = (int32_t)et_round_to_nearest_even(scaled2);
                if (quantized2 < -8) quantized2 = -8;
                if (quantized2 > 7) quantized2 = 7;
                q2 = (uint8_t)(quantized2 + 8);

                // 다음 인덱스로 이동
                for (int j = (int)input->ndim - 1; j >= 0; j--) {
                    indices[j]++;
                    if (indices[j] < input->shape[j]) break;
                    indices[j] = 0;
                }
            }

            // 패킹하여 저장
            output_data[i / 2] = et_pack_int4(q1, q2);
        }
    }

    return output;
}

ETTensor* et_quantize_to_int4(const ETTensor* input, ETTensor* output, const ETQuantizationParams* params, ETMemoryPool* pool) {
    return et_quantize_to_int4_advanced(input, output, params, NULL, pool);
}

ETTensor* et_dequantize_from_int4(const ETTensor* input, ETTensor* output, const ETQuantizationParams* params, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || !params || input->dtype != ET_INT4) {
        return NULL;
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_FLOAT32, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_FLOAT32 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    const uint8_t* input_data = (const uint8_t*)input->data;
    float* output_data = (float*)output->data;

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i += 2) {
            // 패킹된 값 언패킹
            uint8_t packed = input_data[i / 2];
            uint8_t q1, q2;
            et_unpack_int4(packed, &q1, &q2);

            // 첫 번째 값 역양자화
            int8_t quantized1 = (int8_t)q1 - 8; // 0~15를 -8~7로 변환
            float val1 = params->scale * (quantized1 - params->zero_point);
            output_data[i] = val1;

            // 두 번째 값 역양자화 (있는 경우)
            if (i + 1 < input->size) {
                int8_t quantized2 = (int8_t)q2 - 8; // 0~15를 -8~7로 변환
                float val2 = params->scale * (quantized2 - params->zero_point);
                output_data[i + 1] = val2;
            }
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 변환
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i += 2) {
            // 패킹된 값 언패킹
            uint8_t packed = input_data[i / 2];
            uint8_t q1, q2;
            et_unpack_int4(packed, &q1, &q2);

            // 첫 번째 값 역양자화
            int8_t quantized1 = (int8_t)q1 - 8;
            float val1 = params->scale * (quantized1 - params->zero_point);
            et_set_float(output, indices, val1);

            // 다음 인덱스로 이동
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }

            // 두 번째 값 역양자화 (있는 경우)
            if (i + 1 < input->size) {
                int8_t quantized2 = (int8_t)q2 - 8;
                float val2 = params->scale * (quantized2 - params->zero_point);
                et_set_float(output, indices, val2);

                // 다음 인덱스로 이동
                for (int j = (int)input->ndim - 1; j >= 0; j--) {
                    indices[j]++;
                    if (indices[j] < input->shape[j]) break;
                    indices[j] = 0;
                }
            }
        }
    }

    return output;
}

// =============================================================================
// 동적 양자화 구현
// =============================================================================

ETTensor* et_dynamic_quantize(const ETTensor* input, ETDataType target_dtype, ETTensor* output, ETQuantizationInfo* quant_info, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || !quant_info || input->dtype != ET_FLOAT32) {
        return NULL;
    }

    if (target_dtype != ET_INT8 && target_dtype != ET_INT4) {
        return NULL;
    }

    // 양자화 파라미터 계산
    ETQuantizationParams params;
    if (!et_compute_quantization_params(input, target_dtype, &params)) {
        return NULL;
    }

    // 양자화 정보 설정
    quant_info->quant_type = ET_QUANT_DYNAMIC;
    quant_info->params = params;
    quant_info->original_dtype = input->dtype;

    // 타입에 따른 양자화 수행
    if (target_dtype == ET_INT8) {
        return et_quantize_to_int8(input, output, &params, pool);
    } else { // ET_INT4
        return et_quantize_to_int4(input, output, &params, pool);
    }
}

ETTensor* et_dynamic_dequantize(const ETTensor* input, ETTensor* output, const ETQuantizationInfo* quant_info, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || !quant_info) {
        return NULL;
    }

    if (quant_info->quant_type != ET_QUANT_DYNAMIC) {
        return NULL;
    }

    // 타입에 따른 역양자화 수행
    if (input->dtype == ET_INT8) {
        return et_dequantize_from_int8(input, output, &quant_info->params, pool);
    } else if (input->dtype == ET_INT4) {
        return et_dequantize_from_int4(input, output, &quant_info->params, pool);
    } else {
        return NULL;
    }
}

// =============================================================================
// 음성 특화 BF16 양자화 파라미터 튜닝
// =============================================================================

/**
 * @brief 음성 합성 특화 BF16 양자화 파라미터 계산
 *
 * 음성 신호의 특성을 고려하여 BF16 양자화 파라미터를 최적화합니다.
 * 주파수 도메인과 시간 도메인 특성을 모두 고려합니다.
 */
bool et_compute_voice_optimized_bf16_params(const ETTensor* input, bool is_frequency_domain,
                                           float* scale_factor, float* bias_factor) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32 || !scale_factor || !bias_factor) {
        return false;
    }

    const float* data = (const float*)input->data;
    size_t size = input->size;

    // 기본값 설정
    *scale_factor = 1.0f;
    *bias_factor = 0.0f;

    if (size == 0) {
        return true;
    }

    // 통계 계산
    float mean = 0.0f;
    float variance = 0.0f;
    float min_val = data[0];
    float max_val = data[0];

    // 평균 및 최소/최대값 계산
    for (size_t i = 0; i < size; i++) {
        float val = data[i];
        mean += val;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }
    mean /= (float)size;

    // 분산 계산
    for (size_t i = 0; i < size; i++) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)size;

    float std_dev = sqrtf(variance);

    if (is_frequency_domain) {
        // 주파수 도메인: 로그 스케일 특성 고려
        // 음성 신호의 주파수 성분은 로그 스케일로 분포

        // 동적 범위 계산
        float dynamic_range = max_val - min_val;

        if (dynamic_range > 1e-8f) {
            // 99.9% 분위수를 사용하여 이상치 제거
            float percentile_99_9 = mean + 3.3f * std_dev;  // 약 99.9% 분위수
            float percentile_0_1 = mean - 3.3f * std_dev;   // 약 0.1% 분위수

            // 클램핑된 범위로 스케일 계산
            float effective_range = percentile_99_9 - percentile_0_1;
            if (effective_range > 1e-8f) {
                // BF16의 유효 정밀도 범위 고려 (약 7비트 가수)
                *scale_factor = 127.0f / effective_range;
                *bias_factor = -percentile_0_1 * (*scale_factor);
            }
        }

        // 주파수 도메인에서는 낮은 주파수가 더 중요하므로 약간의 바이어스 조정
        *bias_factor *= 0.9f;

    } else {
        // 시간 도메인: 신호의 동적 범위 보존
        float abs_max = fmaxf(fabsf(min_val), fabsf(max_val));

        if (abs_max > 1e-8f) {
            // 음성 신호의 피크를 고려한 스케일링
            // 일반적으로 음성 신호는 ±1.0 범위에 정규화되어 있음

            // RMS 기반 스케일링 (음성 신호의 에너지 고려)
            float rms = sqrtf(variance + mean * mean);
            float peak_to_rms_ratio = abs_max / (rms + 1e-8f);

            // 피크 대 RMS 비율이 높으면 (스파이크가 많으면) 더 보수적으로 스케일링
            float scale_adjustment = 1.0f / (1.0f + 0.1f * peak_to_rms_ratio);

            // BF16의 안전한 범위 내에서 스케일링
            float bf16_safe_range = 32768.0f; // BF16에서 안전한 정수 범위
            *scale_factor = (bf16_safe_range * scale_adjustment) / (abs_max * 1.2f); // 20% 여유

            // 시간 도메인에서는 일반적으로 바이어스 없음 (DC 성분 보존)
            *bias_factor = 0.0f;
        }
    }

    // 스케일 팩터 클램핑 (수치적 안정성)
    if (*scale_factor < 1e-6f) *scale_factor = 1e-6f;
    if (*scale_factor > 1e6f) *scale_factor = 1e6f;

    // 바이어스 팩터 클램핑
    if (*bias_factor < -10000.0f) *bias_factor = -10000.0f;
    if (*bias_factor > 10000.0f) *bias_factor = 10000.0f;

    return true;
}

/**
 * @brief 적응형 BF16 양자화 (음성 특화)
 *
 * 입력 데이터의 특성을 분석하여 최적의 BF16 양자화를 수행합니다.
 */
ETTensor* et_adaptive_quantize_to_bfloat16(const ETTensor* input, ETTensor* output,
                                          bool is_frequency_domain, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32) {
        return NULL;
    }

    // 음성 특화 파라미터 계산
    float scale_factor, bias_factor;
    if (!et_compute_voice_optimized_bf16_params(input, is_frequency_domain, &scale_factor, &bias_factor)) {
        // 파라미터 계산 실패시 기본 양자화 사용
        return et_quantize_to_bfloat16(input, output, pool);
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_BFLOAT16, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_BFLOAT16 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    const float* input_data = (const float*)input->data;
    uint16_t* output_data = (uint16_t*)output->data;

    // 스케일링 및 바이어스 적용 후 BF16 변환
    if (input->is_contiguous && output->is_contiguous) {
        // SIMD 최적화 버전 사용 (가능한 경우)
        if (fabsf(scale_factor - 1.0f) < 1e-6f && fabsf(bias_factor) < 1e-6f) {
            // 스케일링이 필요없는 경우 직접 변환
#if defined(LIBETUDE_ENABLE_SIMD) && LIBETUDE_ENABLE_SIMD
            simd_float32_to_bfloat16_optimal(input_data, output_data, input->size);
#else
            // Fallback implementation
            for (size_t i = 0; i < input->size; i++) {
                union {
                    float f;
                    uint32_t i;
                } u;
                u.f = input_data[i];
                output_data[i] = (uint16_t)(u.i >> 16);
            }
#endif
        } else {
            // 스케일링이 필요한 경우 - 하지만 너무 극단적인 스케일링은 피함
            float safe_scale = scale_factor;
            float safe_bias = bias_factor;

            // 안전한 범위로 클램핑
            if (safe_scale > 10.0f) safe_scale = 10.0f;
            if (safe_scale < 0.1f) safe_scale = 0.1f;
            if (safe_bias > 10.0f) safe_bias = 10.0f;
            if (safe_bias < -10.0f) safe_bias = -10.0f;

            for (size_t i = 0; i < input->size; i++) {
                float scaled_val = input_data[i] * safe_scale + safe_bias;

                union {
                    float f;
                    uint32_t i;
                } converter;

                converter.f = scaled_val;
                uint32_t bias = 0x00007FFF + ((converter.i >> 16) & 1);
                output_data[i] = (uint16_t)((converter.i + bias) >> 16);
            }
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 변환
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            float val = et_get_float(input, indices);
            float scaled_val = val * scale_factor + bias_factor;

            union {
                float f;
                uint32_t i;
            } converter;

            converter.f = scaled_val;
            uint32_t bias = 0x00007FFF + ((converter.i >> 16) & 1);
            uint16_t bf16_val = (uint16_t)((converter.i + bias) >> 16);

            // 출력 텐서에 설정
            void* out_ptr = et_get_ptr(output, indices);
            if (out_ptr) {
                *(uint16_t*)out_ptr = bf16_val;
            }

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    return output;
}

/**
 * @brief 배치별 적응형 BF16 양자화
 *
 * 배치 단위로 최적화된 BF16 양자화를 수행합니다.
 * 각 배치마다 독립적인 양자화 파라미터를 계산합니다.
 */
ETTensor* et_batch_adaptive_quantize_to_bfloat16(const ETTensor* input, ETTensor* output,
                                                bool is_frequency_domain, size_t batch_axis,
                                                ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32 || batch_axis >= input->ndim) {
        return NULL;
    }

    // 출력 텐서 생성 또는 검증
    if (!output) {
        output = et_create_tensor(pool ? pool : input->pool, ET_BFLOAT16, input->ndim, input->shape);
        if (!output) return NULL;
    } else {
        if (output->dtype != ET_BFLOAT16 || !et_same_shape(input, output)) {
            return NULL;
        }
    }

    size_t batch_size = input->shape[batch_axis];
    size_t batch_stride = input->strides[batch_axis] / sizeof(float);
    size_t elements_per_batch = input->size / batch_size;

    const float* input_data = (const float*)input->data;
    uint16_t* output_data = (uint16_t*)output->data;

    // 각 배치에 대해 독립적으로 양자화 수행
    for (size_t batch = 0; batch < batch_size; batch++) {
        const float* batch_input = input_data + batch * batch_stride;
        uint16_t* batch_output = output_data + batch * (batch_stride / 2); // BF16은 2바이트

        // 배치별 양자화 파라미터 계산
        float scale_factor, bias_factor;

        // 임시 텐서 생성 (배치 데이터용)
        size_t batch_shape[ET_MAX_TENSOR_DIMS];
        for (size_t i = 0; i < input->ndim; i++) {
            batch_shape[i] = (i == batch_axis) ? 1 : input->shape[i];
        }

        // 배치 통계 계산을 위한 간단한 구현
        float mean = 0.0f, variance = 0.0f, min_val = batch_input[0], max_val = batch_input[0];

        for (size_t i = 0; i < elements_per_batch; i++) {
            float val = batch_input[i];
            mean += val;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        mean /= (float)elements_per_batch;

        for (size_t i = 0; i < elements_per_batch; i++) {
            float diff = batch_input[i] - mean;
            variance += diff * diff;
        }
        variance /= (float)elements_per_batch;

        // 배치별 최적화된 파라미터 계산
        if (is_frequency_domain) {
            float std_dev = sqrtf(variance);
            float dynamic_range = max_val - min_val;

            if (dynamic_range > 1e-8f) {
                float percentile_99_9 = mean + 3.3f * std_dev;
                float percentile_0_1 = mean - 3.3f * std_dev;
                float effective_range = percentile_99_9 - percentile_0_1;

                if (effective_range > 1e-8f) {
                    scale_factor = 127.0f / effective_range;
                    bias_factor = -percentile_0_1 * scale_factor * 0.9f;
                } else {
                    scale_factor = 1.0f;
                    bias_factor = 0.0f;
                }
            } else {
                scale_factor = 1.0f;
                bias_factor = 0.0f;
            }
        } else {
            float abs_max = fmaxf(fabsf(min_val), fabsf(max_val));
            if (abs_max > 1e-8f) {
                float rms = sqrtf(variance + mean * mean);
                float peak_to_rms_ratio = abs_max / (rms + 1e-8f);
                float scale_adjustment = 1.0f / (1.0f + 0.1f * peak_to_rms_ratio);
                scale_factor = (32768.0f * scale_adjustment) / (abs_max * 1.2f);
                bias_factor = 0.0f;
            } else {
                scale_factor = 1.0f;
                bias_factor = 0.0f;
            }
        }

        // 파라미터 클램핑
        if (scale_factor < 1e-6f) scale_factor = 1e-6f;
        if (scale_factor > 1e6f) scale_factor = 1e6f;
        if (bias_factor < -10000.0f) bias_factor = -10000.0f;
        if (bias_factor > 10000.0f) bias_factor = 10000.0f;

        // 배치 데이터 양자화
        for (size_t i = 0; i < elements_per_batch; i++) {
            float scaled_val = batch_input[i] * scale_factor + bias_factor;

            union {
                float f;
                uint32_t i;
            } converter;

            converter.f = scaled_val;
            uint32_t bias = 0x00007FFF + ((converter.i >> 16) & 1);
            batch_output[i] = (uint16_t)((converter.i + bias) >> 16);
        }
    }

    return output;
}