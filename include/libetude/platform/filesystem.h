/**
 * @file filesystem.h
 * @brief 플랫폼 추상화 파일시스템 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 파일시스템 API 차이를 추상화하여 일관된 인터페이스를 제공합니다.
 * Windows의 백슬래시와 Unix의 슬래시 경로 차이, 파일 권한 시스템 차이 등을 처리합니다.
 */

#ifndef LIBETUDE_PLATFORM_FILESYSTEM_H
#define LIBETUDE_PLATFORM_FILESYSTEM_H

#include "libetude/platform/common.h"
#include "libetude/types.h"
#include "libetude/error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 파일시스템 관련 타입 정의
// ============================================================================

/**
 * @brief 파일 모드 열거형
 */
typedef enum {
    ET_FILE_MODE_READ = 0x01,           /**< 읽기 모드 */
    ET_FILE_MODE_WRITE = 0x02,          /**< 쓰기 모드 */
    ET_FILE_MODE_APPEND = 0x04,         /**< 추가 모드 */
    ET_FILE_MODE_CREATE = 0x08,         /**< 생성 모드 */
    ET_FILE_MODE_TRUNCATE = 0x10,       /**< 잘라내기 모드 */
    ET_FILE_MODE_BINARY = 0x20,         /**< 바이너리 모드 */
    ET_FILE_MODE_TEXT = 0x40            /**< 텍스트 모드 */
} ETFileMode;

/**
 * @brief 파일 탐색 원점 열거형
 */
typedef enum {
    ET_SEEK_SET = 0,                    /**< 파일 시작점 */
    ET_SEEK_CUR = 1,                    /**< 현재 위치 */
    ET_SEEK_END = 2                     /**< 파일 끝점 */
} ETSeekOrigin;

/**
 * @brief 파일 타입 열거형
 */
typedef enum {
    ET_FILE_TYPE_UNKNOWN = 0,           /**< 알 수 없는 타입 */
    ET_FILE_TYPE_REGULAR = 1,           /**< 일반 파일 */
    ET_FILE_TYPE_DIRECTORY = 2,         /**< 디렉토리 */
    ET_FILE_TYPE_SYMLINK = 3,           /**< 심볼릭 링크 */
    ET_FILE_TYPE_DEVICE = 4,            /**< 디바이스 파일 */
    ET_FILE_TYPE_PIPE = 5,              /**< 파이프 */
    ET_FILE_TYPE_SOCKET = 6             /**< 소켓 */
} ETFileType;

/**
 * @brief 파일 권한 플래그
 */
typedef enum {
    ET_PERM_NONE = 0x000,               /**< 권한 없음 */
    ET_PERM_OWNER_READ = 0x100,         /**< 소유자 읽기 */
    ET_PERM_OWNER_WRITE = 0x080,        /**< 소유자 쓰기 */
    ET_PERM_OWNER_EXEC = 0x040,         /**< 소유자 실행 */
    ET_PERM_GROUP_READ = 0x020,         /**< 그룹 읽기 */
    ET_PERM_GROUP_WRITE = 0x010,        /**< 그룹 쓰기 */
    ET_PERM_GROUP_EXEC = 0x008,         /**< 그룹 실행 */
    ET_PERM_OTHER_READ = 0x004,         /**< 기타 읽기 */
    ET_PERM_OTHER_WRITE = 0x002,        /**< 기타 쓰기 */
    ET_PERM_OTHER_EXEC = 0x001,         /**< 기타 실행 */
    ET_PERM_ALL = 0x1FF                 /**< 모든 권한 */
} ETFilePermissions;

/**
 * @brief 디렉토리 열거 옵션
 */
typedef enum {
    ET_DIR_OPTION_NONE = 0,             /**< 기본 옵션 */
    ET_DIR_OPTION_RECURSIVE = 1 << 0,   /**< 재귀적 탐색 */
    ET_DIR_OPTION_INCLUDE_HIDDEN = 1 << 1, /**< 숨김 파일 포함 */
    ET_DIR_OPTION_FOLLOW_SYMLINKS = 1 << 2  /**< 심볼릭 링크 따라가기 */
} ETDirectoryOptions;

