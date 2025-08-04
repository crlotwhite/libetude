@echo off
REM LibEtude 멀티 플랫폼 NuGet 패키지 빌드 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

echo LibEtude 멀티 플랫폼 NuGet 패키지 빌드 시작...

REM 명령줄 인수 처리
set VERSION=%1
if "%VERSION%"=="" set VERSION=1.0.0

echo 빌드 버전: %VERSION%

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set OUTPUT_DIR=%PROJECT_ROOT%\packages

REM 출력 디렉토리 생성
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM 지원 플랫폼 목록
set PLATFORMS=x64 Win32 ARM64

REM 각 플랫폼별로 빌드
for %%P in (%PLATFORMS%) do (
    echo.
    echo ========================================
    echo %%P 플랫폼 빌드 시작
    echo ========================================

    call "%SCRIPT_DIR%build_nuget.bat" %VERSION% Release %%P
    if errorlevel 1 (
        echo %%P 플랫폼 빌드 실패
        exit /b 1
    )

    echo %%P 플랫폼 빌드 완료
)

echo.
echo ========================================
echo 모든 플랫폼 빌드 완료
echo ========================================

REM 통합 패키지 생성
echo 통합 NuGet 패키지 생성 중...

set BUILD_DIR=%PROJECT_ROOT%\build_nuget_multi
set PACKAGE_DIR=%PROJECT_ROOT%\packaging\nuget

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

REM 모든 플랫폼의 빌드 결과를 통합
set PKG_TEMP=%BUILD_DIR%\nuget_temp
mkdir "%PKG_TEMP%"

REM 헤더 파일 복사 (한 번만)
echo 헤더 파일 복사 중...
xcopy "%PROJECT_ROOT%\include" "%PKG_TEMP%\include" /s /i /y
if errorlevel 1 (
    echo 헤더 파일 복사 실패
    exit /b 1
)

REM 각 플랫폼별 라이브러리 복사
for %%P in (%PLATFORMS%) do (
    echo %%P 플랫폼 라이브러리 복사 중...

    set PLATFORM_BUILD_DIR=%PROJECT_ROOT%\build_nuget_%%P

    REM 디렉토리 생성
    mkdir "%PKG_TEMP%\lib\%%P\Release" 2>nul
    mkdir "%PKG_TEMP%\lib\%%P\Debug" 2>nul
    mkdir "%PKG_TEMP%\bin\%%P\Release" 2>nul
    mkdir "%PKG_TEMP%\bin\%%P\Debug" 2>nul

    REM Release 라이브러리
    if exist "!PLATFORM_BUILD_DIR!\src\Release\libetude.lib" (
        copy "!PLATFORM_BUILD_DIR!\src\Release\libetude.lib" "%PKG_TEMP%\lib\%%P\Release\" >nul
        echo   %%P Release 정적 라이브러리 복사됨
    )
    if exist "!PLATFORM_BUILD_DIR!\src\Release\libetude.pdb" (
        copy "!PLATFORM_BUILD_DIR!\src\Release\libetude.pdb" "%PKG_TEMP%\lib\%%P\Release\" >nul
    )
    if exist "!PLATFORM_BUILD_DIR!\src\Release\libetude.dll" (
        copy "!PLATFORM_BUILD_DIR!\src\Release\libetude.dll" "%PKG_TEMP%\bin\%%P\Release\" >nul
        copy "!PLATFORM_BUILD_DIR!\src\Release\libetude.lib" "%PKG_TEMP%\lib\%%P\Release\libetude_import.lib" >nul
        echo   %%P Release 동적 라이브러리 복사됨
    )

    REM Debug 라이브러리
    if exist "!PLATFORM_BUILD_DIR!\src\Debug\libetude.lib" (
        copy "!PLATFORM_BUILD_DIR!\src\Debug\libetude.lib" "%PKG_TEMP%\lib\%%P\Debug\" >nul
        echo   %%P Debug 정적 라이브러리 복사됨
    )
    if exist "!PLATFORM_BUILD_DIR!\src\Debug\libetude.pdb" (
        copy "!PLATFORM_BUILD_DIR!\src\Debug\libetude.pdb" "%PKG_TEMP%\lib\%%P\Debug\" >nul
    )
    if exist "!PLATFORM_BUILD_DIR!\src\Debug\libetude.dll" (
        copy "!PLATFORM_BUILD_DIR!\src\Debug\libetude.dll" "%PKG_TEMP%\bin\%%P\Debug\" >nul
        copy "!PLATFORM_BUILD_DIR!\src\Debug\libetude.lib" "%PKG_TEMP%\lib\%%P\Debug\libetude_import.lib" >nul
        echo   %%P Debug 동적 라이브러리 복사됨
    )
)

