@echo off
REM LibEtude 빌드 시스템 테스트 실행 스크립트
REM Copyright (c) 2025 LibEtude Project
REM
REM 이 스크립트는 작업 9.3의 모든 빌드 시스템 및 배포 테스트를 실행합니다.

setlocal enabledelayedexpansion

echo LibEtude 빌드 시스템 테스트 실행
echo ===================================

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set TEST_RESULTS_DIR=%PROJECT_ROOT%\test_results

REM 결과 디렉토리 생성
if not exist "%TEST_RESULTS_DIR%" mkdir "%TEST_RESULTS_DIR%"

REM 테스트 결과 변수
set TOTAL_TESTS=0
set PASSED_TESTS=0
set FAILED_TESTS=0

REM 로그 파일 설정
set LOG_FILE=%TEST_RESULTS_DIR%\build_system_tests.log
echo LibEtude 빌드 시스템 테스트 로그 > "%LOG_FILE%"
echo 시작 시간: %DATE% %TIME% >> "%LOG_FILE%"
echo. >> "%LOG_FILE%"

REM 함수: 테스트 결과 기록
:record_test
set /a TOTAL_TESTS+=1
if "%2"=="PASS" (
    set /a PASSED_TESTS+=1
    echo [PASS] %1 >> "%LOG_FILE%"
    echo ✅ %1
) else (
    set /a FAILED_TESTS+=1
    echo [FAIL] %1 >> "%LOG_FILE%"
    echo ❌ %1
    if not "%3"=="" echo   오류: %3 >> "%LOG_FILE%"
)
goto :eof

echo.
echo === 1. 환경 검증 ===

REM Visual Studio 환경 확인
call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    if errorlevel 1 (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        if errorlevel 1 (
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
            if errorlevel 1 (
                call :record_test "Visual Studio 환경" "FAIL" "Visual Studio를 찾을 수 없습니다"
                goto :end_tests
            )
        )
    )
)
call :record_test "Visual Studio 환경" "PASS"

REM CMake 확인
where cmake.exe >nul 2>&1
if errorlevel 1 (
    call :record_test "CMake 가용성" "FAIL" "CMake를 찾을 수 없습니다"
    goto :end_tests
) else (
    call :record_test "CMake 가용성" "PASS"
)

echo.
echo === 2. 프로젝트 빌드 ===

REM 테스트용 빌드 디렉토리 생성
set BUILD_DIR=%TEST_RESULTS_DIR%\build_test
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

cd /d "%BUILD_DIR%"

REM CMake 구성
echo CMake 구성 중...
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DLIBETUDE_BUILD_TESTS=ON ^
    -DLIBETUDE_ENABLE_SIMD=ON ^
    "%PROJECT_ROOT%" >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    call :record_test "CMake 구성" "FAIL" "CMake 구성 실패"
    goto :end_tests
)
call :record_test "CMake 구성" "PASS"

REM 프로젝트 빌드
echo 프로젝트 빌드 중...
cmake --build . --config Release --target windows_build_system_integration_test >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test "빌드 시스템 테스트 빌드" "FAIL" "빌드 실패"
) else (
    call :record_test "빌드 시스템 테스트 빌드" "PASS"
)

cmake --build . --config Release --target windows_deployment_validation_test >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test "배포 검증 테스트 빌드" "FAIL" "빌드 실패"
) else (
    call :record_test "배포 검증 테스트 빌드" "PASS"
)

echo.
echo === 3. 단위 테스트 실행 ===

REM Windows 빌드 시스템 통합 테스트 실행
if exist "tests\unit\windows\Release\windows_build_system_integration_test.exe" (
    echo Windows 빌드 시스템 통합 테스트 실행 중...
    tests\unit\windows\Release\windows_build_system_integration_test.exe >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test "빌드 시스템 통합 테스트 실행" "FAIL" "테스트 실행 실패"
    ) else (
        call :record_test "빌드 시스템 통합 테스트 실행" "PASS"
    )
) else (
    call :record_test "빌드 시스템 통합 테스트 실행" "FAIL" "실행 파일 없음"
)

REM Windows 배포 검증 테스트 실행
if exist "tests\unit\windows\Release\windows_deployment_validation_test.exe" (
    echo Windows 배포 검증 테스트 실행 중...
    tests\unit\windows\Release\windows_deployment_validation_test.exe >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test "배포 검증 테스트 실행" "FAIL" "테스트 실행 실패"
    ) else (
        call :record_test "배포 검증 테스트 실행" "PASS"
    )
) else (
    call :record_test "배포 검증 테스트 실행" "FAIL" "실행 파일 없음"
)

