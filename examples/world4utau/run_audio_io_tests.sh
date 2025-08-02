#!/bin/bash

# world4utau 오디오 I/O 테스트 실행 스크립트

echo "=== world4utau 오디오 I/O 테스트 실행 ==="

# 현재 디렉토리 확인
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "프로젝트 루트: $PROJECT_ROOT"
echo "테스트 디렉토리: $SCRIPT_DIR"

# 빌드 디렉토리 확인
BUILD_DIR="$PROJECT_ROOT/build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: 빌드 디렉토리가 없습니다: $BUILD_DIR"
    echo "먼저 프로젝트를 빌드해주세요:"
    echo "  cd $PROJECT_ROOT"
    echo "  cmake -B build -DCMAKE_BUILD_TYPE=Debug"
    echo "  cmake --build build"
    exit 1
fi

# 테스트 실행 파일 컴파일
echo "=== 테스트 컴파일 ==="

TEST_SOURCE="$PROJECT_ROOT/tests/unit/test_world4utau_audio_io.c"
TEST_EXECUTABLE="$BUILD_DIR/test_world4utau_audio_io"

# 컴파일 명령어 구성
COMPILE_CMD="gcc -std=c11 -g -O0"
INCLUDE_DIRS="-I$PROJECT_ROOT/include -I$SCRIPT_DIR/include"
SOURCE_FILES="$TEST_SOURCE $SCRIPT_DIR/src/audio_file_io.c $SCRIPT_DIR/src/utau_interface.c"
LINK_LIBS="-L$BUILD_DIR/src -llibetude_static -lm"

# 플랫폼별 링크 라이브러리 추가
case "$(uname -s)" in
    Darwin)
        LINK_LIBS="$LINK_LIBS -framework AudioToolbox -framework CoreAudio"
        ;;
    Linux)
        LINK_LIBS="$LINK_LIBS -lasound -lpthread"
        ;;
    MINGW*|CYGWIN*|MSYS*)
        LINK_LIBS="$LINK_LIBS -lwinmm -ldsound -lpsapi"
        ;;
esac

echo "컴파일 명령어:"
echo "$COMPILE_CMD $INCLUDE_DIRS $SOURCE_FILES $LINK_LIBS -o $TEST_EXECUTABLE"

# 컴파일 실행
$COMPILE_CMD $INCLUDE_DIRS $SOURCE_FILES $LINK_LIBS -o "$TEST_EXECUTABLE"

if [ $? -ne 0 ]; then
    echo "Error: 테스트 컴파일 실패"
    exit 1
fi

echo "✓ 테스트 컴파일 성공"

# 테스트 실행
echo ""
echo "=== 테스트 실행 ==="

if [ -x "$TEST_EXECUTABLE" ]; then
    "$TEST_EXECUTABLE"
    TEST_RESULT=$?

    echo ""
    if [ $TEST_RESULT -eq 0 ]; then
        echo "✓ 모든 테스트가 성공했습니다!"
    else
        echo "✗ 일부 테스트가 실패했습니다. (종료 코드: $TEST_RESULT)"
    fi

    # 테스트 실행 파일 정리
    rm -f "$TEST_EXECUTABLE"

    exit $TEST_RESULT
else
    echo "Error: 테스트 실행 파일을 찾을 수 없습니다: $TEST_EXECUTABLE"
    exit 1
fi