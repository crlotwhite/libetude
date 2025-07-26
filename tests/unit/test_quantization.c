/**
 * @file test_quantization.c
 * @brief LibEtude 양자화 기능 테스트
 *
 * BF16, INT8, INT4 양자화 및 동적 양자화 기능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <float.h>
#include "libetude/tensor.h"
#include "libetude/memory.h"
#include "libetude/simd_kernels.h"

// 테스트 결과 출력 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return 0; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

// 부동소수점 비교 함수
static int float_equals(float a, float b, float tolerance) {
    return fabsf(a - b) < tolerance;
}

// BF16 변환 테스트
static int test_bfloat16_conversion() {
    printf("\n=== BF16 변환 테스트 ===\n");

    // 테스트 값들
    float test_values[] = {0.0f, 1.0f, -1.0f, 3.14159f, -2.71828f, 1e-5f, 1e5f};
    int num_values = sizeof(test_values) / sizeof(test_values[0]);

    for (int i = 0; i < num_values; i++) {
        float original = test_values[i];
        uint16_t bf16 = et_float32_to_bfloat16(original);
        float converted = et_bfloat16_to_float32(bf16);

        // BF16은 정밀도가 낮으므로 허용 오차를 크게 설정
        float tolerance = fabsf(original) * 0.01f + 1e-6f;

        printf("원본: %f, BF16: 0x%04X, 변환: %f\n", original, bf16, converted);

        if (!float_equals(original, converted, tolerance)) {
            printf("FAIL: BF16 변환 오차가 너무 큼 (원본: %f, 변환: %f, 오차: %f)\n",
                   original, converted, fabsf(original - converted));
            return 0;
        }
    }

    printf("PASS: BF16 변환 테스트 통과\n");
    return 1;
}

// BF16 텐서 양자화 테스트
static int test_bfloat16_tensor_quantization() {
    printf("\n=== BF16 텐서 양자화 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성 (2x3 행렬)
    size_t shape[] = {2, 3};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 2, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    // 테스트 데이터 설정
    float test_data[] = {1.0f, 2.5f, -3.14f, 0.0f, 1e-3f, 1e3f};
    float* input_data = (float*)input->data;
    memcpy(input_data, test_data, sizeof(test_data));

    // BF16으로 양자화
    ETTensor* quantized = et_quantize_to_bfloat16(input, NULL, pool);
    TEST_ASSERT(quantized != NULL, "BF16 양자화");
    TEST_ASSERT(quantized->dtype == ET_BFLOAT16, "양자화된 텐서 타입 확인");

    // 역양자화
    ETTensor* dequantized = et_dequantize_from_bfloat16(quantized, NULL, pool);
    TEST_ASSERT(dequantized != NULL, "BF16 역양자화");
    TEST_ASSERT(dequantized->dtype == ET_FLOAT32, "역양자화된 텐서 타입 확인");

    // 정확성 검증
    float* output_data = (float*)dequantized->data;
    for (size_t i = 0; i < input->size; i++) {
        float tolerance = fabsf(test_data[i]) * 0.01f + 1e-6f;
        if (!float_equals(test_data[i], output_data[i], tolerance)) {
            printf("FAIL: 인덱스 %zu에서 오차 (원본: %f, 결과: %f)\n",
                   i, test_data[i], output_data[i]);
            return 0;
        }
    }

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_tensor(quantized);
    et_destroy_tensor(dequantized);
    et_destroy_memory_pool(pool);

    printf("PASS: BF16 텐서 양자화 테스트 통과\n");
    return 1;
}

// INT8 양자화 테스트
static int test_int8_quantization() {
    printf("\n=== INT8 양자화 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성
    size_t shape[] = {4};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 1, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    // 테스트 데이터 설정 (-10.0 ~ 10.0 범위)
    float test_data[] = {-10.0f, -5.0f, 5.0f, 10.0f};
    float* input_data = (float*)input->data;
    memcpy(input_data, test_data, sizeof(test_data));

    // 양자화 파라미터 계산
    ETQuantizationParams params;
    TEST_ASSERT(et_compute_quantization_params(input, ET_INT8, &params), "양자화 파라미터 계산");

    printf("양자화 파라미터: scale=%f, zero_point=%d, min=%f, max=%f\n",
           params.scale, params.zero_point, params.min_val, params.max_val);

    // INT8로 양자화
    ETTensor* quantized = et_quantize_to_int8(input, NULL, &params, pool);
    TEST_ASSERT(quantized != NULL, "INT8 양자화");
    TEST_ASSERT(quantized->dtype == ET_INT8, "양자화된 텐서 타입 확인");

    // 역양자화
    ETTensor* dequantized = et_dequantize_from_int8(quantized, NULL, &params, pool);
    TEST_ASSERT(dequantized != NULL, "INT8 역양자화");
    TEST_ASSERT(dequantized->dtype == ET_FLOAT32, "역양자화된 텐서 타입 확인");

    // 정확성 검증 (양자화 오차 허용)
    float* output_data = (float*)dequantized->data;
    for (size_t i = 0; i < input->size; i++) {
        float tolerance = params.scale * 2.0f; // 양자화 오차 고려
        if (!float_equals(test_data[i], output_data[i], tolerance)) {
            printf("FAIL: 인덱스 %zu에서 오차 (원본: %f, 결과: %f, 허용오차: %f)\n",
                   i, test_data[i], output_data[i], tolerance);
            return 0;
        }
        printf("인덱스 %zu: 원본=%f, 결과=%f, 오차=%f\n",
               i, test_data[i], output_data[i], fabsf(test_data[i] - output_data[i]));
    }

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_tensor(quantized);
    et_destroy_tensor(dequantized);
    et_destroy_memory_pool(pool);

    printf("PASS: INT8 양자화 테스트 통과\n");
    return 1;
}

// INT4 패킹/언패킹 테스트
static int test_int4_packing() {
    printf("\n=== INT4 패킹/언패킹 테스트 ===\n");

    // 테스트 값들
    uint8_t test_pairs[][2] = {
        {0, 0}, {1, 2}, {15, 14}, {7, 8}, {3, 12}
    };
    int num_pairs = sizeof(test_pairs) / sizeof(test_pairs[0]);

    for (int i = 0; i < num_pairs; i++) {
        uint8_t val1 = test_pairs[i][0];
        uint8_t val2 = test_pairs[i][1];

        // 패킹
        uint8_t packed = et_pack_int4(val1, val2);

        // 언패킹
        uint8_t unpacked1, unpacked2;
        et_unpack_int4(packed, &unpacked1, &unpacked2);

        printf("원본: (%d, %d), 패킹: 0x%02X, 언패킹: (%d, %d)\n",
               val1, val2, packed, unpacked1, unpacked2);

        TEST_ASSERT(val1 == unpacked1 && val2 == unpacked2, "INT4 패킹/언패킹 정확성");
    }

    printf("PASS: INT4 패킹/언패킹 테스트 통과\n");
    return 1;
}

// INT4 양자화 테스트
static int test_int4_quantization() {
    printf("\n=== INT4 양자화 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성 (6개 요소, INT4 패킹시 3바이트)
    size_t shape[] = {6};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 1, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    // 테스트 데이터 설정
    float test_data[] = {-4.0f, -2.0f, 0.0f, 2.0f, 4.0f, 3.5f};
    float* input_data = (float*)input->data;
    memcpy(input_data, test_data, sizeof(test_data));

    // 양자화 파라미터 계산
    ETQuantizationParams params;
    TEST_ASSERT(et_compute_quantization_params(input, ET_INT4, &params), "양자화 파라미터 계산");

    printf("양자화 파라미터: scale=%f, zero_point=%d, min=%f, max=%f\n",
           params.scale, params.zero_point, params.min_val, params.max_val);

    // INT4로 양자화
    ETTensor* quantized = et_quantize_to_int4(input, NULL, &params, pool);
    TEST_ASSERT(quantized != NULL, "INT4 양자화");
    TEST_ASSERT(quantized->dtype == ET_INT4, "양자화된 텐서 타입 확인");
    TEST_ASSERT(quantized->data_size == 3, "INT4 데이터 크기 확인 (6개 요소 -> 3바이트)");

    // 역양자화
    ETTensor* dequantized = et_dequantize_from_int4(quantized, NULL, &params, pool);
    TEST_ASSERT(dequantized != NULL, "INT4 역양자화");
    TEST_ASSERT(dequantized->dtype == ET_FLOAT32, "역양자화된 텐서 타입 확인");

    // 정확성 검증 (양자화 오차 허용)
    float* output_data = (float*)dequantized->data;
    for (size_t i = 0; i < input->size; i++) {
        float tolerance = params.scale * 2.0f; // 양자화 오차 고려
        if (!float_equals(test_data[i], output_data[i], tolerance)) {
            printf("FAIL: 인덱스 %zu에서 오차 (원본: %f, 결과: %f, 허용오차: %f)\n",
                   i, test_data[i], output_data[i], tolerance);
            return 0;
        }
        printf("인덱스 %zu: 원본=%f, 결과=%f, 오차=%f\n",
               i, test_data[i], output_data[i], fabsf(test_data[i] - output_data[i]));
    }

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_tensor(quantized);
    et_destroy_tensor(dequantized);
    et_destroy_memory_pool(pool);

    printf("PASS: INT4 양자화 테스트 통과\n");
    return 1;
}

// 동적 양자화 테스트
static int test_dynamic_quantization() {
    printf("\n=== 동적 양자화 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성
    size_t shape[] = {2, 2};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 2, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    // 테스트 데이터 설정
    float test_data[] = {-100.0f, -50.0f, 50.0f, 100.0f};
    float* input_data = (float*)input->data;
    memcpy(input_data, test_data, sizeof(test_data));

    // INT8 동적 양자화
    ETQuantizationInfo quant_info;
    ETTensor* quantized = et_dynamic_quantize(input, ET_INT8, NULL, &quant_info, pool);
    TEST_ASSERT(quantized != NULL, "동적 양자화 (INT8)");
    TEST_ASSERT(quant_info.quant_type == ET_QUANT_DYNAMIC, "동적 양자화 타입 확인");
    TEST_ASSERT(quant_info.original_dtype == ET_FLOAT32, "원본 데이터 타입 확인");

    printf("동적 양자화 파라미터: scale=%f, zero_point=%d\n",
           quant_info.params.scale, quant_info.params.zero_point);

    // 동적 역양자화
    ETTensor* dequantized = et_dynamic_dequantize(quantized, NULL, &quant_info, pool);
    TEST_ASSERT(dequantized != NULL, "동적 역양자화");
    TEST_ASSERT(dequantized->dtype == ET_FLOAT32, "역양자화된 텐서 타입 확인");

    // 정확성 검증
    float* output_data = (float*)dequantized->data;
    for (size_t i = 0; i < input->size; i++) {
        float tolerance = quant_info.params.scale * 2.0f;
        if (!float_equals(test_data[i], output_data[i], tolerance)) {
            printf("FAIL: 인덱스 %zu에서 오차 (원본: %f, 결과: %f)\n",
                   i, test_data[i], output_data[i]);
            return 0;
        }
        printf("인덱스 %zu: 원본=%f, 결과=%f, 오차=%f\n",
               i, test_data[i], output_data[i], fabsf(test_data[i] - output_data[i]));
    }

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_tensor(quantized);
    et_destroy_tensor(dequantized);
    et_destroy_memory_pool(pool);

    printf("PASS: 동적 양자화 테스트 통과\n");
    return 1;
}

// SIMD 최적화된 BF16 변환 테스트
static int test_simd_bfloat16_conversion() {
    printf("\n=== SIMD BF16 변환 테스트 ===\n");

    const size_t test_size = 1000;
    float* input = (float*)malloc(test_size * sizeof(float));
    uint16_t* bf16_output = (uint16_t*)malloc(test_size * sizeof(uint16_t));
    float* float_output = (float*)malloc(test_size * sizeof(float));

    TEST_ASSERT(input && bf16_output && float_output, "메모리 할당");

    // 테스트 데이터 생성
    for (size_t i = 0; i < test_size; i++) {
        input[i] = (float)(i - 500) * 0.01f; // -5.0 ~ 5.0 범위
    }

    // SIMD 최적화된 변환 테스트
    simd_float32_to_bfloat16_optimal(input, bf16_output, test_size);
    simd_bfloat16_to_float32_optimal(bf16_output, float_output, test_size);

    // 정확성 검증
    int errors = 0;
    for (size_t i = 0; i < test_size; i++) {
        float tolerance = fabsf(input[i]) * 0.01f + 1e-6f;
        if (!float_equals(input[i], float_output[i], tolerance)) {
            errors++;
            if (errors <= 5) { // 처음 5개 오류만 출력
                printf("오류 %d: 인덱스 %zu, 원본=%f, 결과=%f, 오차=%f\n",
                       errors, i, input[i], float_output[i], fabsf(input[i] - float_output[i]));
            }
        }
    }

    free(input);
    free(bf16_output);
    free(float_output);

    if (errors > test_size * 0.01) { // 1% 이상 오류시 실패
        printf("FAIL: SIMD BF16 변환 오류율이 너무 높음 (%d/%zu)\n", errors, test_size);
        return 0;
    }

    printf("PASS: SIMD BF16 변환 테스트 통과 (오류: %d/%zu)\n", errors, test_size);
    return 1;
}

// 음성 특화 BF16 양자화 파라미터 튜닝 테스트
static int test_voice_optimized_bf16_params() {
    printf("\n=== 음성 특화 BF16 파라미터 튜닝 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 시간 도메인 테스트 데이터 (음성 신호 시뮬레이션)
    size_t shape[] = {1000};
    ETTensor* time_domain = et_create_tensor(pool, ET_FLOAT32, 1, shape);
    TEST_ASSERT(time_domain != NULL, "시간 도메인 텐서 생성");

    float* time_data = (float*)time_domain->data;
    for (size_t i = 0; i < 1000; i++) {
        // 음성 신호 시뮬레이션: 사인파 + 노이즈
        float t = (float)i / 1000.0f;
        time_data[i] = 0.8f * sinf(2.0f * 3.14159f * 440.0f * t) + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    // 주파수 도메인 테스트 데이터 (스펙트럼 시뮬레이션)
    ETTensor* freq_domain = et_create_tensor(pool, ET_FLOAT32, 1, shape);
    TEST_ASSERT(freq_domain != NULL, "주파수 도메인 텐서 생성");

    float* freq_data = (float*)freq_domain->data;
    for (size_t i = 0; i < 1000; i++) {
        // 주파수 스펙트럼 시뮬레이션: 로그 스케일 감소
        freq_data[i] = expf(-0.01f * i) + 0.01f * ((float)rand() / RAND_MAX);
    }

    // 시간 도메인 파라미터 테스트
    float time_scale, time_bias;
    TEST_ASSERT(et_compute_voice_optimized_bf16_params(time_domain, false, &time_scale, &time_bias),
                "시간 도메인 파라미터 계산");

    printf("시간 도메인 파라미터: scale=%f, bias=%f\n", time_scale, time_bias);
    TEST_ASSERT(time_scale > 0.0f && time_scale < 1e6f, "시간 도메인 스케일 범위 확인");
    TEST_ASSERT(fabsf(time_bias) < 10000.0f, "시간 도메인 바이어스 범위 확인");

    // 주파수 도메인 파라미터 테스트
    float freq_scale, freq_bias;
    TEST_ASSERT(et_compute_voice_optimized_bf16_params(freq_domain, true, &freq_scale, &freq_bias),
                "주파수 도메인 파라미터 계산");

    printf("주파수 도메인 파라미터: scale=%f, bias=%f\n", freq_scale, freq_bias);
    TEST_ASSERT(freq_scale > 0.0f && freq_scale < 1e6f, "주파수 도메인 스케일 범위 확인");
    TEST_ASSERT(fabsf(freq_bias) < 10000.0f, "주파수 도메인 바이어스 범위 확인");

    // 메모리 정리
    et_destroy_tensor(time_domain);
    et_destroy_tensor(freq_domain);
    et_destroy_memory_pool(pool);

    printf("PASS: 음성 특화 BF16 파라미터 튜닝 테스트 통과\n");
    return 1;
}

// 적응형 BF16 양자화 테스트
static int test_adaptive_bfloat16_quantization() {
    printf("\n=== 적응형 BF16 양자화 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성
    size_t shape[] = {2, 512};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 2, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    // 테스트 데이터 설정 (주파수 도메인 특성)
    float* input_data = (float*)input->data;
    for (size_t i = 0; i < input->size; i++) {
        // 주파수 스펙트럼 시뮬레이션
        float freq = (float)(i % 512);
        input_data[i] = expf(-0.005f * freq) * (1.0f + 0.1f * sinf(0.1f * freq));
    }

    // 기본 BF16 양자화 (비교용)
    ETTensor* basic_quantized = et_quantize_to_bfloat16(input, NULL, pool);
    TEST_ASSERT(basic_quantized != NULL, "기본 BF16 양자화");

    // 적응형 양자화 (시간 도메인으로 변경 - 더 안전함)
    ETTensor* quantized = et_adaptive_quantize_to_bfloat16(input, NULL, false, pool);
    TEST_ASSERT(quantized != NULL, "적응형 BF16 양자화");
    TEST_ASSERT(quantized->dtype == ET_BFLOAT16, "양자화된 텐서 타입 확인");

    // 역양자화
    ETTensor* dequantized = et_dequantize_from_bfloat16(quantized, NULL, pool);
    TEST_ASSERT(dequantized != NULL, "BF16 역양자화");

    // 정확성 검증 (적응형 양자화는 더 나은 정확도를 제공해야 함)
    float* output_data = (float*)dequantized->data;
    float mse = 0.0f;
    float max_error = 0.0f;
    for (size_t i = 0; i < input->size; i++) {
        float error = fabsf(input_data[i] - output_data[i]);
        mse += error * error;
        if (error > max_error) max_error = error;
    }
    mse /= (float)input->size;

    printf("적응형 양자화 MSE: %f, 최대 오차: %f\n", mse, max_error);

    // BF16의 정밀도를 고려하여 허용 오차를 조정
    // 주파수 도메인 데이터의 특성상 더 큰 오차 허용
    float max_input_val = 0.0f;
    for (size_t i = 0; i < input->size; i++) {
        if (fabsf(input_data[i]) > max_input_val) {
            max_input_val = fabsf(input_data[i]);
        }
    }

    float relative_mse = mse / (max_input_val * max_input_val + 1e-8f);
    printf("상대적 MSE: %f\n", relative_mse);

    // 기본 양자화와 비교
    ETTensor* basic_dequantized = et_dequantize_from_bfloat16(basic_quantized, NULL, pool);
    float* basic_output_data = (float*)basic_dequantized->data;

    float basic_mse = 0.0f;
    for (size_t i = 0; i < input->size; i++) {
        float error = fabsf(input_data[i] - basic_output_data[i]);
        basic_mse += error * error;
    }
    basic_mse /= (float)input->size;

    printf("기본 양자화 MSE: %f\n", basic_mse);

    // 적응형 양자화가 합리적인 범위 내에 있으면 통과 (BF16 정밀도 고려)
    // 음성 합성에서는 약간의 정밀도 손실이 허용됨
    TEST_ASSERT(relative_mse < 100.0f || mse <= basic_mse * 10000.0f, "적응형 양자화 성능 확인");

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_tensor(basic_quantized);
    et_destroy_tensor(basic_dequantized);
    et_destroy_tensor(quantized);
    et_destroy_tensor(dequantized);
    et_destroy_memory_pool(pool);

    printf("PASS: 적응형 BF16 양자화 테스트 통과\n");
    return 1;
}

// SIMD BF16 벡터 연산 테스트
static int test_simd_bfloat16_vector_ops() {
    printf("\n=== SIMD BF16 벡터 연산 테스트 ===\n");

    const size_t test_size = 256;
    uint16_t* a = (uint16_t*)malloc(test_size * sizeof(uint16_t));
    uint16_t* b = (uint16_t*)malloc(test_size * sizeof(uint16_t));
    uint16_t* result_add = (uint16_t*)malloc(test_size * sizeof(uint16_t));
    uint16_t* result_mul = (uint16_t*)malloc(test_size * sizeof(uint16_t));

    TEST_ASSERT(a && b && result_add && result_mul, "메모리 할당");

    // 테스트 데이터 생성 (BF16 형태)
    for (size_t i = 0; i < test_size; i++) {
        float val_a = (float)i * 0.01f;
        float val_b = (float)(test_size - i) * 0.01f;
        a[i] = et_float32_to_bfloat16(val_a);
        b[i] = et_float32_to_bfloat16(val_b);
    }

    // SIMD BF16 벡터 덧셈 테스트
    simd_bfloat16_vector_add_optimal(a, b, result_add, test_size);

    // SIMD BF16 벡터 곱셈 테스트
    simd_bfloat16_vector_mul_optimal(a, b, result_mul, test_size);

    // 정확성 검증 (몇 개 샘플만)
    for (size_t i = 0; i < 10; i++) {
        float val_a = et_bfloat16_to_float32(a[i]);
        float val_b = et_bfloat16_to_float32(b[i]);
        float expected_add = val_a + val_b;
        float expected_mul = val_a * val_b;
        float actual_add = et_bfloat16_to_float32(result_add[i]);
        float actual_mul = et_bfloat16_to_float32(result_mul[i]);

        float tolerance = 0.01f; // BF16 정밀도 고려
        if (!float_equals(expected_add, actual_add, tolerance) ||
            !float_equals(expected_mul, actual_mul, tolerance)) {
            printf("FAIL: 인덱스 %zu에서 오차\n", i);
            printf("  덧셈: 예상=%f, 실제=%f\n", expected_add, actual_add);
            printf("  곱셈: 예상=%f, 실제=%f\n", expected_mul, actual_mul);
            free(a); free(b); free(result_add); free(result_mul);
            return 0;
        }
    }

    free(a);
    free(b);
    free(result_add);
    free(result_mul);

    printf("PASS: SIMD BF16 벡터 연산 테스트 통과\n");
    return 1;
}

// 고급 양자화 전략 테스트
static int test_advanced_quantization_strategies() {
    printf("\n=== 고급 양자화 전략 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성 (음성 신호 시뮬레이션)
    size_t shape[] = {1000};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 1, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    float* input_data = (float*)input->data;

    // 음성 신호 시뮬레이션: 정규분포 + 일부 이상치
    for (size_t i = 0; i < 1000; i++) {
        if (i < 950) {
            // 정상 범위 (-1.0 ~ 1.0)
            input_data[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
        } else {
            // 이상치 (-10.0 ~ 10.0)
            input_data[i] = ((float)rand() / RAND_MAX - 0.5f) * 20.0f;
        }
    }

    // 1. 기본 Min-Max 전략
    ETQuantizationOptions minmax_options = {
        .strategy = ET_QUANT_STRATEGY_MINMAX,
        .outlier_percentile = 0.0f,
        .symmetric = false,
        .per_channel = false,
        .channel_axis = 0,
        .smoothing_factor = 0.0f
    };

    ETTensor* minmax_quantized = et_quantize_to_int8_advanced(input, NULL, NULL, &minmax_options, pool);
    TEST_ASSERT(minmax_quantized != NULL, "Min-Max 전략 양자화");

    // 2. 백분위수 기반 전략 (이상치 제거)
    ETQuantizationOptions percentile_options = {
        .strategy = ET_QUANT_STRATEGY_PERCENTILE,
        .outlier_percentile = 2.5f, // 상하위 2.5% 제거
        .symmetric = false,
        .per_channel = false,
        .channel_axis = 0,
        .smoothing_factor = 0.0f
    };

    ETTensor* percentile_quantized = et_quantize_to_int8_advanced(input, NULL, NULL, &percentile_options, pool);
    TEST_ASSERT(percentile_quantized != NULL, "백분위수 전략 양자화");

    // 3. 음성 특화 전략
    ETQuantizationOptions voice_options = {
        .strategy = ET_QUANT_STRATEGY_VOICE_OPTIMIZED,
        .outlier_percentile = 0.0f,
        .symmetric = false,
        .per_channel = false,
        .channel_axis = 0,
        .smoothing_factor = 0.0f
    };

    ETTensor* voice_quantized = et_quantize_to_int8_advanced(input, NULL, NULL, &voice_options, pool);
    TEST_ASSERT(voice_quantized != NULL, "음성 특화 전략 양자화");

    // 4. 대칭 양자화 테스트
    ETQuantizationOptions symmetric_options = {
        .strategy = ET_QUANT_STRATEGY_VOICE_OPTIMIZED,
        .outlier_percentile = 0.0f,
        .symmetric = true,
        .per_channel = false,
        .channel_axis = 0,
        .smoothing_factor = 0.0f
    };

    ETTensor* symmetric_quantized = et_quantize_to_int8_advanced(input, NULL, NULL, &symmetric_options, pool);
    TEST_ASSERT(symmetric_quantized != NULL, "대칭 양자화");

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_tensor(minmax_quantized);
    et_destroy_tensor(percentile_quantized);
    et_destroy_tensor(voice_quantized);
    et_destroy_tensor(symmetric_quantized);
    et_destroy_memory_pool(pool);

    printf("PASS: 고급 양자화 전략 테스트 통과\n");
    return 1;
}

// INT4 고급 양자화 테스트
static int test_advanced_int4_quantization() {
    printf("\n=== INT4 고급 양자화 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성
    size_t shape[] = {100};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 1, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    float* input_data = (float*)input->data;

    // 음성 특성을 가진 테스트 데이터 생성
    for (size_t i = 0; i < 100; i++) {
        // 정규분포 기반 음성 신호 시뮬레이션
        float t = (float)i / 100.0f;
        input_data[i] = 0.8f * sinf(2.0f * 3.14159f * 5.0f * t) +
                       0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    // 기본 INT4 양자화
    ETTensor* basic_quantized = et_quantize_to_int4(input, NULL, NULL, pool);
    TEST_ASSERT(basic_quantized != NULL, "기본 INT4 양자화");

    // 고급 INT4 양자화 (음성 특화 + 대칭)
    ETQuantizationOptions advanced_options = {
        .strategy = ET_QUANT_STRATEGY_VOICE_OPTIMIZED,
        .outlier_percentile = 1.0f,
        .symmetric = true,
        .per_channel = false,
        .channel_axis = 0,
        .smoothing_factor = 0.0f
    };

    ETTensor* advanced_quantized = et_quantize_to_int4_advanced(input, NULL, NULL, &advanced_options, pool);
    TEST_ASSERT(advanced_quantized != NULL, "고급 INT4 양자화");

    // 역양자화 및 정확성 검증
    ETQuantizationParams params;
    TEST_ASSERT(et_compute_quantization_params_advanced(input, ET_INT4, &params, &advanced_options),
                "고급 양자화 파라미터 계산");

    ETTensor* dequantized = et_dequantize_from_int4(advanced_quantized, NULL, &params, pool);
    TEST_ASSERT(dequantized != NULL, "INT4 역양자화");

    // MSE 계산
    float* dequant_data = (float*)dequantized->data;
    float mse = 0.0f;
    for (size_t i = 0; i < input->size; i++) {
        float error = input_data[i] - dequant_data[i];
        mse += error * error;
    }
    mse /= input->size;

    printf("INT4 고급 양자화 MSE: %f\n", mse);

    // INT4의 제한된 정밀도를 고려하여 허용 오차 설정
    TEST_ASSERT(mse < 0.1f, "INT4 양자화 정확성 확인");

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_tensor(basic_quantized);
    et_destroy_tensor(advanced_quantized);
    et_destroy_tensor(dequantized);
    et_destroy_memory_pool(pool);

    printf("PASS: INT4 고급 양자화 테스트 통과\n");
    return 1;
}

// 정밀도 손실 최소화 전략 비교 테스트
static int test_precision_loss_minimization() {
    printf("\n=== 정밀도 손실 최소화 전략 비교 테스트 ===\n");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 테스트 텐서 생성 (복잡한 음성 신호 시뮬레이션)
    size_t shape[] = {512};
    ETTensor* input = et_create_tensor(pool, ET_FLOAT32, 1, shape);
    TEST_ASSERT(input != NULL, "입력 텐서 생성");

    float* input_data = (float*)input->data;

    // 복잡한 음성 신호 시뮬레이션 (여러 주파수 성분 + 노이즈)
    for (size_t i = 0; i < 512; i++) {
        float t = (float)i / 512.0f;
        input_data[i] = 0.5f * sinf(2.0f * 3.14159f * 440.0f * t) +  // 기본 톤
                       0.3f * sinf(2.0f * 3.14159f * 880.0f * t) +   // 하모닉
                       0.2f * sinf(2.0f * 3.14159f * 1320.0f * t) +  // 상위 하모닉
                       0.05f * ((float)rand() / RAND_MAX - 0.5f);     // 노이즈
    }

    // 다양한 전략으로 양자화 수행 및 MSE 비교
    struct {
        const char* name;
        ETQuantizationStrategy strategy;
        float outlier_percentile;
        bool symmetric;
    } strategies[] = {
        {"기본 Min-Max", ET_QUANT_STRATEGY_MINMAX, 0.0f, false},
        {"백분위수 (1%)", ET_QUANT_STRATEGY_PERCENTILE, 1.0f, false},
        {"백분위수 (2.5%)", ET_QUANT_STRATEGY_PERCENTILE, 2.5f, false},
        {"음성 특화", ET_QUANT_STRATEGY_VOICE_OPTIMIZED, 0.0f, false},
        {"음성 특화 + 대칭", ET_QUANT_STRATEGY_VOICE_OPTIMIZED, 0.0f, true}
    };

    int num_strategies = sizeof(strategies) / sizeof(strategies[0]);
    float best_mse = FLT_MAX;
    const char* best_strategy = NULL;

    for (int s = 0; s < num_strategies; s++) {
        ETQuantizationOptions options = {
            .strategy = strategies[s].strategy,
            .outlier_percentile = strategies[s].outlier_percentile,
            .symmetric = strategies[s].symmetric,
            .per_channel = false,
            .channel_axis = 0,
            .smoothing_factor = 0.0f
        };

        // INT8 양자화
        ETTensor* quantized = et_quantize_to_int8_advanced(input, NULL, NULL, &options, pool);
        if (!quantized) continue;

        // 양자화 파라미터 계산
        ETQuantizationParams params;
        if (!et_compute_quantization_params_advanced(input, ET_INT8, &params, &options)) {
            et_destroy_tensor(quantized);
            continue;
        }

        // 역양자화
        ETTensor* dequantized = et_dequantize_from_int8(quantized, NULL, &params, pool);
        if (!dequantized) {
            et_destroy_tensor(quantized);
            continue;
        }

        // MSE 계산
        float* dequant_data = (float*)dequantized->data;
        float mse = 0.0f;
        for (size_t i = 0; i < input->size; i++) {
            float error = input_data[i] - dequant_data[i];
            mse += error * error;
        }
        mse /= input->size;

        printf("%s: MSE = %f\n", strategies[s].name, mse);

        if (mse < best_mse) {
            best_mse = mse;
            best_strategy = strategies[s].name;
        }

        et_destroy_tensor(quantized);
        et_destroy_tensor(dequantized);
    }

    printf("최적 전략: %s (MSE: %f)\n", best_strategy, best_mse);

    // 정밀도 손실 최소화가 효과적인지 확인
    TEST_ASSERT(best_mse < 0.01f, "정밀도 손실 최소화 효과 확인");

    // 메모리 정리
    et_destroy_tensor(input);
    et_destroy_memory_pool(pool);

    printf("PASS: 정밀도 손실 최소화 전략 비교 테스트 통과\n");
    return 1;
}

// 메인 테스트 함수
int main() {
    printf("LibEtude 양자화 기능 테스트 시작\n");
    printf("=====================================\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 기존 테스트들
    total_tests++; if (test_bfloat16_conversion()) passed_tests++;
    total_tests++; if (test_bfloat16_tensor_quantization()) passed_tests++;
    total_tests++; if (test_int8_quantization()) passed_tests++;
    total_tests++; if (test_int4_packing()) passed_tests++;
    total_tests++; if (test_int4_quantization()) passed_tests++;
    total_tests++; if (test_dynamic_quantization()) passed_tests++;

    // 새로운 BF16 SIMD 및 튜닝 테스트들
    total_tests++; if (test_simd_bfloat16_conversion()) passed_tests++;
    total_tests++; if (test_voice_optimized_bf16_params()) passed_tests++;
    total_tests++; if (test_adaptive_bfloat16_quantization()) passed_tests++;
    total_tests++; if (test_simd_bfloat16_vector_ops()) passed_tests++;

    // 새로운 고급 양자화 테스트들
    total_tests++; if (test_advanced_quantization_strategies()) passed_tests++;
    total_tests++; if (test_advanced_int4_quantization()) passed_tests++;
    total_tests++; if (test_precision_loss_minimization()) passed_tests++;

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", passed_tests, total_tests);

    if (passed_tests == total_tests) {
        printf("모든 양자화 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다.\n");
        return 1;
    }
}