echo.
echo === 4. 통합 스크립트 테스트 ===

REM CMake 통합 테스트 스크립트 실행
if exist "%SCRIPT_DIR%test_cmake_integration.bat" (
    echo CMake 통합 테스트 스크립트 실행 중...
    call "%SCRIPT_DIR%test_cmake_integration.bat" >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test "CMake 통합 스크립트" "FAIL" "스크립트 실행 실패"
    ) else (
        call :record_test "CMake 통합 스크립트" "PASS"
    )
) else (
    call :record_test "CMake 통합 스크립트" "FAIL" "스크립트 파일 없음"
)

REM NuGet 의존성 검증 스크립트 실행
if exist "%SCRIPT_DIR%validate_nuget_dependencies.bat" (
    echo NuGet 의존성 검증 스크립트 실행 중...
    call "%SCRIPT_DIR%validate_nuget_dependencies.bat" >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test "NuGet 의존성 검증 스크립트" "FAIL" "스크립트 실행 실패"
    ) else (
        call :record_test "NuGet 의존성 검증 스크립트" "PASS"
    )
) else (
    call :record_test "NuGet 의존성 검증 스크립트" "FAIL" "스크립트 파일 없음"
)

echo.
echo === 5. 자동화 테스트 ===

REM 빌드 자동화 테스트 스크립트 실행
if exist "%SCRIPT_DIR%test_windows_build_automation.bat" (
    echo Windows 빌드 자동화 테스트 실행 중...
    call "%SCRIPT_DIR%test_windows_build_automation.bat" >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test "빌드 자동화 테스트" "FAIL" "자동화 테스트 실패"
    ) else (
        call :record_test "빌드 자동화 테스트" "PASS"
    )
) else (
    call :record_test "빌드 자동화 테스트" "FAIL" "자동화 스크립트 없음"
)

echo.
echo === 6. CTest 통합 테스트 ===

REM CTest를 사용한 빌드 시스템 테스트 실행
echo CTest 빌드 시스템 테스트 실행 중...
ctest -C Release -L "build" --output-on-failure >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test "CTest 빌드 시스템 테스트" "FAIL" "CTest 실행 실패"
) else (
    call :record_test "CTest 빌드 시스템 테스트" "PASS"
)

REM CTest를 사용한 배포 테스트 실행
echo CTest 배포 테스트 실행 중...
ctest -C Release -L "deployment" --output-on-failure >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test "CTest 배포 테스트" "FAIL" "CTest 실행 실패"
) else (
    call :record_test "CTest 배포 테스트" "PASS"
)

:end_tests

echo.
echo === 7. 결과 요약 ===

cd /d "%PROJECT_ROOT%"

echo.
echo ===================================
echo 빌드 시스템 테스트 결과 요약
echo ===================================
echo 총 테스트: %TOTAL_TESTS%
echo 성공: %PASSED_TESTS%
echo 실패: %FAILED_TESTS%

echo. >> "%LOG_FILE%"
echo 테스트 완료 시간: %DATE% %TIME% >> "%LOG_FILE%"
echo 총 테스트: %TOTAL_TESTS% >> "%LOG_FILE%"
echo 성공: %PASSED_TESTS% >> "%LOG_FILE%"
echo 실패: %FAILED_TESTS% >> "%LOG_FILE%"

REM 결과 파일 생성
set RESULT_FILE=%TEST_RESULTS_DIR%\build_system_test_results.txt
echo LibEtude 빌드 시스템 테스트 결과 > "%RESULT_FILE%"
echo ================================== >> "%RESULT_FILE%"
echo 실행 시간: %DATE% %TIME% >> "%RESULT_FILE%"
echo 총 테스트: %TOTAL_TESTS% >> "%RESULT_FILE%"
echo 성공: %PASSED_TESTS% >> "%RESULT_FILE%"
echo 실패: %FAILED_TESTS% >> "%RESULT_FILE%"
echo. >> "%RESULT_FILE%"
echo 상세 로그: %LOG_FILE% >> "%RESULT_FILE%"

echo.
echo 결과 파일: %RESULT_FILE%
echo 상세 로그: %LOG_FILE%

if %FAILED_TESTS% GTR 0 (
    echo.
    echo ❌ 일부 테스트가 실패했습니다.
    echo    요구사항 1.1, 1.2, 5.2, 5.3 구현을 확인해주세요.
    exit /b 1
) else (
    echo.
    echo ✅ 모든 빌드 시스템 테스트가 성공했습니다!
    echo    작업 9.3이 성공적으로 완료되었습니다.
    exit /b 0
)

endlocal