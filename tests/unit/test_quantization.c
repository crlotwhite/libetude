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
#include "libetude/tensor.h"
#include "libetude/memory.h"

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

// 메인 테스트 함수
int main() {
    printf("LibEtude 양자화 기능 테스트 시작\n");
    printf("=====================================\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 각 테스트 실행
    total_tests++; if (test_bfloat16_conversion()) passed_tests++;
    total_tests++; if (test_bfloat16_tensor_quantization()) passed_tests++;
    total_tests++; if (test_int8_quantization()) passed_tests++;
    total_tests++; if (test_int4_packing()) passed_tests++;
    total_tests++; if (test_int4_quantization()) passed_tests++;
    total_tests++; if (test_dynamic_quantization()) passed_tests++;

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