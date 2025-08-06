/**
 * @file platform_init.c
 * @brief Windows 플랫폼 인터페이스 등록 및 초기화
 * @author LibEtude Team
 */

#ifdef LIBETUDE_PLATFORM_WINDOWS

#include "libetude/platform/factory.h"
#include "libetude/platform/common.h"

/* Windows 오디오 인터페이스 선언 */
extern ETAudioInterface* et_get_windows_audio_interface(void);
extern ETResult et_windows_audio_initialize(void);
extern void et_windows_audio_cleanup(void);

/* 오디오 인터페이스 팩토리 함수 */
static ETResult windows_audio_factory(void** interface, const ETInterfaceMetadata* metadata) {
    (void)metadata; /* 미사용 매개변수 경고 제거 */

    ETResult result = et_windows_audio_initialize();
    if (result != ET_SUCCESS) {
        return result;
    }

    *interface = et_get_windows_audio_interface();
    return (*interface != NULL) ? ET_SUCCESS : ET_ERROR_HARDWARE;
}

static void windows_audio_destructor(void* interface) {
    (void)interface; /* 미사용 매개변수 경고 제거 */
    et_windows_audio_cleanup();
}

static ETResult stub_system_factory(void** interface, const ETInterfaceMetadata* metadata) {
    (void)metadata;
    *interface = NULL;
    return ET_SUCCESS;
}

static void stub_system_destructor(void* interface) {
    (void)interface;
}

static ETResult stub_thread_factory(void** interface, const ETInterfaceMetadata* metadata) {
    (void)metadata;
    *interface = NULL;
    return ET_SUCCESS;
}

static void stub_thread_destructor(void* interface) {
    (void)interface;
}

static ETResult stub_memory_factory(void** interface, const ETInterfaceMetadata* metadata) {
    (void)metadata;
    *interface = NULL;
    return ET_SUCCESS;
}

static void stub_memory_destructor(void* interface) {
    (void)interface;
}

static ETResult stub_filesystem_factory(void** interface, const ETInterfaceMetadata* metadata) {
    (void)metadata;
    *interface = NULL;
    return ET_SUCCESS;
}

static void stub_filesystem_destructor(void* interface) {
    (void)interface;
}

static ETResult stub_network_factory(void** interface, const ETInterfaceMetadata* metadata) {
    (void)metadata;
    *interface = NULL;
    return ET_SUCCESS;
}

static void stub_network_destructor(void* interface) {
    (void)interface;
}

static ETResult stub_dynlib_factory(void** interface, const ETInterfaceMetadata* metadata) {
    (void)metadata;
    *interface = NULL;
    return ET_SUCCESS;
}

static void stub_dynlib_destructor(void* interface) {
    (void)interface;
}

/* Windows 플랫폼 인터페이스 등록 */
ETResult et_register_windows_interfaces(void) {
    ETResult result = ET_SUCCESS;

    /* 오디오 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_AUDIO,
            .version = {1, 0, 0, 0},
            .name = "Windows Audio Interface",
            .description = "DirectSound/WASAPI based audio interface",
            .platform = ET_PLATFORM_WINDOWS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_AUDIO, ET_PLATFORM_WINDOWS,
                                              windows_audio_factory, windows_audio_destructor, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 시스템 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_SYSTEM,
            .version = {1, 0, 0, 0},
            .name = "Windows System Interface",
            .description = "Windows API based system interface",
            .platform = ET_PLATFORM_WINDOWS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_SYSTEM, ET_PLATFORM_WINDOWS,
                                              stub_system_factory, stub_system_destructor, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 스레딩 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_THREAD,
            .version = {1, 0, 0, 0},
            .name = "Windows Threading Interface",
            .description = "Windows Thread API based threading interface",
            .platform = ET_PLATFORM_WINDOWS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_THREAD, ET_PLATFORM_WINDOWS,
                                              stub_thread_factory, stub_thread_destructor, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 메모리 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_MEMORY,
            .version = {1, 0, 0, 0},
            .name = "Windows Memory Interface",
            .description = "Windows VirtualAlloc based memory interface",
            .platform = ET_PLATFORM_WINDOWS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_MEMORY, ET_PLATFORM_WINDOWS,
                                              stub_memory_factory, stub_memory_destructor, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 파일시스템 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_FILESYSTEM,
            .version = {1, 0, 0, 0},
            .name = "Windows Filesystem Interface",
            .description = "Windows File API based filesystem interface",
            .platform = ET_PLATFORM_WINDOWS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_FILESYSTEM, ET_PLATFORM_WINDOWS,
                                              stub_filesystem_factory, stub_filesystem_destructor, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 네트워크 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_NETWORK,
            .version = {1, 0, 0, 0},
            .name = "Windows Network Interface",
            .description = "Winsock based network interface",
            .platform = ET_PLATFORM_WINDOWS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_NETWORK, ET_PLATFORM_WINDOWS,
                                              stub_network_factory, stub_network_destructor, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 동적 라이브러리 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_DYNLIB,
            .version = {1, 0, 0, 0},
            .name = "Windows Dynamic Library Interface",
            .description = "LoadLibrary/GetProcAddress based dynamic library interface",
            .platform = ET_PLATFORM_WINDOWS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_DYNLIB, ET_PLATFORM_WINDOWS,
                                              stub_dynlib_factory, stub_dynlib_destructor, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    return ET_SUCCESS;
}

#endif /* LIBETUDE_PLATFORM_WINDOWS */