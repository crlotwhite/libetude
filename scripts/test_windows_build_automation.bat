@echo off
REM LibEtude Windows 빌드 자동화 테스트 스크립트
REM Copyright (c) 2025 LibEtude Project
REM
REM 이 스크립트는 Visual Studio 2019/2022 및 MinGW 빌드를 자동으로 테스트합니다.
REM 요구사항: 1.1, 1.2, 5.2, 5.3

setlocal enabledelayedexpansion

echo LibEtude Windows 빌드 자동화 테스트 시작...
echo ================================================

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set TEST_OUTPUT_DIR=%PROJECT_ROOT%\build_test_results
set LOG_FILE=%TEST_OUTPUT_DIR%\build_test_log.txt

REM 출력 디렉토리 생성
if not exist "%TEST_OUTPUT_DIR%" mkdir "%TEST_OUTPUT_DIR%"

REM 로그 파일 초기화
echo LibEtude Windows 빌드 자동화 테스트 로그 > "%LOG_FILE%"
echo 시작 시간: %DATE% %TIME% >> "%LOG_FILE%"
echo. >> "%LOG_FILE%"

REM 테스트 결과 변수
set TOTAL_TESTS=0
set PASSED_TESTS=0
set FAILED_TESTS=0

REM 함수: 테스트 결과 기록
:record_test_result
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

REM 1. 환경 검증
echo.
echo === 1. 개발 환경 검증 ===
echo 1. 개발 환경 검증 >> "%LOG_FILE%"

REM Visual Studio 환경 확인
echo Visual Studio 환경 확인 중...
call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    if errorlevel 1 (
        call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        if errorlevel 1 (
            call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
            if errorlevel 1 (
                call :record_test_result "Visual Studio 환경 설정" "FAIL" "Visual Studio를 찾을 수 없습니다"
                goto :skip_vs_tests
            )
        )
    )
)
call :record_test_result "Visual Studio 환경 설정" "PASS"

REM CMake 확인
where cmake.exe >nul 2>&1
if errorlevel 1 (
    call :record_test_result "CMake 가용성" "FAIL" "CMake를 찾을 수 없습니다"
) else (
    call :record_test_result "CMake 가용성" "PASS"
)

REM Git 확인 (선택사항)
where git.exe >nul 2>&1
if errorlevel 1 (
    echo ⚠️  Git을 찾을 수 없습니다 (선택사항)
) else (
    call :record_test_result "Git 가용성" "PASS"
)

REM 2. Visual Studio 2022 빌드 테스트
echo.
echo === 2. Visual Studio 2022 빌드 테스트 ===
echo 2. Visual Studio 2022 빌드 테스트 >> "%LOG_FILE%"

set VS2022_BUILD_DIR=%TEST_OUTPUT_DIR%\vs2022_build
if exist "%VS2022_BUILD_DIR%" rmdir /s /q "%VS2022_BUILD_DIR%"
mkdir "%VS2022_BUILD_DIR%"

echo Visual Studio 2022 x64 Release 빌드 중...
cd /d "%VS2022_BUILD_DIR%"

cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DLIBETUDE_ENABLE_SIMD=ON ^
    -DLIBETUDE_ENABLE_ETW=ON ^
    -DLIBETUDE_BUILD_TESTS=ON ^
    "%PROJECT_ROOT%" >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    call :record_test_result "VS2022 x64 CMake 구성" "FAIL" "CMake 구성 실패"
    goto :skip_vs2022_build
)
call :record_test_result "VS2022 x64 CMake 구성" "PASS"

cmake --build . --config Release --parallel >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test_result "VS2022 x64 Release 빌드" "FAIL" "빌드 실패"
) else (
    call :record_test_result "VS2022 x64 Release 빌드" "PASS"

    REM 빌드 결과 확인
    if exist "src\Release\libetude.lib" (
        call :record_test_result "VS2022 정적 라이브러리 생성" "PASS"
    ) else (
        call :record_test_result "VS2022 정적 라이브러리 생성" "FAIL" "libetude.lib 파일 없음"
    )

    if exist "src\Release\libetude.pdb" (
        call :record_test_result "VS2022 PDB 파일 생성" "PASS"
    ) else (
        call :record_test_result "VS2022 PDB 파일 생성" "FAIL" "PDB 파일 없음"
    )
)

