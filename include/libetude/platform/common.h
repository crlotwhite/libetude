/**
 * @file common.h
 * @brief 플랫폼 추상화 공통 정의
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 모든 플랫폼 추상화 레이어에서 공통으로 사용되는 타입과 상수를 정의합니다.
 */

#ifndef LIBETUDE_PLATFORM_COMMON_H
#define LIBETUDE_PLATFORM_COMMON_H

#include "libetude/types.h"
#include "libetude/error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 플랫폼 타입 정의
// ============================================================================

/**
 * @brief 플랫폼 타입 열거형
 */
typedef enum {
    ET_PLATFORM_UNKNOWN = 0,    /**< 알 수 없는 플랫폼 */
    ET_PLATFORM_WINDOWS = 1,    /**< Windows */
    ET_PLATFORM_LINUX = 2,      /**< Linux */
    ET_PLATFORM_MACOS = 3,      /**< macOS */
    ET_PLATFORM_ANDROID = 4,    /**< Android */
    ET_PLATFORM_IOS = 5         /**< iOS */
} ETPlatformType;

/**
 * @brief 아키텍처 타입 열거형
 */
typedef enum {
    ET_ARCH_UNKNOWN = 0,        /**< 알 수 없는 아키텍처 */
    ET_ARCH_X86 = 1,           /**< x86 (32비트) */
    ET_ARCH_X64 = 2,           /**< x86-64 (64비트) */
    ET_ARCH_ARM = 3,           /**< ARM (32비트) */
    ET_ARCH_ARM64 = 4          /**< ARM64 (64비트) */
} ETArchitecture;

/**
 * @brief 하드웨어 기능 플래그
 */
typedef enum {
    ET_FEATURE_NONE = 0,           /**< 기본 기능만 */
    ET_FEATURE_SSE = 1 << 0,       /**< SSE 지원 */
    ET_FEATURE_SSE2 = 1 << 1,      /**< SSE2 지원 */
    ET_FEATURE_SSE3 = 1 << 2,      /**< SSE3 지원 */
    ET_FEATURE_SSSE3 = 1 << 3,     /**< SSSE3 지원 */
    ET_FEATURE_SSE4_1 = 1 << 4,    /**< SSE4.1 지원 */
    ET_FEATURE_SSE4_2 = 1 << 5,    /**< SSE4.2 지원 */
    ET_FEATURE_AVX = 1 << 6,       /**< AVX 지원 */
    ET_FEATURE_AVX2 = 1 << 7,      /**< AVX2 지원 */
    ET_FEATURE_AVX512 = 1 << 8,    /**< AVX512 지원 */
    ET_FEATURE_NEON = 1 << 9,      /**< ARM NEON 지원 */
    ET_FEATURE_FMA = 1 << 10,      /**< FMA 지원 */
    ET_FEATURE_GPU = 1 << 11,      /**< GPU 가속 지원 */
    ET_FEATURE_AUDIO_HW = 1 << 12, /**< 하드웨어 오디오 가속 */
    ET_FEATURE_HIGH_RES_TIMER = 1 << 13  /**< 고해상도 타이머 */
} ETHardwareFeature;

// 하위 호환성을 위한 별칭
typedef ETHardwareFeature ETFeatureFlags;

/**
 * @brief 플랫폼 정보 구조체
 */
typedef struct {
    ETPlatformType type;           /**< 플랫폼 타입 */
    char name[64];                 /**< 플랫폼 이름 */
    char version[32];              /**< 플랫폼 버전 */
    ETArchitecture arch;           /**< 아키텍처 */
    uint32_t features;             /**< 지원 기능 플래그 */
    uint32_t cpu_count;            /**< CPU 코어 수 */
    uint64_t total_memory;         /**< 총 메모리 크기 (바이트) */
} ETPlatformInfo;

// ============================================================================
// 오류 매핑 시스템
// ============================================================================

/**
 * @brief 플랫폼별 오류를 공통 오류로 매핑하는 구조체
 */
