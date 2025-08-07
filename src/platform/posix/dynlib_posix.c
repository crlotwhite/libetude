/**
 * @file dynlib_posix.c
 * @brief POSIX 동적 라이브러리 구현체 (Linux/macOS 공통)
 * @author LibEtude Project
 * @version 1.0.0
 *
 * POSIX dlopen/dlsym API를 사용한 동적 라이브러리 구현
 */

#ifndef _WIN32

#include "libetude/platform/dynlib.h"
#include "libetude/platform/common.h"
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#endif

// ============================================================================
// POSIX 전용 구조체 정의
// ============================================================================

/**
 * @brief POSIX 동적 라이브러리 핸들 구조체
 */
typedef struct ETDynamicLibraryPosix {
    void* handle;                   /**< dlopen 핸들 */
    char path[PATH_MAX];           /**< 라이브러리 경로 */
    char name[256];                /**< 라이브러리 이름 */
    uint32_t ref_count;            /**< 참조 카운트 */
    bool is_loaded;                /**< 로드 상태 */
    int load_flags;                /**< 로드 플래그 */
} ETDynamicLibraryPosix;

/**
 * @brief POSIX 동적 라이브러리 플랫폼 데이터
 */
typedef struct {
    char search_paths[16][PATH_MAX]; /**< 검색 경로 배열 */
    int search_path_count;           /**< 검색 경로 개수 */
    int last_errno;                  /**< 마지막 errno */
    char last_error_message[512];    /**< 마지막 오류 메시지 */
} ETDynlibPosixData;

// ============================================================================
// 전역 변수
// ============================================================================

static ETDynlibPosixData g_posix_data = {0};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief ETDynlibFlags를 POSIX 플래그로 변환
 */
static int convert_flags_to_posix(uint32_t flags) {
    int posix_flags = 0;

    if (flags & ET_DYNLIB_LAZY) {
        posix_flags |= RTLD_LAZY;
    } else if (flags & ET_DYNLIB_NOW) {
        posix_flags |= RTLD_NOW;
    } else {
        posix_flags |= RTLD_LAZY; // 기본값
    }

    if (flags & ET_DYNLIB_GLOBAL) {
        posix_flags |= RTLD_GLOBAL;
    } else if (flags & ET_DYNLIB_LOCAL) {
        posix_flags |= RTLD_LOCAL;
    } else {
        posix_flags |= RTLD_LOCAL; // 기본값
    }

#ifdef RTLD_NODELETE
    if (flags & ET_DYNLIB_NODELETE) {
        posix_flags |= RTLD_NODELETE;
    }
#endif

#ifdef RTLD_NOLOAD
    if (flags & ET_DYNLIB_NOLOAD) {
        posix_flags |= RTLD_NOLOAD;
    }
#endif

#ifdef RTLD_DEEPBIND
    if (flags & ET_DYNLIB_DEEPBIND) {
        posix_flags |= RTLD_DEEPBIND;
    }
#endif

    return posix_flags;
}

/**
 * @brief 라이브러리 경로 해석
 */
