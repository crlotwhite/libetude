/**
 * @file test_windows_deployment_validation.c
 * @brief Windows 배포 시스템 검증 테스트
 * @author LibEtude Project
 * @version 1.0.0
 *
 * NuGet 패키지 생성 및 CMake 통합 테스트 구현
 * 배포 파일 무결성 및 의존성 검증
 * Requirements: 5.2, 5.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include "libetude/platform/windows.h"
#include "libetude/error.h"

/* 테스트 결과 구조체 */
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
} TestResults;

static TestResults g_test_results = { 0 };

/* 배포 검증 결과 */
typedef struct {
    bool nuget_package_valid;
    bool cmake_config_valid;
    bool dependencies_satisfied;
    bool file_integrity_ok;
    int missing_files_count;
    int invalid_files_count;
} DeploymentValidation;

static DeploymentValidation g_deployment = { 0 };

/* 테스트 매크로 */
#define TEST_START(name) \
    do { \
        printf("테스트 시작: %s\n", name); \
        g_test_results.total_tests++; \
    } while(0)

#define TEST_PASS(name) \
    do { \
        printf("  ✓ %s 통과\n", name); \
        g_test_results.passed_tests++; \
    } while(0)

#define TEST_FAIL(name, reason) \
    do { \
        printf("  ✗ %s 실패: %s\n", name, reason); \
        g_test_results.failed_tests++; \
    } while(0)

#define TEST_SKIP(name, reason) \
    do { \
        printf("  ⚠ %s 건너뜀: %s\n", name, reason); \
        g_test_results.skipped_tests++; \
    } while(0)

/**
 * @brief 파일 존재 및 크기 확인
 */
static bool validate_file(const char* filepath, size_t min_size) {
    HANDLE file = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER file_size;
    bool result = GetFileSizeEx(file, &file_size);
    CloseHandle(file);

    if (!result) {
        return false;
    }

    return (file_size.QuadPart >= (LONGLONG)min_size);
}

/**
 * @brief XML 파일 기본 구문 검증
 */
static bool validate_xml_syntax(const char* xml_filepath) {
    FILE* file = fopen(xml_filepath, "r");
    if (!file) {
        return false;
    }

    char buffer[1024];
    bool has_xml_declaration = false;
    bool has_root_element = false;
    int open_tags = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        /* XML 선언 확인 */
        if (strstr(buffer, "<?xml")) {
            has_xml_declaration = true;
        }

        /* 루트 엘리먼트 확인 */
        if (strstr(buffer, "<package") || strstr(buffer, "<Project")) {
            has_root_element = true;
        }

        /* 간단한 태그 균형 확인 */
        char* pos = buffer;
        while ((pos = strchr(pos, '<')) != NULL) {
            if (pos[1] == '/') {
                open_tags--;
            } else if (pos[1] != '?' && pos[1] != '!') {
                /* 자체 닫는 태그 확인 */
                char* end = strchr(pos, '>');
                if (end && end[-1] != '/') {
                    open_tags++;
                }
            }
            pos++;
        }
    }

    fclose(file);

    return has_xml_declaration && has_root_element && (open_tags == 0);
}

/**
 * @brief NuGet 패키지 구조 검증
 * Requirements: 5.2
 */
