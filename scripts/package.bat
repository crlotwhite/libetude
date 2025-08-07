@echo off
REM LibEtude 패키징 스크립트 (Windows)
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

REM 기본 설정
set "BUILD_TYPE=Release"
set "PACKAGE_TYPE=ZIP"
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "OUTPUT_DIR=%PROJECT_ROOT%\packages"
set "BUILD_DIR=%PROJECT_ROOT%\build-package"
set "CLEAN_BUILD=false"
set "PLATFORM_ONLY=false"
set "MINIMAL_BUILD=false"
set "SKIP_TESTS=false"
set "KEEP_BUILD_DIR=false"

REM 색상 출력 함수 (Windows 10 이상)
set "ESC="

:print_info
echo [INFO] %~1
goto :eof

:print_success
echo [SUCCESS] %~1
goto :eof

:print_error
echo [ERROR] %~1
goto :eof

:print_warning
echo [WARNING] %~1
goto :eof

REM 사용법 출력
:show_usage
echo LibEtude 패키징 스크립트 (Windows)
echo.
echo 사용법: %~nx0 [옵션]
echo.
echo 옵션:
echo     -t, --type TYPE         패키지 타입 (ZIP, NSIS)
echo     -b, --build-type TYPE   빌드 타입 (Debug, Release, RelWithDebInfo)
echo     -o, --output DIR        출력 디렉토리
echo     -c, --clean             빌드 디렉토리 정리
echo     -p, --platform-only     플랫폼 추상화 레이어만 패키징
echo     -m, --minimal           최소 빌드로 패키징
echo     -h, --help              이 도움말 표시
echo.
echo 환경 변수:
echo     BUILD_TYPE              빌드 타입 (기본값: Release)
echo     PACKAGE_TYPE            패키지 타입 (기본값: ZIP)
echo     OUTPUT_DIR              출력 디렉토리 (기본값: .\packages)
echo     BUILD_DIR               빌드 디렉토리 (기본값: .\build-package)
echo.
echo 예시:
echo     %~nx0 -t NSIS -b Release                # NSIS 설치 프로그램 생성
echo     %~nx0 -t ZIP -o C:\temp\packages        # ZIP 패키지를 C:\temp\packages에 생성
echo     %~nx0 -t ZIP -p                         # 플랫폼 추상화 레이어만 ZIP으로 패키징
echo     %~nx0 -t NSIS -m                        # 최소 빌드로 NSIS 설치 프로그램 생성
echo.
goto :eof

REM 의존성 확인
:check_dependencies
call :print_info "의존성 확인 중..."

REM CMake 확인
cmake --version >nul 2>&1
if errorlevel 1 (
    call :print_error "CMake가 설치되지 않았습니다"
    exit /b 1
)

REM Visual Studio 빌드 도구 확인
where msbuild >nul 2>&1
if errorlevel 1 (
    call :print_error "MSBuild가 설치되지 않았습니다. Visual Studio 또는 Build Tools를 설치하세요"
    exit /b 1
)

REM 패키지 타입별 의존성 확인
if "%PACKAGE_TYPE%"=="NSIS" (
    where makensis >nul 2>&1
    if errorlevel 1 (
        call :print_error "NSIS가 설치되지 않았습니다"
        exit /b 1
    )
)

call :print_success "의존성 확인 완료"
goto :eof

REM 빌드 디렉토리 준비
:prepare_build_dir
call :print_info "빌드 디렉토리 준비 중..."

