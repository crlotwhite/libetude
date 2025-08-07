/**
 * @file runtime_adaptation.h
 * @brief 런타임 기능 감지 및 적응 시스템
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 하드웨어 기능 런타임 감지, 동적 함수 디스패치,
 * 성능 프로파일링, 열 관리 및 전력 관리 통합 시스템을 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_RUNTIME_ADAPTATION_H
#define LIBETUDE_PLATFORM_RUNTIME_ADAPTATION_H

#include "libetude/config.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 하드웨어 기능 감지 시스템
// ============================================================================

/**
 * @brief 하드웨어 기능 감지 결과 구조체
 */
typedef struct {
    // CPU 기능
    bool has_sse;
    bool has_sse2;
    bool has_sse3;
    bool has_ssse3;
    bool has_sse4_1;
    bool has_sse4_2;
    bool has_avx;
    bool has_avx2;
    bool has_avx512;
    bool has_fma;
    bool has_neon;

    // GPU 기능
    bool has_cuda;
    bool has_opencl;
    bool has_metal;
    bool has_vulkan;

    // 오디오 하드웨어 가속
    bool has_audio_hw_acceleration;

    // 기타 기능
    bool has_high_res_timer;
    bool has_rdtsc;
    bool has_thermal_sensors;
    bool has_power_management;

    // 캐시 정보
    uint32_t l1_cache_size;
    uint32_t l2_cache_size;
    uint32_t l3_cache_size;
    uint32_t cache_line_size;

    // CPU 정보
    uint32_t cpu_count;
    uint32_t physical_cpu_count;
    uint32_t cpu_frequency_mhz;
    char cpu_vendor[32];
    char cpu_brand[64];

    // 메모리 정보
    uint64_t total_memory;
    uint64_t available_memory;
    uint32_t memory_bandwidth_gbps;

    // 감지 시간 (캐싱용)
    uint64_t detection_timestamp;
    bool is_cached;
} ETHardwareCapabilities;

/**
 * @brief 하드웨어 기능을 감지하고 캐싱합니다
 * @param capabilities 감지된 기능을 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_detect_hardware_capabilities(ETHardwareCapabilities* capabilities);

/**
 * @brief 캐시된 하드웨어 기능 정보를 가져옵니다
 * @param capabilities 기능 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_cached_hardware_capabilities(ETHardwareCapabilities* capabilities);

/**
 * @brief 하드웨어 기능 캐시를 무효화합니다
 */
void et_invalidate_hardware_cache(void);

/**
 * @brief 특정 하드웨어 기능의 지원 여부를 확인합니다
 * @param feature 확인할 하드웨어 기능
 * @return 지원하면 true, 아니면 false
 */
bool et_runtime_has_feature(ETHardwareFeature feature);

// ============================================================================
// 동적 함수 디스패치 시스템
// ============================================================================

/**
 * @brief 함수 포인터 타입 정의
 */
typedef void* (*ETGenericFunction)(void);

/**
 * @brief 동적 디스패치 테이블 엔트리
 */
typedef struct {
    const char* function_name;          /**< 함수 이름 */
    ETGenericFunction generic_impl;     /**< 기본 구현 */
    ETGenericFunction sse_impl;         /**< SSE 최적화 구현 */
    ETGenericFunction sse2_impl;        /**< SSE2 최적화 구현 */
    ETGenericFunction avx_impl;         /**< AVX 최적화 구현 */
    ETGenericFunction avx2_impl;        /**< AVX2 최적화 구현 */
    ETGenericFunction neon_impl;        /**< NEON 최적화 구현 */
    ETGenericFunction gpu_impl;         /**< GPU 가속 구현 */
    ETGenericFunction selected_impl;    /**< 선택된 구현 (캐시됨) */
    uint32_t required_features;         /**< 필요한 하드웨어 기능 플래그 */
} ETDispatchEntry;

/**
 * @brief 동적 디스패치 테이블
 */
typedef struct {
    ETDispatchEntry* entries;           /**< 디스패치 엔트리 배열 */
    size_t entry_count;                 /**< 엔트리 개수 */
    bool is_initialized;                /**< 초기화 여부 */
} ETDispatchTable;

/**
 * @brief 동적 디스패치 시스템을 초기화합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dispatch_initialize(void);

/**
 * @brief 함수를 동적 디스패치 테이블에 등록합니다
 * @param name 함수 이름
 * @param entry 디스패치 엔트리
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dispatch_register_function(const char* name, const ETDispatchEntry* entry);

/**
 * @brief 최적화된 함수 구현을 선택합니다
 * @param name 함수 이름
 * @return 선택된 함수 포인터, 실패시 NULL
 */
ETGenericFunction et_dispatch_select_function(const char* name);

/**
 * @brief 모든 등록된 함수의 최적화된 구현을 선택합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dispatch_select_all_functions(void);

/**
 * @brief 동적 디스패치 시스템을 정리합니다
 */
void et_dispatch_finalize(void);

