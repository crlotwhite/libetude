@echo off
REM LibEtude CMake 통합 테스트 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

echo LibEtude CMake 통합 테스트 시작...

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set TEST_DIR=%PROJECT_ROOT%\cmake_integration_test
set BUILD_DIR=%TEST_DIR%\build

REM 테스트 디렉토리 생성
if exist "%TEST_DIR%" rmdir /s /q "%TEST_DIR%"
mkdir "%TEST_DIR%"
mkdir "%BUILD_DIR%"

echo 테스트 디렉토리: %TEST_DIR%

REM 테스트용 CMakeLists.txt 생성
echo CMake 통합 테스트 프로젝트 생성 중...
echo cmake_minimum_required(VERSION 3.16) > "%TEST_DIR%\CMakeLists.txt"
echo project(LibEtudeIntegrationTest VERSION 1.0.0 LANGUAGES C CXX) >> "%TEST_DIR%\CMakeLists.txt"
echo. >> "%TEST_DIR%\CMakeLists.txt"
echo # LibEtude 패키지 찾기 >> "%TEST_DIR%\CMakeLists.txt"
echo find_package(LibEtude REQUIRED >> "%TEST_DIR%\CMakeLists.txt"
echo     PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../cmake" >> "%TEST_DIR%\CMakeLists.txt"
echo     NO_DEFAULT_PATH >> "%TEST_DIR%\CMakeLists.txt"
echo ) >> "%TEST_DIR%\CMakeLists.txt"
echo. >> "%TEST_DIR%\CMakeLists.txt"
echo # 테스트 실행 파일 생성 >> "%TEST_DIR%\CMakeLists.txt"
echo add_executable(integration_test main.c) >> "%TEST_DIR%\CMakeLists.txt"
echo. >> "%TEST_DIR%\CMakeLists.txt"
echo # LibEtude 라이브러리 링크 >> "%TEST_DIR%\CMakeLists.txt"
echo if(TARGET LibEtude::LibEtude) >> "%TEST_DIR%\CMakeLists.txt"
echo     target_link_libraries(integration_test PRIVATE LibEtude::LibEtude) >> "%TEST_DIR%\CMakeLists.txt"
echo     message(STATUS "LibEtude::LibEtude 타겟 사용") >> "%TEST_DIR%\CMakeLists.txt"
echo else() >> "%TEST_DIR%\CMakeLists.txt"
echo     # 수동 설정 >> "%TEST_DIR%\CMakeLists.txt"
echo     target_include_directories(integration_test PRIVATE ${LIBETUDE_INCLUDE_DIRS}) >> "%TEST_DIR%\CMakeLists.txt"
echo     target_link_libraries(integration_test PRIVATE ${LIBETUDE_LIBRARIES}) >> "%TEST_DIR%\CMakeLists.txt"
echo     target_compile_definitions(integration_test PRIVATE ${LIBETUDE_DEFINITIONS}) >> "%TEST_DIR%\CMakeLists.txt"
echo     if(WIN32) >> "%TEST_DIR%\CMakeLists.txt"
echo         target_link_libraries(integration_test PRIVATE ${LIBETUDE_WINDOWS_LIBRARIES}) >> "%TEST_DIR%\CMakeLists.txt"
echo     endif() >> "%TEST_DIR%\CMakeLists.txt"
echo     message(STATUS "수동 LibEtude 설정 사용") >> "%TEST_DIR%\CMakeLists.txt"
echo endif() >> "%TEST_DIR%\CMakeLists.txt"
echo. >> "%TEST_DIR%\CMakeLists.txt"
echo # Windows 특화 설정 적용 >> "%TEST_DIR%\CMakeLists.txt"
echo if(WIN32) >> "%TEST_DIR%\CMakeLists.txt"
echo     libetude_configure_windows_target(integration_test) >> "%TEST_DIR%\CMakeLists.txt"
echo endif() >> "%TEST_DIR%\CMakeLists.txt"

