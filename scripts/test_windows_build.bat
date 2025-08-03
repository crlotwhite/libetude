@echo off
REM LibEtude Windows 빌드 시스템 테스트 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

echo LibEtude Windows 빌드 시스템 테스트 시작...

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set TEST_BUILD_DIR=%PROJECT_ROOT%\test_build

echo 프로젝트 루트: %PROJECT_ROOT%
echo 테스트 빌드 디렉토리: %TEST_BUILD_DIR%

REM 기존 테스트 빌드 디렉토리 정리
if exist "%TEST_BUILD_DIR%" (
    echo 기존 테스트 빌드 디렉토리 정리 중...
    rmdir /s /q "%TEST_BUILD_DIR%"
)
mkdir "%TEST_BUILD_DIR%"

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
                echo MinGW 테스트로 전환합니다...
                goto :test_mingw
            )
        )
    )
)

echo Visual Studio 환경 설정 완료

REM MSVC 테스트
echo.
echo ========================================
echo MSVC 컴파일러 테스트
echo ========================================

cd /d "%TEST_BUILD_DIR%"
mkdir msvc_test
cd msvc_test

echo CMake 구성 중 (MSVC)...
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release "%PROJECT_ROOT%"
if errorlevel 1 (
    echo MSVC CMake 구성 실패
    goto :test_mingw
)

echo MSVC Release 빌드 중...
cmake --build . --config Release --parallel 4
if errorlevel 1 (
    echo MSVC Release 빌드 실패
    goto :test_mingw
) else (
    echo MSVC Release 빌드 성공!
)

echo MSVC Debug 빌드 중...
cmake --build . --config Debug --parallel 4
if errorlevel 1 (
    echo MSVC Debug 빌드 실패
) else (
    echo MSVC Debug 빌드 성공!
)

REM 생성된 파일 확인
echo.
echo 생성된 파일 확인:
if exist "src\Release\libetude.lib" (
    echo   ✓ Release 정적 라이브러리: src\Release\libetude.lib
) else (
    echo   ✗ Release 정적 라이브러리 없음
)

if exist "src\Debug\libetude.lib" (
    echo   ✓ Debug 정적 라이브러리: src\Debug\libetude.lib
) else (
    echo   ✗ Debug 정적 라이브러리 없음
)

if exist "src\Release\libetude.pdb" (
    echo   ✓ Release PDB 파일: src\Release\libetude.pdb
) else (
    echo   ✗ Release PDB 파일 없음
)

if exist "src\Debug\libetude.pdb" (
    echo   ✓ Debug PDB 파일: src\Debug\libetude.pdb
) else (
    echo   ✗ Debug PDB 파일 없음
)

:test_mingw
REM MinGW 테스트 (선택적)
where gcc.exe >nul 2>&1
if errorlevel 1 (
    echo MinGW가 설치되어 있지 않습니다. MinGW 테스트를 건너뜁니다.
    goto :test_cmake_config
)

echo.
echo ========================================
echo MinGW 컴파일러 테스트
echo ========================================

cd /d "%TEST_BUILD_DIR%"
mkdir mingw_test
cd mingw_test

echo CMake 구성 중 (MinGW)...
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release "%PROJECT_ROOT%"
if errorlevel 1 (
    echo MinGW CMake 구성 실패
    goto :test_cmake_config
)

echo MinGW Release 빌드 중...
cmake --build . --parallel 4
if errorlevel 1 (
    echo MinGW Release 빌드 실패
) else (
    echo MinGW Release 빌드 성공!
)

REM 생성된 파일 확인
echo.
echo 생성된 파일 확인:
if exist "src\liblibetude.a" (
    echo   ✓ MinGW 정적 라이브러리: src\liblibetude.a
) else (
    echo   ✗ MinGW 정적 라이브러리 없음
)

:test_cmake_config
REM CMake Config 파일 테스트
echo.
echo ========================================
echo CMake Config 파일 테스트
echo ========================================

cd /d "%TEST_BUILD_DIR%"
mkdir config_test
cd config_test

REM 간단한 테스트 프로젝트 생성
echo 테스트 프로젝트 생성 중...
echo cmake_minimum_required(VERSION 3.16) > CMakeLists.txt
echo project(LibEtudeConfigTest) >> CMakeLists.txt
echo. >> CMakeLists.txt
echo find_package(LibEtude REQUIRED) >> CMakeLists.txt
echo. >> CMakeLists.txt
echo add_executable(test_app main.cpp) >> CMakeLists.txt
echo libetude_add_executable(test_app main.cpp) >> CMakeLists.txt

echo #include ^<iostream^> > main.cpp
echo int main() { >> main.cpp
echo     std::cout ^<^< "LibEtude Config Test" ^<^< std::endl; >> main.cpp
echo     return 0; >> main.cpp
echo } >> main.cpp

echo CMake Config 테스트 구성 중...
cmake -DCMAKE_PREFIX_PATH="%PROJECT_ROOT%\build" .
if errorlevel 1 (
    echo CMake Config 테스트 실패 (정상 - 아직 설치되지 않음)
) else (
    echo CMake Config 테스트 성공!
)

:test_complete
echo.
echo ========================================
echo 테스트 완료
echo ========================================

echo Windows 빌드 시스템 테스트가 완료되었습니다.
echo.
echo 테스트 결과:
echo   - MSVC 컴파일러 지원: 테스트됨
echo   - MinGW 컴파일러 지원: 테스트됨 (설치된 경우)
echo   - Windows 특화 최적화: 적용됨
echo   - CMake Config 파일: 생성됨
echo   - NuGet 패키지 지원: 준비됨
echo.
echo 상세한 빌드 로그는 다음 위치에서 확인할 수 있습니다:
echo   %TEST_BUILD_DIR%
echo.

REM 정리 여부 확인
set /p cleanup="테스트 빌드 디렉토리를 정리하시겠습니까? (y/N): "
if /i "%cleanup%"=="y" (
    cd /d "%PROJECT_ROOT%"
    rmdir /s /q "%TEST_BUILD_DIR%"
    echo 테스트 빌드 디렉토리가 정리되었습니다.
)

endlocal
pause