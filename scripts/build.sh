#!/bin/bash
# LibEtude 크로스 플랫폼 빌드 스크립트
# Copyright (c) 2025 LibEtude Project

set -e

# 기본 설정
BUILD_TYPE="Release"
BUILD_DIR="build"
INSTALL_PREFIX="/usr/local"
ENABLE_TESTS="ON"
ENABLE_EXAMPLES="ON"
ENABLE_TOOLS="ON"
ENABLE_BINDINGS="ON"
ENABLE_SIMD="ON"
ENABLE_GPU="ON"
MINIMAL_BUILD="OFF"
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 도움말 함수
show_help() {
    cat << EOF
LibEtude 빌드 스크립트

사용법: $0 [옵션]

옵션:
    -h, --help              이 도움말 표시
    -t, --type TYPE         빌드 타입 (Debug, Release, RelWithDebInfo, MinSizeRel)
    -d, --dir DIR           빌드 디렉토리 (기본값: build)
    -p, --prefix PREFIX     설치 경로 (기본값: /usr/local)
    -j, --jobs N            병렬 빌드 작업 수 (기본값: CPU 코어 수)
    --no-tests              테스트 빌드 비활성화
    --no-examples           예제 빌드 비활성화
    --no-tools              도구 빌드 비활성화
    --no-bindings           바인딩 빌드 비활성화
    --no-simd               SIMD 최적화 비활성화
    --no-gpu                GPU 가속 비활성화
    --minimal               최소 빌드 (임베디드용)
    --clean                 빌드 디렉토리 정리 후 빌드
    --install               빌드 후 설치 실행
    --package               빌드 후 패키지 생성

플랫폼별 빌드:
    --android               Android 빌드
    --ios                   iOS 빌드
    --embedded              임베디드 빌드

예제:
    $0                      기본 빌드
    $0 --type Debug         디버그 빌드
    $0 --minimal --no-gpu   최소 빌드 (GPU 비활성화)
    $0 --android            Android 빌드
    $0 --clean --install    정리 후 빌드 및 설치
EOF
}

# 명령행 인수 파싱
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -d|--dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --no-tests)
            ENABLE_TESTS="OFF"
            shift
            ;;
        --no-examples)
            ENABLE_EXAMPLES="OFF"
            shift
            ;;
        --no-tools)
            ENABLE_TOOLS="OFF"
            shift
            ;;
        --no-bindings)
            ENABLE_BINDINGS="OFF"
            shift
            ;;
        --no-simd)
            ENABLE_SIMD="OFF"
            shift
            ;;
        --no-gpu)
            ENABLE_GPU="OFF"
            shift
            ;;
        --minimal)
            MINIMAL_BUILD="ON"
            ENABLE_TESTS="OFF"
            ENABLE_EXAMPLES="OFF"
            ENABLE_TOOLS="OFF"
            ENABLE_BINDINGS="OFF"
            ENABLE_GPU="OFF"
            shift
            ;;
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --install)
            DO_INSTALL=1
            shift
            ;;
        --package)
            DO_PACKAGE=1
            shift
            ;;
        --android)
            PLATFORM="android"
            shift
            ;;
        --ios)
            PLATFORM="ios"
            shift
            ;;
        --embedded)
            PLATFORM="embedded"
            MINIMAL_BUILD="ON"
            shift
            ;;
        *)
            echo "알 수 없는 옵션: $1"
            show_help
            exit 1
            ;;
    esac
done

# 플랫폼 감지
if [[ -z "$PLATFORM" ]]; then
    case "$(uname -s)" in
        Linux*)     PLATFORM="linux";;
        Darwin*)    PLATFORM="macos";;
        CYGWIN*|MINGW*|MSYS*) PLATFORM="windows";;
        *)          PLATFORM="unknown";;
    esac
fi

echo "=== LibEtude 빌드 시작 ==="
echo "플랫폼: $PLATFORM"
echo "빌드 타입: $BUILD_TYPE"
echo "빌드 디렉토리: $BUILD_DIR"
echo "설치 경로: $INSTALL_PREFIX"
echo "병렬 작업 수: $PARALLEL_JOBS"

# 빌드 디렉토리 정리
if [[ -n "$CLEAN_BUILD" && -d "$BUILD_DIR" ]]; then
    echo "빌드 디렉토리 정리 중..."
    rm -rf "$BUILD_DIR"
fi

# 빌드 디렉토리 생성
mkdir -p "$BUILD_DIR"

# CMake 설정
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    -DLIBETUDE_BUILD_TESTS="$ENABLE_TESTS"
    -DLIBETUDE_BUILD_EXAMPLES="$ENABLE_EXAMPLES"
    -DLIBETUDE_BUILD_TOOLS="$ENABLE_TOOLS"
    -DLIBETUDE_BUILD_BINDINGS="$ENABLE_BINDINGS"
    -DLIBETUDE_ENABLE_SIMD="$ENABLE_SIMD"
    -DLIBETUDE_ENABLE_GPU="$ENABLE_GPU"
    -DLIBETUDE_MINIMAL="$MINIMAL_BUILD"
)

# 플랫폼별 설정
case "$PLATFORM" in
    android)
        if [[ -z "$ANDROID_NDK" ]]; then
            echo "오류: ANDROID_NDK 환경 변수가 설정되지 않았습니다."
            exit 1
        fi
        CMAKE_ARGS+=(
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake"
            -DANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"
            -DANDROID_PLATFORM="${ANDROID_PLATFORM:-android-21}"
            -DANDROID_STL=c++_shared
        )
        BUILD_DIR="${BUILD_DIR}-android"
        ;;
    ios)
        CMAKE_ARGS+=(
            -DCMAKE_TOOLCHAIN_FILE="cmake/ios.toolchain.cmake"
            -DPLATFORM=OS64
            -DENABLE_BITCODE=OFF
        )
        BUILD_DIR="${BUILD_DIR}-ios"
        ;;
    embedded)
        if [[ -n "$EMBEDDED_TOOLCHAIN" ]]; then
            CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$EMBEDDED_TOOLCHAIN")
        fi
        BUILD_DIR="${BUILD_DIR}-embedded"
        ;;
esac

# CMake 구성
echo "CMake 구성 중..."
cmake -B "$BUILD_DIR" "${CMAKE_ARGS[@]}" .

# 빌드 실행
echo "빌드 실행 중..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel "$PARALLEL_JOBS"

# 테스트 실행 (활성화된 경우)
if [[ "$ENABLE_TESTS" == "ON" && "$PLATFORM" != "android" && "$PLATFORM" != "ios" ]]; then
    echo "테스트 실행 중..."
    cd "$BUILD_DIR"
    ctest --output-on-failure --parallel "$PARALLEL_JOBS"
    cd ..
fi

# 설치 실행 (요청된 경우)
if [[ -n "$DO_INSTALL" ]]; then
    echo "설치 실행 중..."
    cmake --install "$BUILD_DIR" --config "$BUILD_TYPE"
fi

# 패키지 생성 (요청된 경우)
if [[ -n "$DO_PACKAGE" ]]; then
    echo "패키지 생성 중..."
    cd "$BUILD_DIR"
    cpack
    cd ..
fi

echo "=== 빌드 완료 ==="