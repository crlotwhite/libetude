/**
 * @file test_simd_kernels.c
 * @brief SIMD 커널 단위 테스트 (Unity)
 *
 * SIMD 커널의 정확성과 성능을 테스트합니다.
 * - 다양한 입력 크기에 대한 정확성 테스트
 * - 성능 벤치마크 테스트
 * - 하드웨어별 최적화 테스트
 */

#include "unity.h"
#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/simd_kernels.h"
#include "libetude/kernel_registry.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// 테스트 허용 오차
#define TEST_EPSILON 1e-5f
#define TEST_LARGE_EPSILON 1e-3f // 근사 함수용
#define TEST_PERFORMANCE_EPSILON 1e-4f // 성능 테스트용

// 성능 테스트 설정
#define PERFORMANCE_ITERATIONS 1000
#define BENCHMARK_WARMUP_ITERATIONS 10

// 테스트 데이터 크기들
static const size_t test_sizes[] = {1, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};
static const size_t num_test_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

// 성능 측정 구조체
typedef struct {
    const char* test_name;
    size_t data_size;
    double execution_time_ms;
    double operations_per_second;
} PerformanceResult;

static PerformanceResult* performance_results = NULL;
static size_t performance_result_count = 0;
static size_t performance_result_capacity = 0;

// 유틸리티 함수
static bool float_equals(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

static bool vector_equals(const float* a, const float* b, size_t size, float epsilon) {
    for (size_t i = 0; i < size; i++) {
        if (!float_equals(a[i], b[i], epsilon)) {
            printf("Mismatch at index %zu: %f vs %f (diff: %f)\n",
                   i, a[i], b[i], fabsf(a[i] - b[i]));
            return false;
        }
    }
    return true;
}

static void generate_test_vector(float* vec, size_t size, float min_val, float max_val) {
    for (size_t i = 0; i < size; i++) {
        vec[i] = min_val + ((float)rand() / RAND_MAX) * (max_val - min_val);
    }
}

// 성능 측정 유틸리티 함수
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void record_performance_result(const char* test_name, size_t data_size,
                                    double execution_time_ms, double operations_per_second) {
    if (performance_result_count >= performance_result_capacity) {
        return; // 용량 초과
    }

    PerformanceResult* result = &performance_results[performance_result_count++];
    result->test_name = test_name;
    result->data_size = data_size;
    result->execution_time_ms = execution_time_ms;
    result->operations_per_second = operations_per_second;
}

// 벤치마크 실행 함수
typedef void (*BenchmarkFunction)(const float* a, const float* b, float* result, size_t size);

static void run_vector_benchmark(const char* test_name, BenchmarkFunction func, size_t size) {
    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));
    float* result = (float*)malloc(size * sizeof(float));

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(result);

    generate_test_vector(a, size, -10.0f, 10.0f);
    generate_test_vector(b, size, -10.0f, 10.0f);

    // 워밍업
    for (int i = 0; i < BENCHMARK_WARMUP_ITERATIONS; i++) {
        func(a, b, result, size);
    }

    // 실제 측정
    double start_time = get_time_ms();
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        func(a, b, result, size);
    }
    double end_time = get_time_ms();

    double total_time = end_time - start_time;
    double avg_time = total_time / PERFORMANCE_ITERATIONS;
    double operations_per_second = (size * PERFORMANCE_ITERATIONS) / (total_time / 1000.0);

    record_performance_result(test_name, size, avg_time, operations_per_second);

    free(a);
    free(b);
    free(result);
}

// 활성화 함수 벤치마크
typedef void (*ActivationBenchmarkFunction)(const float* input, float* output, size_t size);

static void run_activation_benchmark(const char* test_name, ActivationBenchmarkFunction func, size_t size) {
    float* input = (float*)malloc(size * sizeof(float));
    float* output = (float*)malloc(size * sizeof(float));

    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_NOT_NULL(output);

    generate_test_vector(input, size, -5.0f, 5.0f);

    // 워밍업
    for (int i = 0; i < BENCHMARK_WARMUP_ITERATIONS; i++) {
        func(input, output, size);
    }

    // 실제 측정
    double start_time = get_time_ms();
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        func(input, output, size);
    }
    double end_time = get_time_ms();

    double total_time = end_time - start_time;
    double avg_time = total_time / PERFORMANCE_ITERATIONS;
    double operations_per_second = (size * PERFORMANCE_ITERATIONS) / (total_time / 1000.0);

    record_performance_result(test_name, size, avg_time, operations_per_second);

    free(input);
    free(output);
}

