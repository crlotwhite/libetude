/**
 * @file hardware_detect.c
 * @brief 하드웨어 기능 감지 모듈 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * CPU, GPU, 메모리 등 하드웨어 특성을 감지하고 최적화에 필요한 정보를 제공합니다.
 */

#include "libetude/hardware.h"
#include "libetude/config.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #include <intrin.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <mach/mach_host.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <cpuid.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <sched.h>
    #define _GNU_SOURCE
#endif

// ============================================================================
// 내부 함수 선언
// ============================================================================

static void _detect_cpu_vendor_and_brand(LibEtudeHardwareCPUInfo* cpu_info);
static void _detect_cpu_cache_info(LibEtudeHardwareCPUInfo* cpu_info);
static void _detect_cpu_frequency(LibEtudeHardwareCPUInfo* cpu_info);
static uint32_t _detect_simd_features_internal(void);
static void _detect_platform_info(LibEtudeHardwareInfo* info);

// ============================================================================
// CPUID 관련 유틸리티 함수
// ============================================================================

#if defined(_WIN32) || defined(__linux__)
static void _cpuid(int info[4], int function_id) {
#ifdef _WIN32
    __cpuid(info, function_id);
#elif defined(__linux__)
    __cpuid(function_id, info[0], info[1], info[2], info[3]);
#endif
}

static void _cpuidex(int info[4], int function_id, int subfunction_id) {
#ifdef _WIN32
    __cpuidex(info, function_id, subfunction_id);
#elif defined(__linux__)
    __cpuid_count(function_id, subfunction_id, info[0], info[1], info[2], info[3]);
#endif
}
#endif

// ============================================================================
// CPU 정보 감지 구현
// ============================================================================

