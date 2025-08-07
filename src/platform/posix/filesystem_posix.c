/**
 * @file filesystem_posix.c
 * @brief POSIX 파일시스템 구현체 (Linux/macOS 공통)
 * @author LibEtude Project
 * @version 1.0.0
 *
 * POSIX API를 사용한 파일시스템 추상화 구현입니다.
 * Linux와 macOS에서 공통으로 사용되며, Unix 경로 처리와 POSIX 권한 시스템을 지원합니다.
 */

#if defined(__linux__) || defined(__APPLE__)

#include "libetude/platform/filesystem.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <utime.h>
#include <limits.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <copyfile.h>
#endif

#ifdef __linux__
#include <sys/sendfile.h>
#endif

// ============================================================================
// POSIX 전용 구조체 및 상수
// ============================================================================

/**
 * @brief POSIX 파일 핸들 구조체
 */
typedef struct {
    int fd;                             /**< POSIX 파일 디스크립터 */
    char path[PATH_MAX];                /**< 파일 경로 */
    uint32_t mode;                      /**< 파일 모드 */
    bool is_open;                       /**< 열림 상태 */
} ETPosixFile;

/**
 * @brief POSIX 파일시스템 플랫폼 데이터
 */
typedef struct {
    char current_directory[PATH_MAX];   /**< 현재 작업 디렉토리 */
    int last_errno;                     /**< 마지막 errno 값 */
} ETPosixFilesystemData;

// ============================================================================
// 내부 함수 선언
// ============================================================================

/**
 * @brief errno 값을 공통 오류 코드로 변환합니다
 * @param error_code errno 값
 * @return 공통 오류 코드
 */
static ETResult errno_to_et_result(int error_code);

/**
 * @brief struct stat을 ETFileInfo로 변환합니다
 * @param st struct stat 포인터
 * @param path 파일 경로
 * @param info ETFileInfo 포인터 (출력)
 */
static void stat_to_file_info(const struct stat* st, const char* path, ETFileInfo* info);

/**
 * @brief POSIX 파일 모드를 공통 파일 타입으로 변환합니다
 * @param mode POSIX 파일 모드
 * @return 공통 파일 타입
 */
static ETFileType posix_mode_to_file_type(mode_t mode);

/**
 * @brief POSIX 파일 모드를 공통 권한으로 변환합니다
 * @param mode POSIX 파일 모드
 * @return 공통 파일 권한
 */
static uint32_t posix_mode_to_permissions(mode_t mode);

/**
 * @brief 공통 권한을 POSIX 파일 모드로 변환합니다
 * @param permissions 공통 파일 권한
 * @return POSIX 파일 모드
 */
static mode_t permissions_to_posix_mode(uint32_t permissions);

