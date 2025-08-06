/**
 * @file linux_system.c
 * @brief Linux 시스템 정보 구현체
 * @author LibEtude Project
 * @version 1.0.0
 */

#ifdef __linux__

#include "libetude/platform/system.h"
#include "libetude/memory.h"
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cpuid.h>

// ============================================================================
// Linux 플랫폼 데이터 구조체
// ============================================================================

typedef struct {
    struct timespec boot_time;     /**< 시스템 부팅 시간 */
    struct timespec process_start; /**< 프로세스 시작 시간 */
    bool timing_initialized;       /**< 타이밍 정보 초기화 상태 */
    long clock_ticks_per_sec;     /**< 클럭 틱 수 */
} ETLinuxSystemData;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult linux_get_system_info(ETSystemInfo* info);
static ETResult linux_get_memory_info(ETMemoryInfo* info);
static ETResult linux_get_cpu_info(ETCPUInfo* info);
static ETResult linux_get_high_resolution_time(uint64_t* time_ns);
static ETResult linux_sleep(uint32_t milliseconds);
static ETResult linux_get_timer_frequency(uint64_t* frequency);
static uint32_t linux_get_simd_features(void);
static bool linux_has_feature(ETHardwareFeature feature);
static ETResult linux_detect_hardware_capabilities(uint32_t* capabilities);
static ETResult linux_get_cpu_usage(float* usage_percent);
static ETResult linux_get_memory_usage(ETMemoryUsage* usage);
static ETResult linux_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage);
static ETResult linux_get_system_uptime(uint64_t* uptime_seconds);
static ETResult linux_get_process_uptime(uint64_t* uptime_seconds);

static ETResult linux_read_proc_file(const char* path, char* buffer, size_t buffer_size);
static ETResult linux_parse_cpuinfo(ETCPUInfo* info);
static ETResult linux_parse_meminfo(ETMemoryInfo* info);
static uint32_t linux_detect_simd_from_cpuinfo(void);
static uint32_t linux_detect_simd_from_cpuid(void);

// ============================================================================
// 인터페이스 생성 함수
// ============================================================================

ETResult et_system_interface_create_linux(ETSystemInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETSystemInterface* sys_interface = (ETSystemInterface*)libetude_malloc(sizeof(ETSystemInterface));
    if (!sys_interface) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    ETLinuxSystemData* platform_data = (ETLinuxSystemData*)libetude_malloc(sizeof(ETLinuxSystemData));
    if (!platform_data) {
        libetude_free(sys_interface);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 플랫폼 데이터 초기화
    memset(platform_data, 0, sizeof(ETLinuxSystemData));

    // 시스템 부팅 시간 및 프로세스 시작 시간 초기화
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        clock_gettime(CLOCK_REALTIME, &platform_data->boot_time);
        platform_data->boot_time.tv_sec -= si.uptime;
        platform_data->boot_time.tv_nsec = 0;
    }

    clock_gettime(CLOCK_REALTIME, &platform_data->process_start);
    platform_data->clock_ticks_per_sec = sysconf(_SC_CLK_TCK);
    platform_data->timing_initialized = true;

    // 인터페이스 함수 포인터 설정
    sys_interface->get_system_info = linux_get_system_info;
    sys_interface->get_memory_info = linux_get_memory_info;
    sys_interface->get_cpu_info = linux_get_cpu_info;
    sys_interface->get_high_resolution_time = linux_get_high_resolution_time;
    sys_interface->sleep = linux_sleep;
    sys_interface->get_timer_frequency = linux_get_timer_frequency;
    sys_interface->get_simd_features = linux_get_simd_features;
    sys_interface->has_feature = linux_has_feature;
    sys_interface->detect_hardware_capabilities = linux_detect_hardware_capabilities;
    sys_interface->get_cpu_usage = linux_get_cpu_usage;
    sys_interface->get_memory_usage = linux_get_memory_usage;
    sys_interface->get_process_memory_info = linux_get_process_memory_info;
    sys_interface->get_system_uptime = linux_get_system_uptime;
    sys_interface->get_process_uptime = linux_get_process_uptime;
    sys_interface->platform_data = platform_data;

    *interface = sys_interface;
    return ET_SUCCESS;
}

// ============================================================================
// 시스템 정보 수집 함수 구현
// ============================================================================

