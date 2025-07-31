#!/bin/bash
# LibEtude 설치 스크립트
# Copyright (c) 2025 LibEtude Project

set -e

# 기본 설정
INSTALL_PREFIX="/usr/local"
BUILD_DIR="build"
SUDO_REQUIRED=0
CREATE_SYMLINKS=1
INSTALL_DEV=1
INSTALL_DOCS=1

# 도움말 함수
show_help() {
    cat << EOF
LibEtude 설치 스크립트

사용법: $0 [옵션]

옵션:
    -h, --help              이 도움말 표시
    -p, --prefix PREFIX     설치 경로 (기본값: /usr/local)
    -d, --build-dir DIR     빌드 디렉토리 (기본값: build)
    --no-sudo               sudo 사용 안함
    --no-symlinks           심볼릭 링크 생성 안함
    --no-dev                개발 파일 설치 안함
    --no-docs               문서 설치 안함
    --user                  사용자 디렉토리에 설치 (~/.local)

예제:
    $0                      기본 설치
    $0 --prefix /opt/libetude  사용자 지정 경로에 설치
    $0 --user               사용자 디렉토리에 설치
    $0 --no-dev             런타임만 설치
EOF
}

# 명령행 인수 파싱
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        -d|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --no-sudo)
            SUDO_REQUIRED=0
            shift
            ;;
        --no-symlinks)
            CREATE_SYMLINKS=0
            shift
            ;;
        --no-dev)
            INSTALL_DEV=0
            shift
            ;;
        --no-docs)
            INSTALL_DOCS=0
            shift
            ;;
        --user)
            INSTALL_PREFIX="$HOME/.local"
            SUDO_REQUIRED=0
            shift
            ;;
        *)
            echo "알 수 없는 옵션: $1"
            show_help
            exit 1
            ;;
    esac
done

# 권한 확인
if [[ "$INSTALL_PREFIX" == "/usr"* || "$INSTALL_PREFIX" == "/opt"* ]]; then
    if [[ $EUID -ne 0 && $SUDO_REQUIRED -eq 1 ]]; then
        echo "시스템 디렉토리에 설치하려면 sudo 권한이 필요합니다."
        echo "sudo $0 $@ 를 실행하거나 --no-sudo 옵션을 사용하세요."
        exit 1
    fi
fi

# 빌드 디렉토리 확인
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "오류: 빌드 디렉토리 '$BUILD_DIR'를 찾을 수 없습니다."
    echo "먼저 빌드를 실행하세요: ./scripts/build.sh"
    exit 1
fi

echo "=== LibEtude 설치 시작 ==="
echo "설치 경로: $INSTALL_PREFIX"
echo "빌드 디렉토리: $BUILD_DIR"

# 설치 디렉토리 생성
mkdir -p "$INSTALL_PREFIX"/{bin,lib,include,share/libetude}

# 라이브러리 설치
echo "라이브러리 설치 중..."
if [[ -f "$BUILD_DIR/src/libetude.so" ]]; then
    cp "$BUILD_DIR/src/libetude.so"* "$INSTALL_PREFIX/lib/"
elif [[ -f "$BUILD_DIR/src/libetude.dylib" ]]; then
    cp "$BUILD_DIR/src/libetude.dylib" "$INSTALL_PREFIX/lib/"
elif [[ -f "$BUILD_DIR/src/Release/libetude.dll" ]]; then
    cp "$BUILD_DIR/src/Release/libetude.dll" "$INSTALL_PREFIX/bin/"
    cp "$BUILD_DIR/src/Release/libetude.lib" "$INSTALL_PREFIX/lib/"
fi

# 정적 라이브러리 설치
if [[ -f "$BUILD_DIR/src/libetude.a" ]]; then
    cp "$BUILD_DIR/src/libetude.a" "$INSTALL_PREFIX/lib/"
elif [[ -f "$BUILD_DIR/src/Release/libetude.lib" ]]; then
    cp "$BUILD_DIR/src/Release/libetude.lib" "$INSTALL_PREFIX/lib/"
fi

# 헤더 파일 설치 (개발 파일)
if [[ $INSTALL_DEV -eq 1 ]]; then
    echo "헤더 파일 설치 중..."
    cp -r include/libetude "$INSTALL_PREFIX/include/"
fi

# 도구 설치
echo "도구 설치 중..."
if [[ -d "$BUILD_DIR/tools" ]]; then
    find "$BUILD_DIR/tools" -type f -executable -exec cp {} "$INSTALL_PREFIX/bin/" \;
fi

