/**
 * @file system.h
 * @brief 시스템 정보 추상화 레이어
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 시스템 정보 수집과 하드웨어 기능 감지를 위한 추상화 인터페이스를 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_SYSTEM_H
#define LIBETUDE_PLATFORM_SYSTEM_H

#include "libetude/platform/common.h"
#include "libetude/types.h"
#include "libetude/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 시스템 정보 데이터 구조
// ============================================================================

/**
 * @brief 시스템 정보 구조체
 */
typedef struct {
    uint64_t total_memory;         /**< 총 메모리 크기 (바이트) */
    uint64_t available_memory;     /**< 사용 가능한 메모리 크기 (바이트) */
    uint32_t cpu_count;            /**< CPU 코어 수 */
    uint32_t cpu_frequency;        /**< CPU 주파수 (MHz) */
    char cpu_name[128];            /**< CPU 이름 */
    char system_name[64];          /**< 시스템 이름 */
    char os_version[32];           /**< 운영체제 버전 */
    ETPlatformType platform_type;  /**< 플랫폼 타입 */
    ETArchitecture architecture;   /**< 아키텍처 */
} ETSystemInfo;

/**
 * @brief 메모리 정보 구조체
 */
typedef struct {
    uint64_t total_physical;       /**< 총 물리 메모리 (바이트) */
    uint64_t available_physical;   /**< 사용 가능한 물리 메모리 (바이트) */
    uint64_t total_virtual;        /**< 총 가상 메모리 (바이트) */
    uint64_t available_virtual;    /**< 사용 가능한 가상 메모리 (바이트) */
    uint32_t page_size;            /**< 메모리 페이지 크기 (바이트) */
    uint32_t allocation_granularity; /**< 메모리 할당 단위 (바이트) */
} ETMemoryInfo;

/**
 * @brief CPU 정보 구조체
 */
typedef struct {
    char vendor[13];               /**< CPU 제조사 */
    char brand[49];                /**< CPU 브랜드명 */
    uint32_t family;               /**< CPU 패밀리 */
    uint32_t model;                /**< CPU 모델 */
    uint32_t stepping;             /**< CPU 스테핑 */
    uint32_t physical_cores;       /**< 물리 코어 수 */
    uint32_t logical_cores;        /**< 논리 코어 수 */
    uint32_t cache_line_size;      /**< 캐시 라인 크기 (바이트) */
    uint32_t l1_cache_size;        /**< L1 캐시 크기 (KB) */
    uint32_t l2_cache_size;        /**< L2 캐시 크기 (KB) */
    uint32_t l3_cache_size;        /**< L3 캐시 크기 (KB) */
    uint32_t base_frequency_mhz;   /**< 기본 주파수 (MHz) */
    uint32_t max_frequency_mhz;    /**< 최대 주파수 (MHz) */
} ETCPUInfo;

/**
 * @brief 메모리 사용량 구조체
 */
typedef struct {
    uint64_t process_memory_usage;     /**< 현재 프로세스 메모리 사용량 (바이트) */
    uint64_t process_peak_memory;      /**< 최대 프로세스 메모리 사용량 (바이트) */
    float cpu_usage_percent;           /**< CPU 사용률 (%) */
    float memory_usage_percent;        /**< 메모리 사용률 (%) */
} ETMemoryUsage;

/**
 * @brief SIMD 기능 플래그
 */
typedef enum {
    ET_SIMD_NONE = 0,              /**< SIMD 지원 없음 */
    ET_SIMD_SSE = 1 << 0,          /**< SSE 지원 */
    ET_SIMD_SSE2 = 1 << 1,         /**< SSE2 지원 */
    ET_SIMD_SSE3 = 1 << 2,         /**< SSE3 지원 */
    ET_SIMD_SSSE3 = 1 << 3,        /**< SSSE3 지원 */
    ET_SIMD_SSE4_1 = 1 << 4,       /**< SSE4.1 지원 */
    ET_SIMD_SSE4_2 = 1 << 5,       /**< SSE4.2 지원 */
    ET_SIMD_AVX = 1 << 6,          /**< AVX 지원 */
    ET_SIMD_AVX2 = 1 << 7,         /**< AVX2 지원 */
    ET_SIMD_AVX512 = 1 << 8,       /**< AVX-512 지원 */
    ET_SIMD_NEON = 1 << 9,         /**< ARM NEON 지원 */
    ET_SIMD_FMA = 1 << 10          /**< FMA 지원 */
} ETSIMDFeatures;

