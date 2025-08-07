/**
 * @file filesystem_common.c
 * @brief 파일시스템 추상화 공통 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼 독립적인 파일시스템 공통 기능을 구현합니다.
 */

#include "libetude/platform/filesystem.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef __linux__
#include <unistd.h>
#endif

// ============================================================================
// 내부 상수 및 매크로
// ============================================================================

#ifdef _WIN32
    #define PATH_SEPARATOR '\\'
    #define PATH_LIST_SEPARATOR ';'
    #define ALT_PATH_SEPARATOR '/'
#else
    #define PATH_SEPARATOR '/'
    #define PATH_LIST_SEPARATOR ':'
    #define ALT_PATH_SEPARATOR '\\'
#endif

// ============================================================================
// 내부 함수 선언
// ============================================================================

/**
 * @brief 경로에서 불필요한 구분자를 정리합니다
 * @param path 정리할 경로
 */
static void cleanup_path_separators(char* path);

/**
 * @brief 상대 경로 요소를 해석합니다 (., .. 처리)
 * @param path 해석할 경로
 */
static void resolve_relative_components(char* path);

/**
 * @brief 문자열이 절대 경로인지 확인합니다
 * @param path 확인할 경로
 * @return 절대 경로이면 true, 아니면 false
 */
static bool is_absolute_path(const char* path);

