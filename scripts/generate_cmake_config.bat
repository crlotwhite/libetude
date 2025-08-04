@echo off
REM LibEtude CMake 설정 파일 생성 스크립트
REM Copyright (c) 2025 LibEtude Project

setlocal enabledelayedexpansion

echo LibEtude CMake 설정 파일 생성 시작...

REM 명령줄 인수 처리
set VERSION=%1
set INSTALL_PREFIX=%2

if "%VERSION%"=="" set VERSION=1.0.0
if "%INSTALL_PREFIX%"=="" set INSTALL_PREFIX=C:\Program Files\LibEtude

echo 설정:
echo   버전: %VERSION%
echo   설치 경로: %INSTALL_PREFIX%

REM 스크립트 디렉토리 설정
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set CMAKE_DIR=%PROJECT_ROOT%\cmake
set OUTPUT_DIR=%PROJECT_ROOT%\build\cmake_config

REM 출력 디렉토리 생성
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo 출력 디렉토리: %OUTPUT_DIR%

REM 버전 정보 파싱
for /f "tokens=1,2,3 delims=." %%a in ("%VERSION%") do (
    set VERSION_MAJOR=%%a
    set VERSION_MINOR=%%b
    set VERSION_PATCH=%%c
)

echo 버전 정보:
echo   메이저: %VERSION_MAJOR%
echo   마이너: %VERSION_MINOR%
echo   패치: %VERSION_PATCH%

REM LibEtudeConfig.cmake 파일 생성
echo LibEtudeConfig.cmake 파일 생성 중...

REM 템플릿 파일에서 변수 치환
powershell -Command "
$content = Get-Content '%CMAKE_DIR%\LibEtudeConfig.cmake.in' -Raw;
$content = $content -replace '@PACKAGE_INIT@', '';
$content = $content -replace '@PROJECT_VERSION@', '%VERSION%';
$content = $content -replace '@PROJECT_VERSION_MAJOR@', '%VERSION_MAJOR%';
$content = $content -replace '@PROJECT_VERSION_MINOR@', '%VERSION_MINOR%';
$content = $content -replace '@PROJECT_VERSION_PATCH@', '%VERSION_PATCH%';
$content = $content -replace '@CMAKE_INSTALL_PREFIX@', '%INSTALL_PREFIX%';
$content = $content -replace '@LIBETUDE_ENABLE_SIMD@', 'ON';
$content = $content -replace '@LIBETUDE_ENABLE_GPU@', 'OFF';
$content = $content -replace '@LIBETUDE_MINIMAL@', 'OFF';
Set-Content -Path '%OUTPUT_DIR%\LibEtudeConfig.cmake' -Value $content -Encoding UTF8;
"

if errorlevel 1 (
    echo LibEtudeConfig.cmake 생성 실패
    exit /b 1
)

echo LibEtudeConfig.cmake 생성 완료

REM LibEtudeConfigVersion.cmake 파일 생성
echo LibEtudeConfigVersion.cmake 파일 생성 중...

powershell -Command "
$content = Get-Content '%CMAKE_DIR%\LibEtudeConfigVersion.cmake.in' -Raw;
$content = $content -replace '@PROJECT_VERSION@', '%VERSION%';
$content = $content -replace '@PROJECT_VERSION_MAJOR@', '%VERSION_MAJOR%';
$content = $content -replace '@PACKAGE_FIND_VERSION_MAJOR@', '${PACKAGE_FIND_VERSION_MAJOR}';
Set-Content -Path '%OUTPUT_DIR%\LibEtudeConfigVersion.cmake' -Value $content -Encoding UTF8;
"

if errorlevel 1 (
    echo LibEtudeConfigVersion.cmake 생성 실패
    exit /b 1
)

echo LibEtudeConfigVersion.cmake 생성 완료

REM WindowsConfig.cmake 파일 복사
echo WindowsConfig.cmake 파일 복사 중...
copy "%CMAKE_DIR%\WindowsConfig.cmake" "%OUTPUT_DIR%\" >nul
if errorlevel 1 (
    echo WindowsConfig.cmake 복사 실패
    exit /b 1
)

