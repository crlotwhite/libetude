/**
 * @file test_simd_math_functions.c
 * @brief SIMD 최적화된 수학 함수 성능 테스트
 *
 * SIMD 커널과 수학 함수의 통합 성능을 테스트하고,
 * 다양한 입력 크기에 대한 성능 특성을 검증합니다.
 */

#include "unity.h"
#include "libetude/simd_kernels.h"
#include "libetude/fast_math.h"
#include "libetude/kernel_registry.h"
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

// 테스트 설정
#define TEST_EPSILON 1e-5f
#define PERFORMANCE_ITERATIONS 1000
#define WARMUP_ITERATIONS 10

// 테스트 데이터 크기들
static const size_t test_sizes[] = {64, 256, 1024, 4096, 16384};
static const size_t num_test_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

// 성능 결과 구조체
typedef struct {
    const char* test_name;
    size_t data_size;
    double simd_time_ms;
    double scalar_time_ms;
    double speedup_ratio;
    double accuracy_error;
} SIMDMathPerformanceResult;

static SIMDMathPerformanceResult* performance_results = NULL;
static size_t performance_result_count = 0;
static size_t performance_result_capacity = 0;

// 유틸리티 함수들
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void record_performance_result(const char* test_name, size_t data_size,
                                    double simd_time, double scalar_time,
                                    double accuracy_error) {
    if (performance_result_count >= performance_result_capacity) {
        return; // 용량 초과
    }

    SIMDMathPerformanceResult* result = &performance_results[performance_result_count++];
    result->test_name = test_name;
    result->data_size = data_size;
    result->simd_time_ms = simd_time;
    result->scalar_time_ms = scalar_time;
    result->speedup_ratio = scalar_time / simd_time;
    result->accuracy_error = accuracy_error;
}

static void generate_test_data(float* data, size_t size, float min_val, float max_val) {
    for (size_t i = 0; i < size; i++) {
        data[i] = min_val + ((float)rand() / RAND_MAX) * (max_val - min_val);
    }
}

static double calculate_max_relative_error(const float* expected, const float* actual, size_t size) {
    double max_error = 0.0;
    for (size_t i = 0; i < size; i++) {
        if (fabsf(expected[i]) > 1e-6f) {
            double error = fabs((actual[i] - expected[i]) / expected[i]);
            if (error > max_error) {
                max_error = error;
            }
        }
    }
    return max_error;
}

// 테스트 설정 및 정리
void setUp(void) {
    // SIMD 커널 시스템 초기화
    LibEtudeErrorCode result = et_init_simd_kernels();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);

    // Fast Math 초기화
    TEST_ASSERT_EQUAL(0, et_fast_math_init());

    srand((unsigned int)time(NULL));

    // 성능 결과 배열 초기화
    if (!performance_results) {
        performance_result_capacity = 100;
        performance_results = (SIMDMathPerformanceResult*)malloc(
            performance_result_capacity * sizeof(SIMDMathPerformanceResult));
        TEST_ASSERT_NOT_NULL(performance_results);
        performance_result_count = 0;
    }
}

void tearDown(void) {
    et_cleanup_simd_kernels();
    et_fast_math_cleanup();
}

// SIMD 벡터 연산과 수학 함수 통합 테스트
void test_simd_vector_math_integration(void) {
    printf("\n=== SIMD Vector Math Integration Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Testing SIMD vector math integration with size %zu... ", size);

        float* input_a = (float*)malloc(size * sizeof(float));
        float* input_b = (float*)malloc(size * sizeof(float));
        float* simd_result = (float*)malloc(size * sizeof(float));
        float* scalar_result = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input_a);
        TEST_ASSERT_NOT_NULL(input_b);
        TEST_ASSERT_NOT_NULL(simd_result);
        TEST_ASSERT_NOT_NULL(scalar_result);

        // 테스트 데이터 생성
        generate_test_data(input_a, size, -5.0f, 5.0f);
        generate_test_data(input_b, size, -5.0f, 5.0f);

        // SIMD 벡터 덧셈 후 지수 함수 적용
        et_simd_vector_add(input_a, input_b, simd_result, size);
        et_fast_exp_vec(simd_result, simd_result, size);

        // 스칼라 버전으로 참조 결과 계산
        for (size_t j = 0; j < size; j++) {
            scalar_result[j] = et_fast_exp(input_a[j] + input_b[j]);
        }

        // 정확성 검증
        double max_error = calculate_max_relative_error(scalar_result, simd_result, size);
        TEST_ASSERT_TRUE(max_error < 0.01); // 1% 이하 오차

        printf("PASS (max error: %.4f%%)\n", max_error * 100.0);

        free(input_a);
        free(input_b);
        free(simd_result);
        free(scalar_result);
    }
}