REM Debug 빌드 테스트
echo Visual Studio 2022 x64 Debug 빌드 중...
cmake --build . --config Debug --parallel >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test_result "VS2022 x64 Debug 빌드" "FAIL" "Debug 빌드 실패"
) else (
    call :record_test_result "VS2022 x64 Debug 빌드" "PASS"
)

:skip_vs2022_build

REM 3. Visual Studio 2019 빌드 테스트 (가능한 경우)
echo.
echo === 3. Visual Studio 2019 빌드 테스트 ===
echo 3. Visual Studio 2019 빌드 테스트 >> "%LOG_FILE%"

set VS2019_BUILD_DIR=%TEST_OUTPUT_DIR%\vs2019_build
if exist "%VS2019_BUILD_DIR%" rmdir /s /q "%VS2019_BUILD_DIR%"
mkdir "%VS2019_BUILD_DIR%"

cd /d "%VS2019_BUILD_DIR%"

cmake -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DLIBETUDE_ENABLE_SIMD=ON ^
    -DLIBETUDE_BUILD_TESTS=ON ^
    "%PROJECT_ROOT%" >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    call :record_test_result "VS2019 x64 CMake 구성" "FAIL" "Visual Studio 2019 없음 또는 구성 실패"
    goto :skip_vs2019_build
)
call :record_test_result "VS2019 x64 CMake 구성" "PASS"

cmake --build . --config Release --parallel >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test_result "VS2019 x64 Release 빌드" "FAIL" "빌드 실패"
) else (
    call :record_test_result "VS2019 x64 Release 빌드" "PASS"
)

:skip_vs2019_build

REM 4. MinGW 빌드 테스트
echo.
echo === 4. MinGW 빌드 테스트 ===
echo 4. MinGW 빌드 테스트 >> "%LOG_FILE%"

where gcc.exe >nul 2>&1
if errorlevel 1 (
    call :record_test_result "MinGW 가용성" "FAIL" "MinGW GCC를 찾을 수 없습니다"
    goto :skip_mingw_tests
)
call :record_test_result "MinGW 가용성" "PASS"

set MINGW_BUILD_DIR=%TEST_OUTPUT_DIR%\mingw_build
if exist "%MINGW_BUILD_DIR%" rmdir /s /q "%MINGW_BUILD_DIR%"
mkdir "%MINGW_BUILD_DIR%"

cd /d "%MINGW_BUILD_DIR%"

cmake -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DLIBETUDE_ENABLE_SIMD=ON ^
    -DLIBETUDE_BUILD_TESTS=ON ^
    "%PROJECT_ROOT%" >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    call :record_test_result "MinGW CMake 구성" "FAIL" "CMake 구성 실패"
    goto :skip_mingw_build
)
call :record_test_result "MinGW CMake 구성" "PASS"

cmake --build . --parallel >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test_result "MinGW Release 빌드" "FAIL" "빌드 실패"
) else (
    call :record_test_result "MinGW Release 빌드" "PASS"

    REM 빌드 결과 확인
    if exist "src\liblibetude.a" (
        call :record_test_result "MinGW 정적 라이브러리 생성" "PASS"
    ) else (
        call :record_test_result "MinGW 정적 라이브러리 생성" "FAIL" "liblibetude.a 파일 없음"
    )
)

:skip_mingw_build
:skip_mingw_tests

REM 5. 멀티 플랫폼 빌드 테스트
echo.
echo === 5. 멀티 플랫폼 빌드 테스트 ===
echo 5. 멀티 플랫폼 빌드 테스트 >> "%LOG_FILE%"

REM Win32 플랫폼 테스트
set WIN32_BUILD_DIR=%TEST_OUTPUT_DIR%\win32_build
if exist "%WIN32_BUILD_DIR%" rmdir /s /q "%WIN32_BUILD_DIR%"
mkdir "%WIN32_BUILD_DIR%"

cd /d "%WIN32_BUILD_DIR%"