// ============================================================================
// 파일시스템 구조체 정의
// ============================================================================

/**
 * @brief 파일 핸들 구조체 (불투명한 포인터)
 */
typedef struct ETFile ETFile;

/**
 * @brief 파일 정보 구조체
 */
typedef struct {
    char name[256];                     /**< 파일 이름 */
    char full_path[1024];               /**< 전체 경로 */
    ETFileType type;                    /**< 파일 타입 */
    uint64_t size;                      /**< 파일 크기 (바이트) */
    uint32_t permissions;               /**< 파일 권한 */
    time_t created_time;                /**< 생성 시간 */
    time_t modified_time;               /**< 수정 시간 */
    time_t accessed_time;               /**< 접근 시간 */
    bool is_hidden;                     /**< 숨김 파일 여부 */
    bool is_readonly;                   /**< 읽기 전용 여부 */
    bool is_system;                     /**< 시스템 파일 여부 */
} ETFileInfo;

/**
 * @brief 디렉토리 항목 구조체
 */
typedef struct {
    char name[256];                     /**< 항목 이름 */
    char path[1024];                    /**< 상대 경로 */
    ETFileType type;                    /**< 항목 타입 */
    uint64_t size;                      /**< 크기 (파일인 경우) */
    time_t modified_time;               /**< 수정 시간 */
    bool is_hidden;                     /**< 숨김 항목 여부 */
} ETDirectoryEntry;

/**
 * @brief 디스크 공간 정보 구조체
 */
typedef struct {
    uint64_t total_space;               /**< 총 공간 (바이트) */
    uint64_t free_space;                /**< 사용 가능한 공간 (바이트) */
    uint64_t used_space;                /**< 사용된 공간 (바이트) */
    char filesystem_type[32];           /**< 파일시스템 타입 */
} ETDiskSpaceInfo;

// ============================================================================
// 파일시스템 인터페이스 정의
// ============================================================================

/**
 * @brief 파일시스템 추상화 인터페이스
 */