static void test_nuget_package_structure_validation(void) {
    TEST_START("NuGet 패키지 구조 검증");

    /* NuGet 패키지 관련 파일 확인 */
    const char* nuget_files[] = {
        "..\\packaging\\nuget\\LibEtude.nuspec",
        "..\\packaging\\nuget\\LibEtude.targets",
        "..\\packaging\\nuget\\LibEtude.props"
    };

    int nuget_file_count = sizeof(nuget_files) / sizeof(nuget_files[0]);
    int valid_files = 0;

    for (int i = 0; i < nuget_file_count; i++) {
        if (validate_file(nuget_files[i], 100)) { /* 최소 100바이트 */
            valid_files++;
            printf("    ✓ 파일 유효: %s\n", nuget_files[i]);

            /* XML 파일 구문 검증 */
            if (strstr(nuget_files[i], ".nuspec") ||
                strstr(nuget_files[i], ".targets") ||
                strstr(nuget_files[i], ".props")) {

                if (validate_xml_syntax(nuget_files[i])) {
                    printf("      ✓ XML 구문 유효\n");
                } else {
                    printf("      ⚠ XML 구문 검증 실패\n");
                    g_deployment.invalid_files_count++;
                }
            }
        } else {
            printf("    ✗ 파일 없음 또는 무효: %s\n", nuget_files[i]);
            g_deployment.missing_files_count++;
        }
    }

    if (valid_files == nuget_file_count) {
        TEST_PASS("NuGet 패키지 파일 구조");
        g_deployment.nuget_package_valid = true;
    } else {
        TEST_FAIL("NuGet 패키지 파일 구조", "일부 파일이 없거나 무효함");
    }

    /* nuspec 파일 내용 검증 */
    FILE* nuspec_file = fopen("..\\packaging\\nuget\\LibEtude.nuspec", "r");
    if (nuspec_file) {
        char buffer[1024];
        bool has_id = false;
        bool has_version = false;
        bool has_authors = false;
        bool has_description = false;

        while (fgets(buffer, sizeof(buffer), nuspec_file)) {
            if (strstr(buffer, "<id>")) has_id = true;
            if (strstr(buffer, "<version>")) has_version = true;
            if (strstr(buffer, "<authors>")) has_authors = true;
            if (strstr(buffer, "<description>")) has_description = true;
        }

        fclose(nuspec_file);

        if (has_id && has_version && has_authors && has_description) {
            TEST_PASS("nuspec 파일 필수 메타데이터");
        } else {
            TEST_FAIL("nuspec 파일 필수 메타데이터", "필수 메타데이터 누락");
        }
    } else {
        TEST_SKIP("nuspec 파일 내용 검증", "파일을 열 수 없음");
    }
}

/**
 * @brief CMake 설정 파일 검증
 * Requirements: 5.3
 */
static void test_cmake_config_validation(void) {
    TEST_START("CMake 설정 파일 검증");

    /* CMake 설정 파일 확인 */
    const char* cmake_files[] = {
        "..\\cmake\\LibEtudeConfig.cmake.in",
        "..\\cmake\\LibEtudeConfigVersion.cmake.in",
        "..\\cmake\\WindowsConfig.cmake"
    };

    int cmake_file_count = sizeof(cmake_files) / sizeof(cmake_files[0]);
    int valid_cmake_files = 0;

    for (int i = 0; i < cmake_file_count; i++) {
        if (validate_file(cmake_files[i], 50)) { /* 최소 50바이트 */
            valid_cmake_files++;
            printf("    ✓ CMake 파일 유효: %s\n", cmake_files[i]);

            /* CMake 파일 내용 기본 검증 */
            FILE* cmake_file = fopen(cmake_files[i], "r");
            if (cmake_file) {
                char buffer[1024];
                bool has_cmake_content = false;

                while (fgets(buffer, sizeof(buffer), cmake_file)) {
                    /* CMake 명령어 또는 변수 확인 */
                    if (strstr(buffer, "set(") ||
                        strstr(buffer, "find_") ||
                        strstr(buffer, "target_") ||
                        strstr(buffer, "CMAKE_") ||
                        strstr(buffer, "LIBETUDE_")) {
                        has_cmake_content = true;
                        break;
                    }
                }

                fclose(cmake_file);

                if (has_cmake_content) {
                    printf("      ✓ CMake 내용 유효\n");
                } else {
                    printf("      ⚠ CMake 내용 검증 실패\n");
                    g_deployment.invalid_files_count++;
                }
            }
        } else {
            printf("    ✗ CMake 파일 없음 또는 무효: %s\n", cmake_files[i]);
            g_deployment.missing_files_count++;
        }
    }

    if (valid_cmake_files == cmake_file_count) {
        TEST_PASS("CMake 설정 파일 구조");
        g_deployment.cmake_config_valid = true;
    } else {
        TEST_FAIL("CMake 설정 파일 구조", "일부 CMake 파일이 없거나 무효함");
    }

    /* LibEtudeConfig.cmake.in 특별 검증 */
    FILE* config_file = fopen("..\\cmake\\LibEtudeConfig.cmake.in", "r");
    if (config_file) {
        char buffer[1024];
        bool has_version_var = false;
        bool has_include_dirs = false;
        bool has_libraries = false;
        bool has_windows_libs = false;

        while (fgets(buffer, sizeof(buffer), config_file)) {
            if (strstr(buffer, "LIBETUDE_VERSION")) has_version_var = true;
            if (strstr(buffer, "LIBETUDE_INCLUDE_DIRS")) has_include_dirs = true;
            if (strstr(buffer, "LIBETUDE_LIBRARIES")) has_libraries = true;
            if (strstr(buffer, "LIBETUDE_WINDOWS_LIBRARIES")) has_windows_libs = true;
        }

        fclose(config_file);

        if (has_version_var && has_include_dirs && has_libraries && has_windows_libs) {
            TEST_PASS("LibEtudeConfig.cmake.in 필수 변수");
        } else {
            TEST_FAIL("LibEtudeConfig.cmake.in 필수 변수", "필수 CMake 변수 누락");
        }
    } else {
        TEST_SKIP("LibEtudeConfig.cmake.in 내용 검증", "파일을 열 수 없음");
    }
}