static ETResult linux_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETSystemInfo));

    // sysinfo를 통한 기본 정보
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        return ET_ERROR_SYSTEM;
    }

    info->total_memory = si.totalram * si.mem_unit;
    info->available_memory = si.freeram * si.mem_unit;
    info->cpu_count = get_nprocs();
    info->platform_type = ET_PLATFORM_LINUX;

    // uname을 통한 시스템 정보
    struct utsname uts;
    if (uname(&uts) == 0) {
        strncpy(info->system_name, uts.nodename, sizeof(info->system_name) - 1);
        strncpy(info->os_version, uts.release, sizeof(info->os_version) - 1);
    }

    // 아키텍처 설정
    if (sizeof(void*) == 8) {
        info->architecture = ET_ARCH_X64;
    } else {
        info->architecture = ET_ARCH_X86;
    }

    // CPU 정보 가져오기
    ETCPUInfo cpu_info;
    if (linux_get_cpu_info(&cpu_info) == ET_SUCCESS) {
        strncpy(info->cpu_name, cpu_info.brand, sizeof(info->cpu_name) - 1);
        info->cpu_frequency = cpu_info.base_frequency_mhz;
    }

    return ET_SUCCESS;
}

static ETResult linux_get_memory_info(ETMemoryInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    return linux_parse_meminfo(info);
}

static ETResult linux_get_cpu_info(ETCPUInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    return linux_parse_cpuinfo(info);
}

// ============================================================================
// 타이머 관련 함수 구현
// ============================================================================

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

    // Linux의 고해상도 타이머는 나노초 단위이므로 1GHz
    *frequency = 1000000000ULL;
    return ET_SUCCESS;
}

// ============================================================================
// SIMD 기능 감지 함수 구현
// ============================================================================

static uint32_t linux_get_simd_features(void) {
    uint32_t features = ET_SIMD_NONE;

    // CPUID를 통한 감지 시도
    features |= linux_detect_simd_from_cpuid();

    // /proc/cpuinfo를 통한 감지로 보완
    features |= linux_detect_simd_from_cpuinfo();

    return features;
}

static bool linux_has_feature(ETHardwareFeature feature) {
    switch (feature) {
        case ET_HW_FEATURE_SIMD:
            return linux_get_simd_features() != ET_SIMD_NONE;
        case ET_HW_FEATURE_HIGH_RES_TIMER:
            return true; // Linux는 항상 고해상도 타이머 지원
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

static ETResult linux_detect_hardware_capabilities(uint32_t* capabilities) {
    if (!capabilities) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *capabilities = 0;

    if (linux_has_feature(ET_HW_FEATURE_SIMD)) {
        *capabilities |= ET_HW_FEATURE_SIMD;
    }
    if (linux_has_feature(ET_HW_FEATURE_HIGH_RES_TIMER)) {
        *capabilities |= ET_HW_FEATURE_HIGH_RES_TIMER;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 성능 모니터링 함수 구현
// ============================================================================

static ETResult linux_get_cpu_usage(float* usage_percent) {
    if (!usage_percent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    static unsigned long long prev_idle = 0, prev_total = 0;
    char buffer[1024];

    if (linux_read_proc_file("/proc/stat", buffer, sizeof(buffer)) != ET_SUCCESS) {
        return ET_ERROR_SYSTEM;
    }

    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
        return ET_ERROR_SYSTEM;
    }

    unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;

    if (prev_total != 0) {
        unsigned long long total_diff = total - prev_total;
        unsigned long long idle_diff = idle - prev_idle;

        if (total_diff > 0) {
            *usage_percent = 100.0f * (1.0f - (float)idle_diff / (float)total_diff);
        } else {
            *usage_percent = 0.0f;
        }
    } else {
        *usage_percent = 0.0f;
    }

    prev_idle = idle;
    prev_total = total;

    return ET_SUCCESS;
}

static ETResult linux_get_memory_usage(ETMemoryUsage* usage) {
    if (!usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(usage, 0, sizeof(ETMemoryUsage));

    // 프로세스 메모리 정보
    char buffer[1024];
    if (linux_read_proc_file("/proc/self/status", buffer, sizeof(buffer)) == ET_SUCCESS) {
        char* line = strtok(buffer, "\n");
        while (line) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                unsigned long kb;
                if (sscanf(line, "VmRSS: %lu kB", &kb) == 1) {
                    usage->process_memory_usage = kb * 1024;
                }
            } else if (strncmp(line, "VmHWM:", 6) == 0) {
                unsigned long kb;
                if (sscanf(line, "VmHWM: %lu kB", &kb) == 1) {
                    usage->process_peak_memory = kb * 1024;
                }
            }
            line = strtok(NULL, "\n");
        }
    }

    // 시스템 메모리 사용률
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        uint64_t used_memory = (si.totalram - si.freeram) * si.mem_unit;
        usage->memory_usage_percent = 100.0f * (float)used_memory / (float)(si.totalram * si.mem_unit);
    }

    // CPU 사용률
    linux_get_cpu_usage(&usage->cpu_usage_percent);

    return ET_SUCCESS;
}

static ETResult linux_get_process_memory_info(uint64_t* current_usage, uint64_t* peak_usage) {
    if (!current_usage || !peak_usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) {
        return ET_ERROR_SYSTEM;
    }

    *current_usage = ru.ru_maxrss * 1024; // Linux에서는 KB 단위
    *peak_usage = ru.ru_maxrss * 1024;

    return ET_SUCCESS;
}

static ETResult linux_get_system_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        return ET_ERROR_SYSTEM;
    }

    *uptime_seconds = si.uptime;
    return ET_SUCCESS;
}

