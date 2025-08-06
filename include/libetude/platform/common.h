/**
 * @file common.h
 * @brief 플랫폼 추상화 레이어의 공통 타입 정의 및 플랫폼 감지
 * @author LibEtude Team
 */

#ifndef LIBETUDE_PLATFORM_COMMON_H
#define LIBETUDE_PLATFORM_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 플랫폼 감지 매크로 */
#if defined(_WIN32) || defined(_WIN64)
    #define LIBETUDE_PLATFORM_WINDOWS 1
    #define LIBETUDE_PLATFORM_NAME "Windows"
    #ifdef _WIN64
        #define LIBETUDE_ARCH_X64 1
    #else
        #define LIBETUDE_ARCH_X86 1
    #endif
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define LIBETUDE_PLATFORM_MACOS 1
        #define LIBETUDE_PLATFORM_NAME "macOS"
    #elif TARGET_OS_IPHONE
        #define LIBETUDE_PLATFORM_IOS 1
        #define LIBETUDE_PLATFORM_NAME "iOS"
    #endif
    #if defined(__x86_64__)
        #define LIBETUDE_ARCH_X64 1
    #elif defined(__aarch64__)
        #define LIBETUDE_ARCH_ARM64 1
    #endif
#elif defined(__linux__)
    #define LIBETUDE_PLATFORM_LINUX 1
    #define LIBETUDE_PLATFORM_NAME "Linux"
    #if defined(__x86_64__)
        #define LIBETUDE_ARCH_X64 1
    #elif defined(__i386__)
        #define LIBETUDE_ARCH_X86 1
    #elif defined(__aarch64__)
        #define LIBETUDE_ARCH_ARM64 1
    #elif defined(__arm__)
        #define LIBETUDE_ARCH_ARM 1
    #endif
#elif defined(__ANDROID__)
    #define LIBETUDE_PLATFORM_ANDROID 1
    #define LIBETUDE_PLATFORM_NAME "Android"
    #if defined(__aarch64__)
        #define LIBETUDE_ARCH_ARM64 1
    #elif defined(__arm__)
        #define LIBETUDE_ARCH_ARM 1
    #endif
#else
    #define LIBETUDE_PLATFORM_UNKNOWN 1
    #define LIBETUDE_PLATFORM_NAME "Unknown"
#endif

/* 컴파일러별 최적화 힌트 */
#if defined(_MSC_VER)
    #define LIBETUDE_INLINE __forceinline
    #define LIBETUDE_NOINLINE __declspec(noinline)
    #define LIBETUDE_ALIGN(n) __declspec(align(n))
    #define LIBETUDE_LIKELY(x) (x)
    #define LIBETUDE_UNLIKELY(x) (x)
#elif defined(__GNUC__) || defined(__clang__)
    #define LIBETUDE_INLINE inline __attribute__((always_inline))
    #define LIBETUDE_NOINLINE __attribute__((noinline))
    #define LIBETUDE_ALIGN(n) __attribute__((aligned(n)))
    #define LIBETUDE_LIKELY(x) __builtin_expect(!!(x), 1)
    #define LIBETUDE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIBETUDE_INLINE inline
    #define LIBETUDE_NOINLINE
    #define LIBETUDE_ALIGN(n)
    #define LIBETUDE_LIKELY(x) (x)
    #define LIBETUDE_UNLIKELY(x) (x)
#endif

/* 플랫폼 타입 열거형 */
typedef enum {
    ET_PLATFORM_UNKNOWN = 0,
    ET_PLATFORM_WINDOWS,
    ET_PLATFORM_LINUX,
    ET_PLATFORM_MACOS,
    ET_PLATFORM_IOS,
    ET_PLATFORM_ANDROID
} ETPlatformType;

/* 아키텍처 타입 열거형 */
typedef enum {
    ET_ARCH_UNKNOWN = 0,
    ET_ARCH_X86,
    ET_ARCH_X64,
    ET_ARCH_ARM,
    ET_ARCH_ARM64
} ETArchitecture;

