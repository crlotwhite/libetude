@echo off
REM LibEtude 플랫폼 추상화 레이어 테스트 실행 스크립트 (Windows)

setlocal enabledelayedexpansion

REM 기본 설정
set RUN_UNIT=false
set RUN_INTEGRATION=false
set RUN_MOCKING=false
set RUN_ALL=true
set VERBOSE=false
set CI_MODE=false
set REPORT_FILE=
set BUILD_DIR=build

REM CI 환경 감지
if defined CI set CI_MODE=true
if defined GITHUB_ACTIONS set CI_MODE=true
if defined JENKINS_URL set CI_MODE=true

if "%CI_MODE%"=="true" (
    echo [INFO] CI 환경이 감지되었습니다
)

REM 명령행 인수 파싱
:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="-u" (
    set RUN_UNIT=true
    set RUN_ALL=false
    shift
    goto parse_args
)
if "%~1"=="--unit" (
    set RUN_UNIT=true
    set RUN_ALL=false
    shift
    goto parse_args
)
if "%~1"=="-i" (
    set RUN_INTEGRATION=true
    set RUN_ALL=false
    shift
    goto parse_args
)
if "%~1"=="--integration" (
    set RUN_INTEGRATION=true
    set RUN_ALL=false
    shift
    goto parse_args
)
if "%~1"=="-m" (
    set RUN_MOCKING=true
    set RUN_ALL=false
    shift
    goto parse_args
)
if "%~1"=="--mocking" (
    set RUN_MOCKING=true
    set RUN_ALL=false
    shift
    goto parse_args
)
if "%~1"=="-a" (
    set RUN_ALL=true
    shift
    goto parse_args
)
if "%~1"=="--all" (
    set RUN_ALL=true
    shift
    goto parse_args
)
if "%~1"=="-v" (
    set VERBOSE=true
    shift
    goto parse_args
)
if "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto parse_args
)
if "%~1"=="-c" (
    set CI_MODE=true
    shift
    goto parse_args
)
if "%~1"=="--ci" (
    set CI_MODE=true
    shift
    goto parse_args
)
if "%~1"=="-r" (
    set REPORT_FILE=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--report" (
    set REPORT_FILE=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="-h" goto usage
if "%~1"=="--help" goto usage

echo [ERROR] 알 수 없는 옵션: %~1
goto usage

:end_parse

REM 빌드 디렉토리 확인
if not exist "%BUILD_DIR%" (
    echo [ERROR] 빌드 디렉토리를 찾을 수 없습니다: %BUILD_DIR%
    echo [INFO] 먼저 프로젝트를 빌드해주세요:
    echo [INFO]   cmake -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Debug
    echo [INFO]   cmake --build %BUILD_DIR%
    exit /b 1
)

REM 테스트 결과 추적
set TOTAL_TESTS=0
set PASSED_TESTS=0
set FAILED_TESTS=0

echo [INFO] LibEtude 플랫폼 추상화 레이어 테스트 시작
echo [INFO] ========================================

REM 단위 테스트 실행
if "%RUN_ALL%"=="true" (
    call :run_test "단위 테스트" "test_platform_abstraction.exe" "플랫폼 추상화 레이어 단위 테스트"
)
if "%RUN_UNIT%"=="true" (
    call :run_test "단위 테스트" "test_platform_abstraction.exe" "플랫폼 추상화 레이어 단위 테스트"
)

REM 통합 테스트 실행
if "%RUN_ALL%"=="true" (
    call :run_test "통합 테스트" "test_platform_integration.exe" "플랫폼 추상화 레이어 통합 테스트"
)
if "%RUN_INTEGRATION%"=="true" (
    call :run_test "통합 테스트" "test_platform_integration.exe" "플랫폼 추상화 레이어 통합 테스트"
)

REM 모킹 테스트 실행
if "%RUN_ALL%"=="true" (
    call :run_test "모킹 테스트" "test_platform_mocking.exe" "플랫폼 추상화 레이어 모킹 프레임워크 테스트"
)
if "%RUN_MOCKING%"=="true" (
    call :run_test "모킹 테스트" "test_platform_mocking.exe" "플랫폼 추상화 레이어 모킹 프레임워크 테스트"
)

REM 결과 요약
echo.
echo [INFO] 테스트 결과 요약
echo [INFO] ================
echo 총 테스트: !TOTAL_TESTS!
echo 통과: !PASSED_TESTS!
echo 실패: !FAILED_TESTS!

REM 테스트 보고서 생성
if not "%REPORT_FILE%"=="" (
    echo [INFO] 테스트 보고서 생성 중: %REPORT_FILE%
    (
        echo LibEtude 플랫폼 추상화 레이어 테스트 보고서
        echo ==========================================
        echo.
        echo 실행 시간: %date% %time%
        echo 총 테스트: !TOTAL_TESTS!
        echo 통과: !PASSED_TESTS!
        echo 실패: !FAILED_TESTS!
        echo.
        echo 테스트 환경:
        echo - OS: Windows
        echo - 빌드 디렉토리: %BUILD_DIR%
        echo - CI 모드: %CI_MODE%
    ) > "%REPORT_FILE%"
    echo [SUCCESS] 테스트 보고서가 생성되었습니다: %REPORT_FILE%
)

REM CI 환경용 출력
if "%CI_MODE%"=="true" (
    if defined GITHUB_ACTIONS (
        echo ::set-output name=total_tests::!TOTAL_TESTS!
        echo ::set-output name=passed_tests::!PASSED_TESTS!
        echo ::set-output name=failed_tests::!FAILED_TESTS!

        if !FAILED_TESTS! gtr 0 (
            echo ::error::!FAILED_TESTS!개의 테스트가 실패했습니다
        )
    )

    if defined JENKINS_URL (
        echo JENKINS_TEST_RESULTS=total:!TOTAL_TESTS!,passed:!PASSED_TESTS!,failed:!FAILED_TESTS!
    )
)

REM 최종 결과
if !FAILED_TESTS! equ 0 (
    echo [SUCCESS] 모든 테스트가 성공적으로 완료되었습니다! ✓
    exit /b 0
) else (
    echo [ERROR] !FAILED_TESTS!개의 테스트가 실패했습니다 ✗
    exit /b 1
)

REM 테스트 실행 함수
:run_test
set test_name=%~1
set test_executable=%~2
set description=%~3

echo [INFO] 실행 중: %description%

if not exist "%BUILD_DIR%\tests\%test_executable%" (
    echo [WARNING] 테스트 실행 파일을 찾을 수 없습니다: %test_executable%
    set /a TOTAL_TESTS+=1
    set /a FAILED_TESTS+=1
    goto :eof
)

set cmd=%BUILD_DIR%\tests\%test_executable%
if "%VERBOSE%"=="true" (
    set cmd=!cmd! --verbose
)

set /a TOTAL_TESTS+=1

if "%CI_MODE%"=="true" (
    REM CI 환경에서는 타임아웃 설정 (Windows에서는 timeout 명령 사용)
    timeout /t 300 /nobreak > nul & !cmd!
    if !errorlevel! neq 0 (
        echo [ERROR] %test_name% 테스트가 실패했습니다 ^(종료 코드: !errorlevel!^)
        set /a FAILED_TESTS+=1
        goto :eof
    )
) else (
    !cmd!
    if !errorlevel! neq 0 (
        echo [ERROR] %test_name% 테스트가 실패했습니다
        set /a FAILED_TESTS+=1
        goto :eof
    )
)

echo [SUCCESS] %test_name% 테스트 완료
set /a PASSED_TESTS+=1
goto :eof

:usage
echo 사용법: %0 [옵션]
echo 옵션:
echo   -u, --unit          단위 테스트만 실행
echo   -i, --integration   통합 테스트만 실행
echo   -m, --mocking       모킹 테스트만 실행
echo   -a, --all           모든 테스트 실행 ^(기본값^)
echo   -v, --verbose       상세 출력
echo   -c, --ci            CI 환경 모드
echo   -r, --report FILE   테스트 보고서 파일 생성
echo   -h, --help          도움말 출력
exit /b 1