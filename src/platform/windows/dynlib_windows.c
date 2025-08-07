/**
 * @file dynlib_windows.c
 * @brief Windows 동적 라이브러리 구현체
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Windows LoadLibrary/GetProcAddress API를 사용한 동적 라이브러리 구현
 */

#ifdef _WIN32

#include "libetude/platform/dynlib.h"
#include "libetude/platform/common.h"
#include <windows.h>
#include <psapi.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Windows 전용 구조체 정의
// ============================================================================

/**
 * @brief Windows 동적 라이브러리 핸들 구조체
 */
typedef struct ETDynamicLibraryWindows {
    HMODULE handle;                 /**< Windows 모듈 핸들 */
    char path[MAX_PATH];           /**< 라이브러리 경로 */
    char name[256];                /**< 라이브러리 이름 */
    uint32_t ref_count;            /**< 참조 카운트 */
    bool is_loaded;                /**< 로드 상태 */
    DWORD load_flags;              /**< 로드 플래그 */
} ETDynamicLibraryWindows;

/**
 * @brief Windows 동적 라이브러리 플랫폼 데이터
 */
typedef struct {
    char search_paths[16][MAX_PATH]; /**< 검색 경로 배열 */
    int search_path_count;           /**< 검색 경로 개수 */
    DWORD last_error;                /**< 마지막 오류 코드 */
    char last_error_message[512];    /**< 마지막 오류 메시지 */
} ETDynlibWindowsData;

// ============================================================================
// 전역 변수
// ============================================================================

static ETDynlibWindowsData g_windows_data = {0};

// ============================================================================
// 내부 함수
// ============================================================================

/**
 * @brief ETDynlibFlags를 Windows 플래그로 변환
 */
static DWORD convert_flags_to_windows(uint32_t flags) {
    DWORD windows_flags = 0;

    // Windows는 RTLD_LAZY/NOW 구분이 없으므로 기본값 사용
    if (flags & ET_DYNLIB_LAZY) {
        windows_flags |= DONT_RESOLVE_DLL_REFERENCES;
    }

    // Windows는 RTLD_GLOBAL/LOCAL 구분이 없으므로 무시

    if (flags & ET_DYNLIB_NODELETE) {
        // Windows에서는 GET_MODULE_HANDLE_EX_FLAG_PIN 사용
        windows_flags |= LOAD_LIBRARY_AS_DATAFILE;
    }

    return windows_flags;
}

/**
 * @brief 라이브러리 경로 해석
 */
static ETResult resolve_library_path(const char* name, char* resolved_path, size_t size) {
    if (!name || !resolved_path || size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 절대 경로인 경우 그대로 사용
    if (strchr(name, '\\') || strchr(name, '/') || strchr(name, ':')) {
        strncpy(resolved_path, name, size - 1);
        resolved_path[size - 1] = '\0';
        return ET_SUCCESS;
    }

    // 확장자가 없으면 .dll 추가
    char full_name[MAX_PATH];
    if (!strstr(name, ".dll")) {
        snprintf(full_name, sizeof(full_name), "%s.dll", name);
    } else {
        strncpy(full_name, name, sizeof(full_name) - 1);
        full_name[sizeof(full_name) - 1] = '\0';
    }

    // 현재 디렉토리에서 먼저 검색
    if (GetFileAttributesA(full_name) != INVALID_FILE_ATTRIBUTES) {
        strncpy(resolved_path, full_name, size - 1);
        resolved_path[size - 1] = '\0';
        return ET_SUCCESS;
    }

    // 검색 경로에서 찾기
    for (int i = 0; i < g_windows_data.search_path_count; i++) {
        char test_path[MAX_PATH];
        snprintf(test_path, sizeof(test_path), "%s\\%s",
                g_windows_data.search_paths[i], full_name);

        if (GetFileAttributesA(test_path) != INVALID_FILE_ATTRIBUTES) {
            strncpy(resolved_path, test_path, size - 1);
            resolved_path[size - 1] = '\0';
            return ET_SUCCESS;
        }
    }

    // 시스템 경로에서 검색 (기본 동작)
    strncpy(resolved_path, full_name, size - 1);
    resolved_path[size - 1] = '\0';
    return ET_SUCCESS;
}

/**
 * @brief 오류 정보 업데이트
 */
static void update_error_info(void) {
    g_windows_data.last_error = GetLastError();

    DWORD result = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        g_windows_data.last_error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        g_windows_data.last_error_message,
        sizeof(g_windows_data.last_error_message) - 1,
        NULL
    );

    if (result == 0) {
        snprintf(g_windows_data.last_error_message,
                sizeof(g_windows_data.last_error_message),
                "Windows 오류 코드: %lu", g_windows_data.last_error);
    } else {
        // 개행 문자 제거
        char* newline = strchr(g_windows_data.last_error_message, '\r');
        if (newline) *newline = '\0';
        newline = strchr(g_windows_data.last_error_message, '\n');
        if (newline) *newline = '\0';
    }
}

