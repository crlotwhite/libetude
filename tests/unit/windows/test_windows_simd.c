/**
 * @file test_windows_simd.c
 * @brief Windows SIMD 최적화 기능 테스트
 *
 * Windows 환경에서 CPU 기능 감지 및 SIMD 최적화된 연산을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#ifdef _WIN32
#include "libetude/platform/windows_simd.h"
#include "libetude/types.h"

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return 0; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

#define FLOAT_TOLERANCE 1e-5f

// 부동소수점 비교 함수
static int float_equals(float a, float b, float tolerance) {
    return fabsf(a - b) < tolerance;
}

// 벡터 비교 함수
static int vectors_equal(const float* a, const float* b, int size, float tolerance) {
    for (int i = 0; i < size; i++) {
        if (!float_equals(a[i], b[i], tolerance)) {
            printf("벡터 차이 발견: index=%d, a=%f, b=%f, diff=%f\n",
                   i, a[i], b[i], fabsf(a[i] - b[i]));
            return 0;
        }
    }
    return 1;
}

// 행렬 비교 함수
static int matrices_equal(const float* a, const float* b, int m, int n, float tolerance) {
    for (int i = 0; i < m * n; i++) {
        if (!float_equals(a[i], b[i], tolerance)) {
            printf("행렬 차이 발견: index=%d, a=%f, b=%f, diff=%f\n",
                   i, a[i], b[i], fabsf(a[i] - b[i]));
            return 0;
        }
    }
    return 1;
}

/**
 * @brief CPU 기능 감지 테스트
 */
static int test_cpu_feature_detection(void) {
    printf("\n=== CPU 기능 감지 테스트 ===\n");

    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();

    // 기본적인 CPU 기능 정보 출력
    char feature_str[512];
    et_windows_cpu_features_to_string(&features, feature_str, sizeof(feature_str));
    printf("감지된 CPU 기능: %s\n", feature_str);

    // 최소한 SSE2는 지원해야 함 (x64 시스템에서)
    #ifdef _M_X64
    TEST_ASSERT(features.has_sse2, "x64 시스템에서 SSE2 지원 확인");
    #endif

    // 두 번 호출해도 같은 결과가 나와야 함 (캐싱 테스트)
    ETWindowsCPUFeatures features2 = et_windows_detect_cpu_features();
    TEST_ASSERT(memcmp(&features, &features2, sizeof(features)) == 0,
                "CPU 기능 감지 결과 캐싱 확인");

    return 1;
}

/**
 * @brief 벡터 덧셈 테스트
 */
static int test_vector_addition(void) {
    printf("\n=== 벡터 덧셈 테스트 ===\n");

    const int size = 1000;
    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));
    float* c_fallback = (float*)malloc(size * sizeof(float));
    float* c_avx2 = (float*)malloc(size * sizeof(float));

    if (!a || !b || !c_fallback || !c_avx2) {
        printf("메모리 할당 실패\n");
        return 0;
    }

    // 테스트 데이터 초기화
    for (int i = 0; i < size; i++) {
        a[i] = (float)(i * 0.1);
        b[i] = (float)(i * 0.2);
    }

    // 기본 구현으로 계산
    et_windows_simd_vector_add_fallback(a, b, c_fallback, size);

    // AVX2 구현으로 계산 (지원되는 경우)
    et_windows_simd_vector_add_avx2(a, b, c_avx2, size);

    // 결과 비교
    TEST_ASSERT(vectors_equal(c_fallback, c_avx2, size, FLOAT_TOLERANCE),
                "벡터 덧셈 결과 일치 확인 (기본 vs AVX2)");

    // 경계 조건 테스트
    memset(c_avx2, 0, size * sizeof(float));
    et_windows_simd_vector_add_avx2(a, b, c_avx2, 7); // 8의 배수가 아닌 크기
    TEST_ASSERT(vectors_equal(c_fallback, c_avx2, 7, FLOAT_TOLERANCE),
                "벡터 덧셈 경계 조건 테스트 (크기 7)");

    free(a);
    free(b);
    free(c_fallback);
    free(c_avx2);

    return 1;
}

