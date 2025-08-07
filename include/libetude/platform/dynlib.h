/**
 * @file dynlib.h
 * @brief 동적 라이브러리 추상화 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 동적 라이브러리 로딩 및 심볼 해석 기능을 추상화합니다.
 * Windows DLL, Unix 공유 라이브러리(.so), macOS 동적 라이브러리(.dylib)를
 * 통일된 인터페이스로 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_DYNLIB_H
#define LIBETUDE_PLATFORM_DYNLIB_H

#include "libetude/platform/common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 타입 정의
// ============================================================================

/**
 * @brief 동적 라이브러리 핸들 (불투명 포인터)
 */
typedef struct ETDynamicLibrary ETDynamicLibrary;

/**
 * @brief 라이브러리 로딩 플래그
 */
typedef enum {
    ET_DYNLIB_LAZY = 1 << 0,        /**< 지연 로딩 (RTLD_LAZY) */
    ET_DYNLIB_NOW = 1 << 1,         /**< 즉시 로딩 (RTLD_NOW) */
    ET_DYNLIB_GLOBAL = 1 << 2,      /**< 전역 심볼 (RTLD_GLOBAL) */
    ET_DYNLIB_LOCAL = 1 << 3,       /**< 로컬 심볼 (RTLD_LOCAL) */
    ET_DYNLIB_NODELETE = 1 << 4,    /**< 언로드 방지 (RTLD_NODELETE) */
    ET_DYNLIB_NOLOAD = 1 << 5,      /**< 이미 로드된 라이브러리만 (RTLD_NOLOAD) */
    ET_DYNLIB_DEEPBIND = 1 << 6     /**< 심볼 우선순위 (RTLD_DEEPBIND) */
} ETDynlibFlags;

/**
 * @brief 라이브러리 정보 구조체
 */
typedef struct {
    char path[512];                 /**< 라이브러리 경로 */
    char name[256];                 /**< 라이브러리 이름 */
    uint64_t size;                  /**< 라이브러리 크기 (바이트) */
    uint32_t version_major;         /**< 주 버전 번호 */
    uint32_t version_minor;         /**< 부 버전 번호 */
    uint32_t version_patch;         /**< 패치 버전 번호 */
    bool is_loaded;                 /**< 로드 상태 */
    uint32_t ref_count;             /**< 참조 카운트 */
    void* platform_handle;          /**< 플랫폼별 핸들 */
} ETDynlibInfo;

/**
 * @brief 심볼 정보 구조체
 */
typedef struct {
    char name[256];                 /**< 심볼 이름 */
    void* address;                  /**< 심볼 주소 */
    uint32_t size;                  /**< 심볼 크기 (가능한 경우) */
    bool is_function;               /**< 함수 여부 */
    bool is_exported;               /**< 내보내기 여부 */
} ETSymbolInfo;

/**
 * @brief 의존성 정보 구조체
 */
typedef struct {
    char name[256];                 /**< 의존성 라이브러리 이름 */
    char path[512];                 /**< 의존성 라이브러리 경로 */
    bool is_required;               /**< 필수 의존성 여부 */
    bool is_loaded;                 /**< 로드 상태 */
} ETDependencyInfo;

// ============================================================================
// 동적 라이브러리 인터페이스
// ============================================================================

/**
 * @brief 동적 라이브러리 추상화 인터페이스
 */
