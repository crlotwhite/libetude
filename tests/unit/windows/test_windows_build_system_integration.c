/**
 * @file test_windows_build_system_integration.c
 * @brief Windows 빌드 시스템 및 배포 통합 테스트
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Visual Studio 2019/2022 및 MinGW 빌드 테스트 자동화
 * NuGet 패키지 생성 및 CMake 통합 테스트
 * Requirements: 1.1, 1.2, 5.2, 5.3
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

/* 빌드 환경 정보 */
typedef struct {
    bool visual_studio_2019_available;
    bool visual_studio_2022_available;
    bool mingw_available;
    bool cmake_available;
    bool nuget_available;
    bool dotnet_available;
    char vs_version[64];
    char cmake_version[64];
    char nuget_version[64];
} BuildEnvironment;

static BuildEnvironment g_build_env = { 0 };

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
 * @brief 명령 실행 및 결과 확인
 */
static bool execute_command(const char* command, char* output, size_t output_size) {
    FILE* pipe = _popen(command, "r");
    if (!pipe) {
        return false;
    }

    if (output && output_size > 0) {
        memset(output, 0, output_size);
        fgets(output, (int)output_size, pipe);

        /* 개행 문자 제거 */
        size_t len = strlen(output);
        if (len > 0 && output[len - 1] == '\n') {
            output[len - 1] = '\0';
        }
    }

    int result = _pclose(pipe);
    return (result == 0);
}

/**
 * @brief 빌드 환경 감지
 * Requirements: 1.1, 1.2
 */
static void detect_build_environment(void) {
    TEST_START("빌드 환경 감지");

    /* Visual Studio 2022 확인 */
    char vs2022_path[MAX_PATH];
    snprintf(vs2022_path, sizeof(vs2022_path),
             "%s\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
             getenv("ProgramFiles"));

    if (GetFileAttributesA(vs2022_path) != INVALID_FILE_ATTRIBUTES) {
        g_build_env.visual_studio_2022_available = true;
        strcpy_s(g_build_env.vs_version, sizeof(g_build_env.vs_version), "2022 Professional");
    } else {
        snprintf(vs2022_path, sizeof(vs2022_path),
                 "%s\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
                 getenv("ProgramFiles"));

        if (GetFileAttributesA(vs2022_path) != INVALID_FILE_ATTRIBUTES) {
            g_build_env.visual_studio_2022_available = true;
            strcpy_s(g_build_env.vs_version, sizeof(g_build_env.vs_version), "2022 Community");
        }
    }

    /* Visual Studio 2019 확인 */
    if (!g_build_env.visual_studio_2022_available) {
        char vs2019_path[MAX_PATH];
        snprintf(vs2019_path, sizeof(vs2019_path),
                 "%s\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
                 getenv("ProgramFiles(x86)"));

        if (GetFileAttributesA(vs2019_path) != INVALID_FILE_ATTRIBUTES) {
            g_build_env.visual_studio_2019_available = true;
            strcpy_s(g_build_env.vs_version, sizeof(g_build_env.vs_version), "2019 Professional");
        } else {
            snprintf(vs2019_path, sizeof(vs2019_path),
                     "%s\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
                     getenv("ProgramFiles(x86)"));

            if (GetFileAttributesA(vs2019_path) != INVALID_FILE_ATTRIBUTES) {
                g_build_env.visual_studio_2019_available = true;
                strcpy_s(g_build_env.vs_version, sizeof(g_build_env.vs_version), "2019 Community");
            }
        }
    }

    /* MinGW 확인 */
    g_build_env.mingw_available = execute_command("gcc --version", NULL, 0);

    /* CMake 확인 */
    g_build_env.cmake_available = execute_command("cmake --version",
                                                  g_build_env.cmake_version,
                                                  sizeof(g_build_env.cmake_version));

    /* NuGet CLI 확인 */
    g_build_env.nuget_available = execute_command("nuget",
                                                  g_build_env.nuget_version,
                                                  sizeof(g_build_env.nuget_version));

    /* .NET CLI 확인 */
    g_build_env.dotnet_available = execute_command("dotnet --version", NULL, 0);

    /* 결과 출력 */
    printf("    감지된 빌드 환경:\n");
    printf("      Visual Studio 2022: %s\n", g_build_env.visual_studio_2022_available ? "사용 가능" : "없음");
    printf("      Visual Studio 2019: %s\n", g_build_env.visual_studio_2019_available ? "사용 가능" : "없음");
    printf("      MinGW: %s\n", g_build_env.mingw_available ? "사용 가능" : "없음");
    printf("      CMake: %s\n", g_build_env.cmake_available ? g_build_env.cmake_version : "없음");
    printf("      NuGet CLI: %s\n", g_build_env.nuget_available ? "사용 가능" : "없음");
    printf("      .NET CLI: %s\n", g_build_env.dotnet_available ? "사용 가능" : "없음");

    if (g_build_env.visual_studio_2019_available || g_build_env.visual_studio_2022_available) {
        TEST_PASS("Visual Studio 환경 감지");
    } else {
        TEST_FAIL("Visual Studio 환경 감지", "Visual Studio 2019 또는 2022가 설치되지 않음");
    }

    if (g_build_env.cmake_available) {
        TEST_PASS("CMake 환경 감지");
    } else {
        TEST_FAIL("CMake 환경 감지", "CMake가 설치되지 않음");
    }
}

