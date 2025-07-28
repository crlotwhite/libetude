/**
 * @file thermal_management.h
 * @brief 모바일 열 관리 시스템
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 모바일 환경에서의 열 관리와 온도 기반 성능 조절을 위한 시스템입니다.
 */

#ifndef LIBETUDE_THERMAL_MANAGEMENT_H
#define LIBETUDE_THERMAL_MANAGEMENT_H

#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 열 상태 레벨
 */
typedef enum {
    THERMAL_STATE_NORMAL = 0,       ///< 정상 온도
    THERMAL_STATE_WARM = 1,         ///< 따뜻한 상태
    THERMAL_STATE_HOT = 2,          ///< 뜨거운 상태
    THERMAL_STATE_CRITICAL = 3      ///< 임계 온도
} ThermalState;

/**
 * 열 제한 정책
 */
typedef enum {
    THERMAL_POLICY_NONE = 0,        ///< 제한 없음
    THERMAL_POLICY_CONSERVATIVE = 1, ///< 보수적 제한
    THERMAL_POLICY_AGGRESSIVE = 2   ///< 적극적 제한
} ThermalPolicy;

/**
 * 온도 센서 타입
 */
typedef enum {
    TEMP_SENSOR_CPU = 0,            ///< CPU 온도 센서
    TEMP_SENSOR_GPU = 1,            ///< GPU 온도 센서
    TEMP_SENSOR_BATTERY = 2,        ///< 배터리 온도 센서
    TEMP_SENSOR_AMBIENT = 3,        ///< 주변 온도 센서
    TEMP_SENSOR_SKIN = 4            ///< 표면 온도 센서
} TempSensorType;

/**
 * 온도 임계값 설정
 */
typedef struct {
    float normal_threshold_c;       ///< 정상 온도 임계값 (°C)
    float warm_threshold_c;         ///< 따뜻한 상태 임계값 (°C)
    float hot_threshold_c;          ///< 뜨거운 상태 임계값 (°C)
    float critical_threshold_c;     ///< 임계 온도 임계값 (°C)
    float hysteresis_c;             ///< 히스테리시스 온도 (°C)
} ThermalThresholds;

/**
 * 열 관리 설정
 */
typedef struct {
    ThermalPolicy policy;           ///< 열 제한 정책
    ThermalThresholds thresholds;   ///< 온도 임계값

    // 모니터링 설정
    int monitoring_interval_ms;     ///< 모니터링 간격 (ms)
    bool enable_predictive_throttling; ///< 예측적 제한 활성화

    // 제한 설정
    float cpu_throttle_ratio;       ///< CPU 제한 비율 (0.0-1.0)
    float gpu_throttle_ratio;       ///< GPU 제한 비율 (0.0-1.0)
    int max_threads_when_hot;       ///< 뜨거울 때 최대 스레드 수

    // 냉각 설정
    bool enable_active_cooling;     ///< 능동 냉각 활성화
    int cooling_timeout_ms;         ///< 냉각 타임아웃 (ms)
} ThermalConfig;

/**
 * 온도 센서 정보
 */
typedef struct {
    TempSensorType type;            ///< 센서 타입
    char name[32];                  ///< 센서 이름
    float temperature_c;            ///< 현재 온도 (°C)
    float max_temperature_c;        ///< 최대 온도 (°C)
    bool is_available;              ///< 센서 사용 가능 여부
    char device_path[128];          ///< 디바이스 경로 (Linux)
} TempSensorInfo;

/**
 * 열 상태 정보
 */
typedef struct {
    ThermalState current_state;     ///< 현재 열 상태
    float max_temperature_c;        ///< 최고 온도 (°C)
    float avg_temperature_c;        ///< 평균 온도 (°C)

    // 센서별 온도
    float cpu_temperature_c;        ///< CPU 온도 (°C)
    float gpu_temperature_c;        ///< GPU 온도 (°C)
    float battery_temperature_c;    ///< 배터리 온도 (°C)
    float ambient_temperature_c;    ///< 주변 온도 (°C)
    float skin_temperature_c;       ///< 표면 온도 (°C)

    // 제한 상태
    bool cpu_throttled;             ///< CPU 제한 여부
    bool gpu_throttled;             ///< GPU 제한 여부
    float current_cpu_ratio;        ///< 현재 CPU 성능 비율
    float current_gpu_ratio;        ///< 현재 GPU 성능 비율

    // 통계
    int throttle_events_count;      ///< 제한 이벤트 수
    int64_t total_throttle_time_ms; ///< 총 제한 시간 (ms)
} ThermalStatus;

/**
 * 열 이벤트 콜백 타입
 */
typedef void (*ThermalEventCallback)(ThermalState old_state, ThermalState new_state, const ThermalStatus* status, void* user_data);

// ============================================================================
// 열 관리 초기화 및 정리 함수들
// ============================================================================

