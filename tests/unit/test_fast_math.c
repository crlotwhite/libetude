/**
 * @file test_fast_math.c
 * @brief FastApprox 기반 고속 수학 함수 단위 테스트 (Unity)
 *
 * 고속 수학 함수의 정확성과 성능을 테스트합니다.
 * - 다양한 입력 범위에 대한 정확성 테스트
 * - 성능 벤치마크 테스트
 * - 음성 특화 수학 함수 테스트
 */

#include "unity.h"
#include "libetude/fast_math.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// 테스트 허용 오차
#define TOLERANCE_HIGH 0.01f    // 1% 오차
#define TOLERANCE_MED 0.05f     // 5% 오차
#define TOLERANCE_LOW 0.1f      // 10% 오차

// 성능 테스트 설정
#define PERFORMANCE_ITERATIONS 10000
#define BENCHMARK_WARMUP_ITERATIONS 100

// 테스트 데이터 크기들
static const size_t test_sizes[] = {100, 500, 1000, 5000, 10000};
static const size_t num_test_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

// 성능 측정 구조체
typedef struct {
    const char* function_name;
    size_t data_size;
    double fast_time_ms;
    double standard_time_ms;
    double speedup_ratio;
} MathPerformanceResult;

static MathPerformanceResult* math_performance_results = NULL;
static size_t math_performance_result_count = 0;
static size_t math_performance_result_capacity = 0;

// 상대 오차 계산 함수
static float relative_error(float expected, float actual) {
    if (fabsf(expected) < FLT_EPSILON) {
        return fabsf(actual);
    }
    return fabsf((actual - expected) / expected);
}

// 성능 측정 유틸리티 함수
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void record_math_performance_result(const char* function_name, size_t data_size,
                                         double fast_time_ms, double standard_time_ms) {
    if (math_performance_result_count >= math_performance_result_capacity) {
        return; // 용량 초과
    }

    MathPerformanceResult* result = &math_performance_results[math_performance_result_count++];
    result->function_name = function_name;
    result->data_size = data_size;
    result->fast_time_ms = fast_time_ms;
    result->standard_time_ms = standard_time_ms;
    result->speedup_ratio = standard_time_ms / fast_time_ms;
}

// 테스트 설정 함수
void setUp(void) {
    TEST_ASSERT_EQUAL(0, et_fast_math_init());

    // 성능 결과 배열 초기화
    if (!math_performance_results) {
        math_performance_result_capacity = 1000;
        math_performance_results = (MathPerformanceResult*)malloc(
            math_performance_result_capacity * sizeof(MathPerformanceResult));
        TEST_ASSERT_NOT_NULL(math_performance_results);
        math_performance_result_count = 0;
    }
}

// 테스트 정리 함수
void tearDown(void) {
    et_fast_math_cleanup();
}

// 초기화 테스트
void test_fast_math_initialization(void) {
    // 여러 번 초기화해도 안전해야 함
    TEST_ASSERT_EQUAL(0, et_fast_math_init());
    TEST_ASSERT_EQUAL(0, et_fast_math_init());
}