/**
 * @brief 의존성 검증
 * Requirements: 5.2, 5.3
 */
static void test_dependency_validation(void) {
    TEST_START("의존성 검증");

    /* Windows SDK 의존성 확인 */
    const char* required_headers[] = {
        "windows.h",
        "mmdeviceapi.h",
        "audioclient.h",
        "dsound.h",
        "winmm.h"
    };

    int header_count = sizeof(required_headers) / sizeof(required_headers[0]);
    int found_headers = 0;

    printf("    Windows SDK 헤더 확인:\n");
    for (int i = 0; i < header_count; i++) {
        /* 간단한 컴파일 테스트로 헤더 존재 확인 */
        char test_file[MAX_PATH];
        GetTempPathA(MAX_PATH, test_file);
        strcat_s(test_file, MAX_PATH, "header_test.c");

        FILE* test_fp = fopen(test_file, "w");
        if (test_fp) {
            fprintf(test_fp, "#include <%s>\n", required_headers[i]);
            fprintf(test_fp, "int main() { return 0; }\n");
            fclose(test_fp);

            /* 컴파일 시도 */
            char compile_cmd[512];
            snprintf(compile_cmd, sizeof(compile_cmd),
                     "cl /nologo /c \"%s\" >nul 2>&1", test_file);

            if (system(compile_cmd) == 0) {
                found_headers++;
                printf("      ✓ %s\n", required_headers[i]);
            } else {
                printf("      ✗ %s\n", required_headers[i]);
            }

            /* 임시 파일 정리 */
            DeleteFileA(test_file);

            char obj_file[MAX_PATH];
            GetTempPathA(MAX_PATH, obj_file);
            strcat_s(obj_file, MAX_PATH, "header_test.obj");
            DeleteFileA(obj_file);
        }
    }

    if (found_headers >= header_count - 1) { /* 1개 정도는 누락되어도 허용 */
        TEST_PASS("Windows SDK 헤더 의존성");
    } else {
        TEST_FAIL("Windows SDK 헤더 의존성", "필수 헤더가 너무 많이 누락됨");
    }

    /* 시스템 라이브러리 의존성 확인 */
    const char* required_libs[] = {
        "kernel32.lib",
        "user32.lib",
        "ole32.lib",
        "oleaut32.lib",
        "uuid.lib",
        "winmm.lib"
    };

    int lib_count = sizeof(required_libs) / sizeof(required_libs[0]);
    int found_libs = 0;

    printf("    시스템 라이브러리 확인:\n");
    for (int i = 0; i < lib_count; i++) {
        /* LIB 환경 변수에서 라이브러리 검색 */
        char* lib_paths = getenv("LIB");
        bool lib_found = false;

        if (lib_paths) {
            char* lib_paths_copy = _strdup(lib_paths);
            char* token = strtok(lib_paths_copy, ";");

            while (token && !lib_found) {
                char lib_path[MAX_PATH];
                snprintf(lib_path, sizeof(lib_path), "%s\\%s", token, required_libs[i]);

                if (GetFileAttributesA(lib_path) != INVALID_FILE_ATTRIBUTES) {
                    lib_found = true;
                    found_libs++;
                }

                token = strtok(NULL, ";");
            }

            free(lib_paths_copy);
        }

        if (lib_found) {
            printf("      ✓ %s\n", required_libs[i]);
        } else {
            printf("      ✗ %s\n", required_libs[i]);
        }
    }

    if (found_libs >= lib_count - 1) { /* 1개 정도는 누락되어도 허용 */
        TEST_PASS("시스템 라이브러리 의존성");
        g_deployment.dependencies_satisfied = true;
    } else {
        TEST_FAIL("시스템 라이브러리 의존성", "필수 라이브러리가 너무 많이 누락됨");
    }
}