/**
 * @brief 파일 모드 플래그를 플랫폼별 모드 문자열로 변환합니다
 * @param mode 파일 모드 플래그
 * @param mode_str 모드 문자열을 저장할 버퍼
 * @param size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult convert_file_mode(uint32_t mode, char* mode_str, size_t size);

// ============================================================================
// 공통 편의 함수 구현
// ============================================================================

char et_get_path_separator(void) {
    return PATH_SEPARATOR;
}

char et_get_path_list_separator(void) {
    return PATH_LIST_SEPARATOR;
}

ETResult et_get_temp_directory(char* path, size_t size) {
    ET_CHECK_NULL(path, "경로 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    // Windows: TEMP 또는 TMP 환경변수 사용
    const char* temp_env = getenv("TEMP");
    if (!temp_env) {
        temp_env = getenv("TMP");
    }
    if (!temp_env) {
        temp_env = "C:\\Windows\\Temp";
    }
#else
    // Unix: TMPDIR 환경변수 또는 /tmp 사용
    const char* temp_env = getenv("TMPDIR");
    if (!temp_env) {
        temp_env = "/tmp";
    }
#endif

    size_t len = strlen(temp_env);
    if (len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(path, temp_env);
    return ET_SUCCESS;
}

ETResult et_get_home_directory(char* path, size_t size) {
    ET_CHECK_NULL(path, "경로 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    // Windows: USERPROFILE 환경변수 사용
    const char* home_env = getenv("USERPROFILE");
    if (!home_env) {
        home_env = getenv("HOMEDRIVE");
        const char* homepath = getenv("HOMEPATH");
        if (home_env && homepath) {
            snprintf(path, size, "%s%s", home_env, homepath);
            return ET_SUCCESS;
        }
        home_env = "C:\\Users\\Default";
    }
#else
    // Unix: HOME 환경변수 사용
    const char* home_env = getenv("HOME");
    if (!home_env) {
        home_env = "/";
    }
#endif

    size_t len = strlen(home_env);
    if (len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(path, home_env);
    return ET_SUCCESS;
}

ETResult et_get_executable_path(char* path, size_t size) {
    ET_CHECK_NULL(path, "경로 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    // Windows: GetModuleFileName 사용 (플랫폼별 구현에서 처리)
    ET_SET_ERROR(ET_ERROR_NOT_IMPLEMENTED, "플랫폼별 구현이 필요합니다");
    return ET_ERROR_NOT_IMPLEMENTED;
#elif defined(__linux__)
    // Linux: /proc/self/exe 심볼릭 링크 읽기
    ssize_t len = readlink("/proc/self/exe", path, size - 1);
    if (len == -1) {
        ET_SET_ERROR(ET_ERROR_SYSTEM, "실행 파일 경로를 읽을 수 없습니다");
        return ET_ERROR_SYSTEM;
    }
    path[len] = '\0';
    return ET_SUCCESS;
#elif defined(__APPLE__)
    // macOS: _NSGetExecutablePath 사용 (플랫폼별 구현에서 처리)
    ET_SET_ERROR(ET_ERROR_NOT_IMPLEMENTED, "플랫폼별 구현이 필요합니다");
    return ET_ERROR_NOT_IMPLEMENTED;
#else
    ET_SET_ERROR(ET_ERROR_UNSUPPORTED, "지원되지 않는 플랫폼입니다");
    return ET_ERROR_UNSUPPORTED;
#endif
}

// ============================================================================
// 경로 처리 공통 함수 구현
// ============================================================================

ETResult et_normalize_path_common(const char* path, char* normalized, size_t size) {
    ET_CHECK_NULL(path, "입력 경로가 NULL입니다");
    ET_CHECK_NULL(normalized, "출력 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t path_len = strlen(path);
    if (path_len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }

    // 경로 복사
    strcpy(normalized, path);

    // 경로 구분자 정리
    cleanup_path_separators(normalized);

    // 상대 경로 요소 해석
    resolve_relative_components(normalized);

    return ET_SUCCESS;
}

ETResult et_join_path_common(const char* base, const char* relative, char* result, size_t size) {
    ET_CHECK_NULL(base, "기본 경로가 NULL입니다");
    ET_CHECK_NULL(relative, "상대 경로가 NULL입니다");
    ET_CHECK_NULL(result, "결과 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 상대 경로가 절대 경로인 경우 그대로 사용
    if (is_absolute_path(relative)) {
        size_t len = strlen(relative);
        if (len >= size) {
            ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
            return ET_ERROR_BUFFER_FULL;
        }
        strcpy(result, relative);
        return et_normalize_path_common(result, result, size);
    }

    // 기본 경로 복사
    size_t base_len = strlen(base);
    if (base_len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }
    strcpy(result, base);

    // 경로 구분자 추가 (필요한 경우)
    if (base_len > 0 && result[base_len - 1] != PATH_SEPARATOR && result[base_len - 1] != ALT_PATH_SEPARATOR) {
        if (base_len + 1 >= size) {
            ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
            return ET_ERROR_BUFFER_FULL;
        }
        result[base_len] = PATH_SEPARATOR;
        result[base_len + 1] = '\0';
        base_len++;
    }

    // 상대 경로 추가
    size_t relative_len = strlen(relative);
    if (base_len + relative_len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }
    strcat(result, relative);

    // 경로 정규화
    return et_normalize_path_common(result, result, size);
}

ETResult et_get_dirname_common(const char* path, char* dirname, size_t size) {
    ET_CHECK_NULL(path, "입력 경로가 NULL입니다");
    ET_CHECK_NULL(dirname, "출력 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t path_len = strlen(path);
    if (path_len == 0) {
        if (size < 2) {
            ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
            return ET_ERROR_BUFFER_FULL;
        }
        strcpy(dirname, ".");
        return ET_SUCCESS;
    }

    // 마지막 경로 구분자 찾기
    const char* last_sep = NULL;
    for (const char* p = path + path_len - 1; p >= path; p--) {
        if (*p == PATH_SEPARATOR || *p == ALT_PATH_SEPARATOR) {
            last_sep = p;
            break;
        }
    }

    if (!last_sep) {
        // 경로 구분자가 없으면 현재 디렉토리
        if (size < 2) {
            ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
            return ET_ERROR_BUFFER_FULL;
        }
        strcpy(dirname, ".");
        return ET_SUCCESS;
    }

    // 디렉토리 부분 복사
    size_t dir_len = last_sep - path;
    if (dir_len == 0) {
        dir_len = 1; // 루트 디렉토리
    }

    if (dir_len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }

    strncpy(dirname, path, dir_len);
    dirname[dir_len] = '\0';

    return ET_SUCCESS;
}

ETResult et_get_basename_common(const char* path, char* basename, size_t size) {
    ET_CHECK_NULL(path, "입력 경로가 NULL입니다");
    ET_CHECK_NULL(basename, "출력 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t path_len = strlen(path);
    if (path_len == 0) {
        if (size < 2) {
            ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
            return ET_ERROR_BUFFER_FULL;
        }
        strcpy(basename, ".");
        return ET_SUCCESS;
    }

    // 마지막 경로 구분자 찾기
    const char* last_sep = NULL;
    for (const char* p = path + path_len - 1; p >= path; p--) {
        if (*p == PATH_SEPARATOR || *p == ALT_PATH_SEPARATOR) {
            last_sep = p;
            break;
        }
    }

    const char* name_start = last_sep ? last_sep + 1 : path;
    size_t name_len = strlen(name_start);

    if (name_len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(basename, name_start);
    return ET_SUCCESS;
}

ETResult et_get_extension_common(const char* path, char* extension, size_t size) {
    ET_CHECK_NULL(path, "입력 경로가 NULL입니다");
    ET_CHECK_NULL(extension, "출력 버퍼가 NULL입니다");

    if (size == 0) {
        ET_SET_ERROR(ET_ERROR_INVALID_ARGUMENT, "버퍼 크기가 0입니다");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 먼저 파일 이름 부분만 추출
    char basename[ET_MAX_FILENAME_LENGTH];
    ETResult result = et_get_basename_common(path, basename, sizeof(basename));
    if (result != ET_SUCCESS) {
        return result;
    }

    // 마지막 점 찾기
    const char* last_dot = strrchr(basename, '.');
    if (!last_dot || last_dot == basename) {
        // 확장자가 없거나 숨김 파일
        if (size < 1) {
            ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
            return ET_ERROR_BUFFER_FULL;
        }
        extension[0] = '\0';
        return ET_SUCCESS;
    }

    size_t ext_len = strlen(last_dot);
    if (ext_len >= size) {
        ET_SET_ERROR(ET_ERROR_BUFFER_FULL, "버퍼가 너무 작습니다");
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(extension, last_dot);
    return ET_SUCCESS;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static void cleanup_path_separators(char* path) {
    if (!path || !*path) return;

    char* write_pos = path;
    char* read_pos = path;
    char prev_char = '\0';

    while (*read_pos) {
        char current = *read_pos;

        // 경로 구분자 통일
        if (current == ALT_PATH_SEPARATOR) {
            current = PATH_SEPARATOR;
        }

        // 연속된 구분자 제거
        if (current == PATH_SEPARATOR && prev_char == PATH_SEPARATOR) {
            read_pos++;
            continue;
        }

        *write_pos = current;
        prev_char = current;
        write_pos++;
        read_pos++;
    }

    // 마지막 구분자 제거 (루트 디렉토리가 아닌 경우)
    if (write_pos > path + 1 && *(write_pos - 1) == PATH_SEPARATOR) {
        write_pos--;
    }

    *write_pos = '\0';
}

static void resolve_relative_components(char* path) {
    if (!path || !*path) return;

    char* components[256]; // 최대 256개 경로 구성요소
    int component_count = 0;

    // 경로를 구성요소로 분할
    char* token = strtok(path, "/\\");
    while (token && component_count < 256) {
        if (strcmp(token, ".") == 0) {
            // 현재 디렉토리는 무시
            token = strtok(NULL, "/\\");
            continue;
        } else if (strcmp(token, "..") == 0) {
            // 상위 디렉토리로 이동
            if (component_count > 0) {
                component_count--;
            }
        } else {
            // 일반 구성요소 추가
            components[component_count++] = token;
        }
        token = strtok(NULL, "/\\");
    }

    // 경로 재구성
    path[0] = '\0';
    for (int i = 0; i < component_count; i++) {
        if (i > 0) {
            strcat(path, PATH_SEPARATOR == '/' ? "/" : "\\");
        }
        strcat(path, components[i]);
    }

    // 빈 경로인 경우 현재 디렉토리로 설정
    if (path[0] == '\0') {
        strcpy(path, ".");
    }
}

static bool is_absolute_path(const char* path) {
    if (!path || !*path) return false;

#ifdef _WIN32
    // Windows: C:\ 형태 또는 UNC 경로 \\server\share
    if (strlen(path) >= 3 && isalpha(path[0]) && path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    if (strlen(path) >= 2 && path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    return false;
#else
    // Unix: / 로 시작
    return path[0] == '/';
#endif
}

static ETResult convert_file_mode(uint32_t mode, char* mode_str, size_t size) {
    if (!mode_str || size < 4) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    mode_str[0] = '\0';

    // 읽기/쓰기 모드 결정
    if ((mode & ET_FILE_MODE_READ) && (mode & ET_FILE_MODE_WRITE)) {
        if (mode & ET_FILE_MODE_APPEND) {
            strcat(mode_str, "a+");
        } else if (mode & ET_FILE_MODE_TRUNCATE) {
            strcat(mode_str, "w+");
        } else {
            strcat(mode_str, "r+");
        }
    } else if (mode & ET_FILE_MODE_WRITE) {
        if (mode & ET_FILE_MODE_APPEND) {
            strcat(mode_str, "a");
        } else {
            strcat(mode_str, "w");
        }
    } else {
        strcat(mode_str, "r");
    }

    // 바이너리/텍스트 모드
    if (mode & ET_FILE_MODE_BINARY) {
        strcat(mode_str, "b");
    }

    return ET_SUCCESS;
}

// ============================================================================
// 팩토리 인터페이스 구현은 factory.c에서 처리됨
// ============================================================================

// 공통 유틸리티 함수들은 위에서 구현됨