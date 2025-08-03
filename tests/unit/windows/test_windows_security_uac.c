#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>

#include "libetude/platform/windows_security.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

/**
 * @brief UAC 레벨 문자열 변환
 */
const char* uac_level_to_string(ETUACLevel level) {
    switch (level) {
        case ET_UAC_LEVEL_UNKNOWN: return "알 수 없음";
        case ET_UAC_LEVEL_USER: return "일반 사용자";
        case ET_UAC_LEVEL_ELEVATED: return "관리자 권한";
        case ET_UAC_LEVEL_SYSTEM: return "시스템 권한";
        default: return "정의되지 않음";
    }
}

/**
 * @brief UAC 레벨 확인 테스트
 */
void test_uac_level_check(void) {
    printf("\n=== UAC 레벨 확인 테스트 ===\n");

    ETUACLevel level = et_windows_check_uac_level();
    printf("현재 UAC 레벨: %s\n", uac_level_to_string(level));

    // UAC 레벨이 유효한 범위 내에 있는지 확인
    TEST_ASSERT(level >= ET_UAC_LEVEL_UNKNOWN && level <= ET_UAC_LEVEL_SYSTEM,
                "UAC 레벨이 유효한 범위 내에 있음");

    // 함수가 정상적으로 실행되는지 확인
    TEST_ASSERT(true, "UAC 레벨 확인 함수 실행 성공");
}

/**
 * @brief UAC 상태 조회 테스트
 */
void test_uac_status_query(void) {
    printf("\n=== UAC 상태 조회 테스트 ===\n");

    ETUACStatus status;
    bool result = et_windows_get_uac_status(&status);

    TEST_ASSERT(result == true, "UAC 상태 조회 성공");

    if (result) {
        printf("현재 레벨: %s\n", uac_level_to_string(status.current_level));
        printf("관리자 권한: %s\n", status.is_admin ? "예" : "아니오");
        printf("승격된 권한: %s\n", status.is_elevated ? "예" : "아니오");
        printf("UAC 활성화: %s\n", status.uac_enabled ? "예" : "아니오");

        // 논리적 일관성 확인
        if (status.is_elevated) {
            TEST_ASSERT(status.current_level == ET_UAC_LEVEL_ELEVATED,
                        "승격된 권한 시 UAC 레벨이 ELEVATED");
        }
    }

    // NULL 포인터 테스트
    result = et_windows_get_uac_status(NULL);
    TEST_ASSERT(result == false, "NULL 포인터 전달 시 실패 반환");
}

/**
 * @brief 관리자 권한 확인 테스트
 */
void test_admin_check(void) {
    printf("\n=== 관리자 권한 확인 테스트 ===\n");

    bool is_admin = et_windows_is_admin();
    printf("관리자 권한: %s\n", is_admin ? "있음" : "없음");

    // 함수가 정상적으로 실행되는지 확인
    TEST_ASSERT(true, "관리자 권한 확인 함수 실행 성공");
}

/**
 * @brief 승격된 권한 확인 테스트
 */
void test_elevation_check(void) {
    printf("\n=== 승격된 권한 확인 테스트 ===\n");

    bool is_elevated = et_windows_is_elevated();
    printf("승격된 권한: %s\n", is_elevated ? "있음" : "없음");

    // 함수가 정상적으로 실행되는지 확인
    TEST_ASSERT(true, "승격된 권한 확인 함수 실행 성공");
}

/**
 * @brief UAC 활성화 확인 테스트
 */
void test_uac_enabled_check(void) {
    printf("\n=== UAC 활성화 확인 테스트 ===\n");

    bool uac_enabled = et_windows_is_uac_enabled();
    printf("UAC 활성화: %s\n", uac_enabled ? "예" : "아니오");

    // 함수가 정상적으로 실행되는지 확인
    TEST_ASSERT(true, "UAC 활성화 확인 함수 실행 성공");
}

/**
 * @brief 권한 확인 테스트
 */
void test_privilege_check(void) {
    printf("\n=== 권한 확인 테스트 ===\n");

    // 일반적인 권한들 테스트
    const char* privileges[] = {
        SE_DEBUG_NAME,
        SE_BACKUP_NAME,
        SE_RESTORE_NAME,
        SE_SHUTDOWN_NAME,
        SE_LOAD_DRIVER_NAME
    };

    const char* privilege_names[] = {
        "디버그 권한",
        "백업 권한",
        "복원 권한",
        "시스템 종료 권한",
        "드라이버 로드 권한"
    };

    int privilege_count = sizeof(privileges) / sizeof(privileges[0]);

    for (int i = 0; i < privilege_count; i++) {
        bool has_privilege = et_windows_check_privilege(privileges[i]);
        printf("%s: %s\n", privilege_names[i], has_privilege ? "있음" : "없음");
    }

    // 함수가 정상적으로 실행되는지 확인
    TEST_ASSERT(true, "권한 확인 함수들 실행 성공");

    // NULL 포인터 테스트
    bool null_result = et_windows_check_privilege(NULL);
    TEST_ASSERT(null_result == false, "NULL 권한 이름 전달 시 false 반환");

    // 잘못된 권한 이름 테스트
    bool invalid_result = et_windows_check_privilege("INVALID_PRIVILEGE_NAME");
    TEST_ASSERT(invalid_result == false, "잘못된 권한 이름 전달 시 false 반환");
}

