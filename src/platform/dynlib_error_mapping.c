/**
 * @file dynlib_error_mapping.c
 * @brief 동적 라이브러리 오류 매핑 시스템 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 동적 라이브러리 오류 코드를 공통 오류 코드로 매핑합니다.
 */

#include "libetude/platform/dynlib.h"
#include "libetude/platform/common.h"
#include <string.h>

// ============================================================================
// Windows 오류 매핑
// ============================================================================

#ifdef _WIN32
#include <windows.h>

static const ETErrorMapping windows_dynlib_error_mappings[] = {
    { ERROR_FILE_NOT_FOUND, ET_ERROR_FILE_NOT_FOUND, "라이브러리 파일을 찾을 수 없습니다" },
    { ERROR_PATH_NOT_FOUND, ET_ERROR_FILE_NOT_FOUND, "라이브러리 경로를 찾을 수 없습니다" },
    { ERROR_ACCESS_DENIED, ET_ERROR_ACCESS_DENIED, "라이브러리 접근이 거부되었습니다" },
    { ERROR_NOT_ENOUGH_MEMORY, ET_ERROR_OUT_OF_MEMORY, "메모리가 부족합니다" },
    { ERROR_OUTOFMEMORY, ET_ERROR_OUT_OF_MEMORY, "메모리가 부족합니다" },
    { ERROR_BAD_EXE_FORMAT, ET_ERROR_INVALID_FORMAT, "잘못된 실행 파일 형식입니다" },
    { ERROR_INVALID_DATA, ET_ERROR_INVALID_FORMAT, "잘못된 데이터 형식입니다" },
    { ERROR_MOD_NOT_FOUND, ET_ERROR_DEPENDENCY_NOT_FOUND, "의존성 모듈을 찾을 수 없습니다" },
    { ERROR_PROC_NOT_FOUND, ET_ERROR_SYMBOL_NOT_FOUND, "함수를 찾을 수 없습니다" },
    { ERROR_DLL_INIT_FAILED, ET_ERROR_INITIALIZATION_FAILED, "DLL 초기화에 실패했습니다" },
    { ERROR_INVALID_HANDLE, ET_ERROR_INVALID_HANDLE, "잘못된 핸들입니다" },
    { ERROR_SHARING_VIOLATION, ET_ERROR_RESOURCE_BUSY, "파일이 다른 프로세스에서 사용 중입니다" },
    { 0, ET_SUCCESS, NULL } // 종료 마커
};

/**
 * @brief Windows 동적 라이브러리 오류를 공통 오류로 변환
 */
ETResult et_dynlib_windows_error_to_common(DWORD windows_error) {
    for (int i = 0; windows_dynlib_error_mappings[i].description != NULL; i++) {
        if (windows_dynlib_error_mappings[i].platform_error == (int)windows_error) {
            return windows_dynlib_error_mappings[i].common_error;
        }
    }
    return ET_ERROR_SYSTEM_ERROR; // 매핑되지 않은 오류
}

/**
 * @brief Windows 동적 라이브러리 오류 설명 가져오기
 */
const char* et_dynlib_windows_error_description(DWORD windows_error) {
    for (int i = 0; windows_dynlib_error_mappings[i].description != NULL; i++) {
        if (windows_dynlib_error_mappings[i].platform_error == (int)windows_error) {
            return windows_dynlib_error_mappings[i].description;
        }
    }

    // Windows 시스템 오류 메시지 가져오기
    static char error_buffer[512];
    DWORD result = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        windows_error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        error_buffer,
        sizeof(error_buffer) - 1,
        NULL
    );

    if (result > 0) {
        // 개행 문자 제거
        char* newline = strchr(error_buffer, '\r');
        if (newline) *newline = '\0';
        newline = strchr(error_buffer, '\n');
        if (newline) *newline = '\0';

        return error_buffer;
    }

    return "알 수 없는 Windows 오류";
}

#endif // _WIN32

// ============================================================================
// POSIX 오류 매핑 (Linux/macOS 공통)
// ============================================================================

#ifndef _WIN32
#include <errno.h>
#include <dlfcn.h>

static const ETErrorMapping posix_dynlib_error_mappings[] = {
    { ENOENT, ET_ERROR_FILE_NOT_FOUND, "라이브러리 파일을 찾을 수 없습니다" },
    { EACCES, ET_ERROR_ACCESS_DENIED, "라이브러리 접근이 거부되었습니다" },
    { ENOMEM, ET_ERROR_OUT_OF_MEMORY, "메모리가 부족합니다" },
    { EINVAL, ET_ERROR_INVALID_PARAMETER, "잘못된 매개변수입니다" },
    { ENOEXEC, ET_ERROR_INVALID_FORMAT, "실행할 수 없는 파일 형식입니다" },
    { ELIBBAD, ET_ERROR_INVALID_FORMAT, "손상된 라이브러리 파일입니다" },
    { ELIBACC, ET_ERROR_ACCESS_DENIED, "라이브러리 접근 권한이 없습니다" },
    { ELIBMAX, ET_ERROR_RESOURCE_LIMIT, "라이브러리 개수 제한에 도달했습니다" },
    { ELIBSCN, ET_ERROR_INVALID_FORMAT, "라이브러리 섹션이 손상되었습니다" },
    { ELIBEXEC, ET_ERROR_EXECUTION_FAILED, "라이브러리 실행에 실패했습니다" },
    { 0, ET_SUCCESS, NULL } // 종료 마커
};