if "%CLEAN_BUILD%"=="true" (
    if exist "%BUILD_DIR%" (
        call :print_info "기존 빌드 디렉토리 정리: %BUILD_DIR%"
        rmdir /s /q "%BUILD_DIR%"
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

call :print_success "빌드 디렉토리 준비 완료: %BUILD_DIR%"
goto :eof

REM CMake 설정
:configure_cmake
call :print_info "CMake 설정 중..."

cd /d "%BUILD_DIR%"

REM CMake 옵션 설정
set CMAKE_OPTS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%
set CMAKE_OPTS=%CMAKE_OPTS% -DCMAKE_INSTALL_PREFIX="C:/Program Files/LibEtude"
set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_EXAMPLES=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_TOOLS=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_BINDINGS=ON
set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_ENABLE_PLATFORM_ABSTRACTION=ON

REM 플랫폼 추상화 레이어 전용 빌드
if "%PLATFORM_ONLY%"=="true" (
    set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_EXAMPLES=OFF
    set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_TOOLS=OFF
    set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_BINDINGS=OFF
    call :print_info "플랫폼 추상화 레이어 전용 빌드 설정"
)

REM 최소 빌드
if "%MINIMAL_BUILD%"=="true" (
    set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_MINIMAL=ON
    set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_ENABLE_GPU=OFF
    set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_EXAMPLES=OFF
    set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_BUILD_TOOLS=OFF
    call :print_info "최소 빌드 설정"
)

REM Windows 특화 설정
set CMAKE_OPTS=%CMAKE_OPTS% -DCPACK_GENERATOR=%PACKAGE_TYPE%
set CMAKE_OPTS=%CMAKE_OPTS% -DLIBETUDE_ENABLE_SIMD=ON

REM Visual Studio 생성기 감지
set VS_GENERATOR=
for /f "tokens=*" %%i in ('where msbuild 2^>nul') do (
    echo %%i | findstr "2022" >nul && set VS_GENERATOR="Visual Studio 17 2022"
    echo %%i | findstr "2019" >nul && set VS_GENERATOR="Visual Studio 16 2019"
)

if defined VS_GENERATOR (
    set CMAKE_OPTS=%CMAKE_OPTS% -G %VS_GENERATOR% -A x64
    call :print_info "Visual Studio 생성기: %VS_GENERATOR%"
) else (
    call :print_info "기본 생성기 사용"
)

call :print_info "CMake 명령: cmake %CMAKE_OPTS% %PROJECT_ROOT%"
cmake %CMAKE_OPTS% "%PROJECT_ROOT%"
if errorlevel 1 (
    call :print_error "CMake 설정 실패"
    exit /b 1
)

call :print_success "CMake 설정 완료"
goto :eof

REM 빌드 실행
:build_project
call :print_info "프로젝트 빌드 중..."

cd /d "%BUILD_DIR%"

REM 병렬 빌드 설정
set PARALLEL_JOBS=%NUMBER_OF_PROCESSORS%
if not defined PARALLEL_JOBS set PARALLEL_JOBS=4

cmake --build . --config %BUILD_TYPE% --parallel %PARALLEL_JOBS%
if errorlevel 1 (
    call :print_error "빌드 실패"
    exit /b 1
)

call :print_success "빌드 완료"
goto :eof

REM 테스트 실행 (선택적)
:run_tests
if "%SKIP_TESTS%"=="true" (
    call :print_warning "테스트 건너뜀"
    goto :eof
)

call :print_info "테스트 실행 중..."

cd /d "%BUILD_DIR%"
ctest --output-on-failure --parallel %PARALLEL_JOBS% -C %BUILD_TYPE%
if errorlevel 1 (
    call :print_warning "일부 테스트가 실패했습니다"
) else (
    call :print_success "테스트 완료"
)
goto :eof

REM 패키지 생성
:create_package
call :print_info "%PACKAGE_TYPE% 패키지 생성 중..."

cd /d "%BUILD_DIR%"

if "%PACKAGE_TYPE%"=="ZIP" (
    cpack -G ZIP -C %BUILD_TYPE%
) else if "%PACKAGE_TYPE%"=="NSIS" (
    cpack -G NSIS -C %BUILD_TYPE%
) else (
    call :print_error "지원하지 않는 패키지 타입: %PACKAGE_TYPE%"
    exit /b 1
)

if errorlevel 1 (
    call :print_error "패키지 생성 실패"
    exit /b 1
)

REM 생성된 패키지 파일을 출력 디렉토리로 이동
for %%f in (*.zip *.exe) do (
    if exist "%%f" (
        move "%%f" "%OUTPUT_DIR%\"
        call :print_success "패키지 생성 완료: %OUTPUT_DIR%\%%f"
    )
)
goto :eof

REM 패키지 검증
:verify_package
call :print_info "패키지 검증 중..."

for %%f in ("%OUTPUT_DIR%\*.zip" "%OUTPUT_DIR%\*.exe") do (
    if exist "%%f" (
        call :print_info "패키지 크기: %%~nxf - %%~zf bytes"
    )
)

call :print_success "패키지 검증 완료"
goto :eof

REM 정리 작업
:cleanup
if "%KEEP_BUILD_DIR%"=="true" (
    call :print_info "빌드 디렉토리 유지: %BUILD_DIR%"
    goto :eof
)

call :print_info "빌드 디렉토리 정리 중..."
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
call :print_success "정리 완료"
goto :eof

REM 메인 함수
:main
call :print_info "LibEtude 패키징 시작"
call :print_info "빌드 타입: %BUILD_TYPE%"
call :print_info "패키지 타입: %PACKAGE_TYPE%"
call :print_info "출력 디렉토리: %OUTPUT_DIR%"

call :check_dependencies
if errorlevel 1 exit /b 1

call :prepare_build_dir
if errorlevel 1 exit /b 1

call :configure_cmake
if errorlevel 1 exit /b 1

call :build_project
if errorlevel 1 exit /b 1

call :run_tests

call :create_package
if errorlevel 1 exit /b 1

call :verify_package

call :cleanup

call :print_success "패키징 완료!"
call :print_info "생성된 패키지는 %OUTPUT_DIR% 디렉토리에 있습니다"
goto :eof

REM 명령행 인수 처리
:parse_args
if "%~1"=="" goto main

if "%~1"=="-t" (
    set "PACKAGE_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--type" (
    set "PACKAGE_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="-b" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--build-type" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="-o" (
    set "OUTPUT_DIR=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="--output" (
    set "OUTPUT_DIR=%~2"
    shift
    shift
    goto parse_args
)
if "%~1"=="-c" (
    set "CLEAN_BUILD=true"
    shift
    goto parse_args
)
if "%~1"=="--clean" (
    set "CLEAN_BUILD=true"
    shift
    goto parse_args
)
if "%~1"=="-p" (
    set "PLATFORM_ONLY=true"
    shift
    goto parse_args
)
if "%~1"=="--platform-only" (
    set "PLATFORM_ONLY=true"
    shift
    goto parse_args
)
if "%~1"=="-m" (
    set "MINIMAL_BUILD=true"
    shift
    goto parse_args
)
if "%~1"=="--minimal" (
    set "MINIMAL_BUILD=true"
    shift
    goto parse_args
)
if "%~1"=="--skip-tests" (
    set "SKIP_TESTS=true"
    shift
    goto parse_args
)
if "%~1"=="--keep-build-dir" (
    set "KEEP_BUILD_DIR=true"
    shift
    goto parse_args
)
if "%~1"=="-h" (
    call :show_usage
    exit /b 0
)
if "%~1"=="--help" (
    call :show_usage
    exit /b 0
)

call :print_error "알 수 없는 옵션: %~1"
call :show_usage
exit /b 1

REM 스크립트 시작점
call :parse_args %*