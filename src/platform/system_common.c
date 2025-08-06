/**
 * @file system_common.c
 * @brief 시스템 정보 추상화 레이어 공통 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/system.h"
#include "libetude/platform/factory.h"
#include "libetude/memory.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// 전역 변수
// ============================================================================

static ETSystemInterface* g_system_interface = NULL;
static bool g_system_interface_initialized = false;

// ============================================================================
// 플랫폼별 구현 함수 선언
// ============================================================================

#ifdef _WIN32
extern ETResult et_system_interface_create_windows(ETSystemInterface** interface);
#elif defined(__linux__)
extern ETResult et_system_interface_create_linux(ETSystemInterface** interface);
#elif defined(__APPLE__)
extern ETResult et_system_interface_create_macos(ETSystemInterface** interface);
#endif

// ============================================================================
// 공통 인터페이스 함수 구현
// ============================================================================

ETResult et_system_interface_create(ETSystemInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETResult result = ET_ERROR_NOT_IMPLEMENTED;

#ifdef _WIN32
    result = et_system_interface_create_windows(interface);
#elif defined(__linux__)
    result = et_system_interface_create_linux(interface);
#elif defined(__APPLE__)
    result = et_system_interface_create_macos(interface);
#endif

    return result;
}

void et_system_interface_destroy(ETSystemInterface* interface) {
    if (interface) {
        if (interface->platform_data) {
            libetude_free(interface->platform_data);
        }
        libetude_free(interface);
    }
}

const ETSystemInterface* et_get_system_interface(void) {
    if (!g_system_interface_initialized) {
        ETResult result = et_system_interface_create(&g_system_interface);
        if (result != ET_SUCCESS) {
            return NULL;
        }
        g_system_interface_initialized = true;
    }
    return g_system_interface;
}

// ============================================================================
// 편의 함수 구현
// ============================================================================

ETResult et_get_system_info(ETSystemInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->get_system_info) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return interface->get_system_info(info);
}

ETResult et_get_memory_info(ETMemoryInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->get_memory_info) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return interface->get_memory_info(info);
}

ETResult et_get_cpu_info(ETCPUInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->get_cpu_info) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return interface->get_cpu_info(info);
}

ETResult et_get_high_resolution_time(uint64_t* time_ns) {
    if (!time_ns) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->get_high_resolution_time) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return interface->get_high_resolution_time(time_ns);
}

ETResult et_sleep(uint32_t milliseconds) {
    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->sleep) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return interface->sleep(milliseconds);
}

uint32_t et_get_simd_features(void) {
    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->get_simd_features) {
        return ET_SIMD_NONE;
    }

    return interface->get_simd_features();
}

bool et_has_hardware_feature(ETHardwareFeature feature) {
    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->has_feature) {
        return false;
    }

    return interface->has_feature(feature);
}

ETResult et_get_cpu_usage(float* usage_percent) {
    if (!usage_percent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->get_cpu_usage) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return interface->get_cpu_usage(usage_percent);
}

ETResult et_get_memory_usage(ETMemoryUsage* usage) {
    if (!usage) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETSystemInterface* interface = et_get_system_interface();
    if (!interface || !interface->get_memory_usage) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return interface->get_memory_usage(usage);
}

// ============================================================================
// 유틸리티 함수 구현
// ============================================================================

ETResult et_simd_features_to_string(uint32_t features, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    buffer[0] = '\0';
    size_t pos = 0;

    if (features == ET_SIMD_NONE) {
        snprintf(buffer, buffer_size, "None");
        return ET_SUCCESS;
    }

    const struct {
        uint32_t flag;
        const char* name;
    } simd_names[] = {
        {ET_SIMD_SSE, "SSE"},
        {ET_SIMD_SSE2, "SSE2"},
        {ET_SIMD_SSE3, "SSE3"},
        {ET_SIMD_SSSE3, "SSSE3"},
        {ET_SIMD_SSE4_1, "SSE4.1"},
        {ET_SIMD_SSE4_2, "SSE4.2"},
        {ET_SIMD_AVX, "AVX"},
        {ET_SIMD_AVX2, "AVX2"},
        {ET_SIMD_AVX512, "AVX-512"},
        {ET_SIMD_NEON, "NEON"},
        {ET_SIMD_FMA, "FMA"}
    };

    for (size_t i = 0; i < sizeof(simd_names) / sizeof(simd_names[0]); i++) {
        if (features & simd_names[i].flag) {
            if (pos > 0) {
                if (pos + 2 >= buffer_size) break;
                strcat(buffer + pos, ", ");
                pos += 2;
            }
            size_t name_len = strlen(simd_names[i].name);
            if (pos + name_len >= buffer_size) break;
            strcat(buffer + pos, simd_names[i].name);
            pos += name_len;
        }
    }

    return ET_SUCCESS;
}

void et_print_system_info(const ETSystemInfo* info) {
    if (!info) return;

    printf("=== 시스템 정보 ===\n");
    printf("시스템 이름: %s\n", info->system_name);
    printf("OS 버전: %s\n", info->os_version);
    printf("CPU 이름: %s\n", info->cpu_name);
    printf("CPU 코어 수: %u\n", info->cpu_count);
    printf("CPU 주파수: %u MHz\n", info->cpu_frequency);
    printf("총 메모리: %.2f GB\n", (double)info->total_memory / (1024.0 * 1024.0 * 1024.0));
    printf("사용 가능한 메모리: %.2f GB\n", (double)info->available_memory / (1024.0 * 1024.0 * 1024.0));
    printf("플랫폼: %d\n", info->platform_type);
    printf("아키텍처: %d\n", info->architecture);
    printf("\n");
}

void et_print_memory_info(const ETMemoryInfo* info) {
    if (!info) return;

    printf("=== 메모리 정보 ===\n");
    printf("총 물리 메모리: %.2f GB\n", (double)info->total_physical / (1024.0 * 1024.0 * 1024.0));
    printf("사용 가능한 물리 메모리: %.2f GB\n", (double)info->available_physical / (1024.0 * 1024.0 * 1024.0));
    printf("총 가상 메모리: %.2f GB\n", (double)info->total_virtual / (1024.0 * 1024.0 * 1024.0));
    printf("사용 가능한 가상 메모리: %.2f GB\n", (double)info->available_virtual / (1024.0 * 1024.0 * 1024.0));
    printf("페이지 크기: %u bytes\n", info->page_size);
    printf("할당 단위: %u bytes\n", info->allocation_granularity);
    printf("\n");
}

void et_print_cpu_info(const ETCPUInfo* info) {
    if (!info) return;

    printf("=== CPU 정보 ===\n");
    printf("제조사: %s\n", info->vendor);
    printf("브랜드: %s\n", info->brand);
    printf("패밀리: %u, 모델: %u, 스테핑: %u\n", info->family, info->model, info->stepping);
    printf("물리 코어: %u, 논리 코어: %u\n", info->physical_cores, info->logical_cores);
    printf("캐시 라인 크기: %u bytes\n", info->cache_line_size);
    printf("L1 캐시: %u KB, L2 캐시: %u KB, L3 캐시: %u KB\n",
           info->l1_cache_size, info->l2_cache_size, info->l3_cache_size);
    printf("기본 주파수: %u MHz, 최대 주파수: %u MHz\n",
           info->base_frequency_mhz, info->max_frequency_mhz);
    printf("\n");
}

// ============================================================================
// 정리 함수
// ============================================================================

void et_system_interface_cleanup(void) {
    if (g_system_interface) {
        et_system_interface_destroy(g_system_interface);
        g_system_interface = NULL;
        g_system_interface_initialized = false;
    }
}