static ETResult linux_get_process_uptime(uint64_t* uptime_seconds) {
    if (!uptime_seconds) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSystemData* platform_data = (ETLinuxSystemData*)
        et_get_system_interface()->platform_data;

    if (!platform_data || !platform_data->timing_initialized) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);

    *uptime_seconds = current_time.tv_sec - platform_data->process_start.tv_sec;
    return ET_SUCCESS;
}

// ============================================================================
// 헬퍼 함수 구현
// ============================================================================

static ETResult linux_read_proc_file(const char* path, char* buffer, size_t buffer_size) {
    if (!path || !buffer || buffer_size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(path, "r");
    if (!file) {
        return ET_ERROR_IO;
    }

    size_t bytes_read = fread(buffer, 1, buffer_size - 1, file);
    buffer[bytes_read] = '\0';

    fclose(file);
    return ET_SUCCESS;
}

static ETResult linux_parse_cpuinfo(ETCPUInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETCPUInfo));

    char buffer[4096];
    if (linux_read_proc_file("/proc/cpuinfo", buffer, sizeof(buffer)) != ET_SUCCESS) {
        return ET_ERROR_IO;
    }

    char* line = strtok(buffer, "\n");
    while (line) {
        if (strncmp(line, "vendor_id", 9) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                colon += 2; // ": " 건너뛰기
                strncpy(info->vendor, colon, sizeof(info->vendor) - 1);
            }
        } else if (strncmp(line, "model name", 10) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                colon += 2;
                strncpy(info->brand, colon, sizeof(info->brand) - 1);
            }
        } else if (strncmp(line, "cpu family", 10) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                info->family = atoi(colon + 2);
            }
        } else if (strncmp(line, "model", 5) == 0 && line[5] == '\t') {
            char* colon = strchr(line, ':');
            if (colon) {
                info->model = atoi(colon + 2);
            }
        } else if (strncmp(line, "stepping", 8) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                info->stepping = atoi(colon + 2);
            }
        } else if (strncmp(line, "cpu cores", 9) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                info->physical_cores = atoi(colon + 2);
            }
        } else if (strncmp(line, "siblings", 8) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                info->logical_cores = atoi(colon + 2);
            }
        } else if (strncmp(line, "cache size", 10) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                int cache_kb = atoi(colon + 2);
                info->l3_cache_size = cache_kb; // 일반적으로 L3 캐시
            }
        } else if (strncmp(line, "cpu MHz", 7) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                info->base_frequency_mhz = (uint32_t)atof(colon + 2);
                info->max_frequency_mhz = info->base_frequency_mhz;
            }
        }
        line = strtok(NULL, "\n");
    }

    // 기본값 설정
    if (info->logical_cores == 0) {
        info->logical_cores = get_nprocs();
    }
    if (info->physical_cores == 0) {
        info->physical_cores = info->logical_cores;
    }
    if (info->cache_line_size == 0) {
        info->cache_line_size = 64; // 기본값
    }
    if (info->l1_cache_size == 0) {
        info->l1_cache_size = 32; // 기본값 (KB)
    }
    if (info->l2_cache_size == 0) {
        info->l2_cache_size = 256; // 기본값 (KB)
    }

    return ET_SUCCESS;
}

