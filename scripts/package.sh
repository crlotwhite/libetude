#!/bin/bash
# LibEtude 패키징 스크립트
# Copyright (c) 2025 LibEtude Project

set -e

# 기본 설정
BUILD_DIR="build"
PACKAGE_DIR="packages"
VERSION="1.0.0"
PACKAGE_TYPE="auto"
INCLUDE_DEBUG="OFF"
INCLUDE_EXAMPLES="ON"
INCLUDE_DOCS="ON"

# 도움말 함수
show_help() {
    cat << EOF
LibEtude 패키징 스크립트

사용법: $0 [옵션]

옵션:
    -h, --help              이 도움말 표시
    -d, --build-dir DIR     빌드 디렉토리 (기본값: build)
    -o, --output-dir DIR    패키지 출력 디렉토리 (기본값: packages)
    -v, --version VERSION   패키지 버전 (기본값: 1.0.0)
    -t, --type TYPE         패키지 타입 (deb, rpm, dmg, zip, tar.gz, auto)
    --include-debug         디버그 심볼 포함
    --no-examples           예제 제외
    --no-docs               문서 제외
    --runtime-only          런타임 패키지만 생성
    --dev-only              개발 패키지만 생성

패키지 타입:
    auto                    플랫폼에 따라 자동 선택
    deb                     Debian/Ubuntu 패키지
    rpm                     RedHat/CentOS 패키지
    dmg                     macOS 디스크 이미지
    zip                     ZIP 아카이브
    tar.gz                  TAR.GZ 아카이브

예제:
    $0                      기본 패키징
    $0 --type deb           Debian 패키지 생성
    $0 --runtime-only       런타임 패키지만 생성
    $0 --include-debug      디버그 심볼 포함
EOF
}

# 명령행 인수 파싱
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -d|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -o|--output-dir)
            PACKAGE_DIR="$2"
            shift 2
            ;;
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        -t|--type)
            PACKAGE_TYPE="$2"
            shift 2
            ;;
        --include-debug)
            INCLUDE_DEBUG="ON"
            shift
            ;;
        --no-examples)
            INCLUDE_EXAMPLES="OFF"
            shift
            ;;
        --no-docs)
            INCLUDE_DOCS="OFF"
            shift
            ;;
        --runtime-only)
            RUNTIME_ONLY=1
            shift
            ;;
        --dev-only)
            DEV_ONLY=1
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
case "$(uname -s)" in
    Linux*)
        PLATFORM="linux"
        if [[ "$PACKAGE_TYPE" == "auto" ]]; then
            if command -v dpkg >/dev/null 2>&1; then
                PACKAGE_TYPE="deb"
            elif command -v rpm >/dev/null 2>&1; then
                PACKAGE_TYPE="rpm"
            else
                PACKAGE_TYPE="tar.gz"
            fi
        fi
        ;;
    Darwin*)
        PLATFORM="macos"
        if [[ "$PACKAGE_TYPE" == "auto" ]]; then
            PACKAGE_TYPE="dmg"
        fi
        ;;
    CYGWIN*|MINGW*|MSYS*)
        PLATFORM="windows"
        if [[ "$PACKAGE_TYPE" == "auto" ]]; then
            PACKAGE_TYPE="zip"
        fi
        ;;
    *)
        PLATFORM="unknown"
        if [[ "$PACKAGE_TYPE" == "auto" ]]; then
            PACKAGE_TYPE="tar.gz"
        fi
        ;;
esac

# 빌드 디렉토리 확인
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "오류: 빌드 디렉토리 '$BUILD_DIR'를 찾을 수 없습니다."
    echo "먼저 빌드를 실행하세요: ./scripts/build.sh"
    exit 1
fi

echo "=== LibEtude 패키징 시작 ==="
echo "플랫폼: $PLATFORM"
echo "패키지 타입: $PACKAGE_TYPE"
echo "버전: $VERSION"
echo "빌드 디렉토리: $BUILD_DIR"
echo "출력 디렉토리: $PACKAGE_DIR"

# 패키지 디렉토리 생성
mkdir -p "$PACKAGE_DIR"

# 임시 디렉토리 생성
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# 아키텍처 감지
ARCH=$(uname -m)
case "$ARCH" in
    x86_64) ARCH="x64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    armv7l) ARCH="armv7" ;;
esac

# 패키지 이름 생성
PACKAGE_NAME="libetude-${VERSION}-${PLATFORM}-${ARCH}"

