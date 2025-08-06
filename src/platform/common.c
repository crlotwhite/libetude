/**
 * @file common.c
 * @brief 플랫폼 추상화 공통 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <string.h>
#include <time.h>

// ============================================================================
// 플랫폼 감지 및 정보 조회
// ============================================================================

/**
 * @brief 현재 플랫폼 정보를 가져옵니다
 */
ETResult et_platform_get_info(ETPlatformInfo* info) {
    if (!info) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "info 포인터가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(ETPlatformInfo));

    // 플랫폼 타입 감지
#ifdef _WIN32
    info->type = ET_PLATFORM_WINDOWS;
    strncpy(info->name, "Windows", sizeof(info->name) - 1);

    // Windows 버전 정보 (간단한 구현)
    strncpy(info->version, "10.0", sizeof(info->version) - 1);

#elif defined(__linux__) && !defined(__ANDROID__)
    info->type = ET_PLATFORM_LINUX;
    strncpy(info->name, "Linux", sizeof(info->name) - 1);
    strncpy(info->version, "Generic", sizeof(info->version) - 1);

#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        info->type = ET_PLATFORM_IOS;
        strncpy(info->name, "iOS", sizeof(info->name) - 1);
    #else
        info->type = ET_PLATFORM_MACOS;
        strncpy(info->name, "macOS", sizeof(info->name) - 1);
    #endif
    strncpy(info->version, "Generic", sizeof(info->version) - 1);

#elif defined(__ANDROID__)
    info->type = ET_PLATFORM_ANDROID;
    strncpy(info->name, "Android", sizeof(info->name) - 1);
    strncpy(info->version, "Generic", sizeof(info->version) - 1);

#else
    info->type = ET_PLATFORM_UNKNOWN;
    strncpy(info->name, "Unknown", sizeof(info->name) - 1);
    strncpy(info->version, "Unknown", sizeof(info->version) - 1);
#endif

    // 아키텍처 감지
#if defined(_M_X64) || defined(__x86_64__)
    info->arch = ET_ARCH_X64;
#elif defined(_M_IX86) || defined(__i386__)
    info->arch = ET_ARCH_X86;
#elif defined(_M_ARM64) || defined(__aarch64__)
    info->arch = ET_ARCH_ARM64;
#elif defined(_M_ARM) || defined(__arm__)
    info->arch = ET_ARCH_ARM;
#else
    info->arch = ET_ARCH_UNKNOWN;
#endif

    // 기본 기능 설정
    info->features = ET_HW_FEATURE_NONE;

    // SIMD 기능 감지 (간단한 구현)
#if defined(__SSE__) || defined(__AVX__) || defined(__ARM_NEON)
    info->features |= ET_HW_FEATURE_SIMD;
#endif

    // 고해상도 타이머 지원
    info->features |= ET_HW_FEATURE_HIGH_RES_TIMER;

    return ET_SUCCESS;
}

// ============================================================================
// 오류 매핑 시스템
// ============================================================================

/**
 * @brief Windows 오류 매핑 테이블
 */
#ifdef _WIN32
static const ETErrorMapping windows_error_mappings[] = {
    { 0, ET_SUCCESS, "성공" },
    { 2, ET_ERROR_NOT_FOUND, "파일을 찾을 수 없습니다" },
    { 5, ET_ERROR_INVALID_ARGUMENT, "액세스가 거부되었습니다" },
    { 8, ET_ERROR_OUT_OF_MEMORY, "메모리가 부족합니다" },
    { 87, ET_ERROR_INVALID_ARGUMENT, "매개 변수가 잘못되었습니다" },
    { -1, ET_ERROR_UNKNOWN, "알 수 없는 오류" }
};
#endif

/**
 * @brief Linux 오류 매핑 테이블
 */
#ifdef __linux__
static const ETErrorMapping linux_error_mappings[] = {
    { 0, ET_SUCCESS, "성공" },
    { 2, ET_ERROR_NOT_FOUND, "파일이나 디렉터리가 없습니다" },
    { 12, ET_ERROR_OUT_OF_MEMORY, "메모리를 할당할 수 없습니다" },
    { 13, ET_ERROR_INVALID_ARGUMENT, "권한이 거부되었습니다" },
    { 22, ET_ERROR_INVALID_ARGUMENT, "잘못된 인수입니다" },
    { -1, ET_ERROR_UNKNOWN, "알 수 없는 오류" }
};
#endif