// ============================================================================
// 성능 프로파일링 시스템
// ============================================================================

/**
 * @brief 성능 측정 데이터 구조체
 */
typedef struct {
    const char* name;                   /**< 측정 항목 이름 */
    uint64_t total_time_ns;             /**< 총 실행 시간 (나노초) */
    uint64_t min_time_ns;               /**< 최소 실행 시간 */
    uint64_t max_time_ns;               /**< 최대 실행 시간 */
    uint64_t call_count;                /**< 호출 횟수 */
    double average_time_ns;             /**< 평균 실행 시간 */
    double cpu_usage_percent;           /**< CPU 사용률 */
    uint64_t memory_usage_bytes;        /**< 메모리 사용량 */
    uint64_t cache_misses;              /**< 캐시 미스 횟수 */
    uint64_t branch_mispredictions;     /**< 분기 예측 실패 횟수 */
} ETPerformanceMetrics;

/**
 * @brief 적응적 최적화 설정 구조체
 */
typedef struct {
    bool enable_auto_optimization;      /**< 자동 최적화 활성화 */
    uint32_t optimization_interval_ms;  /**< 최적화 간격 (밀리초) */
    double cpu_threshold_percent;       /**< CPU 사용률 임계값 */
    double memory_threshold_percent;    /**< 메모리 사용률 임계값 */
    double latency_threshold_ms;        /**< 지연시간 임계값 */
    uint32_t sample_window_size;        /**< 샘플 윈도우 크기 */
} ETAdaptiveOptimizationConfig;

/**
 * @brief 성능 프로파일링을 시작합니다
 * @param name 프로파일링 항목 이름
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_profiling_begin(const char* name);

/**
 * @brief 성능 프로파일링을 종료합니다
 * @param name 프로파일링 항목 이름
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_profiling_end(const char* name);

/**
 * @brief 성능 메트릭을 가져옵니다
 * @param name 항목 이름
 * @param metrics 메트릭을 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_profiling_get_metrics(const char* name, ETPerformanceMetrics* metrics);

/**
 * @brief 모든 성능 메트릭을 초기화합니다
 */
void et_profiling_reset_all_metrics(void);

/**
 * @brief 적응적 최적화를 시작합니다
 * @param config 최적화 설정
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_adaptive_optimization_start(const ETAdaptiveOptimizationConfig* config);

/**
 * @brief 적응적 최적화를 중지합니다
 */
void et_adaptive_optimization_stop(void);

/**
 * @brief 현재 성능 상태를 분석하고 최적화를 수행합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_adaptive_optimization_update(void);

// ============================================================================
// 열 관리 시스템
// ============================================================================

/**
 * @brief 온도 센서 타입
 */
typedef enum {
    ET_TEMP_SENSOR_CPU = 0,             /**< CPU 온도 센서 */
    ET_TEMP_SENSOR_GPU = 1,             /**< GPU 온도 센서 */
    ET_TEMP_SENSOR_SYSTEM = 2,          /**< 시스템 온도 센서 */
    ET_TEMP_SENSOR_BATTERY = 3,         /**< 배터리 온도 센서 */
    ET_TEMP_SENSOR_COUNT                /**< 센서 개수 */
} ETTemperatureSensorType;

/**
 * @brief 온도 정보 구조체
 */
typedef struct {
    float current_temp_celsius;         /**< 현재 온도 (섭씨) */
    float max_temp_celsius;             /**< 최대 온도 */
    float critical_temp_celsius;        /**< 임계 온도 */
    bool is_overheating;                /**< 과열 상태 */
    bool is_throttling;                 /**< 스로틀링 상태 */
    uint64_t timestamp;                 /**< 측정 시간 */
} ETTemperatureInfo;

/**
 * @brief 열 관리 설정 구조체
 */
typedef struct {
    float warning_temp_celsius;         /**< 경고 온도 */
    float critical_temp_celsius;        /**< 임계 온도 */
    uint32_t monitoring_interval_ms;    /**< 모니터링 간격 */
    bool enable_auto_throttling;        /**< 자동 스로틀링 활성화 */
    bool enable_emergency_shutdown;     /**< 비상 종료 활성화 */
} ETThermalManagementConfig;

/**
 * @brief 온도를 측정합니다
 * @param sensor_type 센서 타입
 * @param temp_info 온도 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_thermal_get_temperature(ETTemperatureSensorType sensor_type, ETTemperatureInfo* temp_info);

/**
 * @brief 열 관리 시스템을 시작합니다
 * @param config 열 관리 설정
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_thermal_management_start(const ETThermalManagementConfig* config);

/**
 * @brief 열 관리 시스템을 중지합니다
 */
void et_thermal_management_stop(void);

/**
 * @brief 현재 열 상태를 확인하고 필요시 조치를 취합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_thermal_management_update(void);

// ============================================================================
// 전력 관리 시스템
// ============================================================================

/**
 * @brief 전력 상태 열거형
 */
