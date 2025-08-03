#ifndef LIBETUDE_WINDOWS_SECURITY_H
#define LIBETUDE_WINDOWS_SECURITY_H

#ifdef _WIN32

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Windows security features status structure
 */
typedef struct {
    bool dep_enabled;           // DEP(Data Execution Prevention) enabled
    bool aslr_enabled;          // ASLR(Address Space Layout Randomization) enabled
    bool large_address_aware;   // Large Address Aware support
} ETWindowsSecurityStatus;

/**
 * @brief Windows secure memory allocator structure
 */
typedef struct {
    void* base_address;         // Base address
    size_t total_size;          // Total size
    size_t allocated_size;      // Allocated size
    HANDLE heap_handle;         // Heap handle
    bool use_large_pages;       // Use Large Pages
} ETWindowsSecureAllocator;

/**
 * @brief Check DEP compatibility
 * @return true if DEP is enabled, false otherwise
 */
bool et_windows_check_dep_compatibility(void);

/**
 * @brief Check ASLR compatibility
 * @return true if ASLR is enabled, false otherwise
 */
bool et_windows_check_aslr_compatibility(void);

/**
 * @brief Get Windows security status
 * @param status Pointer to structure to store security status
 * @return true on success, false on failure
 */
bool et_windows_get_security_status(ETWindowsSecurityStatus* status);

/**
 * @brief Allocate ASLR compatible memory
 * @param size Size of memory to allocate
 * @return Pointer to allocated memory, NULL on failure
 */
void* et_windows_alloc_aslr_compatible(size_t size);

/**
 * @brief Free ASLR compatible memory
 * @param ptr Pointer to memory to free
 */
void et_windows_free_aslr_compatible(void* ptr);

/**
 * @brief 보안 메모리 할당자 초기화
 * @param allocator 초기화할 할당자 구조체 포인터
 * @param initial_size 초기 힙 크기
 * @param use_large_pages Large Page 사용 여부
 * @return 성공 시 true, 실패 시 false
 */
bool et_windows_secure_allocator_init(ETWindowsSecureAllocator* allocator,
                                     size_t initial_size,
                                     bool use_large_pages);

/**
 * @brief 보안 메모리 할당자에서 메모리 할당
 * @param allocator 할당자 구조체 포인터
 * @param size 할당할 메모리 크기
 * @return 할당된 메모리 포인터, 실패 시 NULL
 */
void* et_windows_secure_allocator_alloc(ETWindowsSecureAllocator* allocator,
                                       size_t size);

/**
 * @brief 보안 메모리 할당자에서 메모리 해제
 * @param allocator 할당자 구조체 포인터
 * @param ptr 해제할 메모리 포인터
 */
void et_windows_secure_allocator_free(ETWindowsSecureAllocator* allocator,
                                     void* ptr);

/**
 * @brief 보안 메모리 할당자 정리
 * @param allocator 정리할 할당자 구조체 포인터
 */
void et_windows_secure_allocator_cleanup(ETWindowsSecureAllocator* allocator);

/**
 * @brief 메모리 영역의 실행 권한 제거 (DEP 호환)
 * @param ptr 메모리 포인터
 * @param size 메모리 크기
 * @return 성공 시 true, 실패 시 false
 */
bool et_windows_make_memory_non_executable(void* ptr, size_t size);

/**
 * @brief 메모리 영역의 쓰기 권한 제거
 * @param ptr 메모리 포인터
 * @param size 메모리 크기
 * @return 성공 시 true, 실패 시 false
 */
bool et_windows_make_memory_read_only(void* ptr, size_t size);

/**
 * @brief UAC 권한 레벨 열거형
 */
typedef enum {
    ET_UAC_LEVEL_UNKNOWN = 0,       // 알 수 없음
    ET_UAC_LEVEL_USER = 1,          // 일반 사용자
    ET_UAC_LEVEL_ELEVATED = 2,      // 관리자 권한
    ET_UAC_LEVEL_SYSTEM = 3         // 시스템 권한
} ETUACLevel;