# 예제 설치
echo "예제 설치 중..."
if [[ -d "$BUILD_DIR/examples" ]]; then
    mkdir -p "$INSTALL_PREFIX/share/libetude/examples"
    find "$BUILD_DIR/examples" -type f -executable -exec cp {} "$INSTALL_PREFIX/share/libetude/examples/" \;
    cp -r examples/* "$INSTALL_PREFIX/share/libetude/examples/" 2>/dev/null || true
fi

# 문서 설치
if [[ $INSTALL_DOCS -eq 1 ]]; then
    echo "문서 설치 중..."
    mkdir -p "$INSTALL_PREFIX/share/libetude/docs"
    if [[ -d "docs" ]]; then
        cp -r docs/* "$INSTALL_PREFIX/share/libetude/docs/"
    fi
    cp README.md "$INSTALL_PREFIX/share/libetude/" 2>/dev/null || true
    cp LICENSE "$INSTALL_PREFIX/share/libetude/" 2>/dev/null || true
fi

# pkg-config 파일 생성
echo "pkg-config 파일 생성 중..."
mkdir -p "$INSTALL_PREFIX/lib/pkgconfig"
cat > "$INSTALL_PREFIX/lib/pkgconfig/libetude.pc" << EOF
prefix=$INSTALL_PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: LibEtude
Description: AI inference engine specialized for voice synthesis
Version: 1.0.0
Libs: -L\${libdir} -letude
Cflags: -I\${includedir}
EOF

# 심볼릭 링크 생성 (Linux/macOS)
if [[ $CREATE_SYMLINKS -eq 1 && "$INSTALL_PREFIX" != "/usr/local" ]]; then
    echo "심볼릭 링크 생성 중..."

    # 라이브러리 링크
    if [[ -f "$INSTALL_PREFIX/lib/libetude.so" ]]; then
        ln -sf "$INSTALL_PREFIX/lib/libetude.so" /usr/local/lib/libetude.so 2>/dev/null || true
    elif [[ -f "$INSTALL_PREFIX/lib/libetude.dylib" ]]; then
        ln -sf "$INSTALL_PREFIX/lib/libetude.dylib" /usr/local/lib/libetude.dylib 2>/dev/null || true
    fi

    # 헤더 링크
    if [[ $INSTALL_DEV -eq 1 ]]; then
        ln -sf "$INSTALL_PREFIX/include/libetude" /usr/local/include/libetude 2>/dev/null || true
    fi

    # pkg-config 링크
    ln -sf "$INSTALL_PREFIX/lib/pkgconfig/libetude.pc" /usr/local/lib/pkgconfig/libetude.pc 2>/dev/null || true
fi

# 라이브러리 캐시 업데이트 (Linux)
if command -v ldconfig >/dev/null 2>&1; then
    echo "라이브러리 캐시 업데이트 중..."
    if [[ $EUID -eq 0 ]]; then
        ldconfig
    else
        echo "라이브러리 캐시 업데이트를 위해 sudo 권한이 필요합니다:"
        echo "sudo ldconfig"
    fi
fi

# 환경 변수 설정 안내
echo ""
echo "=== 설치 완료 ==="
echo "LibEtude가 $INSTALL_PREFIX 에 설치되었습니다."
echo ""

if [[ "$INSTALL_PREFIX" != "/usr/local" && "$INSTALL_PREFIX" != "/usr" ]]; then
    echo "환경 변수 설정이 필요할 수 있습니다:"
    echo "export PATH=\"$INSTALL_PREFIX/bin:\$PATH\""
    echo "export LD_LIBRARY_PATH=\"$INSTALL_PREFIX/lib:\$LD_LIBRARY_PATH\""
    echo "export PKG_CONFIG_PATH=\"$INSTALL_PREFIX/lib/pkgconfig:\$PKG_CONFIG_PATH\""
    echo ""
    echo "이 설정을 ~/.bashrc 또는 ~/.zshrc에 추가하세요."
fi

echo "설치된 파일:"
echo "- 라이브러리: $INSTALL_PREFIX/lib/"
if [[ $INSTALL_DEV -eq 1 ]]; then
    echo "- 헤더 파일: $INSTALL_PREFIX/include/libetude/"
fi
echo "- 도구: $INSTALL_PREFIX/bin/"
echo "- 예제: $INSTALL_PREFIX/share/libetude/examples/"
if [[ $INSTALL_DOCS -eq 1 ]]; then
    echo "- 문서: $INSTALL_PREFIX/share/libetude/docs/"
fi
echo "- pkg-config: $INSTALL_PREFIX/lib/pkgconfig/libetude.pc"