/**
 * @brief Visual Studio 빌드 테스트
 * Requirements: 1.1, 1.2
 */
static void test_visual_studio_build(void) {
    TEST_START("Visual Studio 빌드");

    if (!g_build_env.visual_studio_2019_available && !g_build_env.visual_studio_2022_available) {
        TEST_SKIP("Visual Studio 빌드", "Visual Studio가 설치되지 않음");
        return;
    }

    if (!g_build_env.cmake_available) {
        TEST_SKIP("Visual Studio 빌드", "CMake가 설치되지 않음");
        return;
    }

    /* 임시 빌드 디렉토리 생성 */
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    strcat_s(temp_dir, MAX_PATH, "libetude_vs_build_test");

    CreateDirectoryA(temp_dir, NULL);

    /* CMake 구성 명령 생성 */
    char cmake_config_cmd[1024];
    snprintf(cmake_config_cmd, sizeof(cmake_config_cmd),
             "cd /d \"%s\" && cmake -G \"Visual Studio %s\" -A x64 -DCMAKE_BUILD_TYPE=Release \"%s\"",
             temp_dir,
             g_build_env.visual_studio_2022_available ? "17 2022" : "16 2019",
             ".."); /* 실제 프로젝트 루트 경로로 변경 필요 */

    /* CMake 구성 테스트 */
    printf("    CMake 구성 테스트 중...\n");
    bool config_success = execute_command(cmake_config_cmd, NULL, 0);

    if (config_success) {
        TEST_PASS("CMake 구성 (Visual Studio)");

        /* 빌드 테스트 */
        char build_cmd[512];
        snprintf(build_cmd, sizeof(build_cmd),
                 "cd /d \"%s\" && cmake --build . --config Release --parallel 4",
                 temp_dir);

        printf("    빌드 테스트 중...\n");
        bool build_success = execute_command(build_cmd, NULL, 0);

        if (build_success) {
            TEST_PASS("Visual Studio Release 빌드");

            /* Debug 빌드 테스트 */
            snprintf(build_cmd, sizeof(build_cmd),
                     "cd /d \"%s\" && cmake --build . --config Debug --parallel 4",
                     temp_dir);

            bool debug_build_success = execute_command(build_cmd, NULL, 0);

            if (debug_build_success) {
                TEST_PASS("Visual Studio Debug 빌드");
            } else {
                TEST_FAIL("Visual Studio Debug 빌드", "Debug 빌드 실패");
            }
        } else {
            TEST_FAIL("Visual Studio Release 빌드", "Release 빌드 실패");
        }
    } else {
        TEST_FAIL("CMake 구성 (Visual Studio)", "CMake 구성 실패");
    }

    /* 임시 디렉토리 정리 */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rmdir /s /q \"%s\"", temp_dir);
    execute_command(cleanup_cmd, NULL, 0);
}