typedef struct ETFilesystemInterface {
    // ========================================================================
    // 경로 처리 함수들
    // ========================================================================

    /**
     * @brief 경로를 정규화합니다 (플랫폼별 구분자 통일, 상대경로 해석 등)
     * @param path 원본 경로
     * @param normalized 정규화된 경로를 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*normalize_path)(const char* path, char* normalized, size_t size);

    /**
     * @brief 두 경로를 결합합니다
     * @param base 기본 경로
     * @param relative 상대 경로
     * @param result 결합된 경로를 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*join_path)(const char* base, const char* relative, char* result, size_t size);

    /**
     * @brief 절대 경로를 가져옵니다
     * @param path 상대 경로
     * @param absolute 절대 경로를 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_absolute_path)(const char* path, char* absolute, size_t size);

    /**
     * @brief 경로에서 디렉토리 부분을 추출합니다
     * @param path 전체 경로
     * @param dirname 디렉토리 이름을 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_dirname)(const char* path, char* dirname, size_t size);

    /**
     * @brief 경로에서 파일 이름 부분을 추출합니다
     * @param path 전체 경로
     * @param basename 파일 이름을 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_basename)(const char* path, char* basename, size_t size);

    /**
     * @brief 파일 확장자를 가져옵니다
     * @param path 파일 경로
     * @param extension 확장자를 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_extension)(const char* path, char* extension, size_t size);

    // ========================================================================
    // 파일 I/O 함수들
    // ========================================================================

    /**
     * @brief 파일을 엽니다
     * @param path 파일 경로
     * @param mode 파일 모드
     * @param file 파일 핸들 포인터 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*open_file)(const char* path, uint32_t mode, ETFile** file);

    /**
     * @brief 파일에서 데이터를 읽습니다
     * @param file 파일 핸들
     * @param buffer 데이터를 저장할 버퍼
     * @param size 읽을 바이트 수
     * @param bytes_read 실제로 읽은 바이트 수 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*read_file)(ETFile* file, void* buffer, size_t size, size_t* bytes_read);

    /**
     * @brief 파일에 데이터를 씁니다
     * @param file 파일 핸들
     * @param buffer 쓸 데이터 버퍼
     * @param size 쓸 바이트 수
     * @param bytes_written 실제로 쓴 바이트 수 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*write_file)(ETFile* file, const void* buffer, size_t size, size_t* bytes_written);

    /**
     * @brief 파일 포인터를 이동합니다
     * @param file 파일 핸들
     * @param offset 이동할 오프셋
     * @param origin 기준점
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*seek_file)(ETFile* file, int64_t offset, ETSeekOrigin origin);

    /**
     * @brief 현재 파일 포인터 위치를 가져옵니다
     * @param file 파일 핸들
     * @return 현재 위치 (실패시 -1)
     */
    int64_t (*tell_file)(ETFile* file);

    /**
     * @brief 파일 버퍼를 플러시합니다
     * @param file 파일 핸들
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*flush_file)(ETFile* file);

    /**
     * @brief 파일을 닫습니다
     * @param file 파일 핸들
     */
    void (*close_file)(ETFile* file);

    // ========================================================================
    // 디렉토리 조작 함수들
    // ========================================================================

    /**
     * @brief 디렉토리를 생성합니다
     * @param path 디렉토리 경로
     * @param permissions 디렉토리 권한
     * @param recursive 상위 디렉토리도 함께 생성할지 여부
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_directory)(const char* path, uint32_t permissions, bool recursive);

    /**
     * @brief 디렉토리를 삭제합니다
     * @param path 디렉토리 경로
     * @param recursive 하위 항목도 함께 삭제할지 여부
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*remove_directory)(const char* path, bool recursive);

    /**
     * @brief 디렉토리 내용을 열거합니다
     * @param path 디렉토리 경로
     * @param entries 디렉토리 항목 배열 (출력)
     * @param count 배열 크기 (입력) / 실제 항목 수 (출력)
     * @param options 열거 옵션
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*list_directory)(const char* path, ETDirectoryEntry* entries, int* count, uint32_t options);

    /**
     * @brief 현재 작업 디렉토리를 가져옵니다
     * @param path 현재 디렉토리를 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_current_directory)(char* path, size_t size);

    /**
     * @brief 현재 작업 디렉토리를 변경합니다
     * @param path 새로운 작업 디렉토리 경로
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*set_current_directory)(const char* path);

    // ========================================================================
    // 파일 속성 및 정보 함수들
    // ========================================================================

    /**
     * @brief 파일 정보를 가져옵니다
     * @param path 파일 경로
     * @param info 파일 정보 구조체 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_file_info)(const char* path, ETFileInfo* info);

    /**
     * @brief 파일 권한을 설정합니다
     * @param path 파일 경로
     * @param permissions 새로운 권한
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*set_file_permissions)(const char* path, uint32_t permissions);

    /**
     * @brief 파일 시간을 설정합니다
     * @param path 파일 경로
     * @param access_time 접근 시간 (NULL이면 변경하지 않음)
     * @param modify_time 수정 시간 (NULL이면 변경하지 않음)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*set_file_times)(const char* path, const time_t* access_time, const time_t* modify_time);

    /**
     * @brief 파일이 존재하는지 확인합니다
     * @param path 파일 경로
     * @return 존재하면 true, 아니면 false
     */
    bool (*file_exists)(const char* path);

    /**
     * @brief 디렉토리인지 확인합니다
     * @param path 경로
     * @return 디렉토리이면 true, 아니면 false
     */
    bool (*is_directory)(const char* path);

    /**
     * @brief 일반 파일인지 확인합니다
     * @param path 경로
     * @return 일반 파일이면 true, 아니면 false
     */
    bool (*is_regular_file)(const char* path);

    /**
     * @brief 심볼릭 링크인지 확인합니다
     * @param path 경로
     * @return 심볼릭 링크이면 true, 아니면 false
     */
    bool (*is_symlink)(const char* path);

    // ========================================================================
    // 파일 조작 함수들
    // ========================================================================

    /**
     * @brief 파일을 복사합니다
     * @param source 원본 파일 경로
     * @param destination 대상 파일 경로
     * @param overwrite 덮어쓰기 허용 여부
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*copy_file)(const char* source, const char* destination, bool overwrite);

    /**
     * @brief 파일을 이동합니다 (이름 변경 포함)
     * @param source 원본 파일 경로
     * @param destination 대상 파일 경로
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*move_file)(const char* source, const char* destination);

    /**
     * @brief 파일을 삭제합니다
     * @param path 파일 경로
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*delete_file)(const char* path);

    /**
     * @brief 심볼릭 링크를 생성합니다
     * @param target 링크 대상 경로
     * @param linkpath 링크 파일 경로
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_symlink)(const char* target, const char* linkpath);

    /**
     * @brief 심볼릭 링크의 대상을 읽습니다
     * @param linkpath 링크 파일 경로
     * @param target 대상 경로를 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*read_symlink)(const char* linkpath, char* target, size_t size);

    // ========================================================================
    // 디스크 공간 정보 함수들
    // ========================================================================

    /**
     * @brief 디스크 공간 정보를 가져옵니다
     * @param path 경로 (해당 경로가 속한 디스크의 정보)
     * @param info 디스크 공간 정보 구조체 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_disk_space)(const char* path, ETDiskSpaceInfo* info);

    // ========================================================================
    // 플랫폼별 확장 데이터
    // ========================================================================

    /**
     * @brief 플랫폼별 확장 데이터
     */
    void* platform_data;

} ETFilesystemInterface;