// ============================================================================
// 시스템 인터페이스 구조체
// ============================================================================

/**
 * @brief 시스템 정보 추상화 인터페이스
 */
typedef struct ETSystemInterface {
    // 시스템 정보 수집
    ETResult (*get_system_info)(ETSystemInfo* info);
    ETResult (*get_memory_info)(ETMemoryInfo* info);
    ETResult (*get_cpu_info)(ETCPUInfo* info);

    // 고해상도 타이머
    ETResult (*get_high_resolution_time)(uint64_t* time_ns);
    ETResult (*sleep)(uint32_t milliseconds);
    ETResult (*get_timer_frequency)(uint64_t* frequency);

    // 하드웨어 기능 감지
    uint32_t (*get_simd_features)(void);
    bool (*has_feature)(ETHardwareFeature feature);
    ETResult (*detect_hardware_capabilities)(uint32_t* capabilities);

    // 성능 모니터링
    ETResult (*get_cpu_usage)(float* usage_percent);
    ETResult (*get_memory_usage)(ETMemoryUsage* usage);
    ETResult (*get_process_memory_info)(uint64_t* current_usage, uint64_t* peak_usage);

    // 시스템 상태
    ETResult (*get_system_uptime)(uint64_t* uptime_seconds);
    ETResult (*get_process_uptime)(uint64_t* uptime_seconds);

    // 플랫폼별 확장 데이터
    void* platform_data;
} ETSystemInterface;

// ============================================================================
// 공통 함수 선언
// ============================================================================

/**
 * @brief 시스템 인터페이스를 생성합니다
 * @param interface 생성된 인터페이스를 저장할 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_system_interface_create(ETSystemInterface** interface);

/**
 * @brief 시스템 인터페이스를 해제합니다
 * @param interface 해제할 인터페이스
 */
void et_system_interface_destroy(ETSystemInterface* interface);

/**
 * @brief 현재 플랫폼에 맞는 시스템 인터페이스를 가져옵니다
 * @return 시스템 인터페이스 포인터
 */
const ETSystemInterface* et_get_system_interface(void);

/**
 * @brief 시스템 정보를 가져옵니다
 * @param info 시스템 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_system_info(ETSystemInfo* info);

/**
 * @brief 메모리 정보를 가져옵니다
 * @param info 메모리 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_memory_info(ETMemoryInfo* info);

/**
 * @brief CPU 정보를 가져옵니다
 * @param info CPU 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_cpu_info(ETCPUInfo* info);

/**
 * @brief 고해상도 시간을 가져옵니다
 * @param time_ns 나노초 단위 시간을 저장할 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_high_resolution_time(uint64_t* time_ns);

/**
 * @brief 지정된 시간만큼 대기합니다
 * @param milliseconds 대기할 시간 (밀리초)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_sleep(uint32_t milliseconds);

/**
 * @brief SIMD 기능을 감지합니다
 * @return 감지된 SIMD 기능 플래그
 */
uint32_t et_get_simd_features(void);

/**
 * @brief 하드웨어 기능 지원 여부를 확인합니다
 * @param feature 확인할 하드웨어 기능
 * @return 지원하면 true, 아니면 false
 */
bool et_has_hardware_feature(ETHardwareFeature feature);

/**
 * @brief CPU 사용률을 가져옵니다
 * @param usage_percent CPU 사용률을 저장할 포인터 (%)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_cpu_usage(float* usage_percent);

/**
 * @brief 메모리 사용량을 가져옵니다
 * @param usage 메모리 사용량을 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_memory_usage(ETMemoryUsage* usage);

// ============================================================================
// 유틸리티 함수
// ============================================================================

/**
 * @brief SIMD 기능을 문자열로 변환합니다
 * @param features SIMD 기능 플래그
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_simd_features_to_string(uint32_t features, char* buffer, size_t buffer_size);

/**
 * @brief 시스템 정보를 출력합니다 (디버그용)
 * @param info 시스템 정보 구조체
 */
void et_print_system_info(const ETSystemInfo* info);

/**
 * @brief 메모리 정보를 출력합니다 (디버그용)
 * @param info 메모리 정보 구조체
 */
void et_print_memory_info(const ETMemoryInfo* info);

/**
 * @brief CPU 정보를 출력합니다 (디버그용)
 * @param info CPU 정보 구조체
 */
void et_print_cpu_info(const ETCPUInfo* info);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_SYSTEM_H