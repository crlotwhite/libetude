/**
 * @file macos_compat.h
 * @brief macOS SDK 호환성 헤더
 * @author LibEtude Project
 * @version 1.0.0
 *
 * macOS 15 SDK와의 호환성을 보장하기 위한 매크로 및 정의들
 */

#ifndef LIBETUDE_MACOS_COMPAT_H
#define LIBETUDE_MACOS_COMPAT_H

#ifdef __APPLE__

#include <TargetConditionals.h>
#include <AvailabilityMacros.h>

// macOS 버전 감지
#if TARGET_OS_OSX
    #include <CoreFoundation/CoreFoundation.h>

    // macOS 15+ SDK 감지
    #if defined(MAC_OS_VERSION_15_0) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_15_0
        #define LIBETUDE_MACOS_15_PLUS 1
    #else
        #define LIBETUDE_MACOS_15_PLUS 0
    #endif
#endif

// Objective-C 블록 문법 비활성화
#if LIBETUDE_MACOS_15_PLUS || defined(LIBETUDE_MACOS_BLOCKS_DISABLED)

    // 블록 관련 매크로 비활성화
    #ifndef __BLOCKS__
        #define __BLOCKS__ 0
    #endif

    // 블록 키워드 무효화
    #ifndef __block
        #define __block
    #endif

    // Nullability 어노테이션 무효화 (C 컴파일러 호환성)
    #ifndef _Nullable
        #define _Nullable
    #endif

    #ifndef _Nonnull
        #define _Nonnull
    #endif

    #ifndef _Null_unspecified
        #define _Null_unspecified
    #endif

    // 추가 Objective-C 어노테이션 무효화
    #ifndef NS_ASSUME_NONNULL_BEGIN
        #define NS_ASSUME_NONNULL_BEGIN
    #endif

    #ifndef NS_ASSUME_NONNULL_END
        #define NS_ASSUME_NONNULL_END
    #endif

    // 블록 타입 정의 대체
    #define LIBETUDE_BLOCK_UNAVAILABLE 1

#endif // LIBETUDE_MACOS_15_PLUS

// CoreAudio 호환성 매크로
#ifdef LIBETUDE_MACOS_15_PLUS

    // 블록 기반 API 대신 콜백 기반 API 사용 강제
    #define LIBETUDE_USE_CALLBACK_API 1

    // 경고 억제 매크로
    #define LIBETUDE_SUPPRESS_BLOCK_WARNINGS \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Wunused-parameter\"") \
        _Pragma("clang diagnostic ignored \"-Wnullability-completeness\"")

    #define LIBETUDE_RESTORE_WARNINGS \
        _Pragma("clang diagnostic pop")

#else
    #define LIBETUDE_USE_CALLBACK_API 0
    #define LIBETUDE_SUPPRESS_BLOCK_WARNINGS
    #define LIBETUDE_RESTORE_WARNINGS
#endif

// 호환성 함수 선언
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief macOS SDK 버전 확인
 * @return SDK 주 버전 번호
 */
int libetude_get_macos_sdk_version(void);

/**
 * @brief 블록 문법 지원 여부 확인
 * @return 블록 문법이 활성화되어 있으면 1, 아니면 0
 */
int libetude_is_blocks_enabled(void);

/**
 * @brief 호환성 모드 초기화
 * @return 성공하면 0, 실패하면 -1
 */
int libetude_init_macos_compatibility(void);

#ifdef __cplusplus
}
#endif

#endif // __APPLE__

#endif // LIBETUDE_MACOS_COMPAT_H