typedef struct {
    int platform_error;           /**< 플랫폼별 오류 코드 */
    ETResult common_error;        /**< 공통 오류 코드 */
    const char* description;      /**< 오류 설명 */
} ETErrorMapping;

/**
 * @brief 상세 오류 정보 구조체
 */
typedef struct {
    ETResult code;                /**< 공통 오류 코드 */
    int platform_code;            /**< 플랫폼별 오류 코드 */
    char message[256];            /**< 오류 메시지 */
    const char* file;             /**< 발생 파일 */
    int line;                     /**< 발생 라인 */
    const char* function;         /**< 발생 함수 */
    uint64_t timestamp;           /**< 발생 시간 */
    ETPlatformType platform;      /**< 발생 플랫폼 */
} ETDetailedError;

// ============================================================================
// 공통 함수 선언
// ============================================================================

// ============================================================================
// 핵심 플랫폼 함수 선언
// ============================================================================

/**
 * @brief 플랫폼 추상화 레이어를 초기화합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_platform_initialize(void);

/**
 * @brief 현재 플랫폼 정보를 가져옵니다
 * @param info 플랫폼 정보를 저장할 구조체 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_platform_info(ETPlatformInfo* info);

/**
 * @brief 플랫폼 추상화 레이어를 정리합니다
 */
void et_platform_finalize(void);

/**
 * @brief 현재 플랫폼 타입을 가져옵니다
 * @return 현재 플랫폼 타입
 */
ETPlatformType et_get_current_platform(void);

/**
 * @brief 현재 아키텍처를 가져옵니다
 * @return 현재 아키텍처
 */
ETArchitecture et_get_current_architecture(void);

/**
 * @brief 하드웨어 기능 지원 여부를 확인합니다
 * @param feature 확인할 하드웨어 기능
 * @return 지원하면 true, 아니면 false
 */
bool et_has_hardware_feature(ETHardwareFeature feature);

/**
 * @brief 플랫폼별 오류를 공통 오류로 변환합니다 (기존 함수)
 * @param platform 플랫폼 타입
 * @param platform_error 플랫폼별 오류 코드
 * @return 공통 오류 코드
 */
ETResult et_platform_error_to_common(ETPlatformType platform, int platform_error);

/**
 * @brief 플랫폼별 오류를 공통 오류로 변환합니다
 * @param platform 플랫폼 타입
 * @param platform_error 플랫폼별 오류 코드
 * @return 공통 오류 코드
 */
ETResult et_platform_error_to_common(ETPlatformType platform, int platform_error);

/**
 * @brief 플랫폼별 오류 설명을 가져옵니다
 * @param platform 플랫폼 타입
 * @param platform_error 플랫폼별 오류 코드
 * @return 오류 설명 문자열
 */
const char* et_get_platform_error_description(ETPlatformType platform, int platform_error);

/**
 * @brief 하드웨어 기능 지원 여부를 확인합니다
 * @param feature 확인할 하드웨어 기능
 * @return 지원하면 true, 아니면 false
 */
bool et_platform_has_feature(ETHardwareFeature feature);

/**
 * @brief 현재 플랫폼의 팩토리를 가져옵니다
 * @return 플랫폼 팩토리 포인터
 */
const struct ETPlatformFactory* et_get_platform_factory(void);

// ============================================================================
// 로깅 매크로 (간단한 구현)
// ============================================================================

#ifndef ET_LOG_INFO
#define ET_LOG_INFO(msg) printf("[INFO] %s\n", msg)
#endif

#ifndef ET_LOG_ERROR
#define ET_LOG_ERROR(msg) printf("[ERROR] %s\n", msg)
#endif

#ifndef ET_LOG_DEBUG
#define ET_LOG_DEBUG(msg) printf("[DEBUG] %s\n", msg)
#endif

// ============================================================================
// 오류 처리 매크로
// ============================================================================

/**
 * @brief 상세 오류 정보를 설정합니다
 */
void et_set_detailed_error(void* context, ETResult code, int platform_code,
                          const char* message, const char* file, int line, const char* function);

/**
 * @brief 오류를 초기화합니다
 */
void et_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_COMMON_H