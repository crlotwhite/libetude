/**
 * @file factory.c
 * @brief 플랫폼 추상화 레이어의 팩토리 패턴 구현
 * @author LibEtude Team
 */

#include "libetude/platform/factory.h"
#include "libetude/platform/common.h"
#include <string.h>
#include <stdlib.h>

/* 전역 플랫폼 컨텍스트 */
ETPlatformContext g_platform_context = {0};

/* 인터페이스 타입 이름 배열 */
static const char* interface_type_names[ET_INTERFACE_COUNT] = {
    "Audio",
    "System",
    "Thread",
    "Memory",
    "Filesystem",
    "Network",
    "DynamicLibrary"
};

/* 플랫폼 초기화 */
ETResult et_platform_initialize(void) {
    if (g_platform_context.initialized) {
        return ET_SUCCESS;
    }

    /* 플랫폼 정보 수집 */
    ETResult result = et_get_platform_info(&g_platform_context.platform_info);
    if (result != ET_SUCCESS) {
        et_set_detailed_error(&g_platform_context.last_error, result, 0,
                             "Failed to get platform information", __FILE__, __LINE__, __func__);
        return result;
    }

    /* 레지스트리 초기화 */
    memset(g_platform_context.registry, 0, sizeof(g_platform_context.registry));
    memset(g_platform_context.interfaces, 0, sizeof(g_platform_context.interfaces));

    /* 플랫폼별 인터페이스 등록 */
    ETPlatformType platform = g_platform_context.platform_info.type;

    switch (platform) {
#ifdef LIBETUDE_PLATFORM_WINDOWS
        case ET_PLATFORM_WINDOWS:
            result = et_register_windows_interfaces();
            break;
#endif
#ifdef LIBETUDE_PLATFORM_LINUX
        case ET_PLATFORM_LINUX:
            result = et_register_linux_interfaces();
            break;
#endif
#ifdef LIBETUDE_PLATFORM_MACOS
        case ET_PLATFORM_MACOS:
            result = et_register_macos_interfaces();
            break;
#endif
        default:
            result = ET_ERROR_NOT_SUPPORTED;
            et_set_detailed_error(&g_platform_context.last_error, result, 0,
                                 "Unsupported platform", __FILE__, __LINE__, __func__);
            return result;
    }

    if (result != ET_SUCCESS) {
        et_set_detailed_error(&g_platform_context.last_error, result, 0,
                             "Failed to register platform interfaces", __FILE__, __LINE__, __func__);
        return result;
    }

    g_platform_context.initialized = true;
    return ET_SUCCESS;
}

/* 플랫폼 정리 */
void et_platform_finalize(void) {
    if (!g_platform_context.initialized) {
        return;
    }

    /* 생성된 인터페이스들 정리 */
    for (int i = 0; i < ET_INTERFACE_COUNT; i++) {
        if (g_platform_context.interfaces[i]) {
            et_destroy_interface((ETInterfaceType)i, g_platform_context.interfaces[i]);
        }
    }

    /* 컨텍스트 초기화 */
    memset(&g_platform_context, 0, sizeof(g_platform_context));
}

/* 인터페이스 팩토리 등록 */
ETResult et_register_interface_factory(ETInterfaceType type, ETPlatformType platform,
                                      ETInterfaceFactory factory, ETInterfaceDestructor destructor,
                                      const ETInterfaceMetadata* metadata) {
    if (type >= ET_INTERFACE_COUNT || !factory || !metadata) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETInterfaceRegistry* registry = &g_platform_context.registry[type];

    registry->type = type;
    registry->platform = platform;
    registry->factory = factory;
    registry->destructor = destructor;
    registry->metadata = *metadata;
    registry->is_available = true;

    return ET_SUCCESS;
}

