@echo off
REM LibEtude Windows 테스트 실행 스크립트

echo === LibEtude Windows 테스트 실행 ===
echo.

REM 빌드 디렉토리 확인
if not exist "build" (
    echo 빌드 디렉토리가 없습니다. 먼저 프로젝트를 빌드해주세요.
    echo cmake -B build -DCMAKE_BUILD_TYPE=Debug
    echo cmake --build build --config Debug
    pause
    exit /b 1
)

REM 테스트 실행
echo 독립적인 테스트 실행 중...
cmake --build build --target run_standalone_tests --config Debug

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✅ 모든 테스트가 성공했습니다!
) else (
    echo.
    echo ❌ 일부 테스트가 실패했습니다.
    echo 자세한 정보는 위의 출력을 확인해주세요.
)

echo.
echo === 추가 테스트 명령어 ===
echo cmake --build build --target test_info --config Debug  : 테스트 정보 확인
echo cd build ^&^& ctest -C Debug -L standalone             : CTest로 독립적인 테스트 실행
echo cd build ^&^& ctest -C Debug --verbose                 : 상세한 테스트 출력
echo.

pause