/**
 * @brief POSIX 동적 라이브러리 오류를 공통 오류로 변환
 */
ETResult et_dynlib_posix_error_to_common(int posix_error) {
    for (int i = 0; posix_dynlib_error_mappings[i].description != NULL; i++) {
        if (posix_dynlib_error_mappings[i].platform_error == posix_error) {
            return posix_dynlib_error_mappings[i].common_error;
        }
    }
    return ET_ERROR_SYSTEM_ERROR; // 매핑되지 않은 오류
}

/**
 * @brief POSIX 동적 라이브러리 오류 설명 가져오기
 */
const char* et_dynlib_posix_error_description(int posix_error) {
    for (int i = 0; posix_dynlib_error_mappings[i].description != NULL; i++) {
        if (posix_dynlib_error_mappings[i].platform_error == posix_error) {
            return posix_dynlib_error_mappings[i].description;
        }
    }

    // dlopen/dlsym 오류 메시지 확인
    const char* dlerror_msg = dlerror();
    if (dlerror_msg) {
        return dlerror_msg;
    }

    // 시스템 오류 메시지
    return strerror(posix_error);
}

#endif // !_WIN32

// ============================================================================
// 공통 오류 매핑 함수
// ============================================================================

/**
 * @brief 플랫폼별 동적 라이브러리 오류를 공통 오류로 변환
 */
ETResult et_dynlib_platform_error_to_common(int platform_error) {
#ifdef _WIN32
    return et_dynlib_windows_error_to_common((DWORD)platform_error);
#else
    return et_dynlib_posix_error_to_common(platform_error);
#endif
}

/**
 * @brief 플랫폼별 동적 라이브러리 오류 설명 가져오기
 */
const char* et_dynlib_get_platform_error_description(int platform_error) {
#ifdef _WIN32
    return et_dynlib_windows_error_description((DWORD)platform_error);
#else
    return et_dynlib_posix_error_description(platform_error);
#endif
}

/**
 * @brief 현재 플랫폼의 마지막 동적 라이브러리 오류 가져오기
 */
int et_dynlib_get_last_platform_error(void) {
#ifdef _WIN32
    return (int)GetLastError();
#else
    return errno;
#endif
}

/**
 * @brief 동적 라이브러리 오류 초기화
 */
void et_dynlib_clear_platform_error(void) {
#ifdef _WIN32
    SetLastError(0);
#else
    errno = 0;
    dlerror(); // dlerror 상태 초기화
#endif
}

// ============================================================================
// 오류 컨텍스트 관리
// ============================================================================

static __thread ETDetailedError g_last_dynlib_error = {0};
static __thread bool g_dynlib_error_set = false;

/**
 * @brief 상세 동적 라이브러리 오류 정보 설정
 */
void et_dynlib_set_detailed_error(ETResult code, int platform_code,
                                  const char* message, const char* file,
                                  int line, const char* function) {
    g_last_dynlib_error.code = code;
    g_last_dynlib_error.platform_code = platform_code;
    g_last_dynlib_error.platform = et_get_current_platform();
    g_last_dynlib_error.timestamp = 0; // TODO: 고해상도 타이머 사용
    g_last_dynlib_error.file = file;
    g_last_dynlib_error.line = line;
    g_last_dynlib_error.function = function;

    if (message) {
        strncpy(g_last_dynlib_error.message, message, sizeof(g_last_dynlib_error.message) - 1);
        g_last_dynlib_error.message[sizeof(g_last_dynlib_error.message) - 1] = '\0';
    } else {
        g_last_dynlib_error.message[0] = '\0';
    }

    g_dynlib_error_set = true;
}

/**
 * @brief 마지막 상세 동적 라이브러리 오류 정보 가져오기
 */
const ETDetailedError* et_dynlib_get_detailed_error(void) {
    if (!g_dynlib_error_set) {
        return NULL;
    }
    return &g_last_dynlib_error;
}

/**
 * @brief 동적 라이브러리 오류 정보 초기화
 */
void et_dynlib_clear_detailed_error(void) {
    memset(&g_last_dynlib_error, 0, sizeof(g_last_dynlib_error));
    g_dynlib_error_set = false;
}

// ============================================================================
// 오류 처리 매크로 지원
// ============================================================================

/**
 * @brief 동적 라이브러리 오류 설정 매크로
 */
#define ET_DYNLIB_SET_ERROR(code, msg, ...) \
    do { \
        char error_msg[256]; \
        snprintf(error_msg, sizeof(error_msg), msg, ##__VA_ARGS__); \
        et_dynlib_set_detailed_error(code, et_dynlib_get_last_platform_error(), \
                                     error_msg, __FILE__, __LINE__, __func__); \
    } while(0)

/**
 * @brief 플랫폼 오류를 공통 오류로 변환하고 설정
 */
#define ET_DYNLIB_SET_PLATFORM_ERROR(msg, ...) \
    do { \
        int platform_error = et_dynlib_get_last_platform_error(); \
        ETResult common_error = et_dynlib_platform_error_to_common(platform_error); \
        char error_msg[256]; \
        snprintf(error_msg, sizeof(error_msg), msg, ##__VA_ARGS__); \
        et_dynlib_set_detailed_error(common_error, platform_error, \
                                     error_msg, __FILE__, __LINE__, __func__); \
    } while(0)