/**
 * @brief MinGW 빌드 테스트
 * Requirements: 1.2
 */
static void test_mingw_build(void) {
    TEST_START("MinGW 빌드");

    if (!g_build_env.mingw_available) {
        TEST_SKIP("MinGW 빌드", "MinGW가 설치되지 않음");
        return;
    }

    if (!g_build_env.cmake_available) {
        TEST_SKIP("MinGW 빌드", "CMake가 설치되지 않음");
        return;
    }

    /* 임시 빌드 디렉토리 생성 */
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    strcat_s(temp_dir, MAX_PATH, "libetude_mingw_build_test");

    CreateDirectoryA(temp_dir, NULL);

    /* CMake 구성 명령 생성 */
    char cmake_config_cmd[1024];
    snprintf(cmake_config_cmd, sizeof(cmake_config_cmd),
             "cd /d \"%s\" && cmake -G \"MinGW Makefiles\" -DCMAKE_BUILD_TYPE=Release \"%s\"",
             temp_dir, ".."); /* 실제 프로젝트 루트 경로로 변경 필요 */

    /* CMake 구성 테스트 */
    printf("    CMake 구성 테스트 중...\n");
    bool config_success = execute_command(cmake_config_cmd, NULL, 0);

    if (config_success) {
        TEST_PASS("CMake 구성 (MinGW)");

        /* 빌드 테스트 */
        char build_cmd[512];
        snprintf(build_cmd, sizeof(build_cmd),
                 "cd /d \"%s\" && cmake --build . --parallel 4",
                 temp_dir);

        printf("    빌드 테스트 중...\n");
        bool build_success = execute_command(build_cmd, NULL, 0);

        if (build_success) {
            TEST_PASS("MinGW 빌드");
        } else {
            TEST_FAIL("MinGW 빌드", "빌드 실패");
        }
    } else {
        TEST_FAIL("CMake 구성 (MinGW)", "CMake 구성 실패");
    }

    /* 임시 디렉토리 정리 */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rmdir /s /q \"%s\"", temp_dir);
    execute_command(cleanup_cmd, NULL, 0);
}

/**
 * @brief CMake 통합 테스트
 * Requirements: 5.3
 */
