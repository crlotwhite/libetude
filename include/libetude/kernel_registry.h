/**
 * @file kernel_registry.h
 * @brief SIMD 커널 레지스트리 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 하드웨어별 최적화된 커널을 등록하고 선택하는 시스템을 제공합니다.
 */

#ifndef LIBETUDE_KERNEL_REGISTRY_H
#define LIBETUDE_KERNEL_REGISTRY_H

#include "libetude/config.h"
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 커널 함수 타입 정의
// ============================================================================

/**
 * @brief 벡터 덧셈 커널 함수 타입
 */
typedef void (*VectorAddKernel)(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 벡터 곱셈 커널 함수 타입
 */
typedef void (*VectorMulKernel)(const float* a, const float* b, float* result, size_t size);

/**
 * @brief 행렬 곱셈 커널 함수 타입
 */
typedef void (*MatMulKernel)(const float* a, const float* b, float* result,
                            size_t m, size_t n, size_t k);

/**
 * @brief 활성화 함수 커널 타입
 */
typedef void (*ActivationKernel)(const float* input, float* output, size_t size);

// ============================================================================
// 커널 정보 구조체
// ============================================================================

/**
 * @brief 커널 정보 구조체
 */
typedef struct {
    const char* name;               /**< 커널 이름 */
    uint32_t simd_features;         /**< 필요한 SIMD 기능 */
    size_t optimal_size;            /**< 최적 데이터 크기 */
    void* kernel_func;              /**< 커널 함수 포인터 */
    float performance_score;        /**< 성능 점수 (벤치마크 결과) */
} KernelInfo;

/**
 * @brief 커널 레지스트리 구조체
 */
typedef struct {
    KernelInfo* kernels;            /**< 등록된 커널 배열 */
    size_t kernel_count;            /**< 등록된 커널 수 */
    size_t capacity;                /**< 배열 용량 */
    uint32_t hardware_features;     /**< 현재 하드웨어 기능 */
    bool initialized;               /**< 초기화 상태 */
} KernelRegistry;

// ============================================================================
// 커널 레지스트리 함수
// ============================================================================

/**
 * @brief 커널 레지스트리를 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode kernel_registry_init(void);

/**
 * @brief 커널 레지스트리를 정리합니다
 */
LIBETUDE_API void kernel_registry_finalize(void);

/**
 * @brief 커널을 레지스트리에 등록합니다
 *
 * @param kernel_info 등록할 커널 정보
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode kernel_registry_register(const KernelInfo* kernel_info);

/**
 * @brief 최적의 커널을 선택합니다
 *
 * @param kernel_name 커널 이름
 * @param data_size 데이터 크기
 * @return 선택된 커널 함수 포인터, 없으면 NULL
 */
LIBETUDE_API void* kernel_registry_select_optimal(const char* kernel_name, size_t data_size);

/**
 * @brief 현재 하드웨어 기능을 반환합니다
 *
 * @return 하드웨어 기능 플래그
 */
LIBETUDE_API uint32_t kernel_registry_get_hardware_features(void);

/**
 * @brief 등록된 커널 수를 반환합니다
 *
 * @return 커널 수
 */
LIBETUDE_API size_t kernel_registry_get_kernel_count(void);

/**
 * @brief 커널 레지스트리 정보를 출력합니다 (디버그용)
 */
LIBETUDE_API void kernel_registry_print_info(void);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_KERNEL_REGISTRY_H