/*
 * LibEtude Windows 빌드 시스템 통합 테스트
 * Copyright (c) 2025 LibEtude Project
 *
 * 이 파일은 Windows 환경에서 Visual Studio 2019/2022 및 MinGW 빌드 시스템의
 * 통합 테스트를 구현합니다.
 *
 * 요구사항: 1.1, 1.2 - Windows 특화 컴파일러 플래그와 최적화 적용
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

static int execute_command(const char* command, char* output, size_t output_size) {
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

// 컴파일러 감지 테스트
static int test_compiler_detection(void) {
    printf("\n=== 컴파일러 감지 테스트 ===\n");

    char output[1024];
    int result;

    // MSVC 컴파일러 확인
    result = execute_command("cl.exe 2>&1", output, sizeof(output));
    if (result == 0 || strstr(output, "Microsoft") != NULL) {
        TEST_ASSERT(1, "MSVC 컴파일러 감지됨");

        // MSVC 버전 확인
        if (strstr(output, "19.2") != NULL) {
            TEST_ASSERT(1, "Visual Studio 2019 감지됨");
        } else if (strstr(output, "19.3") != NULL) {
            TEST_ASSERT(1, "Visual Studio 2022 감지됨");
        } else {
            printf("⚠️  알 수 없는 MSVC 버전: %s\n", output);
        }
    } else {
        printf("⚠️  MSVC 컴파일러를 찾을 수 없습니다\n");
    }

    // MinGW 컴파일러 확인
    result = execute_command("gcc --version 2>&1", output, sizeof(output));
    if (result == 0 && strstr(output, "gcc") != NULL) {
        TEST_ASSERT(1, "MinGW GCC 컴파일러 감지됨");

        // MinGW 버전 확인
        if (strstr(output, "mingw") != NULL || strstr(output, "w64") != NULL) {
            TEST_ASSERT(1, "MinGW-w64 감지됨");
        }
    } else {
        printf("⚠️  MinGW 컴파일러를 찾을 수 없습니다\n");
    }

    // CMake 확인
    result = execute_command("cmake --version 2>&1", output, sizeof(output));
    TEST_ASSERT(result == 0 && strstr(output, "cmake") != NULL, "CMake 사용 가능");

    return 1;
}

// Windows SDK 감지 테스트
static int test_windows_sdk_detection(void) {
    printf("\n=== Windows SDK 감지 테스트 ===\n");

    // Windows Kits 디렉토리 확인
    const char* sdk_paths[] = {
        "C:\\Program Files (x86)\\Windows Kits\\10",
        "C:\\Program Files\\Windows Kits\\10"
    };

    int sdk_found = 0;
    for (int i = 0; i < 2; i++) {
        if (directory_exists(sdk_paths[i])) {
            printf("✅ Windows SDK 경로 발견: %s\n", sdk_paths[i]);
            sdk_found = 1;

            // Include 디렉토리 확인
            char include_path[512];
            snprintf(include_path, sizeof(include_path), "%s\\Include", sdk_paths[i]);
            TEST_ASSERT(directory_exists(include_path), "Windows SDK Include 디렉토리 존재");

            // Lib 디렉토리 확인
            char lib_path[512];
            snprintf(lib_path, sizeof(lib_path), "%s\\Lib", sdk_paths[i]);
            TEST_ASSERT(directory_exists(lib_path), "Windows SDK Lib 디렉토리 존재");

            break;
        }
    }

    if (!sdk_found) {
        printf("⚠️  Windows SDK를 찾을 수 없습니다\n");
    }

    // 필수 헤더 파일 확인
    char header_check_code[] =
        "#include <windows.h>\n"
        "#include <mmdeviceapi.h>\n"
        "#include <dsound.h>\n"
        "int main() { return 0; }\n";

    FILE* test_file = fopen("temp_header_test.c", "w");
    if (test_file) {
        fputs(header_check_code, test_file);
        fclose(test_file);

        char compile_cmd[512];
        snprintf(compile_cmd, sizeof(compile_cmd),
                "cl.exe /nologo temp_header_test.c /link /subsystem:console 2>nul");

        int result = system(compile_cmd);
        TEST_ASSERT(result == 0, "Windows 헤더 파일 컴파일 테스트 성공");

        // 정리
        remove("temp_header_test.c");
        remove("temp_header_test.obj");
        remove("temp_header_test.exe");
    }

    return 1;
}

// CMake 빌드 시스템 테스트
static int test_cmake_build_system(void) {
    printf("\n=== CMake 빌드 시스템 테스트 ===\n");

    // 임시 테스트 프로젝트 생성
    const char* test_dir = "temp_cmake_test";

    // 디렉토리 생성
    if (!directory_exists(test_dir)) {
        TEST_ASSERT(_mkdir(test_dir) == 0, "테스트 디렉토리 생성");
    }

    // CMakeLists.txt 생성
    char cmake_file_path[256];
    snprintf(cmake_file_path, sizeof(cmake_file_path), "%s\\CMakeLists.txt", test_dir);

    FILE* cmake_file = fopen(cmake_file_path, "w");
    TEST_ASSERT(cmake_file != NULL, "CMakeLists.txt 파일 생성");

    if (cmake_file) {
        fprintf(cmake_file,
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(LibEtudeTest VERSION 1.0.0 LANGUAGES C)\n"
            "\n"
            "# Windows 특화 설정 테스트\n"
            "if(WIN32)\n"
            "    if(MSVC)\n"
            "        add_compile_options(/W4 /WX /O2 /Oi /Ot /Oy)\n"
            "        add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)\n"
            "    elseif(MINGW)\n"
            "        add_compile_options(-Wall -Wextra -O3 -march=native)\n"
            "        add_compile_definitions(WIN32_LEAN_AND_MEAN)\n"
            "    endif()\n"
            "endif()\n"
            "\n"
            "# 테스트 실행 파일\n"
            "add_executable(build_test main.c)\n"
            "\n"
            "# Windows 라이브러리 링크\n"
            "if(WIN32)\n"
            "    target_link_libraries(build_test PRIVATE\n"
            "        kernel32 user32 ole32 oleaut32 uuid\n"
            "        winmm dsound mmdevapi\n"
            "    )\n"
            "endif()\n"
        );
        fclose(cmake_file);
    }

    // main.c 생성
    char main_file_path[256];
    snprintf(main_file_path, sizeof(main_file_path), "%s\\main.c", test_dir);

    FILE* main_file = fopen(main_file_path, "w");
    TEST_ASSERT(main_file != NULL, "main.c 파일 생성");

    if (main_file) {
        fprintf(main_file,
            "#include <stdio.h>\n"
            "#include <windows.h>\n"
            "#include <mmdeviceapi.h>\n"
            "\n"
            "int main(void) {\n"
            "    printf(\"LibEtude Windows 빌드 테스트\\n\");\n"
            "    \n"
            "#ifdef _MSC_VER\n"
            "    printf(\"MSVC 컴파일러 버전: %%d\\n\", _MSC_VER);\n"
            "#endif\n"
            "    \n"
            "#ifdef __MINGW32__\n"
            "    printf(\"MinGW 컴파일러 감지\\n\");\n"
            "#endif\n"
            "    \n"
            "#ifdef WIN32_LEAN_AND_MEAN\n"
            "    printf(\"WIN32_LEAN_AND_MEAN 정의됨\\n\");\n"
            "#endif\n"
            "    \n"
            "    // Windows API 테스트\n"
            "    HRESULT hr = CoInitialize(NULL);\n"
            "    if (SUCCEEDED(hr)) {\n"
            "        printf(\"COM 초기화 성공\\n\");\n"
            "        CoUninitialize();\n"
            "    }\n"
            "    \n"
            "    return 0;\n"
            "}\n"
        );
        fclose(main_file);
    }

    // CMake 구성 테스트
    char build_dir[256];
    snprintf(build_dir, sizeof(build_dir), "%s\\build", test_dir);

    if (!directory_exists(build_dir)) {
        TEST_ASSERT(_mkdir(build_dir) == 0, "빌드 디렉토리 생성");
    }

    // Visual Studio 빌드 테스트
    char cmake_cmd[512];
    snprintf(cmake_cmd, sizeof(cmake_cmd),
            "cd %s && cmake -G \"Visual Studio 17 2022\" -A x64 .. 2>nul",
            build_dir);

    int cmake_result = system(cmake_cmd);
    if (cmake_result == 0) {
        TEST_ASSERT(1, "Visual Studio 2022 CMake 구성 성공");

        // 빌드 테스트
        char build_cmd[512];
        snprintf(build_cmd, sizeof(build_cmd),
                "cd %s && cmake --build . --config Release 2>nul",
                build_dir);

        int build_result = system(build_cmd);
        TEST_ASSERT(build_result == 0, "Visual Studio 빌드 성공");

        // 실행 테스트
        char exe_path[512];
        snprintf(exe_path, sizeof(exe_path), "%s\\Release\\build_test.exe", build_dir);

        if (file_exists(exe_path)) {
            TEST_ASSERT(1, "실행 파일 생성됨");

            char run_cmd[512];
            snprintf(run_cmd, sizeof(run_cmd), "%s 2>nul", exe_path);

            int run_result = system(run_cmd);
            TEST_ASSERT(run_result == 0, "빌드된 실행 파일 실행 성공");
        }
    } else {
        // Visual Studio 2019 시도
        snprintf(cmake_cmd, sizeof(cmake_cmd),
                "cd %s && cmake -G \"Visual Studio 16 2019\" -A x64 .. 2>nul",
                build_dir);

        cmake_result = system(cmake_cmd);
        if (cmake_result == 0) {
            TEST_ASSERT(1, "Visual Studio 2019 CMake 구성 성공");
        } else {
            printf("⚠️  Visual Studio CMake 구성 실패\n");
        }
    }

    // MinGW 빌드 테스트 (MinGW가 설치된 경우)
    char mingw_build_dir[256];
    snprintf(mingw_build_dir, sizeof(mingw_build_dir), "%s\\build_mingw", test_dir);

    if (!directory_exists(mingw_build_dir)) {
        _mkdir(mingw_build_dir);
    }

    snprintf(cmake_cmd, sizeof(cmake_cmd),
            "cd %s && cmake -G \"MinGW Makefiles\" .. 2>nul",
            mingw_build_dir);

    cmake_result = system(cmake_cmd);
    if (cmake_result == 0) {
        TEST_ASSERT(1, "MinGW CMake 구성 성공");

        char mingw_build_cmd[512];
        snprintf(mingw_build_cmd, sizeof(mingw_build_cmd),
                "cd %s && cmake --build . 2>nul",
                mingw_build_dir);

        int mingw_build_result = system(mingw_build_cmd);
        TEST_ASSERT(mingw_build_result == 0, "MinGW 빌드 성공");
    } else {
        printf("⚠️  MinGW를 사용할 수 없습니다\n");
    }

    // 정리
    system("rmdir /s /q temp_cmake_test 2>nul");

    return 1;
}

// 컴파일러 최적화 플래그 테스트
static int test_compiler_optimization_flags(void) {
    printf("\n=== 컴파일러 최적화 플래그 테스트 ===\n");

    // MSVC 최적화 플래그 테스트
    const char* test_code =
        "#include <stdio.h>\n"
        "#include <immintrin.h>\n"
        "int main() {\n"
        "#ifdef _MSC_VER\n"
        "    printf(\"MSVC 최적화 레벨: \");\n"
        "#ifdef _DEBUG\n"
        "    printf(\"Debug\\n\");\n"
        "#else\n"
        "    printf(\"Release\\n\");\n"
        "#endif\n"
        "#endif\n"
        "#ifdef __AVX2__\n"
        "    printf(\"AVX2 지원\\n\");\n"
        "#endif\n"
        "#ifdef __AVX__\n"
        "    printf(\"AVX 지원\\n\");\n"
        "#endif\n"
        "    return 0;\n"
        "}\n";

    FILE* test_file = fopen("optimization_test.c", "w");
    if (test_file) {
        fputs(test_code, test_file);
        fclose(test_file);

        // MSVC 최적화 테스트
        char msvc_cmd[] = "cl.exe /nologo /O2 /Oi /Ot /Oy /arch:AVX2 optimization_test.c 2>nul";
        int msvc_result = system(msvc_cmd);

        if (msvc_result == 0) {
            TEST_ASSERT(1, "MSVC 최적화 플래그 적용 성공");

            // 실행하여 최적화 확인
            int run_result = system("optimization_test.exe 2>nul");
            TEST_ASSERT(run_result == 0, "최적화된 실행 파일 실행 성공");
        } else {
            printf("⚠️  MSVC 최적화 테스트 실패\n");
        }

        // MinGW 최적화 테스트
        char mingw_cmd[] = "gcc -O3 -march=native -mavx2 optimization_test.c -o optimization_test_mingw.exe 2>nul";
        int mingw_result = system(mingw_cmd);

        if (mingw_result == 0) {
            TEST_ASSERT(1, "MinGW 최적화 플래그 적용 성공");

            int mingw_run_result = system("optimization_test_mingw.exe 2>nul");
            TEST_ASSERT(mingw_run_result == 0, "MinGW 최적화된 실행 파일 실행 성공");
        } else {
            printf("⚠️  MinGW 최적화 테스트 실패\n");
        }

        // 정리
        remove("optimization_test.c");
        remove("optimization_test.obj");
        remove("optimization_test.exe");
        remove("optimization_test_mingw.exe");
    }

    return 1;
}

// Windows 특화 라이브러리 링크 테스트
static int test_windows_library_linking(void) {
    printf("\n=== Windows 라이브러리 링크 테스트 ===\n");

    const char* link_test_code =
        "#include <stdio.h>\n"
        "#include <windows.h>\n"
        "#include <mmdeviceapi.h>\n"
        "#include <dsound.h>\n"
        "#include <mmsystem.h>\n"
        "\n"
        "int main() {\n"
        "    printf(\"Windows 라이브러리 링크 테스트\\n\");\n"
        "    \n"
        "    // COM 초기화 테스트\n"
        "    HRESULT hr = CoInitialize(NULL);\n"
        "    if (SUCCEEDED(hr)) {\n"
        "        printf(\"COM 초기화 성공\\n\");\n"
        "        CoUninitialize();\n"
        "    }\n"
        "    \n"
        "    // DirectSound 테스트\n"
        "    LPDIRECTSOUND8 ds = NULL;\n"
        "    hr = DirectSoundCreate8(NULL, &ds, NULL);\n"
        "    if (SUCCEEDED(hr) && ds) {\n"
        "        printf(\"DirectSound 생성 성공\\n\");\n"
        "        ds->lpVtbl->Release(ds);\n"
        "    }\n"
        "    \n"
        "    // 멀티미디어 타이머 테스트\n"
        "    UINT timer_id = timeSetEvent(100, 10, NULL, 0, TIME_ONESHOT);\n"
        "    if (timer_id != 0) {\n"
        "        printf(\"멀티미디어 타이머 생성 성공\\n\");\n"
        "        timeKillEvent(timer_id);\n"
        "    }\n"
        "    \n"
        "    return 0;\n"
        "}\n";

    FILE* link_test_file = fopen("link_test.c", "w");
    if (link_test_file) {
        fputs(link_test_code, link_test_file);
        fclose(link_test_file);

        // MSVC 링크 테스트
        char msvc_link_cmd[] =
            "cl.exe /nologo link_test.c "
            "kernel32.lib user32.lib ole32.lib oleaut32.lib uuid.lib "
            "winmm.lib dsound.lib 2>nul";

        int msvc_link_result = system(msvc_link_cmd);
        TEST_ASSERT(msvc_link_result == 0, "MSVC Windows 라이브러리 링크 성공");

        if (msvc_link_result == 0) {
            int run_result = system("link_test.exe 2>nul");
            TEST_ASSERT(run_result == 0, "링크된 실행 파일 실행 성공");
        }

        // MinGW 링크 테스트
        char mingw_link_cmd[] =
            "gcc link_test.c -o link_test_mingw.exe "
            "-lkernel32 -luser32 -lole32 -loleaut32 -luuid "
            "-lwinmm -ldsound 2>nul";

        int mingw_link_result = system(mingw_link_cmd);
        if (mingw_link_result == 0) {
            TEST_ASSERT(1, "MinGW Windows 라이브러리 링크 성공");

            int mingw_run_result = system("link_test_mingw.exe 2>nul");
            TEST_ASSERT(mingw_run_result == 0, "MinGW 링크된 실행 파일 실행 성공");
        } else {
            printf("⚠️  MinGW 라이브러리 링크 실패\n");
        }

        // 정리
        remove("link_test.c");
        remove("link_test.obj");
        remove("link_test.exe");
        remove("link_test_mingw.exe");
    }

    return 1;
}

// 메인 테스트 함수
int main(void) {
    printf("LibEtude Windows 빌드 시스템 통합 테스트 시작\n");
    printf("=================================================\n");

    // 테스트 실행
    test_compiler_detection();
    test_windows_sdk_detection();
    test_cmake_build_system();
    test_compiler_optimization_flags();
    test_windows_library_linking();

    // 결과 출력
    printf("\n=================================================\n");
    printf("테스트 결과 요약:\n");
    printf("  총 테스트: %d\n", g_test_results.total_tests);
    printf("  성공: %d\n", g_test_results.passed_tests);
    printf("  실패: %d\n", g_test_results.failed_tests);

    if (g_test_results.failed_tests > 0) {
        printf("  마지막 오류: %s\n", g_test_results.last_error);
        return 1;
    }

    printf("\n✅ 모든 빌드 시스템 테스트가 성공했습니다!\n");
    return 0;
}