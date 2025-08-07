/**
 * @file runtime_adaptation.c
 * @brief 런타임 기능 감지 및 적응 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/runtime_adaptation.h"
#include "libetude/platform/common.h"
#include "libetude/platform/system.h"
#include "libetude/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef LIBETUDE_PLATFORM_WINDOWS
    #include <windows.h>
    #include <intrin.h>
    #include <powrprof.h>
#elif defined(LIBETUDE_PLATFORM_LINUX)
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <cpuid.h>
#elif defined(LIBETUDE_PLATFORM_MACOS)
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <IOKit/ps/IOPowerSources.h>
#endif

// ============================================================================
// 전역 변수
// ============================================================================

static ETHardwareCapabilities g_cached_capabilities = {0};
static bool g_capabilities_cached = false;
static ETDispatchTable g_dispatch_table = {0};
static ETRuntimeAdaptationConfig g_adaptation_config = {0};
static bool g_adaptation_initialized = false;
static bool g_adaptation_running = false;

// 성능 메트릭 저장소 (간단한 해시 테이블)
#define MAX_PERFORMANCE_ENTRIES 256
static ETPerformanceMetrics g_performance_metrics[MAX_PERFORMANCE_ENTRIES];
static uint64_t g_profile_start_times[MAX_PERFORMANCE_ENTRIES];
static char g_metric_names[MAX_PERFORMANCE_ENTRIES][64];
static int g_metric_count = 0;

// ============================================================================
// 내부 함수들
// ============================================================================

/**
 * @brief 고해상도 타이머를 사용하여 현재 시간을 가져옵니다
 */
static uint64_t get_high_resolution_time(void) {
#ifdef LIBETUDE_PLATFORM_WINDOWS
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (uint64_t)(counter.QuadPart * 1000000000ULL / frequency.QuadPart);
#elif defined(LIBETUDE_PLATFORM_LINUX) || defined(LIBETUDE_PLATFORM_MACOS)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#else
    return 0;
#endif
}

/**
 * @brief CPU 기능을 감지합니다
 */
static void detect_cpu_features(ETHardwareCapabilities* caps) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // x86/x64 CPUID 사용
    #ifdef LIBETUDE_PLATFORM_WINDOWS
        int cpuinfo[4];
        __cpuid(cpuinfo, 1);

        caps->has_sse = (cpuinfo[3] & (1 << 25)) != 0;
        caps->has_sse2 = (cpuinfo[3] & (1 << 26)) != 0;
        caps->has_sse3 = (cpuinfo[2] & (1 << 0)) != 0;
        caps->has_ssse3 = (cpuinfo[2] & (1 << 9)) != 0;
        caps->has_sse4_1 = (cpuinfo[2] & (1 << 19)) != 0;
        caps->has_sse4_2 = (cpuinfo[2] & (1 << 20)) != 0;
        caps->has_avx = (cpuinfo[2] & (1 << 28)) != 0;
        caps->has_fma = (cpuinfo[2] & (1 << 12)) != 0;

        // AVX2 확인
        __cpuid(cpuinfo, 7);
        caps->has_avx2 = (cpuinfo[1] & (1 << 5)) != 0;
        caps->has_avx512 = (cpuinfo[1] & (1 << 16)) != 0;
    #elif defined(LIBETUDE_PLATFORM_LINUX) || defined(LIBETUDE_PLATFORM_MACOS)
        unsigned int eax, ebx, ecx, edx;

        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            caps->has_sse = (edx & (1 << 25)) != 0;
            caps->has_sse2 = (edx & (1 << 26)) != 0;
            caps->has_sse3 = (ecx & (1 << 0)) != 0;
            caps->has_ssse3 = (ecx & (1 << 9)) != 0;
            caps->has_sse4_1 = (ecx & (1 << 19)) != 0;
            caps->has_sse4_2 = (ecx & (1 << 20)) != 0;
            caps->has_avx = (ecx & (1 << 28)) != 0;
            caps->has_fma = (ecx & (1 << 12)) != 0;
        }

        if (__get_cpuid(7, &eax, &ebx, &ecx, &edx)) {
            caps->has_avx2 = (ebx & (1 << 5)) != 0;
            caps->has_avx512 = (ebx & (1 << 16)) != 0;
        }
    #endif
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
    // ARM NEON 감지
    #ifdef LIBETUDE_PLATFORM_LINUX
        // /proc/cpuinfo에서 NEON 지원 확인
        FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
        if (cpuinfo) {
            char line[256];
            while (fgets(line, sizeof(line), cpuinfo)) {
                if (strstr(line, "neon") || strstr(line, "asimd")) {
                    caps->has_neon = true;
                    break;
                }
            }
            fclose(cpuinfo);
        }
    #elif defined(LIBETUDE_PLATFORM_MACOS)
        // macOS에서 sysctl 사용
        size_t size = sizeof(int);
        int has_neon = 0;
        if (sysctlbyname("hw.optional.neon", &has_neon, &size, NULL, 0) == 0) {
            caps->has_neon = (has_neon != 0);
        }
    #endif