static ETResult linux_parse_meminfo(ETMemoryInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETMemoryInfo));

    char buffer[2048];
    if (linux_read_proc_file("/proc/meminfo", buffer, sizeof(buffer)) != ET_SUCCESS) {
        return ET_ERROR_IO;
    }

    char* line = strtok(buffer, "\n");
    while (line) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            unsigned long kb;
            if (sscanf(line, "MemTotal: %lu kB", &kb) == 1) {
                info->total_physical = kb * 1024;
            }
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            unsigned long kb;
            if (sscanf(line, "MemAvailable: %lu kB", &kb) == 1) {
                info->available_physical = kb * 1024;
            }
        } else if (strncmp(line, "VmallocTotal:", 13) == 0) {
            unsigned long kb;
            if (sscanf(line, "VmallocTotal: %lu kB", &kb) == 1) {
                info->total_virtual = kb * 1024;
            }
        }
        line = strtok(NULL, "\n");
    }

    // 페이지 크기 설정
    info->page_size = getpagesize();
    info->allocation_granularity = info->page_size;

    // 사용 가능한 가상 메모리는 매우 큰 값으로 설정 (64비트 시스템)
    if (info->total_virtual == 0) {
        info->total_virtual = (uint64_t)1 << 47; // 128TB (일반적인 64비트 가상 주소 공간)
        info->available_virtual = info->total_virtual;
    } else {
        info->available_virtual = info->total_virtual;
    }

    return ET_SUCCESS;
}

static uint32_t linux_detect_simd_from_cpuinfo(void) {
    uint32_t features = ET_SIMD_NONE;
    char buffer[2048];

    if (linux_read_proc_file("/proc/cpuinfo", buffer, sizeof(buffer)) != ET_SUCCESS) {
        return features;
    }

    // flags 라인 찾기
    char* flags_line = strstr(buffer, "flags");
    if (!flags_line) {
        return features;
    }

    char* colon = strchr(flags_line, ':');
    if (!colon) {
        return features;
    }

    char* flags = colon + 2;

    // SIMD 기능 확인
    if (strstr(flags, "sse")) features |= ET_SIMD_SSE;
    if (strstr(flags, "sse2")) features |= ET_SIMD_SSE2;
    if (strstr(flags, "sse3")) features |= ET_SIMD_SSE3;
    if (strstr(flags, "ssse3")) features |= ET_SIMD_SSSE3;
    if (strstr(flags, "sse4_1")) features |= ET_SIMD_SSE4_1;
    if (strstr(flags, "sse4_2")) features |= ET_SIMD_SSE4_2;
    if (strstr(flags, "avx")) features |= ET_SIMD_AVX;
    if (strstr(flags, "avx2")) features |= ET_SIMD_AVX2;
    if (strstr(flags, "avx512")) features |= ET_SIMD_AVX512;
    if (strstr(flags, "fma")) features |= ET_SIMD_FMA;

    return features;
}

static uint32_t linux_detect_simd_from_cpuid(void) {
    uint32_t features = ET_SIMD_NONE;

#ifdef __x86_64__
    unsigned int eax, ebx, ecx, edx;

    // CPUID 지원 확인
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        // SSE 계열 확인
        if (edx & (1 << 25)) features |= ET_SIMD_SSE;
        if (edx & (1 << 26)) features |= ET_SIMD_SSE2;
        if (ecx & (1 << 0))  features |= ET_SIMD_SSE3;
        if (ecx & (1 << 9))  features |= ET_SIMD_SSSE3;
        if (ecx & (1 << 19)) features |= ET_SIMD_SSE4_1;
        if (ecx & (1 << 20)) features |= ET_SIMD_SSE4_2;
        if (ecx & (1 << 28)) features |= ET_SIMD_AVX;
        if (ecx & (1 << 12)) features |= ET_SIMD_FMA;
    }

    // AVX2 확인
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (ebx & (1 << 5)) features |= ET_SIMD_AVX2;
        if (ebx & (1 << 16)) features |= ET_SIMD_AVX512;
    }
#endif

    return features;
}

#endif // __linux__