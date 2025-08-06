/**
 * @file factory.c
 * @brief 플랫폼 추상화 팩토리 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/factory.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// 전역 변수
// ============================================================================

/** 등록된 팩토리들 */
static const ETPlatformFactory* g_registered_factories[8] = { NULL };
static int g_factory_count = 0;
static bool g_factory_initialized = false;

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 플랫폼 자동 감지
 */
ETPlatformType et_platform_detect(void) {
    ETPlatformInfo info;
    if (et_platform_get_info(&info) == ET_SUCCESS) {
        return info.type;
    }
    return ET_PLATFORM_UNKNOWN;
}

/**
 * @brief 플랫폼 타입을 문자열로 변환
 */
const char* et_platform_type_to_string(ETPlatformType platform_type) {
    switch (platform_type) {
        case ET_PLATFORM_WINDOWS: return "Windows";
        case ET_PLATFORM_LINUX: return "Linux";
        case ET_PLATFORM_MACOS: return "macOS";
        case ET_PLATFORM_ANDROID: return "Android";
        case ET_PLATFORM_IOS: return "iOS";
        default: return "Unknown";
    }
}

/**
 * @brief 문자열을 플랫폼 타입으로 변환
 */
ETPlatformType et_platform_type_from_string(const char* platform_name) {
    if (!platform_name) return ET_PLATFORM_UNKNOWN;

    if (strcmp(platform_name, "Windows") == 0) return ET_PLATFORM_WINDOWS;
    if (strcmp(platform_name, "Linux") == 0) return ET_PLATFORM_LINUX;
    if (strcmp(platform_name, "macOS") == 0) return ET_PLATFORM_MACOS;
    if (strcmp(platform_name, "Android") == 0) return ET_PLATFORM_ANDROID;
    if (strcmp(platform_name, "iOS") == 0) return ET_PLATFORM_IOS;

    return ET_PLATFORM_UNKNOWN;
}

// ============================================================================
// 팩토리 등록 및 관리
// ============================================================================

/**
 * @brief 플랫폼 팩토리를 등록합니다
 */
ETResult et_platform_factory_register(const ETPlatformFactory* factory) {
    if (!factory) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "factory 포인터가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (g_factory_count >= 8) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "등록 가능한 팩토리 수를 초과했습니다");
        return ET_ERROR_BUFFER_FULL;
    }

    // 중복 등록 확인
    for (int i = 0; i < g_factory_count; i++) {
        if (g_registered_factories[i] &&
            g_registered_factories[i]->platform_type == factory->platform_type) {
            ET_SET_ERROR(ET_ERROR_ALREADY_INITIALIZED, "이미 등록된 플랫폼입니다: %s",
                        et_platform_type_to_string(factory->platform_type));
            return ET_ERROR_ALREADY_INITIALIZED;
        }
    }

    g_registered_factories[g_factory_count] = factory;
    g_factory_count++;

    ET_LOG_INFO("플랫폼 팩토리 등록됨: %s", factory->platform_name);
    return ET_SUCCESS;
}

/**
 * @brief 플랫폼 팩토리 등록을 해제합니다
 */
void et_platform_factory_unregister(ETPlatformType platform_type) {
    for (int i = 0; i < g_factory_count; i++) {
        if (g_registered_factories[i] &&
            g_registered_factories[i]->platform_type == platform_type) {

            // 배열에서 제거 (뒤의 요소들을 앞으로 이동)
            for (int j = i; j < g_factory_count - 1; j++) {
                g_registered_factories[j] = g_registered_factories[j + 1];
            }
            g_registered_factories[g_factory_count - 1] = NULL;
            g_factory_count--;

            ET_LOG_INFO("플랫폼 팩토리 등록 해제됨: %s",
                       et_platform_type_to_string(platform_type));
            break;
        }
    }
}

/**
 * @brief 플랫폼 팩토리 시스템을 초기화합니다
 */