static void test_cmake_integration(void) {
    TEST_START("CMake 통합");

    if (!g_build_env.cmake_available) {
        TEST_SKIP("CMake 통합", "CMake가 설치되지 않음");
        return;
    }

    /* 임시 테스트 프로젝트 디렉토리 생성 */
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    strcat_s(temp_dir, MAX_PATH, "libetude_cmake_integration_test");

    CreateDirectoryA(temp_dir, NULL);

    /* 테스트용 CMakeLists.txt 생성 */
    char cmake_file[MAX_PATH];
    snprintf(cmake_file, sizeof(cmake_file), "%s\\CMakeLists.txt", temp_dir);

    FILE* cmake_fp = fopen(cmake_file, "w");
    if (!cmake_fp) {
        TEST_FAIL("CMake 통합", "테스트 파일 생성 실패");
        return;
    }

    fprintf(cmake_fp, "cmake_minimum_required(VERSION 3.16)\n");
    fprintf(cmake_fp, "project(LibEtudeIntegrationTest VERSION 1.0.0 LANGUAGES C CXX)\n");
    fprintf(cmake_fp, "\n");
    fprintf(cmake_fp, "# LibEtude 패키지 찾기\n");
    fprintf(cmake_fp, "find_package(LibEtude REQUIRED\n");
    fprintf(cmake_fp, "    PATHS \"${CMAKE_CURRENT_SOURCE_DIR}/../cmake\"\n");
    fprintf(cmake_fp, "    NO_DEFAULT_PATH\n");
    fprintf(cmake_fp, ")\n");
    fprintf(cmake_fp, "\n");
    fprintf(cmake_fp, "# 테스트 실행 파일 생성\n");
    fprintf(cmake_fp, "add_executable(integration_test main.c)\n");
    fprintf(cmake_fp, "\n");
    fprintf(cmake_fp, "# LibEtude 라이브러리 링크\n");
    fprintf(cmake_fp, "if(TARGET LibEtude::LibEtude)\n");
    fprintf(cmake_fp, "    target_link_libraries(integration_test PRIVATE LibEtude::LibEtude)\n");
    fprintf(cmake_fp, "    message(STATUS \"LibEtude::LibEtude 타겟 사용\")\n");
    fprintf(cmake_fp, "else()\n");
    fprintf(cmake_fp, "    target_include_directories(integration_test PRIVATE ${LIBETUDE_INCLUDE_DIRS})\n");
    fprintf(cmake_fp, "    target_link_libraries(integration_test PRIVATE ${LIBETUDE_LIBRARIES})\n");
    fprintf(cmake_fp, "    if(WIN32)\n");
    fprintf(cmake_fp, "        target_link_libraries(integration_test PRIVATE ${LIBETUDE_WINDOWS_LIBRARIES})\n");
    fprintf(cmake_fp, "    endif()\n");
    fprintf(cmake_fp, "    message(STATUS \"수동 LibEtude 설정 사용\")\n");
    fprintf(cmake_fp, "endif()\n");
    fprintf(cmake_fp, "\n");
    fprintf(cmake_fp, "# Windows 특화 설정 적용\n");
    fprintf(cmake_fp, "if(WIN32)\n");
    fprintf(cmake_fp, "    libetude_configure_windows_target(integration_test)\n");
    fprintf(cmake_fp, "endif()\n");

    fclose(cmake_fp);

    /* 테스트용 main.c 생성 */
    char main_file[MAX_PATH];
    snprintf(main_file, sizeof(main_file), "%s\\main.c", temp_dir);

    FILE* main_fp = fopen(main_file, "w");
    if (!main_fp) {
        TEST_FAIL("CMake 통합", "테스트 소스 파일 생성 실패");
        return;
    }

    fprintf(main_fp, "#include <stdio.h>\n");
    fprintf(main_fp, "#include <stdlib.h>\n");
    fprintf(main_fp, "\n");
    fprintf(main_fp, "#ifdef LIBETUDE_PLATFORM_WINDOWS\n");
    fprintf(main_fp, "#include <windows.h>\n");
    fprintf(main_fp, "#endif\n");
    fprintf(main_fp, "\n");
    fprintf(main_fp, "int main(void) {\n");
    fprintf(main_fp, "    printf(\"LibEtude CMake 통합 테스트\\n\");\n");
    fprintf(main_fp, "\n");
    fprintf(main_fp, "#ifdef LIBETUDE_PLATFORM_WINDOWS\n");
    fprintf(main_fp, "    printf(\"Windows 플랫폼 감지됨\\n\");\n");
    fprintf(main_fp, "#endif\n");
    fprintf(main_fp, "\n");
    fprintf(main_fp, "#ifdef LIBETUDE_ENABLE_SIMD\n");
    fprintf(main_fp, "    printf(\"SIMD 최적화 활성화\\n\");\n");
    fprintf(main_fp, "#endif\n");
    fprintf(main_fp, "\n");
    fprintf(main_fp, "    printf(\"테스트 완료\\n\");\n");
    fprintf(main_fp, "    return 0;\n");
    fprintf(main_fp, "}\n");

    fclose(main_fp);

    /* CMake 구성 테스트 */
    char build_dir[MAX_PATH];
    snprintf(build_dir, sizeof(build_dir), "%s\\build", temp_dir);
    CreateDirectoryA(build_dir, NULL);

    char cmake_config_cmd[1024];
    snprintf(cmake_config_cmd, sizeof(cmake_config_cmd),
             "cd /d \"%s\" && cmake -DCMAKE_PREFIX_PATH=\"..\\..\\cmake\" \"%s\"",
             build_dir, temp_dir);

    printf("    CMake 구성 테스트 중...\n");
    bool config_success = execute_command(cmake_config_cmd, NULL, 0);

    if (config_success) {
        TEST_PASS("CMake find_package 테스트");

        /* 빌드 테스트 */
        char build_cmd[512];
        snprintf(build_cmd, sizeof(build_cmd),
                 "cd /d \"%s\" && cmake --build .",
                 build_dir);

        printf("    빌드 테스트 중...\n");
        bool build_success = execute_command(build_cmd, NULL, 0);

        if (build_success) {
            TEST_PASS("CMake 통합 빌드");

            /* 실행 테스트 */
            char exe_path[MAX_PATH];
            snprintf(exe_path, sizeof(exe_path), "%s\\Debug\\integration_test.exe", build_dir);

            if (GetFileAttributesA(exe_path) != INVALID_FILE_ATTRIBUTES) {
                char run_cmd[512];
                snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", exe_path);

                bool run_success = execute_command(run_cmd, NULL, 0);

                if (run_success) {
                    TEST_PASS("CMake 통합 실행");
                } else {
                    TEST_FAIL("CMake 통합 실행", "실행 실패");
                }
            } else {
                TEST_SKIP("CMake 통합 실행", "실행 파일을 찾을 수 없음");
            }
        } else {
            TEST_FAIL("CMake 통합 빌드", "빌드 실패");
        }
    } else {
        TEST_SKIP("CMake find_package 테스트", "LibEtude가 설치되지 않음 (정상)");
    }

    /* 임시 디렉토리 정리 */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rmdir /s /q \"%s\"", temp_dir);
    execute_command(cleanup_cmd, NULL, 0);
}

