/**
 * @file macos_system.c
 * @brief macOS 시스템 정보 구현체
 * @author LibEtude Project
 * @version 1.0.0
 */

#ifdef __APPLE__

#include "libetude/platform/system.h"
#include "libetude/memory.h"
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/vm_statistics.h>
#include <mach/host_info.h>
#include <mach/processor_info.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// macOS 플랫폼 데이터 구조체
// ============================================================================

typedef struct {
    mach_timebase_info_data_t timebase_info; /**< Mach 타이머 기준 정보 */
    uint64_t process_start_time;              /**< 프로세스 시작 시간 */
    bool timing_initialized;                  /**< 타이밍 정보 초기화 상태 */
    host_t host_port;                        /**< Mach 호스트 포트 */
} ETMacOSSystemData;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult macos_get_system_info(ETSystemInfo* info);
static ETResult macos_get_memory_info(ETMemoryInfo* info);
static ETResult macos_get_cpu_info(ETCPUInfo* info);
static ETResult macos_get_high_resolution_time(uint64_t* time_ns);
static ETResult macos_sleep(uint32_t milliseconds);
static ETResult macos_get_timer_frequency(uint64_t* frequency);
static uint32_t macos_get_simd_features(void);
static bool macos_has_feature(ETHardwareFeature feature);
static ETResult macos_detect_hardware_capabilities(uint32_t* capabilities);
static ETResult macos_get_cpu_usage(float* usage_percent);
static ETResult macos_get_memory_usage(ETMemoryUsage* usage);
static ETResult macos_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage);
static ETResult macos_get_system_uptime(uint64_t* uptime_seconds);
static ETResult macos_get_process_uptime(uint64_t* uptime_seconds);

static ETResult macos_sysctl_get_string(const char* name, char* buffer, size_t buffer_size);
static ETResult macos_sysctl_get_uint32(const char* name, uint32_t* value);
static ETResult macos_sysctl_get_uint64(const char* name, uint64_t* value);
static uint32_t macos_detect_simd_features(void);

// ============================================================================
// 인터페이스 생성 함수
// ============================================================================

ETResult et_system_interface_create_macos(ETSystemInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETSystemInterface* sys_interface = (ETSystemInterface*)libetude_malloc(sizeof(ETSystemInterface));
    if (!sys_interface) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)libetude_malloc(sizeof(ETMacOSSystemData));
    if (!platform_data) {
        libetude_free(sys_interface);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 플랫폼 데이터 초기화
    memset(platform_data, 0, sizeof(ETMacOSSystemData));

    // Mach 타이머 기준 정보 초기화
    mach_timebase_info(&platform_data->timebase_info);
    platform_data->process_start_time = mach_absolute_time();
    platform_data->host_port = mach_host_self();
    platform_data->timing_initialized = true;

    // 인터페이스 함수 포인터 설정
    sys_interface->get_system_info = macos_get_system_info;
    sys_interface->get_memory_info = macos_get_memory_info;
    sys_interface->get_cpu_info = macos_get_cpu_info;
    sys_interface->get_high_resolution_time = macos_get_high_resolution_time;
    sys_interface->sleep = macos_sleep;
    sys_interface->get_timer_frequency = macos_get_timer_frequency;
    sys_interface->get_simd_features = macos_get_simd_features;
    sys_interface->has_feature = macos_has_feature;
    sys_interface->detect_hardware_capabilities = macos_detect_hardware_capabilities;
    sys_interface->get_cpu_usage = macos_get_cpu_usage;
    sys_interface->get_memory_usage = macos_get_memory_usage;
    sys_interface->get_process_memory_info = macos_get_process_memory_info;
    sys_interface->get_system_uptime = macos_get_system_uptime;
    sys_interface->get_process_uptime = macos_get_process_uptime;
    sys_interface->platform_data = platform_data;

    *interface = sys_interface;
    return ET_SUCCESS;
}

// ============================================================================
// 시스템 정보 수집 함수 구현
// ============================================================================

static ETResult macos_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETSystemInfo));

    // 기본 정보 설정
    info->platform_type = ET_PLATFORM_MACOS;

    // 메모리 정보
    uint64_t memory_size;
    if (macos_sysctl_get_uint64("hw.memsize", &memory_size) == ET_SUCCESS) {
        info->total_memory = memory_size;
    }

    // CPU 코어 수
    uint32_t cpu_count;
    if (macos_sysctl_get_uint32("hw.ncpu", &cpu_count) == ET_SUCCESS) {
        info->cpu_count = cpu_count;
    }

    // 아키텍처 설정
#ifdef __x86_64__
    info->architecture = ET_ARCH_X64;
#elif defined(__arm64__)
    info->architecture = ET_ARCH_ARM64;