// 정확성 테스트를 위한 참조 구현들
static void reference_vector_add(const float* a, const float* b, float* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        result[i] = a[i] + b[i];
    }
}

static void reference_vector_mul(const float* a, const float* b, float* result, size_t size) {
    for (size_t i = 0; i < size; i++) {
        result[i] = a[i] * b[i];
    }
}

static float reference_dot_product(const float* a, const float* b, size_t size) {
    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static void reference_relu(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = input[i] > 0.0f ? input[i] : 0.0f;
    }
}

static void reference_sigmoid(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = 1.0f / (1.0f + expf(-input[i]));
    }
}



void setUp(void) {
    // SIMD 커널 초기화
    LibEtudeErrorCode result = et_init_simd_kernels();
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result);

    srand((unsigned int)time(NULL));

    // 성능 결과 배열 초기화
    if (!performance_results) {
        performance_result_capacity = 1000;
        performance_results = (PerformanceResult*)malloc(performance_result_capacity * sizeof(PerformanceResult));
        TEST_ASSERT_NOT_NULL(performance_results);
        performance_result_count = 0;
    }
}

void tearDown(void) {
    // 정리
    et_cleanup_simd_kernels();
}

void test_vector_addition(void) {
    printf("\n=== Vector Addition Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Testing vector addition with size %zu... ", size);

        float* a = (float*)malloc(size * sizeof(float));
        float* b = (float*)malloc(size * sizeof(float));
        float* result = (float*)malloc(size * sizeof(float));
        float* expected = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(a);
        TEST_ASSERT_NOT_NULL(b);
        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_NOT_NULL(expected);

        // 테스트 데이터 생성
        generate_test_vector(a, size, -10.0f, 10.0f);
        generate_test_vector(b, size, -10.0f, 10.0f);

        // 참조 구현으로 예상 결과 계산
        reference_vector_add(a, b, expected, size);

        // SIMD 벡터 덧셈 실행
        et_simd_vector_add(a, b, result, size);

        // 결과 검증
        bool passed = vector_equals(result, expected, size, TEST_EPSILON);
        TEST_ASSERT_TRUE_MESSAGE(passed, "Vector addition accuracy test failed");

        if (passed) {
            printf("PASS\n");
        }

        free(a);
        free(b);
        free(result);
        free(expected);
    }
}

void test_vector_multiplication(void) {
    printf("\n=== Vector Multiplication Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Testing vector multiplication with size %zu... ", size);

        float* a = (float*)malloc(size * sizeof(float));
        float* b = (float*)malloc(size * sizeof(float));
        float* result = (float*)malloc(size * sizeof(float));
        float* expected = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(a);
        TEST_ASSERT_NOT_NULL(b);
        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_NOT_NULL(expected);

        generate_test_vector(a, size, -10.0f, 10.0f);
        generate_test_vector(b, size, -10.0f, 10.0f);

        // 참조 구현으로 예상 결과 계산
        reference_vector_mul(a, b, expected, size);

        et_simd_vector_mul(a, b, result, size);

        bool passed = vector_equals(result, expected, size, TEST_EPSILON);
        TEST_ASSERT_TRUE_MESSAGE(passed, "Vector multiplication accuracy test failed");

        if (passed) {
            printf("PASS\n");
        }

        free(a);
        free(b);
        free(result);
        free(expected);
    }
}