# 패키지 내용 준비
prepare_package_content() {
    local dest_dir="$1"
    local package_type="$2"  # runtime, dev, full

    mkdir -p "$dest_dir"/{bin,lib,include,share/libetude}

    # 라이브러리 복사
    echo "라이브러리 복사 중..."
    if [[ -f "$BUILD_DIR/src/libetude.so" ]]; then
        cp "$BUILD_DIR/src/libetude.so"* "$dest_dir/lib/" 2>/dev/null || true
    elif [[ -f "$BUILD_DIR/src/libetude.dylib" ]]; then
        cp "$BUILD_DIR/src/libetude.dylib" "$dest_dir/lib/"
    elif [[ -f "$BUILD_DIR/src/Release/libetude.dll" ]]; then
        cp "$BUILD_DIR/src/Release/libetude.dll" "$dest_dir/bin/"
        if [[ "$package_type" != "runtime" ]]; then
            cp "$BUILD_DIR/src/Release/libetude.lib" "$dest_dir/lib/" 2>/dev/null || true
        fi
    fi

    # 정적 라이브러리 (개발 패키지)
    if [[ "$package_type" != "runtime" ]]; then
        if [[ -f "$BUILD_DIR/src/libetude.a" ]]; then
            cp "$BUILD_DIR/src/libetude.a" "$dest_dir/lib/"
        elif [[ -f "$BUILD_DIR/src/Release/libetude.lib" ]]; then
            cp "$BUILD_DIR/src/Release/libetude.lib" "$dest_dir/lib/" 2>/dev/null || true
        fi
    fi

    # 헤더 파일 (개발 패키지)
    if [[ "$package_type" != "runtime" ]]; then
        echo "헤더 파일 복사 중..."
        cp -r include/libetude "$dest_dir/include/"
    fi

    # 도구
    echo "도구 복사 중..."
    if [[ -d "$BUILD_DIR/tools" ]]; then
        find "$BUILD_DIR/tools" -type f -executable -exec cp {} "$dest_dir/bin/" \; 2>/dev/null || true
    fi

    # 예제 (요청된 경우)
    if [[ "$INCLUDE_EXAMPLES" == "ON" && "$package_type" != "runtime" ]]; then
        echo "예제 복사 중..."
        mkdir -p "$dest_dir/share/libetude/examples"
        if [[ -d "$BUILD_DIR/examples" ]]; then
            find "$BUILD_DIR/examples" -type f -executable -exec cp {} "$dest_dir/share/libetude/examples/" \; 2>/dev/null || true
        fi
        cp -r examples/* "$dest_dir/share/libetude/examples/" 2>/dev/null || true
    fi

    # 문서 (요청된 경우)
    if [[ "$INCLUDE_DOCS" == "ON" ]]; then
        echo "문서 복사 중..."
        mkdir -p "$dest_dir/share/libetude/docs"
        if [[ -d "docs" ]]; then
            cp -r docs/* "$dest_dir/share/libetude/docs/"
        fi
        cp README.md "$dest_dir/share/libetude/" 2>/dev/null || true
        cp LICENSE "$dest_dir/share/libetude/" 2>/dev/null || true
    fi

    # pkg-config 파일 (개발 패키지)
    if [[ "$package_type" != "runtime" ]]; then
        mkdir -p "$dest_dir/lib/pkgconfig"
        cat > "$dest_dir/lib/pkgconfig/libetude.pc" << EOF
prefix=/usr/local
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: LibEtude
Description: AI inference engine specialized for voice synthesis
Version: $VERSION
Libs: -L\${libdir} -letude
Cflags: -I\${includedir}
EOF
    fi
}

# 패키지 생성 함수
create_package() {
    local package_type="$1"
    local suffix="$2"

    local temp_package_dir="$TEMP_DIR/${PACKAGE_NAME}${suffix}"
    prepare_package_content "$temp_package_dir" "$package_type"

    case "$PACKAGE_TYPE" in
        deb)
            create_deb_package "$temp_package_dir" "$package_type" "$suffix"
            ;;
        rpm)
            create_rpm_package "$temp_package_dir" "$package_type" "$suffix"
            ;;
        dmg)
            create_dmg_package "$temp_package_dir" "$package_type" "$suffix"
            ;;
        zip)
            create_zip_package "$temp_package_dir" "$package_type" "$suffix"
            ;;
        tar.gz)
            create_tarball_package "$temp_package_dir" "$package_type" "$suffix"
            ;;
        *)
            echo "지원하지 않는 패키지 타입: $PACKAGE_TYPE"
            exit 1
            ;;
    esac
}

# Debian 패키지 생성
create_deb_package() {
    local package_dir="$1"
    local package_type="$2"
    local suffix="$3"

    local deb_dir="$TEMP_DIR/deb${suffix}"
    mkdir -p "$deb_dir/DEBIAN"

    # 패키지 내용 복사
    mkdir -p "$deb_dir/usr/local"
    cp -r "$package_dir"/* "$deb_dir/usr/local/"

    # control 파일 생성
    local package_name="libetude"
    if [[ "$package_type" == "dev" ]]; then
        package_name="libetude-dev"
    fi

    cat > "$deb_dir/DEBIAN/control" << EOF
Package: $package_name
Version: $VERSION
Section: libs
Priority: optional
Architecture: $ARCH
Maintainer: LibEtude Project <info@libetude.org>
Description: AI inference engine specialized for voice synthesis
 LibEtude is a specialized AI inference engine for voice synthesis.
 Unlike general-purpose AI frameworks, it's highly optimized for
 the voice synthesis domain.
EOF

    # 패키지 빌드
    dpkg-deb --build "$deb_dir" "$PACKAGE_DIR/${PACKAGE_NAME}${suffix}.deb"
    echo "Debian 패키지 생성됨: ${PACKAGE_NAME}${suffix}.deb"
}

# RPM 패키지 생성
create_rpm_package() {
    local package_dir="$1"
    local package_type="$2"
    local suffix="$3"

    # RPM 빌드 디렉토리 구조 생성
    local rpm_dir="$TEMP_DIR/rpm${suffix}"
    mkdir -p "$rpm_dir"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

    # 소스 타르볼 생성
    tar -czf "$rpm_dir/SOURCES/${PACKAGE_NAME}${suffix}.tar.gz" -C "$package_dir" .

    # spec 파일 생성
    local package_name="libetude"
    if [[ "$package_type" == "dev" ]]; then
        package_name="libetude-devel"
    fi

    cat > "$rpm_dir/SPECS/libetude${suffix}.spec" << EOF
Name: $package_name
Version: $VERSION
Release: 1
Summary: AI inference engine specialized for voice synthesis
License: MIT
Source0: ${PACKAGE_NAME}${suffix}.tar.gz

%description
LibEtude is a specialized AI inference engine for voice synthesis.
Unlike general-purpose AI frameworks, it's highly optimized for
the voice synthesis domain.

%prep
%setup -q -n .

%install
mkdir -p %{buildroot}/usr/local
cp -r * %{buildroot}/usr/local/

%files
/usr/local/*

%changelog
* $(date '+%a %b %d %Y') LibEtude Project <info@libetude.org> - $VERSION-1
- Initial package
EOF

    # RPM 빌드
    rpmbuild --define "_topdir $rpm_dir" -bb "$rpm_dir/SPECS/libetude${suffix}.spec"
    cp "$rpm_dir/RPMS"/*/*.rpm "$PACKAGE_DIR/"
    echo "RPM 패키지 생성됨: $(basename "$rpm_dir/RPMS"/*/*.rpm)"
}