// 지수 함수 기본값 테스트
void test_exponential_basic_values(void) {
    float test_values[] = {0.0f, 1.0f, -1.0f, 2.0f, -2.0f, 0.5f, -0.5f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = expf(x);
        float actual = et_fast_exp(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "exp(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 지수 함수 극한값 테스트
void test_exponential_extreme_values(void) {
    TEST_ASSERT_EQUAL_FLOAT(INFINITY, et_fast_exp(100.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, et_fast_exp(-100.0f));
}

// 지수 함수 경계값 테스트
void test_exponential_edge_cases(void) {
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_HIGH, 1.0f, et_fast_exp(0.0f));

    float expected_e = expf(1.0f);
    float actual_e = et_fast_exp(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(expected_e * TOLERANCE_MED, expected_e, actual_e);
}

// 로그 함수 기본값 테스트
void test_logarithm_basic_values(void) {
    float test_values[] = {1.0f, 2.0f, 0.5f, 10.0f, 0.1f, 2.718f, 100.0f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = logf(x);
        float actual = et_fast_log(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "log(%.3f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 로그 함수 특수값 테스트
void test_logarithm_special_values(void) {
    TEST_ASSERT_EQUAL_FLOAT(-INFINITY, et_fast_log(0.0f));
    TEST_ASSERT_TRUE(isnan(et_fast_log(-1.0f)));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_HIGH, 0.0f, et_fast_log(1.0f));
}

// 사인 함수 테스트
void test_sine_function(void) {
    float test_angles[] = {0.0f, M_PI/6, M_PI/4, M_PI/3, M_PI/2, M_PI, 3*M_PI/2, 2*M_PI};
    size_t num_tests = sizeof(test_angles) / sizeof(test_angles[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_angles[i];
        float expected = sinf(x);
        float actual = et_fast_sin(x);

        // 0에 가까운 값들에 대해서는 절대 오차 사용
        float error;
        if (fabsf(expected) < 1e-6f) {
            error = fabsf(actual - expected);
        } else {
            error = relative_error(expected, actual);
        }

        // 특수한 경우 (2π 근처)에서는 더 관대한 허용 오차 적용
        float tolerance = (fabsf((float)x - 2.0f*(float)M_PI) < 0.001f) ? TOLERANCE_MED : TOLERANCE_HIGH;

        char msg[256];
        snprintf(msg, sizeof(msg),
                "sin(%.4f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < tolerance, msg);
    }
}

// 코사인 함수 테스트
void test_cosine_function(void) {
    float test_angles[] = {0.0f, M_PI/6, M_PI/4, M_PI/3, M_PI/2, M_PI, 3*M_PI/2, 2*M_PI};
    size_t num_tests = sizeof(test_angles) / sizeof(test_angles[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_angles[i];
        float expected = cosf(x);
        float actual = et_fast_cos(x);
        float error = relative_error(expected, actual);

        float tolerance = (fabsf((float)x - 2.0f*(float)M_PI) < 0.001f) ? TOLERANCE_MED : TOLERANCE_HIGH;

        char msg[256];
        snprintf(msg, sizeof(msg),
                "cos(%.4f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < tolerance, msg);
    }
}

// 삼각함수 항등식 테스트
void test_trigonometric_identities(void) {
    float x = M_PI / 4;
    float sin_val = et_fast_sin(x);
    float cos_val = et_fast_cos(x);

    // sin²(x) + cos²(x) = 1
    float identity_result = sin_val * sin_val + cos_val * cos_val;
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE_HIGH, 1.0f, identity_result);
}

// 하이퍼볼릭 탄젠트 테스트
void test_hyperbolic_tangent(void) {
    float test_values[] = {-3.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 3.0f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = tanhf(x);
        float actual = et_fast_tanh(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "tanh(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 시그모이드 함수 테스트
void test_sigmoid_function(void) {
    float test_values[] = {-3.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 3.0f};
    size_t num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        float x = test_values[i];
        float expected = 1.0f / (1.0f + expf(-x));
        float actual = et_fast_sigmoid(x);
        float error = relative_error(expected, actual);

        char msg[256];
        snprintf(msg, sizeof(msg),
                "sigmoid(%.2f): expected=%.6f, actual=%.6f, error=%.4f%%",
                x, expected, actual, error * 100.0f);

        TEST_ASSERT_TRUE_MESSAGE(error < TOLERANCE_MED, msg);
    }
}

// 활성화 함수 속성 테스트
void test_activation_function_properties(void) {
    // Sigmoid는 항상 (0, 1) 범위
    float sig_neg = et_fast_sigmoid(-10.0f);
    float sig_pos = et_fast_sigmoid(10.0f);
    TEST_ASSERT_TRUE(sig_neg >= 0.0f && sig_neg <= 1.0f);
    TEST_ASSERT_TRUE(sig_pos >= 0.0f && sig_pos <= 1.0f);

    // Tanh는 항상 [-1, 1] 범위
    float tanh_neg = et_fast_tanh(-10.0f);
    float tanh_pos = et_fast_tanh(10.0f);
    TEST_ASSERT_TRUE(tanh_neg >= -1.0f && tanh_neg <= 1.0f);
    TEST_ASSERT_TRUE(tanh_pos >= -1.0f && tanh_pos <= 1.0f);
}

// 벡터화된 지수 함수 테스트
void test_vectorized_exponential(void) {
    const size_t size = 10;
    float input[10] = {-2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f};
    float output[10];

    et_fast_exp_vec(input, output, size);

    for (size_t i = 0; i < size; i++) {
        float expected = et_fast_exp(input[i]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, output[i]);
    }
}

// 벡터화된 로그 함수 테스트
void test_vectorized_logarithm(void) {
    float positive_input[5] = {0.1f, 0.5f, 1.0f, 2.0f, 10.0f};
    float output[5];

    et_fast_log_vec(positive_input, output, 5);

    for (size_t i = 0; i < 5; i++) {
        float expected = et_fast_log(positive_input[i]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, output[i]);
    }
}

// 벡터화된 하이퍼볼릭 탄젠트 테스트
void test_vectorized_hyperbolic_tangent(void) {
    const size_t size = 10;
    float input[10] = {-2.0f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f};
    float output[10];

    et_fast_tanh_vec(input, output, size);

    for (size_t i = 0; i < size; i++) {
        float expected = et_fast_tanh(input[i]);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, output[i]);
    }
}

// 성능 특성 테스트
void test_performance_characteristics(void) {
    const int iterations = 1000;
    float test_value = 1.5f;

    // 단순히 함수가 정상적으로 호출되는지 확인
    for (int i = 0; i < iterations; i++) {
        volatile float result = et_fast_exp(test_value);
        (void)result; // 컴파일러 최적화 방지
    }

    // 크래시 없이 완료되면 성공
    TEST_PASS();
}

// 정리 안전성 테스트
void test_cleanup_safety(void) {
    // 여러 번 호출해도 안전해야 함
    et_fast_math_cleanup();
    et_fast_math_cleanup();
    et_fast_math_cleanup();

    // 크래시 없이 완료되면 성공
    TEST_PASS();
}

// =============================================================================
// 성능 비교 테스트들
// =============================================================================

void test_exponential_performance(void) {
    printf("\n=== Exponential Function Performance Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Benchmarking exp() with %zu elements... ", size);

        float* input = (float*)malloc(size * sizeof(float));
        float* fast_output = (float*)malloc(size * sizeof(float));
        float* std_output = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input);
        TEST_ASSERT_NOT_NULL(fast_output);
        TEST_ASSERT_NOT_NULL(std_output);

        // 테스트 데이터 생성 (-5 ~ 5 범위)
        for (size_t j = 0; j < size; j++) {
            input[j] = -5.0f + 10.0f * ((float)j / size);
        }

        // 워밍업
        for (int w = 0; w < BENCHMARK_WARMUP_ITERATIONS; w++) {
            et_fast_exp_vec(input, fast_output, size);
            for (size_t j = 0; j < size; j++) {
                std_output[j] = expf(input[j]);
            }
        }

        // Fast 버전 측정
        double fast_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            et_fast_exp_vec(input, fast_output, size);
        }
        double fast_end = get_time_ms();
        double fast_time = (fast_end - fast_start) / PERFORMANCE_ITERATIONS;

        // Standard 버전 측정
        double std_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            for (size_t j = 0; j < size; j++) {
                std_output[j] = expf(input[j]);
            }
        }
        double std_end = get_time_ms();
        double std_time = (std_end - std_start) / PERFORMANCE_ITERATIONS;

        record_math_performance_result("exp", size, fast_time, std_time);

        printf("Fast: %.3fms, Std: %.3fms, Speedup: %.2fx\n",
               fast_time, std_time, std_time / fast_time);

        free(input);
        free(fast_output);
        free(std_output);
    }
}

void test_logarithm_performance(void) {
    printf("\n=== Logarithm Function Performance Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Benchmarking log() with %zu elements... ", size);

        float* input = (float*)malloc(size * sizeof(float));
        float* fast_output = (float*)malloc(size * sizeof(float));
        float* std_output = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input);
        TEST_ASSERT_NOT_NULL(fast_output);
        TEST_ASSERT_NOT_NULL(std_output);

        // 테스트 데이터 생성 (0.1 ~ 10 범위)
        for (size_t j = 0; j < size; j++) {
            input[j] = 0.1f + 9.9f * ((float)j / size);
        }

        // 워밍업
        for (int w = 0; w < BENCHMARK_WARMUP_ITERATIONS; w++) {
            et_fast_log_vec(input, fast_output, size);
            for (size_t j = 0; j < size; j++) {
                std_output[j] = logf(input[j]);
            }
        }

        // Fast 버전 측정
        double fast_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            et_fast_log_vec(input, fast_output, size);
        }
        double fast_end = get_time_ms();
        double fast_time = (fast_end - fast_start) / PERFORMANCE_ITERATIONS;

        // Standard 버전 측정
        double std_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            for (size_t j = 0; j < size; j++) {
                std_output[j] = logf(input[j]);
            }
        }
        double std_end = get_time_ms();
        double std_time = (std_end - std_start) / PERFORMANCE_ITERATIONS;

        record_math_performance_result("log", size, fast_time, std_time);

        printf("Fast: %.3fms, Std: %.3fms, Speedup: %.2fx\n",
               fast_time, std_time, std_time / fast_time);

        free(input);
        free(fast_output);
        free(std_output);
    }
}

void test_trigonometric_performance(void) {
    printf("\n=== Trigonometric Functions Performance Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Benchmarking sin() with %zu elements... ", size);

        float* input = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input);

        // 테스트 데이터 생성 (0 ~ 2π 범위)
        for (size_t j = 0; j < size; j++) {
            input[j] = 2.0f * M_PI * ((float)j / size);
        }

        // 워밍업
        for (int w = 0; w < BENCHMARK_WARMUP_ITERATIONS; w++) {
            for (size_t j = 0; j < size; j++) {
                volatile float fast_result = et_fast_sin(input[j]);
                volatile float std_result = sinf(input[j]);
                (void)fast_result;
                (void)std_result;
            }
        }

        // Fast 버전 측정
        double fast_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            for (size_t j = 0; j < size; j++) {
                volatile float result = et_fast_sin(input[j]);
                (void)result;
            }
        }
        double fast_end = get_time_ms();
        double fast_time = (fast_end - fast_start) / PERFORMANCE_ITERATIONS;

        // Standard 버전 측정
        double std_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            for (size_t j = 0; j < size; j++) {
                volatile float result = sinf(input[j]);
                (void)result;
            }
        }
        double std_end = get_time_ms();
        double std_time = (std_end - std_start) / PERFORMANCE_ITERATIONS;

        record_math_performance_result("sin", size, fast_time, std_time);

        printf("Fast: %.3fms, Std: %.3fms, Speedup: %.2fx\n",
               fast_time, std_time, std_time / fast_time);

        free(input);
    }
}

