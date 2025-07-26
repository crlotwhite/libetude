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

bool et_compute_quantization_params(const ETTensor* input, ETDataType target_dtype, ETQuantizationParams* params) {
    if (!et_validate_tensor(input) || !params || input->dtype != ET_FLOAT32) {
        return false;
    }

    if (target_dtype != ET_INT8 && target_dtype != ET_INT4) {
        return false;
    }

    // 텐서의 최소값과 최대값 찾기
    float min_val = FLT_MAX;
    float max_val = -FLT_MAX;

    const float* data = (const float*)input->data;

    if (input->is_contiguous) {
        // 연속 메모리인 경우 빠른 탐색
        for (size_t i = 0; i < input->size; i++) {
            float val = data[i];
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
    } else {
        // 비연속 메모리인 경우 인덱스 기반 탐색
        size_t indices[ET_MAX_TENSOR_DIMS] = {0};

        for (size_t i = 0; i < input->size; i++) {
            float val = et_get_float(input, indices);
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;

            // 다음 인덱스 계산
            for (int j = (int)input->ndim - 1; j >= 0; j--) {
                indices[j]++;
                if (indices[j] < input->shape[j]) break;
                indices[j] = 0;
            }
        }
    }

    // 양자화 범위 설정
    int32_t qmin, qmax;
    if (target_dtype == ET_INT8) {
        qmin = -128;
        qmax = 127;
    } else { // ET_INT4
        qmin = -8;
        qmax = 7;
    }

    // 스케일과 제로 포인트 계산
    float scale = (max_val - min_val) / (qmax - qmin);
    if (scale == 0.0f) {
        scale = 1.0f; // 모든 값이 같은 경우
    }

    int32_t zero_point = qmin - (int32_t)roundf(min_val / scale);

    // 제로 포인트 클램핑
    if (zero_point < qmin) zero_point = qmin;
    if (zero_point > qmax) zero_point = qmax;

    // 파라미터 설정
    params->scale = scale;
    params->zero_point = zero_point;
    params->min_val = min_val;
    params->max_val = max_val;

    return true;
}

// =============================================================================
// INT8 양자화 구현
// =============================================================================

ETTensor* et_quantize_to_int8(const ETTensor* input, ETTensor* output, const ETQuantizationParams* params, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32) {
        return NULL;
    }

    // 양자화 파라미터 계산 또는 사용
    ETQuantizationParams computed_params;
    if (!params) {
        if (!et_compute_quantization_params(input, ET_INT8, &computed_params)) {
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

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i++) {
            float val = input_data[i];
            int32_t quantized = (int32_t)roundf(val / params->scale) + params->zero_point;

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
            int32_t quantized = (int32_t)roundf(val / params->scale) + params->zero_point;

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

ETTensor* et_quantize_to_int4(const ETTensor* input, ETTensor* output, const ETQuantizationParams* params, ETMemoryPool* pool) {
    if (!et_validate_tensor(input) || input->dtype != ET_FLOAT32) {
        return NULL;
    }

    // 양자화 파라미터 계산 또는 사용
    ETQuantizationParams computed_params;
    if (!params) {
        if (!et_compute_quantization_params(input, ET_INT4, &computed_params)) {
            return NULL;
        }
        params = &computed_params;
    }

    // INT4는 2개 요소당 1바이트이므로 크기 계산
    // size_t packed_size = (input->size + 1) / 2; // 홀수 개수면 올림 (사용하지 않음)

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

    // 연속 메모리인 경우 빠른 변환
    if (input->is_contiguous && output->is_contiguous) {
        for (size_t i = 0; i < input->size; i += 2) {
            // 첫 번째 값 양자화
            float val1 = input_data[i];
            int32_t quantized1 = (int32_t)roundf(val1 / params->scale) + params->zero_point;
            if (quantized1 < -8) quantized1 = -8;
            if (quantized1 > 7) quantized1 = 7;
            uint8_t q1 = (uint8_t)(quantized1 + 8); // -8~7을 0~15로 변환

            // 두 번째 값 양자화 (있는 경우)
            uint8_t q2 = 0;
            if (i + 1 < input->size) {
                float val2 = input_data[i + 1];
                int32_t quantized2 = (int32_t)roundf(val2 / params->scale) + params->zero_point;
                if (quantized2 < -8) quantized2 = -8;
                if (quantized2 > 7) quantized2 = 7;
                q2 = (uint8_t)(quantized2 + 8); // -8~7을 0~15로 변환
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
            int32_t quantized1 = (int32_t)roundf(val1 / params->scale) + params->zero_point;
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
                int32_t quantized2 = (int32_t)roundf(val2 / params->scale) + params->zero_point;
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
            simd_float32_to_bfloat16_optimal(input_data, output_data, input->size);
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