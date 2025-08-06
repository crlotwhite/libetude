/**
 * @file windows_system.c
 * @brief Windows 시스템 정보 구현체
 * @author LibEtude Project
 * @version 1.0.0
 */

#ifdef _WIN32

#include "libetude/platform/system.h"
#include "libetude/memory.h"
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <intrin.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Windows 플랫폼 데이터 구조체
// ============================================================================

typedef struct {
    LARGE_INTEGER frequency;       /**< 고해상도 타이머 주파수 */
    PDH_HQUERY cpu_query;         /**< CPU 사용률 쿼리 핸들 */
    PDH_HCOUNTER cpu_counter;     /**< CPU 사용률 카운터 핸들 */
    bool pdh_initialized;         /**< PDH 초기화 상태 */
} ETWindowsSystemData;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult windows_get_system_info(ETSystemInfo* info);
static ETResult windows_get_memory_info(ETMemoryInfo* info);
static ETResult windows_get_cpu_info(ETCPUInfo* info);
static ETResult windows_get_high_resolution_time(uint64_t* time_ns);
static ETResult windows_sleep(uint32_t milliseconds);
static ETResult windows_get_timer_frequency(uint64_t* frequency);
static uint32_t windows_get_simd_features(void);
static bool windows_has_feature(ETHardwareFeature feature);
static ETResult windows_detect_hardware_capabilities(uint32_t* capabilities);
static ETResult windows_get_cpu_usage(float* usage_percent);
static ETResult windows_get_memory_usage(ETMemoryUsage* usage);
static ETResult windows_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage);
static ETResult windows_get_system_uptime(uint64_t* uptime_seconds);
static ETResult windows_get_process_uptime(uint64_t* uptime_seconds);

static void windows_cpuid(int info[4], int function_id);
static void windows_cpuidex(int info[4], int function_id, int subfunction_id);

// ============================================================================
// 인터페이스 생성 함수
// ============================================================================

