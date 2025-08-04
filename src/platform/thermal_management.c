/**
 * @file thermal_management.c
 * @brief 모바일 열 관리 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/thermal_management.h"
#include "libetude/api.h"
#include "libetude/error.h"
#include "libetude/mobile_power_management.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifndef LIBETUDE_PLATFORM_WINDOWS
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef ANDROID_PLATFORM
#include <android/log.h>
#endif

#ifdef IOS_PLATFORM
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

// 최대 온도 센서 수
#define MAX_TEMP_SENSORS 16

// 온도 히스토리 크기
#define TEMP_HISTORY_SIZE 60

// 전역 열 관리 상태
static struct {
    bool initialized;
    ThermalConfig config;
    ThermalStatus status;

    // 온도 센서 정보
    TempSensorInfo sensors[MAX_TEMP_SENSORS];
    int sensor_count;

    // 온도 히스토리
    float temp_history[TEMP_HISTORY_SIZE];
    int history_index;

    // 모니터링 스레드
    pthread_t monitoring_thread;
    bool monitoring_active;
    ThermalEventCallback event_callback;
    void* callback_user_data;

    // 동기화
    pthread_mutex_t mutex;

    // 통계
    int64_t start_time_ms;
    int throttle_events_count;
    int64_t total_throttle_time_ms;
    int64_t last_throttle_start_ms;
} g_thermal_state = {0};

// 내부 함수 선언
static void* thermal_monitoring_thread(void* arg);
static int discover_temperature_sensors();
static int read_sensor_temperature_file(const char* path, float* temperature);
static float predict_temperature(float current_temp, float trend);
static int64_t get_current_time_ms();

// ============================================================================
// 초기화 및 정리 함수들
// ============================================================================

int thermal_management_init() {
    pthread_mutex_lock(&g_thermal_state.mutex);

    if (g_thermal_state.initialized) {
        pthread_mutex_unlock(&g_thermal_state.mutex);
        return LIBETUDE_SUCCESS;
    }

    // 기본 설정 초기화
    memset(&g_thermal_state.config, 0, sizeof(ThermalConfig));
    memset(&g_thermal_state.status, 0, sizeof(ThermalStatus));

    // 기본 설정값
    g_thermal_state.config.policy = THERMAL_POLICY_CONSERVATIVE;
    g_thermal_state.config.thresholds.normal_threshold_c = 40.0f;
    g_thermal_state.config.thresholds.warm_threshold_c = 50.0f;
    g_thermal_state.config.thresholds.hot_threshold_c = 65.0f;
    g_thermal_state.config.thresholds.critical_threshold_c = 80.0f;
    g_thermal_state.config.thresholds.hysteresis_c = 2.0f;
    g_thermal_state.config.monitoring_interval_ms = 1000;
    g_thermal_state.config.enable_predictive_throttling = true;
    g_thermal_state.config.cpu_throttle_ratio = 0.7f;
    g_thermal_state.config.gpu_throttle_ratio = 0.5f;
    g_thermal_state.config.max_threads_when_hot = 2;
    g_thermal_state.config.enable_active_cooling = false;
    g_thermal_state.config.cooling_timeout_ms = 30000;

    // 초기 상태 설정
    g_thermal_state.status.current_state = THERMAL_STATE_NORMAL;
    g_thermal_state.status.current_cpu_ratio = 1.0f;
    g_thermal_state.status.current_gpu_ratio = 1.0f;

    // 온도 센서 발견
    discover_temperature_sensors();

    // 시작 시간 기록
    g_thermal_state.start_time_ms = get_current_time_ms();

    g_thermal_state.initialized = true;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

int thermal_management_cleanup() {
    pthread_mutex_lock(&g_thermal_state.mutex);

    if (!g_thermal_state.initialized) {
        pthread_mutex_unlock(&g_thermal_state.mutex);
        return LIBETUDE_SUCCESS;
    }

    // 모니터링 중지
    if (g_thermal_state.monitoring_active) {
        g_thermal_state.monitoring_active = false;
        pthread_mutex_unlock(&g_thermal_state.mutex);
        pthread_join(g_thermal_state.monitoring_thread, NULL);
        pthread_mutex_lock(&g_thermal_state.mutex);
    }

    g_thermal_state.initialized = false;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

int thermal_set_config(const ThermalConfig* config) {
    if (!config) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_thermal_state.initialized) {
        thermal_management_init();
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    g_thermal_state.config = *config;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

int thermal_get_config(ThermalConfig* config) {
    if (!config) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_thermal_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    *config = g_thermal_state.config;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 온도 센서 관리 함수들
// ============================================================================

int thermal_get_sensors(TempSensorInfo* sensors, int max_sensors, int* actual_count) {
    if (!sensors || !actual_count || max_sensors <= 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_thermal_state.initialized) {
        thermal_management_init();
    }

    pthread_mutex_lock(&g_thermal_state.mutex);

    int count = (g_thermal_state.sensor_count < max_sensors) ?
                g_thermal_state.sensor_count : max_sensors;

    memcpy(sensors, g_thermal_state.sensors, count * sizeof(TempSensorInfo));
    *actual_count = count;

    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

int thermal_read_temperature(TempSensorType sensor_type, float* temperature) {
    if (!temperature) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_thermal_state.initialized) {
        thermal_management_init();
    }

    pthread_mutex_lock(&g_thermal_state.mutex);

    // 해당 타입의 센서 찾기
    for (int i = 0; i < g_thermal_state.sensor_count; i++) {
        if (g_thermal_state.sensors[i].type == sensor_type &&
            g_thermal_state.sensors[i].is_available) {

            int result = read_sensor_temperature_file(
                g_thermal_state.sensors[i].device_path, temperature);

            if (result == LIBETUDE_SUCCESS) {
                g_thermal_state.sensors[i].temperature_c = *temperature;

                // 최대 온도 업데이트
                if (*temperature > g_thermal_state.sensors[i].max_temperature_c) {
                    g_thermal_state.sensors[i].max_temperature_c = *temperature;
                }
            }

            pthread_mutex_unlock(&g_thermal_state.mutex);
            return result;
        }
    }

    pthread_mutex_unlock(&g_thermal_state.mutex);
    return LIBETUDE_ERROR_NOT_IMPLEMENTED;  // 센서를 찾을 수 없음
}

int thermal_read_all_temperatures(ThermalStatus* status) {
    if (!status) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_thermal_state.initialized) {
        thermal_management_init();
    }

    pthread_mutex_lock(&g_thermal_state.mutex);

    float max_temp = 0.0f;
    float total_temp = 0.0f;
    int valid_sensors = 0;

    // 모든 센서에서 온도 읽기
    for (int i = 0; i < g_thermal_state.sensor_count; i++) {
        if (!g_thermal_state.sensors[i].is_available) {
            continue;
        }

        float temp;
        if (read_sensor_temperature_file(g_thermal_state.sensors[i].device_path, &temp) == LIBETUDE_SUCCESS) {
            g_thermal_state.sensors[i].temperature_c = temp;

            // 센서 타입별로 온도 저장
            switch (g_thermal_state.sensors[i].type) {
                case TEMP_SENSOR_CPU:
                    status->cpu_temperature_c = temp;
                    break;
                case TEMP_SENSOR_GPU:
                    status->gpu_temperature_c = temp;
                    break;
                case TEMP_SENSOR_BATTERY:
                    status->battery_temperature_c = temp;
                    break;
                case TEMP_SENSOR_AMBIENT:
                    status->ambient_temperature_c = temp;
                    break;
                case TEMP_SENSOR_SKIN:
                    status->skin_temperature_c = temp;
                    break;
            }

            if (temp > max_temp) {
                max_temp = temp;
            }

            total_temp += temp;
            valid_sensors++;
        }
    }

    // 통계 계산
    status->max_temperature_c = max_temp;
    status->avg_temperature_c = (valid_sensors > 0) ? (total_temp / valid_sensors) : 0.0f;

    // 온도 히스토리 업데이트
    g_thermal_state.temp_history[g_thermal_state.history_index] = max_temp;
    g_thermal_state.history_index = (g_thermal_state.history_index + 1) % TEMP_HISTORY_SIZE;

    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 열 상태 관리 함수들
// ============================================================================

int thermal_get_status(ThermalStatus* status) {
    if (!status) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!g_thermal_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    *status = g_thermal_state.status;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

int thermal_update_status() {
    if (!g_thermal_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    // 모든 온도 센서 읽기
    thermal_read_all_temperatures(&g_thermal_state.status);

    pthread_mutex_lock(&g_thermal_state.mutex);

    // 이전 상태 저장
    ThermalState old_state = g_thermal_state.status.current_state;

    // 새로운 열 상태 결정
    ThermalState new_state = thermal_determine_state(
        g_thermal_state.status.max_temperature_c,
        &g_thermal_state.config.thresholds,
        old_state
    );

    g_thermal_state.status.current_state = new_state;

    // 상태 변경 시 이벤트 콜백 호출
    if (old_state != new_state && g_thermal_state.event_callback) {
        g_thermal_state.event_callback(old_state, new_state, &g_thermal_state.status,
                                      g_thermal_state.callback_user_data);
    }

    // 제한 이벤트 통계 업데이트
    if (new_state > THERMAL_STATE_NORMAL && old_state <= THERMAL_STATE_NORMAL) {
        g_thermal_state.throttle_events_count++;
        g_thermal_state.last_throttle_start_ms = get_current_time_ms();
    } else if (new_state <= THERMAL_STATE_NORMAL && old_state > THERMAL_STATE_NORMAL) {
        if (g_thermal_state.last_throttle_start_ms > 0) {
            g_thermal_state.total_throttle_time_ms +=
                get_current_time_ms() - g_thermal_state.last_throttle_start_ms;
        }
    }

    g_thermal_state.status.throttle_events_count = g_thermal_state.throttle_events_count;
    g_thermal_state.status.total_throttle_time_ms = g_thermal_state.total_throttle_time_ms;

    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

ThermalState thermal_determine_state(float temperature, const ThermalThresholds* thresholds, ThermalState current_state) {
    if (!thresholds) {
        return THERMAL_STATE_NORMAL;
    }

    // 히스테리시스를 고려한 상태 결정
    float hysteresis = thresholds->hysteresis_c;

    switch (current_state) {
        case THERMAL_STATE_NORMAL:
            if (temperature >= thresholds->warm_threshold_c) {
                return THERMAL_STATE_WARM;
            }
            break;

        case THERMAL_STATE_WARM:
            if (temperature >= thresholds->hot_threshold_c) {
                return THERMAL_STATE_HOT;
            } else if (temperature <= thresholds->warm_threshold_c - hysteresis) {
                return THERMAL_STATE_NORMAL;
            }
            break;

        case THERMAL_STATE_HOT:
            if (temperature >= thresholds->critical_threshold_c) {
                return THERMAL_STATE_CRITICAL;
            } else if (temperature <= thresholds->hot_threshold_c - hysteresis) {
                return THERMAL_STATE_WARM;
            }
            break;

        case THERMAL_STATE_CRITICAL:
            if (temperature <= thresholds->critical_threshold_c - hysteresis) {
                return THERMAL_STATE_HOT;
            }
            break;
    }

    return current_state;  // 상태 변경 없음
}

// ============================================================================
// 열 제한 관리 함수들
// ============================================================================

int thermal_apply_throttling(void* engine, ThermalState thermal_state) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    ThermalConfig config = g_thermal_state.config;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    int result = LIBETUDE_SUCCESS;

    switch (thermal_state) {
        case THERMAL_STATE_NORMAL:
            // 모든 제한 해제
            result = thermal_remove_throttling(engine);
            break;

        case THERMAL_STATE_WARM:
            // 가벼운 제한
            if (config.policy >= THERMAL_POLICY_CONSERVATIVE) {
                thermal_throttle_cpu(0.9f);
                thermal_throttle_gpu(engine, 0.8f);
                libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
            }
            break;

        case THERMAL_STATE_HOT:
            // 중간 제한
            thermal_throttle_cpu(config.cpu_throttle_ratio);
            thermal_throttle_gpu(engine, config.gpu_throttle_ratio);
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
            break;

        case THERMAL_STATE_CRITICAL:
            // 강한 제한
            thermal_throttle_cpu(0.3f);
            thermal_throttle_gpu(engine, 0.1f);
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);

            // 능동 냉각 시작
            if (config.enable_active_cooling) {
                thermal_start_active_cooling();
            }
            break;
    }

    // 상태 업데이트
    pthread_mutex_lock(&g_thermal_state.mutex);
    g_thermal_state.status.cpu_throttled = (thermal_state > THERMAL_STATE_NORMAL);
    g_thermal_state.status.gpu_throttled = (thermal_state > THERMAL_STATE_NORMAL);

    switch (thermal_state) {
        case THERMAL_STATE_NORMAL:
            g_thermal_state.status.current_cpu_ratio = 1.0f;
            g_thermal_state.status.current_gpu_ratio = 1.0f;
            break;
        case THERMAL_STATE_WARM:
            g_thermal_state.status.current_cpu_ratio = 0.9f;
            g_thermal_state.status.current_gpu_ratio = 0.8f;
            break;
        case THERMAL_STATE_HOT:
            g_thermal_state.status.current_cpu_ratio = config.cpu_throttle_ratio;
            g_thermal_state.status.current_gpu_ratio = config.gpu_throttle_ratio;
            break;
        case THERMAL_STATE_CRITICAL:
            g_thermal_state.status.current_cpu_ratio = 0.3f;
            g_thermal_state.status.current_gpu_ratio = 0.1f;
            break;
    }
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return result;
}

int thermal_throttle_cpu(float throttle_ratio) {
    if (throttle_ratio < 0.0f || throttle_ratio > 1.0f) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // CPU 주파수 제한 적용 (플랫폼별 구현 필요)
#ifdef ANDROID_PLATFORM
    // Android에서 CPU governor 설정
    // echo "conservative" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
    // echo "1000000" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
#endif

#ifdef IOS_PLATFORM
    // iOS에서는 시스템이 자동으로 관리
#endif

    return LIBETUDE_SUCCESS;
}

int thermal_throttle_gpu(void* engine, float throttle_ratio) {
    if (!engine || throttle_ratio < 0.0f || throttle_ratio > 1.0f) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // GPU 성능 제한 (실제로는 GPU 드라이버 API 사용)
    if (throttle_ratio < 0.5f) {
        // GPU 가속 비활성화
        // libetude_disable_gpu_acceleration(engine);
    } else {
        // GPU 주파수 제한
        // gpu_set_max_frequency(base_frequency * throttle_ratio);
    }

    return LIBETUDE_SUCCESS;
}

int thermal_remove_throttling(void* engine) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 모든 제한 해제
    thermal_throttle_cpu(1.0f);
    thermal_throttle_gpu(engine, 1.0f);

    // 품질 모드 복원
    libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH);

    // 능동 냉각 중지
    thermal_stop_active_cooling();

    return LIBETUDE_SUCCESS;
}

int thermal_predictive_throttling(void* engine, float predicted_temperature) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    ThermalThresholds thresholds = g_thermal_state.config.thresholds;
    ThermalState current_state = g_thermal_state.status.current_state;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    // 예측 온도를 기반으로 미리 제한 적용
    ThermalState predicted_state = thermal_determine_state(predicted_temperature, &thresholds, current_state);

    if (predicted_state > current_state) {
        return thermal_apply_throttling(engine, predicted_state);
    }

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 모니터링 및 이벤트 함수들
// ============================================================================

int thermal_start_monitoring(ThermalEventCallback callback, void* user_data) {
    if (!g_thermal_state.initialized) {
        thermal_management_init();
    }

    pthread_mutex_lock(&g_thermal_state.mutex);

    if (g_thermal_state.monitoring_active) {
        pthread_mutex_unlock(&g_thermal_state.mutex);
        return LIBETUDE_ERROR_RUNTIME;  // 이미 모니터링 중
    }

    g_thermal_state.event_callback = callback;
    g_thermal_state.callback_user_data = user_data;
    g_thermal_state.monitoring_active = true;

    int result = pthread_create(&g_thermal_state.monitoring_thread, NULL, thermal_monitoring_thread, NULL);
    if (result != 0) {
        g_thermal_state.monitoring_active = false;
        pthread_mutex_unlock(&g_thermal_state.mutex);
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_unlock(&g_thermal_state.mutex);
    return LIBETUDE_SUCCESS;
}

int thermal_stop_monitoring() {
    pthread_mutex_lock(&g_thermal_state.mutex);

    if (!g_thermal_state.monitoring_active) {
        pthread_mutex_unlock(&g_thermal_state.mutex);
        return LIBETUDE_SUCCESS;
    }

    g_thermal_state.monitoring_active = false;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    pthread_join(g_thermal_state.monitoring_thread, NULL);

    return LIBETUDE_SUCCESS;
}

int thermal_set_event_callback(ThermalEventCallback callback, void* user_data) {
    if (!g_thermal_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    g_thermal_state.event_callback = callback;
    g_thermal_state.callback_user_data = user_data;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 냉각 관리 함수들
// ============================================================================

int thermal_start_active_cooling() {
    // 능동 냉각 시작 (실제로는 팬 제어나 CPU 클럭 다운)
    // 여기서는 시뮬레이션
    return LIBETUDE_SUCCESS;
}

int thermal_stop_active_cooling() {
    // 능동 냉각 중지
    return LIBETUDE_SUCCESS;
}

int thermal_wait_for_cooling(float target_temperature, int timeout_ms) {
    if (target_temperature < 0.0f || timeout_ms <= 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    int64_t start_time = get_current_time_ms();
    int64_t end_time = start_time + timeout_ms;

    while (get_current_time_ms() < end_time) {
        thermal_update_status();

        pthread_mutex_lock(&g_thermal_state.mutex);
        float current_temp = g_thermal_state.status.max_temperature_c;
        pthread_mutex_unlock(&g_thermal_state.mutex);

        if (current_temp <= target_temperature) {
            return LIBETUDE_SUCCESS;  // 목표 온도 달성
        }

        usleep(1000000);  // 1초 대기
    }

    return LIBETUDE_ERROR_TIMEOUT;  // 타임아웃
}

// ============================================================================
// 통계 및 리포트 함수들
// ============================================================================

int thermal_get_statistics(ThermalStatus* status) {
    return thermal_get_status(status);
}

char* thermal_generate_report() {
    if (!g_thermal_state.initialized) {
        return NULL;
    }

    char* report = malloc(2048);
    if (!report) {
        return NULL;
    }

    thermal_update_status();

    pthread_mutex_lock(&g_thermal_state.mutex);

    snprintf(report, 2048,
        "=== LibEtude Thermal Management Report ===\n\n"
        "Current State: %s\n"
        "Policy: %s\n\n"

        "Temperature Status:\n"
        "  Max Temperature: %.1f°C\n"
        "  Avg Temperature: %.1f°C\n"
        "  CPU Temperature: %.1f°C\n"
        "  GPU Temperature: %.1f°C\n"
        "  Battery Temperature: %.1f°C\n"
        "  Ambient Temperature: %.1f°C\n"
        "  Skin Temperature: %.1f°C\n\n"

        "Thresholds:\n"
        "  Normal: < %.1f°C\n"
        "  Warm: %.1f°C - %.1f°C\n"
        "  Hot: %.1f°C - %.1f°C\n"
        "  Critical: > %.1f°C\n\n"

        "Throttling Status:\n"
        "  CPU Throttled: %s (%.1f%%)\n"
        "  GPU Throttled: %s (%.1f%%)\n\n"

        "Statistics:\n"
        "  Throttle Events: %d\n"
        "  Total Throttle Time: %.1f seconds\n"
        "  Sensors Available: %d\n",

        // Current state
        (g_thermal_state.status.current_state == THERMAL_STATE_NORMAL) ? "Normal" :
        (g_thermal_state.status.current_state == THERMAL_STATE_WARM) ? "Warm" :
        (g_thermal_state.status.current_state == THERMAL_STATE_HOT) ? "Hot" : "Critical",

        // Policy
        (g_thermal_state.config.policy == THERMAL_POLICY_NONE) ? "None" :
        (g_thermal_state.config.policy == THERMAL_POLICY_CONSERVATIVE) ? "Conservative" : "Aggressive",

        // Temperatures
        g_thermal_state.status.max_temperature_c,
        g_thermal_state.status.avg_temperature_c,
        g_thermal_state.status.cpu_temperature_c,
        g_thermal_state.status.gpu_temperature_c,
        g_thermal_state.status.battery_temperature_c,
        g_thermal_state.status.ambient_temperature_c,
        g_thermal_state.status.skin_temperature_c,

        // Thresholds
        g_thermal_state.config.thresholds.normal_threshold_c,
        g_thermal_state.config.thresholds.warm_threshold_c,
        g_thermal_state.config.thresholds.hot_threshold_c,
        g_thermal_state.config.thresholds.hot_threshold_c,
        g_thermal_state.config.thresholds.critical_threshold_c,
        g_thermal_state.config.thresholds.critical_threshold_c,

        // Throttling
        g_thermal_state.status.cpu_throttled ? "Yes" : "No",
        g_thermal_state.status.current_cpu_ratio * 100.0f,
        g_thermal_state.status.gpu_throttled ? "Yes" : "No",
        g_thermal_state.status.current_gpu_ratio * 100.0f,

        // Statistics
        g_thermal_state.status.throttle_events_count,
        g_thermal_state.status.total_throttle_time_ms / 1000.0f,
        g_thermal_state.sensor_count
    );

    pthread_mutex_unlock(&g_thermal_state.mutex);

    return report;
}

int thermal_reset_history() {
    if (!g_thermal_state.initialized) {
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    memset(g_thermal_state.temp_history, 0, sizeof(g_thermal_state.temp_history));
    g_thermal_state.history_index = 0;
    g_thermal_state.throttle_events_count = 0;
    g_thermal_state.total_throttle_time_ms = 0;
    g_thermal_state.start_time_ms = get_current_time_ms();
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static void* thermal_monitoring_thread(void* arg) {
    (void)arg;

    while (g_thermal_state.monitoring_active) {
        thermal_update_status();

        // 예측적 제한이 활성화된 경우
        pthread_mutex_lock(&g_thermal_state.mutex);
        bool predictive_enabled = g_thermal_state.config.enable_predictive_throttling;
        int interval_ms = g_thermal_state.config.monitoring_interval_ms;
        pthread_mutex_unlock(&g_thermal_state.mutex);

        if (predictive_enabled) {
            // 온도 트렌드 계산 및 예측
            float current_temp = g_thermal_state.status.max_temperature_c;
            float trend = 0.0f;  // 실제로는 히스토리 기반 계산
            float predicted_temp = predict_temperature(current_temp, trend);

            // 예측 온도가 임계값에 근접하면 미리 제한 적용
            // thermal_predictive_throttling(engine, predicted_temp);  // engine 필요
        }

        usleep(interval_ms * 1000);
    }

    return NULL;
}

static int discover_temperature_sensors() {
    g_thermal_state.sensor_count = 0;

#ifdef ANDROID_PLATFORM
    // Android thermal zone 스캔
    DIR* thermal_dir = opendir("/sys/class/thermal");
    if (thermal_dir) {
        struct dirent* entry;
        while ((entry = readdir(thermal_dir)) != NULL &&
               g_thermal_state.sensor_count < MAX_TEMP_SENSORS) {

            if (strncmp(entry->d_name, "thermal_zone", 12) == 0) {
                TempSensorInfo* sensor = &g_thermal_state.sensors[g_thermal_state.sensor_count];

                snprintf(sensor->device_path, sizeof(sensor->device_path),
                        "/sys/class/thermal/%s/temp", entry->d_name);

                // 센서 타입 추정 (실제로는 type 파일 읽기)
                if (strstr(entry->d_name, "cpu") || strstr(entry->d_name, "tsens")) {
                    sensor->type = TEMP_SENSOR_CPU;
                    strcpy(sensor->name, "CPU");
                } else if (strstr(entry->d_name, "gpu")) {
                    sensor->type = TEMP_SENSOR_GPU;
                    strcpy(sensor->name, "GPU");
                } else if (strstr(entry->d_name, "battery")) {
                    sensor->type = TEMP_SENSOR_BATTERY;
                    strcpy(sensor->name, "Battery");
                } else {
                    sensor->type = TEMP_SENSOR_AMBIENT;
                    strcpy(sensor->name, "Ambient");
                }

                sensor->is_available = (access(sensor->device_path, R_OK) == 0);
                sensor->temperature_c = 0.0f;
                sensor->max_temperature_c = 0.0f;

                g_thermal_state.sensor_count++;
            }
        }
        closedir(thermal_dir);
    }

#elif defined(IOS_PLATFORM)
    // iOS에서는 제한된 온도 센서 정보만 사용 가능
    // 시뮬레이션된 센서 추가
    if (g_thermal_state.sensor_count < MAX_TEMP_SENSORS) {
        TempSensorInfo* sensor = &g_thermal_state.sensors[g_thermal_state.sensor_count];
        sensor->type = TEMP_SENSOR_CPU;
        strcpy(sensor->name, "CPU");
        strcpy(sensor->device_path, "ios_cpu_temp");
        sensor->is_available = true;
        sensor->temperature_c = 0.0f;
        sensor->max_temperature_c = 0.0f;
        g_thermal_state.sensor_count++;
    }

#else
    // 기본 구현 - 시뮬레이션된 센서
    const char* sensor_names[] = {"CPU", "GPU", "Battery"};
    TempSensorType sensor_types[] = {TEMP_SENSOR_CPU, TEMP_SENSOR_GPU, TEMP_SENSOR_BATTERY};

    for (int i = 0; i < 3 && g_thermal_state.sensor_count < MAX_TEMP_SENSORS; i++) {
        TempSensorInfo* sensor = &g_thermal_state.sensors[g_thermal_state.sensor_count];
        sensor->type = sensor_types[i];
        strcpy(sensor->name, sensor_names[i]);
        snprintf(sensor->device_path, sizeof(sensor->device_path), "sim_%s", sensor_names[i]);
        sensor->is_available = true;
        sensor->temperature_c = 30.0f + i * 5.0f;  // 시뮬레이션 온도
        sensor->max_temperature_c = sensor->temperature_c;
        g_thermal_state.sensor_count++;
    }
#endif

    return g_thermal_state.sensor_count;
}

static int read_sensor_temperature_file(const char* path, float* temperature) {
    if (!path || !temperature) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

#ifdef ANDROID_PLATFORM
    FILE* temp_file = fopen(path, "r");
    if (!temp_file) {
        return LIBETUDE_ERROR_IO;
    }

    int temp_millidegree;
    if (fscanf(temp_file, "%d", &temp_millidegree) == 1) {
        *temperature = temp_millidegree / 1000.0f;
        fclose(temp_file);
        return LIBETUDE_SUCCESS;
    }

    fclose(temp_file);
    return LIBETUDE_ERROR_IO;

#else
    // 시뮬레이션된 온도 (실제로는 플랫폼별 API 사용)
    static float sim_temp = 35.0f;
    sim_temp += (rand() % 10 - 5) / 10.0f;  // ±0.5도 변동
    if (sim_temp < 20.0f) sim_temp = 20.0f;
    if (sim_temp > 80.0f) sim_temp = 80.0f;

    *temperature = sim_temp;
    return LIBETUDE_SUCCESS;
#endif
}

static float predict_temperature(float current_temp, float trend) {
    // 간단한 선형 예측 (실제로는 더 복잡한 모델 사용)
    return current_temp + trend * 5.0f;  // 5초 후 예측 온도
}

static int64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ============================================================================
// 플랫폼별 특화 함수들
// ============================================================================

#ifdef ANDROID_PLATFORM
int thermal_android_init_zones() {
    return discover_temperature_sensors();
}

int thermal_android_read_zone_temperature(int zone_id, float* temperature) {
    if (!temperature || zone_id < 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    char path[128];
    snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", zone_id);

    return read_sensor_temperature_file(path, temperature);
}
#endif

#ifdef IOS_PLATFORM
int thermal_ios_handle_thermal_state(int thermal_state) {
    // iOS 열 상태 알림 처리
    ThermalState state;

    switch (thermal_state) {
        case 0:  // NSProcessInfoThermalStateNominal
            state = THERMAL_STATE_NORMAL;
            break;
        case 1:  // NSProcessInfoThermalStateFair
            state = THERMAL_STATE_WARM;
            break;
        case 2:  // NSProcessInfoThermalStateSerious
            state = THERMAL_STATE_HOT;
            break;
        case 3:  // NSProcessInfoThermalStateCritical
            state = THERMAL_STATE_CRITICAL;
            break;
        default:
            state = THERMAL_STATE_NORMAL;
            break;
    }

    pthread_mutex_lock(&g_thermal_state.mutex);
    g_thermal_state.status.current_state = state;
    pthread_mutex_unlock(&g_thermal_state.mutex);

    return LIBETUDE_SUCCESS;
}

int thermal_ios_read_sensor_temperature(const char* sensor_name, float* temperature) {
    if (!sensor_name || !temperature) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // iOS에서는 공개된 온도 센서 API가 제한적이므로 시뮬레이션
    *temperature = 40.0f + (rand() % 20);  // 40-60도 범위

    return LIBETUDE_SUCCESS;
}
#endif