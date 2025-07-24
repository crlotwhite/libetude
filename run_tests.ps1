# LibEtude PowerShell 테스트 실행 스크립트

Write-Host "=== LibEtude PowerShell 테스트 실행 ===" -ForegroundColor Cyan
Write-Host ""

# 빌드 디렉토리 확인
if (-not (Test-Path "build")) {
    Write-Host "빌드 디렉토리가 없습니다. 먼저 프로젝트를 빌드해주세요." -ForegroundColor Red
    Write-Host "cmake -B build -DCMAKE_BUILD_TYPE=Debug" -ForegroundColor Yellow
    Write-Host "cmake --build build --config Debug" -ForegroundColor Yellow
    Read-Host "계속하려면 Enter를 누르세요"
    exit 1
}

# 테스트 실행
Write-Host "독립적인 테스트 실행 중..." -ForegroundColor Green
$result = & cmake --build build --target run_standalone_tests --config Debug

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "✅ 모든 테스트가 성공했습니다!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "❌ 일부 테스트가 실패했습니다." -ForegroundColor Red
    Write-Host "자세한 정보는 위의 출력을 확인해주세요." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== 추가 테스트 명령어 ===" -ForegroundColor Cyan
Write-Host "cmake --build build --target test_info --config Debug  : 테스트 정보 확인" -ForegroundColor White
Write-Host "cd build; ctest -C Debug -L standalone                 : CTest로 독립적인 테스트 실행" -ForegroundColor White
Write-Host "cd build; ctest -C Debug --verbose                     : 상세한 테스트 출력" -ForegroundColor White
Write-Host ""

Read-Host "계속하려면 Enter를 누르세요"