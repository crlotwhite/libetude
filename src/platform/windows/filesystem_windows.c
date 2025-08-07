/**
 * @file filesystem_windows.c
 * @brief Windows 파일시스템 구현체
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Windows File API를 사용한 파일시스템 추상화 구현입니다.
 * 백슬래시 경로 처리, Windows 파일 속성 및 권한 관리를 지원합니다.
 */

#ifdef _WIN32

#include "libetude/platform/filesystem.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <windows.h>
#include <shlwapi.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// 링크 라이브러리
#pragma comment(lib, "shlwapi.lib")

// ============================================================================
// Windows 전용 구조체 및 상수
// ============================================================================

/**
 * @brief Windows 파일 핸들 구조체
 */
typedef struct {
    HANDLE handle;                      /**< Windows 파일 핸들 */
    char path[MAX_PATH];                /**< 파일 경로 */
    uint32_t mode;                      /**< 파일 모드 */
    bool is_open;                       /**< 열림 상태 */
} ETWindowsFile;

/**
 * @brief Windows 파일시스템 플랫폼 데이터
 */
typedef struct {
    char current_directory[MAX_PATH];   /**< 현재 작업 디렉토리 */
    DWORD last_error;                   /**< 마지막 Windows 오류 코드 */
} ETWindowsFilesystemData;

// ============================================================================
// 내부 함수 선언
// ============================================================================

/**
 * @brief Windows 오류 코드를 공통 오류 코드로 변환합니다
 * @param win_error Windows 오류 코드
 * @return 공통 오류 코드
 */
static ETResult windows_error_to_et_result(DWORD win_error);

/**
 * @brief FILETIME을 time_t로 변환합니다
 * @param ft FILETIME 구조체
 * @return time_t 값
 */
static time_t filetime_to_time_t(const FILETIME* ft);

/**
 * @brief time_t를 FILETIME으로 변환합니다
 * @param t time_t 값
 * @param ft FILETIME 구조체 (출력)
 */
static void time_t_to_filetime(time_t t, FILETIME* ft);

/**
 * @brief Windows 파일 속성을 공통 파일 타입으로 변환합니다
 * @param attributes Windows 파일 속성
 * @return 공통 파일 타입
 */
static ETFileType windows_attributes_to_file_type(DWORD attributes);

/**
 * @brief Windows 파일 속성을 공통 권한으로 변환합니다
 * @param attributes Windows 파일 속성
 * @return 공통 파일 권한
 */
static uint32_t windows_attributes_to_permissions(DWORD attributes);

/**
 * @brief 공통 권한을 Windows 파일 속성으로 변환합니다
 * @param permissions 공통 파일 권한
 * @return Windows 파일 속성
 */
static DWORD permissions_to_windows_attributes(uint32_t permissions);