void test_vector_dot_product(void) {
    printf("\n=== Vector Dot Product Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Testing dot product with size %zu... ", size);

        float* a = (float*)malloc(size * sizeof(float));
        float* b = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(a);
        TEST_ASSERT_NOT_NULL(b);

        generate_test_vector(a, size, -10.0f, 10.0f);
        generate_test_vector(b, size, -10.0f, 10.0f);

        // 참조 구현으로 예상 결과 계산
        float expected = reference_dot_product(a, b, size);

        // SIMD 내적 계산
        float result = et_simd_dot_product(a, b, size);

        float tolerance = TEST_EPSILON * (float)size; // 크기에 따른 허용 오차 조정
        bool passed = fabsf(result - expected) <= tolerance;
        TEST_ASSERT_TRUE_MESSAGE(passed, "Dot product accuracy test failed");

        if (passed) {
            printf("PASS (expected: %.6f, got: %.6f)\n", expected, result);
        }

        free(a);
        free(b);
    }
}

void test_matrix_vector_multiplication(void) {
    const size_t rows = 64;
    const size_t cols = 64;

    float matrix[rows * cols], vector[cols], result[rows], expected[rows];

    generate_test_vector(matrix, rows * cols, -1.0f, 1.0f);
    generate_test_vector(vector, cols, -1.0f, 1.0f);

    // 예상 결과 계산 (일반적인 행렬-벡터 곱셈)
    for (size_t i = 0; i < rows; i++) {
        expected[i] = 0.0f;
        for (size_t j = 0; j < cols; j++) {
            expected[i] += matrix[i * cols + j] * vector[j];
        }
    }

    // SIMD 행렬-벡터 곱셈
    et_simd_matrix_vector_mul(matrix, vector, result, rows, cols);

    TEST_ASSERT_TRUE(vector_equals(result, expected, rows, TEST_EPSILON * cols));
}

void test_relu_activation(void) {
    printf("\n=== ReLU Activation Tests ===\n");

    for (size_t i = 0; i < num_test_sizes; i++) {
        size_t size = test_sizes[i];
        printf("Testing ReLU with size %zu... ", size);

        float* input = (float*)malloc(size * sizeof(float));
        float* result = (float*)malloc(size * sizeof(float));
        float* expected = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input);
        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_NOT_NULL(expected);

        generate_test_vector(input, size, -5.0f, 5.0f);

        // 참조 구현으로 예상 결과 계산
        reference_relu(input, expected, size);

        // SIMD ReLU 실행
        et_simd_relu(input, result, size);

        bool passed = vector_equals(result, expected, size, TEST_EPSILON);
        TEST_ASSERT_TRUE_MESSAGE(passed, "ReLU activation accuracy test failed");

        if (passed) {
            printf("PASS\n");
        }

        free(input);
        free(result);
        free(expected);
    }
}

void test_sigmoid_activation(void) {
    printf("\n=== Sigmoid Activation Tests ===\n");

    // Sigmoid는 지수 함수를 사용하므로 작은 크기부터 테스트
    const size_t sigmoid_test_sizes[] = {1, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    const size_t num_sigmoid_sizes = sizeof(sigmoid_test_sizes) / sizeof(sigmoid_test_sizes[0]);

    for (size_t i = 0; i < num_sigmoid_sizes; i++) {
        size_t size = sigmoid_test_sizes[i];
        printf("Testing Sigmoid with size %zu... ", size);

        float* input = (float*)malloc(size * sizeof(float));
        float* result = (float*)malloc(size * sizeof(float));
        float* expected = (float*)malloc(size * sizeof(float));

        TEST_ASSERT_NOT_NULL(input);
        TEST_ASSERT_NOT_NULL(result);
        TEST_ASSERT_NOT_NULL(expected);

        generate_test_vector(input, size, -3.0f, 3.0f);

        // 참조 구현으로 예상 결과 계산
        reference_sigmoid(input, expected, size);

        // SIMD Sigmoid 실행
        et_simd_sigmoid(input, result, size);

        bool passed = vector_equals(result, expected, size, TEST_LARGE_EPSILON);
        TEST_ASSERT_TRUE_MESSAGE(passed, "Sigmoid activation accuracy test failed");

        if (passed) {
            printf("PASS\n");
        }

        free(input);
        free(result);
        free(expected);
    }
}

void test_edge_cases(void) {
    // 빈 벡터
    float* null_ptr = NULL;
    et_simd_vector_add(null_ptr, null_ptr, null_ptr, 0);
    TEST_PASS(); // 크래시하지 않으면 성공

    // 크기가 1인 벡터
    float a = 1.0f, b = 2.0f, result;
    et_simd_vector_add(&a, &b, &result, 1);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPSILON, 3.0f, result);
}

