/**
 * @file platform_core.c
 * @brief 플랫폼 추상화 레이어 핵심 구현
 * @author LibEtude Team
 */

#include "libetude/platform/common.h"
#include "libetude/platform/factory.h"
#include "libetude/hardware.h"
#include <string.h>
#include <stdio.h>

// 플랫폼 매크로 정의
#ifdef _WIN32
    #define LIBETUDE_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define LIBETUDE_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define LIBETUDE_PLATFORM_MACOS
#endif

// ============================================================================
// 전역 변수
// ============================================================================

static bool g_platform_initialized = false;
static ETPlatformInfo g_platform_info;

// ============================================================================
// 플랫폼 감지 함수들
// ============================================================================

static ETPlatformType detect_platform_type(void) {
#ifdef _WIN32
    return ET_PLATFORM_WINDOWS;
#elif defined(__APPLE__)
    #ifdef TARGET_OS_IOS
        return ET_PLATFORM_IOS;
    #else
        return ET_PLATFORM_MACOS;
    #endif
#elif defined(__ANDROID__)
    return ET_PLATFORM_ANDROID;
#elif defined(__linux__)
    return ET_PLATFORM_LINUX;
#else
    return ET_PLATFORM_UNKNOWN;
#endif
}

static ETArchitecture detect_architecture(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return ET_ARCH_X64;
#elif defined(__i386) || defined(_M_IX86)
    return ET_ARCH_X86;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return ET_ARCH_ARM64;
#elif defined(__arm__) || defined(_M_ARM)
    return ET_ARCH_ARM;
#else
    return ET_ARCH_UNKNOWN;
#endif
}

static uint32_t detect_hardware_features(void) {
    uint32_t features = ET_FEATURE_NONE;
    
    // 기본적으로 하드웨어 감지 기능 사용
    LibEtudeHardwareInfo hw_info;
    if (libetude_hardware_detect(&hw_info) == LIBETUDE_SUCCESS) {
        // SIMD 기능을 플랫폼 기능으로 변환
        uint32_t simd_features = hw_info.cpu.simd_features;
        
        if (simd_features & LIBETUDE_SIMD_SSE) features |= ET_FEATURE_SSE;
        if (simd_features & LIBETUDE_SIMD_SSE2) features |= ET_FEATURE_SSE2;
        if (simd_features & LIBETUDE_SIMD_SSE3) features |= ET_FEATURE_SSE3;
        if (simd_features & LIBETUDE_SIMD_SSSE3) features |= ET_FEATURE_SSSE3;
        if (simd_features & LIBETUDE_SIMD_SSE4_1) features |= ET_FEATURE_SSE4_1;
        if (simd_features & LIBETUDE_SIMD_SSE4_2) features |= ET_FEATURE_SSE4_2;
        if (simd_features & LIBETUDE_SIMD_AVX) features |= ET_FEATURE_AVX;
        if (simd_features & LIBETUDE_SIMD_AVX2) features |= ET_FEATURE_AVX2;
        if (simd_features & LIBETUDE_SIMD_AVX512F) features |= ET_FEATURE_AVX512;
        if (simd_features & LIBETUDE_SIMD_NEON) features |= ET_FEATURE_NEON;
        if (simd_features & LIBETUDE_SIMD_FMA) features |= ET_FEATURE_FMA;
        
        // GPU 지원 여부
        if (hw_info.gpu.vendor_id != 0) {
            features |= ET_FEATURE_GPU;
        }
        
        // 고해상도 타이머는 기본적으로 지원한다고 가정
        features |= ET_FEATURE_HIGH_RES_TIMER;
    }
    
    return features;
}

static void get_platform_name(char* name, size_t size) {
    ETPlatformType type = detect_platform_type();
    
    switch (type) {
        case ET_PLATFORM_WINDOWS:
            strncpy(name, "Windows", size - 1);
            break;
        case ET_PLATFORM_LINUX:
            strncpy(name, "Linux", size - 1);
            break;
        case ET_PLATFORM_MACOS:
            strncpy(name, "macOS", size - 1);
            break;
        case ET_PLATFORM_ANDROID:
            strncpy(name, "Android", size - 1);
            break;
        case ET_PLATFORM_IOS:
            strncpy(name, "iOS", size - 1);
            break;
        default:
            strncpy(name, "Unknown", size - 1);
            break;
    }
    name[size - 1] = '\0';
}

