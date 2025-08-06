/**
 * @file platform.c
 * @brief 플랫폼 추상화 레이어 고수준 초기화 및 유틸리티 함수
 * @author LibEtude Team
 */

#include "libetude/platform.h"
#include <stdio.h>
#include <string.h>

/* 고수준 초기화 함수 */
ETResult et_platform_setup(void) {
    /* 플랫폼 초기화 */
    ETResult result = et_platform_initialize();
    if (result != ET_SUCCESS) {
        return result;
    }

    /* 플랫폼 정보 검증 */
    ETPlatformInfo info;
    result = et_get_platform_info(&info);
    if (result != ET_SUCCESS) {
        et_platform_finalize();
        return result;
    }

    /* 최소 요구사항 확인 */
    if (info.cpu_count == 0) {
        ET_SET_ERROR(ET_ERROR_PLATFORM_SPECIFIC, "Invalid CPU count detected");
        et_platform_finalize();
        return ET_ERROR_PLATFORM_SPECIFIC;
    }

    if (info.total_memory == 0) {
        ET_SET_ERROR(ET_ERROR_PLATFORM_SPECIFIC, "Invalid memory size detected");
        et_platform_finalize();
        return ET_ERROR_PLATFORM_SPECIFIC;
    }

    return ET_SUCCESS;
}

/* 고수준 정리 함수 */
void et_platform_shutdown(void) {
    et_platform_finalize();
}

/* 플랫폼 정보 출력 (디버그용) */
void et_print_platform_info(void) {
    ETPlatformInfo info;
    ETResult result = et_get_platform_info(&info);

    if (result != ET_SUCCESS) {
        printf("Failed to get platform information: %s\n", et_result_to_string(result));
        return;
    }

    printf("=== LibEtude Platform Information ===\n");
    printf("Platform: %s\n", info.name);
    printf("Version: %s\n", info.version);

    /* 아키텍처 정보 */
    const char* arch_name = "Unknown";
    switch (info.arch) {
        case ET_ARCH_X86: arch_name = "x86 (32-bit)"; break;
        case ET_ARCH_X64: arch_name = "x64 (64-bit)"; break;
        case ET_ARCH_ARM: arch_name = "ARM (32-bit)"; break;
        case ET_ARCH_ARM64: arch_name = "ARM64 (64-bit)"; break;
        default: break;
    }
    printf("Architecture: %s\n", arch_name);

    /* 시스템 리소스 */
    printf("CPU Cores: %u\n", info.cpu_count);
    printf("Total Memory: %.2f GB\n", (double)info.total_memory / (1024.0 * 1024.0 * 1024.0));

    /* 하드웨어 기능 */
    printf("Hardware Features:\n");
    if (info.features == ET_FEATURE_NONE) {
        printf("  - None detected\n");
    } else {
        if (info.features & ET_FEATURE_SSE) printf("  - SSE\n");
        if (info.features & ET_FEATURE_SSE2) printf("  - SSE2\n");
        if (info.features & ET_FEATURE_SSE3) printf("  - SSE3\n");
        if (info.features & ET_FEATURE_SSSE3) printf("  - SSSE3\n");
        if (info.features & ET_FEATURE_SSE4_1) printf("  - SSE4.1\n");
        if (info.features & ET_FEATURE_SSE4_2) printf("  - SSE4.2\n");
        if (info.features & ET_FEATURE_AVX) printf("  - AVX\n");
        if (info.features & ET_FEATURE_AVX2) printf("  - AVX2\n");
        if (info.features & ET_FEATURE_AVX512) printf("  - AVX512\n");
        if (info.features & ET_FEATURE_NEON) printf("  - NEON\n");
        if (info.features & ET_FEATURE_FMA) printf("  - FMA\n");
    }

    /* 인터페이스 가용성 */
    printf("Available Interfaces:\n");
    for (int i = 0; i < ET_INTERFACE_COUNT; i++) {
        ETInterfaceType type = (ETInterfaceType)i;
        bool available = et_is_interface_available(type);
        printf("  - %s: %s\n", et_interface_type_to_string(type),
               available ? "Available" : "Not Available");
    }

    printf("=====================================\n");
}