LibEtudeErrorCode libetude_hardware_detect_cpu(LibEtudeHardwareCPUInfo* cpu_info) {
    if (!cpu_info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(cpu_info, 0, sizeof(LibEtudeHardwareCPUInfo));

    // CPU 제조사 및 브랜드 감지
    _detect_cpu_vendor_and_brand(cpu_info);

    // 코어 수 감지
#ifdef _WIN32
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    cpu_info->logical_cores = sys_info.dwNumberOfProcessors;

    // 물리 코어 수는 논리 코어 수의 절반으로 추정 (하이퍼스레딩 고려)
    cpu_info->physical_cores = cpu_info->logical_cores / 2;
    if (cpu_info->physical_cores == 0) {
        cpu_info->physical_cores = cpu_info->logical_cores;
    }

    cpu_info->cache_line_size = 64; // 일반적인 값
#elif defined(__APPLE__)
    size_t size = sizeof(uint32_t);
    sysctlbyname("hw.logicalcpu", &cpu_info->logical_cores, &size, NULL, 0);
    sysctlbyname("hw.physicalcpu", &cpu_info->physical_cores, &size, NULL, 0);

    size_t cache_line_size = 0;
    size = sizeof(size_t);
    if (sysctlbyname("hw.cachelinesize", &cache_line_size, &size, NULL, 0) == 0) {
        cpu_info->cache_line_size = (uint32_t)cache_line_size;
    } else {
        cpu_info->cache_line_size = 64; // 기본값
    }
#elif defined(__linux__)
    cpu_info->logical_cores = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_info->physical_cores = cpu_info->logical_cores; // 간단한 추정
    cpu_info->cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    if (cpu_info->cache_line_size == 0) {
        cpu_info->cache_line_size = 64; // 기본값
    }
#else
    cpu_info->logical_cores = 1;
    cpu_info->physical_cores = 1;
    cpu_info->cache_line_size = 64;
#endif

    // 캐시 정보 감지
    _detect_cpu_cache_info(cpu_info);

    // SIMD 기능 감지
    cpu_info->simd_features = _detect_simd_features_internal();

    // 주파수 정보 감지
    _detect_cpu_frequency(cpu_info);

    return LIBETUDE_SUCCESS;
}

static void _detect_cpu_vendor_and_brand(LibEtudeHardwareCPUInfo* cpu_info) {
#if defined(_WIN32) || defined(__linux__)
    int cpu_info_array[4];

    // CPU 제조사 정보 (CPUID function 0)
    _cpuid(cpu_info_array, 0);

    // 제조사 문자열 구성 (EBX, EDX, ECX 순서)
    memcpy(cpu_info->vendor, &cpu_info_array[1], 4);
    memcpy(cpu_info->vendor + 4, &cpu_info_array[3], 4);
    memcpy(cpu_info->vendor + 8, &cpu_info_array[2], 4);
    cpu_info->vendor[12] = '\0';

    // CPU 기본 정보 (CPUID function 1)
    _cpuid(cpu_info_array, 1);
    cpu_info->family = (cpu_info_array[0] >> 8) & 0xF;
    cpu_info->model = (cpu_info_array[0] >> 4) & 0xF;
    cpu_info->stepping = cpu_info_array[0] & 0xF;

    // 확장 패밀리/모델 처리
    if (cpu_info->family == 0xF) {
        cpu_info->family += (cpu_info_array[0] >> 20) & 0xFF;
    }
    if (cpu_info->family == 0x6 || cpu_info->family == 0xF) {
        cpu_info->model += ((cpu_info_array[0] >> 16) & 0xF) << 4;
    }

    // CPU 브랜드 문자열 (CPUID functions 0x80000002-0x80000004)
    char brand_string[49] = {0};
    for (int i = 0; i < 3; i++) {
        _cpuid(cpu_info_array, 0x80000002 + i);
        memcpy(brand_string + i * 16, cpu_info_array, 16);
    }

    // 앞뒤 공백 제거
    char* start = brand_string;
    while (*start == ' ') start++;

    char* end = start + strlen(start) - 1;
    while (end > start && *end == ' ') end--;
    *(end + 1) = '\0';

    strncpy(cpu_info->brand, start, sizeof(cpu_info->brand) - 1);
    cpu_info->brand[sizeof(cpu_info->brand) - 1] = '\0';

#elif defined(__APPLE__)
    size_t size = sizeof(cpu_info->vendor);
    if (sysctlbyname("machdep.cpu.vendor", cpu_info->vendor, &size, NULL, 0) != 0) {
        strcpy(cpu_info->vendor, "Unknown");
    }

    size = sizeof(cpu_info->brand);
    if (sysctlbyname("machdep.cpu.brand_string", cpu_info->brand, &size, NULL, 0) != 0) {
        strcpy(cpu_info->brand, "Unknown");
    }

    size = sizeof(uint32_t);
    sysctlbyname("machdep.cpu.family", &cpu_info->family, &size, NULL, 0);
    sysctlbyname("machdep.cpu.model", &cpu_info->model, &size, NULL, 0);
    sysctlbyname("machdep.cpu.stepping", &cpu_info->stepping, &size, NULL, 0);
#else
    strcpy(cpu_info->vendor, "Unknown");
    strcpy(cpu_info->brand, "Unknown");
    cpu_info->family = 0;
    cpu_info->model = 0;
    cpu_info->stepping = 0;
#endif
}

static void _detect_cpu_cache_info(LibEtudeHardwareCPUInfo* cpu_info) {
#ifdef __APPLE__
    size_t size = sizeof(uint32_t);
    uint64_t cache_size;

    size = sizeof(uint64_t);
    if (sysctlbyname("hw.l1icachesize", &cache_size, &size, NULL, 0) == 0) {
        cpu_info->l1_cache_size = (uint32_t)(cache_size / 1024);
    }
    if (sysctlbyname("hw.l2cachesize", &cache_size, &size, NULL, 0) == 0) {
        cpu_info->l2_cache_size = (uint32_t)(cache_size / 1024);
    }
    if (sysctlbyname("hw.l3cachesize", &cache_size, &size, NULL, 0) == 0) {
        cpu_info->l3_cache_size = (uint32_t)(cache_size / 1024);
    }
#elif defined(__linux__)
    // Linux에서는 /sys/devices/system/cpu/cpu0/cache/ 디렉토리에서 정보를 읽을 수 있지만
    // 간단한 구현을 위해 기본값 사용
    cpu_info->l1_cache_size = 32;  // 32KB (일반적인 값)
    cpu_info->l2_cache_size = 256; // 256KB
    cpu_info->l3_cache_size = 8192; // 8MB
#else
    // 기본값 설정
    cpu_info->l1_cache_size = 32;  // 32KB
    cpu_info->l2_cache_size = 256; // 256KB
    cpu_info->l3_cache_size = 8192; // 8MB
#endif
}

static void _detect_cpu_frequency(LibEtudeHardwareCPUInfo* cpu_info) {
#ifdef __APPLE__
    size_t size = sizeof(uint64_t);
    uint64_t freq;

    if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) == 0) {
        cpu_info->base_frequency_mhz = (uint32_t)(freq / 1000000);
        cpu_info->max_frequency_mhz = cpu_info->base_frequency_mhz;
    }

    if (sysctlbyname("hw.cpufrequency_max", &freq, &size, NULL, 0) == 0) {
        cpu_info->max_frequency_mhz = (uint32_t)(freq / 1000000);
    }
#else
    // 다른 플랫폼에서는 기본값 사용 (실제로는 더 복잡한 감지 로직 필요)
    cpu_info->base_frequency_mhz = 2400; // 2.4GHz (추정값)
    cpu_info->max_frequency_mhz = 3200;  // 3.2GHz (추정값)
#endif
}

// ============================================================================
// SIMD 기능 감지 구현
// ============================================================================

uint32_t libetude_hardware_detect_simd_features(void) {
    return _detect_simd_features_internal();
}

