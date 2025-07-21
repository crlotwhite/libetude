/**
 * @file kernel_registry.c
 * @brief 커널 레지스트리 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/config.h"
#include "libetude/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_KERNELS 256
#define MAX_KERNEL_NAME_LEN 64

/**
 * @brief SIMD 커널 구조체
 */
typedef struct {
    char name[MAX_KERNEL_NAME_LEN];     /**< 커널 이름 */
    void (*kernel_func)(void* params);  /**< 커널 함수 포인터 */
    uint32_t simd_support;              /**< 지원하는 SIMD 기능 */
    size_t optimal_size;                /**< 최적 데이터 크기 */
    bool active;                        /**< 활성 상태 */
} SIMDKernel;

/**
 * @brief 커널 레지스트리 구조체
 */
typedef struct {
    SIMDKernel kernels[MAX_KERNELS];    /**< 등록된 커널 배열 */
    int kernel_count;                   /**< 등록된 커널 수 */
    uint32_t hardware_features;         /**< 감지된 하드웨어 기능 */
    bool initialized;                   /**< 초기화 상태 */
} KernelRegistry;

// 전역 커널 레지스트리
static KernelRegistry g_kernel_registry = {0};

/**
 * @brief 하드웨어 SIMD 기능을 감지합니다
 */
static uint32_t detect_simd_features() {
    uint32_t features = LIBETUDE_SIMD_NONE;

#if defined(LIBETUDE_PLATFORM_WINDOWS) || defined(LIBETUDE_PLATFORM_LINUX) || defined(LIBETUDE_PLATFORM_MACOS)
    // x86/x64 플랫폼에서 CPUID를 사용한 기능 감지
    #ifdef LIBETUDE_HAVE_SSE
        features |= LIBETUDE_SIMD_SSE;
    #endif
    #ifdef LIBETUDE_HAVE_SSE2
        features |= LIBETUDE_SIMD_SSE2;
    #endif
    #ifdef LIBETUDE_HAVE_SSE3
        features |= LIBETUDE_SIMD_SSE3;
    #endif
    #ifdef LIBETUDE_HAVE_SSSE3
        features |= LIBETUDE_SIMD_SSSE3;
    #endif
    #ifdef LIBETUDE_HAVE_SSE4_1
        features |= LIBETUDE_SIMD_SSE4_1;
    #endif
    #ifdef LIBETUDE_HAVE_SSE4_2
        features |= LIBETUDE_SIMD_SSE4_2;
    #endif
    #ifdef LIBETUDE_HAVE_AVX
        features |= LIBETUDE_SIMD_AVX;
    #endif
    #ifdef LIBETUDE_HAVE_AVX2
        features |= LIBETUDE_SIMD_AVX2;
    #endif
#endif

#if defined(LIBETUDE_PLATFORM_ANDROID) || defined(LIBETUDE_PLATFORM_IOS)
    // ARM 플랫폼에서 NEON 기능 감지
    #ifdef LIBETUDE_HAVE_NEON
        features |= LIBETUDE_SIMD_NEON;
    #endif
#endif

    return features;
}

/**
 * @brief 커널 레지스트리를 초기화합니다
 */
LIBETUDE_API int kernel_registry_init() {
    if (g_kernel_registry.initialized) {
        return LIBETUDE_SUCCESS;
    }

    memset(&g_kernel_registry, 0, sizeof(KernelRegistry));

    // 하드웨어 기능 감지
    g_kernel_registry.hardware_features = detect_simd_features();
    g_kernel_registry.initialized = true;

    // 기본 커널들 등록
    // TODO: 기본 CPU 커널 등록
    // TODO: SIMD 커널 등록 (조건부)
    // TODO: GPU 커널 등록 (조건부)

    return LIBETUDE_SUCCESS;
}

/**
 * @brief 커널 레지스트리를 정리합니다
 */
LIBETUDE_API void kernel_registry_finalize() {
    if (!g_kernel_registry.initialized) {
        return;
    }

    // 등록된 커널들 정리
    memset(&g_kernel_registry, 0, sizeof(KernelRegistry));
}

/**
 * @brief 커널을 등록합니다
 */