ETResult et_system_interface_create_windows(ETSystemInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETSystemInterface* sys_interface = (ETSystemInterface*)libetude_malloc(sizeof(ETSystemInterface));
    if (!sys_interface) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    ETWindowsSystemData* platform_data = (ETWindowsSystemData*)libetude_malloc(sizeof(ETWindowsSystemData));
    if (!platform_data) {
        libetude_free(sys_interface);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 플랫폼 데이터 초기화
    memset(platform_data, 0, sizeof(ETWindowsSystemData));

    // 고해상도 타이머 주파수 초기화
    QueryPerformanceFrequency(&platform_data->frequency);

    // PDH 초기화 (CPU 사용률 모니터링용)
    if (PdhOpenQuery(NULL, 0, &platform_data->cpu_query) == ERROR_SUCCESS) {
        if (PdhAddCounter(platform_data->cpu_query, TEXT("\\Processor(_Total)\\% Processor Time"),
                         0, &platform_data->cpu_counter) == ERROR_SUCCESS) {
            platform_data->pdh_initialized = true;
            PdhCollectQueryData(platform_data->cpu_query); // 첫 번째 샘플 수집
        }
    }

    // 인터페이스 함수 포인터 설정
    sys_interface->get_system_info = windows_get_system_info;
    sys_interface->get_memory_info = windows_get_memory_info;
    sys_interface->get_cpu_info = windows_get_cpu_info;
    sys_interface->get_high_resolution_time = windows_get_high_resolution_time;
    sys_interface->sleep = windows_sleep;
    sys_interface->get_timer_frequency = windows_get_timer_frequency;
    sys_interface->get_simd_features = windows_get_simd_features;
    sys_interface->has_feature = windows_has_feature;
    sys_interface->detect_hardware_capabilities = windows_detect_hardware_capabilities;
    sys_interface->get_cpu_usage = windows_get_cpu_usage;
    sys_interface->get_memory_usage = windows_get_memory_usage;
    sys_interface->get_process_memory_info = windows_get_process_memory_info;
    sys_interface->get_system_uptime = windows_get_system_uptime;
    sys_interface->get_process_uptime = windows_get_process_uptime;
    sys_interface->platform_data = platform_data;

    *interface = sys_interface;
    return ET_SUCCESS;
}

// ============================================================================
// 시스템 정보 수집 함수 구현
// ============================================================================

static ETResult windows_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETSystemInfo));

    // 시스템 정보 가져오기
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    // 메모리 상태 가져오기
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    GlobalMemoryStatusEx(&mem_status);

    // 기본 정보 설정
    info->total_memory = mem_status.ullTotalPhys;
    info->available_memory = mem_status.ullAvailPhys;
    info->cpu_count = sys_info.dwNumberOfProcessors;
    info->platform_type = ET_PLATFORM_WINDOWS;

    // 아키텍처 설정
    switch (sys_info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            info->architecture = ET_ARCH_X64;
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            info->architecture = ET_ARCH_X86;
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            info->architecture = ET_ARCH_ARM;
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            info->architecture = ET_ARCH_ARM64;
            break;
        default:
            info->architecture = ET_ARCH_UNKNOWN;
            break;
    }

    // 시스템 이름 설정
    DWORD size = sizeof(info->system_name);
    GetComputerNameA(info->system_name, &size);

    // OS 버전 정보
    OSVERSIONINFOEXA os_version;
    os_version.dwOSVersionInfoSize = sizeof(os_version);
    if (GetVersionExA((OSVERSIONINFOA*)&os_version)) {
        snprintf(info->os_version, sizeof(info->os_version), "%lu.%lu.%lu",
                os_version.dwMajorVersion, os_version.dwMinorVersion, os_version.dwBuildNumber);
    }

    // CPU 정보 가져오기 (기본 정보만)
    ETCPUInfo cpu_info;
    if (windows_get_cpu_info(&cpu_info) == ET_SUCCESS) {
        strncpy(info->cpu_name, cpu_info.brand, sizeof(info->cpu_name) - 1);
        info->cpu_frequency = cpu_info.base_frequency_mhz;
    }

    return ET_SUCCESS;
}

static ETResult windows_get_memory_info(ETMemoryInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETMemoryInfo));

    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (!GlobalMemoryStatusEx(&mem_status)) {
        return ET_ERROR_SYSTEM;
    }

    info->total_physical = mem_status.ullTotalPhys;
    info->available_physical = mem_status.ullAvailPhys;
    info->total_virtual = mem_status.ullTotalVirtual;
    info->available_virtual = mem_status.ullAvailVirtual;

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    info->page_size = sys_info.dwPageSize;
    info->allocation_granularity = sys_info.dwAllocationGranularity;

    return ET_SUCCESS;
}