#elif defined(__i386__)
    info->architecture = ET_ARCH_X86;
#elif defined(__arm__)
    info->architecture = ET_ARCH_ARM;
#else
    info->architecture = ET_ARCH_UNKNOWN;
#endif

    // 시스템 이름
    struct utsname uts;
    if (uname(&uts) == 0) {
        strncpy(info->system_name, uts.nodename, sizeof(info->system_name) - 1);
        strncpy(info->os_version, uts.release, sizeof(info->os_version) - 1);
    }

    // CPU 정보
    ETCPUInfo cpu_info;
    if (macos_get_cpu_info(&cpu_info) == ET_SUCCESS) {
        strncpy(info->cpu_name, cpu_info.brand, sizeof(info->cpu_name) - 1);
        info->cpu_frequency = cpu_info.base_frequency_mhz;
    }

    // 사용 가능한 메모리 (현재 여유 메모리)
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)
        et_get_system_interface()->platform_data;

    if (platform_data &&
        host_page_size(platform_data->host_port, &page_size) == KERN_SUCCESS &&
        host_statistics64(platform_data->host_port, HOST_VM_INFO64,
                         (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
        info->available_memory = (uint64_t)vm_stat.free_count * page_size;
    }

    return ET_SUCCESS;
}

static ETResult macos_get_memory_info(ETMemoryInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETMemoryInfo));

    // 총 물리 메모리
    uint64_t memory_size;
    if (macos_sysctl_get_uint64("hw.memsize", &memory_size) == ET_SUCCESS) {
        info->total_physical = memory_size;
    }

    // VM 통계 정보
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)
        et_get_system_interface()->platform_data;

    if (platform_data &&
        host_page_size(platform_data->host_port, &page_size) == KERN_SUCCESS &&
        host_statistics64(platform_data->host_port, HOST_VM_INFO64,
                         (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {

        info->page_size = page_size;
        info->allocation_granularity = page_size;
        info->available_physical = (uint64_t)vm_stat.free_count * page_size;

        // 가상 메모리는 매우 큰 값 (64비트 주소 공간)
        info->total_virtual = (uint64_t)1 << 47; // 128TB
        info->available_virtual = info->total_virtual;
    }

    return ET_SUCCESS;
}

static ETResult macos_get_cpu_info(ETCPUInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETCPUInfo));

    // CPU 브랜드 문자열
    if (macos_sysctl_get_string("machdep.cpu.brand_string", info->brand, sizeof(info->brand)) != ET_SUCCESS) {
        // Apple Silicon의 경우 다른 방법 시도
        if (macos_sysctl_get_string("hw.model", info->brand, sizeof(info->brand)) != ET_SUCCESS) {
            strcpy(info->brand, "Unknown CPU");
        }
    }

    // CPU 제조사
    if (macos_sysctl_get_string("machdep.cpu.vendor", info->vendor, sizeof(info->vendor)) != ET_SUCCESS) {
#ifdef __arm64__
        strcpy(info->vendor, "Apple");
#else
        strcpy(info->vendor, "Unknown");
#endif
    }

    // CPU 패밀리, 모델, 스테핑
    uint32_t value;
    if (macos_sysctl_get_uint32("machdep.cpu.family", &value) == ET_SUCCESS) {
        info->family = value;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.model", &value) == ET_SUCCESS) {
        info->model = value;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.stepping", &value) == ET_SUCCESS) {
        info->stepping = value;
    }

    // 코어 수
    if (macos_sysctl_get_uint32("hw.physicalcpu", &value) == ET_SUCCESS) {
        info->physical_cores = value;
    }
    if (macos_sysctl_get_uint32("hw.logicalcpu", &value) == ET_SUCCESS) {
        info->logical_cores = value;
    }

    // 캐시 정보
    if (macos_sysctl_get_uint32("hw.cachelinesize", &value) == ET_SUCCESS) {
        info->cache_line_size = value;
    }
    if (macos_sysctl_get_uint32("hw.l1icachesize", &value) == ET_SUCCESS) {
        info->l1_cache_size = value / 1024; // 바이트를 KB로 변환
    }
    if (macos_sysctl_get_uint32("hw.l2cachesize", &value) == ET_SUCCESS) {
        info->l2_cache_size = value / 1024;
    }
    if (macos_sysctl_get_uint32("hw.l3cachesize", &value) == ET_SUCCESS) {
        info->l3_cache_size = value / 1024;
    }

    // 주파수 정보
    uint64_t freq_hz;
    if (macos_sysctl_get_uint64("hw.cpufrequency", &freq_hz) == ET_SUCCESS) {
        info->base_frequency_mhz = (uint32_t)(freq_hz / 1000000);
        info->max_frequency_mhz = info->base_frequency_mhz;
    } else if (macos_sysctl_get_uint64("hw.cpufrequency_max", &freq_hz) == ET_SUCCESS) {
        info->max_frequency_mhz = (uint32_t)(freq_hz / 1000000);
        info->base_frequency_mhz = info->max_frequency_mhz;
    }

    // 기본값 설정
    if (info->cache_line_size == 0) {
        info->cache_line_size = 64;
    }
    if (info->l1_cache_size == 0) {
        info->l1_cache_size = 32; // KB
    }
    if (info->l2_cache_size == 0) {
        info->l2_cache_size = 256; // KB
    }

    return ET_SUCCESS;
}