/**
 * @brief UAC 권한 상태 구조체
 */
typedef struct {
    ETUACLevel current_level;       // 현재 권한 레벨
    bool is_admin;                  // 관리자 권한 여부
    bool is_elevated;               // 승격된 권한 여부
    bool uac_enabled;               // UAC 활성화 여부
} ETUACStatus;

/**
 * @brief 기능 제한 모드 설정 구조체
 */
typedef struct {
    bool allow_file_operations;     // 파일 작업 허용
    bool allow_registry_access;     // 레지스트리 접근 허용
    bool allow_network_access;      // 네트워크 접근 허용
    bool allow_hardware_access;     // 하드웨어 접근 허용
    bool allow_system_changes;      // 시스템 변경 허용
} ETRestrictedModeConfig;

/**
 * @brief 현재 프로세스의 UAC 권한 확인
 * @return UAC 권한 레벨
 */
ETUACLevel et_windows_check_uac_level(void);

/**
 * @brief UAC 상태 조회
 * @param status UAC 상태를 저장할 구조체 포인터
 * @return 성공 시 true, 실패 시 false
 */
bool et_windows_get_uac_status(ETUACStatus* status);

/**
 * @brief 관리자 권한 여부 확인
 * @return 관리자 권한이 있으면 true, 그렇지 않으면 false
 */
bool et_windows_is_admin(void);

/**
 * @brief 승격된 권한 여부 확인
 * @return 승격된 권한이 있으면 true, 그렇지 않으면 false
 */
bool et_windows_is_elevated(void);

/**
 * @brief UAC가 활성화되어 있는지 확인
 * @return UAC가 활성화되어 있으면 true, 그렇지 않으면 false
 */
bool et_windows_is_uac_enabled(void);

/**
 * @brief 특정 권한이 필요한 작업 수행 가능 여부 확인
 * @param privilege_name 확인할 권한 이름 (예: SE_DEBUG_NAME)
 * @return 권한이 있으면 true, 그렇지 않으면 false
 */
bool et_windows_check_privilege(const char* privilege_name);

/**
 * @brief 기능 제한 모드 초기화
 * @param config 제한 모드 설정 구조체 포인터
 * @param uac_level 현재 UAC 권한 레벨
 */
void et_windows_init_restricted_mode(ETRestrictedModeConfig* config, ETUACLevel uac_level);

/**
 * @brief 파일 작업 권한 확인
 * @param config 제한 모드 설정 구조체 포인터
 * @param file_path 확인할 파일 경로
 * @return 파일 작업이 허용되면 true, 그렇지 않으면 false
 */
bool et_windows_check_file_access_permission(const ETRestrictedModeConfig* config, const char* file_path);

/**
 * @brief 레지스트리 접근 권한 확인
 * @param config 제한 모드 설정 구조체 포인터
 * @param registry_key 확인할 레지스트리 키
 * @return 레지스트리 접근이 허용되면 true, 그렇지 않으면 false
 */
bool et_windows_check_registry_access_permission(const ETRestrictedModeConfig* config, const char* registry_key);

/**
 * @brief 네트워크 접근 권한 확인
 * @param config 제한 모드 설정 구조체 포인터
 * @return 네트워크 접근이 허용되면 true, 그렇지 않으면 false
 */
bool et_windows_check_network_access_permission(const ETRestrictedModeConfig* config);

/**
 * @brief 하드웨어 접근 권한 확인
 * @param config 제한 모드 설정 구조체 포인터
 * @return 하드웨어 접근이 허용되면 true, 그렇지 않으면 false
 */
bool et_windows_check_hardware_access_permission(const ETRestrictedModeConfig* config);

#ifdef __cplusplus
}
#endif

#endif // _WIN32

#endif // LIBETUDE_WINDOWS_SECURITY_H