// SIMD 활성화 함수 성능 테스트
void test_simd_activation_performance(void) {
    printf("\n=== SIMD Activation Function Performance Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Benchmarking SIMD activation functions (size %zu)...\n", size);

        float* input = (float*)malloc(size * sizeof(float));
        float* simd_output = (float*)malloc(size * sizeof(float));
        float* scalar_output = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input);
        TEST_ASSERT_NOT_NULL(simd_output);
        TEST_ASSERT_NOT_NULL(scalar_output);

        generate_test_data(input, size, -3.0f, 3.0f);

        // ReLU 성능 테스트
        {
            // 워밍업
            for (int w = 0; w < WARMUP_ITERATIONS; w++) {
                et_simd_relu(input, simd_output, size);
            }

            // SIMD 버전 측정
            double simd_start = get_time_ms();
            for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
                et_simd_relu(input, simd_output, size);
            }
            double simd_end = get_time_ms();
            double simd_time = (simd_end - simd_start) / PERFORMANCE_ITERATIONS;

            // 스칼라 버전 측정
            double scalar_start = get_time_ms();
            for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
                for (size_t j = 0; j < size; j++) {
                    scalar_output[j] = input[j] > 0.0f ? input[j] : 0.0f;
                }
            }
            double scalar_end = get_time_ms();
            double scalar_time = (scalar_end - scalar_start) / PERFORMANCE_ITERATIONS;

            // 정확성 검증
            et_simd_relu(input, simd_output, size);
            for (size_t j = 0; j < size; j++) {
                scalar_output[j] = input[j] > 0.0f ? input[j] : 0.0f;
            }
            double accuracy_error = calculate_max_relative_error(scalar_output, simd_output, size);

            record_performance_result("ReLU", size, simd_time, scalar_time, accuracy_error);

            printf("  ReLU: SIMD=%.3fms, Scalar=%.3fms, Speedup=%.2fx\n",
                   simd_time, scalar_time, scalar_time / simd_time);
        }

        // Sigmoid 성능 테스트
        {
            // 워밍업
            for (int w = 0; w < WARMUP_ITERATIONS; w++) {
                et_simd_sigmoid(input, simd_output, size);
            }

            // SIMD 버전 측정
            double simd_start = get_time_ms();
            for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
                et_simd_sigmoid(input, simd_output, size);
            }
            double simd_end = get_time_ms();
            double simd_time = (simd_end - simd_start) / PERFORMANCE_ITERATIONS;

            // 스칼라 버전 측정
            double scalar_start = get_time_ms();
            for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
                for (size_t j = 0; j < size; j++) {
                    scalar_output[j] = et_fast_sigmoid(input[j]);
                }
            }
            double scalar_end = get_time_ms();
            double scalar_time = (scalar_end - scalar_start) / PERFORMANCE_ITERATIONS;

            // 정확성 검증
            et_simd_sigmoid(input, simd_output, size);
            for (size_t j = 0; j < size; j++) {
                scalar_output[j] = et_fast_sigmoid(input[j]);
            }
            double accuracy_error = calculate_max_relative_error(scalar_output, simd_output, size);

            record_performance_result("Sigmoid", size, simd_time, scalar_time, accuracy_error);

            printf("  Sigmoid: SIMD=%.3fms, Scalar=%.3fms, Speedup=%.2fx\n",
                   simd_time, scalar_time, scalar_time / simd_time);
        }

        free(input);
        free(simd_output);
        free(scalar_output);
    }
}