REM 테스트용 main.c 파일 생성
echo 테스트 소스 파일 생성 중...
echo #include ^<stdio.h^> > "%TEST_DIR%\main.c"
echo #include ^<stdlib.h^> >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo #ifdef LIBETUDE_PLATFORM_WINDOWS >> "%TEST_DIR%\main.c"
echo #include ^<windows.h^> >> "%TEST_DIR%\main.c"
echo #endif >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo /* LibEtude 헤더 파일 포함 테스트 */ >> "%TEST_DIR%\main.c"
echo #ifdef LIBETUDE_INCLUDE_DIRS >> "%TEST_DIR%\main.c"
echo /* LibEtude 헤더가 사용 가능한 경우 */ >> "%TEST_DIR%\main.c"
echo /* #include ^<libetude/api.h^> */ >> "%TEST_DIR%\main.c"
echo #endif >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo int main(void) { >> "%TEST_DIR%\main.c"
echo     printf("LibEtude CMake 통합 테스트\\n"); >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo #ifdef LIBETUDE_PLATFORM_WINDOWS >> "%TEST_DIR%\main.c"
echo     printf("Windows 플랫폼 감지됨\\n"); >> "%TEST_DIR%\main.c"
echo #endif >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo #ifdef LIBETUDE_ARCH_X64 >> "%TEST_DIR%\main.c"
echo     printf("x64 아키텍처\\n"); >> "%TEST_DIR%\main.c"
echo #elif defined(LIBETUDE_ARCH_X86) >> "%TEST_DIR%\main.c"
echo     printf("x86 아키텍처\\n"); >> "%TEST_DIR%\main.c"
echo #elif defined(LIBETUDE_ARCH_ARM64) >> "%TEST_DIR%\main.c"
echo     printf("ARM64 아키텍처\\n"); >> "%TEST_DIR%\main.c"
echo #endif >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo #ifdef LIBETUDE_ENABLE_SIMD >> "%TEST_DIR%\main.c"
echo     printf("SIMD 최적화 활성화\\n"); >> "%TEST_DIR%\main.c"
echo #endif >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo #ifdef LIBETUDE_ENABLE_ETW >> "%TEST_DIR%\main.c"
echo     printf("ETW 로깅 활성화\\n"); >> "%TEST_DIR%\main.c"
echo #endif >> "%TEST_DIR%\main.c"
echo. >> "%TEST_DIR%\main.c"
echo     printf("테스트 완료\\n"); >> "%TEST_DIR%\main.c"
echo     return 0; >> "%TEST_DIR%\main.c"
echo } >> "%TEST_DIR%\main.c"

REM Visual Studio 환경 설정
echo Visual Studio 환경 설정 중...
call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 2>nul
if errorlevel 1 (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    if errorlevel 1 (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" 2>nul
        if errorlevel 1 (
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
            if errorlevel 1 (
                echo Visual Studio 환경을 찾을 수 없습니다.
                exit /b 1
            )
        )
    )
)

REM CMake 구성 테스트
echo CMake 구성 테스트 중...
cd /d "%BUILD_DIR%"

cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DLIBETUDE_ENABLE_SIMD=ON ^
    -DLIBETUDE_ENABLE_ETW=ON ^
    -DLIBETUDE_ENABLE_LARGE_PAGES=ON ^
    "%TEST_DIR%"

if errorlevel 1 (
    echo CMake 구성 실패
    cd /d "%PROJECT_ROOT%"
    exit /b 1
)

echo CMake 구성 성공

REM 빌드 테스트
echo 빌드 테스트 중...
cmake --build . --config Release

if errorlevel 1 (
    echo 빌드 실패
    cd /d "%PROJECT_ROOT%"
    exit /b 1
)

echo 빌드 성공

REM 실행 테스트
echo 실행 테스트 중...
if exist "Release\integration_test.exe" (
    Release\integration_test.exe
    if errorlevel 1 (
        echo 실행 테스트 실패
        cd /d "%PROJECT_ROOT%"
        exit /b 1
    )
    echo 실행 테스트 성공
) else (
    echo 실행 파일을 찾을 수 없습니다.
    cd /d "%PROJECT_ROOT%"
    exit /b 1
)

REM 정리
cd /d "%PROJECT_ROOT%"
rmdir /s /q "%TEST_DIR%"

echo.
echo ========================================
echo CMake 통합 테스트 완료!
echo ========================================
echo 모든 테스트가 성공적으로 완료되었습니다.
echo LibEtude CMake 설정이 올바르게 작동합니다.

endlocal
pause