LIBETUDE_API int kernel_registry_register(const char* name,
                                         void (*kernel_func)(void*),
                                         uint32_t simd_support,
                                         size_t optimal_size) {
    if (!g_kernel_registry.initialized) {
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (!name || !kernel_func) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (g_kernel_registry.kernel_count >= MAX_KERNELS) {
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    // 중복 이름 확인
    for (int i = 0; i < g_kernel_registry.kernel_count; i++) {
        if (strcmp(g_kernel_registry.kernels[i].name, name) == 0) {
            return LIBETUDE_ERROR_ALREADY_INITIALIZED;
        }
    }

    // 새 커널 등록
    SIMDKernel* kernel = &g_kernel_registry.kernels[g_kernel_registry.kernel_count];
    strncpy(kernel->name, name, MAX_KERNEL_NAME_LEN - 1);
    kernel->name[MAX_KERNEL_NAME_LEN - 1] = '\0';
    kernel->kernel_func = kernel_func;
    kernel->simd_support = simd_support;
    kernel->optimal_size = optimal_size;
    kernel->active = true;

    g_kernel_registry.kernel_count++;

    return LIBETUDE_SUCCESS;
}

/**
 * @brief 최적 커널을 선택합니다
 */
LIBETUDE_API void* kernel_registry_select_optimal(const char* op_name, size_t data_size) {
    if (!g_kernel_registry.initialized || !op_name) {
        return NULL;
    }

    SIMDKernel* best_kernel = NULL;
    int best_score = -1;

    // 등록된 커널들 중에서 최적 커널 선택
    for (int i = 0; i < g_kernel_registry.kernel_count; i++) {
        SIMDKernel* kernel = &g_kernel_registry.kernels[i];

        if (!kernel->active) {
            continue;
        }

        // 이름 매칭 확인 (부분 매칭)
        if (strstr(kernel->name, op_name) == NULL) {
            continue;
        }

        // SIMD 지원 확인
        if ((kernel->simd_support & g_kernel_registry.hardware_features) == 0 &&
            kernel->simd_support != LIBETUDE_SIMD_NONE) {
            continue;
        }

        // 점수 계산 (SIMD 기능이 많을수록 높은 점수)
        int score = 0;
        if (kernel->simd_support & LIBETUDE_SIMD_AVX2) score += 8;
        else if (kernel->simd_support & LIBETUDE_SIMD_AVX) score += 6;
        else if (kernel->simd_support & LIBETUDE_SIMD_SSE4_2) score += 4;
        else if (kernel->simd_support & LIBETUDE_SIMD_SSE2) score += 2;
        else if (kernel->simd_support & LIBETUDE_SIMD_NEON) score += 5;
        else score += 1; // 기본 CPU 커널

        // 데이터 크기 최적화 고려
        if (kernel->optimal_size > 0) {
            if (data_size >= kernel->optimal_size) {
                score += 2;
            } else if (data_size >= kernel->optimal_size / 2) {
                score += 1;
            }
        }

        if (score > best_score) {
            best_score = score;
            best_kernel = kernel;
        }
    }

    return best_kernel ? best_kernel->kernel_func : NULL;
}

/**
 * @brief 감지된 하드웨어 기능을 반환합니다
 */
LIBETUDE_API uint32_t kernel_registry_get_hardware_features() {
    return g_kernel_registry.hardware_features;
}

/**
 * @brief 등록된 커널 수를 반환합니다
 */
LIBETUDE_API int kernel_registry_get_kernel_count() {
    return g_kernel_registry.kernel_count;
}

/**
 * @brief 커널 정보를 출력합니다 (디버그용)
 */
LIBETUDE_API void kernel_registry_print_info() {
    if (!g_kernel_registry.initialized) {
        printf("Kernel registry not initialized\n");
        return;
    }

    printf("=== LibEtude Kernel Registry ===\n");
    printf("Hardware features: 0x%08X\n", g_kernel_registry.hardware_features);
    printf("Registered kernels: %d\n", g_kernel_registry.kernel_count);

    for (int i = 0; i < g_kernel_registry.kernel_count; i++) {
        SIMDKernel* kernel = &g_kernel_registry.kernels[i];
        printf("  [%d] %s (SIMD: 0x%08X, Size: %zu, Active: %s)\n",
               i, kernel->name, kernel->simd_support, kernel->optimal_size,
               kernel->active ? "Yes" : "No");
    }
    printf("================================\n");
}