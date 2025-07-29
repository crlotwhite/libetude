/**
 * @file kernel_registry.c
 * @brief 커널 레지스트리 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include "libetude/kernel_registry.h"
#include "libetude/hardware.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_KERNELS 256
#define MAX_KERNEL_NAME_LEN 64

// 외부 함수 선언
extern void register_cpu_kernels(void);

#ifdef LIBETUDE_HAVE_SSE2
extern void register_sse_kernels(void);
#endif

#ifdef LIBETUDE_HAVE_AVX
extern void register_avx_kernels(void);
#endif

#ifdef LIBETUDE_HAVE_NEON
extern void register_neon_kernels(void);
#endif

extern void register_gpu_kernels(void);

// 전역 커널 레지스트리
static KernelRegistry g_kernel_registry = {0};

// 내부 함수 전방 선언
static void kernel_registry_simd_features_to_string(uint32_t features, char* buffer, size_t buffer_size);

/**
 * @brief 커널 벤치마크 함수
 *
 * @param kernel_info 벤치마크할 커널 정보
 * @param iterations 반복 횟수
 * @return float 성능 점수 (높을수록 좋음)
 */
static float benchmark_kernel(const KernelInfo* kernel_info, int iterations) {
    // 벤치마크를 위한 간단한 구현
    // 실제로는 더 정교한 벤치마크가 필요함

    // 커널 타입에 따라 다른 벤치마크 로직 적용
    if (strstr(kernel_info->name, "vector_add") != NULL) {
        // 벡터 덧셈 벤치마크
        const size_t size = 10000;
        float* a = (float*)malloc(size * sizeof(float));
        float* b = (float*)malloc(size * sizeof(float));
        float* result = (float*)malloc(size * sizeof(float));

        // 테스트 데이터 초기화
        for (size_t i = 0; i < size; i++) {
            a[i] = (float)i;
            b[i] = (float)(size - i);
        }

        // 벤치마크 실행
        clock_t start = clock();
        for (int i = 0; i < iterations; i++) {
            VectorAddKernel kernel_func = (VectorAddKernel)kernel_info->kernel_func;
            kernel_func(a, b, result, size);
        }
        clock_t end = clock();

        // 성능 점수 계산 (낮은 시간 = 높은 점수)
        float elapsed = (float)(end - start) / CLOCKS_PER_SEC;
        float score = iterations / (elapsed + 0.001f); // 0으로 나누기 방지

        free(a);
        free(b);
        free(result);
        return score;
    }
    else if (strstr(kernel_info->name, "vector_mul") != NULL) {
        // 벡터 곱셈 벤치마크 (위와 유사)
        const size_t size = 10000;
        float* a = (float*)malloc(size * sizeof(float));
        float* b = (float*)malloc(size * sizeof(float));
        float* result = (float*)malloc(size * sizeof(float));

        for (size_t i = 0; i < size; i++) {
            a[i] = (float)i / 100.0f;
            b[i] = (float)(size - i) / 100.0f;
        }

        clock_t start = clock();
        for (int i = 0; i < iterations; i++) {
            VectorMulKernel kernel_func = (VectorMulKernel)kernel_info->kernel_func;
            kernel_func(a, b, result, size);
        }
        clock_t end = clock();

        float elapsed = (float)(end - start) / CLOCKS_PER_SEC;
        float score = iterations / (elapsed + 0.001f);

        free(a);
        free(b);
        free(result);
        return score;
    }
    else if (strstr(kernel_info->name, "matmul") != NULL) {
        // 행렬 곱셈 벤치마크
        const size_t m = 100, n = 100, k = 100;
        float* a = (float*)malloc(m * k * sizeof(float));
        float* b = (float*)malloc(k * n * sizeof(float));
        float* c = (float*)malloc(m * n * sizeof(float));

        for (size_t i = 0; i < m * k; i++) {
            a[i] = (float)i / 1000.0f;
        }
        for (size_t i = 0; i < k * n; i++) {
            b[i] = (float)i / 1000.0f;
        }

        clock_t start = clock();
        for (int i = 0; i < iterations / 10; i++) { // 행렬 곱셈은 더 무거워서 반복 횟수 감소
            MatMulKernel kernel_func = (MatMulKernel)kernel_info->kernel_func;
            kernel_func(a, b, c, m, n, k);
        }
        clock_t end = clock();

        float elapsed = (float)(end - start) / CLOCKS_PER_SEC;
        float score = (iterations / 10) / (elapsed + 0.001f);

        free(a);
        free(b);
        free(c);
        return score;
    }
    else if (strstr(kernel_info->name, "activation") != NULL) {
        // 활성화 함수 벤치마크
        const size_t size = 10000;
        float* input = (float*)malloc(size * sizeof(float));
        float* output = (float*)malloc(size * sizeof(float));

        for (size_t i = 0; i < size; i++) {
            input[i] = ((float)i / size) * 2.0f - 1.0f; // -1 ~ 1 범위
        }

        clock_t start = clock();
        for (int i = 0; i < iterations; i++) {
            ActivationKernel kernel_func = (ActivationKernel)kernel_info->kernel_func;
            kernel_func(input, output, size);
        }
        clock_t end = clock();

        float elapsed = (float)(end - start) / CLOCKS_PER_SEC;
        float score = iterations / (elapsed + 0.001f);

        free(input);
        free(output);
        return score;
    }

    // 기본 점수
    return 1.0f;
}