cmake -G "Visual Studio 17 2022" -A Win32 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DLIBETUDE_BUILD_TESTS=ON ^
    "%PROJECT_ROOT%" >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    call :record_test_result "Win32 플랫폼 CMake 구성" "FAIL" "CMake 구성 실패"
) else (
    call :record_test_result "Win32 플랫폼 CMake 구성" "PASS"

    cmake --build . --config Release >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test_result "Win32 플랫폼 빌드" "FAIL" "빌드 실패"
    ) else (
        call :record_test_result "Win32 플랫폼 빌드" "PASS"
    )
)

REM ARM64 플랫폼 테스트 (가능한 경우)
set ARM64_BUILD_DIR=%TEST_OUTPUT_DIR%\arm64_build
if exist "%ARM64_BUILD_DIR%" rmdir /s /q "%ARM64_BUILD_DIR%"
mkdir "%ARM64_BUILD_DIR%"

cd /d "%ARM64_BUILD_DIR%"

cmake -G "Visual Studio 17 2022" -A ARM64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DLIBETUDE_BUILD_TESTS=ON ^
    "%PROJECT_ROOT%" >> "%LOG_FILE%" 2>&1

if errorlevel 1 (
    call :record_test_result "ARM64 플랫폼 CMake 구성" "FAIL" "ARM64 도구 체인 없음 또는 구성 실패"
) else (
    call :record_test_result "ARM64 플랫폼 CMake 구성" "PASS"

    cmake --build . --config Release >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test_result "ARM64 플랫폼 빌드" "FAIL" "빌드 실패"
    ) else (
        call :record_test_result "ARM64 플랫폼 빌드" "PASS"
    )
)

REM 6. 테스트 실행
echo.
echo === 6. 단위 테스트 실행 ===
echo 6. 단위 테스트 실행 >> "%LOG_FILE%"

cd /d "%VS2022_BUILD_DIR%"

if exist "tests\unit\windows\Release\test_windows_build_system_integration.exe" (
    echo Windows 빌드 시스템 통합 테스트 실행 중...
    tests\unit\windows\Release\test_windows_build_system_integration.exe >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test_result "빌드 시스템 통합 테스트" "FAIL" "테스트 실행 실패"
    ) else (
        call :record_test_result "빌드 시스템 통합 테스트" "PASS"
    )
) else (
    call :record_test_result "빌드 시스템 통합 테스트" "FAIL" "테스트 실행 파일 없음"
)

if exist "tests\unit\windows\Release\test_windows_deployment_validation.exe" (
    echo Windows 배포 검증 테스트 실행 중...
    tests\unit\windows\Release\test_windows_deployment_validation.exe >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test_result "배포 검증 테스트" "FAIL" "테스트 실행 실패"
    ) else (
        call :record_test_result "배포 검증 테스트" "PASS"
    )
) else (
    call :record_test_result "배포 검증 테스트" "FAIL" "테스트 실행 파일 없음"
)

REM 7. NuGet 패키지 생성 테스트
echo.
echo === 7. NuGet 패키지 생성 테스트 ===
echo 7. NuGet 패키지 생성 테스트 >> "%LOG_FILE%"

REM NuGet 도구 확인
where nuget.exe >nul 2>&1
if errorlevel 1 (
    where dotnet.exe >nul 2>&1
    if errorlevel 1 (
        call :record_test_result "NuGet 도구 가용성" "FAIL" "nuget.exe와 dotnet.exe 모두 없음"
        goto :skip_nuget_tests
    ) else (
        call :record_test_result "NuGet 도구 가용성" "PASS" "dotnet CLI 사용"
        set NUGET_TOOL=dotnet
    )
) else (
    call :record_test_result "NuGet 도구 가용성" "PASS" "nuget.exe 사용"
    set NUGET_TOOL=nuget
)

REM NuGet 패키지 빌드 스크립트 테스트
echo NuGet 패키지 빌드 테스트 중...
call "%SCRIPT_DIR%build_nuget.bat" 1.0.0-test Release x64 >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test_result "NuGet 패키지 빌드" "FAIL" "빌드 스크립트 실행 실패"
) else (
    call :record_test_result "NuGet 패키지 빌드" "PASS"

    REM 생성된 패키지 확인
    if exist "%PROJECT_ROOT%\packages\LibEtude.1.0.0-test.nupkg" (
        call :record_test_result "NuGet 패키지 파일 생성" "PASS"
    ) else (
        call :record_test_result "NuGet 패키지 파일 생성" "FAIL" "nupkg 파일 없음"
    )
)