static uint32_t _detect_simd_features_internal(void) {
    uint32_t features = LIBETUDE_SIMD_NONE;

#if defined(_WIN32) || defined(__linux__)
    int cpu_info[4];

    // CPUID function 1 - 기본 기능 플래그
    _cpuid(cpu_info, 1);

    // EDX 레지스터에서 SSE 관련 기능 확인
    if (cpu_info[3] & (1 << 25)) features |= LIBETUDE_SIMD_SSE;
    if (cpu_info[3] & (1 << 26)) features |= LIBETUDE_SIMD_SSE2;

    // ECX 레지스터에서 SSE3, SSSE3, SSE4 확인
    if (cpu_info[2] & (1 << 0))  features |= LIBETUDE_SIMD_SSE3;
    if (cpu_info[2] & (1 << 9))  features |= LIBETUDE_SIMD_SSSE3;
    if (cpu_info[2] & (1 << 19)) features |= LIBETUDE_SIMD_SSE4_1;
    if (cpu_info[2] & (1 << 20)) features |= LIBETUDE_SIMD_SSE4_2;
    if (cpu_info[2] & (1 << 28)) features |= LIBETUDE_SIMD_AVX;

    // CPUID function 7 - 확장 기능 플래그
    _cpuidex(cpu_info, 7, 0);
    if (cpu_info[1] & (1 << 5)) features |= LIBETUDE_SIMD_AVX2;

#elif defined(__APPLE__)
    // macOS에서는 sysctlbyname을 사용하여 기능 확인
    uint32_t has_feature = 0;
    size_t size = sizeof(uint32_t);

    if (sysctlbyname("hw.optional.sse", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_SSE;
    }
    if (sysctlbyname("hw.optional.sse2", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_SSE2;
    }
    if (sysctlbyname("hw.optional.sse3", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_SSE3;
    }
    if (sysctlbyname("hw.optional.supplementalsse3", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_SSSE3;
    }
    if (sysctlbyname("hw.optional.sse4_1", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_SSE4_1;
    }
    if (sysctlbyname("hw.optional.sse4_2", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_SSE4_2;
    }
    if (sysctlbyname("hw.optional.avx1_0", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_AVX;
    }
    if (sysctlbyname("hw.optional.avx2_0", &has_feature, &size, NULL, 0) == 0 && has_feature) {
        features |= LIBETUDE_SIMD_AVX2;
    }

    // ARM Mac의 경우 NEON 지원
    #ifdef __aarch64__
    features |= LIBETUDE_SIMD_NEON;
    #endif

#elif defined(__ARM_NEON) || defined(__aarch64__)
    // ARM 플랫폼에서는 NEON 지원
    features |= LIBETUDE_SIMD_NEON;
#endif

    return features;
}

// ============================================================================
// GPU 정보 감지 구현
// ============================================================================

LibEtudeErrorCode libetude_hardware_detect_gpu(LibEtudeHardwareGPUInfo* gpu_info) {
    if (!gpu_info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(gpu_info, 0, sizeof(LibEtudeHardwareGPUInfo));

    // 기본적으로 GPU 사용 불가로 설정
    gpu_info->backend = LIBETUDE_GPU_NONE;
    gpu_info->available = false;
    strcpy(gpu_info->name, "No GPU");
    strcpy(gpu_info->vendor, "Unknown");

#ifdef _WIN32
    // Windows에서는 DirectX나 WMI를 통해 GPU 정보 조회 가능
    // 여기서는 간단한 구현으로 NVIDIA GPU 감지
    FILE* nvidia_smi = _popen("where nvidia-smi", "r");
    if (nvidia_smi) {
        char path[256];
        if (fgets(path, sizeof(path), nvidia_smi) != NULL) {
            // nvidia-smi가 존재하면 NVIDIA GPU가 있다고 가정
            gpu_info->backend = LIBETUDE_GPU_CUDA;
            strcpy(gpu_info->name, "NVIDIA GPU");
            strcpy(gpu_info->vendor, "NVIDIA");
            gpu_info->available = true;

            // nvidia-smi로 추가 정보 조회
            FILE* gpu_info_pipe = _popen("nvidia-smi --query-gpu=name,memory.total,count --format=csv,noheader", "r");
            if (gpu_info_pipe) {
                char info_line[256];
                if (fgets(info_line, sizeof(info_line), gpu_info_pipe) != NULL) {
                    // 첫 번째 GPU 정보만 사용
                    char* name_end = strchr(info_line, ',');
                    if (name_end) {
                        *name_end = '\0';
                        strncpy(gpu_info->name, info_line, sizeof(gpu_info->name) - 1);
                        gpu_info->name[sizeof(gpu_info->name) - 1] = '\0';

                        // 메모리 정보 파싱
                        char* mem_start = name_end + 1;
                        char* mem_end = strchr(mem_start, ' ');
                        if (mem_end) {
                            *mem_end = '\0';
                            gpu_info->total_memory = (size_t)(atof(mem_start) * 1024 * 1024); // MiB -> bytes
                            gpu_info->available_memory = gpu_info->total_memory / 2; // 추정값
                        }
                    }
                }
                _pclose(gpu_info_pipe);
            }

            // 기본값 설정
            if (gpu_info->total_memory == 0) {
                gpu_info->total_memory = 4ULL * 1024 * 1024 * 1024; // 4GB 기본값
                gpu_info->available_memory = 2ULL * 1024 * 1024 * 1024; // 2GB 기본값
            }

            gpu_info->compute_units = 16; // 기본값
            gpu_info->max_work_group_size = 1024; // 기본값
            gpu_info->core_clock_mhz = 1500; // 기본값
            gpu_info->memory_clock_mhz = 7000; // 기본값
        }
        _pclose(nvidia_smi);
    }

    // NVIDIA GPU가 감지되지 않으면 AMD GPU 확인
    if (!gpu_info->available) {
        // AMD GPU 감지 로직 (간단한 구현)
        FILE* amd_info = _popen("where amdgpu-pro-info", "r");
        if (amd_info) {
            char path[256];
            if (fgets(path, sizeof(path), amd_info) != NULL) {
                gpu_info->backend = LIBETUDE_GPU_OPENCL;
                strcpy(gpu_info->name, "AMD GPU");
                strcpy(gpu_info->vendor, "AMD");
                gpu_info->available = true;

                // 기본값 설정
                gpu_info->total_memory = 4ULL * 1024 * 1024 * 1024; // 4GB
                gpu_info->available_memory = 2ULL * 1024 * 1024 * 1024; // 2GB
                gpu_info->compute_units = 16;
                gpu_info->max_work_group_size = 1024;
                gpu_info->core_clock_mhz = 1500;
                gpu_info->memory_clock_mhz = 7000;
            }
            _pclose(amd_info);
        }
    }

#elif defined(__APPLE__)
    // macOS에서는 Metal 지원 여부 확인
    gpu_info->backend = LIBETUDE_GPU_METAL;

    // 시스템 프로파일러로 GPU 정보 조회 시도
    FILE* system_profiler = popen("system_profiler SPDisplaysDataType | grep 'Chipset Model'", "r");
    if (system_profiler) {
        char line[256];
        if (fgets(line, sizeof(line), system_profiler) != NULL) {
            // "Chipset Model: XXX" 형식에서 GPU 이름 추출
            char* name_start = strchr(line, ':');
            if (name_start) {
                name_start += 1;
                // 앞뒤 공백 제거
                while (*name_start == ' ') name_start++;
                char* name_end = name_start + strlen(name_start) - 1;
                while (name_end > name_start && (*name_end == '\n' || *name_end == ' ')) name_end--;
                *(name_end + 1) = '\0';

                strncpy(gpu_info->name, name_start, sizeof(gpu_info->name) - 1);
                gpu_info->name[sizeof(gpu_info->name) - 1] = '\0';
            }
        }
        pclose(system_profiler);
    }

    // GPU 이름이 설정되지 않았으면 기본값 사용
    if (strlen(gpu_info->name) == 0) {
        strcpy(gpu_info->name, "Apple GPU");
    }

    strcpy(gpu_info->vendor, "Apple");
    gpu_info->available = true;

    // 기본값 설정
    gpu_info->total_memory = 4ULL * 1024 * 1024 * 1024; // 4GB 기본값
    gpu_info->available_memory = 2ULL * 1024 * 1024 * 1024; // 2GB 기본값
    gpu_info->compute_units = 8; // 기본값
    gpu_info->max_work_group_size = 1024; // 기본값
    gpu_info->core_clock_mhz = 1000; // 기본값
    gpu_info->memory_clock_mhz = 5000; // 기본값

#elif defined(__linux__)
    // Linux에서는 NVIDIA GPU 감지
    FILE* nvidia_version = fopen("/proc/driver/nvidia/version", "r");
    if (nvidia_version) {
        gpu_info->backend = LIBETUDE_GPU_CUDA;
        strcpy(gpu_info->vendor, "NVIDIA");
        gpu_info->available = true;
        fclose(nvidia_version);

        // nvidia-smi로 추가 정보 조회
        FILE* gpu_info_pipe = popen("nvidia-smi --query-gpu=name,memory.total --format=csv,noheader", "r");
        if (gpu_info_pipe) {
            char info_line[256];
            if (fgets(info_line, sizeof(info_line), gpu_info_pipe) != NULL) {
                // 첫 번째 GPU 정보만 사용
                char* name_end = strchr(info_line, ',');
                if (name_end) {
                    *name_end = '\0';
                    strncpy(gpu_info->name, info_line, sizeof(gpu_info->name) - 1);
                    gpu_info->name[sizeof(gpu_info->name) - 1] = '\0';

                    // 메모리 정보 파싱
                    char* mem_start = name_end + 1;
                    char* mem_end = strchr(mem_start, ' ');
                    if (mem_end) {
                        *mem_end = '\0';
                        gpu_info->total_memory = (size_t)(atof(mem_start) * 1024 * 1024); // MiB -> bytes
                        gpu_info->available_memory = gpu_info->total_memory / 2; // 추정값
                    }
                }
            }
            pclose(gpu_info_pipe);
        }

        // GPU 이름이 설정되지 않았으면 기본값 사용
        if (strlen(gpu_info->name) == 0) {
            strcpy(gpu_info->name, "NVIDIA GPU");
        }
    } else {
        // NVIDIA GPU가 없으면 AMD GPU 확인
        FILE* amd_info = fopen("/sys/kernel/debug/dri/0/amdgpu_pm_info", "r");
        if (amd_info) {
            gpu_info->backend = LIBETUDE_GPU_OPENCL;
            strcpy(gpu_info->name, "AMD GPU");
            strcpy(gpu_info->vendor, "AMD");
            gpu_info->available = true;
            fclose(amd_info);
        } else {
            // 마지막으로 OpenCL 지원 확인 (간단한 구현)
            FILE* opencl_check = popen("which clinfo", "r");
            if (opencl_check) {
                char path[256];
                if (fgets(path, sizeof(path), opencl_check) != NULL) {
                    // clinfo가 있으면 OpenCL 지원 가능성 있음
                    gpu_info->backend = LIBETUDE_GPU_OPENCL;
                    strcpy(gpu_info->name, "Generic OpenCL Device");
                    strcpy(gpu_info->vendor, "Unknown");
                    gpu_info->available = true;
                }
                pclose(opencl_check);
            }
        }
    }

    // 기본값 설정 (GPU가 감지된 경우)
    if (gpu_info->available && gpu_info->total_memory == 0) {
        gpu_info->total_memory = 4ULL * 1024 * 1024 * 1024; // 4GB 기본값
        gpu_info->available_memory = 2ULL * 1024 * 1024 * 1024; // 2GB 기본값
        gpu_info->compute_units = 16; // 기본값
        gpu_info->max_work_group_size = 1024; // 기본값
        gpu_info->core_clock_mhz = 1500; // 기본값
        gpu_info->memory_clock_mhz = 7000; // 기본값
    }
#endif

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 메모리 정보 감지 구현
// ============================================================================

LibEtudeErrorCode libetude_hardware_detect_memory(LibEtudeHardwareMemoryInfo* memory_info) {
    if (!memory_info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(memory_info, 0, sizeof(LibEtudeHardwareMemoryInfo));

#ifdef _WIN32
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);

    if (GlobalMemoryStatusEx(&mem_status)) {
        memory_info->total_physical = mem_status.ullTotalPhys;
        memory_info->available_physical = mem_status.ullAvailPhys;
        memory_info->total_virtual = mem_status.ullTotalVirtual;
        memory_info->available_virtual = mem_status.ullAvailVirtual;
    }

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    memory_info->page_size = sys_info.dwPageSize;
    memory_info->allocation_granularity = sys_info.dwAllocationGranularity;

    // 메모리 제약 감지
    // 사용 가능한 메모리가 총 메모리의 20% 미만이면 제약 상태로 간주
    if (memory_info->available_physical < (memory_info->total_physical / 5)) {
        memory_info->memory_constrained = true;
    }

    // 메모리 대역폭 추정 (CPU 모델에 따라 다름)
    // 여기서는 간단한 추정값 사용
    memory_info->memory_bandwidth_gbps = 25; // 25 GB/s (추정값)

    // 메모리 사용량 추적을 위한 초기 값 설정
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        memory_info->process_memory_usage = pmc.WorkingSetSize;
        memory_info->process_peak_memory_usage = pmc.PeakWorkingSetSize;
    }

#elif defined(__APPLE__)
    // 물리 메모리 크기
    uint64_t mem_size;
    size_t size = sizeof(mem_size);
    if (sysctlbyname("hw.memsize", &mem_size, &size, NULL, 0) == 0) {
        memory_info->total_physical = mem_size;
    }

    // 사용 가능한 메모리 (vm_stat 사용)
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t host_size = sizeof(vm_stat) / sizeof(natural_t);

    if (host_page_size(mach_host_self(), &page_size) == KERN_SUCCESS &&
        host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS) {

        memory_info->available_physical = (vm_stat.free_count + vm_stat.inactive_count) * page_size;
        memory_info->page_size = (uint32_t)page_size;
    }

    memory_info->total_virtual = memory_info->total_physical * 2; // 추정값
    memory_info->available_virtual = memory_info->available_physical * 2;
    memory_info->allocation_granularity = memory_info->page_size;

    // 메모리 제약 감지
    if (memory_info->available_physical < (memory_info->total_physical / 5)) {
        memory_info->memory_constrained = true;
    }

    // 메모리 대역폭 추정 (Apple Silicon vs Intel)
    bool is_apple_silicon = false;
    size = sizeof(is_apple_silicon);
    sysctlbyname("hw.optional.arm64", &is_apple_silicon, &size, NULL, 0);

    if (is_apple_silicon) {
        // Apple Silicon은 통합 메모리로 대역폭이 높음
        memory_info->memory_bandwidth_gbps = 70; // 70 GB/s (M1 기준 추정값)
    } else {
        // Intel Mac은 일반적인 DDR4 대역폭
        memory_info->memory_bandwidth_gbps = 25; // 25 GB/s (추정값)
    }

    // 현재 프로세스 메모리 사용량
    struct task_basic_info info;
    mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &info_count) == KERN_SUCCESS) {
        memory_info->process_memory_usage = info.resident_size;
        memory_info->process_peak_memory_usage = info.resident_size; // macOS에서는 peak 값을 직접 제공하지 않음
    }

#elif defined(__linux__)
    struct sysinfo sys_info;
    if (sysinfo(&sys_info) == 0) {
        memory_info->total_physical = sys_info.totalram * sys_info.mem_unit;
        memory_info->available_physical = sys_info.freeram * sys_info.mem_unit;
        memory_info->total_virtual = sys_info.totalswap * sys_info.mem_unit + memory_info->total_physical;
        memory_info->available_virtual = sys_info.freeswap * sys_info.mem_unit + memory_info->available_physical;
    }

    memory_info->page_size = sysconf(_SC_PAGESIZE);
    memory_info->allocation_granularity = memory_info->page_size;

    // 메모리 제약 감지
    if (memory_info->available_physical < (memory_info->total_physical / 5)) {
        memory_info->memory_constrained = true;
    }

    // 메모리 대역폭 추정 (CPU 모델에 따라 다름)
    // 여기서는 간단한 추정값 사용
    memory_info->memory_bandwidth_gbps = 25; // 25 GB/s (추정값)

    // 현재 프로세스 메모리 사용량
    FILE* stat_file = fopen("/proc/self/status", "r");
    if (stat_file) {
        char line[128];
        while (fgets(line, sizeof(line), stat_file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                // VmRSS는 KB 단위로 제공됨
                memory_info->process_memory_usage = strtoull(line + 6, NULL, 10) * 1024;
            } else if (strncmp(line, "VmHWM:", 6) == 0) {
                // VmHWM은 KB 단위로 제공됨
                memory_info->process_peak_memory_usage = strtoull(line + 6, NULL, 10) * 1024;
            }
        }
        fclose(stat_file);
    }
#endif

    // 메모리 제약 상태에 따른 추가 정보 설정
    if (memory_info->memory_constrained) {
        // 제약 상태에서는 더 작은 메모리 풀 크기 권장
        memory_info->recommended_pool_size = memory_info->available_physical / 8;
    } else {
        // 일반 상태에서는 더 큰 메모리 풀 크기 권장
        memory_info->recommended_pool_size = memory_info->available_physical / 4;
    }

    // 최소/최대 제한 적용
    const size_t min_pool_size = 64 * 1024 * 1024;   // 64MB
    const size_t max_pool_size = 2ULL * 1024 * 1024 * 1024; // 2GB

    if (memory_info->recommended_pool_size < min_pool_size) {
        memory_info->recommended_pool_size = min_pool_size;
    } else if (memory_info->recommended_pool_size > max_pool_size) {
        memory_info->recommended_pool_size = max_pool_size;
    }

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 통합 하드웨어 정보 감지
// ============================================================================

LibEtudeErrorCode libetude_hardware_detect(LibEtudeHardwareInfo* info) {
    if (!info) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 구조체 초기화
    memset(info, 0, sizeof(LibEtudeHardwareInfo));

    // 각 하드웨어 컴포넌트 정보 감지
    LibEtudeErrorCode result;

    result = libetude_hardware_detect_cpu(&info->cpu);
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    result = libetude_hardware_detect_gpu(&info->gpu);
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    result = libetude_hardware_detect_memory(&info->memory);
    if (result != LIBETUDE_SUCCESS) {
        return result;
    }

    // 플랫폼 정보 감지
    _detect_platform_info(info);

    // 성능 등급 계산
    info->performance_tier = libetude_hardware_calculate_performance_tier(info);

    info->initialized = true;
    return LIBETUDE_SUCCESS;
}

static void _detect_platform_info(LibEtudeHardwareInfo* info) {
#ifdef _WIN32
    strcpy(info->platform_name, "Windows");

    // Windows 버전 정보
    OSVERSIONINFOEX os_info;
    memset(&os_info, 0, sizeof(os_info));
    os_info.dwOSVersionInfoSize = sizeof(os_info);

    // GetVersionEx는 deprecated이지만 간단한 구현을 위해 사용
    #pragma warning(push)
    #pragma warning(disable: 4996)
    if (GetVersionEx((OSVERSIONINFO*)&os_info)) {
        snprintf(info->os_version, sizeof(info->os_version),
                "%lu.%lu.%lu",
                os_info.dwMajorVersion,
                os_info.dwMinorVersion,
                os_info.dwBuildNumber);
    }
    #pragma warning(pop)

#elif defined(__APPLE__)
    strcpy(info->platform_name, "macOS");

    // macOS 버전 정보
    size_t size = sizeof(info->os_version);
    if (sysctlbyname("kern.version", info->os_version, &size, NULL, 0) != 0) {
        strcpy(info->os_version, "Unknown");
    }

#elif defined(__linux__)
    strcpy(info->platform_name, "Linux");

    // Linux 버전 정보 (간단한 구현)
    FILE* version_file = fopen("/proc/version", "r");
    if (version_file) {
        if (fgets(info->os_version, sizeof(info->os_version), version_file) != NULL) {
            // 첫 번째 줄만 사용하고 개행 문자 제거
            char* newline = strchr(info->os_version, '\n');
            if (newline) *newline = '\0';
        }
        fclose(version_file);
    } else {
        strcpy(info->os_version, "Unknown");
    }

#else
    strcpy(info->platform_name, "Unknown");
    strcpy(info->os_version, "Unknown");
#endif
}

// ============================================================================
// 하드웨어 정보 조회 및 계산 함수
// ============================================================================

uint32_t libetude_hardware_calculate_performance_tier(const LibEtudeHardwareInfo* info) {
    if (!info || !info->initialized) {
        return 1; // 최저 등급
    }

    uint32_t score = 0;

    // CPU 성능 점수 (최대 40점)
    score += (info->cpu.physical_cores > 8) ? 15 : (info->cpu.physical_cores * 2);
    score += (info->cpu.simd_features & LIBETUDE_SIMD_AVX2) ? 10 :
             (info->cpu.simd_features & LIBETUDE_SIMD_AVX) ? 8 :
             (info->cpu.simd_features & LIBETUDE_SIMD_SSE4_2) ? 6 : 3;
    score += (info->cpu.max_frequency_mhz > 3000) ? 10 :
             (info->cpu.max_frequency_mhz > 2500) ? 8 : 5;
    score += (info->cpu.l3_cache_size > 16384) ? 5 :
             (info->cpu.l3_cache_size > 8192) ? 3 : 2;

    // 메모리 성능 점수 (최대 30점)
    size_t total_gb = info->memory.total_physical / (1024 * 1024 * 1024);
    score += (total_gb >= 32) ? 15 : (total_gb >= 16) ? 12 : (total_gb >= 8) ? 8 : 4;
    score += (info->memory.memory_bandwidth_gbps > 40) ? 10 :
             (info->memory.memory_bandwidth_gbps > 25) ? 8 : 5;
    score += 5; // 기본 메모리 점수

    // GPU 성능 점수 (최대 30점)
    if (info->gpu.available) {
        score += (info->gpu.backend == LIBETUDE_GPU_CUDA) ? 15 :
                 (info->gpu.backend == LIBETUDE_GPU_METAL) ? 12 :
                 (info->gpu.backend == LIBETUDE_GPU_OPENCL) ? 10 : 5;
        score += (info->gpu.total_memory > 8ULL * 1024 * 1024 * 1024) ? 10 :
                 (info->gpu.total_memory > 4ULL * 1024 * 1024 * 1024) ? 8 : 5;
        score += 5; // GPU 사용 가능 보너스
    }

    // 점수를 1-5 등급으로 변환
    if (score >= 80) return 5;      // 최고 성능
    else if (score >= 65) return 4; // 고성능
    else if (score >= 50) return 3; // 중간 성능
    else if (score >= 35) return 2; // 저성능
    else return 1;                  // 최저 성능
}

uint32_t libetude_hardware_get_optimal_thread_count(const LibEtudeHardwareCPUInfo* cpu_info) {
    if (!cpu_info) {
        return 1;
    }

    // 물리 코어 수를 기준으로 하되, 최소 1개, 최대 16개로 제한
    uint32_t optimal_threads = cpu_info->physical_cores;

    if (optimal_threads == 0) {
        optimal_threads = cpu_info->logical_cores / 2;
    }

    if (optimal_threads == 0) {
        optimal_threads = 1;
    } else if (optimal_threads > 16) {
        optimal_threads = 16;
    }

    return optimal_threads;
}

size_t libetude_hardware_get_optimal_memory_pool_size(const LibEtudeHardwareMemoryInfo* memory_info) {
    if (!memory_info) {
        return 64 * 1024 * 1024; // 64MB 기본값
    }

    // 이미 계산된 권장 메모리 풀 크기가 있으면 그것을 사용
    if (memory_info->recommended_pool_size > 0) {
        return memory_info->recommended_pool_size;
    }

    // 권장 크기가 설정되지 않은 경우 (이전 버전과의 호환성을 위해)
    // 사용 가능한 물리 메모리의 25%를 메모리 풀로 사용
    size_t pool_size = memory_info->available_physical / 4;

    // 메모리 제약 상태인 경우 더 작은 크기 사용
    if (memory_info->memory_constrained) {
        pool_size = memory_info->available_physical / 8;
    }

    // 최소 64MB, 최대 2GB로 제한
    const size_t min_size = 64 * 1024 * 1024;   // 64MB
    const size_t max_size = 2ULL * 1024 * 1024 * 1024; // 2GB

    if (pool_size < min_size) {
        pool_size = min_size;
    } else if (pool_size > max_size) {
        pool_size = max_size;
    }

    return pool_size;
}

bool libetude_hardware_is_gpu_available(const LibEtudeHardwareGPUInfo* gpu_info) {
    return gpu_info && gpu_info->available && gpu_info->backend != LIBETUDE_GPU_NONE;
}

// ============================================================================
// 디버그 및 정보 출력 함수
// ============================================================================

void libetude_hardware_print_info(const LibEtudeHardwareInfo* info) {
    if (!info || !info->initialized) {
        printf("하드웨어 정보가 초기화되지 않았습니다.\n");
        return;
    }

    printf("=== LibEtude 하드웨어 정보 ===\n");
    printf("플랫폼: %s\n", info->platform_name);
    printf("OS 버전: %s\n", info->os_version);
    printf("성능 등급: %u/5\n\n", info->performance_tier);

    // CPU 정보
    printf("--- CPU 정보 ---\n");
    printf("제조사: %s\n", info->cpu.vendor);
    printf("브랜드: %s\n", info->cpu.brand);
    printf("물리 코어: %u개\n", info->cpu.physical_cores);
    printf("논리 코어: %u개\n", info->cpu.logical_cores);
    printf("기본 주파수: %u MHz\n", info->cpu.base_frequency_mhz);
    printf("최대 주파수: %u MHz\n", info->cpu.max_frequency_mhz);
    printf("L1 캐시: %u KB\n", info->cpu.l1_cache_size);
    printf("L2 캐시: %u KB\n", info->cpu.l2_cache_size);
    printf("L3 캐시: %u KB\n", info->cpu.l3_cache_size);

    char simd_features[256];
    libetude_hardware_simd_features_to_string(info->cpu.simd_features, simd_features, sizeof(simd_features));
    printf("SIMD 기능: %s\n\n", simd_features);

    // GPU 정보
    printf("--- GPU 정보 ---\n");
    printf("Available: %s\n", info->gpu.available ? "Yes" : "No");
    if (info->gpu.available) {
        printf("이름: %s\n", info->gpu.name);
        printf("제조사: %s\n", info->gpu.vendor);
        printf("백엔드: %s\n",
               (info->gpu.backend == LIBETUDE_GPU_CUDA) ? "CUDA" :
               (info->gpu.backend == LIBETUDE_GPU_OPENCL) ? "OpenCL" :
               (info->gpu.backend == LIBETUDE_GPU_METAL) ? "Metal" : "없음");
        printf("총 메모리: %.2f GB\n", (double)info->gpu.total_memory / (1024.0 * 1024.0 * 1024.0));
    }
    printf("\n");

    // 메모리 정보
    printf("--- 메모리 정보 ---\n");
    printf("총 물리 메모리: %.2f GB\n", (double)info->memory.total_physical / (1024.0 * 1024.0 * 1024.0));
    printf("사용 가능한 물리 메모리: %.2f GB\n", (double)info->memory.available_physical / (1024.0 * 1024.0 * 1024.0));
    printf("페이지 크기: %u bytes\n", info->memory.page_size);
    printf("메모리 대역폭: %u GB/s (추정)\n", info->memory.memory_bandwidth_gbps);
    printf("메모리 제약 상태: %s\n", info->memory.memory_constrained ? "예" : "아니오");
    printf("권장 메모리 풀 크기: %.2f MB\n", (double)info->memory.recommended_pool_size / (1024.0 * 1024.0));
    printf("현재 프로세스 메모리 사용량: %.2f MB\n", (double)info->memory.process_memory_usage / (1024.0 * 1024.0));
    printf("최대 프로세스 메모리 사용량: %.2f MB\n", (double)info->memory.process_peak_memory_usage / (1024.0 * 1024.0));
    printf("\n");

    // 최적화 권장사항
    printf("--- 최적화 권장사항 ---\n");
    printf("권장 스레드 수: %u개\n", libetude_hardware_get_optimal_thread_count(&info->cpu));
    printf("권장 메모리 풀 크기: %.2f MB\n",
           (double)libetude_hardware_get_optimal_memory_pool_size(&info->memory) / (1024.0 * 1024.0));
    printf("GPU 가속: %s\n", libetude_hardware_is_gpu_available(&info->gpu) ? "권장" : "사용 불가");
}

LibEtudeErrorCode libetude_hardware_simd_features_to_string(uint32_t features,
                                                           char* buffer,
                                                           size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    buffer[0] = '\0';
    bool first = true;

    if (features == LIBETUDE_SIMD_NONE) {
        strncpy(buffer, "None", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return LIBETUDE_SUCCESS;
    }

    const struct {
        uint32_t flag;
        const char* name;
    } simd_names[] = {
        {LIBETUDE_SIMD_SSE, "SSE"},
        {LIBETUDE_SIMD_SSE2, "SSE2"},
        {LIBETUDE_SIMD_SSE3, "SSE3"},
        {LIBETUDE_SIMD_SSSE3, "SSSE3"},
        {LIBETUDE_SIMD_SSE4_1, "SSE4.1"},
        {LIBETUDE_SIMD_SSE4_2, "SSE4.2"},
        {LIBETUDE_SIMD_AVX, "AVX"},
        {LIBETUDE_SIMD_AVX2, "AVX2"},
        {LIBETUDE_SIMD_NEON, "NEON"}
    };

    for (size_t i = 0; i < sizeof(simd_names) / sizeof(simd_names[0]); i++) {
        if (features & simd_names[i].flag) {
            if (!first) {
                strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
            }
            strncat(buffer, simd_names[i].name, buffer_size - strlen(buffer) - 1);
            first = false;
        }
    }

    return LIBETUDE_SUCCESS;
}