/**
 * @brief 커널 레지스트리를 초기화합니다
 */
LIBETUDE_API LibEtudeErrorCode kernel_registry_init(void) {
    if (g_kernel_registry.initialized) {
        return LIBETUDE_SUCCESS;
    }

    // 메모리 할당
    g_kernel_registry.kernels = (KernelInfo*)malloc(MAX_KERNELS * sizeof(KernelInfo));
    if (!g_kernel_registry.kernels) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    g_kernel_registry.kernel_count = 0;
    g_kernel_registry.capacity = MAX_KERNELS;

    // 하드웨어 기능 감지 (간단한 버전)
    g_kernel_registry.hardware_features = LIBETUDE_SIMD_NONE;

#ifdef LIBETUDE_HAVE_SSE2
    g_kernel_registry.hardware_features |= LIBETUDE_SIMD_SSE2;
#endif

#ifdef LIBETUDE_HAVE_AVX
    g_kernel_registry.hardware_features |= LIBETUDE_SIMD_AVX;
#endif

#ifdef LIBETUDE_HAVE_NEON
    g_kernel_registry.hardware_features |= LIBETUDE_SIMD_NEON;
#endif

    // 기본 CPU 커널 등록
    register_cpu_kernels();

    // SIMD 커널 등록 (하드웨어 지원 여부에 따라)
#ifdef LIBETUDE_HAVE_SSE2
    if (g_kernel_registry.hardware_features & LIBETUDE_SIMD_SSE2) {
        register_sse_kernels();
    }
#endif

#ifdef LIBETUDE_HAVE_AVX
    if (g_kernel_registry.hardware_features & LIBETUDE_SIMD_AVX) {
        register_avx_kernels();
    }
#endif

#ifdef LIBETUDE_HAVE_NEON
    if (g_kernel_registry.hardware_features & LIBETUDE_SIMD_NEON) {
        register_neon_kernels();
    }
#endif

    g_kernel_registry.initialized = true;
    return LIBETUDE_SUCCESS;
}

/**
 * @brief 커널 레지스트리를 정리합니다
 */
LIBETUDE_API void kernel_registry_finalize(void) {
    if (!g_kernel_registry.initialized) {
        return;
    }

    // 메모리 해제
    if (g_kernel_registry.kernels) {
        free(g_kernel_registry.kernels);
        g_kernel_registry.kernels = NULL;
    }

    g_kernel_registry.kernel_count = 0;
    g_kernel_registry.capacity = 0;
    g_kernel_registry.initialized = false;
}

/**
 * @brief 커널을 레지스트리에 등록합니다
 */
