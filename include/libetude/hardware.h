/**
 * @file hardware.h
 * @brief 하드웨어 기능 감지 및 정보 수집
 * @author LibEtude Project
 * @version 1.0.0
 *
 * CPU, GPU, 메모리 등 하드웨어 특성을 감지하고 최적화에 필요한 정보를 제공합니다.
 */

#ifndef LIBETUDE_HARDWARE_H
#define LIBETUDE_HARDWARE_H

#include "libetude/config.h"
#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CPU 정보 구조체
// ============================================================================

/**
 * @brief CPU 정보 구조체
 */
typedef struct {
    char vendor[13];                /**< CPU 제조사 (예: "GenuineIntel") */
    char brand[49];                 /**< CPU 브랜드명 */
    uint32_t family;                /**< CPU 패밀리 */
    uint32_t model;                 /**< CPU 모델 */
    uint32_t stepping;              /**< CPU 스테핑 */

    // 코어 및 스레드 정보
    uint32_t physical_cores;        /**< 물리 코어 수 */
    uint32_t logical_cores;         /**< 논리 코어 수 (하이퍼스레딩 포함) */
    uint32_t cache_line_size;       /**< 캐시 라인 크기 (바이트) */

    // 캐시 정보
    uint32_t l1_cache_size;         /**< L1 캐시 크기 (KB) */
    uint32_t l2_cache_size;         /**< L2 캐시 크기 (KB) */
    uint32_t l3_cache_size;         /**< L3 캐시 크기 (KB) */

    // SIMD 지원 기능
    uint32_t simd_features;         /**< 지원하는 SIMD 기능 플래그 */

    // 주파수 정보
    uint32_t base_frequency_mhz;    /**< 기본 주파수 (MHz) */
    uint32_t max_frequency_mhz;     /**< 최대 주파수 (MHz) */
} LibEtudeHardwareCPUInfo;

// ============================================================================
// GPU 정보 구조체
// ============================================================================

/**
 * @brief GPU 정보 구조체
 */
typedef struct {
    LibEtudeGPUBackend backend;     /**< GPU 백엔드 타입 */
    char name[128];                 /**< GPU 이름 */
    char vendor[64];                /**< GPU 제조사 */

    // 메모리 정보
    size_t total_memory;            /**< 총 GPU 메모리 (바이트) */
    size_t available_memory;        /**< 사용 가능한 GPU 메모리 (바이트) */

    // 컴퓨트 유닛 정보
    uint32_t compute_units;         /**< 컴퓨트 유닛 수 */
    uint32_t max_work_group_size;   /**< 최대 워크 그룹 크기 */

    // 성능 정보
    uint32_t core_clock_mhz;        /**< 코어 클럭 (MHz) */
    uint32_t memory_clock_mhz;      /**< 메모리 클럭 (MHz) */

    bool available;                 /**< GPU 사용 가능 여부 */
} LibEtudeHardwareGPUInfo;

// ============================================================================
// 메모리 정보 구조체
// ============================================================================

/**
 * @brief 시스템 메모리 정보 구조체
 */
typedef struct {
    size_t total_physical;          /**< 총 물리 메모리 (바이트) */
    size_t available_physical;      /**< 사용 가능한 물리 메모리 (바이트) */
    size_t total_virtual;           /**< 총 가상 메모리 (바이트) */
    size_t available_virtual;       /**< 사용 가능한 가상 메모리 (바이트) */

    uint32_t page_size;             /**< 메모리 페이지 크기 (바이트) */
    uint32_t allocation_granularity; /**< 메모리 할당 단위 (바이트) */

    // 메모리 대역폭 정보 (추정치)
    uint32_t memory_bandwidth_gbps; /**< 메모리 대역폭 (GB/s) */

    // 메모리 제약 정보
    bool memory_constrained;        /**< 메모리 제약 상태 여부 */
    size_t recommended_pool_size;   /**< 권장 메모리 풀 크기 (바이트) */

    // 프로세스 메모리 사용량
    size_t process_memory_usage;    /**< 현재 프로세스 메모리 사용량 (바이트) */
    size_t process_peak_memory_usage; /**< 최대 프로세스 메모리 사용량 (바이트) */
} LibEtudeHardwareMemoryInfo;

