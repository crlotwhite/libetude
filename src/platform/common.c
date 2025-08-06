/**
 * @file common.c
 * @brief 플랫폼 추상화 레이어의 공통 기능 구현
 * @author LibEtude Team
 */

#include "libetude/platform/common.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef LIBETUDE_PLATFORM_WINDOWS
    #include <windows.h>
    #include <intrin.h>
#elif defined(LIBETUDE_PLATFORM_LINUX) || defined(LIBETUDE_PLATFORM_ANDROID)
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <cpuid.h>
#elif defined(LIBETUDE_PLATFORM_MACOS) || defined(LIBETUDE_PLATFORM_IOS)
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <mach/mach.h>
#endif

/* 전역 오류 정보 */
static ETDetailedError g_last_error = {0};

/* 플랫폼 정보 조회 */
ETResult et_get_platform_info(ETPlatformInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(info, 0, sizeof(ETPlatformInfo));

    /* 플랫폼 타입 설정 */
    info->type = et_get_current_platform();
    strncpy(info->name, LIBETUDE_PLATFORM_NAME, sizeof(info->name) - 1);

    /* 아키텍처 설정 */
    info->arch = et_get_current_architecture();

    /* 하드웨어 기능 감지 */
    info->features = et_detect_hardware_features();

#ifdef LIBETUDE_PLATFORM_WINDOWS
    SYSTEM_INFO sys_info;
    MEMORYSTATUSEX mem_status;

    GetSystemInfo(&sys_info);
    info->cpu_count = sys_info.dwNumberOfProcessors;

    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        info->total_memory = mem_status.ullTotalPhys;
    }

    /* Windows 버전 정보 */
    OSVERSIONINFOEX os_info;
    os_info.dwOSVersionInfoSize = sizeof(os_info);
    if (GetVersionEx((OSVERSIONINFO*)&os_info)) {
        snprintf(info->version, sizeof(info->version), "%lu.%lu.%lu",
                os_info.dwMajorVersion, os_info.dwMinorVersion, os_info.dwBuildNumber);
    }

#elif defined(LIBETUDE_PLATFORM_LINUX) || defined(LIBETUDE_PLATFORM_ANDROID)
    info->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        info->total_memory = sys_info.totalram * sys_info.mem_unit;
    }

#ifdef __has_include
    #if __has_include(<sys/utsname.h>)
        #include <sys/utsname.h>
        /* 커널 버전 정보 */
        struct utsname uts;
        if (uname(&uts) == 0) {
            strncpy(info->version, uts.release, sizeof(info->version) - 1);
        }
    #else
        strncpy(info->version, "Linux", sizeof(info->version) - 1);
    #endif
#else
    strncpy(info->version, "Linux", sizeof(info->version) - 1);
#endif

#elif defined(LIBETUDE_PLATFORM_MACOS) || defined(LIBETUDE_PLATFORM_IOS)
    size_t size = sizeof(info->cpu_count);
    sysctlbyname("hw.ncpu", &info->cpu_count, &size, NULL, 0);

    size = sizeof(info->total_memory);
    sysctlbyname("hw.memsize", &info->total_memory, &size, NULL, 0);

    /* macOS 버전 정보 */
    char version_str[32];
    size = sizeof(version_str);
    if (sysctlbyname("kern.version", version_str, &size, NULL, 0) == 0) {
        /* 간단한 버전 추출 */
        strncpy(info->version, "macOS", sizeof(info->version) - 1);
    }
#endif

    return ET_SUCCESS;
}

/* 현재 플랫폼 타입 조회 */
ETPlatformType et_get_current_platform(void) {
#ifdef LIBETUDE_PLATFORM_WINDOWS
    return ET_PLATFORM_WINDOWS;
#elif defined(LIBETUDE_PLATFORM_LINUX)
    return ET_PLATFORM_LINUX;
#elif defined(LIBETUDE_PLATFORM_MACOS)
    return ET_PLATFORM_MACOS;
#elif defined(LIBETUDE_PLATFORM_IOS)
    return ET_PLATFORM_IOS;
#elif defined(LIBETUDE_PLATFORM_ANDROID)
    return ET_PLATFORM_ANDROID;
#else
    return ET_PLATFORM_UNKNOWN;
#endif
}

/* 현재 아키텍처 조회 */
ETArchitecture et_get_current_architecture(void) {
#ifdef LIBETUDE_ARCH_X64
    return ET_ARCH_X64;
#elif defined(LIBETUDE_ARCH_X86)
    return ET_ARCH_X86;
#elif defined(LIBETUDE_ARCH_ARM64)
    return ET_ARCH_ARM64;
#elif defined(LIBETUDE_ARCH_ARM)
    return ET_ARCH_ARM;
#else
    return ET_ARCH_UNKNOWN;
#endif
}