void test_activation_functions_performance(void) {
    printf("\n=== Activation Functions Performance Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Benchmarking tanh() with %zu elements... ", size);

        float* input = (float*)malloc(size * sizeof(float));
        float* fast_output = (float*)malloc(size * sizeof(float));
        float* std_output = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input);
        TEST_ASSERT_NOT_NULL(fast_output);
        TEST_ASSERT_NOT_NULL(std_output);

        // 테스트 데이터 생성 (-3 ~ 3 범위)
        for (size_t j = 0; j < size; j++) {
            input[j] = -3.0f + 6.0f * ((float)j / size);
        }

        // 워밍업
        for (int w = 0; w < BENCHMARK_WARMUP_ITERATIONS; w++) {
            et_fast_tanh_vec(input, fast_output, size);
            for (size_t j = 0; j < size; j++) {
                std_output[j] = tanhf(input[j]);
            }
        }

        // Fast 버전 측정
        double fast_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            et_fast_tanh_vec(input, fast_output, size);
        }
        double fast_end = get_time_ms();
        double fast_time = (fast_end - fast_start) / PERFORMANCE_ITERATIONS;

        // Standard 버전 측정
        double std_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            for (size_t j = 0; j < size; j++) {
                std_output[j] = tanhf(input[j]);
            }
        }
        double std_end = get_time_ms();
        double std_time = (std_end - std_start) / PERFORMANCE_ITERATIONS;

        record_math_performance_result("tanh", size, fast_time, std_time);

        printf("Fast: %.3fms, Std: %.3fms, Speedup: %.2fx\n",
               fast_time, std_time, std_time / fast_time);

        free(input);
        free(fast_output);
        free(std_output);
    }
}

