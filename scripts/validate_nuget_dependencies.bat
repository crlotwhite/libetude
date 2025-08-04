@echo off
REM LibEtude NuGet 패키지 의존성 검증 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

echo LibEtude NuGet 패키지 의존성 검증 시작...

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set PACKAGE_DIR=%PROJECT_ROOT%\packaging\nuget

REM 필수 도구 확인
echo 필수 도구 확인 중...

REM Visual Studio 확인
call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" 2>nul
if errorlevel 1 (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    if errorlevel 1 (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" 2>nul
        if errorlevel 1 (
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
            if errorlevel 1 (
                echo [오류] Visual Studio를 찾을 수 없습니다.
                echo        Visual Studio 2019 또는 2022를 설치해주세요.
                exit /b 1
            )
        )
    )
)
echo [확인] Visual Studio 환경 설정 완료

REM CMake 확인
where cmake.exe >nul 2>&1
if errorlevel 1 (
    echo [오류] CMake를 찾을 수 없습니다.
    echo        https://cmake.org/download/ 에서 CMake를 설치해주세요.
    exit /b 1
)
echo [확인] CMake 사용 가능

REM NuGet CLI 또는 dotnet CLI 확인
where nuget.exe >nul 2>&1
if errorlevel 1 (
    where dotnet.exe >nul 2>&1
    if errorlevel 1 (
        echo [경고] nuget.exe와 dotnet.exe를 모두 찾을 수 없습니다.
        echo        패키지 생성을 위해 다음 중 하나를 설치해주세요:
        echo        - NuGet CLI: https://www.nuget.org/downloads
        echo        - .NET SDK: https://dotnet.microsoft.com/download
        exit /b 1
    ) else (
        echo [확인] .NET CLI 사용 가능
    )
) else (
    echo [확인] NuGet CLI 사용 가능
)

REM Windows SDK 확인
echo Windows SDK 확인 중...
set SDK_FOUND=0

REM Windows 10/11 SDK 경로 확인
for /d %%i in ("%ProgramFiles(x86)%\Windows Kits\10\Include\*") do (
    if exist "%%i\um\windows.h" (
        echo [확인] Windows SDK 발견: %%i
        set SDK_FOUND=1
    )
)

if !SDK_FOUND! == 0 (
    echo [경고] Windows SDK를 찾을 수 없습니다.
    echo        Visual Studio Installer를 통해 Windows SDK를 설치해주세요.
)

REM 필수 시스템 라이브러리 확인
echo 시스템 라이브러리 확인 중...
set REQUIRED_LIBS=kernel32.lib user32.lib ole32.lib oleaut32.lib uuid.lib winmm.lib dsound.lib mmdevapi.lib

for %%L in (%REQUIRED_LIBS%) do (
    set LIB_FOUND=0
    for %%P in ("%LIB%") do (
        if exist "%%~P\%%L" (
            set LIB_FOUND=1
        )
    )
    if !LIB_FOUND! == 0 (
        echo [경고] 라이브러리를 찾을 수 없습니다: %%L
    ) else (
        echo [확인] 라이브러리 사용 가능: %%L
    )
)

REM 프로젝트 구조 확인
echo 프로젝트 구조 확인 중...

set REQUIRED_DIRS=src include cmake packaging\nuget scripts
for %%D in (%REQUIRED_DIRS%) do (
    if not exist "%PROJECT_ROOT%\%%D" (
        echo [오류] 필수 디렉토리가 없습니다: %%D
        exit /b 1
    ) else (
        echo [확인] 디렉토리 존재: %%D
    )
)

REM 필수 파일 확인
echo 필수 파일 확인 중...

set REQUIRED_FILES=CMakeLists.txt README.md LICENSE
for %%F in (%REQUIRED_FILES%) do (
    if not exist "%PROJECT_ROOT%\%%F" (
        echo [오류] 필수 파일이 없습니다: %%F
        exit /b 1
    ) else (
        echo [확인] 파일 존재: %%F
    )
)

REM NuGet 관련 파일 확인
set NUGET_FILES=LibEtude.nuspec LibEtude.targets LibEtude.props
for %%F in (%NUGET_FILES%) do (
    if not exist "%PACKAGE_DIR%\%%F" (
        echo [오류] NuGet 파일이 없습니다: %%F
        exit /b 1
    ) else (
        echo [확인] NuGet 파일 존재: %%F
    )
)

REM CMake 설정 파일 확인
set CMAKE_FILES=WindowsConfig.cmake LibEtudeConfig.cmake.in
for %%F in (%CMAKE_FILES%) do (
    if not exist "%PROJECT_ROOT%\cmake\%%F" (
        echo [오류] CMake 파일이 없습니다: %%F
        exit /b 1
    ) else (
        echo [확인] CMake 파일 존재: %%F
    )
)

REM 빌드 스크립트 확인
set BUILD_SCRIPTS=build_nuget.bat build_nuget_multiplatform.bat
for %%S in (%BUILD_SCRIPTS%) do (
    if not exist "%SCRIPT_DIR%\%%S" (
        echo [오류] 빌드 스크립트가 없습니다: %%S
        exit /b 1
    ) else (
        echo [확인] 빌드 스크립트 존재: %%S
    )
)

REM 디스크 공간 확인
echo 디스크 공간 확인 중...
for /f "tokens=3" %%i in ('dir /-c "%PROJECT_ROOT%" ^| find "bytes free"') do (
    set FREE_SPACE=%%i
)

REM 최소 5GB 필요 (대략적인 계산)
if !FREE_SPACE! LSS 5368709120 (
    echo [경고] 디스크 공간이 부족할 수 있습니다. 최소 5GB 이상 권장합니다.
) else (
    echo [확인] 충분한 디스크 공간 사용 가능
)

REM 권한 확인
echo 권한 확인 중...
mkdir "%PROJECT_ROOT%\temp_permission_test" 2>nul
if errorlevel 1 (
    echo [경고] 프로젝트 디렉토리에 쓰기 권한이 없을 수 있습니다.
) else (
    rmdir "%PROJECT_ROOT%\temp_permission_test" 2>nul
    echo [확인] 프로젝트 디렉토리 쓰기 권한 확인
)

echo.
echo ========================================
echo 의존성 검증 완료
echo ========================================

echo 빌드를 시작하려면 다음 명령을 실행하세요:
echo   단일 플랫폼: %SCRIPT_DIR%build_nuget.bat [버전] [구성] [플랫폼]
echo   멀티 플랫폼: %SCRIPT_DIR%build_nuget_multiplatform.bat [버전]
echo.
echo 예시:
echo   %SCRIPT_DIR%build_nuget.bat 1.0.0 Release x64
echo   %SCRIPT_DIR%build_nuget_multiplatform.bat 1.0.0

endlocal
pause