static void get_platform_version(char* version, size_t size) {
    // 플랫폼별 버전 정보 수집
#ifdef _WIN32
    // Windows 버전 감지 로직 (simplified)
    strncpy(version, "10.0", size - 1);
#elif defined(__APPLE__)
    // macOS/iOS 버전 감지 로직
    strncpy(version, "Unknown", size - 1);
#elif defined(__linux__)
    // Linux 커널 버전 감지 로직
    strncpy(version, "Unknown", size - 1);
#else
    strncpy(version, "Unknown", size - 1);
#endif
    version[size - 1] = '\0';
}

static uint32_t get_cpu_count(void) {
    LibEtudeHardwareInfo hw_info;
    if (libetude_hardware_detect(&hw_info) == LIBETUDE_SUCCESS) {
        return hw_info.cpu.logical_cores;
    }
    return 1; // 기본값
}

static uint64_t get_total_memory(void) {
    LibEtudeHardwareInfo hw_info;
    if (libetude_hardware_detect(&hw_info) == LIBETUDE_SUCCESS) {
        return hw_info.memory.total_physical;
    }
    return 1024 * 1024 * 1024; // 기본값: 1GB
}

// ============================================================================
// 공개 함수 구현
// ============================================================================

ETResult et_platform_initialize(void) {
    if (g_platform_initialized) {
        return ET_SUCCESS; // 이미 초기화됨
    }
    
    // 플랫폼 정보 수집
    memset(&g_platform_info, 0, sizeof(g_platform_info));
    
    g_platform_info.type = detect_platform_type();
    g_platform_info.arch = detect_architecture();
    g_platform_info.features = detect_hardware_features();
    g_platform_info.cpu_count = get_cpu_count();
    g_platform_info.total_memory = get_total_memory();
    
    get_platform_name(g_platform_info.name, sizeof(g_platform_info.name));
    get_platform_version(g_platform_info.version, sizeof(g_platform_info.version));
    
    // 플랫폼별 인터페이스 등록
    ETResult result = ET_SUCCESS;
    
#ifdef LIBETUDE_PLATFORM_WINDOWS
    extern ETResult et_register_windows_interfaces(void);
    result = et_register_windows_interfaces();
#elif defined(LIBETUDE_PLATFORM_LINUX)
    extern ETResult et_register_linux_interfaces(void);
    result = et_register_linux_interfaces();
#elif defined(LIBETUDE_PLATFORM_MACOS)
    extern ETResult et_register_macos_interfaces(void);
    result = et_register_macos_interfaces();
#endif
    
    if (result == ET_SUCCESS) {
        g_platform_initialized = true;
    }
    
    return result;
}

ETResult et_get_platform_info(ETPlatformInfo* info) {
    if (!info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }
    
    if (!g_platform_initialized) {
        ETResult result = et_platform_initialize();
        if (result != ET_SUCCESS) {
            return result;
        }
    }
    
    *info = g_platform_info;
    return ET_SUCCESS;
}

void et_platform_finalize(void) {
    if (!g_platform_initialized) {
        return;
    }
    
    // 인터페이스 정리는 팩토리에서 자동으로 처리됨
    g_platform_initialized = false;
    memset(&g_platform_info, 0, sizeof(g_platform_info));
}

ETPlatformType et_get_current_platform(void) {
    if (!g_platform_initialized) {
        et_platform_initialize();
    }
    return g_platform_info.type;
}

ETArchitecture et_get_current_architecture(void) {
    if (!g_platform_initialized) {
        et_platform_initialize();
    }
    return g_platform_info.arch;
}

bool et_has_hardware_feature(ETHardwareFeature feature) {
    if (!g_platform_initialized) {
        et_platform_initialize();
    }
    return (g_platform_info.features & feature) != 0;
}

ETResult et_platform_error_to_common(ETPlatformType platform, int platform_error) {
    // 플랫폼별 오류 매핑 로직
    (void)platform; // 현재는 간단한 구현
    
    if (platform_error == 0) {
        return ET_SUCCESS;
    }
    
    return ET_ERROR_PLATFORM_SPECIFIC;
}

const char* et_get_platform_error_description(ETPlatformType platform, int platform_error) {
    (void)platform;
    (void)platform_error;
    return "Platform-specific error";
}

bool et_platform_has_feature(ETHardwareFeature feature) {
    return et_has_hardware_feature(feature);
}