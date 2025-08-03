@echo off
REM LibEtude NuGet 패키지 빌드 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

echo LibEtude NuGet 패키지 빌드 시작...

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

REM Release 빌드 (x64)
echo Release x64 빌드 중...
cd /d "%BUILD_DIR%"
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DLIBETUDE_BUILD_EXAMPLES=OFF -DLIBETUDE_BUILD_TOOLS=OFF "%PROJECT_ROOT%"
if errorlevel 1 (
    echo CMake 구성 실패
    exit /b 1
)

cmake --build . --config Release --parallel
if errorlevel 1 (
    echo Release 빌드 실패
    exit /b 1
)

REM Debug 빌드 (x64)
echo Debug x64 빌드 중...
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
mkdir "%PKG_TEMP%\lib\Release"
mkdir "%PKG_TEMP%\lib\Debug"
mkdir "%PKG_TEMP%\bin\Release"
mkdir "%PKG_TEMP%\bin\Debug"

REM Release 라이브러리
copy "%BUILD_DIR%\src\Release\libetude.lib" "%PKG_TEMP%\lib\Release\" >nul
copy "%BUILD_DIR%\src\Release\libetude.pdb" "%PKG_TEMP%\lib\Release\" >nul
if exist "%BUILD_DIR%\src\Release\libetude.dll" (
    copy "%BUILD_DIR%\src\Release\libetude.dll" "%PKG_TEMP%\bin\Release\" >nul
)

REM Debug 라이브러리
copy "%BUILD_DIR%\src\Debug\libetude.lib" "%PKG_TEMP%\lib\Debug\" >nul
copy "%BUILD_DIR%\src\Debug\libetude.pdb" "%PKG_TEMP%\lib\Debug\" >nul
if exist "%BUILD_DIR%\src\Debug\libetude.dll" (
    copy "%BUILD_DIR%\src\Debug\libetude.dll" "%PKG_TEMP%\bin\Debug\" >nul
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

REM NuGet 패키지 생성
echo NuGet 패키지 생성 중...
cd /d "%PKG_TEMP%"

REM nuget.exe 확인
where nuget.exe >nul 2>&1
if errorlevel 1 (
    echo nuget.exe를 찾을 수 없습니다. NuGet CLI를 설치해주세요.
    echo https://www.nuget.org/downloads 에서 다운로드할 수 있습니다.
    exit /b 1
)

REM 패키지 생성
nuget pack "%PACKAGE_DIR%\LibEtude.nuspec" -OutputDirectory "%OUTPUT_DIR%" -BasePath "%PKG_TEMP%"
if errorlevel 1 (
    echo NuGet 패키지 생성 실패
    exit /b 1
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