/**
 * @brief 벡터 내적 테스트
 */
static int test_vector_dot_product(void) {
    printf("\n=== 벡터 내적 테스트 ===\n");

    const int size = 1000;
    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));

    if (!a || !b) {
        printf("메모리 할당 실패\n");
        return 0;
    }

    // 테스트 데이터 초기화
    for (int i = 0; i < size; i++) {
        a[i] = (float)(i * 0.1);
        b[i] = (float)(i * 0.2);
    }

    // 기본 구현으로 계산
    float result_fallback = et_windows_simd_vector_dot_fallback(a, b, size);

    // AVX2 구현으로 계산 (지원되는 경우)
    float result_avx2 = et_windows_simd_vector_dot_avx2(a, b, size);

    printf("내적 결과 - 기본: %f, AVX2: %f, 차이: %f\n",
           result_fallback, result_avx2, fabsf(result_fallback - result_avx2));

    // 결과 비교 (부동소수점 오차 고려)
    TEST_ASSERT(float_equals(result_fallback, result_avx2, FLOAT_TOLERANCE * 10),
                "벡터 내적 결과 일치 확인 (기본 vs AVX2)");

    // 경계 조건 테스트
    float result_small = et_windows_simd_vector_dot_avx2(a, b, 7);
    float expected_small = et_windows_simd_vector_dot_fallback(a, b, 7);
    TEST_ASSERT(float_equals(result_small, expected_small, FLOAT_TOLERANCE),
                "벡터 내적 경계 조건 테스트 (크기 7)");

    free(a);
    free(b);

    return 1;
}

/**
 * @brief 행렬 곱셈 테스트
 */
static int test_matrix_multiplication(void) {
    printf("\n=== 행렬 곱셈 테스트 ===\n");

    const int m = 64, n = 64, k = 64;
    float* a = (float*)malloc(m * k * sizeof(float));
    float* b = (float*)malloc(k * n * sizeof(float));
    float* c_fallback = (float*)malloc(m * n * sizeof(float));
    float* c_avx2 = (float*)malloc(m * n * sizeof(float));
    float* c_auto = (float*)malloc(m * n * sizeof(float));

    if (!a || !b || !c_fallback || !c_avx2 || !c_auto) {
        printf("메모리 할당 실패\n");
        return 0;
    }

    // 테스트 데이터 초기화
    for (int i = 0; i < m * k; i++) {
        a[i] = (float)(i * 0.01);
    }
    for (int i = 0; i < k * n; i++) {
        b[i] = (float)(i * 0.02);
    }

    // 기본 구현으로 계산
    et_windows_simd_matrix_multiply_fallback(a, b, c_fallback, m, n, k);

    // AVX2 구현으로 계산
    et_windows_simd_matrix_multiply_avx2(a, b, c_avx2, m, n, k);

    // 자동 선택 구현으로 계산
    et_windows_simd_matrix_multiply_auto(a, b, c_auto, m, n, k);

    // 결과 비교
    TEST_ASSERT(matrices_equal(c_fallback, c_avx2, m, n, FLOAT_TOLERANCE * 100),
                "행렬 곱셈 결과 일치 확인 (기본 vs AVX2)");

    TEST_ASSERT(matrices_equal(c_fallback, c_auto, m, n, FLOAT_TOLERANCE * 100),
                "행렬 곱셈 결과 일치 확인 (기본 vs 자동 선택)");

    // 작은 크기 행렬 테스트
    const int small_m = 3, small_n = 5, small_k = 4;
    float small_a[12] = {1,2,3,4, 5,6,7,8, 9,10,11,12};
    float small_b[20] = {1,2,3,4,5, 6,7,8,9,10, 11,12,13,14,15, 16,17,18,19,20};
    float small_c_fallback[15] = {0};
    float small_c_avx2[15] = {0};

    et_windows_simd_matrix_multiply_fallback(small_a, small_b, small_c_fallback,
                                           small_m, small_n, small_k);
    et_windows_simd_matrix_multiply_avx2(small_a, small_b, small_c_avx2,
                                       small_m, small_n, small_k);

    TEST_ASSERT(matrices_equal(small_c_fallback, small_c_avx2, small_m, small_n, FLOAT_TOLERANCE),
                "작은 행렬 곱셈 테스트 (3x5)");

    free(a);
    free(b);
    free(c_fallback);
    free(c_avx2);
    free(c_auto);

    return 1;
}