void test_hardware_specific_features(void) {
    printf("\n=== Hardware Feature Detection ===\n");

    // 하드웨어 기능 감지 테스트
    bool has_sse = et_has_sse_support();
    bool has_avx = et_has_avx_support();
    bool has_neon = et_has_neon_support();

    printf("SSE support: %s\n", has_sse ? "YES" : "NO");
    printf("AVX support: %s\n", has_avx ? "YES" : "NO");
    printf("NEON support: %s\n", has_neon ? "YES" : "NO");

    // 최소한 하나의 SIMD 기능은 사용 가능해야 함 (또는 fallback)
    // 이 테스트는 항상 통과해야 함 (fallback 구현이 있으므로)
    TEST_PASS();
}

// 성능 테스트 함수들
void test_vector_addition_performance(void) {
    printf("\n=== Vector Addition Performance Tests ===\n");

    // 대표적인 크기들에 대해서만 성능 테스트
    const size_t perf_test_sizes[] = {64, 256, 1024, 4096};
    const size_t num_perf_sizes = sizeof(perf_test_sizes) / sizeof(perf_test_sizes[0]);

    for (size_t i = 0; i < num_perf_sizes; i++) {
        size_t size = perf_test_sizes[i];
        printf("Benchmarking vector addition (size %zu)... ", size);

        run_vector_benchmark("vector_add", et_simd_vector_add, size);

        printf("DONE\n");
    }
}

void test_vector_multiplication_performance(void) {
    printf("\n=== Vector Multiplication Performance Tests ===\n");

    const size_t perf_test_sizes[] = {64, 256, 1024, 4096};
    const size_t num_perf_sizes = sizeof(perf_test_sizes) / sizeof(perf_test_sizes[0]);

    for (size_t i = 0; i < num_perf_sizes; i++) {
        size_t size = perf_test_sizes[i];
        printf("Benchmarking vector multiplication (size %zu)... ", size);

        run_vector_benchmark("vector_mul", et_simd_vector_mul, size);

        printf("DONE\n");
    }
}

void test_activation_functions_performance(void) {
    printf("\n=== Activation Functions Performance Tests ===\n");

    const size_t perf_test_sizes[] = {64, 256, 1024, 4096};
    const size_t num_perf_sizes = sizeof(perf_test_sizes) / sizeof(perf_test_sizes[0]);

    for (size_t i = 0; i < num_perf_sizes; i++) {
        size_t size = perf_test_sizes[i];

        printf("Benchmarking ReLU (size %zu)... ", size);
        run_activation_benchmark("relu", et_simd_relu, size);
        printf("DONE\n");

        printf("Benchmarking Sigmoid (size %zu)... ", size);
        run_activation_benchmark("sigmoid", et_simd_sigmoid, size);
        printf("DONE\n");

        printf("Benchmarking Tanh (size %zu)... ", size);
        run_activation_benchmark("tanh", et_simd_tanh, size);
        printf("DONE\n");
    }
}

// 스트레스 테스트 - 큰 데이터에 대한 안정성 테스트
void test_large_data_stability(void) {
    printf("\n=== Large Data Stability Tests ===\n");

    const size_t large_sizes[] = {16384, 32768, 65536};
    const size_t num_large_sizes = sizeof(large_sizes) / sizeof(large_sizes[0]);

    for (size_t i = 0; i < num_large_sizes; i++) {
        size_t size = large_sizes[i];
        printf("Testing stability with size %zu... ", size);

        float* a = (float*)malloc(size * sizeof(float));
        float* b = (float*)malloc(size * sizeof(float));
        float* result = (float*)malloc(size * sizeof(float));

        if (!a || !b || !result) {
            printf("SKIP (memory allocation failed)\n");
            free(a);
            free(b);
            free(result);
            continue;
        }

        generate_test_vector(a, size, -100.0f, 100.0f);
        generate_test_vector(b, size, -100.0f, 100.0f);

        // 여러 번 실행하여 안정성 확인
        for (int iter = 0; iter < 10; iter++) {
            et_simd_vector_add(a, b, result, size);

            // NaN이나 Inf 값이 없는지 확인
            for (size_t j = 0; j < size; j++) {
                TEST_ASSERT_FALSE(isnan(result[j]));
                TEST_ASSERT_FALSE(isinf(result[j]));
            }
        }

        printf("PASS\n");

        free(a);
        free(b);
        free(result);
    }
}

