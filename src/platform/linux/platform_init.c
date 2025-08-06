/**
 * @file platform_init.c
 * @brief Linux 플랫폼 인터페이스 등록 및 초기화
 * @author LibEtude Team
 */

#ifdef LIBETUDE_PLATFORM_LINUX

#include "libetude/platform/factory.h"
#include "libetude/platform/common.h"
#include "libetude/platform/audio.h"

// Linux 인터페이스 선언
extern const ETAudioInterface* et_get_linux_audio_interface(void);

extern ETSystemInterface* et_get_linux_system_interface(void);
extern ETResult et_linux_system_initialize(void);
extern void et_linux_system_cleanup(void);

/**
 * @brief Linux 오디오 인터페이스 팩토리 함수
 * @param config 설정 (사용하지 않음)
 * @return Linux 오디오 인터페이스 포인터
 */
static const void* linux_audio_factory(const void* config) {
    (void)config; // 사용하지 않는 매개변수
    return et_get_linux_audio_interface();
}

/**
 * @brief Linux 오디오 인터페이스 해제 함수
 * @param interface 인터페이스 포인터 (사용하지 않음)
 */
static void linux_audio_destructor(const void* interface) {
    (void)interface; // 정적 인터페이스이므로 해제할 것이 없음
}

/**
 * @brief Linux 시스템 인터페이스 팩토리 함수
 * @param config 설정 (사용하지 않음)
 * @return Linux 시스템 인터페이스 포인터
 */
static const void* linux_system_factory(const void* config) {
    (void)config; // 사용하지 않는 매개변수
    
    ETResult result = et_linux_system_initialize();
    if (result != ET_SUCCESS) {
        return NULL;
    }
    
    return et_get_linux_system_interface();
}

/**
 * @brief Linux 시스템 인터페이스 해제 함수
 * @param interface 인터페이스 포인터 (사용하지 않음)
 */
static void linux_system_destructor(const void* interface) {
    (void)interface; // 정적 인터페이스이므로 해제할 것이 없음
    et_linux_system_cleanup();
}

/* Linux 플랫폼 인터페이스 등록 */
ETResult et_register_linux_interfaces(void) {
    ETResult result = ET_SUCCESS;

    /* ALSA 오디오 인터페이스 등록 */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_AUDIO,
            .version = {1, 0, 0, 0},
            .name = "Linux ALSA Audio Interface",
            .description = "Advanced Linux Sound Architecture (ALSA) based audio interface",
            .platform = ET_PLATFORM_LINUX,
            .size = sizeof(ETAudioInterface),
            .flags = ET_INTERFACE_FLAG_THREAD_SAFE
        };

        result = et_register_interface_factory(ET_INTERFACE_AUDIO, ET_PLATFORM_LINUX,
                                              linux_audio_factory, linux_audio_destructor, &metadata);
        if (result != ET_SUCCESS) {
            ET_LOG_ERROR("Linux ALSA 오디오 인터페이스 등록 실패");
            return result;
        }

        ET_LOG_INFO("Linux ALSA 오디오 인터페이스 등록 완료");
    }

    /* 시스템 인터페이스 등록 */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_SYSTEM,
            .version = {1, 0, 0, 0},
            .name = "Linux System Interface",
            .description = "Linux syscall based system interface",
            .platform = ET_PLATFORM_LINUX,
            .size = sizeof(ETSystemInterface),
            .flags = ET_INTERFACE_FLAG_THREAD_SAFE
        };

        result = et_register_interface_factory(ET_INTERFACE_SYSTEM, ET_PLATFORM_LINUX,
                                              linux_system_factory, linux_system_destructor, &metadata);
        if (result != ET_SUCCESS) {
            ET_LOG_ERROR("Linux 시스템 인터페이스 등록 실패");
            return result;
        }

        ET_LOG_INFO("Linux 시스템 인터페이스 등록 완료");
    }

    /* 스레딩 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_THREAD,
            .version = {1, 0, 0, 0},
            .name = "Linux Threading Interface",
            .description = "POSIX pthread based threading interface",
            .platform = ET_PLATFORM_LINUX,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_THREAD, ET_PLATFORM_LINUX,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 메모리 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_MEMORY,
            .version = {1, 0, 0, 0},
            .name = "Linux Memory Interface",
            .description = "POSIX mmap based memory interface",
            .platform = ET_PLATFORM_LINUX,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_MEMORY, ET_PLATFORM_LINUX,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 파일시스템 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_FILESYSTEM,
            .version = {1, 0, 0, 0},
            .name = "Linux Filesystem Interface",
            .description = "POSIX file API based filesystem interface",
            .platform = ET_PLATFORM_LINUX,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_FILESYSTEM, ET_PLATFORM_LINUX,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 네트워크 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_NETWORK,
            .version = {1, 0, 0, 0},
            .name = "Linux Network Interface",
            .description = "Linux socket/epoll based network interface",
            .platform = ET_PLATFORM_LINUX,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_NETWORK, ET_PLATFORM_LINUX,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    /* 동적 라이브러리 인터페이스 등록 (스텁) */
    {
        ETInterfaceMetadata metadata = {
            .type = ET_INTERFACE_DYNLIB,
            .version = {1, 0, 0, 0},
            .name = "Linux Dynamic Library Interface",
            .description = "dlopen/dlsym based dynamic library interface",
            .platform = ET_PLATFORM_LINUX,
            .size = sizeof(void*), /* 실제 구조체 크기로 변경 예정 */
            .flags = 0
        };

        result = et_register_interface_factory(ET_INTERFACE_DYNLIB, ET_PLATFORM_LINUX,
                                              NULL, NULL, &metadata);
        if (result != ET_SUCCESS) return result;
    }

    return ET_SUCCESS;
}

#endif /* LIBETUDE_PLATFORM_LINUX */