// 성능 결과 출력 함수
void print_math_performance_results(void) {
    if (math_performance_result_count == 0) {
        return;
    }

    printf("\n=== Math Performance Results Summary ===\n");
    printf("%-15s %-10s %-15s %-15s %-10s\n", "Function", "Size", "Fast (ms)", "Std (ms)", "Speedup");
    printf("%-15s %-10s %-15s %-15s %-10s\n", "--------", "----", "---------", "--------", "-------");

    for (size_t i = 0; i < math_performance_result_count; i++) {
        MathPerformanceResult* result = &math_performance_results[i];
        printf("%-15s %-10zu %-15.6f %-15.6f %-10.2fx\n",
               result->function_name,
               result->data_size,
               result->fast_time_ms,
               result->standard_time_ms,
               result->speedup_ratio);
    }
    printf("\n");
}

// =============================================================================
// 음성 특화 수학 함수 테스트들
// =============================================================================

// Hz to Mel 변환 테스트
void test_hz_to_mel_conversion(void) {
    // 알려진 값들 테스트
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, et_hz_to_mel(0.0f));
    // 1000Hz는 대략 1000 Mel 정도 (정확한 값은 약 999.9)
    float mel_1000 = et_hz_to_mel(1000.0f);
    TEST_ASSERT_TRUE(mel_1000 > 950.0f && mel_1000 < 1050.0f);

    // 음수 입력 처리
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, et_hz_to_mel(-100.0f));

    // 단조 증가 특성 확인
    float hz1 = 100.0f, hz2 = 200.0f;
    TEST_ASSERT_TRUE(et_hz_to_mel(hz2) > et_hz_to_mel(hz1));
}

