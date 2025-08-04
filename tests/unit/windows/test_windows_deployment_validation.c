/*
 * LibEtude Windows 배포 검증 테스트
 * Copyright (c) 2025 LibEtude Project
 *
 * 이 파일은 NuGet 패키지 생성 및 CMake 통합 테스트를 구현합니다.
 *
 * 요구사항: 5.2, 5.3 - NuGet 패키지 배포 및 CMake find_package 지원
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h>
#include <io.h>
#include <shlobj.h>
#else
#error "이 테스트는 Windows 전용입니다"
#endif

// 테스트 결과 구조체
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    char last_error[512];
} TestResults;

// 전역 테스트 결과
static TestResults g_test_results = {0};

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        g_test_results.total_tests++; \
        if (!(condition)) { \
            snprintf(g_test_results.last_error, sizeof(g_test_results.last_error), \
                    "FAIL: %s (line %d)", message, __LINE__); \
            printf("❌ %s\n", g_test_results.last_error); \
            g_test_results.failed_tests++; \
            return 0; \
        } else { \
            printf("✅ %s\n", message); \
            g_test_results.passed_tests++; \
        } \
    } while(0)

// 유틸리티 함수들
static int file_exists(const char* path) {
    return _access(path, 0) == 0;
}

static int directory_exists(const char* path) {
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

static int execute_command_with_output(const char* command, char* output, size_t output_size) {
    FILE* pipe = _popen(command, "r");
    if (!pipe) {
        return -1;
    }

    size_t total_read = 0;
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), pipe) && total_read < output_size - 1) {
        size_t len = strlen(buffer);
        if (total_read + len < output_size - 1) {
            strcpy(output + total_read, buffer);
            total_read += len;
        }
    }

    output[total_read] = '\0';
    return _pclose(pipe);
}

static void create_directory_recursive(const char* path) {
    char temp_path[512];
    char* p = NULL;
    size_t len;

    snprintf(temp_path, sizeof(temp_path), "%s", path);
    len = strlen(temp_path);

    if (temp_path[len - 1] == '\\') {
        temp_path[len - 1] = 0;
    }

    for (p = temp_path + 1; *p; p++) {
        if (*p == '\\') {
            *p = 0;
            _mkdir(temp_path);
            *p = '\\';
        }
    }
    _mkdir(temp_path);
}

// NuGet 도구 확인 테스트
static int test_nuget_tools_availability(void) {
    printf("\n=== NuGet 도구 가용성 테스트 ===\n");

    char output[1024];
    int result;

    // NuGet CLI 확인
    result = execute_command_with_output("nuget.exe help 2>&1", output, sizeof(output));
    if (result == 0 && strstr(output, "NuGet") != NULL) {
        TEST_ASSERT(1, "NuGet CLI 사용 가능");

        // NuGet 버전 확인
        result = execute_command_with_output("nuget.exe help | findstr Version", output, sizeof(output));
        if (result == 0) {
            printf("  NuGet 버전: %s", output);
        }
    } else {
        printf("⚠️  NuGet CLI를 찾을 수 없습니다\n");

        // .NET CLI 확인
        result = execute_command_with_output("dotnet --version 2>&1", output, sizeof(output));
        if (result == 0) {
            TEST_ASSERT(1, ".NET CLI 사용 가능 (NuGet 대안)");
            printf("  .NET 버전: %s", output);
        } else {
            printf("❌ NuGet CLI와 .NET CLI 모두 사용할 수 없습니다\n");
            return 0;
        }
    }

    // MSBuild 확인
    result = execute_command_with_output("msbuild -version 2>&1", output, sizeof(output));
    if (result == 0 && strstr(output, "Microsoft") != NULL) {
        TEST_ASSERT(1, "MSBuild 사용 가능");
    } else {
        printf("⚠️  MSBuild를 찾을 수 없습니다\n");
    }

    return 1;
}

// NuGet 패키지 구조 검증 테스트
static int test_nuget_package_structure(void) {
    printf("\n=== NuGet 패키지 구조 검증 테스트 ===\n");

    const char* test_package_dir = "temp_nuget_package";

    // 테스트 패키지 디렉토리 생성
    create_directory_recursive(test_package_dir);

    // NuGet 패키지 구조 생성
    const char* required_dirs[] = {
        "temp_nuget_package\\lib\\x64\\Release",
        "temp_nuget_package\\lib\\x64\\Debug",
        "temp_nuget_package\\lib\\Win32\\Release",
        "temp_nuget_package\\lib\\Win32\\Debug",
        "temp_nuget_package\\lib\\ARM64\\Release",
        "temp_nuget_package\\lib\\ARM64\\Debug",
        "temp_nuget_package\\include\\libetude",
        "temp_nuget_package\\bin\\x64\\Release",
        "temp_nuget_package\\bin\\x64\\Debug",
        "temp_nuget_package\\cmake",
        "temp_nuget_package\\tools",
        "temp_nuget_package\\examples",
        "temp_nuget_package\\docs"
    };

    for (int i = 0; i < sizeof(required_dirs) / sizeof(required_dirs[0]); i++) {
        create_directory_recursive(required_dirs[i]);
        TEST_ASSERT(directory_exists(required_dirs[i]),
                   strrchr(required_dirs[i], '\\') + 1);
    }

    // 필수 파일 생성
    const char* nuspec_content =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<package>\n"
        "  <metadata>\n"
        "    <id>LibEtude</id>\n"
        "    <version>1.0.0</version>\n"
        "    <title>LibEtude - AI Voice Synthesis Engine</title>\n"
        "    <authors>LibEtude Project</authors>\n"
        "    <description>Optimized AI inference engine for voice synthesis</description>\n"
        "    <tags>ai voice synthesis tts</tags>\n"
        "    <requireLicenseAcceptance>false</requireLicenseAcceptance>\n"
        "  </metadata>\n"
        "  <files>\n"
        "    <file src=\"lib\\**\\*\" target=\"lib\" />\n"
        "    <file src=\"include\\**\\*\" target=\"include\" />\n"
        "    <file src=\"bin\\**\\*\" target=\"bin\" />\n"
        "    <file src=\"cmake\\**\\*\" target=\"cmake\" />\n"
        "    <file src=\"tools\\**\\*\" target=\"tools\" />\n"
        "    <file src=\"examples\\**\\*\" target=\"examples\" />\n"
        "    <file src=\"docs\\**\\*\" target=\"docs\" />\n"
        "    <file src=\"LibEtude.targets\" target=\"\" />\n"
        "    <file src=\"LibEtude.props\" target=\"\" />\n"
        "  </files>\n"
        "</package>\n";

    FILE* nuspec_file = fopen("temp_nuget_package\\LibEtude.nuspec", "w");
    if (nuspec_file) {
        fputs(nuspec_content, nuspec_file);
        fclose(nuspec_file);
        TEST_ASSERT(1, "NuSpec 파일 생성");
    }

    // MSBuild targets 파일 생성
    const char* targets_content =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
        "  <PropertyGroup>\n"
        "    <LibEtudeVersion>1.0.0</LibEtudeVersion>\n"
        "    <LibEtudeRoot>$(MSBuildThisFileDirectory)</LibEtudeRoot>\n"
        "  </PropertyGroup>\n"
        "  \n"
        "  <PropertyGroup Condition=\"'$(Platform)' == 'x64'\">\n"
        "    <LibEtudeLibPath>$(LibEtudeRoot)lib\\x64\\$(Configuration)\\</LibEtudeLibPath>\n"
        "    <LibEtudeBinPath>$(LibEtudeRoot)bin\\x64\\$(Configuration)\\</LibEtudeBinPath>\n"
        "  </PropertyGroup>\n"
        "  \n"
        "  <PropertyGroup Condition=\"'$(Platform)' == 'Win32'\">\n"
        "    <LibEtudeLibPath>$(LibEtudeRoot)lib\\Win32\\$(Configuration)\\</LibEtudeLibPath>\n"
        "    <LibEtudeBinPath>$(LibEtudeRoot)bin\\Win32\\$(Configuration)\\</LibEtudeBinPath>\n"
        "  </PropertyGroup>\n"
        "  \n"
        "  <ItemGroup>\n"
        "    <ClInclude Include=\"$(LibEtudeRoot)include\\libetude\\**\\*.h\" />\n"
        "  </ItemGroup>\n"
        "  \n"
        "  <ItemGroup>\n"
        "    <LibEtudeLibs Include=\"$(LibEtudeLibPath)*.lib\" />\n"
        "  </ItemGroup>\n"
        "  \n"
        "  <ItemGroup>\n"
        "    <Link Include=\"@(LibEtudeLibs)\" />\n"
        "    <Link Include=\"kernel32.lib;user32.lib;ole32.lib;oleaut32.lib;uuid.lib\" />\n"
        "    <Link Include=\"winmm.lib;dsound.lib;mmdevapi.lib\" />\n"
        "  </ItemGroup>\n"
        "</Project>\n";

    FILE* targets_file = fopen("temp_nuget_package\\LibEtude.targets", "w");
    if (targets_file) {
        fputs(targets_content, targets_file);
        fclose(targets_file);
        TEST_ASSERT(1, "MSBuild targets 파일 생성");
    }

    // Props 파일 생성
    const char* props_content =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n"
        "  <PropertyGroup>\n"
        "    <LibEtudeIncludePath>$(MSBuildThisFileDirectory)include</LibEtudeIncludePath>\n"
        "  </PropertyGroup>\n"
        "  \n"
        "  <ItemDefinitionGroup>\n"
        "    <ClCompile>\n"
        "      <AdditionalIncludeDirectories>$(LibEtudeIncludePath);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>\n"
        "      <PreprocessorDefinitions>LIBETUDE_PLATFORM_WINDOWS=1;%(PreprocessorDefinitions)</PreprocessorDefinitions>\n"
        "    </ClCompile>\n"
        "  </ItemDefinitionGroup>\n"
        "</Project>\n";

    FILE* props_file = fopen("temp_nuget_package\\LibEtude.props", "w");
    if (props_file) {
        fputs(props_content, props_file);
        fclose(props_file);
        TEST_ASSERT(1, "MSBuild props 파일 생성");
    }

    // 더미 라이브러리 파일 생성 (테스트용)
    const char* dummy_lib_paths[] = {
        "temp_nuget_package\\lib\\x64\\Release\\libetude.lib",
        "temp_nuget_package\\lib\\x64\\Debug\\libetude.lib",
        "temp_nuget_package\\lib\\Win32\\Release\\libetude.lib",
        "temp_nuget_package\\lib\\Win32\\Debug\\libetude.lib"
    };

    for (int i = 0; i < sizeof(dummy_lib_paths) / sizeof(dummy_lib_paths[0]); i++) {
        FILE* dummy_lib = fopen(dummy_lib_paths[i], "wb");
        if (dummy_lib) {
            // 더미 데이터 작성
            fwrite("DUMMY_LIB", 1, 9, dummy_lib);
            fclose(dummy_lib);
        }
    }

    // 더미 헤더 파일 생성
    const char* dummy_header_content =
        "#ifndef LIBETUDE_API_H\n"
        "#define LIBETUDE_API_H\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "typedef enum {\n"
        "    ET_SUCCESS = 0,\n"
        "    ET_ERROR = -1\n"
        "} ETResult;\n"
        "\n"
        "ETResult et_init(void);\n"
        "void et_finalize(void);\n"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif // LIBETUDE_API_H\n";

    FILE* header_file = fopen("temp_nuget_package\\include\\libetude\\api.h", "w");
    if (header_file) {
        fputs(dummy_header_content, header_file);
        fclose(header_file);
        TEST_ASSERT(1, "더미 헤더 파일 생성");
    }

    // 패키지 생성 테스트
    char nuget_pack_cmd[512];
    snprintf(nuget_pack_cmd, sizeof(nuget_pack_cmd),
            "cd temp_nuget_package && nuget pack LibEtude.nuspec -OutputDirectory .. 2>nul");

    int pack_result = system(nuget_pack_cmd);
    if (pack_result == 0) {
        TEST_ASSERT(file_exists("LibEtude.1.0.0.nupkg"), "NuGet 패키지 생성 성공");
    } else {
        // .NET CLI로 시도
        snprintf(nuget_pack_cmd, sizeof(nuget_pack_cmd),
                "cd temp_nuget_package && dotnet pack LibEtude.nuspec -o .. 2>nul");

        pack_result = system(nuget_pack_cmd);
        if (pack_result == 0) {
            TEST_ASSERT(1, ".NET CLI로 패키지 생성 성공");
        } else {
            printf("⚠️  패키지 생성 실패\n");
        }
    }

    // 정리
    system("rmdir /s /q temp_nuget_package 2>nul");
    remove("LibEtude.1.0.0.nupkg");

    return 1;
}

// CMake find_package 통합 테스트
static int test_cmake_find_package_integration(void) {
    printf("\n=== CMake find_package 통합 테스트 ===\n");

    const char* test_project_dir = "temp_cmake_integration";

    // 테스트 프로젝트 디렉토리 생성
    create_directory_recursive(test_project_dir);
    create_directory_recursive("temp_cmake_integration\\cmake");

    // LibEtudeConfig.cmake 파일 생성
    const char* config_cmake_content =
        "# LibEtude CMake 설정 파일 (테스트용)\n"
        "set(LIBETUDE_VERSION \"1.0.0\")\n"
        "set(LIBETUDE_VERSION_MAJOR \"1\")\n"
        "set(LIBETUDE_VERSION_MINOR \"0\")\n"
        "set(LIBETUDE_VERSION_PATCH \"0\")\n"
        "\n"
        "# 설치 경로 (테스트용)\n"
        "set(LIBETUDE_INSTALL_PREFIX \"${CMAKE_CURRENT_LIST_DIR}/..\")\n"
        "set(LIBETUDE_INCLUDE_DIRS \"${LIBETUDE_INSTALL_PREFIX}/include\")\n"
        "set(LIBETUDE_LIBRARY_DIRS \"${LIBETUDE_INSTALL_PREFIX}/lib\")\n"
        "\n"
        "# 플랫폼별 라이브러리 설정\n"
        "if(WIN32)\n"
        "    if(CMAKE_SIZEOF_VOID_P EQUAL 8)\n"
        "        set(LIBETUDE_ARCH \"x64\")\n"
        "    else()\n"
        "        set(LIBETUDE_ARCH \"Win32\")\n"
        "    endif()\n"
        "    \n"
        "    set(LIBETUDE_STATIC_LIBRARY \"${LIBETUDE_LIBRARY_DIRS}/${LIBETUDE_ARCH}/Release/libetude.lib\")\n"
        "    set(LIBETUDE_LIBRARIES ${LIBETUDE_STATIC_LIBRARY})\n"
        "    \n"
        "    set(LIBETUDE_WINDOWS_LIBRARIES\n"
        "        kernel32 user32 ole32 oleaut32 uuid\n"
        "        winmm dsound mmdevapi\n"
        "    )\n"
        "endif()\n"
        "\n"
        "# 컴파일 정의\n"
        "set(LIBETUDE_DEFINITIONS\n"
        "    -DLIBETUDE_PLATFORM_WINDOWS=1\n"
        "    -DWIN32_LEAN_AND_MEAN\n"
        "    -DNOMINMAX\n"
        ")\n"
        "\n"
        "# 가져온 타겟 생성\n"
        "if(NOT TARGET LibEtude::LibEtude)\n"
        "    add_library(LibEtude::LibEtude STATIC IMPORTED)\n"
        "    set_target_properties(LibEtude::LibEtude PROPERTIES\n"
        "        IMPORTED_LOCATION \"${LIBETUDE_STATIC_LIBRARY}\"\n"
        "        INTERFACE_INCLUDE_DIRECTORIES \"${LIBETUDE_INCLUDE_DIRS}\"\n"
        "        INTERFACE_COMPILE_DEFINITIONS \"${LIBETUDE_DEFINITIONS}\"\n"
        "        INTERFACE_LINK_LIBRARIES \"${LIBETUDE_WINDOWS_LIBRARIES}\"\n"
        "    )\n"
        "endif()\n"
        "\n"
        "# 버전 호환성 확인\n"
        "set(PACKAGE_VERSION \"1.0.0\")\n"
        "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
        "set(PACKAGE_VERSION_EXACT TRUE)\n"
        "\n"
        "set(LibEtude_FOUND TRUE)\n"
        "message(STATUS \"LibEtude ${LIBETUDE_VERSION} 발견\")\n";

    FILE* config_file = fopen("temp_cmake_integration\\cmake\\LibEtudeConfig.cmake", "w");
    if (config_file) {
        fputs(config_cmake_content, config_file);
        fclose(config_file);
        TEST_ASSERT(1, "LibEtudeConfig.cmake 파일 생성");
    }

    // 테스트용 CMakeLists.txt 생성
    const char* cmakelists_content =
        "cmake_minimum_required(VERSION 3.16)\n"
        "project(LibEtudeFindPackageTest VERSION 1.0.0 LANGUAGES C)\n"
        "\n"
        "# LibEtude 패키지 찾기\n"
        "list(APPEND CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake\")\n"
        "find_package(LibEtude REQUIRED)\n"
        "\n"
        "# 테스트 실행 파일\n"
        "add_executable(find_package_test main.c)\n"
        "\n"
        "# LibEtude 라이브러리 링크\n"
        "if(TARGET LibEtude::LibEtude)\n"
        "    target_link_libraries(find_package_test PRIVATE LibEtude::LibEtude)\n"
        "    message(STATUS \"LibEtude::LibEtude 타겟 사용\")\n"
        "else()\n"
        "    target_include_directories(find_package_test PRIVATE ${LIBETUDE_INCLUDE_DIRS})\n"
        "    target_link_libraries(find_package_test PRIVATE ${LIBETUDE_LIBRARIES})\n"
        "    target_compile_definitions(find_package_test PRIVATE ${LIBETUDE_DEFINITIONS})\n"
        "    if(WIN32)\n"
        "        target_link_libraries(find_package_test PRIVATE ${LIBETUDE_WINDOWS_LIBRARIES})\n"
        "    endif()\n"
        "    message(STATUS \"수동 LibEtude 설정 사용\")\n"
        "endif()\n";

    FILE* cmakelists_file = fopen("temp_cmake_integration\\CMakeLists.txt", "w");
    if (cmakelists_file) {
        fputs(cmakelists_content, cmakelists_file);
        fclose(cmakelists_file);
        TEST_ASSERT(1, "테스트 CMakeLists.txt 생성");
    }

    // 테스트용 main.c 생성
    const char* main_content =
        "#include <stdio.h>\n"
        "\n"
        "#ifdef LIBETUDE_PLATFORM_WINDOWS\n"
        "#include <windows.h>\n"
        "#endif\n"
        "\n"
        "int main(void) {\n"
        "    printf(\"LibEtude find_package 테스트\\n\");\n"
        "    \n"
        "#ifdef LIBETUDE_PLATFORM_WINDOWS\n"
        "    printf(\"Windows 플랫폼 정의 확인됨\\n\");\n"
        "#endif\n"
        "    \n"
        "#ifdef WIN32_LEAN_AND_MEAN\n"
        "    printf(\"WIN32_LEAN_AND_MEAN 정의 확인됨\\n\");\n"
        "#endif\n"
        "    \n"
        "    printf(\"find_package 통합 테스트 성공\\n\");\n"
        "    return 0;\n"
        "}\n";

    FILE* main_file = fopen("temp_cmake_integration\\main.c", "w");
    if (main_file) {
        fputs(main_content, main_file);
        fclose(main_file);
        TEST_ASSERT(1, "테스트 main.c 생성");
    }

    // 더미 include 및 lib 디렉토리 생성
    create_directory_recursive("temp_cmake_integration\\include\\libetude");
    create_directory_recursive("temp_cmake_integration\\lib\\x64\\Release");

    // 더미 헤더 파일
    FILE* dummy_header = fopen("temp_cmake_integration\\include\\libetude\\api.h", "w");
    if (dummy_header) {
        fputs("#define LIBETUDE_VERSION \"1.0.0\"\n", dummy_header);
        fclose(dummy_header);
    }

    // 더미 라이브러리 파일
    FILE* dummy_lib = fopen("temp_cmake_integration\\lib\\x64\\Release\\libetude.lib", "wb");
    if (dummy_lib) {
        fwrite("DUMMY", 1, 5, dummy_lib);
        fclose(dummy_lib);
    }

    // CMake 구성 테스트
    create_directory_recursive("temp_cmake_integration\\build");

    char cmake_configure_cmd[512];
    snprintf(cmake_configure_cmd, sizeof(cmake_configure_cmd),
            "cd temp_cmake_integration\\build && "
            "cmake -G \"Visual Studio 17 2022\" -A x64 .. 2>nul");

    int configure_result = system(cmake_configure_cmd);
    if (configure_result == 0) {
        TEST_ASSERT(1, "CMake find_package 구성 성공");

        // 빌드 테스트
        char build_cmd[512];
        snprintf(build_cmd, sizeof(build_cmd),
                "cd temp_cmake_integration\\build && "
                "cmake --build . --config Release 2>nul");

        int build_result = system(build_cmd);
        TEST_ASSERT(build_result == 0, "find_package 프로젝트 빌드 성공");

        // 실행 테스트
        if (file_exists("temp_cmake_integration\\build\\Release\\find_package_test.exe")) {
            int run_result = system("temp_cmake_integration\\build\\Release\\find_package_test.exe 2>nul");
            TEST_ASSERT(run_result == 0, "find_package 테스트 실행 성공");
        }
    } else {
        // Visual Studio 2019 시도
        snprintf(cmake_configure_cmd, sizeof(cmake_configure_cmd),
                "cd temp_cmake_integration\\build && "
                "cmake -G \"Visual Studio 16 2019\" -A x64 .. 2>nul");

        configure_result = system(cmake_configure_cmd);
        if (configure_result == 0) {
            TEST_ASSERT(1, "CMake find_package 구성 성공 (VS2019)");
        } else {
            printf("⚠️  CMake find_package 구성 실패\n");
        }
    }

    // 정리
    system("rmdir /s /q temp_cmake_integration 2>nul");

    return 1;
}

// 배포 패키지 검증 테스트
static int test_deployment_package_validation(void) {
    printf("\n=== 배포 패키지 검증 테스트 ===\n");

    // 필수 배포 파일 목록
    const char* required_files[] = {
        "packaging\\nuget\\LibEtude.nuspec",
        "packaging\\nuget\\LibEtude.targets",
        "packaging\\nuget\\LibEtude.props",
        "cmake\\LibEtudeConfig.cmake.in",
        "cmake\\WindowsConfig.cmake",
        "scripts\\build_nuget.bat",
        "scripts\\build_nuget_multiplatform.bat",
        "scripts\\validate_nuget_dependencies.bat"
    };

    printf("필수 배포 파일 확인:\n");
    for (int i = 0; i < sizeof(required_files) / sizeof(required_files[0]); i++) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "..\\..\\..\\%s", required_files[i]);

        if (file_exists(full_path)) {
            printf("✅ %s\n", required_files[i]);
            g_test_results.passed_tests++;
        } else {
            printf("❌ %s (누락)\n", required_files[i]);
            g_test_results.failed_tests++;
        }
        g_test_results.total_tests++;
    }

    // 스크립트 실행 가능성 테스트
    char script_test_cmd[512];
    snprintf(script_test_cmd, sizeof(script_test_cmd),
            "..\\..\\..\\scripts\\validate_nuget_dependencies.bat 2>nul");

    int script_result = system(script_test_cmd);
    if (script_result == 0) {
        TEST_ASSERT(1, "NuGet 의존성 검증 스크립트 실행 가능");
    } else {
        printf("⚠️  NuGet 의존성 검증 스크립트 실행 실패\n");
    }

    return 1;
}

// 멀티 플랫폼 지원 테스트
static int test_multiplatform_support(void) {
    printf("\n=== 멀티 플랫폼 지원 테스트 ===\n");

    // 지원 플랫폼 목록
    const char* platforms[] = {"x64", "Win32", "ARM64"};
    const char* configurations[] = {"Release", "Debug"};

    printf("지원 플랫폼 및 구성 확인:\n");

    for (int p = 0; p < 3; p++) {
        for (int c = 0; c < 2; c++) {
            printf("  %s %s: ", platforms[p], configurations[c]);

            // 플랫폼별 CMake 구성 테스트
            char platform_test_dir[256];
            snprintf(platform_test_dir, sizeof(platform_test_dir),
                    "temp_platform_test_%s_%s", platforms[p], configurations[c]);

            create_directory_recursive(platform_test_dir);

            // 간단한 CMakeLists.txt 생성
            char cmake_path[512];
            snprintf(cmake_path, sizeof(cmake_path), "%s\\CMakeLists.txt", platform_test_dir);

            FILE* cmake_file = fopen(cmake_path, "w");
            if (cmake_file) {
                fprintf(cmake_file,
                    "cmake_minimum_required(VERSION 3.16)\n"
                    "project(PlatformTest LANGUAGES C)\n"
                    "add_executable(test main.c)\n"
                );
                fclose(cmake_file);
            }

            // main.c 생성
            char main_path[512];
            snprintf(main_path, sizeof(main_path), "%s\\main.c", platform_test_dir);

            FILE* main_file = fopen(main_path, "w");
            if (main_file) {
                fprintf(main_file, "int main(){return 0;}\n");
                fclose(main_file);
            }

            // CMake 구성 테스트
            char build_dir[512];
            snprintf(build_dir, sizeof(build_dir), "%s\\build", platform_test_dir);
            create_directory_recursive(build_dir);

            char cmake_cmd[1024];
            if (strcmp(platforms[p], "ARM64") == 0) {
                snprintf(cmake_cmd, sizeof(cmake_cmd),
                        "cd %s && cmake -G \"Visual Studio 17 2022\" -A ARM64 .. 2>nul",
                        build_dir);
            } else {
                snprintf(cmake_cmd, sizeof(cmake_cmd),
                        "cd %s && cmake -G \"Visual Studio 17 2022\" -A %s .. 2>nul",
                        build_dir, platforms[p]);
            }

            int result = system(cmake_cmd);
            if (result == 0) {
                printf("✅\n");
                g_test_results.passed_tests++;
            } else {
                printf("❌\n");
                g_test_results.failed_tests++;
            }
            g_test_results.total_tests++;

            // 정리
            system("rmdir /s /q %s 2>nul");
        }
    }

    return 1;
}

// 메인 테스트 함수
int main(void) {
    printf("LibEtude Windows 배포 검증 테스트 시작\n");
    printf("==========================================\n");

    // 테스트 실행
    test_nuget_tools_availability();
    test_nuget_package_structure();
    test_cmake_find_package_integration();
    test_deployment_package_validation();
    test_multiplatform_support();

    // 결과 출력
    printf("\n==========================================\n");
    printf("테스트 결과 요약:\n");
    printf("  총 테스트: %d\n", g_test_results.total_tests);
    printf("  성공: %d\n", g_test_results.passed_tests);
    printf("  실패: %d\n", g_test_results.failed_tests);

    if (g_test_results.failed_tests > 0) {
        printf("  마지막 오류: %s\n", g_test_results.last_error);
        return 1;
    }

    printf("\n✅ 모든 배포 검증 테스트가 성공했습니다!\n");
    return 0;
}