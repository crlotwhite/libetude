/**
 * @file mobile_optimization.c
 * @brief 모바일 플랫폼 최적화 유틸리티 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "mobile_optimization.h"
#include "libetude/api.h"
#include "libetude/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#ifdef ANDROID_PLATFORM
#include <android/log.h>
#include <sys/system_properties.h>
#endif

#ifdef IOS_PLATFORM
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

// 전역 모니터링 상태
static struct {
    bool monitoring_active;
    pthread_t monitoring_thread;
    MobileOptimizationCallback callback;
    void* user_data;
    int interval_ms;
    pthread_mutex_t mutex;
} g_monitoring_state = {0};

// 내부 함수 선언
static void* resource_monitoring_thread(void* arg);
static int get_cpu_usage(float* usage);
static int get_memory_info(size_t* used_mb, size_t* available_mb);
static int get_cpu_temperature(float* temperature);

// ============================================================================
// 플랫폼 감지 함수들
// ============================================================================

MobilePlatform mobile_detect_platform() {
#ifdef ANDROID_PLATFORM
    return MOBILE_PLATFORM_ANDROID;
#elif defined(IOS_PLATFORM)
    return MOBILE_PLATFORM_IOS;
#else
    return MOBILE_PLATFORM_UNKNOWN;
#endif
}

MobileDeviceClass mobile_detect_device_class() {
    // 메모리 크기와 CPU 코어 수를 기반으로 디바이스 클래스 결정
    size_t memory_mb = 0;
    int cpu_cores = 0;

#ifdef ANDROID_PLATFORM
    // Android에서 메모리 정보 가져오기
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                unsigned long mem_kb;
                sscanf(line, "MemTotal: %lu kB", &mem_kb);
                memory_mb = mem_kb / 1024;
                break;
            }
        }
        fclose(meminfo);
    }

    // CPU 코어 수 가져오기
    cpu_cores = sysconf(_SC_NPROCESSORS_CONF);

#elif defined(IOS_PLATFORM)
    // iOS에서 메모리 정보 가져오기
    size_t size = sizeof(uint64_t);
    uint64_t memory_bytes;
    if (sysctlbyname("hw.memsize", &memory_bytes, &size, NULL, 0) == 0) {
        memory_mb = memory_bytes / (1024 * 1024);
    }

    // CPU 코어 수 가져오기
    size = sizeof(int);
    sysctlbyname("hw.ncpu", &cpu_cores, &size, NULL, 0);
#endif

    // 디바이스 클래스 결정 로직
    if (memory_mb >= 6144 && cpu_cores >= 6) {  // 6GB+ RAM, 6+ cores
        return MOBILE_DEVICE_HIGH_END;
    } else if (memory_mb >= 3072 && cpu_cores >= 4) {  // 3GB+ RAM, 4+ cores
        return MOBILE_DEVICE_MID_RANGE;
    } else {
        return MOBILE_DEVICE_LOW_END;
    }
}

// ============================================================================
// 설정 생성 함수
// ============================================================================

MobileOptimizationConfig mobile_create_default_config(MobilePlatform platform, MobileDeviceClass device_class) {
    MobileOptimizationConfig config = {0};

    config.platform = platform;
    config.device_class = device_class;

    // 디바이스 클래스별 기본 설정
    switch (device_class) {
        case MOBILE_DEVICE_LOW_END:
            config.memory_limit_mb = 64;
            config.max_threads = 2;
            config.battery_optimized = true;
            config.adaptive_quality = true;
            config.min_quality_level = 0;  // FAST
            config.max_quality_level = 1;  // BALANCED
            config.cpu_usage_limit = 0.6f;
            break;

        case MOBILE_DEVICE_MID_RANGE:
            config.memory_limit_mb = 128;
            config.max_threads = 4;
            config.battery_optimized = false;
            config.adaptive_quality = true;
            config.min_quality_level = 0;  // FAST
            config.max_quality_level = 2;  // HIGH
            config.cpu_usage_limit = 0.8f;
            break;

        case MOBILE_DEVICE_HIGH_END:
            config.memory_limit_mb = 256;
            config.max_threads = 6;
            config.battery_optimized = false;
            config.adaptive_quality = false;
            config.min_quality_level = 1;  // BALANCED
            config.max_quality_level = 2;  // HIGH
            config.cpu_usage_limit = 0.9f;
            break;
    }

    // 공통 설정
    config.enable_memory_pressure_handling = true;
    config.memory_warning_threshold = 0.8f;
    config.enable_thermal_throttling = true;
    config.disable_gpu_on_battery = true;
    config.enable_model_streaming = false;
    config.cache_size_mb = 32;

    return config;
}

// ============================================================================
// 리소스 상태 함수들
// ============================================================================

int mobile_get_resource_status(MobileResourceStatus* status) {
    if (!status) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    memset(status, 0, sizeof(MobileResourceStatus));

    // 메모리 정보 가져오기
    get_memory_info(&status->memory_used_mb, &status->memory_available_mb);

    // 메모리 압박 수준 계산
    size_t total_memory = status->memory_used_mb + status->memory_available_mb;
    if (total_memory > 0) {
        status->memory_pressure = (float)status->memory_used_mb / total_memory;
    }

    // CPU 사용률 가져오기
    get_cpu_usage(&status->cpu_usage);

    // CPU 온도 가져오기
    get_cpu_temperature(&status->cpu_temperature);

    // 열 제한 상태 확인 (온도가 70도 이상이면 활성화)
    status->thermal_throttling_active = (status->cpu_temperature > 70.0f);

    // 배터리 정보는 플랫폼별로 구현 필요
#ifdef ANDROID_PLATFORM
    // Android 배터리 정보 구현
    status->battery_level = 0.8f;  // 임시값
    status->is_charging = false;
    status->low_power_mode = false;
#elif defined(IOS_PLATFORM)
    // iOS 배터리 정보 구현
    status->battery_level = 0.8f;  // 임시값
    status->is_charging = false;
    status->low_power_mode = false;
#endif

    // 네트워크 정보는 간단히 구현
    status->network_available = true;
    status->wifi_connected = true;
    status->cellular_connected = false;

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 최적화 처리 함수들
// ============================================================================

int mobile_handle_memory_pressure(LibEtudeEngine* engine, float pressure_level) {
    if (!engine || pressure_level < 0.0f || pressure_level > 1.0f) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 메모리 압박 수준에 따른 처리
    if (pressure_level > 0.9f) {
        // 심각한 메모리 압박 - 품질을 최저로 설정
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
        // 추가적인 메모리 해제 작업 수행
    } else if (pressure_level > 0.7f) {
        // 중간 수준 메모리 압박 - 품질을 균형 모드로 설정
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
    }

    return LIBETUDE_SUCCESS;
}

int mobile_handle_thermal_throttling(LibEtudeEngine* engine, float temperature) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 온도에 따른 처리
    if (temperature > 80.0f) {
        // 매우 높은 온도 - 품질을 최저로 설정하고 스레드 수 제한
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
        // 스레드 수 제한 로직 (별도 API 필요)
    } else if (temperature > 70.0f) {
        // 높은 온도 - 품질을 균형 모드로 설정
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
    }

    return LIBETUDE_SUCCESS;
}

int mobile_optimize_for_battery(LibEtudeEngine* engine, float battery_level, bool is_charging, bool low_power_mode) {
    if (!engine || battery_level < 0.0f || battery_level > 1.0f) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (low_power_mode || (!is_charging && battery_level < 0.2f)) {
        // 저전력 모드 또는 배터리 부족 - 최대 절약 모드
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
        // GPU 가속 비활성화 (별도 API 필요)
    } else if (!is_charging && battery_level < 0.5f) {
        // 배터리 절약 모드
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
    }

    return LIBETUDE_SUCCESS;
}

int mobile_adaptive_quality_adjustment(LibEtudeEngine* engine, const MobileResourceStatus* status, const MobileOptimizationConfig* config) {
    if (!engine || !status || !config) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!config->adaptive_quality) {
        return LIBETUDE_SUCCESS;  // 적응형 품질이 비활성화됨
    }

    int target_quality = config->max_quality_level;

    // 메모리 압박 상황 확인
    if (status->memory_pressure > 0.8f) {
        target_quality = config->min_quality_level;
    } else if (status->memory_pressure > 0.6f) {
        target_quality = (config->min_quality_level + config->max_quality_level) / 2;
    }

    // CPU 사용률 확인
    if (status->cpu_usage > config->cpu_usage_limit) {
        target_quality = config->min_quality_level;
    }

    // 열 제한 확인
    if (status->thermal_throttling_active) {
        target_quality = config->min_quality_level;
    }

    // 배터리 상태 확인
    if (status->low_power_mode || (!status->is_charging && status->battery_level < 0.2f)) {
        target_quality = config->min_quality_level;
    }

    // 품질 설정 적용
    libetude_set_quality_mode(engine, (QualityMode)target_quality);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 리소스 모니터링 함수들
// ============================================================================

int mobile_start_resource_monitoring(MobileOptimizationCallback callback, void* user_data, int interval_ms) {
    if (!callback || interval_ms <= 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&g_monitoring_state.mutex);

    if (g_monitoring_state.monitoring_active) {
        pthread_mutex_unlock(&g_monitoring_state.mutex);
        return LIBETUDE_ERROR_RUNTIME;  // 이미 모니터링 중
    }

    g_monitoring_state.callback = callback;
    g_monitoring_state.user_data = user_data;
    g_monitoring_state.interval_ms = interval_ms;
    g_monitoring_state.monitoring_active = true;

    int result = pthread_create(&g_monitoring_state.monitoring_thread, NULL, resource_monitoring_thread, NULL);
    if (result != 0) {
        g_monitoring_state.monitoring_active = false;
        pthread_mutex_unlock(&g_monitoring_state.mutex);
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_unlock(&g_monitoring_state.mutex);
    return LIBETUDE_SUCCESS;
}

int mobile_stop_resource_monitoring() {
    pthread_mutex_lock(&g_monitoring_state.mutex);

    if (!g_monitoring_state.monitoring_active) {
        pthread_mutex_unlock(&g_monitoring_state.mutex);
        return LIBETUDE_SUCCESS;  // 이미 중지됨
    }

    g_monitoring_state.monitoring_active = false;
    pthread_mutex_unlock(&g_monitoring_state.mutex);

    // 스레드 종료 대기
    pthread_join(g_monitoring_state.monitoring_thread, NULL);

    return LIBETUDE_SUCCESS;
}

char* mobile_get_optimization_stats() {
    // 간단한 통계 문자열 생성
    char* stats = malloc(512);
    if (!stats) {
        return NULL;
    }

    MobileResourceStatus status;
    if (mobile_get_resource_status(&status) == LIBETUDE_SUCCESS) {
        snprintf(stats, 512,
            "Mobile Optimization Stats:\n"
            "Memory: %zu/%zu MB (%.1f%% pressure)\n"
            "CPU: %.1f%% usage, %.1f°C\n"
            "Battery: %.1f%% (charging: %s, low power: %s)\n"
            "Thermal throttling: %s\n",
            status.memory_used_mb, status.memory_used_mb + status.memory_available_mb,
            status.memory_pressure * 100.0f,
            status.cpu_usage * 100.0f, status.cpu_temperature,
            status.battery_level * 100.0f,
            status.is_charging ? "yes" : "no",
            status.low_power_mode ? "yes" : "no",
            status.thermal_throttling_active ? "active" : "inactive"
        );
    } else {
        snprintf(stats, 512, "Mobile Optimization Stats: Unable to retrieve status\n");
    }

    return stats;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static void* resource_monitoring_thread(void* arg) {
    (void)arg;  // 사용하지 않는 매개변수

    while (g_monitoring_state.monitoring_active) {
        MobileResourceStatus status;
        if (mobile_get_resource_status(&status) == LIBETUDE_SUCCESS) {
            pthread_mutex_lock(&g_monitoring_state.mutex);
            if (g_monitoring_state.monitoring_active && g_monitoring_state.callback) {
                g_monitoring_state.callback(&status, g_monitoring_state.user_data);
            }
            pthread_mutex_unlock(&g_monitoring_state.mutex);
        }

        // 지정된 간격만큼 대기
        usleep(g_monitoring_state.interval_ms * 1000);
    }

    return NULL;
}

static int get_cpu_usage(float* usage) {
    if (!usage) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 간단한 CPU 사용률 측정 구현
    // 실제로는 /proc/stat (Android) 또는 host_statistics (iOS)를 사용해야 함
    static float last_usage = 0.0f;

    // 임시 구현 - 실제로는 플랫폼별 구현 필요
    *usage = last_usage + (rand() % 20 - 10) / 100.0f;  // ±10% 변동
    if (*usage < 0.0f) *usage = 0.0f;
    if (*usage > 1.0f) *usage = 1.0f;

    last_usage = *usage;
    return LIBETUDE_SUCCESS;
}

static int get_memory_info(size_t* used_mb, size_t* available_mb) {
    if (!used_mb || !available_mb) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

#ifdef ANDROID_PLATFORM
    // Android 메모리 정보 구현
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) {
        return LIBETUDE_ERROR_IO;
    }

    unsigned long total_kb = 0, available_kb = 0;
    char line[256];

    while (fgets(line, sizeof(line), meminfo)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %lu kB", &total_kb);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %lu kB", &available_kb);
        }
    }

    fclose(meminfo);

    *available_mb = available_kb / 1024;
    *used_mb = (total_kb - available_kb) / 1024;

#elif defined(IOS_PLATFORM)
    // iOS 메모리 정보 구현
    struct mach_task_basic_info info;
    mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        *used_mb = info.resident_size / (1024 * 1024);

        // 전체 메모리에서 사용 중인 메모리를 빼서 사용 가능한 메모리 계산
        size_t total_memory_size = sizeof(uint64_t);
        uint64_t total_memory;
        if (sysctlbyname("hw.memsize", &total_memory, &total_memory_size, NULL, 0) == 0) {
            *available_mb = (total_memory / (1024 * 1024)) - *used_mb;
        } else {
            *available_mb = 1024;  // 기본값
        }
    } else {
        return LIBETUDE_ERROR_RUNTIME;
    }

#else
    // 기본 구현
    *used_mb = 128;
    *available_mb = 256;
#endif

    return LIBETUDE_SUCCESS;
}

static int get_cpu_temperature(float* temperature) {
    if (!temperature) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // CPU 온도 측정은 플랫폼별로 매우 다르므로 간단한 구현
    // 실제로는 thermal zone 파일이나 시스템 API를 사용해야 함

#ifdef ANDROID_PLATFORM
    // Android thermal zone 파일 읽기 시도
    FILE* thermal = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (thermal) {
        int temp_millidegree;
        if (fscanf(thermal, "%d", &temp_millidegree) == 1) {
            *temperature = temp_millidegree / 1000.0f;
        } else {
            *temperature = 45.0f;  // 기본값
        }
        fclose(thermal);
    } else {
        *temperature = 45.0f;  // 기본값
    }

#elif defined(IOS_PLATFORM)
    // iOS에서는 공개된 온도 API가 없으므로 추정값 사용
    *temperature = 40.0f + (rand() % 20);  // 40-60도 범위

#else
    *temperature = 45.0f;  // 기본값
#endif

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 플랫폼별 특화 함수들
// ============================================================================

#ifdef ANDROID_PLATFORM
int mobile_android_apply_optimizations(LibEtudeEngine* engine, const MobileOptimizationConfig* config) {
    if (!engine || !config) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // Android 특화 최적화 적용
    // 예: ART 힙 크기 조정, JNI 최적화 등

    return LIBETUDE_SUCCESS;
}

int mobile_android_handle_trim_memory(LibEtudeEngine* engine, int trim_level) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // Android trim memory 레벨에 따른 처리
    switch (trim_level) {
        case 80:  // TRIM_MEMORY_COMPLETE
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
            break;
        case 60:  // TRIM_MEMORY_MODERATE
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
            break;
        default:
            break;
    }

    return LIBETUDE_SUCCESS;
}
#endif

#ifdef IOS_PLATFORM
int mobile_ios_apply_optimizations(LibEtudeEngine* engine, const MobileOptimizationConfig* config) {
    if (!engine || !config) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // iOS 특화 최적화 적용
    // 예: Metal 성능 셰이더, Accelerate 프레임워크 활용 등

    return LIBETUDE_SUCCESS;
}

int mobile_ios_handle_memory_warning(LibEtudeEngine* engine, int warning_level) {
    if (!engine) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // iOS 메모리 경고 레벨에 따른 처리
    switch (warning_level) {
        case 2:  // Critical memory warning
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
            break;
        case 1:  // Memory warning
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
            break;
        default:
            break;
    }

    return LIBETUDE_SUCCESS;
}
#endif