static ETResult resolve_library_path(const char* name, char* resolved_path, size_t size) {
    if (!name || !resolved_path || size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 절대 경로인 경우 그대로 사용
    if (name[0] == '/') {
        strncpy(resolved_path, name, size - 1);
        resolved_path[size - 1] = '\0';
        return ET_SUCCESS;
    }

    // 상대 경로에 ./ 또는 ../ 포함된 경우
    if (strstr(name, "./")) {
        strncpy(resolved_path, name, size - 1);
        resolved_path[size - 1] = '\0';
        return ET_SUCCESS;
    }

    // 확장자 확인 및 추가
    char full_name[PATH_MAX];
    const char* extension = NULL;

#ifdef __APPLE__
    extension = ".dylib";
#else
    extension = ".so";
#endif

    if (!strstr(name, extension)) {
        // lib 접두사가 없으면 추가
        if (strncmp(name, "lib", 3) != 0) {
            snprintf(full_name, sizeof(full_name), "lib%s%s", name, extension);
        } else {
            snprintf(full_name, sizeof(full_name), "%s%s", name, extension);
        }
    } else {
        strncpy(full_name, name, sizeof(full_name) - 1);
        full_name[sizeof(full_name) - 1] = '\0';
    }

    // 현재 디렉토리에서 먼저 검색
    struct stat st;
    if (stat(full_name, &st) == 0) {
        strncpy(resolved_path, full_name, size - 1);
        resolved_path[size - 1] = '\0';
        return ET_SUCCESS;
    }

    // 검색 경로에서 찾기
    for (int i = 0; i < g_posix_data.search_path_count; i++) {
        char test_path[PATH_MAX];
        snprintf(test_path, sizeof(test_path), "%s/%s",
                g_posix_data.search_paths[i], full_name);

        if (stat(test_path, &st) == 0) {
            strncpy(resolved_path, test_path, size - 1);
            resolved_path[size - 1] = '\0';
            return ET_SUCCESS;
        }
    }

    // 시스템 경로에서 검색 (dlopen이 자동으로 처리)
    strncpy(resolved_path, full_name, size - 1);
    resolved_path[size - 1] = '\0';
    return ET_SUCCESS;
}

/**
 * @brief 오류 정보 업데이트
 */
static void update_error_info(void) {
    g_posix_data.last_errno = errno;

    const char* dlerror_msg = dlerror();
    if (dlerror_msg) {
        strncpy(g_posix_data.last_error_message, dlerror_msg,
                sizeof(g_posix_data.last_error_message) - 1);
        g_posix_data.last_error_message[sizeof(g_posix_data.last_error_message) - 1] = '\0';
    } else if (g_posix_data.last_errno != 0) {
        strncpy(g_posix_data.last_error_message, strerror(g_posix_data.last_errno),
                sizeof(g_posix_data.last_error_message) - 1);
        g_posix_data.last_error_message[sizeof(g_posix_data.last_error_message) - 1] = '\0';
    } else {
        strcpy(g_posix_data.last_error_message, "알 수 없는 오류");
    }
}

/**
 * @brief 라이브러리 정보 수집
 */
static ETResult get_library_info(void* handle, const char* path, ETDynlibInfo* info) {
    if (!handle || !path || !info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(info, 0, sizeof(ETDynlibInfo));

    // 경로 설정
    strncpy(info->path, path, sizeof(info->path) - 1);
    info->path[sizeof(info->path) - 1] = '\0';

    // 파일 이름 추출
    const char* filename = strrchr(path, '/');
    if (filename) {
        strncpy(info->name, filename + 1, sizeof(info->name) - 1);
    } else {
        strncpy(info->name, path, sizeof(info->name) - 1);
    }
    info->name[sizeof(info->name) - 1] = '\0';

    // 파일 크기 가져오기
    struct stat st;
    if (stat(path, &st) == 0) {
        info->size = (uint64_t)st.st_size;
    }

    info->is_loaded = true;
    info->platform_handle = handle;

    return ET_SUCCESS;
}

// ============================================================================
// 인터페이스 구현
// ============================================================================

/**
 * @brief 라이브러리 로드
 */
static ETResult posix_load_library(const char* path, uint32_t flags, ETDynamicLibrary** lib) {
    if (!path || !lib) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 이전 오류 초기화
    dlerror();

    // 경로 해석
    char resolved_path[PATH_MAX];
    ETResult result = resolve_library_path(path, resolved_path, sizeof(resolved_path));
    if (result != ET_SUCCESS) {
        return result;
    }

    // POSIX 플래그 변환
    int load_flags = convert_flags_to_posix(flags);

    // 라이브러리 로드
    void* handle = dlopen(resolved_path, load_flags);
    if (!handle) {
        update_error_info();
        return ET_ERROR_FILE_NOT_FOUND;
    }

    // 구조체 생성
    ETDynamicLibraryPosix* posix_lib = malloc(sizeof(ETDynamicLibraryPosix));
    if (!posix_lib) {
        dlclose(handle);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(posix_lib, 0, sizeof(ETDynamicLibraryPosix));
    posix_lib->handle = handle;
    posix_lib->ref_count = 1;
    posix_lib->is_loaded = true;
    posix_lib->load_flags = load_flags;

    strncpy(posix_lib->path, resolved_path, sizeof(posix_lib->path) - 1);
    posix_lib->path[sizeof(posix_lib->path) - 1] = '\0';

    // 파일 이름 추출
    const char* filename = strrchr(resolved_path, '/');
    if (filename) {
        strncpy(posix_lib->name, filename + 1, sizeof(posix_lib->name) - 1);
    } else {
        strncpy(posix_lib->name, resolved_path, sizeof(posix_lib->name) - 1);
    }
    posix_lib->name[sizeof(posix_lib->name) - 1] = '\0';

    *lib = (ETDynamicLibrary*)posix_lib;
    return ET_SUCCESS;
}

/**
 * @brief 메모리에서 라이브러리 로드 (POSIX 미지원)
 */
static ETResult posix_load_library_from_memory(const void* data, size_t size, ETDynamicLibrary** lib) {
    // POSIX에서는 기본적으로 메모리에서 라이브러리 로드를 지원하지 않음
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 라이브러리 언로드
 */
static void posix_unload_library(ETDynamicLibrary* lib) {
    if (!lib) {
        return;
    }

    ETDynamicLibraryPosix* posix_lib = (ETDynamicLibraryPosix*)lib;

    if (posix_lib->ref_count > 0) {
        posix_lib->ref_count--;
    }

    if (posix_lib->ref_count == 0 && posix_lib->handle) {
        dlclose(posix_lib->handle);
        posix_lib->handle = NULL;
        posix_lib->is_loaded = false;
    }

    if (posix_lib->ref_count == 0) {
        free(posix_lib);
    }
}

/**
 * @brief 심볼 가져오기
 */
static ETResult posix_get_symbol(ETDynamicLibrary* lib, const char* symbol_name, void** symbol) {
    if (!lib || !symbol_name || !symbol) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynamicLibraryPosix* posix_lib = (ETDynamicLibraryPosix*)lib;
    if (!posix_lib->handle) {
        return ET_ERROR_INVALID_HANDLE;
    }

    // 이전 오류 초기화
    dlerror();

    void* sym = dlsym(posix_lib->handle, symbol_name);
    if (!sym) {
        const char* error = dlerror();
        if (error) {
            update_error_info();
            return ET_ERROR_SYMBOL_NOT_FOUND;
        }
    }

    *symbol = sym;
    return ET_SUCCESS;
}

/**
 * @brief 심볼 정보 가져오기
 */
static ETResult posix_get_symbol_info(ETDynamicLibrary* lib, const char* symbol_name, ETSymbolInfo* info) {
    if (!lib || !symbol_name || !info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    void* symbol;
    ETResult result = posix_get_symbol(lib, symbol_name, &symbol);
    if (result != ET_SUCCESS) {
        return result;
    }

    memset(info, 0, sizeof(ETSymbolInfo));
    strncpy(info->name, symbol_name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->address = symbol;
    info->is_function = true; // POSIX에서는 구분하기 어려움
    info->is_exported = true;

    return ET_SUCCESS;
}

/**
 * @brief 심볼 열거 (POSIX에서는 제한적 지원)
 */
static ETResult posix_enumerate_symbols(ETDynamicLibrary* lib, ETSymbolInfo* symbols, int* count) {
    // POSIX에서는 심볼 열거가 복잡함 (ELF/Mach-O 파일 파싱 필요)
    // 기본 구현에서는 지원하지 않음
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 라이브러리 정보 가져오기
 */
static ETResult posix_get_library_info(ETDynamicLibrary* lib, ETDynlibInfo* info) {
    if (!lib || !info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynamicLibraryPosix* posix_lib = (ETDynamicLibraryPosix*)lib;
    if (!posix_lib->handle) {
        return ET_ERROR_INVALID_HANDLE;
    }

    return get_library_info(posix_lib->handle, posix_lib->path, info);
}

/**
 * @brief 라이브러리 경로 가져오기
 */
static ETResult posix_get_library_path(ETDynamicLibrary* lib, char* path, size_t path_size) {
    if (!lib || !path || path_size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynamicLibraryPosix* posix_lib = (ETDynamicLibraryPosix*)lib;

    strncpy(path, posix_lib->path, path_size - 1);
    path[path_size - 1] = '\0';

    return ET_SUCCESS;
}

/**
 * @brief 라이브러리 로드 상태 확인
 */
static bool posix_is_library_loaded(const char* path) {
    if (!path) {
        return false;
    }

    char resolved_path[PATH_MAX];
    if (resolve_library_path(path, resolved_path, sizeof(resolved_path)) != ET_SUCCESS) {
        return false;
    }

    // RTLD_NOLOAD 플래그로 이미 로드된 라이브러리만 확인
#ifdef RTLD_NOLOAD
    void* handle = dlopen(resolved_path, RTLD_NOLOAD | RTLD_LAZY);
    if (handle) {
        dlclose(handle);
        return true;
    }
#endif

    return false;
}

/**
 * @brief 의존성 가져오기 (기본 구현)
 */
static ETResult posix_get_dependencies(ETDynamicLibrary* lib, ETDependencyInfo* deps, int* count) {
    // ELF/Mach-O 파일 파싱이 필요한 복잡한 작업
    // 기본 구현에서는 지원하지 않음
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 의존성 해석
 */
static ETResult posix_resolve_dependencies(ETDynamicLibrary* lib) {
    // POSIX는 로드 시 자동으로 의존성 해석
    return ET_SUCCESS;
}

/**
 * @brief 의존성 확인
 */
static ETResult posix_check_dependencies(const char* path, ETDependencyInfo* missing_deps, int* count) {
    // ELF/Mach-O 파일 파싱이 필요한 복잡한 작업
    // 기본 구현에서는 지원하지 않음
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 마지막 오류 메시지 가져오기
 */
static const char* posix_get_last_error(void) {
    return g_posix_data.last_error_message;
}

/**
 * @brief 마지막 오류 코드 가져오기
 */
static ETResult posix_get_last_error_code(void) {
    return ET_ERROR_SYSTEM_ERROR; // 플랫폼별 오류 매핑 필요
}

/**
 * @brief 오류 초기화
 */
static void posix_clear_error(void) {
    g_posix_data.last_errno = 0;
    g_posix_data.last_error_message[0] = '\0';
    errno = 0;
    dlerror(); // dlerror 상태 초기화
}

// ============================================================================
// 인터페이스 생성
// ============================================================================

/**
 * @brief POSIX 동적 라이브러리 인터페이스 생성
 */
ETResult et_create_posix_dynlib_interface(ETDynlibInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynlibInterface* impl = malloc(sizeof(ETDynlibInterface));
    if (!impl) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(impl, 0, sizeof(ETDynlibInterface));

    // 함수 포인터 설정
    impl->load_library = posix_load_library;
    impl->load_library_from_memory = posix_load_library_from_memory;
    impl->unload_library = posix_unload_library;
    impl->get_symbol = posix_get_symbol;
    impl->get_symbol_info = posix_get_symbol_info;
    impl->enumerate_symbols = posix_enumerate_symbols;
    impl->get_library_info = posix_get_library_info;
    impl->get_library_path = posix_get_library_path;
    impl->is_library_loaded = posix_is_library_loaded;
    impl->get_dependencies = posix_get_dependencies;
    impl->resolve_dependencies = posix_resolve_dependencies;
    impl->check_dependencies = posix_check_dependencies;
    impl->get_last_error = posix_get_last_error;
    impl->get_last_error_code = posix_get_last_error_code;
    impl->clear_error = posix_clear_error;

    // 플랫폼 데이터 설정
    impl->platform_data = &g_posix_data;

    // 초기 검색 경로 설정
    g_posix_data.search_path_count = 0;

    // 기본 검색 경로 추가
    const char* default_paths[] = {
        "/usr/lib",
        "/usr/local/lib",
        "/lib",
#ifdef __APPLE__
        "/usr/lib/system",
        "/System/Library/Frameworks",
#endif
        NULL
    };

    for (int i = 0; default_paths[i] && g_posix_data.search_path_count < 16; i++) {
        strncpy(g_posix_data.search_paths[g_posix_data.search_path_count],
                default_paths[i], PATH_MAX - 1);
        g_posix_data.search_paths[g_posix_data.search_path_count][PATH_MAX - 1] = '\0';
        g_posix_data.search_path_count++;
    }

    *interface = impl;
    return ET_SUCCESS;
}

/**
 * @brief POSIX 동적 라이브러리 인터페이스 해제
 */
void et_destroy_posix_dynlib_interface(ETDynlibInterface* interface) {
    if (interface) {
        free(interface);
    }
}

#endif // !_WIN32