/**
 * @brief 배포 파일 무결성 검증
 * Requirements: 5.2, 5.3
 */
static void test_deployment_file_integrity(void) {
    TEST_START("배포 파일 무결성 검증");

    /* 프로젝트 루트 파일 확인 */
    const char* root_files[] = {
        "..\\CMakeLists.txt",
        "..\\README.md",
        "..\\LICENSE"
    };

    int root_file_count = sizeof(root_files) / sizeof(root_files[0]);
    int valid_root_files = 0;

    printf("    프로젝트 루트 파일 확인:\n");
    for (int i = 0; i < root_file_count; i++) {
        if (validate_file(root_files[i], 10)) { /* 최소 10바이트 */
            valid_root_files++;
            printf("      ✓ %s\n", root_files[i]);
        } else {
            printf("      ✗ %s\n", root_files[i]);
            g_deployment.missing_files_count++;
        }
    }

    if (valid_root_files == root_file_count) {
        TEST_PASS("프로젝트 루트 파일");
    } else {
        TEST_FAIL("프로젝트 루트 파일", "일부 루트 파일이 누락됨");
    }

    /* 헤더 파일 구조 확인 */
    const char* header_dirs[] = {
        "..\\include\\libetude"
    };

    int header_dir_count = sizeof(header_dirs) / sizeof(header_dirs[0]);
    int valid_header_dirs = 0;

    printf("    헤더 파일 디렉토리 확인:\n");
    for (int i = 0; i < header_dir_count; i++) {
        DWORD attrs = GetFileAttributesA(header_dirs[i]);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            valid_header_dirs++;
            printf("      ✓ %s\n", header_dirs[i]);

            /* 디렉토리 내 헤더 파일 개수 확인 */
            char search_pattern[MAX_PATH];
            snprintf(search_pattern, sizeof(search_pattern), "%s\\*.h", header_dirs[i]);

            WIN32_FIND_DATAA find_data;
            HANDLE find_handle = FindFirstFileA(search_pattern, &find_data);

            int header_count = 0;
            if (find_handle != INVALID_HANDLE_VALUE) {
                do {
                    if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        header_count++;
                    }
                } while (FindNextFileA(find_handle, &find_data));

                FindClose(find_handle);
            }

            printf("        헤더 파일 개수: %d\n", header_count);

            if (header_count > 0) {
                printf("        ✓ 헤더 파일 존재\n");
            } else {
                printf("        ⚠ 헤더 파일 없음\n");
                g_deployment.missing_files_count++;
            }
        } else {
            printf("      ✗ %s\n", header_dirs[i]);
            g_deployment.missing_files_count++;
        }
    }

    if (valid_header_dirs == header_dir_count) {
        TEST_PASS("헤더 파일 구조");
    } else {
        TEST_FAIL("헤더 파일 구조", "헤더 디렉토리가 누락됨");
    }

    /* 소스 파일 구조 확인 */
    const char* source_dirs[] = {
        "..\\src",
        "..\\src\\core",
        "..\\src\\platform\\windows"
    };

    int source_dir_count = sizeof(source_dirs) / sizeof(source_dirs[0]);
    int valid_source_dirs = 0;

    printf("    소스 파일 디렉토리 확인:\n");
    for (int i = 0; i < source_dir_count; i++) {
        DWORD attrs = GetFileAttributesA(source_dirs[i]);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            valid_source_dirs++;
            printf("      ✓ %s\n", source_dirs[i]);
        } else {
            printf("      ✗ %s\n", source_dirs[i]);
            g_deployment.missing_files_count++;
        }
    }

    if (valid_source_dirs == source_dir_count) {
        TEST_PASS("소스 파일 구조");
        g_deployment.file_integrity_ok = true;
    } else {
        TEST_FAIL("소스 파일 구조", "소스 디렉토리가 누락됨");
    }
}