// Mel to Hz 변환 테스트
void test_mel_to_hz_conversion(void) {
    // 알려진 값들 테스트
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, et_mel_to_hz(0.0f));
    // 1000 Mel은 대략 1000Hz 정도
    float hz_1000 = et_mel_to_hz(1000.0f);
    TEST_ASSERT_TRUE(hz_1000 > 900.0f && hz_1000 < 1300.0f);

    // 음수 입력 처리
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, et_mel_to_hz(-100.0f));

    // 역변환 테스트
    float hz_original = 440.0f; // A4 음표
    float mel = et_hz_to_mel(hz_original);
    float hz_converted = et_mel_to_hz(mel);
    TEST_ASSERT_FLOAT_WITHIN(20.0f, hz_original, hz_converted); // 더 관대한 허용 오차
}

// Mel 필터뱅크 생성 테스트
void test_mel_filterbank_creation(void) {
    int n_fft = 512;
    int n_mels = 40;
    float sample_rate = 16000.0f;
    float fmin = 0.0f;
    float fmax = 8000.0f;

    int n_freqs = n_fft / 2 + 1;
    float* mel_filters = (float*)calloc(n_mels * n_freqs, sizeof(float));
    TEST_ASSERT_NOT_NULL(mel_filters);

    int result = et_create_mel_filterbank(n_fft, n_mels, sample_rate, fmin, fmax, mel_filters);
    TEST_ASSERT_EQUAL(0, result);

    // 필터가 생성되었는지 확인 (모든 값이 0이 아님)
    bool has_nonzero = false;
    for (int i = 0; i < n_mels * n_freqs; i++) {
        if (mel_filters[i] > 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(has_nonzero);

    // 각 필터의 최대값이 1.0 이하인지 확인
    for (int m = 0; m < n_mels; m++) {
        float max_val = 0.0f;
        for (int f = 0; f < n_freqs; f++) {
            if (mel_filters[m * n_freqs + f] > max_val) {
                max_val = mel_filters[m * n_freqs + f];
            }
        }
        TEST_ASSERT_TRUE(max_val <= 1.0f);
    }

    free(mel_filters);
}

// 스펙트로그램을 Mel 스펙트로그램으로 변환 테스트
void test_spectrogram_to_mel(void) {
    int n_frames = 10;
    int n_freqs = 257; // 512 FFT의 경우
    int n_mels = 40;

    // 테스트 데이터 생성
    float* spectrogram = (float*)malloc(n_frames * n_freqs * sizeof(float));
    float* mel_filters = (float*)calloc(n_mels * n_freqs, sizeof(float));
    float* mel_spectrogram = (float*)malloc(n_frames * n_mels * sizeof(float));

    TEST_ASSERT_NOT_NULL(spectrogram);
    TEST_ASSERT_NOT_NULL(mel_filters);
    TEST_ASSERT_NOT_NULL(mel_spectrogram);

    // 간단한 테스트 데이터로 채우기
    for (int i = 0; i < n_frames * n_freqs; i++) {
        spectrogram[i] = 1.0f; // 균등한 스펙트럼
    }

    // 간단한 필터 (첫 번째 필터만 활성화)
    for (int f = 0; f < n_freqs / 2; f++) {
        mel_filters[f] = 1.0f;
    }

    et_spectrogram_to_mel(spectrogram, mel_filters, mel_spectrogram, n_frames, n_freqs, n_mels);

    // 첫 번째 Mel 빈이 0이 아닌지 확인
    TEST_ASSERT_TRUE(mel_spectrogram[0] > 0.0f);

    free(spectrogram);
    free(mel_filters);
    free(mel_spectrogram);
}

// 피치 시프팅 테스트
void test_pitch_shifting(void) {
    float freq = 440.0f; // A4

    // 1옥타브 위로 (2배)
    float shifted_up = et_pitch_shift_frequency(freq, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 880.0f, shifted_up);

    // 1옥타브 아래로 (0.5배)
    float shifted_down = et_pitch_shift_frequency(freq, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 220.0f, shifted_down);

    // 변화 없음 (1.0배)
    float unchanged = et_pitch_shift_frequency(freq, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, freq, unchanged);
}

// 세미톤 변환 테스트
void test_semitone_conversion(void) {
    // 12 세미톤 = 1 옥타브 = 2배 주파수
    float ratio_octave = et_semitones_to_ratio(12.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.0f, ratio_octave); // 더 관대한 허용 오차

    // 0 세미톤 = 변화 없음
    float ratio_zero = et_semitones_to_ratio(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ratio_zero);

    // 역변환 테스트
    float semitones_original = 7.0f; // 완전 5도
    float ratio = et_semitones_to_ratio(semitones_original);
    float semitones_converted = et_ratio_to_semitones(ratio);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, semitones_original, semitones_converted); // 더 관대한 허용 오차
}

// 윈도우 함수 테스트
void test_window_functions(void) {
    int size = 64;
    float* window = (float*)malloc(size * sizeof(float));
    TEST_ASSERT_NOT_NULL(window);

    // 해밍 윈도우 테스트
    et_hamming_window(window, size);
    bool hamming_valid = true;
    for (int i = 0; i < size; i++) {
        if (isnan(window[i]) || isinf(window[i]) || window[i] < 0.0f || window[i] > 1.0f) {
            hamming_valid = false;
            break;
        }
    }
    TEST_ASSERT_TRUE(hamming_valid);

    // 한 윈도우 테스트
    et_hann_window(window, size);
    bool hann_valid = true;
    for (int i = 0; i < size; i++) {
        if (isnan(window[i]) || isinf(window[i]) || window[i] < 0.0f || window[i] > 1.0f) {
            hann_valid = false;
            break;
        }
    }
    TEST_ASSERT_TRUE(hann_valid);

    // 블랙만 윈도우 테스트
    et_blackman_window(window, size);
    bool blackman_valid = true;
    for (int i = 0; i < size; i++) {
        if (isnan(window[i]) || isinf(window[i])) {
            blackman_valid = false;
            break;
        }
    }
    TEST_ASSERT_TRUE(blackman_valid); // 블랙만 윈도우는 음수값이 나올 수 있으므로 NaN/Inf만 체크

    free(window);
}

// 오디오 유틸리티 함수 테스트
void test_audio_utilities(void) {
    int size = 100;
    float* signal = (float*)malloc(size * sizeof(float));
    TEST_ASSERT_NOT_NULL(signal);

    // 테스트 신호 생성 (사인파)
    for (int i = 0; i < size; i++) {
        signal[i] = sinf(2.0f * M_PI * i / size);
    }

    // RMS 계산 테스트
    float rms = et_audio_rms(signal, size);
    TEST_ASSERT_TRUE(rms > 0.0f && rms < 1.0f);

    // 피크 찾기 테스트
    float peak = et_find_peak(signal, size);
    TEST_ASSERT_TRUE(peak > 0.7f && peak <= 1.0f); // 사인파의 피크는 1에 가까움

    // 정규화 테스트
    et_normalize_audio(signal, size, 0.5f);
    float new_peak = et_find_peak(signal, size);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, new_peak);

    free(signal);
}

