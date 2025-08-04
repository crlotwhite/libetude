/**
 * @file mobile_power_management.h
 * @brief 모바일 전력 관리 및 배터리 최적화
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 모바일 환경에서의 전력 효율성과 배터리 수명을 최적화하기 위한 고급 기능들을 제공합니다.
 */

#ifndef LIBETUDE_MOBILE_POWER_MANAGEMENT_H
#define LIBETUDE_MOBILE_POWER_MANAGEMENT_H

#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 전력 프로파일 타입
 */
typedef enum {
    POWER_PROFILE_MAXIMUM_PERFORMANCE = 0,  ///< 최대 성능 모드
    POWER_PROFILE_BALANCED = 1,             ///< 균형 모드
    POWER_PROFILE_POWER_SAVER = 2,          ///< 절전 모드
    POWER_PROFILE_ULTRA_POWER_SAVER = 3     ///< 극절전 모드
} PowerProfile;

/**
 * CPU 주파수 스케일링 정책
 */
typedef enum {
    CPU_SCALING_PERFORMANCE = 0,    ///< 성능 우선
    CPU_SCALING_ONDEMAND = 1,       ///< 필요시 스케일링
    CPU_SCALING_CONSERVATIVE = 2,   ///< 보수적 스케일링
    CPU_SCALING_POWERSAVE = 3       ///< 절전 우선
} CPUScalingPolicy;

/**
 * GPU 전력 상태
 */
typedef enum {
    GPU_POWER_HIGH = 0,     ///< 고성능 모드
    GPU_POWER_MEDIUM = 1,   ///< 중간 성능 모드
    GPU_POWER_LOW = 2,      ///< 저성능 모드
    GPU_POWER_OFF = 3       ///< GPU 비활성화
} GPUPowerState;

/**
 * 전력 관리 설정
 */
typedef struct {
    PowerProfile profile;                   ///< 전력 프로파일
    CPUScalingPolicy cpu_scaling;           ///< CPU 스케일링 정책
    GPUPowerState gpu_power_state;          ///< GPU 전력 상태

    // CPU 설정
    float cpu_max_frequency_ratio;          ///< 최대 CPU 주파수 비율 (0.0-1.0)
    int max_active_cores;                   ///< 최대 활성 코어 수
    bool enable_cpu_hotplug;                ///< CPU 핫플러그 활성화

    // 메모리 설정
    bool enable_memory_compression;         ///< 메모리 압축 활성화
    bool enable_swap;                       ///< 스왑 활성화
    size_t memory_pool_size_mb;             ///< 메모리 풀 크기 (MB)

    // 네트워크 설정
    bool enable_network_optimization;       ///< 네트워크 최적화 활성화
    int network_timeout_ms;                 ///< 네트워크 타임아웃 (ms)

    // 디스플레이 설정 (향후 확장용)
    bool reduce_display_updates;            ///< 디스플레이 업데이트 감소

    // 백그라운드 처리 설정
    bool enable_background_processing;      ///< 백그라운드 처리 활성화
    int background_thread_priority;         ///< 백그라운드 스레드 우선순위
} PowerManagementConfig;

/**
 * 전력 사용량 통계
 */
typedef struct {
    // CPU 전력 사용량
    float cpu_power_mw;                     ///< CPU 전력 사용량 (mW)
    float cpu_frequency_mhz;                ///< 현재 CPU 주파수 (MHz)
    int active_cpu_cores;                   ///< 활성 CPU 코어 수

    // GPU 전력 사용량
    float gpu_power_mw;                     ///< GPU 전력 사용량 (mW)
    float gpu_frequency_mhz;                ///< 현재 GPU 주파수 (MHz)
    float gpu_utilization;                  ///< GPU 사용률 (0.0-1.0)

    // 메모리 전력 사용량
    float memory_power_mw;                  ///< 메모리 전력 사용량 (mW)
    size_t memory_bandwidth_mbps;           ///< 메모리 대역폭 (MB/s)

    // 전체 전력 사용량
    float total_power_mw;                   ///< 총 전력 사용량 (mW)
    float estimated_battery_life_hours;     ///< 예상 배터리 수명 (시간)

    // 효율성 지표
    float performance_per_watt;             ///< 와트당 성능
    float energy_efficiency_score;          ///< 에너지 효율성 점수 (0.0-1.0)
} PowerUsageStats;