/* 인터페이스 생성 */
ETResult et_create_interface(ETInterfaceType type, void** interface) {
    if (type >= ET_INTERFACE_COUNT || !interface) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    if (!g_platform_context.initialized) {
        ETResult result = et_platform_initialize();
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    ETInterfaceRegistry* registry = &g_platform_context.registry[type];

    if (!registry->is_available || !registry->factory) {
        et_set_detailed_error(&g_platform_context.last_error, ET_ERROR_NOT_SUPPORTED, 0,
                             "Interface not available", __FILE__, __LINE__, __func__);
        return ET_ERROR_NOT_SUPPORTED;
    }

    /* 이미 생성된 인터페이스가 있으면 반환 */
    if (g_platform_context.interfaces[type]) {
        *interface = g_platform_context.interfaces[type];
        return ET_SUCCESS;
    }

    /* 새 인터페이스 생성 */
    ETResult result = registry->factory(interface, &registry->metadata);
    if (result == ET_SUCCESS) {
        g_platform_context.interfaces[type] = *interface;
    } else {
        et_set_detailed_error(&g_platform_context.last_error, result, 0,
                             "Failed to create interface", __FILE__, __LINE__, __func__);
    }

    return result;
}

/* 인터페이스 소멸 */
void et_destroy_interface(ETInterfaceType type, void* interface) {
    if (type >= ET_INTERFACE_COUNT || !interface) {
        return;
    }

    ETInterfaceRegistry* registry = &g_platform_context.registry[type];

    if (registry->destructor) {
        registry->destructor(interface);
    }

    if (g_platform_context.interfaces[type] == interface) {
        g_platform_context.interfaces[type] = NULL;
    }
}

/* 인터페이스 조회 */
void* et_get_interface(ETInterfaceType type) {
    if (type >= ET_INTERFACE_COUNT) {
        return NULL;
    }

    /* 인터페이스가 없으면 생성 시도 */
    if (!g_platform_context.interfaces[type]) {
        void* interface = NULL;
        if (et_create_interface(type, &interface) != ET_SUCCESS) {
            return NULL;
        }
    }

    return g_platform_context.interfaces[type];
}

/* 인터페이스 사용 가능 여부 확인 */
bool et_is_interface_available(ETInterfaceType type) {
    if (type >= ET_INTERFACE_COUNT) {
        return false;
    }

    if (!g_platform_context.initialized) {
        if (et_platform_initialize() != ET_SUCCESS) {
            return false;
        }
    }

    return g_platform_context.registry[type].is_available;
}

/* 인터페이스 메타데이터 조회 */
const ETInterfaceMetadata* et_get_interface_metadata(ETInterfaceType type) {
    if (type >= ET_INTERFACE_COUNT) {
        return NULL;
    }

    if (!g_platform_context.initialized) {
        if (et_platform_initialize() != ET_SUCCESS) {
            return NULL;
        }
    }

    return &g_platform_context.registry[type].metadata;
}

/* 버전 호환성 검사 */
bool et_is_interface_compatible(const ETInterfaceVersion* required, const ETInterfaceVersion* provided) {
    if (!required || !provided) {
        return false;
    }

    /* 주 버전이 다르면 호환되지 않음 */
    if (required->major != provided->major) {
        return false;
    }

    /* 제공된 부 버전이 요구된 것보다 낮으면 호환되지 않음 */
    if (provided->minor < required->minor) {
        return false;
    }

    /* 부 버전이 같으면 패치 버전도 확인 */
    if (provided->minor == required->minor && provided->patch < required->patch) {
        return false;
    }

    return true;
}

/* 인터페이스 타입을 문자열로 변환 */
const char* et_interface_type_to_string(ETInterfaceType type) {
    if (type >= ET_INTERFACE_COUNT) {
        return "Unknown";
    }

    return interface_type_names[type];
}

/* 디버그 및 진단 함수들 */
#ifdef LIBETUDE_DEBUG
#include <stdio.h>

void et_dump_platform_info(void) {
    if (!g_platform_context.initialized) {
        printf("Platform not initialized\n");
        return;
    }

    ETPlatformInfo* info = &g_platform_context.platform_info;

    printf("=== Platform Information ===\n");
    printf("Platform: %s (%d)\n", info->name, info->type);
    printf("Version: %s\n", info->version);
    printf("Architecture: %d\n", info->arch);
    printf("CPU Count: %u\n", info->cpu_count);
    printf("Total Memory: %llu bytes\n", (unsigned long long)info->total_memory);
    printf("Hardware Features: 0x%08X\n", info->features);

    /* 지원 기능 상세 출력 */
    printf("Supported Features:\n");
    if (info->features & ET_FEATURE_SSE) printf("  - SSE\n");
    if (info->features & ET_FEATURE_SSE2) printf("  - SSE2\n");
    if (info->features & ET_FEATURE_SSE3) printf("  - SSE3\n");
    if (info->features & ET_FEATURE_SSSE3) printf("  - SSSE3\n");
    if (info->features & ET_FEATURE_SSE4_1) printf("  - SSE4.1\n");
    if (info->features & ET_FEATURE_SSE4_2) printf("  - SSE4.2\n");
    if (info->features & ET_FEATURE_AVX) printf("  - AVX\n");
    if (info->features & ET_FEATURE_AVX2) printf("  - AVX2\n");
    if (info->features & ET_FEATURE_AVX512) printf("  - AVX512\n");
    if (info->features & ET_FEATURE_NEON) printf("  - NEON\n");
    if (info->features & ET_FEATURE_FMA) printf("  - FMA\n");
    printf("\n");
}

void et_dump_interface_registry(void) {
    if (!g_platform_context.initialized) {
        printf("Platform not initialized\n");
        return;
    }

    printf("=== Interface Registry ===\n");
    for (int i = 0; i < ET_INTERFACE_COUNT; i++) {
        ETInterfaceRegistry* registry = &g_platform_context.registry[i];

        printf("Interface: %s\n", et_interface_type_to_string((ETInterfaceType)i));
        printf("  Available: %s\n", registry->is_available ? "Yes" : "No");
        printf("  Platform: %d\n", registry->platform);
        printf("  Version: %u.%u.%u.%u\n",
               registry->metadata.version.major,
               registry->metadata.version.minor,
               registry->metadata.version.patch,
               registry->metadata.version.build);
        printf("  Size: %u bytes\n", registry->metadata.size);
        printf("  Created: %s\n", g_platform_context.interfaces[i] ? "Yes" : "No");
        printf("\n");
    }
}
#endif