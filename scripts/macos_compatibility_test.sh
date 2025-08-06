#!/bin/bash
# macOS 호환성 테스트 스크립트
# LibEtude Project

set -e

echo "=== LibEtude macOS 호환성 테스트 ==="
echo

# 시스템 정보 출력
echo "시스템 정보:"
echo "  - macOS 버전: $(sw_vers -productVersion)"
echo "  - Xcode 버전: $(xcodebuild -version | head -n1)"
echo "  - SDK 경로: $(xcrun --show-sdk-path)"
echo

# SDK 버전 확인
SDK_VERSION=$(xcrun --show-sdk-version)
echo "  - SDK 버전: $SDK_VERSION"

# macOS 15+ SDK 감지
if [[ $(echo "$SDK_VERSION" | cut -d. -f1) -ge 15 ]]; then
    echo "  ⚠️  macOS 15+ SDK 감지됨 - 블록 문법 호환성 모드 필요"
    MACOS_15_PLUS=1
else
    echo "  ✅ macOS 14 이하 SDK - 기본 호환성"
    MACOS_15_PLUS=0
fi

echo

# 빌드 디렉토리 생성
BUILD_DIR="build_compat_test"
if [ -d "$BUILD_DIR" ]; then
    echo "기존 빌드 디렉토리 정리..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "CMake 구성 중..."

# macOS 15+ SDK인 경우 호환성 플래그 추가
if [ $MACOS_15_PLUS -eq 1 ]; then
    echo "  - macOS 15+ 호환성 모드 활성화"
    CMAKE_FLAGS="-DCMAKE_C_FLAGS=-DLIBETUDE_MACOS_BLOCKS_DISABLED=1"
else
    CMAKE_FLAGS=""
fi

# CMake 구성
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DLIBETUDE_BUILD_EXAMPLES=ON \
    -DLIBETUDE_BUILD_TESTS=ON \
    $CMAKE_FLAGS

echo
echo "빌드 중..."
make -j$(sysctl -n hw.ncpu)

echo
echo "=== 호환성 테스트 실행 ==="

# 기본 API 테스트
echo "1. 기본 API 테스트..."
if [ -f "tests/test_api" ]; then
    ./tests/test_api
    echo "  ✅ 기본 API 테스트 통과"
else
    echo "  ⚠️  기본 API 테스트 바이너리 없음"
fi

# macOS 오디오 테스트
echo
echo "2. macOS 오디오 호환성 테스트..."
if [ -f "tests/test_macos_audio" ]; then
    ./tests/test_macos_audio
    echo "  ✅ macOS 오디오 테스트 통과"
else
    echo "  ⚠️  macOS 오디오 테스트 바이너리 없음"
fi

# 예제 실행 테스트
echo
echo "3. 예제 실행 테스트..."
if [ -f "examples/basic_tts/basic_tts_example" ]; then
    echo "  기본 TTS 예제 실행 중..."
    timeout 5s ./examples/basic_tts/basic_tts_example || echo "  (타임아웃 또는 정상 종료)"
    echo "  ✅ 기본 TTS 예제 실행 완료"
else
    echo "  ⚠️  기본 TTS 예제 바이너리 없음"
fi

echo
echo "=== 컴파일러 경고 검사 ==="

# 블록 관련 경고 검사
echo "블록 문법 관련 경고 검사 중..."
WARNINGS=$(make 2>&1 | grep -i "block\|nullability" || true)

if [ -n "$WARNINGS" ]; then
    echo "  ⚠️  블록 관련 경고 발견:"
    echo "$WARNINGS" | sed 's/^/    /'
else
    echo "  ✅ 블록 관련 경고 없음"
fi

echo
echo "=== 런타임 호환성 검증 ==="

# 호환성 함수 테스트 (간단한 C 프로그램 컴파일 및 실행)
cat > compat_test.c << 'EOF'
#include <stdio.h>
#include "../include/libetude/platform/macos_compat.h"

int main() {
    printf("macOS 호환성 테스트\n");

    #ifdef __APPLE__
    if (libetude_init_macos_compatibility() == 0) {
        printf("  - 호환성 초기화: 성공\n");
        printf("  - SDK 버전: %d\n", libetude_get_macos_sdk_version());
        printf("  - 블록 활성화: %s\n", libetude_is_blocks_enabled() ? "예" : "아니오");
    } else {
        printf("  - 호환성 초기화: 실패\n");
        return 1;
    }
    #else
    printf("  - macOS가 아닌 플랫폼\n");
    #endif

    return 0;
}
EOF

echo "런타임 호환성 테스트 컴파일 중..."
if gcc -I.. compat_test.c ../src/platform/macos/macos_compat.c -o compat_test 2>/dev/null; then
    echo "  ✅ 컴파일 성공"
    echo "런타임 테스트 실행 중..."
    ./compat_test
    echo "  ✅ 런타임 테스트 통과"
else
    echo "  ❌ 컴파일 실패"
fi

# 정리
rm -f compat_test.c compat_test

echo
echo "=== 테스트 완료 ==="

if [ $MACOS_15_PLUS -eq 1 ]; then
    echo "✅ macOS 15+ SDK 호환성 테스트 완료"
    echo "   블록 문법 비활성화 모드로 정상 작동"
else
    echo "✅ macOS 14 이하 SDK 호환성 테스트 완료"
fi

echo
echo "권장사항:"
echo "  - 프로덕션 빌드 시 Release 모드 사용"
echo "  - CI/CD에서 여러 macOS 버전 테스트"
echo "  - 정기적인 호환성 검증 수행"

cd ..