// 복합 연산 성능 테스트
void test_complex_math_operations(void) {
    printf("\n=== Complex Math Operations Performance Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Testing complex operations (size %zu)... ", size);

        float* input1 = (float*)malloc(size * sizeof(float));
        float* input2 = (float*)malloc(size * sizeof(float));
        float* temp = (float*)malloc(size * sizeof(float));
        float* simd_result = (float*)malloc(size * sizeof(float));
        float* scalar_result = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input1);
        TEST_ASSERT_NOT_NULL(input2);
        TEST_ASSERT_NOT_NULL(temp);
        TEST_ASSERT_NOT_NULL(simd_result);
        TEST_ASSERT_NOT_NULL(scalar_result);

        generate_test_data(input1, size, -2.0f, 2.0f);
        generate_test_data(input2, size, -2.0f, 2.0f);

        // 복합 연산: (a * b + a) -> tanh -> sigmoid
        // SIMD 버전
        double simd_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            et_simd_vector_mul(input1, input2, temp, size);
            et_simd_vector_add(temp, input1, temp, size);
            et_simd_tanh(temp, temp, size);
            et_simd_sigmoid(temp, simd_result, size);
        }
        double simd_end = get_time_ms();
        double simd_time = (simd_end - simd_start) / PERFORMANCE_ITERATIONS;

        // 스칼라 버전
        double scalar_start = get_time_ms();
        for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
            for (size_t j = 0; j < size; j++) {
                float temp_val = input1[j] * input2[j] + input1[j];
                temp_val = et_fast_tanh(temp_val);
                scalar_result[j] = et_fast_sigmoid(temp_val);
            }
        }
        double scalar_end = get_time_ms();
        double scalar_time = (scalar_end - scalar_start) / PERFORMANCE_ITERATIONS;

        // 정확성 검증 (한 번만 실행)
        et_simd_vector_mul(input1, input2, temp, size);
        et_simd_vector_add(temp, input1, temp, size);
        et_simd_tanh(temp, temp, size);
        et_simd_sigmoid(temp, simd_result, size);

        for (size_t j = 0; j < size; j++) {
            float temp_val = input1[j] * input2[j] + input1[j];
            temp_val = et_fast_tanh(temp_val);
            scalar_result[j] = et_fast_sigmoid(temp_val);
        }

        double accuracy_error = calculate_max_relative_error(scalar_result, simd_result, size);
        record_performance_result("Complex", size, simd_time, scalar_time, accuracy_error);

        printf("SIMD=%.3fms, Scalar=%.3fms, Speedup=%.2fx, Error=%.4f%%\n",
               simd_time, scalar_time, scalar_time / simd_time, accuracy_error * 100.0);

        free(input1);
        free(input2);
        free(temp);
        free(simd_result);
        free(scalar_result);
    }
}

// 메모리 대역폭 테스트
void test_memory_bandwidth_characteristics(void) {
    printf("\n=== Memory Bandwidth Characteristics Tests ===\n");

    const size_t large_sizes[] = {1024, 4096, 16384, 65536, 262144};
    const size_t num_large_sizes = sizeof(large_sizes) / sizeof(large_sizes[0]);

    for (size_t i = 0; i < num_large_sizes; i++) {
        size_t size = large_sizes[i];
        printf("Testing memory bandwidth (size %zu)... ", size);

        float* input = (float*)malloc(size * sizeof(float));
        float* output = (float*)malloc(size * sizeof(float));

        if (!input || !output) {
            printf("SKIP (memory allocation failed)\n");
            free(input);
            free(output);
            continue;
        }

        generate_test_data(input, size, -1.0f, 1.0f);

        // 메모리 집약적 연산 (단순 복사)
        double start_time = get_time_ms();
        for (int iter = 0; iter < 100; iter++) {
            et_simd_vector_add(input, input, output, size);
        }
        double end_time = get_time_ms();

        double total_time = end_time - start_time;
        double bytes_per_iteration = size * sizeof(float) * 3; // 2 reads + 1 write
        double total_bytes = bytes_per_iteration * 100;
        double bandwidth_gb_per_sec = (total_bytes / (1024.0 * 1024.0 * 1024.0)) / (total_time / 1000.0);

        printf("Bandwidth: %.2f GB/s\n", bandwidth_gb_per_sec);

        // 대역폭이 합리적인 범위에 있는지 확인 (최소 1 GB/s)
        TEST_ASSERT_TRUE(bandwidth_gb_per_sec > 1.0);

        free(input);
        free(output);
    }
}

// 캐시 효율성 테스트
void test_cache_efficiency(void) {
    printf("\n=== Cache Efficiency Tests ===\n");

    // L1 캐시 크기 근사 (32KB)
    const size_t l1_cache_size = 32 * 1024 / sizeof(float);
    // L2 캐시 크기 근사 (256KB)
    const size_t l2_cache_size = 256 * 1024 / sizeof(float);
    // L3 캐시 크기 근사 (8MB)
    const size_t l3_cache_size = 8 * 1024 * 1024 / sizeof(float);

    size_t cache_test_sizes[] = {
        l1_cache_size / 4,    // L1 캐시 내
        l1_cache_size,        // L1 캐시 경계
        l2_cache_size / 4,    // L2 캐시 내
        l2_cache_size,        // L2 캐시 경계
        l3_cache_size / 4,    // L3 캐시 내
        l3_cache_size         // L3 캐시 경계
    };

    const char* cache_names[] = {
        "L1 (1/4)", "L1 (full)", "L2 (1/4)", "L2 (full)", "L3 (1/4)", "L3 (full)"
    };

    for (size_t i = 0; i < 6; i++) {
        size_t size = cache_test_sizes[i];
        printf("Testing %s cache efficiency (%zu elements)... ", cache_names[i], size);

        float* input = (float*)malloc(size * sizeof(float));
        float* output = (float*)malloc(size * sizeof(float));

        if (!input || !output) {
            printf("SKIP (memory allocation failed)\n");
            free(input);
            free(output);
            continue;
        }

        generate_test_data(input, size, -1.0f, 1.0f);

        // 캐시 효율성 측정 (반복 접근)
        double start_time = get_time_ms();
        for (int iter = 0; iter < 1000; iter++) {
            et_simd_vector_add(input, input, output, size);
        }
        double end_time = get_time_ms();

        double avg_time = (end_time - start_time) / 1000.0;
        double elements_per_ms = size / avg_time;

        printf("%.0f elements/ms\n", elements_per_ms);

        free(input);
        free(output);
    }
}

