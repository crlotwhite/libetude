/**
 * @file macos_compat.c
 * @brief macOS SDK 호환성 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/macos_compat.h"

#ifdef __APPLE__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>

// 호환성 모드 상태
static int g_compatibility_initialized = 0;
static int g_macos_version_major = 0;
static int g_blocks_disabled = 0;

/**
 * @brief 시스템 버전 정보 조회
 */
static int get_system_version_info(void) {
    size_t size = 0;

    // 커널 버전 문자열 크기 조회
    if (sysctlbyname("kern.version", NULL, &size, NULL, 0) != 0) {
        return -1;
    }

    char* version_str = malloc(size);
    if (!version_str) {
        return -1;
    }

    // 커널 버전 문자열 조회
    if (sysctlbyname("kern.version", version_str, &size, NULL, 0) != 0) {
        free(version_str);
        return -1;
    }

    // Darwin 버전에서 macOS 버전 추정
    // Darwin 21.x.x = macOS 12.x (Monterey)
    // Darwin 22.x.x = macOS 13.x (Ventura)
    // Darwin 23.x.x = macOS 14.x (Sonoma)
    // Darwin 24.x.x = macOS 15.x (Sequoia)

    int darwin_major = 0;
    if (sscanf(version_str, "Darwin Kernel Version %d", &darwin_major) == 1) {
        if (darwin_major >= 24) {
            g_macos_version_major = 15;
        } else if (darwin_major >= 23) {
            g_macos_version_major = 14;
        } else if (darwin_major >= 22) {
            g_macos_version_major = 13;
        } else if (darwin_major >= 21) {
            g_macos_version_major = 12;
        } else {
            g_macos_version_major = 11; // 이전 버전
        }
    }

    free(version_str);
    return 0;
}

int libetude_get_macos_sdk_version(void) {
    if (!g_compatibility_initialized) {
        libetude_init_macos_compatibility();
    }

    return g_macos_version_major;
}

int libetude_is_blocks_enabled(void) {
    if (!g_compatibility_initialized) {
        libetude_init_macos_compatibility();
    }

    return !g_blocks_disabled;
}

int libetude_init_macos_compatibility(void) {
    if (g_compatibility_initialized) {
        return 0; // 이미 초기화됨
    }

    // 시스템 버전 정보 조회
    if (get_system_version_info() != 0) {
        // 실패 시 기본값 사용
        g_macos_version_major = 12;
    }

    // 블록 문법 비활성화 여부 확인
    #if defined(LIBETUDE_MACOS_BLOCKS_DISABLED) || defined(__BLOCKS__) && __BLOCKS__ == 0
        g_blocks_disabled = 1;
    #else
        g_blocks_disabled = 0;
    #endif

    // macOS 15+ 에서는 강제로 블록 비활성화
    if (g_macos_version_major >= 15) {
        g_blocks_disabled = 1;
    }

    g_compatibility_initialized = 1;

    // 디버그 정보 출력
    #ifdef LIBETUDE_DEBUG
        printf("[LibEtude] macOS 호환성 초기화 완료\n");
        printf("  - macOS 버전: %d.x\n", g_macos_version_major);
        printf("  - 블록 문법 비활성화: %s\n", g_blocks_disabled ? "예" : "아니오");
    #endif

    return 0;
}

// 호환성 검증 함수들

/**
 * @brief CoreAudio API 호환성 검증
 */
int libetude_verify_coreaudio_compatibility(void) {
    // 블록 기반 API 사용 가능 여부 확인
    if (g_blocks_disabled) {
        // 콜백 기반 API만 사용
        return 1; // 호환 가능
    }

    return 1; // 기본적으로 호환 가능
}

/**
 * @brief 컴파일 타임 호환성 검증
 */
void libetude_compile_time_compatibility_check(void) {
    #if LIBETUDE_MACOS_15_PLUS
        // macOS 15+ SDK에서 컴파일 시 경고 출력
        #warning "macOS 15+ SDK에서 컴파일됨 - 블록 문법 호환성 모드 활성화"
    #endif

    #if defined(__BLOCKS__) && __BLOCKS__ != 0
        #warning "블록 문법이 활성화되어 있습니다 - 호환성 문제가 발생할 수 있습니다"
    #endif
}

#endif // __APPLE__