/**
 * @brief 성능 벤치마크 테스트
 */
static int test_performance_benchmark(void) {
    printf("\n=== 성능 벤치마크 테스트 ===\n");

    const int size = 10000;
    const int iterations = 100;

    float* a = (float*)malloc(size * sizeof(float));
    float* b = (float*)malloc(size * sizeof(float));
    float* c = (float*)malloc(size * sizeof(float));

    if (!a || !b || !c) {
        printf("메모리 할당 실패\n");
        return 0;
    }

    // 테스트 데이터 초기화
    for (int i = 0; i < size; i++) {
        a[i] = (float)(i * 0.1);
        b[i] = (float)(i * 0.2);
    }

    // 기본 구현 성능 측정
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        et_windows_simd_vector_add_fallback(a, b, c, size);
    }
    clock_t end = clock();
    double time_fallback = ((double)(end - start)) / CLOCKS_PER_SEC;

    // AVX2 구현 성능 측정
    start = clock();
    for (int i = 0; i < iterations; i++) {
        et_windows_simd_vector_add_avx2(a, b, c, size);
    }
    end = clock();
    double time_avx2 = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("벡터 덧셈 성능 비교 (%d 반복, 크기 %d):\n", iterations, size);
    printf("  기본 구현: %.6f 초\n", time_fallback);
    printf("  AVX2 구현: %.6f 초\n", time_avx2);

    if (time_fallback > 0 && time_avx2 > 0) {
        double speedup = time_fallback / time_avx2;
        printf("  성능 향상: %.2fx\n", speedup);

        // AVX2가 지원되는 경우 성능 향상이 있어야 함
        ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
        if (features.has_avx2 && speedup < 0.8) {
            printf("경고: AVX2 구현이 기본 구현보다 느립니다.\n");
        }
    }

    free(a);
    free(b);
    free(c);

    return 1;
}

/**
 * @brief 모듈 초기화/정리 테스트
 */
static int test_module_lifecycle(void) {
    printf("\n=== 모듈 생명주기 테스트 ===\n");

    // 초기화 테스트
    ETResult result = et_windows_simd_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "SIMD 모듈 초기화 성공");

    // 중복 초기화 테스트 (문제없이 처리되어야 함)
    result = et_windows_simd_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "SIMD 모듈 중복 초기화 처리");

    // 정리 테스트
    et_windows_simd_finalize();
    printf("PASS: SIMD 모듈 정리 완료\n");

    // 정리 후 재초기화 테스트
    result = et_windows_simd_init();
    TEST_ASSERT(result == ET_RESULT_SUCCESS, "SIMD 모듈 재초기화 성공");

    et_windows_simd_finalize();

    return 1;
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("Windows SIMD 최적화 테스트 시작\n");
    printf("=====================================\n");

    int total_tests = 0;
    int passed_tests = 0;

    // 각 테스트 실행
    total_tests++; if (test_cpu_feature_detection()) passed_tests++;
    total_tests++; if (test_vector_addition()) passed_tests++;
    total_tests++; if (test_vector_dot_product()) passed_tests++;
    total_tests++; if (test_matrix_multiplication()) passed_tests++;
    total_tests++; if (test_performance_benchmark()) passed_tests++;
    total_tests++; if (test_module_lifecycle()) passed_tests++;

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", passed_tests, total_tests);

    if (passed_tests == total_tests) {
        printf("모든 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다.\n");
        return 1;
    }
}

#else
int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}
#endif // _WIN32