// 보간 함수 테스트
void test_interpolation_functions(void) {
    float a = 1.0f, b = 3.0f;

    // 선형 보간 테스트
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, et_lerp(a, b, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, et_lerp(a, b, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, et_lerp(a, b, 0.5f));

    // 코사인 보간 테스트
    float cos_interp = et_cosine_interp(a, b, 0.5f);
    TEST_ASSERT_TRUE(cos_interp > 1.0f && cos_interp < 3.0f);

    // 3차 스플라인 보간 테스트
    float p0 = 0.0f, p1 = 1.0f, p2 = 3.0f, p3 = 4.0f;
    float cubic_result = et_cubic_interp(p0, p1, p2, p3, 0.5f);
    TEST_ASSERT_TRUE(cubic_result > 1.0f && cubic_result < 3.0f);
}

// dB 변환 테스트
void test_db_conversion(void) {
    // 0 dB = 1.0 선형
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, et_db_to_linear(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, et_linear_to_db(1.0f));

    // 20 dB = 10배
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, et_db_to_linear(20.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, et_linear_to_db(10.0f));

    // -20 dB = 0.1배
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.1f, et_db_to_linear(-20.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -20.0f, et_linear_to_db(0.1f));

    // 0 또는 음수 선형값 처리
    TEST_ASSERT_EQUAL_FLOAT(-INFINITY, et_linear_to_db(0.0f));
    TEST_ASSERT_TRUE(et_linear_to_db(-1.0f) == -INFINITY);
}

// Unity 테스트 러너
int main(void) {
    printf("LibEtude Fast Math Test Suite\n");
    printf("=============================\n");

    UNITY_BEGIN();

    // 초기화 테스트
    printf("\n>>> INITIALIZATION TESTS <<<\n");
    RUN_TEST(test_fast_math_initialization);

    // 기본 함수 정확성 테스트
    printf("\n>>> ACCURACY TESTS <<<\n");
    RUN_TEST(test_exponential_basic_values);
    RUN_TEST(test_exponential_extreme_values);
    RUN_TEST(test_exponential_edge_cases);
    RUN_TEST(test_logarithm_basic_values);
    RUN_TEST(test_logarithm_special_values);
    RUN_TEST(test_sine_function);
    RUN_TEST(test_cosine_function);
    RUN_TEST(test_trigonometric_identities);
    RUN_TEST(test_hyperbolic_tangent);
    RUN_TEST(test_sigmoid_function);
    RUN_TEST(test_activation_function_properties);

    // 벡터화된 함수 테스트
    printf("\n>>> VECTORIZED FUNCTION TESTS <<<\n");
    RUN_TEST(test_vectorized_exponential);
    RUN_TEST(test_vectorized_logarithm);
    RUN_TEST(test_vectorized_hyperbolic_tangent);

    // 성능 비교 테스트
    printf("\n>>> PERFORMANCE COMPARISON TESTS <<<\n");
    RUN_TEST(test_exponential_performance);
    RUN_TEST(test_logarithm_performance);
    RUN_TEST(test_trigonometric_performance);
    RUN_TEST(test_activation_functions_performance);

    // 음성 특화 수학 함수 테스트
    printf("\n>>> VOICE-SPECIFIC MATH TESTS <<<\n");
    RUN_TEST(test_hz_to_mel_conversion);
    RUN_TEST(test_mel_to_hz_conversion);
    RUN_TEST(test_mel_filterbank_creation);
    RUN_TEST(test_spectrogram_to_mel);
    RUN_TEST(test_pitch_shifting);
    RUN_TEST(test_semitone_conversion);
    RUN_TEST(test_window_functions);
    RUN_TEST(test_audio_utilities);
    RUN_TEST(test_interpolation_functions);
    RUN_TEST(test_db_conversion);

    // 안전성 테스트
    printf("\n>>> SAFETY TESTS <<<\n");
    RUN_TEST(test_performance_characteristics);
    RUN_TEST(test_cleanup_safety);

    // 성능 결과 출력
    print_math_performance_results();

    // 메모리 정리
    if (math_performance_results) {
        free(math_performance_results);
        math_performance_results = NULL;
    }

    printf("\n>>> TEST SUMMARY <<<\n");
    return UNITY_END();
}