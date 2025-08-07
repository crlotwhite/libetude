#!/bin/bash
# LibEtude 패키징 스크립트 (Linux/macOS)
# Copyright (c) 2025 LibEtude Project

set -e

# 스크립트 디렉토리 확인
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 기본 설정
BUILD_TYPE="${BUILD_TYPE:-Release}"
PACKAGE_TYPE="${PACKAGE_TYPE:-TGZ}"
OUTPUT_DIR="${OUTPUT_DIR:-$PROJECT_ROOT/packages}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-package}"

# 색상 출력 함수
print_info() {
    echo -e "\033[1;34m[INFO]\033[0m $1"
}

print_success() {
    echo -e "\033[1;32m[SUCCESS]\033[0m $1"
}

print_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1"
}

print_warning() {
    echo -e "\033[1;33m[WARNING]\033[0m $1"
}

# 사용법 출력
show_usage() {
    cat << EOF
LibEtude 패키징 스크립트

사용법: $0 [옵션]

옵션:
    -t, --type TYPE         패키지 타입 (TGZ, DEB, RPM, DMG)
    -b, --build-type TYPE   빌드 타입 (Debug, Release, RelWithDebInfo)
    -o, --output DIR        출력 디렉토리
    -c, --clean             빌드 디렉토리 정리
    -p, --platform-only     플랫폼 추상화 레이어만 패키징
    -m, --minimal           최소 빌드로 패키징
    -h, --help              이 도움말 표시

환경 변수:
    BUILD_TYPE              빌드 타입 (기본값: Release)
    PACKAGE_TYPE            패키지 타입 (기본값: TGZ)
    OUTPUT_DIR              출력 디렉토리 (기본값: ./packages)
    BUILD_DIR               빌드 디렉토리 (기본값: ./build-package)

예시:
    $0 -t DEB -b Release                    # Debian 패키지 생성
    $0 -t RPM -o /tmp/packages              # RPM 패키지를 /tmp/packages에 생성
    $0 -t TGZ -p                            # 플랫폼 추상화 레이어만 TGZ로 패키징
    $0 -t DMG -m                            # 최소 빌드로 macOS DMG 생성

EOF
}

# 플랫폼 감지
detect_platform() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PLATFORM="Linux"
        DEFAULT_PACKAGE_TYPE="DEB"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        PLATFORM="macOS"
        DEFAULT_PACKAGE_TYPE="DMG"
    else
        PLATFORM="Unknown"
        DEFAULT_PACKAGE_TYPE="TGZ"
    fi

    print_info "플랫폼: $PLATFORM"
}

# 의존성 확인
check_dependencies() {
    print_info "의존성 확인 중..."

    # CMake 확인
    if ! command -v cmake &> /dev/null; then
        print_error "CMake가 설치되지 않았습니다"
        exit 1
    fi

    # 패키지 타입별 의존성 확인
    case "$PACKAGE_TYPE" in
        "DEB")
            if ! command -v dpkg-deb &> /dev/null; then
                print_error "dpkg-deb가 설치되지 않았습니다"
                exit 1
            fi
            ;;
        "RPM")
            if ! command -v rpmbuild &> /dev/null; then
                print_error "rpmbuild가 설치되지 않았습니다"
                exit 1
            fi
            ;;
        "DMG")
            if [[ "$PLATFORM" != "macOS" ]]; then
                print_error "DMG 패키지는 macOS에서만 생성할 수 있습니다"
                exit 1
            fi
            if ! command -v hdiutil &> /dev/null; then
                print_error "hdiutil이 설치되지 않았습니다"
                exit 1
            fi
            ;;
    esac

    print_success "의존성 확인 완료"
}

# 빌드 디렉토리 준비
prepare_build_dir() {
    print_info "빌드 디렉토리 준비 중..."

    if [[ "$CLEAN_BUILD" == "true" ]] && [[ -d "$BUILD_DIR" ]]; then
        print_info "기존 빌드 디렉토리 정리: $BUILD_DIR"
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    mkdir -p "$OUTPUT_DIR"

    print_success "빌드 디렉토리 준비 완료: $BUILD_DIR"
}

# CMake 설정
configure_cmake() {
    print_info "CMake 설정 중..."

    cd "$BUILD_DIR"

    # CMake 옵션 설정
    CMAKE_OPTS=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DCMAKE_INSTALL_PREFIX=/usr/local"
        "-DLIBETUDE_BUILD_EXAMPLES=ON"
        "-DLIBETUDE_BUILD_TOOLS=ON"
        "-DLIBETUDE_BUILD_BINDINGS=ON"
        "-DLIBETUDE_ENABLE_PLATFORM_ABSTRACTION=ON"
    )

    # 플랫폼 추상화 레이어 전용 빌드
    if [[ "$PLATFORM_ONLY" == "true" ]]; then
        CMAKE_OPTS+=(
            "-DLIBETUDE_BUILD_EXAMPLES=OFF"
            "-DLIBETUDE_BUILD_TOOLS=OFF"
            "-DLIBETUDE_BUILD_BINDINGS=OFF"
        )
        print_info "플랫폼 추상화 레이어 전용 빌드 설정"
    fi

    # 최소 빌드
    if [[ "$MINIMAL_BUILD" == "true" ]]; then
        CMAKE_OPTS+=(
            "-DLIBETUDE_MINIMAL=ON"
            "-DLIBETUDE_ENABLE_GPU=OFF"
            "-DLIBETUDE_BUILD_EXAMPLES=OFF"
            "-DLIBETUDE_BUILD_TOOLS=OFF"
        )
        print_info "최소 빌드 설정"
    fi

    # 플랫폼별 설정
    case "$PLATFORM" in
        "Linux")
            CMAKE_OPTS+=(
                "-DCPACK_GENERATOR=$PACKAGE_TYPE"
                "-DLIBETUDE_ENABLE_SIMD=ON"
            )
            ;;
        "macOS")
            CMAKE_OPTS+=(
                "-DCPACK_GENERATOR=$PACKAGE_TYPE"
                "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0"
                "-DLIBETUDE_ENABLE_SIMD=ON"
            )
            # Universal Binary 빌드 (선택적)
            if [[ "$UNIVERSAL_BINARY" == "true" ]]; then
                CMAKE_OPTS+=("-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64")
                print_info "Universal Binary 빌드 설정"
            fi
            ;;
    esac

    print_info "CMake 명령: cmake ${CMAKE_OPTS[*]} $PROJECT_ROOT"
    cmake "${CMAKE_OPTS[@]}" "$PROJECT_ROOT"

    print_success "CMake 설정 완료"
}

