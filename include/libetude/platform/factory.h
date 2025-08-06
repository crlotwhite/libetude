/**
 * @file factory.h
 * @brief 플랫폼 추상화 팩토리 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 구현체를 생성하고 관리하는 팩토리 패턴을 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_FACTORY_H
#define LIBETUDE_PLATFORM_FACTORY_H

#include "libetude/platform/common.h"
#include "libetude/platform/audio.h"
#include "libetude/types.h"
#include "libetude/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 플랫폼 팩토리 인터페이스
// ============================================================================

/**
 * @brief 플랫폼 팩토리 구조체
 */
typedef struct ETPlatformFactory {
    // 플랫폼 정보
    ETPlatformType platform_type;
    const char* platform_name;

    // 인터페이스 생성 함수들
    ETResult (*create_audio_interface)(ETAudioInterface** interface);
    void (*destroy_audio_interface)(ETAudioInterface* interface);

    // 플랫폼 초기화/정리
    ETResult (*initialize)(void);
    void (*finalize)(void);

    // 플랫폼별 확장 데이터
    void* platform_data;
} ETPlatformFactory;

// ============================================================================
// 전역 팩토리 관리 함수
// ============================================================================

/**
 * @brief 플랫폼 팩토리 시스템을 초기화합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_platform_factory_init(void);

/**
 * @brief 플랫폼 팩토리 시스템을 정리합니다
 */
void et_platform_factory_cleanup(void);

/**
 * @brief 현재 플랫폼에 맞는 팩토리를 가져옵니다
 * @return 플랫폼 팩토리 포인터 (실패시 NULL)
 */
const ETPlatformFactory* et_platform_factory_get_current(void);

/**
 * @brief 특정 플랫폼의 팩토리를 가져옵니다
 * @param platform_type 플랫폼 타입
 * @return 플랫폼 팩토리 포인터 (실패시 NULL)
 */
const ETPlatformFactory* et_platform_factory_get(ETPlatformType platform_type);

/**
 * @brief 사용 가능한 플랫폼 목록을 가져옵니다
 * @param platforms 플랫폼 타입 배열 (출력)
 * @param count 배열 크기 (입력) / 실제 플랫폼 수 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_platform_factory_list_available(ETPlatformType* platforms, int* count);

// ============================================================================
// 편의 함수들
// ============================================================================

/**
 * @brief 현재 플랫폼의 오디오 인터페이스를 생성합니다
 * @param interface 생성된 인터페이스 포인터 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_create_audio_interface(ETAudioInterface** interface);

/**
 * @brief 오디오 인터페이스를 해제합니다
 * @param interface 해제할 인터페이스
 */
void et_destroy_audio_interface(ETAudioInterface* interface);

/**
 * @brief 현재 플랫폼을 자동 감지합니다
 * @return 감지된 플랫폼 타입
 */
ETPlatformType et_platform_detect(void);

/**
 * @brief 플랫폼 타입을 문자열로 변환합니다
 * @param platform_type 플랫폼 타입
 * @return 플랫폼 이름 문자열
 */
const char* et_platform_type_to_string(ETPlatformType platform_type);

/**
 * @brief 문자열을 플랫폼 타입으로 변환합니다
 * @param platform_name 플랫폼 이름 문자열
 * @return 플랫폼 타입 (실패시 ET_PLATFORM_UNKNOWN)
 */
ETPlatformType et_platform_type_from_string(const char* platform_name);

// ============================================================================
// 플랫폼별 팩토리 등록 함수 (내부 사용)
// ============================================================================

/**
 * @brief 플랫폼 팩토리를 등록합니다 (내부 사용)
 * @param factory 등록할 팩토리
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_platform_factory_register(const ETPlatformFactory* factory);

/**
 * @brief 플랫폼 팩토리 등록을 해제합니다 (내부 사용)
 * @param platform_type 해제할 플랫폼 타입
 */
void et_platform_factory_unregister(ETPlatformType platform_type);

// ============================================================================
// 플랫폼별 팩토리 선언 (각 플랫폼에서 구현)
// ============================================================================

#ifdef _WIN32
/**
 * @brief Windows 플랫폼 팩토리를 가져옵니다
 * @return Windows 팩토리 포인터
 */
const ETPlatformFactory* et_platform_factory_windows(void);
#endif

#ifdef __linux__
/**
 * @brief Linux 플랫폼 팩토리를 가져옵니다
 * @return Linux 팩토리 포인터
 */
const ETPlatformFactory* et_platform_factory_linux(void);
#endif

#ifdef __APPLE__
/**
 * @brief macOS 플랫폼 팩토리를 가져옵니다
 * @return macOS 팩토리 포인터
 */
const ETPlatformFactory* et_platform_factory_macos(void);
#endif

#ifdef __ANDROID__
/**
 * @brief Android 플랫폼 팩토리를 가져옵니다
 * @return Android 팩토리 포인터
 */
const ETPlatformFactory* et_platform_factory_android(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_FACTORY_H