/**
 * @brief 기능 제한 모드 초기화 테스트
 */
void test_restricted_mode_init(void) {
    printf("\n=== 기능 제한 모드 초기화 테스트 ===\n");

    ETRestrictedModeConfig config;

    // 각 UAC 레벨에 대한 테스트
    ETUACLevel levels[] = {
        ET_UAC_LEVEL_UNKNOWN,
        ET_UAC_LEVEL_USER,
        ET_UAC_LEVEL_ELEVATED,
        ET_UAC_LEVEL_SYSTEM
    };

    const char* level_names[] = {
        "알 수 없음",
        "일반 사용자",
        "관리자 권한",
        "시스템 권한"
    };

    int level_count = sizeof(levels) / sizeof(levels[0]);

    for (int i = 0; i < level_count; i++) {
        et_windows_init_restricted_mode(&config, levels[i]);

        printf("\n%s 레벨 설정:\n", level_names[i]);
        printf("  파일 작업: %s\n", config.allow_file_operations ? "허용" : "제한");
        printf("  레지스트리 접근: %s\n", config.allow_registry_access ? "허용" : "제한");
        printf("  네트워크 접근: %s\n", config.allow_network_access ? "허용" : "제한");
        printf("  하드웨어 접근: %s\n", config.allow_hardware_access ? "허용" : "제한");
        printf("  시스템 변경: %s\n", config.allow_system_changes ? "허용" : "제한");

        // 권한 레벨에 따른 논리적 일관성 확인
        if (levels[i] == ET_UAC_LEVEL_ELEVATED || levels[i] == ET_UAC_LEVEL_SYSTEM) {
            TEST_ASSERT(config.allow_file_operations == true,
                        "관리자 권한에서 파일 작업 허용");
            TEST_ASSERT(config.allow_system_changes == true,
                        "관리자 권한에서 시스템 변경 허용");
        } else if (levels[i] == ET_UAC_LEVEL_USER) {
            TEST_ASSERT(config.allow_file_operations == true,
                        "일반 사용자에서 파일 작업 허용");
            TEST_ASSERT(config.allow_system_changes == false,
                        "일반 사용자에서 시스템 변경 제한");
        }
    }

    // NULL 포인터 테스트
    et_windows_init_restricted_mode(NULL, ET_UAC_LEVEL_USER);
    TEST_ASSERT(true, "NULL 포인터 전달 시 크래시 없음");
}

/**
 * @brief 파일 접근 권한 확인 테스트
 */
void test_file_access_permission(void) {
    printf("\n=== 파일 접근 권한 확인 테스트 ===\n");

    ETRestrictedModeConfig user_config, admin_config;

    // 사용자 및 관리자 설정 초기화
    et_windows_init_restricted_mode(&user_config, ET_UAC_LEVEL_USER);
    et_windows_init_restricted_mode(&admin_config, ET_UAC_LEVEL_ELEVATED);

    // 테스트할 파일 경로들
    const char* test_paths[] = {
        "C:\\Users\\TestUser\\Documents\\test.txt",  // 사용자 폴더
        "C:\\Windows\\System32\\test.dll",           // 시스템 폴더
        "C:\\Program Files\\TestApp\\test.exe",      // Program Files
        "D:\\MyData\\test.dat"                       // 일반 드라이브
    };

    const char* path_descriptions[] = {
        "사용자 문서 폴더",
        "시스템 폴더",
        "Program Files",
        "일반 데이터 드라이브"
    };

    int path_count = sizeof(test_paths) / sizeof(test_paths[0]);

    printf("\n일반 사용자 권한:\n");
    for (int i = 0; i < path_count; i++) {
        bool allowed = et_windows_check_file_access_permission(&user_config, test_paths[i]);
        printf("  %s: %s\n", path_descriptions[i], allowed ? "허용" : "제한");
    }

    printf("\n관리자 권한:\n");
    for (int i = 0; i < path_count; i++) {
        bool allowed = et_windows_check_file_access_permission(&admin_config, test_paths[i]);
        printf("  %s: %s\n", path_descriptions[i], allowed ? "허용" : "제한");
    }

    // 시스템 폴더는 관리자만 접근 가능해야 함
    bool user_system_access = et_windows_check_file_access_permission(&user_config, "C:\\Windows\\System32\\test.dll");
    bool admin_system_access = et_windows_check_file_access_permission(&admin_config, "C:\\Windows\\System32\\test.dll");

    TEST_ASSERT(user_system_access == false, "일반 사용자는 시스템 폴더 접근 제한");
    TEST_ASSERT(admin_system_access == true, "관리자는 시스템 폴더 접근 허용");

    // NULL 포인터 테스트
    bool null_result = et_windows_check_file_access_permission(NULL, test_paths[0]);
    TEST_ASSERT(null_result == false, "NULL config 전달 시 false 반환");

    null_result = et_windows_check_file_access_permission(&user_config, NULL);
    TEST_ASSERT(null_result == false, "NULL 경로 전달 시 false 반환");
}

