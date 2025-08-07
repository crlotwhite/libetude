/**
 * @file factory_linux.c
 * @brief Linux 플랫폼 팩토리 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#ifdef __linux__

#include "libetude/platform/factory.h"
#include "libetude/platform/threading.h"
#include "libetude/platform/network.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"

// ============================================================================
// 외부 함수 선언
// ============================================================================

// 오디오 인터페이스
extern ETResult et_create_linux_audio_interface(ETAudioInterface** interface);
extern void et_destroy_linux_audio_interface(ETAudioInterface* interface);

// 스레딩 인터페이스
extern ETResult et_create_posix_thread_interface(ETThreadInterface** interface);
extern void et_destroy_posix_thread_interface(ETThreadInterface* interface);

// 메모리 관리 인터페이스
extern ETResult et_create_posix_memory_interface(ETMemoryInterface** interface);
extern void et_destroy_posix_memory_interface(ETMemoryInterface* interface);

// 파일시스템 인터페이스
extern ETResult et_create_posix_filesystem_interface(struct ETFilesystemInterface** interface);
extern void et_destroy_posix_filesystem_interface(struct ETFilesystemInterface* interface);

// 네트워크 인터페이스
extern const ETNetworkInterface* et_get_linux_network_interface(void);

// 동적 라이브러리 인터페이스
extern ETResult et_create_posix_dynlib_interface(struct ETDynlibInterface** interface);
extern void et_destroy_posix_dynlib_interface(struct ETDynlibInterface* interface);

// ============================================================================
// Linux 팩토리 함수들
// ============================================================================

/**
 * @brief Linux 오디오 인터페이스를 생성합니다
 */
static ETResult linux_create_audio_interface(ETAudioInterface** interface) {
    return et_create_linux_audio_interface(interface);
}

/**
 * @brief Linux 오디오 인터페이스를 해제합니다
 */
static void linux_destroy_audio_interface(ETAudioInterface* interface) {
    et_destroy_linux_audio_interface(interface);
}

/**
 * @brief Linux 스레딩 인터페이스를 생성합니다
 */
static ETResult linux_create_thread_interface(ETThreadInterface** interface) {
    return et_create_posix_thread_interface(interface);
}

/**
 * @brief Linux 스레딩 인터페이스를 해제합니다
 */
static void linux_destroy_thread_interface(ETThreadInterface* interface) {
    et_destroy_posix_thread_interface(interface);
}

/**
 * @brief Linux 메모리 관리 인터페이스를 생성합니다
 */
static ETResult linux_create_memory_interface(ETMemoryInterface** interface) {
    return et_create_posix_memory_interface(interface);
}

/**
 * @brief Linux 메모리 관리 인터페이스를 해제합니다
 */
static void linux_destroy_memory_interface(ETMemoryInterface* interface) {
    et_destroy_posix_memory_interface(interface);
}

/**
 * @brief Linux 파일시스템 인터페이스를 생성합니다
 */
static ETResult linux_create_filesystem_interface(struct ETFilesystemInterface** interface) {
    return et_create_posix_filesystem_interface(interface);
}

/**
 * @brief Linux 파일시스템 인터페이스를 해제합니다
 */
static void linux_destroy_filesystem_interface(struct ETFilesystemInterface* interface) {
    et_destroy_posix_filesystem_interface(interface);
}

/**
 * @brief Linux 네트워크 인터페이스를 생성합니다
 */
static ETResult linux_create_network_interface(void** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETNetworkInterface* net_interface = et_get_linux_network_interface();
    if (!net_interface) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    *interface = (void*)net_interface;
    return ET_SUCCESS;
}

/**
 * @brief Linux 네트워크 인터페이스를 해제합니다
 */
static void linux_destroy_network_interface(void* interface) {
    // Linux 네트워크 인터페이스는 정적 구조체이므로 해제할 필요 없음
    (void)interface;
}

/**
 * @brief Linux 동적 라이브러리 인터페이스를 생성합니다
 */
static ETResult linux_create_dynlib_interface(struct ETDynlibInterface** interface) {
    return et_create_posix_dynlib_interface(interface);
}

/**
 * @brief Linux 동적 라이브러리 인터페이스를 해제합니다
 */
static void linux_destroy_dynlib_interface(struct ETDynlibInterface* interface) {
    et_destroy_posix_dynlib_interface(interface);
}

/**
 * @brief Linux 플랫폼을 초기화합니다
 */
static ETResult linux_initialize(void) {
    // Linux 플랫폼 초기화 작업
    // 필요한 경우 여기에 추가
    return ET_SUCCESS;
}

/**
 * @brief Linux 플랫폼을 정리합니다
 */
static void linux_finalize(void) {
    // Linux 플랫폼 정리 작업
    // 필요한 경우 여기에 추가
}

// ============================================================================
// Linux 플랫폼 팩토리 구조체
// ============================================================================

static ETPlatformFactory g_linux_factory = {
    .platform_type = ET_PLATFORM_LINUX,
    .platform_name = "Linux",

    // 인터페이스 생성 함수들
    .create_audio_interface = linux_create_audio_interface,
    .destroy_audio_interface = linux_destroy_audio_interface,
    .create_thread_interface = linux_create_thread_interface,
    .destroy_thread_interface = linux_destroy_thread_interface,
    .create_memory_interface = linux_create_memory_interface,
    .destroy_memory_interface = linux_destroy_memory_interface,
    .create_filesystem_interface = linux_create_filesystem_interface,
    .destroy_filesystem_interface = linux_destroy_filesystem_interface,
    .create_network_interface = linux_create_network_interface,
    .destroy_network_interface = linux_destroy_network_interface,
    .create_dynlib_interface = linux_create_dynlib_interface,
    .destroy_dynlib_interface = linux_destroy_dynlib_interface,

    // 플랫폼 초기화/정리
    .initialize = linux_initialize,
    .finalize = linux_finalize,

    // 플랫폼별 확장 데이터
    .platform_data = NULL
};

// ============================================================================
// 공개 함수들
// ============================================================================

/**
 * @brief Linux 플랫폼 팩토리를 가져옵니다
 * @return Linux 팩토리 포인터
 */
const ETPlatformFactory* et_platform_factory_linux(void) {
    return &g_linux_factory;
}

#endif // __linux__