typedef enum {
    ET_POWER_STATE_HIGH_PERFORMANCE = 0, /**< 고성능 모드 */
    ET_POWER_STATE_BALANCED = 1,         /**< 균형 모드 */
    ET_POWER_STATE_POWER_SAVER = 2,      /**< 절전 모드 */
    ET_POWER_STATE_ULTRA_LOW_POWER = 3   /**< 초절전 모드 */
} ETPowerState;

/**
 * @brief 전력 정보 구조체
 */
typedef struct {
    float current_power_watts;          /**< 현재 전력 소비 (와트) */
    float average_power_watts;          /**< 평균 전력 소비 */
    float battery_level_percent;        /**< 배터리 잔량 (%) */
    bool is_charging;                   /**< 충전 중 여부 */
    bool is_low_battery;                /**< 배터리 부족 상태 */
    uint32_t estimated_runtime_minutes; /**< 예상 사용 시간 (분) */
    ETPowerState current_state;         /**< 현재 전력 상태 */
    uint64_t timestamp;                 /**< 측정 시간 */
} ETPowerInfo;

/**
 * @brief 전력 관리 설정 구조체
 */
typedef struct {
    ETPowerState default_state;         /**< 기본 전력 상태 */
    float low_battery_threshold;        /**< 배터리 부족 임계값 (%) */
    float critical_battery_threshold;   /**< 배터리 위험 임계값 (%) */
    uint32_t monitoring_interval_ms;    /**< 모니터링 간격 */
    bool enable_auto_power_management;  /**< 자동 전력 관리 활성화 */
    bool enable_cpu_scaling;            /**< CPU 주파수 스케일링 활성화 */
    bool enable_gpu_power_management;   /**< GPU 전력 관리 활성화 */
} ETPowerManagementConfig;

/**
 * @brief 현재 전력 정보를 가져옵니다
 * @param power_info 전력 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_power_get_info(ETPowerInfo* power_info);

/**
 * @brief 전력 상태를 설정합니다
 * @param state 설정할 전력 상태
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_power_set_state(ETPowerState state);

/**
 * @brief 전력 관리 시스템을 시작합니다
 * @param config 전력 관리 설정
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_power_management_start(const ETPowerManagementConfig* config);

/**
 * @brief 전력 관리 시스템을 중지합니다
 */
void et_power_management_stop(void);

/**
 * @brief 현재 전력 상태를 확인하고 필요시 조치를 취합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_power_management_update(void);

// ============================================================================
// 통합 런타임 적응 시스템
// ============================================================================

/**
 * @brief 런타임 적응 시스템 설정 구조체
 */
typedef struct {
    ETAdaptiveOptimizationConfig optimization_config;
    ETThermalManagementConfig thermal_config;
    ETPowerManagementConfig power_config;

    bool enable_hardware_monitoring;    /**< 하드웨어 모니터링 활성화 */
    bool enable_performance_profiling;  /**< 성능 프로파일링 활성화 */
    bool enable_thermal_management;     /**< 열 관리 활성화 */
    bool enable_power_management;       /**< 전력 관리 활성화 */

    uint32_t update_interval_ms;        /**< 업데이트 간격 */
    uint32_t cache_validity_ms;         /**< 캐시 유효 시간 */
} ETRuntimeAdaptationConfig;

/**
 * @brief 런타임 적응 시스템을 초기화합니다
 * @param config 시스템 설정
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_runtime_adaptation_initialize(const ETRuntimeAdaptationConfig* config);

/**
 * @brief 런타임 적응 시스템을 시작합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_runtime_adaptation_start(void);

/**
 * @brief 런타임 적응 시스템을 업데이트합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_runtime_adaptation_update(void);

/**
 * @brief 런타임 적응 시스템을 중지합니다
 */
void et_runtime_adaptation_stop(void);

/**
 * @brief 런타임 적응 시스템을 정리합니다
 */
void et_runtime_adaptation_finalize(void);

/**
 * @brief 현재 시스템 상태 요약을 가져옵니다
 * @param buffer 상태 정보를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_runtime_adaptation_get_status(char* buffer, size_t buffer_size);

// ============================================================================
// 유틸리티 매크로
// ============================================================================

/**
 * @brief 런타임 기능 감지 매크로
 */
#define ET_RUNTIME_CHECK_FEATURE(feature) et_runtime_has_feature(feature)

/**
 * @brief 동적 디스패치 매크로
 */
#define ET_DISPATCH_CALL(name, ...) \
    do { \
        ETGenericFunction func = et_dispatch_select_function(name); \
        if (func) { \
            ((typeof(func))func)(__VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief 성능 측정 스코프 매크로
 */
#define ET_PROFILE_RUNTIME_SCOPE(name) \
    et_profiling_begin(name); \
    struct ETProfileCleanup { \
        const char* profile_name; \
        ~ETProfileCleanup() { et_profiling_end(profile_name); } \
    } _profile_cleanup = {name}

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_RUNTIME_ADAPTATION_H