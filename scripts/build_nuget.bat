@echo off
REM LibEtude NuGet 패키지 빌드 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

echo LibEtude NuGet 패키지 빌드 시작...

REM 명령줄 인수 처리
set VERSION=%1
set BUILD_CONFIG=%2
set PLATFORM_ARCH=%3

if "%VERSION%"=="" set VERSION=1.0.0
if "%BUILD_CONFIG%"=="" set BUILD_CONFIG=Release
if "%PLATFORM_ARCH%"=="" set PLATFORM_ARCH=x64

echo 빌드 설정:
echo   버전: %VERSION%
echo   구성: %BUILD_CONFIG%
echo   플랫폼: %PLATFORM_ARCH%

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_ROOT%\build_nuget
set PACKAGE_DIR=%PROJECT_ROOT%\packaging\nuget
set OUTPUT_DIR=%PROJECT_ROOT%\packages

REM 빌드 디렉토리 생성
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo 프로젝트 루트: %PROJECT_ROOT%
echo 빌드 디렉토리: %BUILD_DIR%
echo 패키지 디렉토리: %PACKAGE_DIR%
echo 출력 디렉토리: %OUTPUT_DIR%

REM Visual Studio 환경 설정
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

echo Visual Studio 환경 설정 완료

REM 멀티 플랫폼 빌드
echo %PLATFORM_ARCH% 플랫폼 빌드 중...
cd /d "%BUILD_DIR%"

REM CMake 구성
set CMAKE_ARCH=%PLATFORM_ARCH%
if "%PLATFORM_ARCH%"=="Win32" set CMAKE_ARCH=Win32
if "%PLATFORM_ARCH%"=="x64" set CMAKE_ARCH=x64
if "%PLATFORM_ARCH%"=="ARM64" set CMAKE_ARCH=ARM64

cmake -G "Visual Studio 17 2022" -A %CMAKE_ARCH% ^
    -DCMAKE_BUILD_TYPE=%BUILD_CONFIG% ^
    -DLIBETUDE_BUILD_EXAMPLES=OFF ^
    -DLIBETUDE_BUILD_TOOLS=ON ^
    -DLIBETUDE_VERSION=%VERSION% ^
    -DLIBETUDE_ENABLE_WINDOWS_OPTIMIZATIONS=ON ^
    "%PROJECT_ROOT%"
if errorlevel 1 (
    echo CMake 구성 실패
    exit /b 1
)

REM Release 빌드
echo Release %PLATFORM_ARCH% 빌드 중...
cmake --build . --config Release --parallel
if errorlevel 1 (
    echo Release 빌드 실패
    exit /b 1
)

REM Debug 빌드
echo Debug %PLATFORM_ARCH% 빌드 중...
cmake --build . --config Debug --parallel
if errorlevel 1 (
    echo Debug 빌드 실패
    exit /b 1
)

REM 패키지 구조 생성
echo 패키지 구조 생성 중...
set PKG_TEMP=%BUILD_DIR%\nuget_temp
if exist "%PKG_TEMP%" rmdir /s /q "%PKG_TEMP%"
mkdir "%PKG_TEMP%"

REM 헤더 파일 복사
echo 헤더 파일 복사 중...
xcopy "%PROJECT_ROOT%\include" "%PKG_TEMP%\include" /s /i /y
if errorlevel 1 (
    echo 헤더 파일 복사 실패
    exit /b 1
)

REM 라이브러리 파일 복사
echo 라이브러리 파일 복사 중...
mkdir "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Release"
mkdir "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Debug"
mkdir "%PKG_TEMP%\bin\%PLATFORM_ARCH%\Release"
mkdir "%PKG_TEMP%\bin\%PLATFORM_ARCH%\Debug"

REM Release 라이브러리
if exist "%BUILD_DIR%\src\Release\libetude.lib" (
    copy "%BUILD_DIR%\src\Release\libetude.lib" "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Release\" >nul
    echo   정적 라이브러리 복사됨: libetude.lib
)
if exist "%BUILD_DIR%\src\Release\libetude.pdb" (
    copy "%BUILD_DIR%\src\Release\libetude.pdb" "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Release\" >nul
    echo   디버그 정보 복사됨: libetude.pdb
)
if exist "%BUILD_DIR%\src\Release\libetude.dll" (
    copy "%BUILD_DIR%\src\Release\libetude.dll" "%PKG_TEMP%\bin\%PLATFORM_ARCH%\Release\" >nul
    copy "%BUILD_DIR%\src\Release\libetude.lib" "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Release\libetude_import.lib" >nul
    echo   동적 라이브러리 복사됨: libetude.dll
)

REM Debug 라이브러리
if exist "%BUILD_DIR%\src\Debug\libetude.lib" (
    copy "%BUILD_DIR%\src\Debug\libetude.lib" "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Debug\" >nul
    echo   Debug 정적 라이브러리 복사됨: libetude.lib
)
if exist "%BUILD_DIR%\src\Debug\libetude.pdb" (
    copy "%BUILD_DIR%\src\Debug\libetude.pdb" "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Debug\" >nul
    echo   Debug 정보 복사됨: libetude.pdb
)
if exist "%BUILD_DIR%\src\Debug\libetude.dll" (
    copy "%BUILD_DIR%\src\Debug\libetude.dll" "%PKG_TEMP%\bin\%PLATFORM_ARCH%\Debug\" >nul
    copy "%BUILD_DIR%\src\Debug\libetude.lib" "%PKG_TEMP%\lib\%PLATFORM_ARCH%\Debug\libetude_import.lib" >nul
    echo   Debug 동적 라이브러리 복사됨: libetude.dll
)

REM CMake 파일 복사
echo CMake 파일 복사 중...
mkdir "%PKG_TEMP%\cmake"
copy "%PROJECT_ROOT%\cmake\LibEtudeConfig.cmake" "%PKG_TEMP%\cmake\" >nul
copy "%PROJECT_ROOT%\cmake\WindowsConfig.cmake" "%PKG_TEMP%\cmake\" >nul

REM MSBuild 타겟 파일 복사
copy "%PACKAGE_DIR%\LibEtude.targets" "%PKG_TEMP%\" >nul

REM 도구 복사 (선택적)
if exist "%BUILD_DIR%\tools\Release" (
    echo 도구 복사 중...
    xcopy "%BUILD_DIR%\tools\Release" "%PKG_TEMP%\tools" /s /i /y >nul
)

REM 예제 복사
echo 예제 복사 중...
xcopy "%PROJECT_ROOT%\examples" "%PKG_TEMP%\examples" /s /i /y /exclude:"%SCRIPT_DIR%\nuget_exclude.txt" >nul

REM 문서 복사
echo 문서 복사 중...
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
echo   ^<platform^>%PLATFORM_ARCH%^</platform^> >> "%PKG_TEMP%\version.xml"
echo   ^<buildDate^>%DATE% %TIME%^</buildDate^> >> "%PKG_TEMP%\version.xml"
echo ^</version^> >> "%PKG_TEMP%\version.xml"

REM NuGet 패키지 생성
echo NuGet 패키지 생성 중...
cd /d "%PKG_TEMP%"

REM nuget.exe 확인
where nuget.exe >nul 2>&1
if errorlevel 1 (
    echo nuget.exe를 찾을 수 없습니다. NuGet CLI를 설치해주세요.
    echo https://www.nuget.org/downloads 에서 다운로드할 수 있습니다.

    REM dotnet CLI로 대체 시도
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
    REM 패키지 생성
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
echo NuGet 패키지 빌드 완료!
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