/**
 * @file factory_macos.c
 * @brief macOS 플랫폼 팩토리 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#ifdef __APPLE__

#include "libetude/platform/factory.h"
#include "libetude/platform/threading.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"

// ============================================================================
// 외부 함수 선언
// ============================================================================

// 오디오 인터페이스
extern ETResult et_create_macos_audio_interface(ETAudioInterface** interface);
extern void et_destroy_macos_audio_interface(ETAudioInterface* interface);

// 스레딩 인터페이스
extern ETResult et_create_posix_thread_interface(ETThreadInterface** interface);
extern void et_destroy_posix_thread_interface(ETThreadInterface* interface);

// 메모리 관리 인터페이스
extern ETResult et_create_posix_memory_interface(ETMemoryInterface** interface);
extern void et_destroy_posix_memory_interface(ETMemoryInterface* interface);

// ============================================================================
// macOS 팩토리 함수들
// ============================================================================

/**
 * @brief macOS 오디오 인터페이스를 생성합니다
 */
static ETResult macos_create_audio_interface(ETAudioInterface** interface) {
    return et_create_macos_audio_interface(interface);
}

/**
 * @brief macOS 오디오 인터페이스를 해제합니다
 */
static void macos_destroy_audio_interface(ETAudioInterface* interface) {
    et_destroy_macos_audio_interface(interface);
}

/**
 * @brief macOS 스레딩 인터페이스를 생성합니다
 */
static ETResult macos_create_thread_interface(ETThreadInterface** interface) {
    return et_create_posix_thread_interface(interface);
}

/**
 * @brief macOS 스레딩 인터페이스를 해제합니다
 */
static void macos_destroy_thread_interface(ETThreadInterface* interface) {
    et_destroy_posix_thread_interface(interface);
}

/**
 * @brief macOS 메모리 관리 인터페이스를 생성합니다
 */
static ETResult macos_create_memory_interface(ETMemoryInterface** interface) {
    return et_create_posix_memory_interface(interface);
}

/**
 * @brief macOS 메모리 관리 인터페이스를 해제합니다
 */
static void macos_destroy_memory_interface(ETMemoryInterface* interface) {
    et_destroy_posix_memory_interface(interface);
}

/**
 * @brief macOS 플랫폼을 초기화합니다
 */
static ETResult macos_initialize(void) {
    // macOS 플랫폼 초기화 작업
    // 필요한 경우 여기에 추가
    return ET_SUCCESS;
}

/**
 * @brief macOS 플랫폼을 정리합니다
 */
static void macos_finalize(void) {
    // macOS 플랫폼 정리 작업
    // 필요한 경우 여기에 추가
}

// ============================================================================
// macOS 플랫폼 팩토리 구조체
// ============================================================================

static ETPlatformFactory g_macos_factory = {
    .platform_type = ET_PLATFORM_MACOS,
    .platform_name = "macOS",

    // 인터페이스 생성 함수들
    .create_audio_interface = macos_create_audio_interface,
    .destroy_audio_interface = macos_destroy_audio_interface,
    .create_thread_interface = macos_create_thread_interface,
    .destroy_thread_interface = macos_destroy_thread_interface,
    .create_memory_interface = macos_create_memory_interface,
    .destroy_memory_interface = macos_destroy_memory_interface,

    // 플랫폼 초기화/정리
    .initialize = macos_initialize,
    .finalize = macos_finalize,

    // 플랫폼별 확장 데이터
    .platform_data = NULL
};

// ============================================================================
// 공개 함수들
// ============================================================================

/**
 * @brief macOS 플랫폼 팩토리를 가져옵니다
 * @return macOS 팩토리 포인터
 */
const ETPlatformFactory* et_platform_factory_macos(void) {
    return &g_macos_factory;
}

#endif // __APPLE__