// 하드웨어 특성 테스트
void test_hardware_specific_optimizations(void) {
    printf("\n=== Hardware Specific Optimizations Tests ===\n");

    // 하드웨어 기능 확인
    bool has_sse = et_has_sse_support();
    bool has_avx = et_has_avx_support();
    bool has_neon = et_has_neon_support();

    printf("Hardware features: SSE=%s, AVX=%s, NEON=%s\n",
           has_sse ? "YES" : "NO",
           has_avx ? "YES" : "NO",
           has_neon ? "YES" : "NO");

    // 하드웨어별 최적화 테스트
    const size_t test_size = 1024;
    float* input_a = (float*)malloc(test_size * sizeof(float));
    float* input_b = (float*)malloc(test_size * sizeof(float));
    float* output = (float*)malloc(test_size * sizeof(float));

    TEST_ASSERT_NOT_NULL(input_a);
    TEST_ASSERT_NOT_NULL(input_b);
    TEST_ASSERT_NOT_NULL(output);

    generate_test_data(input_a, test_size, -10.0f, 10.0f);
    generate_test_data(input_b, test_size, -10.0f, 10.0f);

    // 벡터 연산 성능 측정
    double start_time = get_time_ms();
    for (int iter = 0; iter < PERFORMANCE_ITERATIONS; iter++) {
        et_simd_vector_add(input_a, input_b, output, test_size);
    }
    double end_time = get_time_ms();

    double avg_time = (end_time - start_time) / PERFORMANCE_ITERATIONS;
    double ops_per_sec = test_size / (avg_time / 1000.0);

    printf("Vector addition performance: %.0f ops/sec\n", ops_per_sec);

    // 성능이 합리적인 범위에 있는지 확인
    TEST_ASSERT_TRUE(ops_per_sec > 1000000.0); // 최소 1M ops/sec

    free(input_a);
    free(input_b);
    free(output);
}

// 성능 결과 출력
void print_performance_summary(void) {
    if (performance_result_count == 0) {
        return;
    }

    printf("\n=== SIMD Math Performance Summary ===\n");
    printf("%-12s %-8s %-12s %-12s %-10s %-12s\n",
           "Function", "Size", "SIMD (ms)", "Scalar (ms)", "Speedup", "Max Error (%)");
    printf("%-12s %-8s %-12s %-12s %-10s %-12s\n",
           "--------", "----", "---------", "-----------", "-------", "-------------");

    for (size_t i = 0; i < performance_result_count; i++) {
        SIMDMathPerformanceResult* result = &performance_results[i];
        printf("%-12s %-8zu %-12.6f %-12.6f %-10.2fx %-12.4f\n",
               result->test_name,
               result->data_size,
               result->simd_time_ms,
               result->scalar_time_ms,
               result->speedup_ratio,
               result->accuracy_error * 100.0);
    }
    printf("\n");
}

int main(void) {
    printf("LibEtude SIMD Math Functions Test Suite\n");
    printf("=======================================\n");

    UNITY_BEGIN();

    // 통합 테스트
    printf("\n>>> INTEGRATION TESTS <<<\n");
    RUN_TEST(test_simd_vector_math_integration);

    // 성능 테스트
    printf("\n>>> PERFORMANCE TESTS <<<\n");
    RUN_TEST(test_simd_activation_performance);
    RUN_TEST(test_complex_math_operations);

    // 메모리 및 캐시 테스트
    printf("\n>>> MEMORY AND CACHE TESTS <<<\n");
    RUN_TEST(test_memory_bandwidth_characteristics);
    RUN_TEST(test_cache_efficiency);

    // 하드웨어 특성 테스트
    printf("\n>>> HARDWARE OPTIMIZATION TESTS <<<\n");
    RUN_TEST(test_hardware_specific_optimizations);

    // 성능 결과 출력
    print_performance_summary();

    // 메모리 정리
    if (performance_results) {
        free(performance_results);
        performance_results = NULL;
    }

    printf("\n>>> TEST SUMMARY <<<\n");
    return UNITY_END();
}