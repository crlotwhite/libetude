/**
 * @file windows_system.c
 * @brief Windows 시스템 정보 인터페이스 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/system.h"
#include "libetude/platform/common.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#include <versionhelpers.h>

#pragma comment(lib, "psapi.lib")

// ============================================================================
// Windows 시스템 인터페이스 구현
// ============================================================================

static ETResult windows_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(ETSystemInfo));
    
    // 시스템 정보
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    
    info->cpu_count = si.dwNumberOfProcessors;
    info->platform_type = ET_PLATFORM_WINDOWS;
    
    // 아키텍처 감지
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            info->architecture = ET_ARCH_X64;
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            info->architecture = ET_ARCH_X86;
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            info->architecture = ET_ARCH_ARM64;
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            info->architecture = ET_ARCH_ARM;
            break;
        default:
            info->architecture = ET_ARCH_UNKNOWN;
            break;
    }
    
    // 메모리 정보
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        info->total_memory = memInfo.ullTotalPhys;
        info->available_memory = memInfo.ullAvailPhys;
    }
    
    // CPU 이름 가져오기 (간단한 구현)
    strcpy_s(info->cpu_name, sizeof(info->cpu_name), "Intel/AMD CPU");
    strcpy_s(info->system_name, sizeof(info->system_name), "Windows");
    
    // Windows 버전
    if (IsWindows10OrGreater()) {
        strcpy_s(info->os_version, sizeof(info->os_version), "10.0");
    } else if (IsWindows8Point1OrGreater()) {
        strcpy_s(info->os_version, sizeof(info->os_version), "8.1");
    } else if (IsWindows8OrGreater()) {
        strcpy_s(info->os_version, sizeof(info->os_version), "8.0");
    } else if (IsWindows7OrGreater()) {
        strcpy_s(info->os_version, sizeof(info->os_version), "7.0");
    } else {
        strcpy_s(info->os_version, sizeof(info->os_version), "Unknown");
    }
    
    return ET_SUCCESS;
}

static ETResult windows_get_memory_info(ETMemoryInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(ETMemoryInfo));
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (!GlobalMemoryStatusEx(&memInfo)) {
        return ET_ERROR_SYSTEM;
    }
    
    info->total_physical = memInfo.ullTotalPhys;
    info->available_physical = memInfo.ullAvailPhys;
    info->total_virtual = memInfo.ullTotalVirtual;
    info->available_virtual = memInfo.ullAvailVirtual;
    
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->page_size = si.dwPageSize;
    info->allocation_granularity = si.dwAllocationGranularity;
    
    return ET_SUCCESS;
}

static ETResult windows_get_cpu_info(ETCPUInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(ETCPUInfo));
    
    // CPUID 정보 수집
    int cpuInfo[4] = {0};
    
    // CPU 제조사
    __cpuid(cpuInfo, 0);
    memcpy(info->vendor, &cpuInfo[1], 4);
    memcpy(info->vendor + 4, &cpuInfo[3], 4);
    memcpy(info->vendor + 8, &cpuInfo[2], 4);
    info->vendor[12] = '\0';
    
    // CPU 모델 정보
    __cpuid(cpuInfo, 1);
    info->family = (cpuInfo[0] >> 8) & 0xF;
    info->model = (cpuInfo[0] >> 4) & 0xF;
    info->stepping = cpuInfo[0] & 0xF;
    
    // 브랜드 문자열 (간단한 구현)
    strcpy_s(info->brand, sizeof(info->brand), "Unknown CPU");
    
    // 코어 수
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->logical_cores = si.dwNumberOfProcessors;
    info->physical_cores = si.dwNumberOfProcessors; // 간단한 구현
    
    // 캐시 정보 (기본값)
    info->cache_line_size = 64;
    info->l1_cache_size = 32;
    info->l2_cache_size = 256;
    info->l3_cache_size = 8192;
    
    return ET_SUCCESS;
}

static ETResult windows_get_high_resolution_time(uint64_t* time_ns) {
    if (!time_ns) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) {
        return ET_ERROR_SYSTEM;
    }
    
    *time_ns = (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
    return ET_SUCCESS;
}

static ETResult windows_sleep(uint32_t milliseconds) {
    Sleep(milliseconds);
    return ET_SUCCESS;
}

static ETResult windows_get_timer_frequency(uint64_t* frequency) {
    if (!frequency) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq)) {
        return ET_ERROR_SYSTEM;
    }
    
    *frequency = freq.QuadPart;
    return ET_SUCCESS;
}

static uint32_t windows_get_simd_features(void) {
    uint32_t features = ET_SIMD_NONE;
    int cpuInfo[4] = {0};
    
    // CPUID 기능 1 호출
    __cpuid(cpuInfo, 1);
    
    if (cpuInfo[3] & (1 << 25)) features |= ET_SIMD_SSE;    // SSE
    if (cpuInfo[3] & (1 << 26)) features |= ET_SIMD_SSE2;   // SSE2
    if (cpuInfo[2] & (1 << 0))  features |= ET_SIMD_SSE3;   // SSE3
    if (cpuInfo[2] & (1 << 9))  features |= ET_SIMD_SSSE3;  // SSSE3
    if (cpuInfo[2] & (1 << 19)) features |= ET_SIMD_SSE4_1; // SSE4.1
    if (cpuInfo[2] & (1 << 20)) features |= ET_SIMD_SSE4_2; // SSE4.2
    if (cpuInfo[2] & (1 << 28)) features |= ET_SIMD_AVX;    // AVX
    if (cpuInfo[2] & (1 << 12)) features |= ET_SIMD_FMA;    // FMA
    
    // CPUID 기능 7 호출 (AVX2)
    __cpuidex(cpuInfo, 7, 0);
    if (cpuInfo[1] & (1 << 5)) features |= ET_SIMD_AVX2;   // AVX2
    if (cpuInfo[1] & (1 << 16)) features |= ET_SIMD_AVX512; // AVX-512F
    
    return features;
}

static bool windows_has_feature(ETHardwareFeature feature) {
    uint32_t simd_features = windows_get_simd_features();
    
    switch (feature) {
        case ET_FEATURE_SSE:
            return (simd_features & ET_SIMD_SSE) != 0;
        case ET_FEATURE_SSE2:
            return (simd_features & ET_SIMD_SSE2) != 0;
        case ET_FEATURE_SSE3:
            return (simd_features & ET_SIMD_SSE3) != 0;
        case ET_FEATURE_SSSE3:
            return (simd_features & ET_SIMD_SSSE3) != 0;
        case ET_FEATURE_SSE4_1:
            return (simd_features & ET_SIMD_SSE4_1) != 0;
        case ET_FEATURE_SSE4_2:
            return (simd_features & ET_SIMD_SSE4_2) != 0;
        case ET_FEATURE_AVX:
            return (simd_features & ET_SIMD_AVX) != 0;
        case ET_FEATURE_AVX2:
            return (simd_features & ET_SIMD_AVX2) != 0;
        case ET_FEATURE_AVX512:
            return (simd_features & ET_SIMD_AVX512) != 0;
        case ET_FEATURE_FMA:
            return (simd_features & ET_SIMD_FMA) != 0;
        case ET_FEATURE_HIGH_RES_TIMER:
            return true; // Windows는 고해상도 타이머를 지원
        default:
            return false;
    }
}

static ETResult windows_detect_hardware_capabilities(uint32_t* capabilities) {
    if (!capabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    *capabilities = 0;
    
    uint32_t simd_features = windows_get_simd_features();
    if (simd_features != ET_SIMD_NONE) {
        *capabilities |= ET_FEATURE_SSE; // 기본적으로 SIMD 지원으로 표시
    }
    
    *capabilities |= ET_FEATURE_HIGH_RES_TIMER;
    
    return ET_SUCCESS;
}

static ETResult windows_get_cpu_usage(float* usage_percent) {
    if (!usage_percent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    // 간단한 구현 - 실제로는 더 복잡한 계산이 필요
    *usage_percent = 0.0f;
    return ET_SUCCESS;
}

static ETResult windows_get_memory_usage(ETMemoryUsage* usage) {
    if (!usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(usage, 0, sizeof(ETMemoryUsage));
    
    // 프로세스 메모리 정보
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        usage->process_memory_usage = pmc.WorkingSetSize;
        usage->process_peak_memory = pmc.PeakWorkingSetSize;
    }
    
    // 시스템 메모리 사용률
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        usage->memory_usage_percent = (float)(memInfo.ullTotalPhys - memInfo.ullAvailPhys) / memInfo.ullTotalPhys * 100.0f;
    }
    
    return ET_SUCCESS;
}

static ETResult windows_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage) {
    if (!current_usage || !peak_usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return ET_ERROR_SYSTEM;
    }
    
    *current_usage = pmc.WorkingSetSize;
    *peak_usage = pmc.PeakWorkingSetSize;
    
    return ET_SUCCESS;
}

static ETResult windows_get_system_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    *uptime_seconds = GetTickCount64() / 1000;
    return ET_SUCCESS;
}

static ETResult windows_get_process_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time)) {
        return ET_ERROR_SYSTEM;
    }
    
    FILETIME current_time;
    GetSystemTimeAsFileTime(&current_time);
    
    ULARGE_INTEGER creation, current;
    creation.LowPart = creation_time.dwLowDateTime;
    creation.HighPart = creation_time.dwHighDateTime;
    current.LowPart = current_time.dwLowDateTime;
    current.HighPart = current_time.dwHighDateTime;
    
    *uptime_seconds = (current.QuadPart - creation.QuadPart) / 10000000ULL;
    return ET_SUCCESS;
}

// ============================================================================
// Windows 시스템 인터페이스 구조체
// ============================================================================

static ETSystemInterface g_windows_system_interface = {
    .get_system_info = windows_get_system_info,
    .get_memory_info = windows_get_memory_info,
    .get_cpu_info = windows_get_cpu_info,
    .get_high_resolution_time = windows_get_high_resolution_time,
    .sleep = windows_sleep,
    .get_timer_frequency = windows_get_timer_frequency,
    .get_simd_features = windows_get_simd_features,
    .has_feature = windows_has_feature,
    .detect_hardware_capabilities = windows_detect_hardware_capabilities,
    .get_cpu_usage = windows_get_cpu_usage,
    .get_memory_usage = windows_get_memory_usage,
    .get_process_memory_info = windows_get_process_memory_info,
    .get_system_uptime = windows_get_system_uptime,
    .get_process_uptime = windows_get_process_uptime,
    .platform_data = NULL
};

// ============================================================================
// 공개 함수
// ============================================================================

ETSystemInterface* et_get_windows_system_interface(void) {
    return &g_windows_system_interface;
}

ETResult et_windows_system_initialize(void) {
    // Windows 시스템 인터페이스 초기화
    return ET_SUCCESS;
}

void et_windows_system_cleanup(void) {
    // Windows 시스템 인터페이스 정리
}

#endif // _WIN32