/**
 * @brief 배포 스크립트 실행 가능성 테스트
 * Requirements: 5.2
 */
static void test_deployment_script_executability(void) {
    TEST_START("배포 스크립트 실행 가능성");

    /* 배포 스크립트 파일 확인 */
    const char* deployment_scripts[] = {
        "..\\scripts\\build_nuget.bat",
        "..\\scripts\\build_nuget_multiplatform.bat",
        "..\\scripts\\validate_nuget_dependencies.bat"
    };

    int script_count = sizeof(deployment_scripts) / sizeof(deployment_scripts[0]);
    int executable_scripts = 0;

    for (int i = 0; i < script_count; i++) {
        if (validate_file(deployment_scripts[i], 100)) { /* 최소 100바이트 */
            executable_scripts++;
            printf("    ✓ 스크립트 존재: %s\n", deployment_scripts[i]);

            /* 스크립트 내용 기본 검증 */
            FILE* script_file = fopen(deployment_scripts[i], "r");
            if (script_file) {
                char buffer[1024];
                bool has_batch_content = false;

                while (fgets(buffer, sizeof(buffer), script_file)) {
                    if (strstr(buffer, "@echo off") ||
                        strstr(buffer, "setlocal") ||
                        strstr(buffer, "echo ") ||
                        strstr(buffer, "set ")) {
                        has_batch_content = true;
                        break;
                    }
                }

                fclose(script_file);

                if (has_batch_content) {
                    printf("      ✓ 배치 스크립트 내용 유효\n");
                } else {
                    printf("      ⚠ 배치 스크립트 내용 검증 실패\n");
                    g_deployment.invalid_files_count++;
                }
            }
        } else {
            printf("    ✗ 스크립트 없음 또는 무효: %s\n", deployment_scripts[i]);
            g_deployment.missing_files_count++;
        }
    }

    if (executable_scripts == script_count) {
        TEST_PASS("배포 스크립트 실행 가능성");
    } else {
        TEST_FAIL("배포 스크립트 실행 가능성", "일부 스크립트가 없거나 무효함");
    }

    /* 스크립트 구문 검증 (간단한 배치 파일 구문 체크) */
    printf("    배치 파일 구문 검증:\n");
    for (int i = 0; i < script_count; i++) {
        if (validate_file(deployment_scripts[i], 100)) {
            /* 간단한 구문 검증 - 괄호 균형 확인 */
            FILE* script_file = fopen(deployment_scripts[i], "r");
            if (script_file) {
                char buffer[1024];
                int paren_balance = 0;
                bool syntax_ok = true;

                while (fgets(buffer, sizeof(buffer), script_file) && syntax_ok) {
                    for (char* p = buffer; *p; p++) {
                        if (*p == '(') paren_balance++;
                        else if (*p == ')') paren_balance--;

                        if (paren_balance < 0) {
                            syntax_ok = false;
                            break;
                        }
                    }
                }

                fclose(script_file);

                if (syntax_ok && paren_balance == 0) {
                    printf("      ✓ %s 구문 유효\n", deployment_scripts[i]);
                } else {
                    printf("      ⚠ %s 구문 오류 가능성\n", deployment_scripts[i]);
                }
            }
        }
    }
}

/**
 * @brief 배포 검증 결과 요약
 */