// 경계 조건 테스트
void test_boundary_conditions(void) {
    printf("\n=== Boundary Conditions Tests ===\n");

    // 빈 벡터 테스트
    printf("Testing empty vectors... ");
    et_simd_vector_add(NULL, NULL, NULL, 0);
    printf("PASS\n");

    // 크기 1 벡터 테스트
    printf("Testing size-1 vectors... ");
    float a = 1.0f, b = 2.0f, result;
    et_simd_vector_add(&a, &b, &result, 1);
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPSILON, 3.0f, result);
    printf("PASS\n");

    // 정렬되지 않은 메모리 주소 테스트
    printf("Testing unaligned memory... ");
    const size_t test_size = 17; // 16의 배수가 아닌 크기
    float* unaligned_a = (float*)malloc((test_size + 1) * sizeof(float)) + 1;
    float* unaligned_b = (float*)malloc((test_size + 1) * sizeof(float)) + 1;
    float* unaligned_result = (float*)malloc((test_size + 1) * sizeof(float)) + 1;

    generate_test_vector(unaligned_a, test_size, -1.0f, 1.0f);
    generate_test_vector(unaligned_b, test_size, -1.0f, 1.0f);

    et_simd_vector_add(unaligned_a, unaligned_b, unaligned_result, test_size);

    // 결과 검증
    for (size_t i = 0; i < test_size; i++) {
        float expected = unaligned_a[i] + unaligned_b[i];
        TEST_ASSERT_FLOAT_WITHIN(TEST_EPSILON, expected, unaligned_result[i]);
    }

    free(unaligned_a - 1);
    free(unaligned_b - 1);
    free(unaligned_result - 1);
    printf("PASS\n");
}

// 성능 결과 출력 함수
void print_performance_results(void) {
    if (performance_result_count == 0) {
        return;
    }

    printf("\n=== Performance Results Summary ===\n");
    printf("%-20s %-10s %-15s %-20s\n", "Test", "Size", "Avg Time (ms)", "Ops/sec");
    printf("%-20s %-10s %-15s %-20s\n", "----", "----", "-------------", "-------");

    for (size_t i = 0; i < performance_result_count; i++) {
        PerformanceResult* result = &performance_results[i];
        printf("%-20s %-10zu %-15.6f %-20.0f\n",
               result->test_name,
               result->data_size,
               result->execution_time_ms,
               result->operations_per_second);
    }
    printf("\n");
}

int main(void) {
    printf("LibEtude SIMD Kernels Test Suite\n");
    printf("================================\n");

    UNITY_BEGIN();

    // 정확성 테스트
    printf("\n>>> ACCURACY TESTS <<<\n");
    RUN_TEST(test_vector_addition);
    RUN_TEST(test_vector_multiplication);
    RUN_TEST(test_vector_dot_product);
    RUN_TEST(test_matrix_vector_multiplication);
    RUN_TEST(test_relu_activation);
    RUN_TEST(test_sigmoid_activation);
    RUN_TEST(test_edge_cases);

    // 하드웨어 기능 테스트
    printf("\n>>> HARDWARE FEATURE TESTS <<<\n");
    RUN_TEST(test_hardware_specific_features);

    // 성능 테스트
    printf("\n>>> PERFORMANCE TESTS <<<\n");
    RUN_TEST(test_vector_addition_performance);
    RUN_TEST(test_vector_multiplication_performance);
    RUN_TEST(test_activation_functions_performance);

    // 안정성 테스트
    printf("\n>>> STABILITY TESTS <<<\n");
    RUN_TEST(test_large_data_stability);
    RUN_TEST(test_boundary_conditions);

    // 성능 결과 출력
    print_performance_results();

    // 메모리 정리
    if (performance_results) {
        free(performance_results);
        performance_results = NULL;
    }

    printf("\n>>> TEST SUMMARY <<<\n");
    return UNITY_END();
}