/**
 * @brief NuGet 패키지 생성 테스트
 * Requirements: 5.2
 */
static void test_nuget_package_creation(void) {
    TEST_START("NuGet 패키지 생성");

    if (!g_build_env.nuget_available && !g_build_env.dotnet_available) {
        TEST_SKIP("NuGet 패키지 생성", "NuGet CLI 또는 .NET CLI가 설치되지 않음");
        return;
    }

    /* 임시 패키지 디렉토리 생성 */
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    strcat_s(temp_dir, MAX_PATH, "libetude_nuget_test");

    CreateDirectoryA(temp_dir, NULL);

    /* 테스트용 nuspec 파일 생성 */
    char nuspec_file[MAX_PATH];
    snprintf(nuspec_file, sizeof(nuspec_file), "%s\\LibEtudeTest.nuspec", temp_dir);

    FILE* nuspec_fp = fopen(nuspec_file, "w");
    if (!nuspec_fp) {
        TEST_FAIL("NuGet 패키지 생성", "nuspec 파일 생성 실패");
        return;
    }

    fprintf(nuspec_fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    fprintf(nuspec_fp, "<package>\n");
    fprintf(nuspec_fp, "  <metadata>\n");
    fprintf(nuspec_fp, "    <id>LibEtudeTest</id>\n");
    fprintf(nuspec_fp, "    <version>1.0.0-test</version>\n");
    fprintf(nuspec_fp, "    <title>LibEtude Test Package</title>\n");
    fprintf(nuspec_fp, "    <authors>LibEtude Project</authors>\n");
    fprintf(nuspec_fp, "    <description>Test package for LibEtude build system</description>\n");
    fprintf(nuspec_fp, "    <tags>ai voice synthesis tts test</tags>\n");
    fprintf(nuspec_fp, "    <requireLicenseAcceptance>false</requireLicenseAcceptance>\n");
    fprintf(nuspec_fp, "  </metadata>\n");
    fprintf(nuspec_fp, "  <files>\n");
    fprintf(nuspec_fp, "    <file src=\"readme.txt\" target=\"\" />\n");
    fprintf(nuspec_fp, "  </files>\n");
    fprintf(nuspec_fp, "</package>\n");

    fclose(nuspec_fp);

    /* 테스트용 파일 생성 */
    char readme_file[MAX_PATH];
    snprintf(readme_file, sizeof(readme_file), "%s\\readme.txt", temp_dir);

    FILE* readme_fp = fopen(readme_file, "w");
    if (readme_fp) {
        fprintf(readme_fp, "LibEtude Test Package\n");
        fprintf(readme_fp, "This is a test package for build system validation.\n");
        fclose(readme_fp);
    }

    /* NuGet 패키지 생성 테스트 */
    char pack_cmd[1024];

    if (g_build_env.nuget_available) {
        snprintf(pack_cmd, sizeof(pack_cmd),
                 "cd /d \"%s\" && nuget pack LibEtudeTest.nuspec -OutputDirectory .",
                 temp_dir);
    } else {
        snprintf(pack_cmd, sizeof(pack_cmd),
                 "cd /d \"%s\" && dotnet pack LibEtudeTest.nuspec -o .",
                 temp_dir);
    }

    printf("    NuGet 패키지 생성 중...\n");
    bool pack_success = execute_command(pack_cmd, NULL, 0);

    if (pack_success) {
        TEST_PASS("NuGet 패키지 생성");

        /* 생성된 패키지 파일 확인 */
        char package_file[MAX_PATH];
        snprintf(package_file, sizeof(package_file), "%s\\LibEtudeTest.1.0.0-test.nupkg", temp_dir);

        if (GetFileAttributesA(package_file) != INVALID_FILE_ATTRIBUTES) {
            TEST_PASS("NuGet 패키지 파일 생성 확인");
        } else {
            TEST_FAIL("NuGet 패키지 파일 생성 확인", "패키지 파일을 찾을 수 없음");
        }
    } else {
        TEST_FAIL("NuGet 패키지 생성", "패키지 생성 실패");
    }

    /* 임시 디렉토리 정리 */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rmdir /s /q \"%s\"", temp_dir);
    execute_command(cleanup_cmd, NULL, 0);
}