#endif
}

/**
 * @brief GPU 기능을 감지합니다
 */
static void detect_gpu_features(ETHardwareCapabilities* caps) {
    // GPU 기능 감지는 플랫폼별로 구현
    caps->has_cuda = false;
    caps->has_opencl = false;
    caps->has_metal = false;
    caps->has_vulkan = false;

#ifdef LIBETUDE_PLATFORM_WINDOWS
    // Windows에서 DirectX/CUDA 확인
    // 실제 구현에서는 해당 라이브러리 로드 시도
#elif defined(LIBETUDE_PLATFORM_MACOS)
    // macOS에서 Metal 지원 확인
    caps->has_metal = true; // macOS 10.11+ 에서는 기본 지원
#elif defined(LIBETUDE_PLATFORM_LINUX)
    // Linux에서 OpenCL/CUDA 확인
    // 실제 구현에서는 해당 라이브러리 로드 시도
#endif
}

/**
 * @brief 시스템 정보를 감지합니다
 */
static void detect_system_info(ETHardwareCapabilities* caps) {
#ifdef LIBETUDE_PLATFORM_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    caps->cpu_count = sysinfo.dwNumberOfProcessors;

    MEMORYSTATUSEX memstat;
    memstat.dwLength = sizeof(memstat);
    GlobalMemoryStatusEx(&memstat);
    caps->total_memory = memstat.ullTotalPhys;
    caps->available_memory = memstat.ullAvailPhys;

    // CPU 브랜드 정보
    int cpuinfo[4];
    __cpuid(cpuinfo, 0x80000002);
    memcpy(caps->cpu_brand, cpuinfo, 16);
    __cpuid(cpuinfo, 0x80000003);
    memcpy(caps->cpu_brand + 16, cpuinfo, 16);
    __cpuid(cpuinfo, 0x80000004);
    memcpy(caps->cpu_brand + 32, cpuinfo, 16);
    caps->cpu_brand[48] = '\0';

#elif defined(LIBETUDE_PLATFORM_LINUX)
    caps->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    caps->physical_cpu_count = sysconf(_SC_NPROCESSORS_CONF);

    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        caps->total_memory = info.totalram * info.mem_unit;
        caps->available_memory = info.freeram * info.mem_unit;
    }

    // /proc/cpuinfo에서 CPU 정보 읽기
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    strncpy(caps->cpu_brand, colon + 2, sizeof(caps->cpu_brand) - 1);
                    caps->cpu_brand[sizeof(caps->cpu_brand) - 1] = '\0';
                    // 개행 문자 제거
                    char* newline = strchr(caps->cpu_brand, '\n');
                    if (newline) *newline = '\0';
                    break;
                }
            }
        }
        fclose(cpuinfo);
    }

#elif defined(LIBETUDE_PLATFORM_MACOS)
    size_t size = sizeof(uint32_t);
    sysctlbyname("hw.ncpu", &caps->cpu_count, &size, NULL, 0);
    sysctlbyname("hw.physicalcpu", &caps->physical_cpu_count, &size, NULL, 0);

    size = sizeof(uint64_t);
    sysctlbyname("hw.memsize", &caps->total_memory, &size, NULL, 0);

    // 사용 가능한 메모리는 vm_stat 사용 (간단히 총 메모리의 일부로 추정)
    caps->available_memory = caps->total_memory / 2;

    // CPU 브랜드 정보
    size = sizeof(caps->cpu_brand);
    sysctlbyname("machdep.cpu.brand_string", caps->cpu_brand, &size, NULL, 0);
#endif

    // 캐시 정보 (기본값 설정)
    caps->l1_cache_size = 32 * 1024;    // 32KB
    caps->l2_cache_size = 256 * 1024;   // 256KB
    caps->l3_cache_size = 8 * 1024 * 1024; // 8MB
    caps->cache_line_size = 64;         // 64 bytes

    // 기타 기능
    caps->has_high_res_timer = true;
    caps->has_rdtsc = true;
    caps->has_thermal_sensors = false;
    caps->has_power_management = false;