/* 하드웨어 기능 플래그 */
typedef enum {
    ET_FEATURE_NONE = 0,
    ET_FEATURE_SSE = 1 << 0,
    ET_FEATURE_SSE2 = 1 << 1,
    ET_FEATURE_SSE3 = 1 << 2,
    ET_FEATURE_SSSE3 = 1 << 3,
    ET_FEATURE_SSE4_1 = 1 << 4,
    ET_FEATURE_SSE4_2 = 1 << 5,
    ET_FEATURE_AVX = 1 << 6,
    ET_FEATURE_AVX2 = 1 << 7,
    ET_FEATURE_AVX512 = 1 << 8,
    ET_FEATURE_NEON = 1 << 9,
    ET_FEATURE_FMA = 1 << 10
} ETHardwareFeature;

/* 플랫폼 정보 구조체 */
typedef struct {
    ETPlatformType type;           /* 플랫폼 타입 */
    char name[64];                 /* 플랫폼 이름 */
    char version[32];              /* 플랫폼 버전 */
    ETArchitecture arch;           /* 아키텍처 */
    uint32_t features;             /* 지원 기능 플래그 */
    uint32_t cpu_count;            /* CPU 코어 수 */
    uint64_t total_memory;         /* 총 메모리 (바이트) */
} ETPlatformInfo;

/* 공통 결과 코드 */
typedef enum {
    ET_SUCCESS = 0,                /* 성공 */
    ET_ERROR_INVALID_PARAMETER,    /* 잘못된 매개변수 */
    ET_ERROR_OUT_OF_MEMORY,        /* 메모리 부족 */
    ET_ERROR_NOT_SUPPORTED,        /* 지원되지 않는 기능 */
    ET_ERROR_PLATFORM_SPECIFIC,    /* 플랫폼별 오류 */
    ET_ERROR_INITIALIZATION_FAILED,/* 초기화 실패 */
    ET_ERROR_RESOURCE_BUSY,        /* 리소스 사용 중 */
    ET_ERROR_TIMEOUT,              /* 시간 초과 */
    ET_ERROR_IO_ERROR,             /* I/O 오류 */
    ET_ERROR_NETWORK_ERROR,        /* 네트워크 오류 */
    ET_ERROR_UNKNOWN               /* 알 수 없는 오류 */
} ETResult;

/* 오류 정보 구조체 */
typedef struct {
    ETResult code;                 /* 공통 오류 코드 */
    int platform_code;             /* 플랫폼별 오류 코드 */
    char message[256];             /* 오류 메시지 */
    const char* file;              /* 발생 파일 */
    int line;                      /* 발생 라인 */
    const char* function;          /* 발생 함수 */
    uint64_t timestamp;            /* 발생 시간 */
    ETPlatformType platform;       /* 발생 플랫폼 */
} ETDetailedError;

/* 플랫폼 정보 조회 함수 */
ETResult et_get_platform_info(ETPlatformInfo* info);

/* 현재 플랫폼 타입 조회 */
ETPlatformType et_get_current_platform(void);

/* 현재 아키텍처 조회 */
ETArchitecture et_get_current_architecture(void);

/* 하드웨어 기능 감지 */
uint32_t et_detect_hardware_features(void);

/* 특정 기능 지원 여부 확인 */
bool et_has_hardware_feature(ETHardwareFeature feature);

/* 오류 코드를 문자열로 변환 */
const char* et_result_to_string(ETResult result);

/* 상세 오류 정보 설정 */
void et_set_detailed_error(ETDetailedError* error, ETResult code, int platform_code,
                          const char* message, const char* file, int line, const char* function);

/* 마지막 오류 정보 조회 */
const ETDetailedError* et_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBETUDE_PLATFORM_COMMON_H */