#ifdef _WIN32

#include "libetude/platform/windows_security.h"
#include <windows.h>
#include <psapi.h>
#include <stdio.h>

// Windows API 함수 포인터 타입 정의
typedef BOOL (WINAPI *GetProcessDEPPolicyFunc)(HANDLE, LPDWORD, PBOOL);
typedef BOOL (WINAPI *IsWow64ProcessFunc)(HANDLE, PBOOL);

// 전역 변수
static GetProcessDEPPolicyFunc g_GetProcessDEPPolicy = NULL;
static IsWow64ProcessFunc g_IsWow64Process = NULL;
static bool g_api_functions_loaded = false;

/**
 * @brief Windows API 함수들을 동적으로 로드
 */
static void load_windows_api_functions(void) {
    if (g_api_functions_loaded) {
        return;
    }

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32) {
        g_GetProcessDEPPolicy = (GetProcessDEPPolicyFunc)
            GetProcAddress(kernel32, "GetProcessDEPPolicy");
        g_IsWow64Process = (IsWow64ProcessFunc)
            GetProcAddress(kernel32, "IsWow64Process");
    }

    g_api_functions_loaded = true;
}

bool et_windows_check_dep_compatibility(void) {
    load_windows_api_functions();

    // GetProcessDEPPolicy 함수가 없으면 DEP를 지원하지 않는 시스템
    if (!g_GetProcessDEPPolicy) {
        return false;
    }

    DWORD flags = 0;
    BOOL permanent = FALSE;
    HANDLE current_process = GetCurrentProcess();

    if (g_GetProcessDEPPolicy(current_process, &flags, &permanent)) {
        // DEP가 활성화되어 있는지 확인
        return (flags & PROCESS_DEP_ENABLE) != 0;
    }

    return false;
}

bool et_windows_check_aslr_compatibility(void) {
    // ASLR은 Windows Vista 이상에서 지원
    OSVERSIONINFOW version_info;
    ZeroMemory(&version_info, sizeof(OSVERSIONINFOW));
    version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);

    // GetVersionEx는 deprecated이지만 ASLR 확인을 위해 사용
    #pragma warning(push)
    #pragma warning(disable: 4996)
    if (GetVersionExW(&version_info)) {
        // Windows Vista (6.0) 이상에서 ASLR 지원
        if (version_info.dwMajorVersion >= 6) {
            return true;
        }
    }
    #pragma warning(pop)

    return false;
}

bool et_windows_get_security_status(ETWindowsSecurityStatus* status) {
    if (!status) {
        return false;
    }

    ZeroMemory(status, sizeof(ETWindowsSecurityStatus));

    // DEP 상태 확인
    status->dep_enabled = et_windows_check_dep_compatibility();

    // ASLR 상태 확인
    status->aslr_enabled = et_windows_check_aslr_compatibility();

    // Large Address Aware 확인 (64비트 프로세스는 기본적으로 지원)
    #ifdef _WIN64
    status->large_address_aware = true;
    #else
    // 32비트 프로세스에서는 IMAGE_FILE_LARGE_ADDRESS_AWARE 플래그 확인 필요
    // 여기서는 간단히 false로 설정
    status->large_address_aware = false;
    #endif

    return true;
}

void* et_windows_alloc_aslr_compatible(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // ASLR 호환 메모리 할당을 위해 VirtualAlloc 사용
    // NULL을 주소로 전달하면 시스템이 ASLR을 고려하여 주소 선택
    void* ptr = VirtualAlloc(
        NULL,                           // 시스템이 주소 선택 (ASLR 호환)
        size,                          // 할당할 크기
        MEM_COMMIT | MEM_RESERVE,      // 메모리 예약 및 커밋
        PAGE_READWRITE                 // 읽기/쓰기 권한
    );

    if (!ptr) {
        // VirtualAlloc 실패 시 일반 힙 할당으로 폴백
        return HeapAlloc(GetProcessHeap(), 0, size);
    }

    return ptr;
}

void et_windows_free_aslr_compatible(void* ptr) {
    if (!ptr) {
        return;
    }

    // VirtualAlloc으로 할당된 메모리인지 확인
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(ptr, &mbi, sizeof(mbi)) != 0) {
        if (mbi.Type == MEM_PRIVATE && mbi.State == MEM_COMMIT) {
            // VirtualAlloc으로 할당된 메모리
            VirtualFree(ptr, 0, MEM_RELEASE);
            return;
        }
    }

    // 일반 힙 메모리로 간주하고 해제
    HeapFree(GetProcessHeap(), 0, ptr);
}

