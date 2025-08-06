/**
 * @file factory.h
 * @brief 플랫폼 추상화 레이어의 팩토리 패턴 기반 인터페이스 생성
 * @author LibEtude Team
 */

#ifndef LIBETUDE_PLATFORM_FACTORY_H
#define LIBETUDE_PLATFORM_FACTORY_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 전방 선언 */
struct ETAudioInterface;
struct ETSystemInterface;
struct ETThreadInterface;
struct ETMemoryInterface;
struct ETFilesystemInterface;
struct ETNetworkInterface;
struct ETDynlibInterface;

/* 인터페이스 타입 열거형 */
typedef enum {
    ET_INTERFACE_AUDIO = 0,
    ET_INTERFACE_SYSTEM,
    ET_INTERFACE_THREAD,
    ET_INTERFACE_MEMORY,
    ET_INTERFACE_FILESYSTEM,
    ET_INTERFACE_NETWORK,
    ET_INTERFACE_DYNLIB,
    ET_INTERFACE_COUNT
} ETInterfaceType;

/* 인터페이스 버전 정보 */
typedef struct {
    uint32_t major;                /* 주 버전 */
    uint32_t minor;                /* 부 버전 */
    uint32_t patch;                /* 패치 버전 */
    uint32_t build;                /* 빌드 번호 */
} ETInterfaceVersion;

/* 인터페이스 메타데이터 */
typedef struct {
    ETInterfaceType type;          /* 인터페이스 타입 */
    ETInterfaceVersion version;    /* 인터페이스 버전 */
    const char* name;              /* 인터페이스 이름 */
    const char* description;       /* 인터페이스 설명 */
    ETPlatformType platform;       /* 대상 플랫폼 */
    uint32_t size;                 /* 구조체 크기 */
    uint32_t flags;                /* 기능 플래그 */
} ETInterfaceMetadata;

/* 인터페이스 팩토리 함수 타입 */
typedef ETResult (*ETInterfaceFactory)(void** interface, const ETInterfaceMetadata* metadata);

/* 인터페이스 소멸자 함수 타입 */
typedef void (*ETInterfaceDestructor)(void* interface);

/* 플랫폼 인터페이스 레지스트리 */
typedef struct {
    ETInterfaceType type;          /* 인터페이스 타입 */
    ETPlatformType platform;       /* 플랫폼 타입 */
    ETInterfaceFactory factory;    /* 팩토리 함수 */
    ETInterfaceDestructor destructor; /* 소멸자 함수 */
    ETInterfaceMetadata metadata;  /* 메타데이터 */
    bool is_available;             /* 사용 가능 여부 */
} ETInterfaceRegistry;

/* 플랫폼 컨텍스트 구조체 */
typedef struct {
    ETPlatformInfo platform_info;                    /* 플랫폼 정보 */
    ETInterfaceRegistry registry[ET_INTERFACE_COUNT]; /* 인터페이스 레지스트리 */
    void* interfaces[ET_INTERFACE_COUNT];            /* 생성된 인터페이스들 */
    bool initialized;                                /* 초기화 상태 */
    ETDetailedError last_error;                      /* 마지막 오류 */
} ETPlatformContext;

/* 전역 플랫폼 컨텍스트 */
extern ETPlatformContext g_platform_context;

/* 플랫폼 초기화 */
ETResult et_platform_initialize(void);

/* 플랫폼 정리 */
void et_platform_finalize(void);

/* 인터페이스 팩토리 등록 */
ETResult et_register_interface_factory(ETInterfaceType type, ETPlatformType platform,
                                      ETInterfaceFactory factory, ETInterfaceDestructor destructor,
                                      const ETInterfaceMetadata* metadata);

/* 인터페이스 생성 */
ETResult et_create_interface(ETInterfaceType type, void** interface);

/* 인터페이스 소멸 */
void et_destroy_interface(ETInterfaceType type, void* interface);

/* 인터페이스 조회 */
void* et_get_interface(ETInterfaceType type);

/* 인터페이스 사용 가능 여부 확인 */
bool et_is_interface_available(ETInterfaceType type);

/* 인터페이스 메타데이터 조회 */
const ETInterfaceMetadata* et_get_interface_metadata(ETInterfaceType type);

/* 버전 호환성 검사 */
bool et_is_interface_compatible(const ETInterfaceVersion* required, const ETInterfaceVersion* provided);

/* 인터페이스 타입을 문자열로 변환 */
const char* et_interface_type_to_string(ETInterfaceType type);

/* 플랫폼별 자동 등록 함수들 */
#ifdef LIBETUDE_PLATFORM_WINDOWS
ETResult et_register_windows_interfaces(void);
#endif

#ifdef LIBETUDE_PLATFORM_LINUX
ETResult et_register_linux_interfaces(void);
#endif

#ifdef LIBETUDE_PLATFORM_MACOS
ETResult et_register_macos_interfaces(void);
#endif

/* 디버그 및 진단 함수들 */
#ifdef LIBETUDE_DEBUG
void et_dump_platform_info(void);
void et_dump_interface_registry(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LIBETUDE_PLATFORM_FACTORY_H */