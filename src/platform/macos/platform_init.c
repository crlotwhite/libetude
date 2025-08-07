/**
 * @file platform_init.c
 * @brief macOS 플랫폼 인터페이스 등록 및 초기화
 * @author LibEtude Team
 */

#ifdef LIBETUDE_PLATFORM_MACOS

#include "libetude/platform/factory.h"
#include "libetude/platform/common.h"

// macOS 인터페이스 선언
extern ETThreadInterface* et_get_posix_thread_interface(void);
extern ETResult et_create_posix_thread_interface(ETThreadInterface** interface);
extern void et_destroy_posix_thread_interface(ETThreadInterface* interface);

/* macOS 플랫폼 인터페이스 등록 */
ETResult et_register_macos_interfaces(void) {
    ETResult result = ET_SUCCESS;

    /* 오디오 인터페이스 등록 */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_AUDIO,
            .version = {1, 0, 0, 0},
            .name = "macOS Audio Interface",
            .description = "CoreAudio based audio interface",
            .platform = ET_PLATFORM_MACOS,
            .size = sizeof(ETAudioInterface),
            .flags = 0
        };

        // 외부 함수 선언 (헤더에서 가져올 예정)
        extern ETAudioInterface* et_create_macos_audio_interface(void);
        extern void et_destroy_macos_audio_interface(ETAudioInterface* interface);

        result = et_register_interface_factory(ET_INTERFACE_AUDIO, ET_PLATFORM_MACOS,
                                              (ETInterfaceCreateFunc)et_create_macos_audio_interface,
                                              (ETInterfaceDestroyFunc)et_destroy_macos_audio_interface,
                                              &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 시스템 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_SYSTEM,
            .version = {1, 0, 0, 0},
            .name = "macOS System Interface",
            .description = "macOS sysctl/mach based system interface",
            .platform = ET_PLATFORM_MACOS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_SYSTEM, ET_PLATFORM_MACOS,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 스레딩 인터페이스 등록 */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_THREAD,
            .version = {1, 0, 0, 0},
            .name = "macOS Threading Interface",
            .description = "POSIX pthread based threading interface",
            .platform = ET_PLATFORM_MACOS,
            .size = sizeof(ETThreadInterface),
            .flags = ET_INTERFACE_FLAG_THREAD_SAFE
        };

        result = et_register_interface_factory(ET_INTERFACE_THREAD, ET_PLATFORM_MACOS,
                                              (ETInterfaceCreateFunc)et_create_posix_thread_interface,
                                              (ETInterfaceDestroyFunc)et_destroy_posix_thread_interface,
                                              &metadata);
        if (result != ET_SUCCESS) {
            ET_LOG_ERROR("macOS 스레딩 인터페이스 등록 실패");
            return result;
        }

        ET_LOG_INFO("macOS 스레딩 인터페이스 등록 완료");
    }

    /* 메모리 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_MEMORY,
            .version = {1, 0, 0, 0},
            .name = "macOS Memory Interface",
            .description = "POSIX mmap/mach based memory interface",
            .platform = ET_PLATFORM_MACOS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_MEMORY, ET_PLATFORM_MACOS,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 파일시스템 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_FILESYSTEM,
            .version = {1, 0, 0, 0},
            .name = "macOS Filesystem Interface",
            .description = "POSIX file API based filesystem interface",
            .platform = ET_PLATFORM_MACOS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_FILESYSTEM, ET_PLATFORM_MACOS,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 네트워크 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_NETWORK,
            .version = {1, 0, 0, 0},
            .name = "macOS Network Interface",
            .description = "macOS socket/kqueue based network interface",
            .platform = ET_PLATFORM_MACOS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_NETWORK, ET_PLATFORM_MACOS,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 동적 라이브러리 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_DYNLIB,
            .version = {1, 0, 0, 0},
            .name = "macOS Dynamic Library Interface",
            .description = "dlopen/dlsym based dynamic library interface",
            .platform = ET_PLATFORM_MACOS,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_DYNLIB, ET_PLATFORM_MACOS,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    return ET_SUCCESS;
}

#endif /* LIBETUDE_PLATFORM_MACOS */