// ============================================================================
// 타이머 관련 함수 구현
// ============================================================================

static ETResult macos_get_high_resolution_time(uint64_t* time_ns) {
    if (!time_ns) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)
        et_get_system_interface()->platform_data;

    if (!platform_data || !platform_data->timing_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    uint64_t absolute_time = mach_absolute_time();
    *time_ns = absolute_time * platform_data->timebase_info.numer / platform_data->timebase_info.denom;

    return ET_SUCCESS;
}

static ETResult macos_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;

    if (nanosleep(&ts, NULL) != 0) {
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

static ETResult macos_get_timer_frequency(uint64_t* frequency) {
    if (!frequency) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)
        et_get_system_interface()->platform_data;

    if (!platform_data || !platform_data->timing_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    // Mach absolute time의 주파수는 timebase_info로 계산
    *frequency = 1000000000ULL * platform_data->timebase_info.denom / platform_data->timebase_info.numer;

    return ET_SUCCESS;
}

// ============================================================================
// SIMD 기능 감지 함수 구현
// ============================================================================

static uint32_t macos_get_simd_features(void) {
    return macos_detect_simd_features();
}

static bool macos_has_feature(ETHardwareFeature feature) {
    switch (feature) {
        case ET_HW_FEATURE_SIMD:
            return macos_get_simd_features() != ET_SIMD_NONE;
        case ET_HW_FEATURE_HIGH_RES_TIMER:
            return true; // macOS는 항상 고해상도 타이머 지원
        case ET_HW_FEATURE_GPU:
            // GPU 감지는 별도 구현 필요
            return false;
        case ET_HW_FEATURE_AUDIO_HW:
            // 오디오 하드웨어 감지는 별도 구현 필요
            return false;
        default:
            return false;
    }
}

static ETResult macos_detect_hardware_capabilities(uint32_t* capabilities) {
    if (!capabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *capabilities = 0;

    if (macos_has_feature(ET_HW_FEATURE_SIMD)) {
        *capabilities |= ET_HW_FEATURE_SIMD;
    }
    if (macos_has_feature(ET_HW_FEATURE_HIGH_RES_TIMER)) {
        *capabilities |= ET_HW_FEATURE_HIGH_RES_TIMER;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 성능 모니터링 함수 구현
// ============================================================================

static ETResult macos_get_cpu_usage(float* usage_percent) {
    if (!usage_percent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    host_cpu_load_info_data_t cpu_info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)
        et_get_system_interface()->platform_data;

    if (!platform_data) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    if (host_statistics(platform_data->host_port, HOST_CPU_LOAD_INFO,
                       (host_info_t)&cpu_info, &count) != KERN_SUCCESS) {
        return ET_ERROR_SYSTEM;
    }

    static natural_t prev_user = 0, prev_system = 0, prev_idle = 0, prev_nice = 0;

    natural_t user_diff = cpu_info.cpu_ticks[CPU_STATE_USER] - prev_user;
    natural_t system_diff = cpu_info.cpu_ticks[CPU_STATE_SYSTEM] - prev_system;
    natural_t idle_diff = cpu_info.cpu_ticks[CPU_STATE_IDLE] - prev_idle;
    natural_t nice_diff = cpu_info.cpu_ticks[CPU_STATE_NICE] - prev_nice;

    natural_t total_diff = user_diff + system_diff + idle_diff + nice_diff;

    if (total_diff > 0) {
        *usage_percent = 100.0f * (float)(user_diff + system_diff + nice_diff) / (float)total_diff;
    } else {
        *usage_percent = 0.0f;
    }

    prev_user = cpu_info.cpu_ticks[CPU_STATE_USER];
    prev_system = cpu_info.cpu_ticks[CPU_STATE_SYSTEM];
    prev_idle = cpu_info.cpu_ticks[CPU_STATE_IDLE];
    prev_nice = cpu_info.cpu_ticks[CPU_STATE_NICE];

    return ET_SUCCESS;
}

static ETResult macos_get_memory_usage(ETMemoryUsage* usage) {
    if (!usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(usage, 0, sizeof(ETMemoryUsage));

    // 프로세스 메모리 정보
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        usage->process_memory_usage = ru.ru_maxrss; // macOS에서는 바이트 단위
        usage->process_peak_memory = ru.ru_maxrss;
    }

    // 시스템 메모리 사용률
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)
        et_get_system_interface()->platform_data;

    if (platform_data &&
        host_page_size(platform_data->host_port, &page_size) == KERN_SUCCESS &&
        host_statistics64(platform_data->host_port, HOST_VM_INFO64,
                         (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {

        uint64_t total_memory;
        if (macos_sysctl_get_uint64("hw.memsize", &total_memory) == ET_SUCCESS) {
            uint64_t used_memory = (vm_stat.active_count + vm_stat.inactive_count +
                                   vm_stat.wire_count) * page_size;
            usage->memory_usage_percent = 100.0f * (float)used_memory / (float)total_memory;
        }
    }

    // CPU 사용률
    macos_get_cpu_usage(&usage->cpu_usage_percent);

    return ET_SUCCESS;
}

static ETResult macos_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage) {
    if (!current_usage || !peak_usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) {
        return ET_ERROR_SYSTEM;
    }

    *current_usage = ru.ru_maxrss; // macOS에서는 바이트 단위
    *peak_usage = ru.ru_maxrss;

    return ET_SUCCESS;
}

static ETResult macos_get_system_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct timeval boottime;
    size_t size = sizeof(boottime);

    if (sysctlbyname("kern.boottime", &boottime, &size, NULL, 0) != 0) {
        return ET_ERROR_SYSTEM;
    }

    struct timeval now;
    gettimeofday(&now, NULL);

    *uptime_seconds = now.tv_sec - boottime.tv_sec;
    return ET_SUCCESS;
}

static ETResult macos_get_process_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSystemData* platform_data = (ETMacOSSystemData*)
        et_get_system_interface()->platform_data;

    if (!platform_data || !platform_data->timing_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    uint64_t current_time = mach_absolute_time();
    uint64_t elapsed_ns = (current_time - platform_data->process_start_time) *
                         platform_data->timebase_info.numer / platform_data->timebase_info.denom;

    *uptime_seconds = elapsed_ns / 1000000000ULL;
    return ET_SUCCESS;
}

// ============================================================================
// 헬퍼 함수 구현
// ============================================================================

static ETResult macos_sysctl_get_string(const char* name, char* buffer, size_t buffer_size) {
    if (!name || !buffer || buffer_size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t size = buffer_size - 1;
    if (sysctlbyname(name, buffer, &size, NULL, 0) != 0) {
        return ET_ERROR_SYSTEM;
    }

    buffer[size] = '\0';
    return ET_SUCCESS;
}

static ETResult macos_sysctl_get_uint32(const char* name, uint32_t* value) {
    if (!name || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t size = sizeof(*value);
    if (sysctlbyname(name, value, &size, NULL, 0) != 0) {
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

static ETResult macos_sysctl_get_uint64(const char* name, uint64_t* value) {
    if (!name || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t size = sizeof(*value);
    if (sysctlbyname(name, value, &size, NULL, 0) != 0) {
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

static uint32_t macos_detect_simd_features(void) {
    uint32_t features = ET_SIMD_NONE;

#ifdef __x86_64__
    // Intel/AMD CPU의 경우 sysctl을 통해 확인
    uint32_t has_feature;

    if (macos_sysctl_get_uint32("machdep.cpu.features", &has_feature) == ET_SUCCESS) {
        // macOS의 CPU 기능 플래그는 다른 형식을 사용할 수 있음
        // 여기서는 기본적인 확인만 수행
        features |= ET_SIMD_SSE; // 현대 macOS는 최소 SSE 지원
        features |= ET_SIMD_SSE2;
    }

    // 개별 기능 확인
    if (macos_sysctl_get_uint32("machdep.cpu.feature.SSE3", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_SSE3;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.feature.SSSE3", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_SSSE3;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.feature.SSE4_1", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_SSE4_1;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.feature.SSE4_2", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_SSE4_2;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.feature.AVX1_0", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_AVX;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.feature.AVX2_0", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_AVX2;
    }
    if (macos_sysctl_get_uint32("machdep.cpu.feature.FMA", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_FMA;
    }

#elif defined(__arm64__)
    // Apple Silicon의 경우 NEON은 항상 지원
    features |= ET_SIMD_NEON;

    // Apple Silicon 특화 기능들은 추가 확인 필요
    uint32_t has_feature;
    if (macos_sysctl_get_uint32("hw.optional.neon", &has_feature) == ET_SUCCESS && has_feature) {
        features |= ET_SIMD_NEON;
    }
#endif

    return features;
}

#endif // __APPLE__