/**
 * 열 관리 시스템을 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_management_init();

/**
 * 열 관리 시스템을 정리합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_management_cleanup();

/**
 * 열 관리 설정을 적용합니다
 *
 * @param config 열 관리 설정
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_set_config(const ThermalConfig* config);

/**
 * 현재 열 관리 설정을 가져옵니다
 *
 * @param config 설정을 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_get_config(ThermalConfig* config);

// ============================================================================
// 온도 센서 관리 함수들
// ============================================================================

/**
 * 사용 가능한 온도 센서 목록을 가져옵니다
 *
 * @param sensors 센서 정보 배열
 * @param max_sensors 최대 센서 수
 * @param actual_count 실제 센서 수를 저장할 포인터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_get_sensors(TempSensorInfo* sensors, int max_sensors, int* actual_count);

/**
 * 특정 센서의 온도를 읽습니다
 *
 * @param sensor_type 센서 타입
 * @param temperature 온도를 저장할 포인터 (°C)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_read_temperature(TempSensorType sensor_type, float* temperature);

/**
 * 모든 센서의 온도를 읽습니다
 *
 * @param status 열 상태를 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_read_all_temperatures(ThermalStatus* status);

// ============================================================================
// 열 상태 관리 함수들
// ============================================================================

/**
 * 현재 열 상태를 가져옵니다
 *
 * @param status 열 상태를 저장할 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_get_status(ThermalStatus* status);

/**
 * 열 상태를 업데이트합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_update_status();

/**
 * 온도 기반으로 열 상태를 결정합니다
 *
 * @param temperature 온도 (°C)
 * @param thresholds 온도 임계값
 * @param current_state 현재 상태
 * @return 새로운 열 상태
 */
ThermalState thermal_determine_state(float temperature, const ThermalThresholds* thresholds, ThermalState current_state);

// ============================================================================
// 열 제한 관리 함수들
// ============================================================================

/**
 * 열 제한을 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param thermal_state 열 상태
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_apply_throttling(void* engine, ThermalState thermal_state);

/**
 * CPU 열 제한을 적용합니다
 *
 * @param throttle_ratio 제한 비율 (0.0-1.0)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_throttle_cpu(float throttle_ratio);

/**
 * GPU 열 제한을 적용합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param throttle_ratio 제한 비율 (0.0-1.0)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_throttle_gpu(void* engine, float throttle_ratio);

/**
 * 모든 열 제한을 해제합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_remove_throttling(void* engine);

/**
 * 예측적 열 제한을 수행합니다
 *
 * @param engine LibEtude 엔진 핸들
 * @param predicted_temperature 예측 온도 (°C)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_predictive_throttling(void* engine, float predicted_temperature);

// ============================================================================
// 모니터링 및 이벤트 함수들
// ============================================================================

/**
 * 열 모니터링을 시작합니다
 *
 * @param callback 이벤트 콜백
 * @param user_data 사용자 데이터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_start_monitoring(ThermalEventCallback callback, void* user_data);

/**
 * 열 모니터링을 중지합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_stop_monitoring();

/**
 * 열 이벤트 콜백을 설정합니다
 *
 * @param callback 이벤트 콜백
 * @param user_data 사용자 데이터
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_set_event_callback(ThermalEventCallback callback, void* user_data);

// ============================================================================
// 냉각 관리 함수들
// ============================================================================

/**
 * 능동 냉각을 시작합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_start_active_cooling();

/**
 * 능동 냉각을 중지합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_stop_active_cooling();

/**
 * 냉각 대기를 수행합니다
 *
 * @param target_temperature 목표 온도 (°C)
 * @param timeout_ms 타임아웃 (ms)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_wait_for_cooling(float target_temperature, int timeout_ms);

// ============================================================================
// 통계 및 리포트 함수들
// ============================================================================

/**
 * 열 관리 통계를 가져옵니다
 *
 * @param status 열 상태 구조체
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_get_statistics(ThermalStatus* status);

/**
 * 열 관리 리포트를 생성합니다
 *
 * @return 리포트 문자열 (호출자가 해제해야 함)
 */
char* thermal_generate_report();

/**
 * 온도 히스토리를 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_reset_history();

// ============================================================================
// 플랫폼별 열 관리 함수들
// ============================================================================

#ifdef ANDROID_PLATFORM
/**
 * Android thermal zone을 초기화합니다
 *
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_android_init_zones();

/**
 * Android thermal zone에서 온도를 읽습니다
 *
 * @param zone_id thermal zone ID
 * @param temperature 온도를 저장할 포인터 (°C)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_android_read_zone_temperature(int zone_id, float* temperature);
#endif

#ifdef IOS_PLATFORM
/**
 * iOS 열 상태 알림을 처리합니다
 *
 * @param thermal_state iOS 열 상태
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_ios_handle_thermal_state(int thermal_state);

/**
 * iOS 온도 센서에서 온도를 읽습니다
 *
 * @param sensor_name 센서 이름
 * @param temperature 온도를 저장할 포인터 (°C)
 * @return 성공 시 LIBETUDE_SUCCESS
 */
int thermal_ios_read_sensor_temperature(const char* sensor_name, float* temperature);
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_THERMAL_MANAGEMENT_H