/**
 * @file mobile_optimization.h
 * @brief 모바일 플랫폼 최적화 유틸리티
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 모바일 환경에서의 리소스 관리 및 성능 최적화를 위한 공통 유틸리티입니다.
 */

#ifndef LIBETUDE_MOBILE_OPTIMIZATION_H
#define LIBETUDE_MOBILE_OPTIMIZATION_H

#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 모바일 플랫폼 타입
 */
typedef enum {
    MOBILE_PLATFORM_UNKNOWN = 0,
    MOBILE_PLATFORM_ANDROID = 1,
    MOBILE_PLATFORM_IOS = 2
} MobilePlatform;

/**
 * 모바일 디바이스 클래스
 */
typedef enum {
    MOBILE_DEVICE_LOW_END = 0,    ///< 저사양 디바이스
    MOBILE_DEVICE_MID_RANGE = 1,  ///< 중급 디바이스
    MOBILE_DEVICE_HIGH_END = 2    ///< 고사양 디바이스
} MobileDeviceClass;

/**
 * 모바일 최적화 설정
 */
typedef struct {
    MobilePlatform platform;           ///< 플랫폼 타입
    MobileDeviceClass device_class;    ///< 디바이스 클래스

    // 메모리 설정
    size_t memory_limit_mb;            ///< 메모리 제한 (MB)
    bool enable_memory_pressure_handling; ///< 메모리 압박 처리 활성화
    float memory_warning_threshold;    ///< 메모리 경고 임계치 (0.0-1.0)

    // CPU 설정
    int max_threads;                   ///< 최대 스레드 수
    bool enable_thermal_throttling;    ///< 열 제한 활성화
    float cpu_usage_limit;             ///< CPU 사용률 제한 (0.0-1.0)

    // 배터리 설정
    bool battery_optimized;            ///< 배터리 최적화 모드
    bool disable_gpu_on_battery;       ///< 배터리 모드에서 GPU 비활성화

    // 품질 설정
    bool adaptive_quality;             ///< 적응형 품질 조정
    int min_quality_level;             ///< 최소 품질 레벨
    int max_quality_level;             ///< 최대 품질 레벨

    // 네트워크 설정 (향후 확장용)
    bool enable_model_streaming;       ///< 모델 스트리밍 활성화
    size_t cache_size_mb;              ///< 캐시 크기 (MB)
} MobileOptimizationConfig;

/**
 * 모바일 리소스 상태
 */
typedef struct {
    // 메모리 상태
    size_t memory_used_mb;             ///< 사용 중인 메모리 (MB)
    size_t memory_available_mb;        ///< 사용 가능한 메모리 (MB)
    float memory_pressure;             ///< 메모리 압박 수준 (0.0-1.0)

    // CPU 상태
    float cpu_usage;                   ///< CPU 사용률 (0.0-1.0)
    float cpu_temperature;             ///< CPU 온도 (섭씨)
    bool thermal_throttling_active;    ///< 열 제한 활성화 여부

    // 배터리 상태
    float battery_level;               ///< 배터리 잔량 (0.0-1.0)
    bool is_charging;                  ///< 충전 중 여부
    bool low_power_mode;               ///< 저전력 모드 여부

    // 네트워크 상태
    bool network_available;            ///< 네트워크 사용 가능 여부
    bool wifi_connected;               ///< WiFi 연결 여부
    bool cellular_connected;           ///< 셀룰러 연결 여부
} MobileResourceStatus;

/**
 * 모바일 최적화 콜백 타입
 */
typedef void (*MobileOptimizationCallback)(const MobileResourceStatus* status, void* user_data);

// ============================================================================
// 모바일 최적화 함수들
// ============================================================================

/**
 * 현재 플랫폼을 감지합니다
 *
 * @return 플랫폼 타입
 */
MobilePlatform mobile_detect_platform();

/**
 * 디바이스 클래스를 감지합니다
 *
 * @return 디바이스 클래스
 */
MobileDeviceClass mobile_detect_device_class();

/**
 * 기본 모바일 최적화 설정을 생성합니다
 *
 * @param platform 플랫폼 타입
 * @param device_class 디바이스 클래스
 * @return 최적화 설정
 */
MobileOptimizationConfig mobile_create_default_config(MobilePlatform platform, MobileDeviceClass device_class);

/**
 * 현재 리소스 상태를 가져옵니다
 *
 * @param status 상태를 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_get_resource_status(MobileResourceStatus* status);

/**
 * 메모리 압박 상황을 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param pressure_level 압박 수준 (0.0-1.0)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_handle_memory_pressure(LibEtudeEngine* engine, float pressure_level);

/**
 * 열 제한을 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param temperature CPU 온도 (섭씨)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_handle_thermal_throttling(LibEtudeEngine* engine, float temperature);

/**
 * 배터리 상태에 따른 최적화를 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param battery_level 배터리 잔량 (0.0-1.0)
 * @param is_charging 충전 중 여부
 * @param low_power_mode 저전력 모드 여부
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_optimize_for_battery(LibEtudeEngine* engine, float battery_level, bool is_charging, bool low_power_mode);

/**
 * 적응형 품질 조정을 수행합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param status 현재 리소스 상태
 * @param config 최적화 설정
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_adaptive_quality_adjustment(LibEtudeEngine* engine, const MobileResourceStatus* status, const MobileOptimizationConfig* config);

/**
 * 리소스 모니터링을 시작합니다
 *
 * @param callback 상태 변경 콜백
 * @param user_data 사용자 데이터
 * @param interval_ms 모니터링 간격 (밀리초)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_start_resource_monitoring(MobileOptimizationCallback callback, void* user_data, int interval_ms);

/**
 * 리소스 모니터링을 중지합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_stop_resource_monitoring();

/**
 * 모바일 최적화 통계를 가져옵니다
 *
 * @return 통계 문자열 (호출자가 해제해야 함)
 */
char* mobile_get_optimization_stats();

// ============================================================================
// 플랫폼별 특화 함수들
// ============================================================================

#ifdef ANDROID_PLATFORM
/**
 * Android 특화 최적화를 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param config 최적화 설정
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_android_apply_optimizations(LibEtudeEngine* engine, const MobileOptimizationConfig* config);

/**
 * Android 메모리 압박 신호를 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param trim_level Android trim level
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_android_handle_trim_memory(LibEtudeEngine* engine, int trim_level);
#endif

#ifdef IOS_PLATFORM
/**
 * iOS 특화 최적화를 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param config 최적화 설정
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_ios_apply_optimizations(LibEtudeEngine* engine, const MobileOptimizationConfig* config);

/**
 * iOS 메모리 경고를 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param warning_level 경고 레벨
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int mobile_ios_handle_memory_warning(LibEtudeEngine* engine, int warning_level);
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_MOBILE_OPTIMIZATION_H