static ETResult windows_get_cpu_info(ETCPUInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETCPUInfo));

    int cpu_info_array[4];

    // CPU 제조사 정보
    windows_cpuid(cpu_info_array, 0);
    memcpy(info->vendor, &cpu_info_array[1], 4);
    memcpy(info->vendor + 4, &cpu_info_array[3], 4);
    memcpy(info->vendor + 8, &cpu_info_array[2], 4);
    info->vendor[12] = '\0';

    // CPU 기본 정보
    windows_cpuid(cpu_info_array, 1);
    info->family = (cpu_info_array[0] >> 8) & 0xF;
    info->model = (cpu_info_array[0] >> 4) & 0xF;
    info->stepping = cpu_info_array[0] & 0xF;

    // 확장 패밀리/모델 정보
    if (info->family == 0xF) {
        info->family += (cpu_info_array[0] >> 20) & 0xFF;
    }
    if (info->family == 0x6 || info->family == 0xF) {
        info->model += ((cpu_info_array[0] >> 16) & 0xF) << 4;
    }

    // CPU 브랜드 문자열
    windows_cpuid(cpu_info_array, 0x80000000);
    if (cpu_info_array[0] >= 0x80000004) {
        char* brand_ptr = info->brand;
        for (int i = 0x80000002; i <= 0x80000004; i++) {
            windows_cpuid(cpu_info_array, i);
            memcpy(brand_ptr, cpu_info_array, 16);
            brand_ptr += 16;
        }
        info->brand[48] = '\0';

        // 앞뒤 공백 제거
        char* start = info->brand;
        while (*start == ' ') start++;
        if (start != info->brand) {
            memmove(info->brand, start, strlen(start) + 1);
        }
    }

    // 시스템 정보에서 코어 수 가져오기
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    info->logical_cores = sys_info.dwNumberOfProcessors;

    // 물리 코어 수 추정 (하이퍼스레딩 고려)
    info->physical_cores = info->logical_cores;
    if (strstr(info->vendor, "Intel") != NULL) {
        // Intel CPU의 경우 하이퍼스레딩 확인
        windows_cpuid(cpu_info_array, 1);
        if (cpu_info_array[3] & (1 << 28)) { // HTT 비트 확인
            info->physical_cores = info->logical_cores / 2;
        }
    }

    // 캐시 정보 (기본값 설정)
    info->cache_line_size = 64; // 대부분의 현대 CPU는 64바이트
    info->l1_cache_size = 32;   // KB
    info->l2_cache_size = 256;  // KB
    info->l3_cache_size = 8192; // KB

    // 주파수 정보 (레지스트리에서 가져오기 시도)
    HKEY hkey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     0, KEY_READ, &hkey) == ERROR_SUCCESS) {
        DWORD freq_mhz;
        DWORD size = sizeof(freq_mhz);
        if (RegQueryValueExA(hkey, "~MHz", NULL, NULL, (LPBYTE)&freq_mhz, &size) == ERROR_SUCCESS) {
            info->base_frequency_mhz = freq_mhz;
            info->max_frequency_mhz = freq_mhz;
        }
        RegCloseKey(hkey);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 타이머 관련 함수 구현
// ============================================================================

static ETResult windows_get_high_resolution_time(uint64_t* time_ns) {
    if (!time_ns) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) {
        return ET_ERROR_SYSTEM;
    }

    // 나노초로 변환
    ETWindowsSystemData* platform_data = (ETWindowsSystemData*)
        et_get_system_interface()->platform_data;

    if (platform_data && platform_data->frequency.QuadPart > 0) {
        *time_ns = (counter.QuadPart * 1000000000ULL) / platform_data->frequency.QuadPart;
    } else {
        return ET_ERROR_SYSTEM;
    }

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

    ETWindowsSystemData* platform_data = (ETWindowsSystemData*)
        et_get_system_interface()->platform_data;

    if (platform_data) {
        *frequency = platform_data->frequency.QuadPart;
        return ET_SUCCESS;
    }

    return ET_ERROR_SYSTEM;
}

// ============================================================================
// SIMD 기능 감지 함수 구현
// ============================================================================

static uint32_t windows_get_simd_features(void) {
    uint32_t features = ET_SIMD_NONE;
    int cpu_info[4];

    // 기본 CPUID 지원 확인
    windows_cpuid(cpu_info, 1);

    // SSE 계열 확인
    if (cpu_info[3] & (1 << 25)) features |= ET_SIMD_SSE;
    if (cpu_info[3] & (1 << 26)) features |= ET_SIMD_SSE2;
    if (cpu_info[2] & (1 << 0))  features |= ET_SIMD_SSE3;
    if (cpu_info[2] & (1 << 9))  features |= ET_SIMD_SSSE3;
    if (cpu_info[2] & (1 << 19)) features |= ET_SIMD_SSE4_1;
    if (cpu_info[2] & (1 << 20)) features |= ET_SIMD_SSE4_2;

    // AVX 계열 확인
    if (cpu_info[2] & (1 << 28)) features |= ET_SIMD_AVX;
    if (cpu_info[2] & (1 << 12)) features |= ET_SIMD_FMA;

    // AVX2 확인
    windows_cpuid(cpu_info, 7);
    if (cpu_info[1] & (1 << 5)) features |= ET_SIMD_AVX2;
    if (cpu_info[1] & (1 << 16)) features |= ET_SIMD_AVX512;

    return features;
}

