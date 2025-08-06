/**
 * @file platform.h
 * @brief 플랫폼 추상화 레이어 통합 헤더
 * @author LibEtude Team
 */

#ifndef LIBETUDE_PLATFORM_H
#define LIBETUDE_PLATFORM_H

/* 플랫폼 추상화 핵심 헤더들 */
#include "platform/common.h"
#include "platform/factory.h"

/* 편의 매크로들 */
#define ET_PLATFORM_INIT() et_platform_initialize()
#define ET_PLATFORM_CLEANUP() et_platform_finalize()

#define ET_GET_INTERFACE(type) et_get_interface(ET_INTERFACE_##type)
#define ET_IS_AVAILABLE(type) et_is_interface_available(ET_INTERFACE_##type)

/* 오류 처리 매크로 */
#define ET_SET_ERROR(code, msg) \
    et_set_detailed_error(NULL, code, 0, msg, __FILE__, __LINE__, __func__)

#define ET_SET_PLATFORM_ERROR(code, platform_code, msg) \
    et_set_detailed_error(NULL, code, platform_code, msg, __FILE__, __LINE__, __func__)

#define ET_CHECK_RESULT(expr) \
    do { \
        ETResult _result = (expr); \
        if (_result != ET_SUCCESS) { \
            ET_SET_ERROR(_result, #expr " failed"); \
            return _result; \
        } \
    } while(0)

/* 플랫폼 감지 매크로 */
#define ET_IS_WINDOWS() (et_get_current_platform() == ET_PLATFORM_WINDOWS)
#define ET_IS_LINUX() (et_get_current_platform() == ET_PLATFORM_LINUX)
#define ET_IS_MACOS() (et_get_current_platform() == ET_PLATFORM_MACOS)
#define ET_IS_IOS() (et_get_current_platform() == ET_PLATFORM_IOS)
#define ET_IS_ANDROID() (et_get_current_platform() == ET_PLATFORM_ANDROID)

/* 아키텍처 감지 매크로 */
#define ET_IS_X86() (et_get_current_architecture() == ET_ARCH_X86)
#define ET_IS_X64() (et_get_current_architecture() == ET_ARCH_X64)
#define ET_IS_ARM() (et_get_current_architecture() == ET_ARCH_ARM)
#define ET_IS_ARM64() (et_get_current_architecture() == ET_ARCH_ARM64)

/* 하드웨어 기능 확인 매크로 */
#define ET_HAS_SSE() et_has_hardware_feature(ET_FEATURE_SSE)
#define ET_HAS_SSE2() et_has_hardware_feature(ET_FEATURE_SSE2)
#define ET_HAS_AVX() et_has_hardware_feature(ET_FEATURE_AVX)
#define ET_HAS_AVX2() et_has_hardware_feature(ET_FEATURE_AVX2)
#define ET_HAS_NEON() et_has_hardware_feature(ET_FEATURE_NEON)
#define ET_HAS_FMA() et_has_hardware_feature(ET_FEATURE_FMA)

#ifdef __cplusplus
extern "C" {
#endif

/* 고수준 초기화 함수 */
ETResult et_platform_setup(void);
void et_platform_shutdown(void);

/* 플랫폼 정보 출력 (디버그용) */
void et_print_platform_info(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBETUDE_PLATFORM_H */