/**
 * @brief macOS 오류 매핑 테이블
 */
#ifdef __APPLE__
static const ETErrorMapping macos_error_mappings[] = {
    { 0, ET_SUCCESS, "성공" },
    { 2, ET_ERROR_NOT_FOUND, "파일이나 디렉터리가 없습니다" },
    { 12, ET_ERROR_OUT_OF_MEMORY, "메모리를 할당할 수 없습니다" },
    { 13, ET_ERROR_INVALID_ARGUMENT, "권한이 거부되었습니다" },
    { 22, ET_ERROR_INVALID_ARGUMENT, "잘못된 인수입니다" },
    { -1, ET_ERROR_UNKNOWN, "알 수 없는 오류" }
};
#endif

/**
 * @brief 플랫폼별 오류를 공통 오류로 변환합니다
 */
ETResult et_platform_error_to_common(ETPlatformType platform, int platform_error) {
    const ETErrorMapping* mappings = NULL;
    int mapping_count = 0;

    // 플랫폼별 매핑 테이블 선택
    switch (platform) {
#ifdef _WIN32
        case ET_PLATFORM_WINDOWS:
            mappings = windows_error_mappings;
            mapping_count = sizeof(windows_error_mappings) / sizeof(ETErrorMapping);
            break;
#endif
#ifdef __linux__
        case ET_PLATFORM_LINUX:
            mappings = linux_error_mappings;
            mapping_count = sizeof(linux_error_mappings) / sizeof(ETErrorMapping);
            break;
#endif
#ifdef __APPLE__
        case ET_PLATFORM_MACOS:
        case ET_PLATFORM_IOS:
            mappings = macos_error_mappings;
            mapping_count = sizeof(macos_error_mappings) / sizeof(ETErrorMapping);
            break;
#endif
        default:
            return ET_ERROR_UNKNOWN;
    }

    // 매핑 테이블에서 검색
    if (mappings) {
        for (int i = 0; i < mapping_count - 1; i++) {
            if (mappings[i].platform_error == platform_error) {
                return mappings[i].common_error;
            }
        }
    }

    return ET_ERROR_UNKNOWN;
}

/**
 * @brief 플랫폼별 오류 설명을 가져옵니다
 */
const char* et_get_platform_error_description(ETPlatformType platform, int platform_error) {
    const ETErrorMapping* mappings = NULL;
    int mapping_count = 0;

    // 플랫폼별 매핑 테이블 선택
    switch (platform) {
#ifdef _WIN32
        case ET_PLATFORM_WINDOWS:
            mappings = windows_error_mappings;
            mapping_count = sizeof(windows_error_mappings) / sizeof(ETErrorMapping);
            break;
#endif
#ifdef __linux__
        case ET_PLATFORM_LINUX:
            mappings = linux_error_mappings;
            mapping_count = sizeof(linux_error_mappings) / sizeof(ETErrorMapping);
            break;
#endif
#ifdef __APPLE__
        case ET_PLATFORM_MACOS:
        case ET_PLATFORM_IOS:
            mappings = macos_error_mappings;
            mapping_count = sizeof(macos_error_mappings) / sizeof(ETErrorMapping);
            break;
#endif
        default:
            return "지원되지 않는 플랫폼";
    }

    // 매핑 테이블에서 검색
    if (mappings) {
        for (int i = 0; i < mapping_count - 1; i++) {
            if (mappings[i].platform_error == platform_error) {
                return mappings[i].description;
            }
        }
        // 마지막 항목은 기본값
        return mappings[mapping_count - 1].description;
    }

    return "알 수 없는 오류";
}

// ============================================================================
// 하드웨어 기능 감지
// ============================================================================

/**
 * @brief 하드웨어 기능 지원 여부를 확인합니다
 */
bool et_platform_has_feature(ETHardwareFeature feature) {
    ETPlatformInfo info;
    if (et_platform_get_info(&info) != ET_SUCCESS) {
        return false;
    }

    return (info.features & feature) != 0;
}