ETResult et_platform_factory_init(void) {
    if (g_factory_initialized) {
        return ET_SUCCESS;
    }

    // 배열 초기화
    memset(g_registered_factories, 0, sizeof(g_registered_factories));
    g_factory_count = 0;

    // 플랫폼별 팩토리 등록
    ETResult result = ET_SUCCESS;

#ifdef _WIN32
    const ETPlatformFactory* windows_factory = et_platform_factory_windows();
    if (windows_factory) {
        result = et_platform_factory_register(windows_factory);
        if (result != ET_SUCCESS) {
            ET_LOG_ERROR("Windows 팩토리 등록 실패");
            return result;
        }
    }
#endif

#ifdef __linux__
    const ETPlatformFactory* linux_factory = et_platform_factory_linux();
    if (linux_factory) {
        result = et_platform_factory_register(linux_factory);
        if (result != ET_SUCCESS) {
            ET_LOG_ERROR("Linux 팩토리 등록 실패");
            return result;
        }
    }
#endif

#ifdef __APPLE__
    const ETPlatformFactory* macos_factory = et_platform_factory_macos();
    if (macos_factory) {
        result = et_platform_factory_register(macos_factory);
        if (result != ET_SUCCESS) {
            ET_LOG_ERROR("macOS 팩토리 등록 실패");
            return result;
        }
    }
#endif

#ifdef __ANDROID__
    const ETPlatformFactory* android_factory = et_platform_factory_android();
    if (android_factory) {
        result = et_platform_factory_register(android_factory);
        if (result != ET_SUCCESS) {
            ET_LOG_ERROR("Android 팩토리 등록 실패");
            return result;
        }
    }
#endif

    g_factory_initialized = true;
    ET_LOG_INFO("플랫폼 팩토리 시스템 초기화 완료 (%d개 팩토리 등록)", g_factory_count);

    return ET_SUCCESS;
}

/**
 * @brief 플랫폼 팩토리 시스템을 정리합니다
 */
void et_platform_factory_cleanup(void) {
    if (!g_factory_initialized) {
        return;
    }

    // 등록된 팩토리들 정리
    for (int i = 0; i < g_factory_count; i++) {
        if (g_registered_factories[i] && g_registered_factories[i]->finalize) {
            g_registered_factories[i]->finalize();
        }
    }

    memset(g_registered_factories, 0, sizeof(g_registered_factories));
    g_factory_count = 0;
    g_factory_initialized = false;

    ET_LOG_INFO("플랫폼 팩토리 시스템 정리 완료");
}

// ============================================================================
// 팩토리 조회 함수
// ============================================================================

/**
 * @brief 현재 플랫폼에 맞는 팩토리를 가져옵니다
 */
const ETPlatformFactory* et_platform_factory_get_current(void) {
    if (!g_factory_initialized) {
        if (et_platform_factory_init() != ET_SUCCESS) {
            return NULL;
        }
    }

    ETPlatformType current_platform = et_platform_detect();
    return et_platform_factory_get(current_platform);
}

/**
 * @brief 특정 플랫폼의 팩토리를 가져옵니다
 */
const ETPlatformFactory* et_platform_factory_get(ETPlatformType platform_type) {
    if (!g_factory_initialized) {
        if (et_platform_factory_init() != ET_SUCCESS) {
            return NULL;
        }
    }

    for (int i = 0; i < g_factory_count; i++) {
        if (g_registered_factories[i] &&
            g_registered_factories[i]->platform_type == platform_type) {
            return g_registered_factories[i];
        }
    }

    ET_SET_ERROR(ET_ERROR_NOT_FOUND, "플랫폼 팩토리를 찾을 수 없습니다: %s",
                et_platform_type_to_string(platform_type));
    return NULL;
}

/**
 * @brief 사용 가능한 플랫폼 목록을 가져옵니다
 */
ETResult et_platform_factory_list_available(ETPlatformType* platforms, int* count) {
    if (!platforms || !count) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "platforms 또는 count 포인터가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!g_factory_initialized) {
        if (et_platform_factory_init() != ET_SUCCESS) {
            return ET_ERROR_NOT_INITIALIZED;
        }
    }

    int available_count = 0;
    int max_count = *count;

    for (int i = 0; i < g_factory_count && available_count < max_count; i++) {
        if (g_registered_factories[i]) {
            platforms[available_count] = g_registered_factories[i]->platform_type;
            available_count++;
        }
    }

    *count = available_count;
    return ET_SUCCESS;
}

// ============================================================================
// 편의 함수들
// ============================================================================

/**
 * @brief 현재 플랫폼의 오디오 인터페이스를 생성합니다
 */
ETResult et_create_audio_interface(ETAudioInterface** interface) {
    if (!interface) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "interface 포인터가 NULL입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETPlatformFactory* factory = et_platform_factory_get_current();
    if (!factory) {
        ET_SET_ERROR(ET_ERROR_NOT_FOUND, "현재 플랫폼의 팩토리를 찾을 수 없습니다");
        return ET_ERROR_NOT_FOUND;
    }

    if (!factory->create_audio_interface) {
        ET_SET_ERROR(ET_ERROR_NOT_IMPLEMENTED, "오디오 인터페이스 생성이 구현되지 않았습니다");
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    return factory->create_audio_interface(interface);
}

/**
 * @brief 오디오 인터페이스를 해제합니다
 */
void et_destroy_audio_interface(ETAudioInterface* interface) {
    if (!interface) {
        return;
    }

    const ETPlatformFactory* factory = et_platform_factory_get_current();
    if (factory && factory->destroy_audio_interface) {
        factory->destroy_audio_interface(interface);
    }
}