bool et_windows_secure_allocator_init(ETWindowsSecureAllocator* allocator,
                                     size_t initial_size,
                                     bool use_large_pages) {
    if (!allocator || initial_size == 0) {
        return false;
    }

    ZeroMemory(allocator, sizeof(ETWindowsSecureAllocator));

    DWORD heap_options = 0;
    if (use_large_pages) {
        // Large Page 권한이 있는지 확인
        HANDLE token;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
            TOKEN_PRIVILEGES tp;
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            if (LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid)) {
                AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
                if (GetLastError() == ERROR_SUCCESS) {
                    heap_options = HEAP_CREATE_ENABLE_EXECUTE;
                }
            }
            CloseHandle(token);
        }
    }

    // 전용 힙 생성
    allocator->heap_handle = HeapCreate(heap_options, initial_size, 0);
    if (!allocator->heap_handle) {
        return false;
    }

    allocator->total_size = initial_size;
    allocator->allocated_size = 0;
    allocator->use_large_pages = use_large_pages;

    return true;
}

void* et_windows_secure_allocator_alloc(ETWindowsSecureAllocator* allocator,
                                       size_t size) {
    if (!allocator || !allocator->heap_handle || size == 0) {
        return NULL;
    }

    // 힙에서 메모리 할당
    void* ptr = HeapAlloc(allocator->heap_handle, HEAP_ZERO_MEMORY, size);
    if (ptr) {
        allocator->allocated_size += size;
    }

    return ptr;
}

void et_windows_secure_allocator_free(ETWindowsSecureAllocator* allocator,
                                     void* ptr) {
    if (!allocator || !allocator->heap_handle || !ptr) {
        return;
    }

    // 메모리 크기 조회
    size_t size = HeapSize(allocator->heap_handle, 0, ptr);
    if (size != (size_t)-1) {
        allocator->allocated_size -= size;
    }

    HeapFree(allocator->heap_handle, 0, ptr);
}

void et_windows_secure_allocator_cleanup(ETWindowsSecureAllocator* allocator) {
    if (!allocator) {
        return;
    }

    if (allocator->heap_handle) {
        HeapDestroy(allocator->heap_handle);
        allocator->heap_handle = NULL;
    }

    ZeroMemory(allocator, sizeof(ETWindowsSecureAllocator));
}

bool et_windows_make_memory_non_executable(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return false;
    }

    DWORD old_protect;
    // 메모리 영역을 읽기/쓰기 전용으로 설정 (실행 불가)
    return VirtualProtect(ptr, size, PAGE_READWRITE, &old_protect) != 0;
}

bool et_windows_make_memory_read_only(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return false;
    }

    DWORD old_protect;
    // 메모리 영역을 읽기 전용으로 설정
    return VirtualProtect(ptr, size, PAGE_READONLY, &old_protect) != 0;
}

ETUACLevel et_windows_check_uac_level(void) {
    HANDLE token = NULL;
    TOKEN_ELEVATION_TYPE elevation_type;
    DWORD size = 0;

    // 현재 프로세스의 토큰 열기
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return ET_UAC_LEVEL_UNKNOWN;
    }

    // 승격 타입 조회
    if (!GetTokenInformation(token, TokenElevationType, &elevation_type, sizeof(elevation_type), &size)) {
        CloseHandle(token);
        return ET_UAC_LEVEL_UNKNOWN;
    }

    CloseHandle(token);

    switch (elevation_type) {
        case TokenElevationTypeDefault:
            // UAC가 비활성화되어 있거나 관리자 계정
            return et_windows_is_admin() ? ET_UAC_LEVEL_ELEVATED : ET_UAC_LEVEL_USER;

        case TokenElevationTypeFull:
            // 승격된 관리자 권한
            return ET_UAC_LEVEL_ELEVATED;

        case TokenElevationTypeLimited:
            // 제한된 사용자 권한
            return ET_UAC_LEVEL_USER;

        default:
            return ET_UAC_LEVEL_UNKNOWN;
    }
}

bool et_windows_get_uac_status(ETUACStatus* status) {
    if (!status) {
        return false;
    }

    ZeroMemory(status, sizeof(ETUACStatus));

    // 현재 UAC 레벨 확인
    status->current_level = et_windows_check_uac_level();

    // 관리자 권한 확인
    status->is_admin = et_windows_is_admin();

    // 승격된 권한 확인
    status->is_elevated = et_windows_is_elevated();

    // UAC 활성화 확인
    status->uac_enabled = et_windows_is_uac_enabled();

    return true;
}

bool et_windows_is_admin(void) {
    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;

    // 관리자 그룹 SID 생성
    if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_group)) {

        // 현재 사용자가 관리자 그룹에 속하는지 확인
        if (!CheckTokenMembership(NULL, admin_group, &is_admin)) {
            is_admin = FALSE;
        }

        FreeSid(admin_group);
    }

    return is_admin != FALSE;
}

bool et_windows_is_elevated(void) {
    HANDLE token = NULL;
    TOKEN_ELEVATION elevation;
    DWORD size = 0;

    // 현재 프로세스의 토큰 열기
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    // 승격 상태 조회
    if (!GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        CloseHandle(token);
        return false;
    }

    CloseHandle(token);
    return elevation.TokenIsElevated != 0;
}