/**
 * 배터리 상태 정보
 */
typedef struct {
    float capacity_percentage;              ///< 배터리 잔량 (0.0-1.0)
    float voltage_v;                        ///< 배터리 전압 (V)
    float current_ma;                       ///< 배터리 전류 (mA)
    float temperature_c;                    ///< 배터리 온도 (°C)

    bool is_charging;                       ///< 충전 중 여부
    bool is_fast_charging;                  ///< 고속 충전 여부
    bool is_wireless_charging;              ///< 무선 충전 여부
    bool low_power_mode;                    ///< 저전력 모드 여부

    int charge_cycles;                      ///< 충전 사이클 수
    float health_percentage;                ///< 배터리 건강도 (0.0-1.0)

    // 예측 정보
    int estimated_time_to_empty_minutes;    ///< 예상 방전 시간 (분)
    int estimated_time_to_full_minutes;     ///< 예상 충전 완료 시간 (분)
} BatteryStatus;

// ============================================================================
// 전력 관리 함수들
// ============================================================================

/**
 * 전력 관리를 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_management_init();

/**
 * 전력 관리를 종료합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_management_cleanup();

/**
 * 전력 프로파일을 설정합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param profile 전력 프로파일
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_set_profile(void* engine, PowerProfile profile);

/**
 * 현재 전력 프로파일을 가져옵니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param profile 전력 프로파일을 저장할 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_get_profile(void* engine, PowerProfile* profile);

/**
 * 전력 관리 설정을 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param config 전력 관리 설정
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_apply_config(void* engine, const PowerManagementConfig* config);

/**
 * CPU 주파수 스케일링을 설정합니다
 *
 * @param policy 스케일링 정책
 * @param max_frequency_ratio 최대 주파수 비율 (0.0-1.0)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_set_cpu_scaling(CPUScalingPolicy policy, float max_frequency_ratio);

/**
 * GPU 전력 상태를 설정합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param state GPU 전력 상태
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_set_gpu_state(void* engine, GPUPowerState state);

/**
 * 현재 전력 사용량 통계를 가져옵니다
 *
 * @param stats 통계를 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_get_usage_stats(PowerUsageStats* stats);

/**
 * 현재 배터리 상태를 가져옵니다
 *
 * @param status 배터리 상태를 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_get_battery_status(BatteryStatus* status);

/**
 * 배터리 상태에 따른 자동 최적화를 수행합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param battery_status 배터리 상태
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_auto_optimize_for_battery(void* engine, const BatteryStatus* battery_status);

/**
 * 전력 효율성을 최적화합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param target_efficiency 목표 효율성 (0.0-1.0)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_optimize_efficiency(void* engine, float target_efficiency);

/**
 * 백그라운드 모드로 전환합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_enter_background_mode(void* engine);

/**
 * 포그라운드 모드로 전환합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_enter_foreground_mode(void* engine);

/**
 * 전력 관리 통계 리포트를 생성합니다
 *
 * @return 리포트 문자열 (호출자가 해제해야 함)
 */
char* power_generate_report();

// ============================================================================
// 플랫폼별 전력 관리 함수들
// ============================================================================

#ifdef ANDROID_PLATFORM
/**
 * Android Doze 모드 최적화를 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_android_optimize_for_doze(void* engine);

/**
 * Android 앱 대기 모드를 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param is_standby 대기 모드 여부
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_android_handle_app_standby(void* engine, bool is_standby);
#endif

#ifdef IOS_PLATFORM
/**
 * iOS 저전력 모드 최적화를 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param low_power_mode 저전력 모드 여부
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_ios_optimize_for_low_power_mode(void* engine, bool low_power_mode);

/**
 * iOS 백그라운드 앱 새로고침 설정을 처리합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param background_refresh_enabled 백그라운드 새로고침 활성화 여부
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int power_ios_handle_background_refresh(void* engine, bool background_refresh_enabled);
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_MOBILE_POWER_MANAGEMENT_H