echo WindowsConfig.cmake 복사 완료

REM 사용 예제 CMakeLists.txt 생성
echo 사용 예제 생성 중...
echo # LibEtude 사용 예제 > "%OUTPUT_DIR%\example_CMakeLists.txt"
echo cmake_minimum_required(VERSION 3.16) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo project(MyLibEtudeProject VERSION 1.0.0 LANGUAGES C CXX) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo. >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo # LibEtude 패키지 찾기 >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo find_package(LibEtude %VERSION% REQUIRED) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo. >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo # 실행 파일 생성 >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo add_executable(my_app main.c) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo. >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo # LibEtude 라이브러리 링크 >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo target_link_libraries(my_app PRIVATE LibEtude::LibEtude) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo. >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo # Windows 특화 설정 (선택적) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo if(WIN32) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo     # Windows 특화 최적화 적용 >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo     libetude_configure_windows_target(my_app) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo     >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo     # 또는 수동으로 Windows 실행 파일 생성 >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo     # libetude_add_windows_executable(my_app main.c) >> "%OUTPUT_DIR%\example_CMakeLists.txt"
echo endif() >> "%OUTPUT_DIR%\example_CMakeLists.txt"

REM 설치 스크립트 생성
echo 설치 스크립트 생성 중...
echo @echo off > "%OUTPUT_DIR%\install.bat"
echo REM LibEtude CMake 설정 파일 설치 스크립트 >> "%OUTPUT_DIR%\install.bat"
echo. >> "%OUTPUT_DIR%\install.bat"
echo set INSTALL_DIR=%INSTALL_PREFIX%\lib\cmake\LibEtude >> "%OUTPUT_DIR%\install.bat"
echo. >> "%OUTPUT_DIR%\install.bat"
echo echo LibEtude CMake 설정 파일 설치 중... >> "%OUTPUT_DIR%\install.bat"
echo. >> "%OUTPUT_DIR%\install.bat"
echo if not exist "%%INSTALL_DIR%%" mkdir "%%INSTALL_DIR%%" >> "%OUTPUT_DIR%\install.bat"
echo. >> "%OUTPUT_DIR%\install.bat"
echo copy "LibEtudeConfig.cmake" "%%INSTALL_DIR%%\" ^>nul >> "%OUTPUT_DIR%\install.bat"
echo copy "LibEtudeConfigVersion.cmake" "%%INSTALL_DIR%%\" ^>nul >> "%OUTPUT_DIR%\install.bat"
echo copy "WindowsConfig.cmake" "%%INSTALL_DIR%%\" ^>nul >> "%OUTPUT_DIR%\install.bat"
echo. >> "%OUTPUT_DIR%\install.bat"
echo echo 설치 완료: %%INSTALL_DIR%% >> "%OUTPUT_DIR%\install.bat"
echo pause >> "%OUTPUT_DIR%\install.bat"