#ifdef LIBETUDE_PLATFORM_WINDOWS
    caps->has_thermal_sensors = true;
    caps->has_power_management = true;
#elif defined(LIBETUDE_PLATFORM_LINUX)
    // /sys/class/thermal 확인
    caps->has_thermal_sensors = (access("/sys/class/thermal", F_OK) == 0);
    caps->has_power_management = (access("/sys/class/power_supply", F_OK) == 0);
#elif defined(LIBETUDE_PLATFORM_MACOS)
    caps->has_thermal_sensors = true;
    caps->has_power_management = true;
#endif
}

/**
 * @brief 메트릭 이름으로 인덱스를 찾습니다
 */
static int find_metric_index(const char* name) {
    for (int i = 0; i < g_metric_count; i++) {
        if (strcmp(g_metric_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 새로운 메트릭을 생성합니다
 */
static int create_metric(const char* name) {
    if (g_metric_count >= MAX_PERFORMANCE_ENTRIES) {
        return -1;
    }

    int index = g_metric_count++;
    strncpy(g_metric_names[index], name, sizeof(g_metric_names[0]) - 1);
    g_metric_names[index][sizeof(g_metric_names[0]) - 1] = '\0';

    memset(&g_performance_metrics[index], 0, sizeof(ETPerformanceMetrics));
    g_performance_metrics[index].name = g_metric_names[index];
    g_performance_metrics[index].min_time_ns = UINT64_MAX;

    return index;
}

// ============================================================================
// 하드웨어 기능 감지 구현
// ============================================================================

ETResult et_detect_hardware_capabilities(ETHardwareCapabilities* capabilities) {
    if (!capabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(capabilities, 0, sizeof(ETHardwareCapabilities));

    // CPU 기능 감지
    detect_cpu_features(capabilities);

    // GPU 기능 감지
    detect_gpu_features(capabilities);

    // 시스템 정보 감지
    detect_system_info(capabilities);

    // 감지 시간 기록
    capabilities->detection_timestamp = get_high_resolution_time();
    capabilities->is_cached = false;

    // 전역 캐시에 저장
    memcpy(&g_cached_capabilities, capabilities, sizeof(ETHardwareCapabilities));
    g_cached_capabilities.is_cached = true;
    g_capabilities_cached = true;

    return ET_SUCCESS;
}

ETResult et_get_cached_hardware_capabilities(ETHardwareCapabilities* capabilities) {
    if (!capabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!g_capabilities_cached) {
        return et_detect_hardware_capabilities(capabilities);
    }

    memcpy(capabilities, &g_cached_capabilities, sizeof(ETHardwareCapabilities));
    return ET_SUCCESS;
}

void et_invalidate_hardware_cache(void) {
    g_capabilities_cached = false;
    memset(&g_cached_capabilities, 0, sizeof(ETHardwareCapabilities));
}

bool et_runtime_has_feature(ETHardwareFeature feature) {
    ETHardwareCapabilities caps;
    if (et_get_cached_hardware_capabilities(&caps) != ET_SUCCESS) {
        return false;
    }

    switch (feature) {
        case ET_FEATURE_SSE: return caps.has_sse;
        case ET_FEATURE_SSE2: return caps.has_sse2;
        case ET_FEATURE_SSE3: return caps.has_sse3;
        case ET_FEATURE_SSSE3: return caps.has_ssse3;
        case ET_FEATURE_SSE4_1: return caps.has_sse4_1;
        case ET_FEATURE_SSE4_2: return caps.has_sse4_2;
        case ET_FEATURE_AVX: return caps.has_avx;
        case ET_FEATURE_AVX2: return caps.has_avx2;
        case ET_FEATURE_AVX512: return caps.has_avx512;
        case ET_FEATURE_NEON: return caps.has_neon;
        case ET_FEATURE_FMA: return caps.has_fma;
        case ET_FEATURE_GPU: return caps.has_cuda || caps.has_opencl || caps.has_metal;
        case ET_FEATURE_HIGH_RES_TIMER: return caps.has_high_res_timer;
        default: return false;
    }
}

// ============================================================================
// 동적 함수 디스패치 구현
// ============================================================================

ETResult et_dispatch_initialize(void) {
    if (g_dispatch_table.is_initialized) {
        return ET_SUCCESS;
    }

    g_dispatch_table.entries = malloc(sizeof(ETDispatchEntry) * 256);
    if (!g_dispatch_table.entries) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    g_dispatch_table.entry_count = 0;
    g_dispatch_table.is_initialized = true;

    return ET_SUCCESS;
}

ETResult et_dispatch_register_function(const char* name, const ETDispatchEntry* entry) {
    if (!name || !entry || !g_dispatch_table.is_initialized) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (g_dispatch_table.entry_count >= 256) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    ETDispatchEntry* new_entry = &g_dispatch_table.entries[g_dispatch_table.entry_count++];
    memcpy(new_entry, entry, sizeof(ETDispatchEntry));

    // 함수 이름 복사
    new_entry->function_name = malloc(strlen(name) + 1);
    if (!new_entry->function_name) {
        g_dispatch_table.entry_count--;
        return ET_ERROR_OUT_OF_MEMORY;
    }
    strcpy((char*)new_entry->function_name, name);

    return ET_SUCCESS;
}

ETGenericFunction et_dispatch_select_function(const char* name) {
    if (!name || !g_dispatch_table.is_initialized) {
        return NULL;
    }

    // 등록된 함수 찾기
    ETDispatchEntry* entry = NULL;
    for (size_t i = 0; i < g_dispatch_table.entry_count; i++) {
        if (strcmp(g_dispatch_table.entries[i].function_name, name) == 0) {
            entry = &g_dispatch_table.entries[i];
            break;
        }
    }

    if (!entry) {
        return NULL;
    }

    // 이미 선택된 구현이 있으면 반환
    if (entry->selected_impl) {
        return entry->selected_impl;
    }

    // 하드웨어 기능에 따라 최적화된 구현 선택
    ETHardwareCapabilities caps;
    if (et_get_cached_hardware_capabilities(&caps) != ET_SUCCESS) {
        entry->selected_impl = entry->generic_impl;
        return entry->selected_impl;
    }

    // 가장 고급 기능부터 확인
    if (entry->avx2_impl && caps.has_avx2) {
        entry->selected_impl = entry->avx2_impl;
    } else if (entry->avx_impl && caps.has_avx) {
        entry->selected_impl = entry->avx_impl;
    } else if (entry->sse2_impl && caps.has_sse2) {
        entry->selected_impl = entry->sse2_impl;
    } else if (entry->sse_impl && caps.has_sse) {
        entry->selected_impl = entry->sse_impl;
    } else if (entry->neon_impl && caps.has_neon) {
        entry->selected_impl = entry->neon_impl;
    } else if (entry->gpu_impl && (caps.has_cuda || caps.has_opencl || caps.has_metal)) {
        entry->selected_impl = entry->gpu_impl;
    } else {
        entry->selected_impl = entry->generic_impl;
    }

    return entry->selected_impl;
}

ETResult et_dispatch_select_all_functions(void) {
    if (!g_dispatch_table.is_initialized) {
        return ET_ERROR_INVALID_STATE;
    }

    for (size_t i = 0; i < g_dispatch_table.entry_count; i++) {
        et_dispatch_select_function(g_dispatch_table.entries[i].function_name);
    }

    return ET_SUCCESS;
}

void et_dispatch_finalize(void) {
    if (!g_dispatch_table.is_initialized) {
        return;
    }

    for (size_t i = 0; i < g_dispatch_table.entry_count; i++) {
        free((void*)g_dispatch_table.entries[i].function_name);
    }

    free(g_dispatch_table.entries);
    memset(&g_dispatch_table, 0, sizeof(ETDispatchTable));
}

// ============================================================================
// 성능 프로파일링 구현
// ============================================================================

ETResult et_profiling_begin(const char* name) {
    if (!name) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int index = find_metric_index(name);
    if (index == -1) {
        index = create_metric(name);
        if (index == -1) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
    }

    g_profile_start_times[index] = get_high_resolution_time();
    return ET_SUCCESS;
}

ETResult et_profiling_end(const char* name) {
    uint64_t end_time = get_high_resolution_time();

    if (!name) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int index = find_metric_index(name);
    if (index == -1) {
        return ET_ERROR_NOT_FOUND;
    }

    uint64_t elapsed = end_time - g_profile_start_times[index];
    ETPerformanceMetrics* metrics = &g_performance_metrics[index];

    metrics->total_time_ns += elapsed;
    metrics->call_count++;

    if (elapsed < metrics->min_time_ns) {
        metrics->min_time_ns = elapsed;
    }
    if (elapsed > metrics->max_time_ns) {
        metrics->max_time_ns = elapsed;
    }

    metrics->average_time_ns = (double)metrics->total_time_ns / metrics->call_count;

    return ET_SUCCESS;
}

ETResult et_profiling_get_metrics(const char* name, ETPerformanceMetrics* metrics) {
    if (!name || !metrics) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int index = find_metric_index(name);
    if (index == -1) {
        return ET_ERROR_NOT_FOUND;
    }

    memcpy(metrics, &g_performance_metrics[index], sizeof(ETPerformanceMetrics));
    return ET_SUCCESS;
}

void et_profiling_reset_all_metrics(void) {
    for (int i = 0; i < g_metric_count; i++) {
        memset(&g_performance_metrics[i], 0, sizeof(ETPerformanceMetrics));
        g_performance_metrics[i].name = g_metric_names[i];
        g_performance_metrics[i].min_time_ns = UINT64_MAX;
    }
}

// ============================================================================
// 적응적 최적화 구현 (기본 구현)
// ============================================================================

ETResult et_adaptive_optimization_start(const ETAdaptiveOptimizationConfig* config) {
    if (!config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 설정 저장
    memcpy(&g_adaptation_config.optimization_config, config, sizeof(ETAdaptiveOptimizationConfig));

    return ET_SUCCESS;
}

void et_adaptive_optimization_stop(void) {
    // 적응적 최적화 중지
}

ETResult et_adaptive_optimization_update(void) {
    // 현재 성능 상태 분석 및 최적화 수행
    return ET_SUCCESS;
}

// ============================================================================
// 열 관리 구현 (기본 구현)
// ============================================================================

ETResult et_thermal_get_temperature(ETTemperatureSensorType sensor_type, ETTemperatureInfo* temp_info) {
    if (!temp_info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(temp_info, 0, sizeof(ETTemperatureInfo));
    temp_info->timestamp = get_high_resolution_time();

    // 플랫폼별 온도 감지 구현 (기본값 설정)
    temp_info->current_temp_celsius = 45.0f;  // 기본 온도
    temp_info->max_temp_celsius = 85.0f;
    temp_info->critical_temp_celsius = 95.0f;
    temp_info->is_overheating = false;
    temp_info->is_throttling = false;

    return ET_SUCCESS;
}

ETResult et_thermal_management_start(const ETThermalManagementConfig* config) {
    if (!config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memcpy(&g_adaptation_config.thermal_config, config, sizeof(ETThermalManagementConfig));
    return ET_SUCCESS;
}

void et_thermal_management_stop(void) {
    // 열 관리 중지
}

ETResult et_thermal_management_update(void) {
    // 열 상태 확인 및 조치
    return ET_SUCCESS;
}

// ============================================================================
// 전력 관리 구현 (기본 구현)
// ============================================================================

ETResult et_power_get_info(ETPowerInfo* power_info) {
    if (!power_info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(power_info, 0, sizeof(ETPowerInfo));
    power_info->timestamp = get_high_resolution_time();

    // 기본값 설정
    power_info->current_power_watts = 15.0f;
    power_info->average_power_watts = 12.0f;
    power_info->battery_level_percent = 75.0f;
    power_info->is_charging = false;
    power_info->is_low_battery = false;
    power_info->estimated_runtime_minutes = 240;
    power_info->current_state = ET_POWER_STATE_BALANCED;

    return ET_SUCCESS;
}

ETResult et_power_set_state(ETPowerState state) {
    // 전력 상태 설정
    return ET_SUCCESS;
}

ETResult et_power_management_start(const ETPowerManagementConfig* config) {
    if (!config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memcpy(&g_adaptation_config.power_config, config, sizeof(ETPowerManagementConfig));
    return ET_SUCCESS;
}

void et_power_management_stop(void) {
    // 전력 관리 중지
}

ETResult et_power_management_update(void) {
    // 전력 상태 확인 및 조치
    return ET_SUCCESS;
}

// ============================================================================
// 통합 런타임 적응 시스템 구현
// ============================================================================

ETResult et_runtime_adaptation_initialize(const ETRuntimeAdaptationConfig* config) {
    if (!config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (g_adaptation_initialized) {
        return ET_SUCCESS;
    }

    // 설정 복사
    memcpy(&g_adaptation_config, config, sizeof(ETRuntimeAdaptationConfig));

    // 하드웨어 기능 감지
    ETHardwareCapabilities caps;
    ETResult result = et_detect_hardware_capabilities(&caps);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 동적 디스패치 시스템 초기화
    result = et_dispatch_initialize();
    if (result != ET_SUCCESS) {
        return result;
    }

    g_adaptation_initialized = true;

    printf("[INFO] 런타임 적응 시스템 초기화 완료\n");
    printf("  - CPU: %s\n", caps.cpu_brand);
    printf("  - 코어 수: %u\n", caps.cpu_count);
    printf("  - 메모리: %.1f GB\n", (double)caps.total_memory / (1024*1024*1024));
    printf("  - SIMD 지원: SSE=%s, AVX=%s, AVX2=%s, NEON=%s\n",
           caps.has_sse ? "예" : "아니오",
           caps.has_avx ? "예" : "아니오",
           caps.has_avx2 ? "예" : "아니오",
           caps.has_neon ? "예" : "아니오");

    return ET_SUCCESS;
}

ETResult et_runtime_adaptation_start(void) {
    if (!g_adaptation_initialized) {
        return ET_ERROR_INVALID_STATE;
    }

    if (g_adaptation_running) {
        return ET_SUCCESS;
    }

    // 각 서브시스템 시작
    if (g_adaptation_config.enable_performance_profiling) {
        et_adaptive_optimization_start(&g_adaptation_config.optimization_config);
    }

    if (g_adaptation_config.enable_thermal_management) {
        et_thermal_management_start(&g_adaptation_config.thermal_config);
    }

    if (g_adaptation_config.enable_power_management) {
        et_power_management_start(&g_adaptation_config.power_config);
    }

    // 모든 함수의 최적화된 구현 선택
    et_dispatch_select_all_functions();

    g_adaptation_running = true;

    printf("[INFO] 런타임 적응 시스템 시작됨\n");

    return ET_SUCCESS;
}

ETResult et_runtime_adaptation_update(void) {
    if (!g_adaptation_running) {
        return ET_ERROR_INVALID_STATE;
    }

    // 각 서브시스템 업데이트
    if (g_adaptation_config.enable_performance_profiling) {
        et_adaptive_optimization_update();
    }

    if (g_adaptation_config.enable_thermal_management) {
        et_thermal_management_update();
    }

    if (g_adaptation_config.enable_power_management) {
        et_power_management_update();
    }

    return ET_SUCCESS;
}

void et_runtime_adaptation_stop(void) {
    if (!g_adaptation_running) {
        return;
    }

    et_adaptive_optimization_stop();
    et_thermal_management_stop();
    et_power_management_stop();

    g_adaptation_running = false;

    printf("[INFO] 런타임 적응 시스템 중지됨\n");
}

void et_runtime_adaptation_finalize(void) {
    if (!g_adaptation_initialized) {
        return;
    }

    et_runtime_adaptation_stop();
    et_dispatch_finalize();
    et_invalidate_hardware_cache();

    g_adaptation_initialized = false;

    printf("[INFO] 런타임 적응 시스템 정리 완료\n");
}

ETResult et_runtime_adaptation_get_status(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!g_adaptation_initialized) {
        snprintf(buffer, buffer_size, "런타임 적응 시스템: 초기화되지 않음");
        return ET_SUCCESS;
    }

    ETHardwareCapabilities caps;
    et_get_cached_hardware_capabilities(&caps);

    ETTemperatureInfo temp_info;
    et_thermal_get_temperature(ET_TEMP_SENSOR_CPU, &temp_info);

    ETPowerInfo power_info;
    et_power_get_info(&power_info);

    int written = snprintf(buffer, buffer_size,
        "런타임 적응 시스템 상태:\n"
        "  상태: %s\n"
        "  CPU: %s (%u 코어)\n"
        "  메모리: %.1f GB / %.1f GB\n"
        "  온도: %.1f°C\n"
        "  전력: %.1fW (배터리: %.1f%%)\n"
        "  SIMD: SSE=%s, AVX=%s, AVX2=%s, NEON=%s\n"
        "  등록된 함수: %zu개\n"
        "  성능 메트릭: %d개\n",
        g_adaptation_running ? "실행 중" : "중지됨",
        caps.cpu_brand, caps.cpu_count,
        (double)caps.available_memory / (1024*1024*1024),
        (double)caps.total_memory / (1024*1024*1024),
        temp_info.current_temp_celsius,
        power_info.current_power_watts, power_info.battery_level_percent,
        caps.has_sse ? "예" : "아니오",
        caps.has_avx ? "예" : "아니오",
        caps.has_avx2 ? "예" : "아니오",
        caps.has_neon ? "예" : "아니오",
        g_dispatch_table.entry_count,
        g_metric_count
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    return ET_SUCCESS;
}