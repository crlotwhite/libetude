/**
 * @file dynlib_common.c
 * @brief 동적 라이브러리 추상화 공통 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 동적 라이브러리 구현에서 공통으로 사용되는 기능들을 제공합니다.
 */

#include "libetude/platform/dynlib.h"
#include "libetude/platform/factory.h"
#include "libetude/platform/common.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// 전역 변수
// ============================================================================

static const ETDynlibInterface* g_dynlib_interface = NULL;
static bool g_dynlib_initialized = false;

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief 라이브러리 경로를 정규화합니다
 * @param path 원본 경로
 * @param normalized 정규화된 경로를 저장할 버퍼
 * @param size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult normalize_library_path(const char* path, char* normalized, size_t size) {
    if (!path || !normalized || size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 기본적인 경로 복사
    size_t path_len = strlen(path);
    if (path_len >= size) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    strcpy(normalized, path);

    // 플랫폼별 경로 구분자 정규화
#ifdef _WIN32
    // Windows: 슬래시를 백슬래시로 변환
    for (size_t i = 0; i < path_len; i++) {
        if (normalized[i] == '/') {
            normalized[i] = '\\';
        }
    }
#else
    // Unix 계열: 백슬래시를 슬래시로 변환
    for (size_t i = 0; i < path_len; i++) {
        if (normalized[i] == '\\') {
            normalized[i] = '/';
        }
    }
#endif

    return ET_SUCCESS;
}

/**
 * @brief 라이브러리 이름에서 확장자를 제거합니다
 * @param name 라이브러리 이름
 * @param base_name 기본 이름을 저장할 버퍼
 * @param size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult extract_base_name(const char* name, char* base_name, size_t size) {
    if (!name || !base_name || size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    size_t name_len = strlen(name);
    if (name_len >= size) {
        return ET_ERROR_BUFFER_TOO_SMALL;
    }

    strcpy(base_name, name);

    // 확장자 제거
    char* dot = strrchr(base_name, '.');
    if (dot) {
        *dot = '\0';
    }

    // 플랫폼별 접두사 제거 (lib 등)
#ifndef _WIN32
    if (strncmp(base_name, "lib", 3) == 0) {
        memmove(base_name, base_name + 3, strlen(base_name + 3) + 1);
    }
#endif

    return ET_SUCCESS;
}

// ============================================================================
// 공개 함수 구현
// ============================================================================

ETResult et_dynlib_initialize(void) {
    if (g_dynlib_initialized) {
        return ET_SUCCESS;
    }

    // 플랫폼 팩토리에서 동적 라이브러리 인터페이스 가져오기
    ETDynlibInterface* interface = NULL;
    ETResult result = et_create_dynlib_interface(&interface);
    if (result != ET_SUCCESS || !interface) {
        ET_LOG_ERROR("동적 라이브러리 인터페이스를 생성할 수 없습니다");
        return result;
    }

    g_dynlib_interface = interface;
    g_dynlib_initialized = true;
    ET_LOG_INFO("동적 라이브러리 인터페이스가 초기화되었습니다");

    return ET_SUCCESS;
}

void et_dynlib_finalize(void) {
    if (!g_dynlib_initialized) {
        return;
    }

    g_dynlib_interface = NULL;
    g_dynlib_initialized = false;

    ET_LOG_INFO("동적 라이브러리 인터페이스가 정리되었습니다");
}

const ETDynlibInterface* et_get_dynlib_interface(void) {
    if (!g_dynlib_initialized) {
        ET_LOG_ERROR("동적 라이브러리 인터페이스가 초기화되지 않았습니다");
        return NULL;
    }

    return g_dynlib_interface;
}

// ============================================================================
// 편의 함수 구현
// ============================================================================

ETResult et_dynlib_load(const char* path, ETDynamicLibrary** lib) {
    if (!g_dynlib_interface) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    if (!path || !lib) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 기본 플래그로 라이브러리 로드
    uint32_t default_flags = ET_DYNLIB_LAZY | ET_DYNLIB_LOCAL;

    return g_dynlib_interface->load_library(path, default_flags, lib);
}

void et_dynlib_unload(ETDynamicLibrary* lib) {
    if (!g_dynlib_interface || !lib) {
        return;
    }

    g_dynlib_interface->unload_library(lib);
}

ETResult et_dynlib_get_symbol(ETDynamicLibrary* lib, const char* symbol_name, void** symbol) {
    if (!g_dynlib_interface) {
        return ET_ERROR_NOT_INITIALIZED;
    }

    if (!lib || !symbol_name || !symbol) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    return g_dynlib_interface->get_symbol(lib, symbol_name, symbol);
}

bool et_dynlib_is_loaded(const char* path) {
    if (!g_dynlib_interface || !path) {
        return false;
    }

    return g_dynlib_interface->is_library_loaded(path);
}

const char* et_dynlib_get_error(void) {
    if (!g_dynlib_interface) {
        return "동적 라이브러리 인터페이스가 초기화되지 않았습니다";
    }

    return g_dynlib_interface->get_last_error();
}

// ============================================================================
// 플랫폼별 확장 함수 구현
// ============================================================================

ETResult et_dynlib_add_search_path(const char* path) {
    // 기본 구현: 환경 변수 수정 (플랫폼별로 오버라이드 가능)
    if (!path) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 플랫폼별 라이브러리 경로 환경 변수
#ifdef _WIN32
    const char* env_var = "PATH";
#elif defined(__APPLE__)
    const char* env_var = "DYLD_LIBRARY_PATH";
#else
    const char* env_var = "LD_LIBRARY_PATH";
#endif

    char* current_path = getenv(env_var);
    if (!current_path) {
        // 환경 변수가 없으면 새로 설정
        return setenv(env_var, path, 1) == 0 ? ET_SUCCESS : ET_ERROR_SYSTEM_ERROR;
    }

    // 기존 경로에 추가
    size_t new_path_len = strlen(current_path) + strlen(path) + 2; // ':' + '\0'
    char* new_path = malloc(new_path_len);
    if (!new_path) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

#ifdef _WIN32
    snprintf(new_path, new_path_len, "%s;%s", current_path, path);
#else
    snprintf(new_path, new_path_len, "%s:%s", current_path, path);
#endif

    int result = setenv(env_var, new_path, 1);
    free(new_path);

    return result == 0 ? ET_SUCCESS : ET_ERROR_SYSTEM_ERROR;
}

ETResult et_dynlib_remove_search_path(const char* path) {
    // 기본 구현: 환경 변수에서 경로 제거
    if (!path) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 플랫폼별 라이브러리 경로 환경 변수
#ifdef _WIN32
    const char* env_var = "PATH";
    const char* separator = ";";
#elif defined(__APPLE__)
    const char* env_var = "DYLD_LIBRARY_PATH";
    const char* separator = ":";
#else
    const char* env_var = "LD_LIBRARY_PATH";
    const char* separator = ":";
#endif

    char* current_path = getenv(env_var);
    if (!current_path) {
        return ET_SUCCESS; // 환경 변수가 없으면 이미 제거된 것으로 간주
    }

    // 경로에서 해당 부분 제거 (간단한 구현)
    char* found = strstr(current_path, path);
    if (!found) {
        return ET_SUCCESS; // 경로가 없으면 이미 제거된 것으로 간주
    }

    // 새로운 경로 문자열 생성 (실제 구현에서는 더 정교하게 처리 필요)
    size_t current_len = strlen(current_path);
    size_t path_len = strlen(path);
    char* new_path = malloc(current_len + 1);
    if (!new_path) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 간단한 제거 로직 (실제로는 더 복잡한 파싱 필요)
    strcpy(new_path, current_path);
    // TODO: 실제 경로 제거 로직 구현

    int result = setenv(env_var, new_path, 1);
    free(new_path);

    return result == 0 ? ET_SUCCESS : ET_ERROR_SYSTEM_ERROR;
}

ETResult et_dynlib_get_search_paths(char paths[][512], int* count) {
    if (!paths || !count) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 플랫폼별 라이브러리 경로 환경 변수
#ifdef _WIN32
    const char* env_var = "PATH";
    const char* separator = ";";
#elif defined(__APPLE__)
    const char* env_var = "DYLD_LIBRARY_PATH";
    const char* separator = ":";
#else
    const char* env_var = "LD_LIBRARY_PATH";
    const char* separator = ":";
#endif

    char* current_path = getenv(env_var);
    if (!current_path) {
        *count = 0;
        return ET_SUCCESS;
    }

    // 경로 파싱 (간단한 구현)
    char* path_copy = malloc(strlen(current_path) + 1);
    if (!path_copy) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    strcpy(path_copy, current_path);

    int path_count = 0;
    char* token = strtok(path_copy, separator);
    while (token && path_count < *count) {
        strncpy(paths[path_count], token, 511);
        paths[path_count][511] = '\0';
        path_count++;
        token = strtok(NULL, separator);
    }

    *count = path_count;
    free(path_copy);

    return ET_SUCCESS;
}

const char* et_dynlib_get_extension(void) {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

ETResult et_dynlib_format_name(const char* name, char* platform_name, size_t size) {
    if (!name || !platform_name || size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 기본 이름 추출
    char base_name[256];
    ETResult result = extract_base_name(name, base_name, sizeof(base_name));
    if (result != ET_SUCCESS) {
        return result;
    }

    // 플랫폼별 형식으로 변환
#ifdef _WIN32
    // Windows: name.dll
    snprintf(platform_name, size, "%s.dll", base_name);
#elif defined(__APPLE__)
    // macOS: libname.dylib
    snprintf(platform_name, size, "lib%s.dylib", base_name);
#else
    // Linux: libname.so
    snprintf(platform_name, size, "lib%s.so", base_name);
#endif

    return ET_SUCCESS;
}