// ============================================================================
// 편의 함수 선언
// ============================================================================

/**
 * @brief 파일시스템 인터페이스를 생성합니다
 * @param interface 생성된 인터페이스 포인터 (출력)
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_create_filesystem_interface(ETFilesystemInterface** interface);

/**
 * @brief 파일시스템 인터페이스를 해제합니다
 * @param interface 해제할 인터페이스
 */
void et_destroy_filesystem_interface(ETFilesystemInterface* interface);

/**
 * @brief 경로 구분자를 가져옵니다 (플랫폼별)
 * @return 경로 구분자 문자
 */
char et_get_path_separator(void);

/**
 * @brief 경로 목록 구분자를 가져옵니다 (플랫폼별)
 * @return 경로 목록 구분자 문자
 */
char et_get_path_list_separator(void);

/**
 * @brief 임시 디렉토리 경로를 가져옵니다
 * @param path 임시 디렉토리 경로를 저장할 버퍼
 * @param size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_temp_directory(char* path, size_t size);

/**
 * @brief 홈 디렉토리 경로를 가져옵니다
 * @param path 홈 디렉토리 경로를 저장할 버퍼
 * @param size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_home_directory(char* path, size_t size);

/**
 * @brief 실행 파일 경로를 가져옵니다
 * @param path 실행 파일 경로를 저장할 버퍼
 * @param size 버퍼 크기
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_get_executable_path(char* path, size_t size);

// ============================================================================
// 상수 정의
// ============================================================================

/** 최대 경로 길이 */
#define ET_MAX_PATH_LENGTH 1024

/** 최대 파일 이름 길이 */
#define ET_MAX_FILENAME_LENGTH 256

/** 기본 디렉토리 권한 */
#define ET_DEFAULT_DIR_PERMISSIONS (ET_PERM_OWNER_READ | ET_PERM_OWNER_WRITE | ET_PERM_OWNER_EXEC | \
                                   ET_PERM_GROUP_READ | ET_PERM_GROUP_EXEC | \
                                   ET_PERM_OTHER_READ | ET_PERM_OTHER_EXEC)

/** 기본 파일 권한 */
#define ET_DEFAULT_FILE_PERMISSIONS (ET_PERM_OWNER_READ | ET_PERM_OWNER_WRITE | \
                                    ET_PERM_GROUP_READ | ET_PERM_OTHER_READ)

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_FILESYSTEM_H