# 빌드 실행
build_project() {
    print_info "프로젝트 빌드 중..."

    cd "$BUILD_DIR"

    # 병렬 빌드 설정
    PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    cmake --build . --config "$BUILD_TYPE" --parallel "$PARALLEL_JOBS"

    print_success "빌드 완료"
}

# 테스트 실행 (선택적)
run_tests() {
    if [[ "$SKIP_TESTS" != "true" ]]; then
        print_info "테스트 실행 중..."

        cd "$BUILD_DIR"
        ctest --output-on-failure --parallel "$PARALLEL_JOBS"

        print_success "테스트 완료"
    else
        print_warning "테스트 건너뜀"
    fi
}

# 패키지 생성
create_package() {
    print_info "$PACKAGE_TYPE 패키지 생성 중..."

    cd "$BUILD_DIR"

    case "$PACKAGE_TYPE" in
        "TGZ")
            cpack -G TGZ
            ;;
        "DEB")
            cpack -G DEB
            ;;
        "RPM")
            cpack -G RPM
            ;;
        "DMG")
            cpack -G DragNDrop
            ;;
        *)
            print_error "지원하지 않는 패키지 타입: $PACKAGE_TYPE"
            exit 1
            ;;
    esac

    # 생성된 패키지 파일을 출력 디렉토리로 이동
    find . -name "*.tar.gz" -o -name "*.deb" -o -name "*.rpm" -o -name "*.dmg" | while read -r package_file; do
        if [[ -f "$package_file" ]]; then
            mv "$package_file" "$OUTPUT_DIR/"
            print_success "패키지 생성 완료: $OUTPUT_DIR/$(basename "$package_file")"
        fi
    done
}

# 패키지 검증
verify_package() {
    print_info "패키지 검증 중..."

    case "$PACKAGE_TYPE" in
        "DEB")
            for deb_file in "$OUTPUT_DIR"/*.deb; do
                if [[ -f "$deb_file" ]]; then
                    print_info "DEB 패키지 검증: $(basename "$deb_file")"
                    dpkg-deb --info "$deb_file"
                    dpkg-deb --contents "$deb_file" | head -20
                fi
            done
            ;;
        "RPM")
            for rpm_file in "$OUTPUT_DIR"/*.rpm; do
                if [[ -f "$rpm_file" ]]; then
                    print_info "RPM 패키지 검증: $(basename "$rpm_file")"
                    rpm -qip "$rpm_file"
                    rpm -qlp "$rpm_file" | head -20
                fi
            done
            ;;
        "TGZ"|"DMG")
            for package_file in "$OUTPUT_DIR"/*.tar.gz "$OUTPUT_DIR"/*.dmg; do
                if [[ -f "$package_file" ]]; then
                    print_info "패키지 크기: $(basename "$package_file") - $(du -h "$package_file" | cut -f1)"
                fi
            done
            ;;
    esac

    print_success "패키지 검증 완료"
}

# 정리 작업
cleanup() {
    if [[ "$KEEP_BUILD_DIR" != "true" ]]; then
        print_info "빌드 디렉토리 정리 중..."
        rm -rf "$BUILD_DIR"
        print_success "정리 완료"
    else
        print_info "빌드 디렉토리 유지: $BUILD_DIR"
    fi
}

# 메인 함수
main() {
    print_info "LibEtude 패키징 시작"
    print_info "빌드 타입: $BUILD_TYPE"
    print_info "패키지 타입: $PACKAGE_TYPE"
    print_info "출력 디렉토리: $OUTPUT_DIR"

    detect_platform
    check_dependencies
    prepare_build_dir
    configure_cmake
    build_project
    run_tests
    create_package
    verify_package
    cleanup

    print_success "패키징 완료!"
    print_info "생성된 패키지는 $OUTPUT_DIR 디렉토리에 있습니다"
}

# 명령행 인수 처리
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            PACKAGE_TYPE="$2"
            shift 2
            ;;
        -b|--build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD="true"
            shift
            ;;
        -p|--platform-only)
            PLATFORM_ONLY="true"
            shift
            ;;
        -m|--minimal)
            MINIMAL_BUILD="true"
            shift
            ;;
        --skip-tests)
            SKIP_TESTS="true"
            shift
            ;;
        --keep-build-dir)
            KEEP_BUILD_DIR="true"
            shift
            ;;
        --universal-binary)
            UNIVERSAL_BINARY="true"
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "알 수 없는 옵션: $1"
            show_usage
            exit 1
            ;;
    esac
done

# 기본값 설정
detect_platform
if [[ -z "$PACKAGE_TYPE" ]]; then
    PACKAGE_TYPE="$DEFAULT_PACKAGE_TYPE"
fi

# 메인 실행
main