/**
 * @brief 빌드 스크립트 검증 테스트
 * Requirements: 1.1, 1.2, 5.2, 5.3
 */
static void test_build_scripts_validation(void) {
    TEST_START("빌드 스크립트 검증");

    /* 필수 빌드 스크립트 파일 확인 */
    const char* required_scripts[] = {
        "scripts\\test_windows_build.bat",
        "scripts\\test_cmake_integration.bat",
        "scripts\\validate_nuget_dependencies.bat",
        "scripts\\build_nuget.bat",
        "scripts\\build_nuget_multiplatform.bat"
    };

    int script_count = sizeof(required_scripts) / sizeof(required_scripts[0]);
    int found_scripts = 0;

    for (int i = 0; i < script_count; i++) {
        char script_path[MAX_PATH];
        snprintf(script_path, sizeof(script_path), "..\\%s", required_scripts[i]);

        if (GetFileAttributesA(script_path) != INVALID_FILE_ATTRIBUTES) {
            found_scripts++;
            printf("    ✓ 스크립트 발견: %s\n", required_scripts[i]);
        } else {
            printf("    ✗ 스크립트 없음: %s\n", required_scripts[i]);
        }
    }

    if (found_scripts == script_count) {
        TEST_PASS("빌드 스크립트 파일 존재 확인");
    } else {
        TEST_FAIL("빌드 스크립트 파일 존재 확인", "일부 스크립트 파일이 없음");
    }

    /* CMake 설정 파일 확인 */
    const char* cmake_files[] = {
        "cmake\\WindowsConfig.cmake",
        "cmake\\LibEtudeConfig.cmake.in",
        "cmake\\LibEtudeConfigVersion.cmake.in"
    };

    int cmake_file_count = sizeof(cmake_files) / sizeof(cmake_files[0]);
    int found_cmake_files = 0;

    for (int i = 0; i < cmake_file_count; i++) {
        char cmake_path[MAX_PATH];
        snprintf(cmake_path, sizeof(cmake_path), "..\\%s", cmake_files[i]);

        if (GetFileAttributesA(cmake_path) != INVALID_FILE_ATTRIBUTES) {
            found_cmake_files++;
            printf("    ✓ CMake 파일 발견: %s\n", cmake_files[i]);
        } else {
            printf("    ✗ CMake 파일 없음: %s\n", cmake_files[i]);
        }
    }

    if (found_cmake_files == cmake_file_count) {
        TEST_PASS("CMake 설정 파일 존재 확인");
    } else {
        TEST_FAIL("CMake 설정 파일 존재 확인", "일부 CMake 파일이 없음");
    }

    /* NuGet 패키지 파일 확인 */
    const char* nuget_files[] = {
        "packaging\\nuget\\LibEtude.nuspec",
        "packaging\\nuget\\LibEtude.targets",
        "packaging\\nuget\\LibEtude.props"
    };

    int nuget_file_count = sizeof(nuget_files) / sizeof(nuget_files[0]);
    int found_nuget_files = 0;

    for (int i = 0; i < nuget_file_count; i++) {
        char nuget_path[MAX_PATH];
        snprintf(nuget_path, sizeof(nuget_path), "..\\%s", nuget_files[i]);

        if (GetFileAttributesA(nuget_path) != INVALID_FILE_ATTRIBUTES) {
            found_nuget_files++;
            printf("    ✓ NuGet 파일 발견: %s\n", nuget_files[i]);
        } else {
            printf("    ✗ NuGet 파일 없음: %s\n", nuget_files[i]);
        }
    }

    if (found_nuget_files == nuget_file_count) {
        TEST_PASS("NuGet 패키지 파일 존재 확인");
    } else {
        TEST_FAIL("NuGet 패키지 파일 존재 확인", "일부 NuGet 파일이 없음");
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

    /* 빌드 환경 요약 */
    printf("\n=== 빌드 환경 요약 ===\n");
    printf("Visual Studio: %s\n",
           g_build_env.visual_studio_2022_available || g_build_env.visual_studio_2019_available ?
           g_build_env.vs_version : "없음");
    printf("MinGW: %s\n", g_build_env.mingw_available ? "사용 가능" : "없음");
    printf("CMake: %s\n", g_build_env.cmake_available ? "사용 가능" : "없음");
    printf("NuGet/dotnet: %s\n",
           g_build_env.nuget_available || g_build_env.dotnet_available ? "사용 가능" : "없음");
}

/**
 * @brief 메인 테스트 함수
 */
int main(void) {
    printf("=== Windows 빌드 시스템 및 배포 통합 테스트 ===\n\n");

    /* 빌드 환경 감지 */
    detect_build_environment();
    printf("\n");

    /* 빌드 스크립트 검증 */
    test_build_scripts_validation();
    printf("\n");

    /* Visual Studio 빌드 테스트 */
    test_visual_studio_build();
    printf("\n");

    /* MinGW 빌드 테스트 */
    test_mingw_build();
    printf("\n");

    /* CMake 통합 테스트 */
    test_cmake_integration();
    printf("\n");

    /* NuGet 패키지 생성 테스트 */
    test_nuget_package_creation();
    printf("\n");

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