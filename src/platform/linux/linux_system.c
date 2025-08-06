/**
 * @file linux_system.c
 * @brief Linux 시스템 정보 인터페이스 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/system.h"
#include "libetude/platform/common.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

// ============================================================================
// Linux 시스템 인터페이스 구현
// ============================================================================

static ETResult linux_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(ETSystemInfo));
    
    // 시스템 정보
    info->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    info->platform_type = ET_PLATFORM_LINUX;
    
    // 아키텍처 감지
#if defined(__x86_64__)
    info->architecture = ET_ARCH_X64;
#elif defined(__i386__)
    info->architecture = ET_ARCH_X86;
#elif defined(__aarch64__)
    info->architecture = ET_ARCH_ARM64;
#elif defined(__arm__)
    info->architecture = ET_ARCH_ARM;
#else
    info->architecture = ET_ARCH_UNKNOWN;
#endif
    
    // 메모리 정보
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info->total_memory = si.totalram * si.mem_unit;
        info->available_memory = si.freeram * si.mem_unit;
    }
    
    // CPU 정보 읽기
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* colon = strchr(line, ':');
                if (colon) {
                    colon += 2; // ": " 건너뛰기
                    strncpy(info->cpu_name, colon, sizeof(info->cpu_name) - 1);
                    // 개행 문자 제거
                    char* newline = strchr(info->cpu_name, '\n');
                    if (newline) *newline = '\0';
                    break;
                }
            }
        }
        fclose(cpuinfo);
    }
    
    if (strlen(info->cpu_name) == 0) {
        strcpy(info->cpu_name, "Unknown CPU");
    }
    
    // 시스템 이름
    strcpy(info->system_name, "Linux");
    
    // OS 버전
    struct utsname uts;
    if (uname(&uts) == 0) {
        strncpy(info->os_version, uts.release, sizeof(info->os_version) - 1);
        info->os_version[sizeof(info->os_version) - 1] = '\0';
    } else {
        strcpy(info->os_version, "Unknown");
    }
    
    return ET_SUCCESS;
}

static ETResult linux_get_memory_info(ETMemoryInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(ETMemoryInfo));
    
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        return ET_ERROR_SYSTEM;
    }
    
    info->total_physical = si.totalram * si.mem_unit;
    info->available_physical = si.freeram * si.mem_unit;
    info->total_virtual = si.totalswap * si.mem_unit;
    info->available_virtual = si.freeswap * si.mem_unit;
    
    info->page_size = sysconf(_SC_PAGESIZE);
    info->allocation_granularity = info->page_size; // Linux에서는 페이지 크기와 동일
    
    return ET_SUCCESS;
}

static ETResult linux_get_cpu_info(ETCPUInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(ETCPUInfo));
    
    // CPU 정보 파싱
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) {
        return ET_ERROR_IO;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), cpuinfo)) {
        if (strncmp(line, "vendor_id", 9) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                colon += 2;
                strncpy(info->vendor, colon, sizeof(info->vendor) - 1);
                char* newline = strchr(info->vendor, '\n');
                if (newline) *newline = '\0';
            }
        } else if (strncmp(line, "model name", 10) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                colon += 2;
                strncpy(info->brand, colon, sizeof(info->brand) - 1);
                char* newline = strchr(info->brand, '\n');
                if (newline) *newline = '\0';
            }
        } else if (strncmp(line, "cpu family", 10) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                info->family = atoi(colon + 1);
            }
        } else if (strncmp(line, "model", 5) == 0 && line[5] == '\t') {
            char* colon = strchr(line, ':');
            if (colon) {
                info->model = atoi(colon + 1);
            }
        } else if (strncmp(line, "stepping", 8) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                info->stepping = atoi(colon + 1);
            }
        }
    }
    fclose(cpuinfo);
    
    // 코어 수
    info->logical_cores = sysconf(_SC_NPROCESSORS_ONLN);
    info->physical_cores = info->logical_cores; // 간단한 구현
    
    // 캐시 정보 (기본값)
    info->cache_line_size = 64;
    info->l1_cache_size = 32;
    info->l2_cache_size = 256;
    info->l3_cache_size = 8192;
    
    return ET_SUCCESS;
}

static ETResult linux_get_high_resolution_time(uint64_t* time_ns) {
    if (!time_ns) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return ET_ERROR_SYSTEM;
    }
    
    *time_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    return ET_SUCCESS;
}

static ETResult linux_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    
    if (nanosleep(&ts, NULL) != 0) {
        return ET_ERROR_SYSTEM;
    }
    
    return ET_SUCCESS;
}

static ETResult linux_get_timer_frequency(uint64_t* frequency) {
    if (!frequency) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    *frequency = 1000000000ULL; // 나노초 단위
    return ET_SUCCESS;
}

static uint32_t linux_get_simd_features(void) {
    uint32_t features = ET_SIMD_NONE;
    
    // /proc/cpuinfo에서 flags 읽기
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[1024];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "flags", 5) == 0) {
                if (strstr(line, "sse")) features |= ET_SIMD_SSE;
                if (strstr(line, "sse2")) features |= ET_SIMD_SSE2;
                if (strstr(line, "sse3")) features |= ET_SIMD_SSE3;
                if (strstr(line, "ssse3")) features |= ET_SIMD_SSSE3;
                if (strstr(line, "sse4_1")) features |= ET_SIMD_SSE4_1;
                if (strstr(line, "sse4_2")) features |= ET_SIMD_SSE4_2;
                if (strstr(line, "avx")) features |= ET_SIMD_AVX;
                if (strstr(line, "avx2")) features |= ET_SIMD_AVX2;
                if (strstr(line, "avx512f")) features |= ET_SIMD_AVX512;
                if (strstr(line, "fma")) features |= ET_SIMD_FMA;
                break;
            }
#ifdef __arm__
            // ARM의 경우 features 섹션에서 NEON 확인
            if (strncmp(line, "Features", 8) == 0) {
                if (strstr(line, "neon")) features |= ET_SIMD_NEON;
                break;
            }
#endif
        }
        fclose(cpuinfo);
    }
    
    return features;
}

static bool linux_has_feature(ETHardwareFeature feature) {
    uint32_t simd_features = linux_get_simd_features();
    
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
        case ET_FEATURE_NEON:
            return (simd_features & ET_SIMD_NEON) != 0;
        case ET_FEATURE_FMA:
            return (simd_features & ET_SIMD_FMA) != 0;
        case ET_FEATURE_HIGH_RES_TIMER:
            return true; // Linux는 고해상도 타이머를 지원
        default:
            return false;
    }
}

static ETResult linux_detect_hardware_capabilities(uint32_t* capabilities) {
    if (!capabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    *capabilities = 0;
    
    uint32_t simd_features = linux_get_simd_features();
    if (simd_features != ET_SIMD_NONE) {
        *capabilities |= ET_FEATURE_SSE; // 기본적으로 SIMD 지원으로 표시
    }
    
    *capabilities |= ET_FEATURE_HIGH_RES_TIMER;
    
    return ET_SUCCESS;
}

static ETResult linux_get_cpu_usage(float* usage_percent) {
    if (!usage_percent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    // 간단한 구현 - /proc/stat 파싱 필요
    *usage_percent = 0.0f;
    return ET_SUCCESS;
}

static ETResult linux_get_memory_usage(ETMemoryUsage* usage) {
    if (!usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    memset(usage, 0, sizeof(ETMemoryUsage));
    
    // /proc/self/status에서 메모리 정보 읽기
    FILE* status = fopen("/proc/self/status", "r");
    if (status) {
        char line[256];
        while (fgets(line, sizeof(line), status)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                long rss_kb = atol(line + 6);
                usage->process_memory_usage = rss_kb * 1024;
            } else if (strncmp(line, "VmHWM:", 6) == 0) {
                long hwm_kb = atol(line + 6);
                usage->process_peak_memory = hwm_kb * 1024;
            }
        }
        fclose(status);
    }
    
    // 시스템 메모리 사용률
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        usage->memory_usage_percent = (float)(si.totalram - si.freeram) / si.totalram * 100.0f;
    }
    
    return ET_SUCCESS;
}

static ETResult linux_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage) {
    if (!current_usage || !peak_usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    FILE* status = fopen("/proc/self/status", "r");
    if (!status) {
        return ET_ERROR_IO;
    }
    
    char line[256];
    *current_usage = 0;
    *peak_usage = 0;
    
    while (fgets(line, sizeof(line), status)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            long rss_kb = atol(line + 6);
            *current_usage = rss_kb * 1024;
        } else if (strncmp(line, "VmHWM:", 6) == 0) {
            long hwm_kb = atol(line + 6);
            *peak_usage = hwm_kb * 1024;
        }
    }
    fclose(status);
    
    return ET_SUCCESS;
}

static ETResult linux_get_system_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    FILE* uptime_file = fopen("/proc/uptime", "r");
    if (!uptime_file) {
        return ET_ERROR_IO;
    }
    
    double uptime_sec;
    if (fscanf(uptime_file, "%lf", &uptime_sec) == 1) {
        *uptime_seconds = (uint64_t)uptime_sec;
    } else {
        *uptime_seconds = 0;
    }
    
    fclose(uptime_file);
    return ET_SUCCESS;
}

static ETResult linux_get_process_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    // /proc/self/stat에서 시작 시간 읽기
    FILE* stat_file = fopen("/proc/self/stat", "r");
    if (!stat_file) {
        return ET_ERROR_IO;
    }
    
    char line[1024];
    if (fgets(line, sizeof(line), stat_file)) {
        // stat 파일에서 22번째 필드가 starttime (jiffies)
        char* token = strtok(line, " ");
        for (int i = 1; i < 22 && token; i++) {
            token = strtok(NULL, " ");
        }
        
        if (token) {
            unsigned long long starttime = strtoull(token, NULL, 10);
            long ticks_per_sec = sysconf(_SC_CLK_TCK);
            
            // 시스템 uptime 가져오기
            uint64_t system_uptime;
            if (linux_get_system_uptime(&system_uptime) == ET_SUCCESS) {
                *uptime_seconds = system_uptime - (starttime / ticks_per_sec);
            } else {
                *uptime_seconds = 0;
            }
        } else {
            *uptime_seconds = 0;
        }
    } else {
        *uptime_seconds = 0;
    }
    
    fclose(stat_file);
    return ET_SUCCESS;
}

// ============================================================================
// Linux 시스템 인터페이스 구조체
// ============================================================================

static ETSystemInterface g_linux_system_interface = {
    .get_system_info = linux_get_system_info,
    .get_memory_info = linux_get_memory_info,
    .get_cpu_info = linux_get_cpu_info,
    .get_high_resolution_time = linux_get_high_resolution_time,
    .sleep = linux_sleep,
    .get_timer_frequency = linux_get_timer_frequency,
    .get_simd_features = linux_get_simd_features,
    .has_feature = linux_has_feature,
    .detect_hardware_capabilities = linux_detect_hardware_capabilities,
    .get_cpu_usage = linux_get_cpu_usage,
    .get_memory_usage = linux_get_memory_usage,
    .get_process_memory_info = linux_get_process_memory_info,
    .get_system_uptime = linux_get_system_uptime,
    .get_process_uptime = linux_get_process_uptime,
    .platform_data = NULL
};

// ============================================================================
// 공개 함수
// ============================================================================

ETSystemInterface* et_get_linux_system_interface(void) {
    return &g_linux_system_interface;
}

ETResult et_linux_system_initialize(void) {
    // Linux 시스템 인터페이스 초기화
    return ET_SUCCESS;
}

void et_linux_system_cleanup(void) {
    // Linux 시스템 인터페이스 정리
}

#endif // __linux__