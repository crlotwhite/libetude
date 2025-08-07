/**
 * @file factory_windows.c
 * @brief Windows 플랫폼 팩토리 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#ifdef _WIN32

#include "libetude/platform/factory.h"
#include "libetude/platform/threading.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"

// ============================================================================
// 외부 함수 선언
// ============================================================================

// 오디오 인터페이스
extern ETResult et_create_windows_audio_interface(ETAudioInterface** interface);
extern void et_destroy_windows_audio_interface(ETAudioInterface* interface);

// 스레딩 인터페이스
extern ETResult et_create_windows_thread_interface(ETThreadInterface** interface);
extern void et_destroy_windows_thread_interface(ETThreadInterface* interface);

// ============================================================================
// Windows 팩토리 함수들
// ============================================================================

/**
 * @brief Windows 오디오 인터페이스를 생성합니다
 */
static ETResult windows_create_audio_interface(ETAudioInterface** interface) {
    return et_create_windows_audio_interface(interface);
}

/**
 * @brief Windows 오디오 인터페이스를 해제합니다
 */
static void windows_destroy_audio_interface(ETAudioInterface* interface) {
    et_destroy_windows_audio_interface(interface);
}

/**
 * @brief Windows 스레딩 인터페이스를 생성합니다
 */
static ETResult windows_create_thread_interface(ETThreadInterface** interface) {
    return et_create_windows_thread_interface(interface);
}

/**
 * @brief Windows 스레딩 인터페이스를 해제합니다
 */
static void windows_destroy_thread_interface(ETThreadInterface* interface) {
    et_destroy_windows_thread_interface(interface);
}

/**
 * @brief Windows 플랫폼을 초기화합니다
 */
static ETResult windows_initialize(void) {
    // Windows 플랫폼 초기화 작업
    // 필요한 경우 여기에 추가
    return ET_SUCCESS;
}

/**
 * @brief Windows 플랫폼을 정리합니다
 */
static void windows_finalize(void) {
    // Windows 플랫폼 정리 작업
    // 필요한 경우 여기에 추가
}

// ============================================================================
// Windows 플랫폼 팩토리 구조체
// ============================================================================

static ETPlatformFactory g_windows_factory = {
    .platform_type = ET_PLATFORM_WINDOWS,
    .platform_name = "Windows",

    // 인터페이스 생성 함수들
    .create_audio_interface = windows_create_audio_interface,
    .destroy_audio_interface = windows_destroy_audio_interface,
    .create_thread_interface = windows_create_thread_interface,
    .destroy_thread_interface = windows_destroy_thread_interface,

    // 플랫폼 초기화/정리
    .initialize = windows_initialize,
    .finalize = windows_finalize,

    // 플랫폼별 확장 데이터
    .platform_data = NULL
};

// ============================================================================
// 공개 함수들
// ============================================================================

/**
 * @brief Windows 플랫폼 팩토리를 가져옵니다
 * @return Windows 팩토리 포인터
 */
const ETPlatformFactory* et_platform_factory_windows(void) {
    return &g_windows_factory;
}

#endif // _WIN32