/**
 * @brief 파일 모드를 Windows 액세스 모드로 변환합니다
 * @param mode 파일 모드
 * @param access Windows 액세스 모드 (출력)
 * @param creation Windows 생성 모드 (출력)
 * @param share Windows 공유 모드 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult convert_file_mode_to_windows(uint32_t mode, DWORD* access, DWORD* creation, DWORD* share);

// ============================================================================
// 경로 처리 함수 구현
// ============================================================================

static ETResult windows_normalize_path(const char* path, char* normalized, size_t size) {
    if (!path || !normalized || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Windows 경로 정규화
    if (!PathCanonicalizeA(normalized, path)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_join_path(const char* base, const char* relative, char* result, size_t size) {
    if (!base || !relative || !result || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Windows 경로 결합
    if (!PathCombineA(result, base, relative)) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    return ET_SUCCESS;
}

static ETResult windows_get_absolute_path(const char* path, char* absolute, size_t size) {
    if (!path || !absolute || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DWORD result = GetFullPathNameA(path, (DWORD)size, absolute, NULL);
    if (result == 0) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    if (result >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    return ET_SUCCESS;
}

static ETResult windows_get_dirname(const char* path, char* dirname, size_t size) {
    if (!path || !dirname || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    char temp_path[MAX_PATH];
    strcpy_s(temp_path, sizeof(temp_path), path);

    PathRemoveFileSpecA(temp_path);

    size_t len = strlen(temp_path);
    if (len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy_s(dirname, size, temp_path);
    return ET_SUCCESS;
}

static ETResult windows_get_basename(const char* path, char* basename, size_t size) {
    if (!path || !basename || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const char* filename = PathFindFileNameA(path);
    if (!filename) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t len = strlen(filename);
    if (len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy_s(basename, size, filename);
    return ET_SUCCESS;
}

static ETResult windows_get_extension(const char* path, char* extension, size_t size) {
    if (!path || !extension || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const char* ext = PathFindExtensionA(path);
    if (!ext || *ext == '\0') {
        extension[0] = '\0';
        return ET_SUCCESS;
    }

    size_t len = strlen(ext);
    if (len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy_s(extension, size, ext);
    return ET_SUCCESS;
}

// ============================================================================
// 파일 I/O 함수 구현
// ============================================================================

static ETResult windows_open_file(const char* path, uint32_t mode, ETFile** file) {
    if (!path || !file) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsFile* win_file = (ETWindowsFile*)malloc(sizeof(ETWindowsFile));
    if (!win_file) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(win_file, 0, sizeof(ETWindowsFile));
    strcpy_s(win_file->path, sizeof(win_file->path), path);
    win_file->mode = mode;

    // 파일 모드를 Windows 파라미터로 변환
    DWORD access, creation, share;
    ETResult result = convert_file_mode_to_windows(mode, &access, &creation, &share);
    if (result != ET_SUCCESS) {
        free(win_file);
        return result;
    }

    // 파일 열기
    win_file->handle = CreateFileA(
        path,
        access,
        share,
        NULL,
        creation,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (win_file->handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        free(win_file);
        return windows_error_to_et_result(error);
    }

    win_file->is_open = true;
    *file = (ETFile*)win_file;

    return ET_SUCCESS;
}

static ETResult windows_read_file(ETFile* file, void* buffer, size_t size, size_t* bytes_read) {
    if (!file || !buffer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsFile* win_file = (ETWindowsFile*)file;
    if (!win_file->is_open || win_file->handle == INVALID_HANDLE_VALUE) {
        return ET_ERROR_INVALID_STATE;
    }

    DWORD read = 0;
    BOOL success = ReadFile(win_file->handle, buffer, (DWORD)size, &read, NULL);

    if (bytes_read) {
        *bytes_read = (size_t)read;
    }

    if (!success) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_write_file(ETFile* file, const void* buffer, size_t size, size_t* bytes_written) {
    if (!file || !buffer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsFile* win_file = (ETWindowsFile*)file;
    if (!win_file->is_open || win_file->handle == INVALID_HANDLE_VALUE) {
        return ET_ERROR_INVALID_STATE;
    }

    DWORD written = 0;
    BOOL success = WriteFile(win_file->handle, buffer, (DWORD)size, &written, NULL);

    if (bytes_written) {
        *bytes_written = (size_t)written;
    }

    if (!success) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_seek_file(ETFile* file, int64_t offset, ETSeekOrigin origin) {
    if (!file) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsFile* win_file = (ETWindowsFile*)file;
    if (!win_file->is_open || win_file->handle == INVALID_HANDLE_VALUE) {
        return ET_ERROR_INVALID_STATE;
    }

    DWORD move_method;
    switch (origin) {
        case ET_SEEK_SET: move_method = FILE_BEGIN; break;
        case ET_SEEK_CUR: move_method = FILE_CURRENT; break;
        case ET_SEEK_END: move_method = FILE_END; break;
        default: return ET_ERROR_INVALID_ARGUMENT;
    }

    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;

    LARGE_INTEGER new_pos;
    if (!SetFilePointerEx(win_file->handle, li_offset, &new_pos, move_method)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static int64_t windows_tell_file(ETFile* file) {
    if (!file) {
        return -1;
    }

    ETWindowsFile* win_file = (ETWindowsFile*)file;
    if (!win_file->is_open || win_file->handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    LARGE_INTEGER zero = {0};
    LARGE_INTEGER pos;

    if (!SetFilePointerEx(win_file->handle, zero, &pos, FILE_CURRENT)) {
        return -1;
    }

    return pos.QuadPart;
}

static ETResult windows_flush_file(ETFile* file) {
    if (!file) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsFile* win_file = (ETWindowsFile*)file;
    if (!win_file->is_open || win_file->handle == INVALID_HANDLE_VALUE) {
        return ET_ERROR_INVALID_STATE;
    }

    if (!FlushFileBuffers(win_file->handle)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static void windows_close_file(ETFile* file) {
    if (!file) {
        return;
    }

    ETWindowsFile* win_file = (ETWindowsFile*)file;
    if (win_file->is_open && win_file->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(win_file->handle);
        win_file->handle = INVALID_HANDLE_VALUE;
        win_file->is_open = false;
    }

    free(win_file);
}

// ============================================================================
// 디렉토리 조작 함수 구현
// ============================================================================

static ETResult windows_create_directory(const char* path, uint32_t permissions, bool recursive) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (recursive) {
        // 재귀적 디렉토리 생성
        char temp_path[MAX_PATH];
        strcpy_s(temp_path, sizeof(temp_path), path);

        // 경로를 정규화
        PathCanonicalizeA(temp_path, temp_path);

        // SHCreateDirectoryEx 사용
        int result = SHCreateDirectoryExA(NULL, temp_path, NULL);
        if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS) {
            return windows_error_to_et_result(result);
        }
    } else {
        // 단일 디렉토리 생성
        if (!CreateDirectoryA(path, NULL)) {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                return windows_error_to_et_result(error);
            }
        }
    }

    // 권한 설정 (Windows에서는 제한적)
    if (permissions != ET_DEFAULT_DIR_PERMISSIONS) {
        DWORD attributes = permissions_to_windows_attributes(permissions);
        SetFileAttributesA(path, attributes);
    }

    return ET_SUCCESS;
}

static ETResult windows_remove_directory(const char* path, bool recursive) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (recursive) {
        // 재귀적 디렉토리 삭제
        char search_path[MAX_PATH];
        snprintf(search_path, sizeof(search_path), "%s\\*", path);

        WIN32_FIND_DATAA find_data;
        HANDLE find_handle = FindFirstFileA(search_path, &find_data);

        if (find_handle != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
                    continue;
                }

                char full_path[MAX_PATH];
                snprintf(full_path, sizeof(full_path), "%s\\%s", path, find_data.cFileName);

                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    windows_remove_directory(full_path, true);
                } else {
                    DeleteFileA(full_path);
                }
            } while (FindNextFileA(find_handle, &find_data));

            FindClose(find_handle);
        }
    }

    // 디렉토리 삭제
    if (!RemoveDirectoryA(path)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_list_directory(const char* path, ETDirectoryEntry* entries, int* count, uint32_t options) {
    if (!path || !entries || !count) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(search_path, &find_data);

    if (find_handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    int entry_count = 0;
    int max_count = *count;

    do {
        if (entry_count >= max_count) {
            break;
        }

        // . 및 .. 항목 건너뛰기
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        // 숨김 파일 처리
        bool is_hidden = (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        if (is_hidden && !(options & ET_DIR_OPTION_INCLUDE_HIDDEN)) {
            continue;
        }

        ETDirectoryEntry* entry = &entries[entry_count];

        // 이름 복사
        strcpy_s(entry->name, sizeof(entry->name), find_data.cFileName);
        snprintf(entry->path, sizeof(entry->path), "%s\\%s", path, find_data.cFileName);

        // 타입 설정
        entry->type = windows_attributes_to_file_type(find_data.dwFileAttributes);

        // 크기 설정
        if (entry->type == ET_FILE_TYPE_REGULAR) {
            LARGE_INTEGER file_size;
            file_size.LowPart = find_data.nFileSizeLow;
            file_size.HighPart = find_data.nFileSizeHigh;
            entry->size = file_size.QuadPart;
        } else {
            entry->size = 0;
        }

        // 수정 시간 설정
        entry->modified_time = filetime_to_time_t(&find_data.ftLastWriteTime);

        // 숨김 파일 여부
        entry->is_hidden = is_hidden;

        entry_count++;

    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);

    *count = entry_count;
    return ET_SUCCESS;
}

static ETResult windows_get_current_directory(char* path, size_t size) {
    if (!path || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DWORD result = GetCurrentDirectoryA((DWORD)size, path);
    if (result == 0) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    if (result >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    return ET_SUCCESS;
}

static ETResult windows_set_current_directory(const char* path) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!SetCurrentDirectoryA(path)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 파일 속성 및 정보 함수 구현
// ============================================================================

static ETResult windows_get_file_info(const char* path, ETFileInfo* info) {
    if (!path || !info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(path, &find_data);

    if (find_handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    FindClose(find_handle);

    // 정보 설정
    strcpy_s(info->name, sizeof(info->name), find_data.cFileName);
    strcpy_s(info->full_path, sizeof(info->full_path), path);

    info->type = windows_attributes_to_file_type(find_data.dwFileAttributes);
    info->permissions = windows_attributes_to_permissions(find_data.dwFileAttributes);

    // 파일 크기
    if (info->type == ET_FILE_TYPE_REGULAR) {
        LARGE_INTEGER file_size;
        file_size.LowPart = find_data.nFileSizeLow;
        file_size.HighPart = find_data.nFileSizeHigh;
        info->size = file_size.QuadPart;
    } else {
        info->size = 0;
    }

    // 시간 정보
    info->created_time = filetime_to_time_t(&find_data.ftCreationTime);
    info->modified_time = filetime_to_time_t(&find_data.ftLastWriteTime);
    info->accessed_time = filetime_to_time_t(&find_data.ftLastAccessTime);

    // 속성 플래그
    info->is_hidden = (find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    info->is_readonly = (find_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    info->is_system = (find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;

    return ET_SUCCESS;
}

static ETResult windows_set_file_permissions(const char* path, uint32_t permissions) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DWORD attributes = permissions_to_windows_attributes(permissions);

    if (!SetFileAttributesA(path, attributes)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_set_file_times(const char* path, const time_t* access_time, const time_t* modify_time) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    HANDLE handle = CreateFileA(
        path,
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    FILETIME* access_ft = NULL;
    FILETIME* modify_ft = NULL;
    FILETIME access_filetime, modify_filetime;

    if (access_time) {
        time_t_to_filetime(*access_time, &access_filetime);
        access_ft = &access_filetime;
    }

    if (modify_time) {
        time_t_to_filetime(*modify_time, &modify_filetime);
        modify_ft = &modify_filetime;
    }

    BOOL success = SetFileTime(handle, NULL, access_ft, modify_ft);
    CloseHandle(handle);

    if (!success) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static bool windows_file_exists(const char* path) {
    if (!path) {
        return false;
    }

    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES;
}

static bool windows_is_directory(const char* path) {
    if (!path) {
        return false;
    }

    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static bool windows_is_regular_file(const char* path) {
    if (!path) {
        return false;
    }

    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY) &&
           !(attributes & FILE_ATTRIBUTE_DEVICE);
}

static bool windows_is_symlink(const char* path) {
    if (!path) {
        return false;
    }

    DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

// ============================================================================
// 파일 조작 함수 구현
// ============================================================================

static ETResult windows_copy_file(const char* source, const char* destination, bool overwrite) {
    if (!source || !destination) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    BOOL fail_if_exists = overwrite ? FALSE : TRUE;

    if (!CopyFileA(source, destination, fail_if_exists)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_move_file(const char* source, const char* destination) {
    if (!source || !destination) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!MoveFileA(source, destination)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_delete_file(const char* path) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!DeleteFileA(path)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_create_symlink(const char* target, const char* linkpath) {
    if (!target || !linkpath) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Windows Vista 이상에서 지원
    DWORD flags = 0;
    if (windows_is_directory(target)) {
        flags = SYMBOLIC_LINK_FLAG_DIRECTORY;
    }

    if (!CreateSymbolicLinkA(linkpath, target, flags)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    return ET_SUCCESS;
}

static ETResult windows_read_symlink(const char* linkpath, char* target, size_t size) {
    if (!linkpath || !target || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    HANDLE handle = CreateFileA(
        linkpath,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL
    );

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    // 심볼릭 링크 대상 읽기는 복잡하므로 간단한 구현
    // 실제로는 DeviceIoControl과 REPARSE_DATA_BUFFER를 사용해야 함
    CloseHandle(handle);

    // 임시로 GetFinalPathNameByHandle 사용
    handle = CreateFileA(
        linkpath,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    DWORD result = GetFinalPathNameByHandleA(handle, target, (DWORD)size, 0);
    CloseHandle(handle);

    if (result == 0) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    if (result >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 디스크 공간 정보 함수 구현
// ============================================================================

static ETResult windows_get_disk_space(const char* path, ETDiskSpaceInfo* info) {
    if (!path || !info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ULARGE_INTEGER free_bytes, total_bytes;

    if (!GetDiskFreeSpaceExA(path, &free_bytes, &total_bytes, NULL)) {
        DWORD error = GetLastError();
        return windows_error_to_et_result(error);
    }

    info->total_space = total_bytes.QuadPart;
    info->free_space = free_bytes.QuadPart;
    info->used_space = total_bytes.QuadPart - free_bytes.QuadPart;

    // 파일시스템 타입 가져오기
    char volume_name[MAX_PATH];
    char filesystem_name[MAX_PATH];
    DWORD serial_number, max_component_len, filesystem_flags;

    if (GetVolumeInformationA(
        path,
        volume_name, sizeof(volume_name),
        &serial_number,
        &max_component_len,
        &filesystem_flags,
        filesystem_name, sizeof(filesystem_name)
    )) {
        strcpy_s(info->filesystem_type, sizeof(info->filesystem_type), filesystem_name);
    } else {
        strcpy_s(info->filesystem_type, sizeof(info->filesystem_type), "Unknown");
    }

    return ET_SUCCESS;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static ETResult windows_error_to_et_result(DWORD win_error) {
    switch (win_error) {
        case ERROR_SUCCESS:
            return ET_SUCCESS;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return ET_ERROR_NOT_FOUND;
        case ERROR_ACCESS_DENIED:
            return ET_ERROR_INVALID_ARGUMENT;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return ET_ERROR_OUT_OF_MEMORY;
        case ERROR_INVALID_PARAMETER:
        case ERROR_INVALID_NAME:
            return ET_ERROR_INVALID_ARGUMENT;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            return ET_ERROR_ALREADY_INITIALIZED;
        case ERROR_DISK_FULL:
            return ET_ERROR_BUFFER_FULL;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            return ET_ERROR_INVALID_STATE;
        default:
            return ET_ERROR_SYSTEM;
    }
}

static time_t filetime_to_time_t(const FILETIME* ft) {
    ULARGE_INTEGER ull;
    ull.LowPart = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;

    // Windows FILETIME은 1601년 1월 1일부터의 100나노초 단위
    // Unix time_t는 1970년 1월 1일부터의 초 단위
    return (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
}

static void time_t_to_filetime(time_t t, FILETIME* ft) {
    ULARGE_INTEGER ull;
    ull.QuadPart = ((ULONGLONG)t * 10000000ULL) + 116444736000000000ULL;

    ft->dwLowDateTime = ull.LowPart;
    ft->dwHighDateTime = ull.HighPart;
}

static ETFileType windows_attributes_to_file_type(DWORD attributes) {
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return ET_FILE_TYPE_DIRECTORY;
    } else if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        return ET_FILE_TYPE_SYMLINK;
    } else if (attributes & FILE_ATTRIBUTE_DEVICE) {
        return ET_FILE_TYPE_DEVICE;
    } else {
        return ET_FILE_TYPE_REGULAR;
    }
}

static uint32_t windows_attributes_to_permissions(DWORD attributes) {
    uint32_t permissions = 0;

    // Windows는 Unix와 다른 권한 시스템을 사용하므로 근사치로 변환
    if (!(attributes & FILE_ATTRIBUTE_READONLY)) {
        permissions |= ET_PERM_OWNER_WRITE | ET_PERM_GROUP_WRITE | ET_PERM_OTHER_WRITE;
    }

    // 읽기 및 실행 권한은 기본적으로 허용
    permissions |= ET_PERM_OWNER_READ | ET_PERM_GROUP_READ | ET_PERM_OTHER_READ;

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        permissions |= ET_PERM_OWNER_EXEC | ET_PERM_GROUP_EXEC | ET_PERM_OTHER_EXEC;
    }

    return permissions;
}

static DWORD permissions_to_windows_attributes(uint32_t permissions) {
    DWORD attributes = FILE_ATTRIBUTE_NORMAL;

    // 쓰기 권한이 없으면 읽기 전용으로 설정
    if (!(permissions & (ET_PERM_OWNER_WRITE | ET_PERM_GROUP_WRITE | ET_PERM_OTHER_WRITE))) {
        attributes |= FILE_ATTRIBUTE_READONLY;
    }

    return attributes;
}

static ETResult convert_file_mode_to_windows(uint32_t mode, DWORD* access, DWORD* creation, DWORD* share) {
    if (!access || !creation || !share) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 액세스 모드 설정
    *access = 0;
    if (mode & ET_FILE_MODE_READ) {
        *access |= GENERIC_READ;
    }
    if (mode & ET_FILE_MODE_WRITE) {
        *access |= GENERIC_WRITE;
    }

    // 생성 모드 설정
    if (mode & ET_FILE_MODE_CREATE) {
        if (mode & ET_FILE_MODE_TRUNCATE) {
            *creation = CREATE_ALWAYS;
        } else {
            *creation = OPEN_ALWAYS;
        }
    } else {
        if (mode & ET_FILE_MODE_TRUNCATE) {
            *creation = TRUNCATE_EXISTING;
        } else {
            *creation = OPEN_EXISTING;
        }
    }

    // 공유 모드 설정
    *share = FILE_SHARE_READ;
    if (!(mode & ET_FILE_MODE_WRITE)) {
        *share |= FILE_SHARE_WRITE;
    }

    return ET_SUCCESS;
}

// ============================================================================
// Windows 파일시스템 인터페이스 생성
// ============================================================================

/**
 * @brief Windows 파일시스템 인터페이스를 생성합니다
 */