typedef struct ETDynlibInterface {
    // 라이브러리 로딩
    ETResult (*load_library)(const char* path, uint32_t flags, ETDynamicLibrary** lib);
    ETResult (*load_library_from_memory)(const void* data, size_t size, ETDynamicLibrary** lib);
    void (*unload_library)(ETDynamicLibrary* lib);

    // 심볼 해석
    ETResult (*get_symbol)(ETDynamicLibrary* lib, const char* symbol_name, void** symbol);
    ETResult (*get_symbol_info)(ETDynamicLibrary* lib, const char* symbol_name, ETSymbolInfo* info);
    ETResult (*enumerate_symbols)(ETDynamicLibrary* lib, ETSymbolInfo* symbols, int* count);

    // 라이브러리 정보
    ETResult (*get_library_info)(ETDynamicLibrary* lib, ETDynlibInfo* info);
    ETResult (*get_library_path)(ETDynamicLibrary* lib, char* path, size_t path_size);
    bool (*is_library_loaded)(const char* path);

    // 의존성 관리
    ETResult (*get_dependencies)(ETDynamicLibrary* lib, ETDependencyInfo* deps, int* count);
    ETResult (*resolve_dependencies)(ETDynamicLibrary* lib);
    ETResult (*check_dependencies)(const char* path, ETDependencyInfo* missing_deps, int* count);

    // 오류 처리
    const char* (*get_last_error)(void);
    ETResult (*get_last_error_code)(void);
    void (*clear_error)(void);

    // 플랫폼별 확장
    void* platform_data;           /**< 플랫폼별 데이터 */
} ETDynlibInterface;

// ============================================================================
// 공통 함수 선언
// ============================================================================

/**
 * @brief 동적 라이브러리 인터페이스를 초기화합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dynlib_initialize(void);

/**
 * @brief 동적 라이브러리 인터페이스를 정리합니다
 */
void et_dynlib_finalize(void);

/**
 * @brief 현재 플랫폼의 동적 라이브러리 인터페이스를 가져옵니다
 * @return 동적 라이브러리 인터페이스 포인터
 */
const ETDynlibInterface* et_get_dynlib_interface(void);

// ============================================================================
// 편의 함수들
// ============================================================================

/**
 * @brief 동적 라이브러리를 로드합니다 (기본 플래그 사용)
 * @param path 라이브러리 경로
 * @param lib 로드된 라이브러리 핸들을 저장할 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dynlib_load(const char* path, ETDynamicLibrary** lib);

/**
 * @brief 동적 라이브러리를 언로드합니다
 * @param lib 라이브러리 핸들
 */
void et_dynlib_unload(ETDynamicLibrary* lib);

/**
 * @brief 심볼을 가져옵니다
 * @param lib 라이브러리 핸들
 * @param symbol_name 심볼 이름
 * @param symbol 심볼 주소를 저장할 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dynlib_get_symbol(ETDynamicLibrary* lib, const char* symbol_name, void** symbol);

/**
 * @brief 라이브러리가 로드되어 있는지 확인합니다
 * @param path 라이브러리 경로
 * @return 로드되어 있으면 true, 아니면 false
 */
bool et_dynlib_is_loaded(const char* path);

/**
 * @brief 마지막 오류 메시지를 가져옵니다
 * @return 오류 메시지 문자열
 */
const char* et_dynlib_get_error(void);

// ============================================================================
// 플랫폼별 확장 함수 (선택적)
// ============================================================================

/**
 * @brief 라이브러리 검색 경로를 추가합니다
 * @param path 검색 경로
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dynlib_add_search_path(const char* path);

/**
 * @brief 라이브러리 검색 경로를 제거합니다
 * @param path 검색 경로
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dynlib_remove_search_path(const char* path);

/**
 * @brief 현재 설정된 검색 경로들을 가져옵니다
 * @param paths 경로 배열을 저장할 버퍼
 * @param count 경로 개수를 저장할 포인터
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dynlib_get_search_paths(char paths[][512], int* count);

/**
 * @brief 플랫폼별 라이브러리 확장자를 가져옵니다
 * @return 라이브러리 확장자 문자열 (.dll, .so, .dylib 등)
 */
const char* et_dynlib_get_extension(void);

/**
 * @brief 라이브러리 이름을 플랫폼별 형식으로 변환합니다
 * @param name 기본 라이브러리 이름
 * @param platform_name 플랫폼별 이름을 저장할 버퍼
 * @param size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_dynlib_format_name(const char* name, char* platform_name, size_t size);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_DYNLIB_H