REM README 파일 생성
echo README 파일 생성 중...
echo # LibEtude CMake 설정 파일 > "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo 이 디렉토리에는 LibEtude %VERSION%용 CMake 설정 파일들이 포함되어 있습니다. >> "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo ## 파일 목록 >> "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo - `LibEtudeConfig.cmake`: 메인 설정 파일 >> "%OUTPUT_DIR%\README.md"
echo - `LibEtudeConfigVersion.cmake`: 버전 호환성 확인 파일 >> "%OUTPUT_DIR%\README.md"
echo - `WindowsConfig.cmake`: Windows 특화 설정 파일 >> "%OUTPUT_DIR%\README.md"
echo - `example_CMakeLists.txt`: 사용 예제 >> "%OUTPUT_DIR%\README.md"
echo - `install.bat`: 설치 스크립트 >> "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo ## 설치 방법 >> "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo 1. `install.bat` 스크립트 실행 >> "%OUTPUT_DIR%\README.md"
echo 2. 또는 수동으로 파일들을 `%INSTALL_PREFIX%\lib\cmake\LibEtude\`에 복사 >> "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo ## 사용 방법 >> "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo ```cmake >> "%OUTPUT_DIR%\README.md"
echo find_package(LibEtude %VERSION% REQUIRED) >> "%OUTPUT_DIR%\README.md"
echo target_link_libraries(your_target PRIVATE LibEtude::LibEtude) >> "%OUTPUT_DIR%\README.md"
echo ``` >> "%OUTPUT_DIR%\README.md"
echo. >> "%OUTPUT_DIR%\README.md"
echo 자세한 사용법은 `example_CMakeLists.txt`를 참조하세요. >> "%OUTPUT_DIR%\README.md"

REM 검증 스크립트 생성
echo 검증 스크립트 생성 중...
echo @echo off > "%OUTPUT_DIR%\validate.bat"
echo REM LibEtude CMake 설정 파일 검증 스크립트 >> "%OUTPUT_DIR%\validate.bat"
echo. >> "%OUTPUT_DIR%\validate.bat"
echo echo LibEtude CMake 설정 파일 검증 중... >> "%OUTPUT_DIR%\validate.bat"
echo. >> "%OUTPUT_DIR%\validate.bat"
echo set ERROR_COUNT=0 >> "%OUTPUT_DIR%\validate.bat"
echo. >> "%OUTPUT_DIR%\validate.bat"
echo if not exist "LibEtudeConfig.cmake" ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo [오류] LibEtudeConfig.cmake 파일이 없습니다. >> "%OUTPUT_DIR%\validate.bat"
echo     set /a ERROR_COUNT+=1 >> "%OUTPUT_DIR%\validate.bat"
echo ) else ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo [확인] LibEtudeConfig.cmake 파일 존재 >> "%OUTPUT_DIR%\validate.bat"
echo ) >> "%OUTPUT_DIR%\validate.bat"
echo. >> "%OUTPUT_DIR%\validate.bat"
echo if not exist "LibEtudeConfigVersion.cmake" ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo [오류] LibEtudeConfigVersion.cmake 파일이 없습니다. >> "%OUTPUT_DIR%\validate.bat"
echo     set /a ERROR_COUNT+=1 >> "%OUTPUT_DIR%\validate.bat"
echo ) else ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo [확인] LibEtudeConfigVersion.cmake 파일 존재 >> "%OUTPUT_DIR%\validate.bat"
echo ) >> "%OUTPUT_DIR%\validate.bat"
echo. >> "%OUTPUT_DIR%\validate.bat"
echo if not exist "WindowsConfig.cmake" ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo [오류] WindowsConfig.cmake 파일이 없습니다. >> "%OUTPUT_DIR%\validate.bat"
echo     set /a ERROR_COUNT+=1 >> "%OUTPUT_DIR%\validate.bat"
echo ) else ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo [확인] WindowsConfig.cmake 파일 존재 >> "%OUTPUT_DIR%\validate.bat"
echo ) >> "%OUTPUT_DIR%\validate.bat"
echo. >> "%OUTPUT_DIR%\validate.bat"
echo if %%ERROR_COUNT%% == 0 ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo 모든 파일이 올바르게 생성되었습니다. >> "%OUTPUT_DIR%\validate.bat"
echo ) else ( >> "%OUTPUT_DIR%\validate.bat"
echo     echo %%ERROR_COUNT%%개의 오류가 발견되었습니다. >> "%OUTPUT_DIR%\validate.bat"
echo     exit /b 1 >> "%OUTPUT_DIR%\validate.bat"
echo ) >> "%OUTPUT_DIR%\validate.bat"
echo. >> "%OUTPUT_DIR%\validate.bat"
echo pause >> "%OUTPUT_DIR%\validate.bat"

echo.
echo ========================================
echo CMake 설정 파일 생성 완료!
echo ========================================
echo 출력 위치: %OUTPUT_DIR%
echo.
echo 생성된 파일들:
dir "%OUTPUT_DIR%" /b

echo.
echo 다음 단계:
echo 1. validate.bat을 실행하여 파일들을 검증하세요
echo 2. install.bat을 실행하여 시스템에 설치하세요
echo 3. example_CMakeLists.txt를 참조하여 프로젝트에서 사용하세요

endlocal
pause