/**
 * @brief 모듈 정보 수집
 */
static ETResult get_module_info(HMODULE handle, ETDynlibInfo* info) {
    if (!handle || !info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(info, 0, sizeof(ETDynlibInfo));

    // 모듈 경로 가져오기
    DWORD result = GetModuleFileNameA(handle, info->path, sizeof(info->path) - 1);
    if (result == 0) {
        update_error_info();
        return ET_ERROR_SYSTEM_ERROR;
    }

    // 파일 이름 추출
    char* filename = strrchr(info->path, '\\');
    if (filename) {
        strncpy(info->name, filename + 1, sizeof(info->name) - 1);
    } else {
        strncpy(info->name, info->path, sizeof(info->name) - 1);
    }
    info->name[sizeof(info->name) - 1] = '\0';

    // 파일 크기 가져오기
    HANDLE file = CreateFileA(info->path, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_size;
        if (GetFileSizeEx(file, &file_size)) {
            info->size = (uint64_t)file_size.QuadPart;
        }
        CloseHandle(file);
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
static ETResult windows_load_library(const char* path, uint32_t flags, ETDynamicLibrary** lib) {
    if (!path || !lib) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 경로 해석
    char resolved_path[MAX_PATH];
    ETResult result = resolve_library_path(path, resolved_path, sizeof(resolved_path));
    if (result != ET_SUCCESS) {
        return result;
    }

    // Windows 플래그 변환
    DWORD load_flags = convert_flags_to_windows(flags);

    // 라이브러리 로드
    HMODULE handle = LoadLibraryExA(resolved_path, NULL, load_flags);
    if (!handle) {
        update_error_info();
        return ET_ERROR_FILE_NOT_FOUND;
    }

    // 구조체 생성
    ETDynamicLibraryWindows* windows_lib = malloc(sizeof(ETDynamicLibraryWindows));
    if (!windows_lib) {
        FreeLibrary(handle);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(windows_lib, 0, sizeof(ETDynamicLibraryWindows));
    windows_lib->handle = handle;
    windows_lib->ref_count = 1;
    windows_lib->is_loaded = true;
    windows_lib->load_flags = load_flags;

    strncpy(windows_lib->path, resolved_path, sizeof(windows_lib->path) - 1);
    windows_lib->path[sizeof(windows_lib->path) - 1] = '\0';

    // 파일 이름 추출
    char* filename = strrchr(resolved_path, '\\');
    if (filename) {
        strncpy(windows_lib->name, filename + 1, sizeof(windows_lib->name) - 1);
    } else {
        strncpy(windows_lib->name, resolved_path, sizeof(windows_lib->name) - 1);
    }
    windows_lib->name[sizeof(windows_lib->name) - 1] = '\0';

    *lib = (ETDynamicLibrary*)windows_lib;
    return ET_SUCCESS;
}

/**
 * @brief 메모리에서 라이브러리 로드 (Windows 미지원)
 */
static ETResult windows_load_library_from_memory(const void* data, size_t size, ETDynamicLibrary** lib) {
    // Windows에서는 기본적으로 메모리에서 DLL 로드를 지원하지 않음
    // 서드파티 라이브러리(예: MemoryModule) 필요
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 라이브러리 언로드
 */
static void windows_unload_library(ETDynamicLibrary* lib) {
    if (!lib) {
        return;
    }

    ETDynamicLibraryWindows* windows_lib = (ETDynamicLibraryWindows*)lib;

    if (windows_lib->ref_count > 0) {
        windows_lib->ref_count--;
    }

    if (windows_lib->ref_count == 0 && windows_lib->handle) {
        FreeLibrary(windows_lib->handle);
        windows_lib->handle = NULL;
        windows_lib->is_loaded = false;
    }

    if (windows_lib->ref_count == 0) {
        free(windows_lib);
    }
}

/**
 * @brief 심볼 가져오기
 */
static ETResult windows_get_symbol(ETDynamicLibrary* lib, const char* symbol_name, void** symbol) {
    if (!lib || !symbol_name || !symbol) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynamicLibraryWindows* windows_lib = (ETDynamicLibraryWindows*)lib;
    if (!windows_lib->handle) {
        return ET_ERROR_INVALID_HANDLE;
    }

    FARPROC proc = GetProcAddress(windows_lib->handle, symbol_name);
    if (!proc) {
        update_error_info();
        return ET_ERROR_SYMBOL_NOT_FOUND;
    }

    *symbol = (void*)proc;
    return ET_SUCCESS;
}

/**
 * @brief 심볼 정보 가져오기
 */
static ETResult windows_get_symbol_info(ETDynamicLibrary* lib, const char* symbol_name, ETSymbolInfo* info) {
    if (!lib || !symbol_name || !info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    void* symbol;
    ETResult result = windows_get_symbol(lib, symbol_name, &symbol);
    if (result != ET_SUCCESS) {
        return result;
    }

    memset(info, 0, sizeof(ETSymbolInfo));
    strncpy(info->name, symbol_name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->address = symbol;
    info->is_function = true; // Windows에서는 구분하기 어려움
    info->is_exported = true;

    return ET_SUCCESS;
}

/**
 * @brief 심볼 열거 (Windows에서는 제한적 지원)
 */
static ETResult windows_enumerate_symbols(ETDynamicLibrary* lib, ETSymbolInfo* symbols, int* count) {
    // Windows에서는 심볼 열거가 복잡함 (PE 파일 파싱 필요)
    // 기본 구현에서는 지원하지 않음
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 라이브러리 정보 가져오기
 */
static ETResult windows_get_library_info(ETDynamicLibrary* lib, ETDynlibInfo* info) {
    if (!lib || !info) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynamicLibraryWindows* windows_lib = (ETDynamicLibraryWindows*)lib;
    if (!windows_lib->handle) {
        return ET_ERROR_INVALID_HANDLE;
    }

    return get_module_info(windows_lib->handle, info);
}

/**
 * @brief 라이브러리 경로 가져오기
 */
static ETResult windows_get_library_path(ETDynamicLibrary* lib, char* path, size_t path_size) {
    if (!lib || !path || path_size == 0) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynamicLibraryWindows* windows_lib = (ETDynamicLibraryWindows*)lib;

    strncpy(path, windows_lib->path, path_size - 1);
    path[path_size - 1] = '\0';

    return ET_SUCCESS;
}

/**
 * @brief 라이브러리 로드 상태 확인
 */
static bool windows_is_library_loaded(const char* path) {
    if (!path) {
        return false;
    }

    char resolved_path[MAX_PATH];
    if (resolve_library_path(path, resolved_path, sizeof(resolved_path)) != ET_SUCCESS) {
        return false;
    }

    HMODULE handle = GetModuleHandleA(resolved_path);
    return handle != NULL;
}

/**
 * @brief 의존성 가져오기 (기본 구현)
 */
static ETResult windows_get_dependencies(ETDynamicLibrary* lib, ETDependencyInfo* deps, int* count) {
    // PE 파일 파싱이 필요한 복잡한 작업
    // 기본 구현에서는 지원하지 않음
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 의존성 해석
 */
static ETResult windows_resolve_dependencies(ETDynamicLibrary* lib) {
    // Windows는 로드 시 자동으로 의존성 해석
    return ET_SUCCESS;
}

/**
 * @brief 의존성 확인
 */
static ETResult windows_check_dependencies(const char* path, ETDependencyInfo* missing_deps, int* count) {
    // PE 파일 파싱이 필요한 복잡한 작업
    // 기본 구현에서는 지원하지 않음
    return ET_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief 마지막 오류 메시지 가져오기
 */
static const char* windows_get_last_error(void) {
    return g_windows_data.last_error_message;
}

/**
 * @brief 마지막 오류 코드 가져오기
 */
static ETResult windows_get_last_error_code(void) {
    return ET_ERROR_SYSTEM_ERROR; // 플랫폼별 오류 매핑 필요
}

/**
 * @brief 오류 초기화
 */
static void windows_clear_error(void) {
    g_windows_data.last_error = 0;
    g_windows_data.last_error_message[0] = '\0';
    SetLastError(0);
}

// ============================================================================
// 인터페이스 생성
// ============================================================================

/**
 * @brief Windows 동적 라이브러리 인터페이스 생성
 */
ETResult et_create_windows_dynlib_interface(ETDynlibInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    ETDynlibInterface* impl = malloc(sizeof(ETDynlibInterface));
    if (!impl) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(impl, 0, sizeof(ETDynlibInterface));

    // 함수 포인터 설정
    impl->load_library = windows_load_library;
    impl->load_library_from_memory = windows_load_library_from_memory;
    impl->unload_library = windows_unload_library;
    impl->get_symbol = windows_get_symbol;
    impl->get_symbol_info = windows_get_symbol_info;
    impl->enumerate_symbols = windows_enumerate_symbols;
    impl->get_library_info = windows_get_library_info;
    impl->get_library_path = windows_get_library_path;
    impl->is_library_loaded = windows_is_library_loaded;
    impl->get_dependencies = windows_get_dependencies;
    impl->resolve_dependencies = windows_resolve_dependencies;
    impl->check_dependencies = windows_check_dependencies;
    impl->get_last_error = windows_get_last_error;
    impl->get_last_error_code = windows_get_last_error_code;
    impl->clear_error = windows_clear_error;

    // 플랫폼 데이터 설정
    impl->platform_data = &g_windows_data;

    // 초기 검색 경로 설정
    g_windows_data.search_path_count = 0;

    *interface = impl;
    return ET_SUCCESS;
}

/**
 * @brief Windows 동적 라이브러리 인터페이스 해제
 */
void et_destroy_windows_dynlib_interface(ETDynlibInterface* interface) {
    if (interface) {
        free(interface);
    }
}

#endif // _WIN32