# macOS DMG 패키지 생성
create_dmg_package() {
    local package_dir="$1"
    local package_type="$2"
    local suffix="$3"

    local dmg_name="${PACKAGE_NAME}${suffix}.dmg"

    # 임시 DMG 디렉토리 생성
    local dmg_dir="$TEMP_DIR/dmg${suffix}"
    mkdir -p "$dmg_dir"

    # 패키지 내용 복사
    cp -r "$package_dir" "$dmg_dir/LibEtude"

    # DMG 생성
    hdiutil create -volname "LibEtude $VERSION" -srcfolder "$dmg_dir" -ov -format UDZO "$PACKAGE_DIR/$dmg_name"
    echo "DMG 패키지 생성됨: $dmg_name"
}

# ZIP 패키지 생성
create_zip_package() {
    local package_dir="$1"
    local package_type="$2"
    local suffix="$3"

    local zip_name="${PACKAGE_NAME}${suffix}.zip"

    cd "$package_dir"
    zip -r "$PACKAGE_DIR/$zip_name" .
    cd - >/dev/null

    echo "ZIP 패키지 생성됨: $zip_name"
}

# TAR.GZ 패키지 생성
create_tarball_package() {
    local package_dir="$1"
    local package_type="$2"
    local suffix="$3"

    local tar_name="${PACKAGE_NAME}${suffix}.tar.gz"

    tar -czf "$PACKAGE_DIR/$tar_name" -C "$package_dir" .
    echo "TAR.GZ 패키지 생성됨: $tar_name"
}

# 패키지 생성 실행
if [[ -n "$RUNTIME_ONLY" ]]; then
    create_package "runtime" "-runtime"
elif [[ -n "$DEV_ONLY" ]]; then
    create_package "dev" "-dev"
else
    # 런타임과 개발 패키지 모두 생성
    create_package "runtime" "-runtime"
    create_package "dev" "-dev"
    create_package "full" ""
fi

echo "=== 패키징 완료 ==="
echo "패키지가 $PACKAGE_DIR 디렉토리에 생성되었습니다."
ls -la "$PACKAGE_DIR"