static bool windows_has_feature(ETHardwareFeature feature) {
    switch (feature) {
        case ET_HW_FEATURE_SIMD:
            return windows_get_simd_features() != ET_SIMD_NONE;
        case ET_HW_FEATURE_HIGH_RES_TIMER:
            return true; // Windows는 항상 고해상도 타이머 지원
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

static ETResult windows_detect_hardware_capabilities(uint32_t* capabilities) {
    if (!capabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *capabilities = 0;

    if (windows_has_feature(ET_HW_FEATURE_SIMD)) {
        *capabilities |= ET_HW_FEATURE_SIMD;
    }
    if (windows_has_feature(ET_HW_FEATURE_HIGH_RES_TIMER)) {
        *capabilities |= ET_HW_FEATURE_HIGH_RES_TIMER;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 성능 모니터링 함수 구현
// ============================================================================

static ETResult windows_get_cpu_usage(float* usage_percent) {
    if (!usage_percent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSystemData* platform_data = (ETWindowsSystemData*)
        et_get_system_interface()->platform_data;

    if (!platform_data || !platform_data->pdh_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    PDH_FMT_COUNTERVALUE counter_value;
    if (PdhCollectQueryData(platform_data->cpu_query) != ERROR_SUCCESS) {
        return ET_ERROR_SYSTEM;
    }

    if (PdhGetFormattedCounterValue(platform_data->cpu_counter, PDH_FMT_DOUBLE,
                                   NULL, &counter_value) != ERROR_SUCCESS) {
        return ET_ERROR_SYSTEM;
    }

    *usage_percent = (float)counter_value.doubleValue;
    return ET_SUCCESS;
}

static ETResult windows_get_memory_usage(ETMemoryUsage* usage) {
    if (!usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(usage, 0, sizeof(ETMemoryUsage));

    // 프로세스 메모리 정보
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        usage->process_memory_usage = pmc.WorkingSetSize;
        usage->process_peak_memory = pmc.PeakWorkingSetSize;
    }

    // 시스템 메모리 사용률
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        usage->memory_usage_percent = (float)mem_status.dwMemoryLoad;
    }

    // CPU 사용률
    windows_get_cpu_usage(&usage->cpu_usage_percent);

    return ET_SUCCESS;
}

static ETResult windows_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage) {
    if (!current_usage || !peak_usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    PROCESS_MEMORY_COUNTERS pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
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
    if (!GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
                        &kernel_time, &user_time)) {
        return ET_ERROR_SYSTEM;
    }

    FILETIME current_time;
    GetSystemTimeAsFileTime(&current_time);

    ULARGE_INTEGER creation_ul, current_ul;
    creation_ul.LowPart = creation_time.dwLowDateTime;
    creation_ul.HighPart = creation_time.dwHighDateTime;
    current_ul.LowPart = current_time.dwLowDateTime;
    current_ul.HighPart = current_time.dwHighDateTime;

    // 100나노초 단위를 초 단위로 변환
    *uptime_seconds = (current_ul.QuadPart - creation_ul.QuadPart) / 10000000ULL;

    return ET_SUCCESS;
}

// ============================================================================
// CPUID 헬퍼 함수
// ============================================================================

static void windows_cpuid(int info[4], int function_id) {
    __cpuid(info, function_id);
}

static void windows_cpuidex(int info[4], int function_id, int subfunction_id) {
    __cpuidex(info, function_id, subfunction_id);
}

#endif // _WIN32