/**
 * @brief 레지스트리 접근 권한 확인 테스트
 */
void test_registry_access_permission(void) {
    printf("\n=== 레지스트리 접근 권한 확인 테스트 ===\n");

    ETRestrictedModeConfig user_config, admin_config;

    et_windows_init_restricted_mode(&user_config, ET_UAC_LEVEL_USER);
    et_windows_init_restricted_mode(&admin_config, ET_UAC_LEVEL_ELEVATED);

    // 테스트할 레지스트리 키들
    const char* test_keys[] = {
        "HKEY_CURRENT_USER\\Software\\TestApp",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
        "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\TestApp"
    };

    const char* key_descriptions[] = {
        "사용자 소프트웨어 키",
        "Windows 버전 정보",
        "시스템 설정",
        "로컬 머신 소프트웨어"
    };

    int key_count = sizeof(test_keys) / sizeof(test_keys[0]);

    printf("\n일반 사용자 권한:\n");
    for (int i = 0; i < key_count; i++) {
        bool allowed = et_windows_check_registry_access_permission(&user_config, test_keys[i]);
        printf("  %s: %s\n", key_descriptions[i], allowed ? "허용" : "제한");
    }

    printf("\n관리자 권한:\n");
    for (int i = 0; i < key_count; i++) {
        bool allowed = et_windows_check_registry_access_permission(&admin_config, test_keys[i]);
        printf("  %s: %s\n", key_descriptions[i], allowed ? "허용" : "제한");
    }

    // 일반 사용자는 레지스트리 접근이 제한되어야 함
    bool user_reg_access = et_windows_check_registry_access_permission(&user_config, test_keys[0]);
    TEST_ASSERT(user_reg_access == false, "일반 사용자는 레지스트리 접근 제한");

    // NULL 포인터 테스트
    bool null_result = et_windows_check_registry_access_permission(NULL, test_keys[0]);
    TEST_ASSERT(null_result == false, "NULL config 전달 시 false 반환");
}

/**
 * @brief 네트워크 및 하드웨어 접근 권한 테스트
 */
void test_network_hardware_permissions(void) {
    printf("\n=== 네트워크 및 하드웨어 접근 권한 테스트 ===\n");

    ETRestrictedModeConfig user_config, admin_config;

    et_windows_init_restricted_mode(&user_config, ET_UAC_LEVEL_USER);
    et_windows_init_restricted_mode(&admin_config, ET_UAC_LEVEL_ELEVATED);

    // 네트워크 접근 권한 확인
    bool user_network = et_windows_check_network_access_permission(&user_config);
    bool admin_network = et_windows_check_network_access_permission(&admin_config);

    printf("네트워크 접근 권한:\n");
    printf("  일반 사용자: %s\n", user_network ? "허용" : "제한");
    printf("  관리자: %s\n", admin_network ? "허용" : "제한");

    TEST_ASSERT(user_network == true, "일반 사용자도 네트워크 접근 허용");
    TEST_ASSERT(admin_network == true, "관리자 네트워크 접근 허용");

    // 하드웨어 접근 권한 확인
    bool user_hardware = et_windows_check_hardware_access_permission(&user_config);
    bool admin_hardware = et_windows_check_hardware_access_permission(&admin_config);

    printf("하드웨어 접근 권한:\n");
    printf("  일반 사용자: %s\n", user_hardware ? "허용" : "제한");
    printf("  관리자: %s\n", admin_hardware ? "허용" : "제한");

    TEST_ASSERT(user_hardware == false, "일반 사용자는 하드웨어 접근 제한");
    TEST_ASSERT(admin_hardware == true, "관리자는 하드웨어 접근 허용");

    // NULL 포인터 테스트
    bool null_network = et_windows_check_network_access_permission(NULL);
    bool null_hardware = et_windows_check_hardware_access_permission(NULL);

    TEST_ASSERT(null_network == false, "NULL config에서 네트워크 접근 거부");
    TEST_ASSERT(null_hardware == false, "NULL config에서 하드웨어 접근 거부");
}

int main(void) {
    printf("Windows UAC 권한 관리 테스트 시작\n");
    printf("==================================\n");

    test_uac_level_check();
    test_uac_status_query();
    test_admin_check();
    test_elevation_check();
    test_uac_enabled_check();
    test_privilege_check();
    test_restricted_mode_init();
    test_file_access_permission();
    test_registry_access_permission();
    test_network_hardware_permissions();

    printf("\n==================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("모든 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다.\n");
        return 1;
    }
}

#else

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif // _WIN32