REM CMake 파일 복사
echo CMake 파일 복사 중...
mkdir "%PKG_TEMP%\cmake"
copy "%PROJECT_ROOT%\cmake\LibEtudeConfig.cmake" "%PKG_TEMP%\cmake\" >nul
copy "%PROJECT_ROOT%\cmake\WindowsConfig.cmake" "%PKG_TEMP%\cmake\" >nul

REM MSBuild 파일 복사
copy "%PACKAGE_DIR%\LibEtude.targets" "%PKG_TEMP%\" >nul
copy "%PACKAGE_DIR%\LibEtude.props" "%PKG_TEMP%\" >nul

REM 도구 복사 (x64 버전 사용)
if exist "%PROJECT_ROOT%\build_nuget_x64\tools\Release" (
    echo 도구 복사 중...
    xcopy "%PROJECT_ROOT%\build_nuget_x64\tools\Release" "%PKG_TEMP%\tools" /s /i /y >nul
)

REM 예제 복사
echo 예제 복사 중...
xcopy "%PROJECT_ROOT%\examples" "%PKG_TEMP%\examples" /s /i /y /exclude:"%SCRIPT_DIR%\nuget_exclude.txt" >nul

REM 문서 복사
echo 문서 복사 중...
mkdir "%PKG_TEMP%\docs"
copy "%PROJECT_ROOT%\README.md" "%PKG_TEMP%\docs\" >nul
copy "%PROJECT_ROOT%\LICENSE" "%PKG_TEMP%\" >nul
if exist "%PROJECT_ROOT%\docs" (
    xcopy "%PROJECT_ROOT%\docs" "%PKG_TEMP%\docs" /s /i /y >nul
)

REM 버전 정보 파일 생성
echo 버전 정보 파일 생성 중...
echo ^<?xml version="1.0" encoding="utf-8"?^> > "%PKG_TEMP%\version.xml"
echo ^<version^> >> "%PKG_TEMP%\version.xml"
echo   ^<number^>%VERSION%^</number^> >> "%PKG_TEMP%\version.xml"
echo   ^<platforms^>x64,Win32,ARM64^</platforms^> >> "%PKG_TEMP%\version.xml"
echo   ^<buildDate^>%DATE% %TIME%^</buildDate^> >> "%PKG_TEMP%\version.xml"
echo ^</version^> >> "%PKG_TEMP%\version.xml"

REM NuGet 패키지 생성
echo 통합 NuGet 패키지 생성 중...
cd /d "%PKG_TEMP%"

REM nuget.exe 확인
where nuget.exe >nul 2>&1
if errorlevel 1 (
    echo nuget.exe를 찾을 수 없습니다. dotnet CLI를 시도합니다...
    where dotnet.exe >nul 2>&1
    if errorlevel 1 (
        echo dotnet CLI도 찾을 수 없습니다.
        exit /b 1
    ) else (
        echo dotnet CLI를 사용하여 패키지를 생성합니다...
        dotnet pack "%PACKAGE_DIR%\LibEtude.nuspec" -o "%OUTPUT_DIR%" -p:BasePath="%PKG_TEMP%" -p:Version=%VERSION%
        if errorlevel 1 (
            echo dotnet pack 실패
            exit /b 1
        )
    )
) else (
    nuget pack "%PACKAGE_DIR%\LibEtude.nuspec" -OutputDirectory "%OUTPUT_DIR%" -BasePath "%PKG_TEMP%" -Version %VERSION%
    if errorlevel 1 (
        echo NuGet 패키지 생성 실패
        exit /b 1
    )
)

REM 임시 디렉토리 정리
cd /d "%PROJECT_ROOT%"
rmdir /s /q "%PKG_TEMP%"

echo.
echo ========================================
echo 멀티 플랫폼 NuGet 패키지 빌드 완료!
echo ========================================
echo 출력 위치: %OUTPUT_DIR%
dir "%OUTPUT_DIR%\*.nupkg"

echo.
echo 패키지 설치 방법:
echo   Visual Studio Package Manager Console에서:
echo   Install-Package LibEtude -Source "%OUTPUT_DIR%"
echo.
echo   또는 nuget.exe 사용:
echo   nuget install LibEtude -Source "%OUTPUT_DIR%"

endlocal
pause