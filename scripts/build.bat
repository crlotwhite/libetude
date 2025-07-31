@echo off
REM LibEtude Windows 빌드 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

REM 기본 설정
set BUILD_TYPE=Release
set BUILD_DIR=build
set INSTALL_PREFIX=C:\Program Files\LibEtude
set ENABLE_TESTS=ON
set ENABLE_EXAMPLES=ON
set ENABLE_TOOLS=ON
set ENABLE_BINDINGS=ON
set ENABLE_SIMD=ON
set ENABLE_GPU=ON
set MINIMAL_BUILD=OFF
set PARALLEL_JOBS=%NUMBER_OF_PROCESSORS%
set GENERATOR="Visual Studio 16 2019"

REM 명령행 인수 파싱
:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="-h" goto show_help
if "%~1"=="--help" goto show_help
if "%~1"=="-t" (
    set BUILD_TYPE=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--type" (
    set BUILD_TYPE=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="-d" (
    set BUILD_DIR=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--dir" (
    set BUILD_DIR=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="-p" (
    set INSTALL_PREFIX=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--prefix" (
    set INSTALL_PREFIX=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="-j" (
    set PARALLEL_JOBS=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--jobs" (
    set PARALLEL_JOBS=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--no-tests" (
    set ENABLE_TESTS=OFF
    shift
    goto parse_args
)
if "%~1"=="--no-examples" (
    set ENABLE_EXAMPLES=OFF
    shift
    goto parse_args
)
if "%~1"=="--no-tools" (
    set ENABLE_TOOLS=OFF
    shift
    goto parse_args
)
if "%~1"=="--no-bindings" (
    set ENABLE_BINDINGS=OFF
    shift
    goto parse_args
)
if "%~1"=="--no-simd" (
    set ENABLE_SIMD=OFF
    shift
    goto parse_args
)
if "%~1"=="--no-gpu" (
    set ENABLE_GPU=OFF
    shift
    goto parse_args
)
if "%~1"=="--minimal" (
    set MINIMAL_BUILD=ON
    set ENABLE_TESTS=OFF
    set ENABLE_EXAMPLES=OFF
    set ENABLE_TOOLS=OFF
    set ENABLE_BINDINGS=OFF
    set ENABLE_GPU=OFF
    shift
    goto parse_args
)
if "%~1"=="--clean" (
    set CLEAN_BUILD=1
    shift
    goto parse_args
)
if "%~1"=="--install" (
    set DO_INSTALL=1
    shift
    goto parse_args
)
if "%~1"=="--package" (
    set DO_PACKAGE=1
    shift
    goto parse_args
)
if "%~1"=="--vs2019" (
    set GENERATOR="Visual Studio 16 2019"
    shift
    goto parse_args
)
if "%~1"=="--vs2022" (
    set GENERATOR="Visual Studio 17 2022"
    shift
    goto parse_args
)
echo 알 수 없는 옵션: %~1
goto show_help

:end_parse

echo === LibEtude Windows 빌드 시작 ===
echo 빌드 타입: %BUILD_TYPE%
echo 빌드 디렉토리: %BUILD_DIR%
echo 설치 경로: %INSTALL_PREFIX%
echo 병렬 작업 수: %PARALLEL_JOBS%
echo 생성기: %GENERATOR%

REM 빌드 디렉토리 정리
if defined CLEAN_BUILD (
    if exist "%BUILD_DIR%" (
        echo 빌드 디렉토리 정리 중...
        rmdir /s /q "%BUILD_DIR%"
    )
)

REM 빌드 디렉토리 생성
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM CMake 구성
echo CMake 구성 중...
cmake -B "%BUILD_DIR%" -G %GENERATOR% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
    -DLIBETUDE_BUILD_TESTS=%ENABLE_TESTS% ^
    -DLIBETUDE_BUILD_EXAMPLES=%ENABLE_EXAMPLES% ^
    -DLIBETUDE_BUILD_TOOLS=%ENABLE_TOOLS% ^
    -DLIBETUDE_BUILD_BINDINGS=%ENABLE_BINDINGS% ^
    -DLIBETUDE_ENABLE_SIMD=%ENABLE_SIMD% ^
    -DLIBETUDE_ENABLE_GPU=%ENABLE_GPU% ^
    -DLIBETUDE_MINIMAL=%MINIMAL_BUILD% ^
    .

if errorlevel 1 (
    echo CMake 구성 실패
    exit /b 1
)

REM 빌드 실행
echo 빌드 실행 중...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel %PARALLEL_JOBS%

if errorlevel 1 (
    echo 빌드 실패
    exit /b 1
)

REM 테스트 실행 (활성화된 경우)
if "%ENABLE_TESTS%"=="ON" (
    echo 테스트 실행 중...
    cd "%BUILD_DIR%"
    ctest --output-on-failure --parallel %PARALLEL_JOBS% -C %BUILD_TYPE%
    cd ..
)

REM 설치 실행 (요청된 경우)
if defined DO_INSTALL (
    echo 설치 실행 중...
    cmake --install "%BUILD_DIR%" --config %BUILD_TYPE%
)

REM 패키지 생성 (요청된 경우)
if defined DO_PACKAGE (
    echo 패키지 생성 중...
    cd "%BUILD_DIR%"
    cpack -C %BUILD_TYPE%
    cd ..
)

echo === 빌드 완료 ===
goto end

:show_help
echo LibEtude Windows 빌드 스크립트
echo.
echo 사용법: %0 [옵션]
echo.
echo 옵션:
echo     -h, --help              이 도움말 표시
echo     -t, --type TYPE         빌드 타입 (Debug, Release, RelWithDebInfo, MinSizeRel)
echo     -d, --dir DIR           빌드 디렉토리 (기본값: build)
echo     -p, --prefix PREFIX     설치 경로 (기본값: C:\Program Files\LibEtude)
echo     -j, --jobs N            병렬 빌드 작업 수 (기본값: CPU 코어 수)
echo     --no-tests              테스트 빌드 비활성화
echo     --no-examples           예제 빌드 비활성화
echo     --no-tools              도구 빌드 비활성화
echo     --no-bindings           바인딩 빌드 비활성화
echo     --no-simd               SIMD 최적화 비활성화
echo     --no-gpu                GPU 가속 비활성화
echo     --minimal               최소 빌드 (임베디드용)
echo     --clean                 빌드 디렉토리 정리 후 빌드
echo     --install               빌드 후 설치 실행
echo     --package               빌드 후 패키지 생성
echo     --vs2019                Visual Studio 2019 사용
echo     --vs2022                Visual Studio 2022 사용
echo.
echo 예제:
echo     %0                      기본 빌드
echo     %0 --type Debug         디버그 빌드
echo     %0 --minimal --no-gpu   최소 빌드 (GPU 비활성화)
echo     %0 --clean --install    정리 후 빌드 및 설치

:end
endlocal