static void print_deployment_validation_summary(void) {
    printf("\n=== 배포 검증 결과 요약 ===\n");

    printf("NuGet 패키지 유효성: %s\n", g_deployment.nuget_package_valid ? "유효" : "무효");
    printf("CMake 설정 유효성: %s\n", g_deployment.cmake_config_valid ? "유효" : "무효");
    printf("의존성 만족: %s\n", g_deployment.dependencies_satisfied ? "만족" : "불만족");
    printf("파일 무결성: %s\n", g_deployment.file_integrity_ok ? "양호" : "문제");

    if (g_deployment.missing_files_count > 0) {
        printf("누락된 파일 수: %d\n", g_deployment.missing_files_count);
    }

    if (g_deployment.invalid_files_count > 0) {
        printf("무효한 파일 수: %d\n", g_deployment.invalid_files_count);
    }

    /* 전체 배포 준비 상태 평가 */
    bool deployment_ready = g_deployment.nuget_package_valid &&
                           g_deployment.cmake_config_valid &&
                           g_deployment.dependencies_satisfied &&
                           g_deployment.file_integrity_ok &&
                           (g_deployment.missing_files_count == 0) &&
                           (g_deployment.invalid_files_count <= 1); /* 1개 정도는 허용 */

    printf("\n배포 준비 상태: %s\n", deployment_ready ? "준비됨" : "준비 안됨");

    if (!deployment_ready) {
        printf("\n개선 필요 사항:\n");
        if (!g_deployment.nuget_package_valid) {
            printf("  - NuGet 패키지 파일 수정 필요\n");
        }
        if (!g_deployment.cmake_config_valid) {
            printf("  - CMake 설정 파일 수정 필요\n");
        }
        if (!g_deployment.dependencies_satisfied) {
            printf("  - 의존성 문제 해결 필요\n");
        }
        if (!g_deployment.file_integrity_ok) {
            printf("  - 파일 구조 문제 해결 필요\n");
        }
        if (g_deployment.missing_files_count > 0) {
            printf("  - 누락된 파일 추가 필요\n");
        }
        if (g_deployment.invalid_files_count > 1) {
            printf("  - 무효한 파일 수정 필요\n");
        }
    }
}

/**
 * @brief 테스트 결과 요약 출력
 */
static void print_test_summary(void) {
    printf("\n=== 테스트 결과 요약 ===\n");
    printf("총 테스트: %d\n", g_test_results.total_tests);
    printf("통과: %d\n", g_test_results.passed_tests);
    printf("실패: %d\n", g_test_results.failed_tests);
    printf("건너뜀: %d\n", g_test_results.skipped_tests);

    double success_rate = 0.0;
    if (g_test_results.total_tests > 0) {
        success_rate = (double)g_test_results.passed_tests / g_test_results.total_tests * 100.0;
    }

    printf("성공률: %.1f%%\n", success_rate);

    if (g_test_results.failed_tests == 0) {
        printf("✓ 모든 테스트 통과!\n");
    } else {
        printf("✗ %d개 테스트 실패\n", g_test_results.failed_tests);
    }
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== Windows 배포 시스템 검증 테스트 ===\n\n");

    /* NuGet 패키지 구조 검증 */
    test_nuget_package_structure_validation();
    printf("\n");

    /* CMake 설정 파일 검증 */
    test_cmake_config_validation();
    printf("\n");

    /* 의존성 검증 */
    test_dependency_validation();
    printf("\n");

    /* 배포 파일 무결성 검증 */
    test_deployment_file_integrity();
    printf("\n");

    /* 배포 스크립트 실행 가능성 테스트 */
    test_deployment_script_executability();
    printf("\n");

    /* 배포 검증 결과 요약 */
    print_deployment_validation_summary();

    /* 테스트 결과 요약 */
    print_test_summary();

    return (g_test_results.failed_tests == 0) ? 0 : 1;
}

#else /* !_WIN32 */

int main(void) {
    printf("이 테스트는 Windows 플랫폼에서만 실행됩니다.\n");
    return 0;
}

#endif /* _WIN32 */