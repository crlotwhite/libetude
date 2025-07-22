/**
 * @file test_neon_simple.c
 * @brief NEON 커널 간단 테스트
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define LIBETUDE_HAVE_NEON 1

// 간단한 NEON 테스트를 위한 함수들
#ifdef LIBETUDE_HAVE_NEON
#include <arm_neon.h>

void neon_vector_add(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;
    for (; i + 3 < size; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        float32x4_t vresult = vaddq_f32(va, vb);
        vst1q_f32(result + i, vresult);
    }
    for (; i < size; i++) {
        result[i] = a[i] + b[i];
    }
}

void neon_vector_mul(const float* a, const float* b, float* result, size_t size) {
    size_t i = 0;
    for (; i + 3 < size; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        float32x4_t vresult = vmulq_f32(va, vb);
        vst1q_f32(result + i, vresult);
    }
    for (; i < size; i++) {
        result[i] = a[i] * b[i];
    }
}

float neon_vector_dot(const float* a, const float* b, size_t size) {
    size_t i = 0;
    float32x4_t vsum = vdupq_n_f32(0.0f);

    for (; i + 3 < size; i += 4) {
        float32x4_t va = vld1q_f32(a + i);
        float32x4_t vb = vld1q_f32(b + i);
        vsum = vmlaq_f32(vsum, va, vb);
    }

    float32x2_t vsum_low = vget_low_f32(vsum);
    float32x2_t vsum_high = vget_high_f32(vsum);
    float32x2_t vsum_pair = vadd_f32(vsum_low, vsum_high);
    float sum = vget_lane_f32(vsum_pair, 0) + vget_lane_f32(vsum_pair, 1);

    for (; i < size; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

void neon_relu(const float* input, float* output, size_t size) {
    size_t i = 0;
    float32x4_t vzero = vdupq_n_f32(0.0f);

    for (; i + 3 < size; i += 4) {
        float32x4_t vinput = vld1q_f32(input + i);
        float32x4_t vresult = vmaxq_f32(vinput, vzero);
        vst1q_f32(output + i, vresult);
    }

    for (; i < size; i++) {
        output[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }
}

#else
// CPU fallback implementations
void neon_vector_add(const float* a, const float* b, float* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        result[i] = a[i] + b[i];
    }
}

void neon_vector_mul(const float* a, const float* b, float* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        result[i] = a[i] * b[i];
    }
}

float neon_vector_dot(const float* a, const float* b, size_t size) {
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

void neon_relu(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }
}
#endif

#define TEST_SIZE 100
#define EPSILON 1e-5f

static int test_passed = 0;
static int test_failed = 0;

void test_vector_add() {
    printf("Testing NEON vector addition...\n");

    float a[TEST_SIZE], b[TEST_SIZE], result[TEST_SIZE], expected[TEST_SIZE];

    // 테스트 데이터 생성
    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)i * 0.1f;
        b[i] = (float)i * 0.2f;
        expected[i] = a[i] + b[i];
    }

    // NEON 함수 실행
    neon_vector_add(a, b, result, TEST_SIZE);

    // 결과 검증
    bool passed = true;
    for (int i = 0; i < TEST_SIZE; i++) {
        if (fabsf(result[i] - expected[i]) > EPSILON) {
            printf("  FAIL at index %d: got %f, expected %f\n", i, result[i], expected[i]);
            passed = false;
            break;
        }
    }

    if (passed) {
        printf("  PASS\n");
        test_passed++;
    } else {
        printf("  FAIL\n");
        test_failed++;
    }
}

void test_vector_mul() {
    printf("Testing NEON vector multiplication...\n");

    float a[TEST_SIZE], b[TEST_SIZE], result[TEST_SIZE], expected[TEST_SIZE];

    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)i * 0.1f;
        b[i] = (float)i * 0.2f;
        expected[i] = a[i] * b[i];
    }

    neon_vector_mul(a, b, result, TEST_SIZE);

    bool passed = true;
    for (int i = 0; i < TEST_SIZE; i++) {
        if (fabsf(result[i] - expected[i]) > EPSILON) {
            printf("  FAIL at index %d: got %f, expected %f\n", i, result[i], expected[i]);
            passed = false;
            break;
        }
    }

    if (passed) {
        printf("  PASS\n");
        test_passed++;
    } else {
        printf("  FAIL\n");
        test_failed++;
    }
}

void test_vector_dot() {
    printf("Testing NEON vector dot product...\n");

    float a[TEST_SIZE], b[TEST_SIZE];
    float expected = 0.0f;

    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)i * 0.1f;
        b[i] = (float)i * 0.2f;
        expected += a[i] * b[i];
    }

    float result = neon_vector_dot(a, b, TEST_SIZE);

    if (fabsf(result - expected) < EPSILON) {
        printf("  PASS (got %f, expected %f)\n", result, expected);
        test_passed++;
    } else {
        printf("  FAIL (got %f, expected %f, diff %f)\n", result, expected, fabsf(result - expected));
        test_failed++;
    }
}

void test_relu() {
    printf("Testing NEON ReLU...\n");

    float input[TEST_SIZE], result[TEST_SIZE], expected[TEST_SIZE];

    for (int i = 0; i < TEST_SIZE; i++) {
        input[i] = (float)i - 50.0f; // -50 to 49
        expected[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }

    neon_relu(input, result, TEST_SIZE);

    bool passed = true;
    for (int i = 0; i < TEST_SIZE; i++) {
        if (fabsf(result[i] - expected[i]) > EPSILON) {
            printf("  FAIL at index %d: got %f, expected %f\n", i, result[i], expected[i]);
            passed = false;
            break;
        }
    }

    if (passed) {
        printf("  PASS\n");
        test_passed++;
    } else {
        printf("  FAIL\n");
        test_failed++;
    }
}

int main() {
    printf("=== NEON Kernel Simple Tests ===\n");

    test_vector_add();
    test_vector_mul();
    test_vector_dot();
    test_relu();

    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    printf("Total:  %d\n", test_passed + test_failed);

    return test_failed > 0 ? 1 : 0;
}