bool et_windows_is_uac_enabled(void) {
    HKEY key;
    DWORD value = 0;
    DWORD size = sizeof(DWORD);
    DWORD type = REG_DWORD;

    // UAC 설정 레지스트리 키 열기
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                     0, KEY_READ, &key) != ERROR_SUCCESS) {
        // 레지스트리 접근 실패 시 기본적으로 UAC가 활성화되어 있다고 가정
        return true;
    }

    // EnableLUA 값 조회 (UAC 활성화 여부)
    if (RegQueryValueExA(key, "EnableLUA", NULL, &type, (LPBYTE)&value, &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return true; // 기본값은 활성화
    }

    RegCloseKey(key);
    return value != 0;
}

bool et_windows_check_privilege(const char* privilege_name) {
    if (!privilege_name) {
        return false;
    }

    HANDLE token = NULL;
    LUID privilege_luid;
    PRIVILEGE_SET privilege_set;
    BOOL result = FALSE;

    // 현재 프로세스의 토큰 열기
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    // 권한 이름을 LUID로 변환
    if (!LookupPrivilegeValueA(NULL, privilege_name, &privilege_luid)) {
        CloseHandle(token);
        return false;
    }

    // 권한 확인
    privilege_set.PrivilegeCount = 1;
    privilege_set.Control = PRIVILEGE_SET_ALL_NECESSARY;
    privilege_set.Privilege[0].Luid = privilege_luid;
    privilege_set.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!PrivilegeCheck(token, &privilege_set, &result)) {
        result = FALSE;
    }

    CloseHandle(token);
    return result != FALSE;
}

void et_windows_init_restricted_mode(ETRestrictedModeConfig* config, ETUACLevel uac_level) {
    if (!config) {
        return;
    }

    ZeroMemory(config, sizeof(ETRestrictedModeConfig));

    // UAC 레벨에 따른 기본 권한 설정
    switch (uac_level) {
        case ET_UAC_LEVEL_SYSTEM:
        case ET_UAC_LEVEL_ELEVATED:
            // 관리자 권한 - 모든 기능 허용
            config->allow_file_operations = true;
            config->allow_registry_access = true;
            config->allow_network_access = true;
            config->allow_hardware_access = true;
            config->allow_system_changes = true;
            break;

        case ET_UAC_LEVEL_USER:
            // 일반 사용자 권한 - 제한된 기능만 허용
            config->allow_file_operations = true;   // 사용자 폴더 내 파일만
            config->allow_registry_access = false;  // 레지스트리 접근 제한
            config->allow_network_access = true;    // 네트워크는 허용
            config->allow_hardware_access = false;  // 하드웨어 접근 제한
            config->allow_system_changes = false;   // 시스템 변경 제한
            break;

        case ET_UAC_LEVEL_UNKNOWN:
        default:
            // 알 수 없는 권한 - 최소 권한만 허용
            config->allow_file_operations = false;
            config->allow_registry_access = false;
            config->allow_network_access = false;
            config->allow_hardware_access = false;
            config->allow_system_changes = false;
            break;
    }
}

bool et_windows_check_file_access_permission(const ETRestrictedModeConfig* config, const char* file_path) {
    if (!config || !file_path) {
        return false;
    }

    // 파일 작업이 허용되지 않으면 거부
    if (!config->allow_file_operations) {
        return false;
    }

    // 시스템 디렉토리 접근 확인
    char system_dir[MAX_PATH];
    char windows_dir[MAX_PATH];

    GetSystemDirectoryA(system_dir, MAX_PATH);
    GetWindowsDirectoryA(windows_dir, MAX_PATH);

    // 시스템 디렉토리나 Windows 디렉토리 접근 시 시스템 변경 권한 필요
    if (strstr(file_path, system_dir) == file_path || strstr(file_path, windows_dir) == file_path) {
        return config->allow_system_changes;
    }

    // Program Files 디렉토리 접근 확인
    char program_files[MAX_PATH];
    if (GetEnvironmentVariableA("ProgramFiles", program_files, MAX_PATH) > 0) {
        if (strstr(file_path, program_files) == file_path) {
            return config->allow_system_changes;
        }
    }

    // 일반 파일 접근은 허용
    return true;
}

bool et_windows_check_registry_access_permission(const ETRestrictedModeConfig* config, const char* registry_key) {
    if (!config || !registry_key) {
        return false;
    }

    // 레지스트리 접근이 허용되지 않으면 거부
    if (!config->allow_registry_access) {
        return false;
    }

    // HKEY_LOCAL_MACHINE의 시스템 키 접근 시 시스템 변경 권한 필요
    if (strstr(registry_key, "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows") == registry_key ||
        strstr(registry_key, "HKEY_LOCAL_MACHINE\\SYSTEM") == registry_key) {
        return config->allow_system_changes;
    }

    return true;
}

bool et_windows_check_network_access_permission(const ETRestrictedModeConfig* config) {
    if (!config) {
        return false;
    }

    return config->allow_network_access;
}

bool et_windows_check_hardware_access_permission(const ETRestrictedModeConfig* config) {
    if (!config) {
        return false;
    }

    return config->allow_hardware_access;
}

#endif // _WIN32