:skip_nuget_tests

REM 8. CMake 통합 테스트
echo.
echo === 8. CMake 통합 테스트 ===
echo 8. CMake 통합 테스트 >> "%LOG_FILE%"

call "%SCRIPT_DIR%test_cmake_integration.bat" >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    call :record_test_result "CMake 통합 테스트" "FAIL" "통합 테스트 실패"
) else (
    call :record_test_result "CMake 통합 테스트" "PASS"
)

:skip_vs_tests

REM 9. 성능 벤치마크 (선택사항)
echo.
echo === 9. 성능 벤치마크 ===
echo 9. 성능 벤치마크 >> "%LOG_FILE%"

if exist "%VS2022_BUILD_DIR%\tools\Release\libetude_benchmarks.exe" (
    echo 성능 벤치마크 실행 중...
    "%VS2022_BUILD_DIR%\tools\Release\libetude_benchmarks.exe" --quick >> "%LOG_FILE%" 2>&1
    if errorlevel 1 (
        call :record_test_result "성능 벤치마크" "FAIL" "벤치마크 실행 실패"
    ) else (
        call :record_test_result "성능 벤치마크" "PASS"
    )
) else (
    echo ⚠️  벤치마크 도구를 찾을 수 없습니다 (선택사항)
)

REM 10. 결과 요약 및 보고서 생성
echo.
echo === 10. 테스트 결과 요약 ===
echo 10. 테스트 결과 요약 >> "%LOG_FILE%"

cd /d "%PROJECT_ROOT%"

echo.
echo ================================================
echo LibEtude Windows 빌드 자동화 테스트 완료
echo ================================================
echo 총 테스트: %TOTAL_TESTS%
echo 성공: %PASSED_TESTS%
echo 실패: %FAILED_TESTS%

echo. >> "%LOG_FILE%"
echo 테스트 완료 시간: %DATE% %TIME% >> "%LOG_FILE%"
echo 총 테스트: %TOTAL_TESTS% >> "%LOG_FILE%"
echo 성공: %PASSED_TESTS% >> "%LOG_FILE%"
echo 실패: %FAILED_TESTS% >> "%LOG_FILE%"

REM HTML 보고서 생성
set HTML_REPORT=%TEST_OUTPUT_DIR%\build_test_report.html
echo ^<!DOCTYPE html^> > "%HTML_REPORT%"
echo ^<html^>^<head^>^<title^>LibEtude Windows 빌드 테스트 보고서^</title^>^</head^> >> "%HTML_REPORT%"
echo ^<body^> >> "%HTML_REPORT%"
echo ^<h1^>LibEtude Windows 빌드 테스트 보고서^</h1^> >> "%HTML_REPORT%"
echo ^<p^>테스트 실행 시간: %DATE% %TIME%^</p^> >> "%HTML_REPORT%"
echo ^<h2^>요약^</h2^> >> "%HTML_REPORT%"
echo ^<ul^> >> "%HTML_REPORT%"
echo ^<li^>총 테스트: %TOTAL_TESTS%^</li^> >> "%HTML_REPORT%"
echo ^<li^>성공: %PASSED_TESTS%^</li^> >> "%HTML_REPORT%"
echo ^<li^>실패: %FAILED_TESTS%^</li^> >> "%HTML_REPORT%"
echo ^</ul^> >> "%HTML_REPORT%"
echo ^<h2^>상세 로그^</h2^> >> "%HTML_REPORT%"
echo ^<pre^> >> "%HTML_REPORT%"
type "%LOG_FILE%" >> "%HTML_REPORT%"
echo ^</pre^> >> "%HTML_REPORT%"
echo ^</body^>^</html^> >> "%HTML_REPORT%"

echo.
echo 상세 로그: %LOG_FILE%
echo HTML 보고서: %HTML_REPORT%

if %FAILED_TESTS% GTR 0 (
    echo.
    echo ❌ 일부 테스트가 실패했습니다. 로그를 확인해주세요.
    exit /b 1
) else (
    echo.
    echo ✅ 모든 빌드 테스트가 성공했습니다!
    exit /b 0
)

endlocal