LIBETUDE_API LibEtudeErrorCode kernel_registry_register(const KernelInfo* kernel_info) {
    printf("Attempting to register kernel: %s\n", kernel_info ? kernel_info->name : "NULL");

    if (!g_kernel_registry.initialized) {
        printf("Registry not initialized!\n");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (!kernel_info || !kernel_info->name || !kernel_info->kernel_func) {
        printf("Invalid kernel info!\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 용량 확인
    if (g_kernel_registry.kernel_count >= g_kernel_registry.capacity) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    // 중복 이름 확인
    for (size_t i = 0; i < g_kernel_registry.kernel_count; i++) {
        if (strcmp(g_kernel_registry.kernels[i].name, kernel_info->name) == 0) {
            // 이미 등록된 커널 업데이트
            g_kernel_registry.kernels[i] = *kernel_info;
            return LIBETUDE_SUCCESS;
        }
    }

    // 새 커널 등록
    g_kernel_registry.kernels[g_kernel_registry.kernel_count] = *kernel_info;
    g_kernel_registry.kernel_count++;

    printf("Successfully registered kernel: %s (total: %zu)\n", kernel_info->name, g_kernel_registry.kernel_count);

    return LIBETUDE_SUCCESS;
}

/**
 * @brief 최적의 커널을 선택합니다
 */
LIBETUDE_API void* kernel_registry_select_optimal(const char* kernel_name, size_t data_size) {
    if (!g_kernel_registry.initialized || !kernel_name) {
        return NULL;
    }

    KernelInfo* best_kernel = NULL;
    float best_score = -1.0f;

    // 등록된 커널들 중에서 최적 커널 선택
    for (size_t i = 0; i < g_kernel_registry.kernel_count; i++) {
        KernelInfo* kernel = &g_kernel_registry.kernels[i];

        // 이름 매칭 확인
        if (strstr(kernel->name, kernel_name) == NULL) {
            continue;
        }

        // SIMD 지원 확인
        if ((kernel->simd_features & g_kernel_registry.hardware_features) == 0 &&
            kernel->simd_features != LIBETUDE_SIMD_NONE) {
            continue;
        }

        // 데이터 크기 최적화 고려
        float size_score = 1.0f;
        if (kernel->optimal_size > 0) {
            if (data_size < kernel->optimal_size / 4) {
                size_score = 0.5f; // 작은 데이터에는 덜 최적화
            } else if (data_size >= kernel->optimal_size) {
                size_score = 2.0f; // 최적 크기 이상에서는 더 최적화
            }
        }

        // 최종 점수 계산
        float score = kernel->performance_score * size_score;

        if (score > best_score) {
            best_score = score;
            best_kernel = kernel;
        }
    }

    return best_kernel ? best_kernel->kernel_func : NULL;
}

/**
 * @brief 현재 하드웨어 기능을 반환합니다
 */
LIBETUDE_API uint32_t kernel_registry_get_hardware_features(void) {
    return g_kernel_registry.hardware_features;
}

/**
 * @brief 등록된 커널 수를 반환합니다
 */
LIBETUDE_API size_t kernel_registry_get_kernel_count(void) {
    return g_kernel_registry.kernel_count;
}

/**
 * @brief 커널 벤치마크를 실행하고 성능 점수를 업데이트합니다
 */
LIBETUDE_API LibEtudeErrorCode kernel_registry_run_benchmarks(void) {
    if (!g_kernel_registry.initialized) {
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    printf("Running kernel benchmarks...\n");

    // 각 커널에 대해 벤치마크 실행
    for (size_t i = 0; i < g_kernel_registry.kernel_count; i++) {
        KernelInfo* kernel = &g_kernel_registry.kernels[i];

        // 현재 하드웨어에서 지원하는 커널만 벤치마크
        if ((kernel->simd_features & g_kernel_registry.hardware_features) == 0 &&
            kernel->simd_features != LIBETUDE_SIMD_NONE) {
            continue;
        }

        printf("  Benchmarking %s... ", kernel->name);
        fflush(stdout);

        // 벤치마크 실행
        const int iterations = 100;
        float score = benchmark_kernel(kernel, iterations);
        kernel->performance_score = score;

        printf("Score: %.2f\n", score);
    }

    printf("Benchmarks completed.\n");
    return LIBETUDE_SUCCESS;
}

/**
 * @brief 커널 레지스트리 정보를 출력합니다 (디버그용)
 */
LIBETUDE_API void kernel_registry_print_info(void) {
    if (!g_kernel_registry.initialized) {
        printf("Kernel registry not initialized\n");
        return;
    }

    printf("=== LibEtude Kernel Registry ===\n");

    // 하드웨어 기능 출력
    char features_str[256];
    kernel_registry_simd_features_to_string(g_kernel_registry.hardware_features,
                                           features_str, sizeof(features_str));
    printf("Hardware features: 0x%08X (%s)\n",
           g_kernel_registry.hardware_features, features_str);

    printf("Registered kernels: %zu\n", g_kernel_registry.kernel_count);

    // 커널 그룹별로 정렬하여 출력
    const char* groups[] = {"vector_add", "vector_mul", "matmul", "activation"};
    const int num_groups = sizeof(groups) / sizeof(groups[0]);

    for (int g = 0; g < num_groups; g++) {
        bool header_printed = false;

        for (size_t i = 0; i < g_kernel_registry.kernel_count; i++) {
            KernelInfo* kernel = &g_kernel_registry.kernels[i];

            if (strstr(kernel->name, groups[g]) != NULL) {
                if (!header_printed) {
                    printf("\n%s kernels:\n", groups[g]);
                    header_printed = true;
                }

                // SIMD 기능 문자열 변환
                char simd_str[128];
                kernel_registry_simd_features_to_string(kernel->simd_features,
                                                       simd_str, sizeof(simd_str));

                // 현재 하드웨어에서 사용 가능한지 표시
                bool available = (kernel->simd_features & g_kernel_registry.hardware_features) != 0 ||
                                kernel->simd_features == LIBETUDE_SIMD_NONE;

                printf("  [%zu] %s\n", i, kernel->name);
                printf("      SIMD: %s\n", simd_str);
                printf("      Optimal size: %zu\n", kernel->optimal_size);
                printf("      Performance score: %.2f\n", kernel->performance_score);
                printf("      Available: %s\n", available ? "Yes" : "No");
            }
        }
    }

    // 그룹에 속하지 않는 기타 커널 출력
    bool other_header_printed = false;

    for (size_t i = 0; i < g_kernel_registry.kernel_count; i++) {
        KernelInfo* kernel = &g_kernel_registry.kernels[i];
        bool in_group = false;

        for (int g = 0; g < num_groups; g++) {
            if (strstr(kernel->name, groups[g]) != NULL) {
                in_group = true;
                break;
            }
        }

        if (!in_group) {
            if (!other_header_printed) {
                printf("\nOther kernels:\n");
                other_header_printed = true;
            }

            char simd_str[128];
            kernel_registry_simd_features_to_string(kernel->simd_features,
                                                   simd_str, sizeof(simd_str));

            bool available = (kernel->simd_features & g_kernel_registry.hardware_features) != 0 ||
                            kernel->simd_features == LIBETUDE_SIMD_NONE;

            printf("  [%zu] %s\n", i, kernel->name);
            printf("      SIMD: %s\n", simd_str);
            printf("      Optimal size: %zu\n", kernel->optimal_size);
            printf("      Performance score: %.2f\n", kernel->performance_score);
            printf("      Available: %s\n", available ? "Yes" : "No");
        }
    }

    printf("\n================================\n");
}

/**
 * @brief SIMD 기능을 문자열로 변환합니다
 */
static void kernel_registry_simd_features_to_string(uint32_t features, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    bool first = true;

    if (features == LIBETUDE_SIMD_NONE) {
        strncpy(buffer, "None", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return;
    }

    if (features & LIBETUDE_SIMD_SSE) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "SSE", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_SSE2) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "SSE2", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_SSE3) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "SSE3", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_SSSE3) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "SSSE3", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_SSE4_1) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "SSE4.1", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_SSE4_2) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "SSE4.2", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_AVX) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "AVX", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_AVX2) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "AVX2", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (features & LIBETUDE_SIMD_NEON) {
        if (!first) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, "NEON", buffer_size - strlen(buffer) - 1);
        first = false;
    }

    if (first) {
        strncpy(buffer, "Unknown", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
}