/**
 * @brief 파일 모드를 POSIX 플래그로 변환합니다
 * @param mode 파일 모드
 * @param flags POSIX 플래그 (출력)
 * @param posix_mode POSIX 모드 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult convert_file_mode_to_posix(uint32_t mode, int* flags, mode_t* posix_mode);

/**
 * @brief 디렉토리를 재귀적으로 삭제합니다
 * @param path 디렉토리 경로
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
static ETResult remove_directory_recursive(const char* path);

// ============================================================================
// 경로 처리 함수 구현
// ============================================================================

static ETResult posix_normalize_path(const char* path, char* normalized, size_t size) {
    if (!path || !normalized || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // realpath를 사용하여 경로 정규화 (심볼릭 링크 해석 포함)
    char* resolved = realpath(path, NULL);
    if (!resolved) {
        // realpath 실패시 수동으로 정규화
        if (strlen(path) >= size) {
            return ET_ERROR_BUFFER_FULL;
        }

        strcpy(normalized, path);

        // 연속된 슬래시 제거
        char* write_pos = normalized;
        char* read_pos = normalized;
        char prev_char = '\0';

        while (*read_pos) {
            if (*read_pos == '/' && prev_char == '/') {
                read_pos++;
                continue;
            }
            *write_pos = *read_pos;
            prev_char = *read_pos;
            write_pos++;
            read_pos++;
        }

        // 마지막 슬래시 제거 (루트가 아닌 경우)
        if (write_pos > normalized + 1 && *(write_pos - 1) == '/') {
            write_pos--;
        }

        *write_pos = '\0';
        return ET_SUCCESS;
    }

    size_t len = strlen(resolved);
    if (len >= size) {
        free(resolved);
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(normalized, resolved);
    free(resolved);

    return ET_SUCCESS;
}

static ETResult posix_join_path(const char* base, const char* relative, char* result, size_t size) {
    if (!base || !relative || !result || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 상대 경로가 절대 경로인 경우 그대로 사용
    if (relative[0] == '/') {
        size_t len = strlen(relative);
        if (len >= size) {
            return ET_ERROR_BUFFER_FULL;
        }
        strcpy(result, relative);
        return posix_normalize_path(result, result, size);
    }

    // 기본 경로 복사
    size_t base_len = strlen(base);
    if (base_len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }
    strcpy(result, base);

    // 슬래시 추가 (필요한 경우)
    if (base_len > 0 && result[base_len - 1] != '/') {
        if (base_len + 1 >= size) {
            return ET_ERROR_BUFFER_FULL;
        }
        result[base_len] = '/';
        result[base_len + 1] = '\0';
        base_len++;
    }

    // 상대 경로 추가
    size_t relative_len = strlen(relative);
    if (base_len + relative_len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }
    strcat(result, relative);

    // 경로 정규화
    return posix_normalize_path(result, result, size);
}

static ETResult posix_get_absolute_path(const char* path, char* absolute, size_t size) {
    if (!path || !absolute || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    char* resolved = realpath(path, NULL);
    if (!resolved) {
        return errno_to_et_result(errno);
    }

    size_t len = strlen(resolved);
    if (len >= size) {
        free(resolved);
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(absolute, resolved);
    free(resolved);

    return ET_SUCCESS;
}

static ETResult posix_get_dirname(const char* path, char* dirname, size_t size) {
    if (!path || !dirname || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    char temp_path[PATH_MAX];
    strcpy(temp_path, path);

    char* dir = dirname(temp_path);
    if (!dir) {
        return ET_ERROR_SYSTEM;
    }

    size_t len = strlen(dir);
    if (len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(dirname, dir);
    return ET_SUCCESS;
}

static ETResult posix_get_basename(const char* path, char* basename, size_t size) {
    if (!path || !basename || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    char temp_path[PATH_MAX];
    strcpy(temp_path, path);

    char* base = basename(temp_path);
    if (!base) {
        return ET_ERROR_SYSTEM;
    }

    size_t len = strlen(base);
    if (len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(basename, base);
    return ET_SUCCESS;
}

static ETResult posix_get_extension(const char* path, char* extension, size_t size) {
    if (!path || !extension || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 먼저 파일 이름 부분만 추출
    char basename_buf[PATH_MAX];
    ETResult result = posix_get_basename(path, basename_buf, sizeof(basename_buf));
    if (result != ET_SUCCESS) {
        return result;
    }

    // 마지막 점 찾기
    const char* last_dot = strrchr(basename_buf, '.');
    if (!last_dot || last_dot == basename_buf) {
        // 확장자가 없거나 숨김 파일
        if (size < 1) {
            return ET_ERROR_BUFFER_FULL;
        }
        extension[0] = '\0';
        return ET_SUCCESS;
    }

    size_t ext_len = strlen(last_dot);
    if (ext_len >= size) {
        return ET_ERROR_BUFFER_FULL;
    }

    strcpy(extension, last_dot);
    return ET_SUCCESS;
}

// ============================================================================
// 파일 I/O 함수 구현
// ============================================================================

static ETResult posix_open_file(const char* path, uint32_t mode, ETFile** file) {
    if (!path || !file) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETPosixFile* posix_file = (ETPosixFile*)malloc(sizeof(ETPosixFile));
    if (!posix_file) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(posix_file, 0, sizeof(ETPosixFile));
    strcpy(posix_file->path, path);
    posix_file->mode = mode;

    // 파일 모드를 POSIX 플래그로 변환
    int flags;
    mode_t posix_mode;
    ETResult result = convert_file_mode_to_posix(mode, &flags, &posix_mode);
    if (result != ET_SUCCESS) {
        free(posix_file);
        return result;
    }

    // 파일 열기
    posix_file->fd = open(path, flags, posix_mode);
    if (posix_file->fd == -1) {
        int error = errno;
        free(posix_file);
        return errno_to_et_result(error);
    }

    posix_file->is_open = true;
    *file = (ETFile*)posix_file;

    return ET_SUCCESS;
}

static ETResult posix_read_file(ETFile* file, void* buffer, size_t size, size_t* bytes_read) {
    if (!file || !buffer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETPosixFile* posix_file = (ETPosixFile*)file;
    if (!posix_file->is_open || posix_file->fd == -1) {
        return ET_ERROR_INVALID_STATE;
    }

    ssize_t result = read(posix_file->fd, buffer, size);
    if (result == -1) {
        return errno_to_et_result(errno);
    }

    if (bytes_read) {
        *bytes_read = (size_t)result;
    }

    return ET_SUCCESS;
}

static ETResult posix_write_file(ETFile* file, const void* buffer, size_t size, size_t* bytes_written) {
    if (!file || !buffer) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETPosixFile* posix_file = (ETPosixFile*)file;
    if (!posix_file->is_open || posix_file->fd == -1) {
        return ET_ERROR_INVALID_STATE;
    }

    ssize_t result = write(posix_file->fd, buffer, size);
    if (result == -1) {
        return errno_to_et_result(errno);
    }

    if (bytes_written) {
        *bytes_written = (size_t)result;
    }

    return ET_SUCCESS;
}

static ETResult posix_seek_file(ETFile* file, int64_t offset, ETSeekOrigin origin) {
    if (!file) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETPosixFile* posix_file = (ETPosixFile*)file;
    if (!posix_file->is_open || posix_file->fd == -1) {
        return ET_ERROR_INVALID_STATE;
    }

    int whence;
    switch (origin) {
        case ET_SEEK_SET: whence = SEEK_SET; break;
        case ET_SEEK_CUR: whence = SEEK_CUR; break;
        case ET_SEEK_END: whence = SEEK_END; break;
        default: return ET_ERROR_INVALID_ARGUMENT;
    }

    off_t result = lseek(posix_file->fd, (off_t)offset, whence);
    if (result == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static int64_t posix_tell_file(ETFile* file) {
    if (!file) {
        return -1;
    }

    ETPosixFile* posix_file = (ETPosixFile*)file;
    if (!posix_file->is_open || posix_file->fd == -1) {
        return -1;
    }

    off_t pos = lseek(posix_file->fd, 0, SEEK_CUR);
    if (pos == -1) {
        return -1;
    }

    return (int64_t)pos;
}

static ETResult posix_flush_file(ETFile* file) {
    if (!file) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETPosixFile* posix_file = (ETPosixFile*)file;
    if (!posix_file->is_open || posix_file->fd == -1) {
        return ET_ERROR_INVALID_STATE;
    }

    if (fsync(posix_file->fd) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static void posix_close_file(ETFile* file) {
    if (!file) {
        return;
    }

    ETPosixFile* posix_file = (ETPosixFile*)file;
    if (posix_file->is_open && posix_file->fd != -1) {
        close(posix_file->fd);
        posix_file->fd = -1;
        posix_file->is_open = false;
    }

    free(posix_file);
}

// ============================================================================
// 디렉토리 조작 함수 구현
// ============================================================================

static ETResult posix_create_directory(const char* path, uint32_t permissions, bool recursive) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    mode_t mode = permissions_to_posix_mode(permissions);

    if (recursive) {
        // 재귀적 디렉토리 생성
        char temp_path[PATH_MAX];
        strcpy(temp_path, path);

        char* p = temp_path;
        if (*p == '/') p++; // 루트 슬래시 건너뛰기

        while ((p = strchr(p, '/')) != NULL) {
            *p = '\0';

            if (mkdir(temp_path, mode) == -1 && errno != EEXIST) {
                return errno_to_et_result(errno);
            }

            *p = '/';
            p++;
        }

        // 마지막 디렉토리 생성
        if (mkdir(temp_path, mode) == -1 && errno != EEXIST) {
            return errno_to_et_result(errno);
        }
    } else {
        // 단일 디렉토리 생성
        if (mkdir(path, mode) == -1 && errno != EEXIST) {
            return errno_to_et_result(errno);
        }
    }

    return ET_SUCCESS;
}

static ETResult posix_remove_directory(const char* path, bool recursive) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (recursive) {
        return remove_directory_recursive(path);
    } else {
        if (rmdir(path) == -1) {
            return errno_to_et_result(errno);
        }
    }

    return ET_SUCCESS;
}

static ETResult posix_list_directory(const char* path, ETDirectoryEntry* entries, int* count, uint32_t options) {
    if (!path || !entries || !count) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    DIR* dir = opendir(path);
    if (!dir) {
        return errno_to_et_result(errno);
    }

    int entry_count = 0;
    int max_count = *count;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL && entry_count < max_count) {
        // . 및 .. 항목 건너뛰기
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 숨김 파일 처리
        bool is_hidden = entry->d_name[0] == '.';
        if (is_hidden && !(options & ET_DIR_OPTION_INCLUDE_HIDDEN)) {
            continue;
        }

        ETDirectoryEntry* dir_entry = &entries[entry_count];

        // 이름 복사
        strcpy(dir_entry->name, entry->d_name);
        snprintf(dir_entry->path, sizeof(dir_entry->path), "%s/%s", path, entry->d_name);

        // 파일 정보 가져오기
        struct stat st;
        if (stat(dir_entry->path, &st) == 0) {
            dir_entry->type = posix_mode_to_file_type(st.st_mode);
            dir_entry->size = (uint64_t)st.st_size;
            dir_entry->modified_time = st.st_mtime;
        } else {
            // stat 실패시 dirent 정보 사용
            switch (entry->d_type) {
                case DT_REG: dir_entry->type = ET_FILE_TYPE_REGULAR; break;
                case DT_DIR: dir_entry->type = ET_FILE_TYPE_DIRECTORY; break;
                case DT_LNK: dir_entry->type = ET_FILE_TYPE_SYMLINK; break;
                default: dir_entry->type = ET_FILE_TYPE_UNKNOWN; break;
            }
            dir_entry->size = 0;
            dir_entry->modified_time = 0;
        }

        dir_entry->is_hidden = is_hidden;
        entry_count++;
    }

    closedir(dir);

    *count = entry_count;
    return ET_SUCCESS;
}

static ETResult posix_get_current_directory(char* path, size_t size) {
    if (!path || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!getcwd(path, size)) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static ETResult posix_set_current_directory(const char* path) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (chdir(path) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 파일 속성 및 정보 함수 구현
// ============================================================================

static ETResult posix_get_file_info(const char* path, ETFileInfo* info) {
    if (!path || !info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        return errno_to_et_result(errno);
    }

    stat_to_file_info(&st, path, info);
    return ET_SUCCESS;
}

static ETResult posix_set_file_permissions(const char* path, uint32_t permissions) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    mode_t mode = permissions_to_posix_mode(permissions);

    if (chmod(path, mode) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static ETResult posix_set_file_times(const char* path, const time_t* access_time, const time_t* modify_time) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct utimbuf times;
    struct stat st;

    // 현재 시간 정보 가져오기
    if (stat(path, &st) == -1) {
        return errno_to_et_result(errno);
    }

    times.actime = access_time ? *access_time : st.st_atime;
    times.modtime = modify_time ? *modify_time : st.st_mtime;

    if (utime(path, &times) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static bool posix_file_exists(const char* path) {
    if (!path) {
        return false;
    }

    return access(path, F_OK) == 0;
}

static bool posix_is_directory(const char* path) {
    if (!path) {
        return false;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        return false;
    }

    return S_ISDIR(st.st_mode);
}

static bool posix_is_regular_file(const char* path) {
    if (!path) {
        return false;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        return false;
    }

    return S_ISREG(st.st_mode);
}

static bool posix_is_symlink(const char* path) {
    if (!path) {
        return false;
    }

    struct stat st;
    if (lstat(path, &st) == -1) {
        return false;
    }

    return S_ISLNK(st.st_mode);
}

// ============================================================================
// 파일 조작 함수 구현
// ============================================================================

static ETResult posix_copy_file(const char* source, const char* destination, bool overwrite) {
    if (!source || !destination) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 대상 파일 존재 확인
    if (!overwrite && access(destination, F_OK) == 0) {
        return ET_ERROR_ALREADY_INITIALIZED;
    }

#ifdef __APPLE__
    // macOS: copyfile 사용
    int flags = COPYFILE_DATA | COPYFILE_METADATA;
    if (copyfile(source, destination, NULL, flags) == -1) {
        return errno_to_et_result(errno);
    }
#else
    // Linux: 수동 복사
    int src_fd = open(source, O_RDONLY);
    if (src_fd == -1) {
        return errno_to_et_result(errno);
    }

    struct stat st;
    if (fstat(src_fd, &st) == -1) {
        close(src_fd);
        return errno_to_et_result(errno);
    }

    int dst_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd == -1) {
        close(src_fd);
        return errno_to_et_result(errno);
    }

    // sendfile을 사용한 효율적인 복사
    off_t offset = 0;
    ssize_t result = sendfile(dst_fd, src_fd, &offset, st.st_size);

    close(src_fd);
    close(dst_fd);

    if (result == -1) {
        return errno_to_et_result(errno);
    }
#endif

    return ET_SUCCESS;
}

static ETResult posix_move_file(const char* source, const char* destination) {
    if (!source || !destination) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (rename(source, destination) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static ETResult posix_delete_file(const char* path) {
    if (!path) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (unlink(path) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static ETResult posix_create_symlink(const char* target, const char* linkpath) {
    if (!target || !linkpath) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (symlink(target, linkpath) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

static ETResult posix_read_symlink(const char* linkpath, char* target, size_t size) {
    if (!linkpath || !target || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ssize_t len = readlink(linkpath, target, size - 1);
    if (len == -1) {
        return errno_to_et_result(errno);
    }

    target[len] = '\0';
    return ET_SUCCESS;
}

// ============================================================================
// 디스크 공간 정보 함수 구현
// ============================================================================

static ETResult posix_get_disk_space(const char* path, ETDiskSpaceInfo* info) {
    if (!path || !info) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct statvfs vfs;
    if (statvfs(path, &vfs) == -1) {
        return errno_to_et_result(errno);
    }

    info->total_space = (uint64_t)vfs.f_blocks * vfs.f_frsize;
    info->free_space = (uint64_t)vfs.f_bavail * vfs.f_frsize;
    info->used_space = info->total_space - info->free_space;

    // 파일시스템 타입은 POSIX에서 표준화되지 않음
    strcpy(info->filesystem_type, "Unknown");

    return ET_SUCCESS;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static ETResult errno_to_et_result(int error_code) {
    switch (error_code) {
        case 0:
            return ET_SUCCESS;
        case ENOENT:
        case ENOTDIR:
            return ET_ERROR_NOT_FOUND;
        case EACCES:
        case EPERM:
            return ET_ERROR_INVALID_ARGUMENT;
        case ENOMEM:
            return ET_ERROR_OUT_OF_MEMORY;
        case EINVAL:
            return ET_ERROR_INVALID_ARGUMENT;
        case EEXIST:
            return ET_ERROR_ALREADY_INITIALIZED;
        case ENOSPC:
            return ET_ERROR_BUFFER_FULL;
        case EBUSY:
        case EAGAIN:
            return ET_ERROR_INVALID_STATE;
        case EIO:
            return ET_ERROR_IO;
        default:
            return ET_ERROR_SYSTEM;
    }
}

static void stat_to_file_info(const struct stat* st, const char* path, ETFileInfo* info) {
    // 파일 이름 추출
    const char* filename = strrchr(path, '/');
    if (filename) {
        filename++; // 슬래시 다음 문자
    } else {
        filename = path;
    }

    strcpy(info->name, filename);
    strcpy(info->full_path, path);

    info->type = posix_mode_to_file_type(st->st_mode);
    info->size = (uint64_t)st->st_size;
    info->permissions = posix_mode_to_permissions(st->st_mode);

    info->created_time = st->st_ctime;
    info->modified_time = st->st_mtime;
    info->accessed_time = st->st_atime;

    info->is_hidden = filename[0] == '.';
    info->is_readonly = !(st->st_mode & S_IWUSR);
    info->is_system = false; // POSIX에는 시스템 파일 개념이 없음
}

static ETFileType posix_mode_to_file_type(mode_t mode) {
    if (S_ISREG(mode)) {
        return ET_FILE_TYPE_REGULAR;
    } else if (S_ISDIR(mode)) {
        return ET_FILE_TYPE_DIRECTORY;
    } else if (S_ISLNK(mode)) {
        return ET_FILE_TYPE_SYMLINK;
    } else if (S_ISCHR(mode) || S_ISBLK(mode)) {
        return ET_FILE_TYPE_DEVICE;
    } else if (S_ISFIFO(mode)) {
        return ET_FILE_TYPE_PIPE;
    } else if (S_ISSOCK(mode)) {
        return ET_FILE_TYPE_SOCKET;
    } else {
        return ET_FILE_TYPE_UNKNOWN;
    }
}

static uint32_t posix_mode_to_permissions(mode_t mode) {
    uint32_t permissions = 0;

    // 소유자 권한
    if (mode & S_IRUSR) permissions |= ET_PERM_OWNER_READ;
    if (mode & S_IWUSR) permissions |= ET_PERM_OWNER_WRITE;
    if (mode & S_IXUSR) permissions |= ET_PERM_OWNER_EXEC;

    // 그룹 권한
    if (mode & S_IRGRP) permissions |= ET_PERM_GROUP_READ;
    if (mode & S_IWGRP) permissions |= ET_PERM_GROUP_WRITE;
    if (mode & S_IXGRP) permissions |= ET_PERM_GROUP_EXEC;

    // 기타 권한
    if (mode & S_IROTH) permissions |= ET_PERM_OTHER_READ;
    if (mode & S_IWOTH) permissions |= ET_PERM_OTHER_WRITE;
    if (mode & S_IXOTH) permissions |= ET_PERM_OTHER_EXEC;

    return permissions;
}

static mode_t permissions_to_posix_mode(uint32_t permissions) {
    mode_t mode = 0;

    // 소유자 권한
    if (permissions & ET_PERM_OWNER_READ) mode |= S_IRUSR;
    if (permissions & ET_PERM_OWNER_WRITE) mode |= S_IWUSR;
    if (permissions & ET_PERM_OWNER_EXEC) mode |= S_IXUSR;

    // 그룹 권한
    if (permissions & ET_PERM_GROUP_READ) mode |= S_IRGRP;
    if (permissions & ET_PERM_GROUP_WRITE) mode |= S_IWGRP;
    if (permissions & ET_PERM_GROUP_EXEC) mode |= S_IXGRP;

    // 기타 권한
    if (permissions & ET_PERM_OTHER_READ) mode |= S_IROTH;
    if (permissions & ET_PERM_OTHER_WRITE) mode |= S_IWOTH;
    if (permissions & ET_PERM_OTHER_EXEC) mode |= S_IXOTH;

    return mode;
}

static ETResult convert_file_mode_to_posix(uint32_t mode, int* flags, mode_t* posix_mode) {
    if (!flags || !posix_mode) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *flags = 0;
    *posix_mode = 0644; // 기본 파일 권한

    // 읽기/쓰기 모드 설정
    if ((mode & ET_FILE_MODE_READ) && (mode & ET_FILE_MODE_WRITE)) {
        *flags |= O_RDWR;
    } else if (mode & ET_FILE_MODE_WRITE) {
        *flags |= O_WRONLY;
    } else {
        *flags |= O_RDONLY;
    }

    // 생성 및 잘라내기 모드
    if (mode & ET_FILE_MODE_CREATE) {
        *flags |= O_CREAT;
    }
    if (mode & ET_FILE_MODE_TRUNCATE) {
        *flags |= O_TRUNC;
    }
    if (mode & ET_FILE_MODE_APPEND) {
        *flags |= O_APPEND;
    }

    return ET_SUCCESS;
}

static ETResult remove_directory_recursive(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        return errno_to_et_result(errno);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            remove_directory_recursive(full_path);
        } else {
            unlink(full_path);
        }
    }

    closedir(dir);

    if (rmdir(path) == -1) {
        return errno_to_et_result(errno);
    }

    return ET_SUCCESS;
}

// ============================================================================
// POSIX 파일시스템 인터페이스 생성
// ============================================================================

/**
 * @brief POSIX 파일시스템 인터페이스를 생성합니다
 */
