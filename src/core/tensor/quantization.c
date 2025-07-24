/**
 * @file quantization.c
 * @brief LibEtude 텐서 양자화 구현
 *
 * BF16, INT8, INT4 양자화 및 동적 양자화를 구현합니다.
 * 음성 합성에 최적화된 양자화 전략을 제공합니다.
 */

#include "libetude/tensor.h"
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
    size_t packed_size = (input->size + 1) / 2; // 홀수 개수면 올림

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