/* CPUID 기반 기능 감지 (x86/x64) */
static uint32_t detect_x86_features(void) {
    uint32_t features = ET_FEATURE_NONE;

#if defined(LIBETUDE_ARCH_X86) || defined(LIBETUDE_ARCH_X64)
    #ifdef LIBETUDE_PLATFORM_WINDOWS
        int cpu_info[4];

        /* CPUID 기본 정보 */
        __cpuid(cpu_info, 1);

        if (cpu_info[3] & (1 << 25)) features |= ET_FEATURE_SSE;
        if (cpu_info[3] & (1 << 26)) features |= ET_FEATURE_SSE2;
        if (cpu_info[2] & (1 << 0))  features |= ET_FEATURE_SSE3;
        if (cpu_info[2] & (1 << 9))  features |= ET_FEATURE_SSSE3;
        if (cpu_info[2] & (1 << 19)) features |= ET_FEATURE_SSE4_1;
        if (cpu_info[2] & (1 << 20)) features |= ET_FEATURE_SSE4_2;
        if (cpu_info[2] & (1 << 28)) features |= ET_FEATURE_AVX;
        if (cpu_info[2] & (1 << 12)) features |= ET_FEATURE_FMA;

        /* 확장 기능 확인 */
        __cpuid(cpu_info, 7);
        if (cpu_info[1] & (1 << 5)) features |= ET_FEATURE_AVX2;
        if (cpu_info[1] & (1 << 16)) features |= ET_FEATURE_AVX512;

    #elif defined(__GNUC__) || defined(__clang__)
        unsigned int eax, ebx, ecx, edx;

        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            if (edx & (1 << 25)) features |= ET_FEATURE_SSE;
            if (edx & (1 << 26)) features |= ET_FEATURE_SSE2;
            if (ecx & (1 << 0))  features |= ET_FEATURE_SSE3;
            if (ecx & (1 << 9))  features |= ET_FEATURE_SSSE3;
            if (ecx & (1 << 19)) features |= ET_FEATURE_SSE4_1;
            if (ecx & (1 << 20)) features |= ET_FEATURE_SSE4_2;
            if (ecx & (1 << 28)) features |= ET_FEATURE_AVX;
            if (ecx & (1 << 12)) features |= ET_FEATURE_FMA;
        }

        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            if (ebx & (1 << 5)) features |= ET_FEATURE_AVX2;
            if (ebx & (1 << 16)) features |= ET_FEATURE_AVX512;
        }
    #endif
#endif

    return features;
}

/* ARM NEON 기능 감지 */
static uint32_t detect_arm_features(void) {
    uint32_t features = ET_FEATURE_NONE;

#if defined(LIBETUDE_ARCH_ARM) || defined(LIBETUDE_ARCH_ARM64)
    #ifdef LIBETUDE_PLATFORM_LINUX
        /* /proc/cpuinfo에서 NEON 지원 확인 */
        FILE* fp = fopen("/proc/cpuinfo", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "neon") || strstr(line, "asimd")) {
                    features |= ET_FEATURE_NEON;
                    break;
                }
            }
            fclose(fp);
        }
    #elif defined(LIBETUDE_PLATFORM_MACOS) || defined(LIBETUDE_PLATFORM_IOS)
        /* Apple Silicon은 기본적으로 NEON 지원 */
        #ifdef LIBETUDE_ARCH_ARM64
            features |= ET_FEATURE_NEON;
        #endif
    #elif defined(LIBETUDE_PLATFORM_ANDROID)
        /* Android NDK의 cpu-features 라이브러리 사용 권장 */
        /* 여기서는 기본적인 감지만 구현 */
        #ifdef __ARM_NEON
            features |= ET_FEATURE_NEON;
        #endif
    #endif
#endif

    return features;
}

/* 하드웨어 기능 감지 */
uint32_t et_detect_hardware_features(void) {
    uint32_t features = ET_FEATURE_NONE;

    ETArchitecture arch = et_get_current_architecture();

    if (arch == ET_ARCH_X86 || arch == ET_ARCH_X64) {
        features |= detect_x86_features();
    } else if (arch == ET_ARCH_ARM || arch == ET_ARCH_ARM64) {
        features |= detect_arm_features();
    }

    return features;
}

/* 특정 기능 지원 여부 확인 */
bool et_has_hardware_feature(ETHardwareFeature feature) {
    static uint32_t cached_features = 0;
    static bool features_cached = false;

    if (!features_cached) {
        cached_features = et_detect_hardware_features();
        features_cached = true;
    }

    return (cached_features & feature) != 0;
}

/* 오류 코드를 문자열로 변환 */
const char* et_result_to_string(ETResult result) {
    switch (result) {
        case ET_SUCCESS: return "Success";
        case ET_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case ET_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case ET_ERROR_NOT_SUPPORTED: return "Not supported";
        case ET_ERROR_PLATFORM_SPECIFIC: return "Platform specific error";
        case ET_ERROR_INITIALIZATION_FAILED: return "Initialization failed";
        case ET_ERROR_RESOURCE_BUSY: return "Resource busy";
        case ET_ERROR_TIMEOUT: return "Timeout";
        case ET_ERROR_IO_ERROR: return "I/O error";
        case ET_ERROR_NETWORK_ERROR: return "Network error";
        case ET_ERROR_UNKNOWN: return "Unknown error";
        default: return "Invalid error code";
    }
}

/* 상세 오류 정보 설정 */
void et_set_detailed_error(ETDetailedError* error, ETResult code, int platform_code,
                          const char* message, const char* file, int line, const char* function) {
    if (!error) {
        error = &g_last_error;
    }

    error->code = code;
    error->platform_code = platform_code;
    error->file = file;
    error->line = line;
    error->function = function;
    error->platform = et_get_current_platform();

    /* 현재 시간 설정 */
    #ifdef LIBETUDE_PLATFORM_WINDOWS
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        error->timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    #else
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        error->timestamp = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    #endif

    /* 메시지 복사 */
    if (message) {
        strncpy(error->message, message, sizeof(error->message) - 1);
        error->message[sizeof(error->message) - 1] = '\0';
    } else {
        strncpy(error->message, et_result_to_string(code), sizeof(error->message) - 1);
    }
}

/* 마지막 오류 정보 조회 */
const ETDetailedError* et_get_last_error(void) {
    return &g_last_error;
}