ETResult et_create_posix_filesystem_interface(ETFilesystemInterface** interface) {
    if (!interface) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETFilesystemInterface* fs_interface = (ETFilesystemInterface*)malloc(sizeof(ETFilesystemInterface));
    if (!fs_interface) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(fs_interface, 0, sizeof(ETFilesystemInterface));

    // 경로 처리 함수들
    fs_interface->normalize_path = posix_normalize_path;
    fs_interface->join_path = posix_join_path;
    fs_interface->get_absolute_path = posix_get_absolute_path;
    fs_interface->get_dirname = posix_get_dirname;
    fs_interface->get_basename = posix_get_basename;
    fs_interface->get_extension = posix_get_extension;

    // 파일 I/O 함수들
    fs_interface->open_file = posix_open_file;
    fs_interface->read_file = posix_read_file;
    fs_interface->write_file = posix_write_file;
    fs_interface->seek_file = posix_seek_file;
    fs_interface->tell_file = posix_tell_file;
    fs_interface->flush_file = posix_flush_file;
    fs_interface->close_file = posix_close_file;

    // 디렉토리 조작 함수들
    fs_interface->create_directory = posix_create_directory;
    fs_interface->remove_directory = posix_remove_directory;
    fs_interface->list_directory = posix_list_directory;
    fs_interface->get_current_directory = posix_get_current_directory;
    fs_interface->set_current_directory = posix_set_current_directory;

    // 파일 속성 및 정보 함수들
    fs_interface->get_file_info = posix_get_file_info;
    fs_interface->set_file_permissions = posix_set_file_permissions;
    fs_interface->set_file_times = posix_set_file_times;
    fs_interface->file_exists = posix_file_exists;
    fs_interface->is_directory = posix_is_directory;
    fs_interface->is_regular_file = posix_is_regular_file;
    fs_interface->is_symlink = posix_is_symlink;

    // 파일 조작 함수들
    fs_interface->copy_file = posix_copy_file;
    fs_interface->move_file = posix_move_file;
    fs_interface->delete_file = posix_delete_file;
    fs_interface->create_symlink = posix_create_symlink;
    fs_interface->read_symlink = posix_read_symlink;

    // 디스크 공간 정보 함수들
    fs_interface->get_disk_space = posix_get_disk_space;

    // 플랫폼 데이터 초기화
    ETPosixFilesystemData* platform_data = (ETPosixFilesystemData*)malloc(sizeof(ETPosixFilesystemData));
    if (!platform_data) {
        free(fs_interface);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    memset(platform_data, 0, sizeof(ETPosixFilesystemData));
    getcwd(platform_data->current_directory, sizeof(platform_data->current_directory));

    fs_interface->platform_data = platform_data;

    *interface = fs_interface;
    return ET_SUCCESS;
}

/**
 * @brief POSIX 파일시스템 인터페이스를 해제합니다
 */
void et_destroy_posix_filesystem_interface(ETFilesystemInterface* interface) {
    if (!interface) {
        return;
    }

    if (interface->platform_data) {
        free(interface->platform_data);
    }

    free(interface);
}

#endif // defined(__linux__) || defined(__APPLE__)