ETResult et_create_windows_filesystem_interface(ETFilesystemInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETFilesystemInterface* fs_interface = (ETFilesystemInterface*)malloc(sizeof(ETFilesystemInterface));
    if (!fs_interface) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(fs_interface, 0, sizeof(ETFilesystemInterface));

    // 경로 처리 함수들
    fs_interface->normalize_path = windows_normalize_path;
    fs_interface->join_path = windows_join_path;
    fs_interface->get_absolute_path = windows_get_absolute_path;
    fs_interface->get_dirname = windows_get_dirname;
    fs_interface->get_basename = windows_get_basename;
    fs_interface->get_extension = windows_get_extension;

    // 파일 I/O 함수들
    fs_interface->open_file = windows_open_file;
    fs_interface->read_file = windows_read_file;
    fs_interface->write_file = windows_write_file;
    fs_interface->seek_file = windows_seek_file;
    fs_interface->tell_file = windows_tell_file;
    fs_interface->flush_file = windows_flush_file;
    fs_interface->close_file = windows_close_file;

    // 디렉토리 조작 함수들
    fs_interface->create_directory = windows_create_directory;
    fs_interface->remove_directory = windows_remove_directory;
    fs_interface->list_directory = windows_list_directory;
    fs_interface->get_current_directory = windows_get_current_directory;
    fs_interface->set_current_directory = windows_set_current_directory;

    // 파일 속성 및 정보 함수들
    fs_interface->get_file_info = windows_get_file_info;
    fs_interface->set_file_permissions = windows_set_file_permissions;
    fs_interface->set_file_times = windows_set_file_times;
    fs_interface->file_exists = windows_file_exists;
    fs_interface->is_directory = windows_is_directory;
    fs_interface->is_regular_file = windows_is_regular_file;
    fs_interface->is_symlink = windows_is_symlink;

    // 파일 조작 함수들
    fs_interface->copy_file = windows_copy_file;
    fs_interface->move_file = windows_move_file;
    fs_interface->delete_file = windows_delete_file;
    fs_interface->create_symlink = windows_create_symlink;
    fs_interface->read_symlink = windows_read_symlink;

    // 디스크 공간 정보 함수들
    fs_interface->get_disk_space = windows_get_disk_space;

    // 플랫폼 데이터 초기화
    ETWindowsFilesystemData* platform_data = (ETWindowsFilesystemData*)malloc(sizeof(ETWindowsFilesystemData));
    if (!platform_data) {
        free(fs_interface);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(platform_data, 0, sizeof(ETWindowsFilesystemData));
    GetCurrentDirectoryA(sizeof(platform_data->current_directory), platform_data->current_directory);

    fs_interface->platform_data = platform_data;

    *interface = fs_interface;
    return ET_SUCCESS;
}

/**
 * @brief Windows 파일시스템 인터페이스를 해제합니다
 */
void et_destroy_windows_filesystem_interface(ETFilesystemInterface* interface) {
    if (!interface) {
        return;
    }

    if (interface->platform_data) {
        free(interface->platform_data);
    }

    free(interface);
}

#endif // _WIN32