// ============================================================================
// 통합 하드웨어 정보 구조체
// ============================================================================

/**
 * @brief 통합 하드웨어 정보 구조체
 */
typedef struct {
    LibEtudeHardwareCPUInfo cpu;    /**< CPU 정보 */
    LibEtudeHardwareGPUInfo gpu;    /**< GPU 정보 */
    LibEtudeHardwareMemoryInfo memory; /**< 메모리 정보 */

    // 플랫폼 정보
    char platform_name[64];         /**< 플랫폼 이름 */
    char os_version[64];            /**< 운영체제 버전 */

    // 성능 등급 (자동 계산)
    uint32_t performance_tier;      /**< 성능 등급 (1=저성능, 5=고성능) */

    bool initialized;               /**< 초기화 상태 */
} LibEtudeHardwareInfo;

// ============================================================================
// 하드웨어 감지 함수
// ============================================================================

/**
 * @brief 하드웨어 정보를 감지하고 초기화합니다
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_hardware_detect(LibEtudeHardwareInfo* info);

/**
 * @brief CPU 정보를 감지합니다
 * @param cpu_info CPU 정보를 저장할 구조체 포인터
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_hardware_detect_cpu(LibEtudeHardwareCPUInfo* cpu_info);

/**
 * @brief GPU 정보를 감지합니다
 * @param gpu_info GPU 정보를 저장할 구조체 포인터
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_hardware_detect_gpu(LibEtudeHardwareGPUInfo* gpu_info);

/**
 * @brief 메모리 정보를 감지합니다
 * @param memory_info 메모리 정보를 저장할 구조체 포인터
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_hardware_detect_memory(LibEtudeHardwareMemoryInfo* memory_info);

/**
 * @brief SIMD 기능을 감지합니다
 * @return 감지된 SIMD 기능 플래그
 */
LIBETUDE_API uint32_t libetude_hardware_detect_simd_features(void);

/**
 * @brief 성능 등급을 계산합니다
 * @param info 하드웨어 정보 구조체
 * @return 성능 등급 (1-5)
 */
LIBETUDE_API uint32_t libetude_hardware_calculate_performance_tier(const LibEtudeHardwareInfo* info);

// ============================================================================
// 하드웨어 정보 조회 함수
// ============================================================================

/**
 * @brief 최적 스레드 수를 계산합니다
 * @param cpu_info CPU 정보
 * @return 권장 스레드 수
 */
LIBETUDE_API uint32_t libetude_hardware_get_optimal_thread_count(const LibEtudeHardwareCPUInfo* cpu_info);

/**
 * @brief 최적 메모리 풀 크기를 계산합니다
 * @param memory_info 메모리 정보
 * @return 권장 메모리 풀 크기 (바이트)
 */
LIBETUDE_API size_t libetude_hardware_get_optimal_memory_pool_size(const LibEtudeHardwareMemoryInfo* memory_info);

/**
 * @brief GPU 사용 가능 여부를 확인합니다
 * @param gpu_info GPU 정보
 * @return GPU 사용 가능 시 true, 아니면 false
 */
LIBETUDE_API bool libetude_hardware_is_gpu_available(const LibEtudeHardwareGPUInfo* gpu_info);

// ============================================================================
// 디버그 및 정보 출력 함수
// ============================================================================

/**
 * @brief 하드웨어 정보를 출력합니다 (디버그용)
 * @param info 하드웨어 정보 구조체
 */
LIBETUDE_API void libetude_hardware_print_info(const LibEtudeHardwareInfo* info);

/**
 * @brief SIMD 기능 정보를 문자열로 변환합니다
 * @param features SIMD 기능 플래그
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공 시 LIBETUDE_SUCCESS, 실패 시 오류 코드
 */
LIBETUDE_API LibEtudeErrorCode libetude_hardware_simd_features_to_string(uint32_t features,
                                                                        char* buffer,
                                                                        size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_HARDWARE_H