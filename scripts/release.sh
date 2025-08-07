#!/bin/bash
# LibEtude 자동화된 릴리스 파이프라인
# Copyright (c) 2025 LibEtude Project

set -e

# 스크립트 디렉토리 확인
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 기본 설정
RELEASE_VERSION=""
RELEASE_BRANCH="main"
RELEASE_TYPE="patch"  # patch, minor, major
DRY_RUN="false"
SKIP_TESTS="false"
SKIP_DOCS="false"
PLATFORMS="linux,macos,windows"
OUTPUT_DIR="$PROJECT_ROOT/releases"

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
LibEtude 자동화된 릴리스 파이프라인

사용법: $0 [옵션]

옵션:
    -v, --version VERSION   릴리스 버전 (예: 1.2.3)
    -t, --type TYPE         릴리스 타입 (patch, minor, major)
    -b, --branch BRANCH     릴리스 브랜치 (기본값: main)
    -p, --platforms LIST    빌드할 플랫폼 (쉼표로 구분, 기본값: linux,macos,windows)
    -o, --output DIR        출력 디렉토리 (기본값: ./releases)
    -d, --dry-run           실제 릴리스 없이 시뮬레이션만 실행
    --skip-tests            테스트 건너뛰기
    --skip-docs             문서 생성 건너뛰기
    -h, --help              이 도움말 표시

릴리스 타입:
    patch   - 버그 수정 (1.0.0 -> 1.0.1)
    minor   - 기능 추가 (1.0.0 -> 1.1.0)
    major   - 호환되지 않는 변경 (1.0.0 -> 2.0.0)

예시:
    $0 -v 1.2.3                         # 버전 1.2.3으로 릴리스
    $0 -t minor                         # 부 버전 증가하여 릴리스
    $0 -p linux,macos -d                # Linux와 macOS만 빌드 (시뮬레이션)
    $0 -v 2.0.0 --skip-tests            # 테스트 없이 메이저 릴리스

EOF
}

# Git 상태 확인
check_git_status() {
    print_info "Git 상태 확인 중..."

    # Git 저장소 확인
    if [[ ! -d "$PROJECT_ROOT/.git" ]]; then
        print_error "Git 저장소가 아닙니다"
        exit 1
    fi

    # 현재 브랜치 확인
    CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    if [[ "$CURRENT_BRANCH" != "$RELEASE_BRANCH" ]]; then
        print_error "릴리스 브랜치($RELEASE_BRANCH)가 아닙니다. 현재 브랜치: $CURRENT_BRANCH"
        exit 1
    fi

    # 변경사항 확인
    if [[ -n $(git status --porcelain) ]]; then
        print_error "커밋되지 않은 변경사항이 있습니다"
        git status --short
        exit 1
    fi

    # 원격 저장소와 동기화 확인
    git fetch origin
    LOCAL_COMMIT=$(git rev-parse HEAD)
    REMOTE_COMMIT=$(git rev-parse origin/$RELEASE_BRANCH)

    if [[ "$LOCAL_COMMIT" != "$REMOTE_COMMIT" ]]; then
        print_error "로컬 브랜치가 원격 브랜치와 동기화되지 않았습니다"
        exit 1
    fi

    print_success "Git 상태 확인 완료"
}

# 현재 버전 가져오기
get_current_version() {
    if [[ -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
        CURRENT_VERSION=$(grep -E "project.*VERSION" "$PROJECT_ROOT/CMakeLists.txt" | sed -n 's/.*VERSION \([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p')
        if [[ -z "$CURRENT_VERSION" ]]; then
            print_error "CMakeLists.txt에서 버전을 찾을 수 없습니다"
            exit 1
        fi
    else
        print_error "CMakeLists.txt 파일을 찾을 수 없습니다"
        exit 1
    fi

    print_info "현재 버전: $CURRENT_VERSION"
}

# 다음 버전 계산
calculate_next_version() {
    if [[ -n "$RELEASE_VERSION" ]]; then
        NEXT_VERSION="$RELEASE_VERSION"
        print_info "지정된 릴리스 버전: $NEXT_VERSION"
        return
    fi

    IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION"

    case "$RELEASE_TYPE" in
        "patch")
            NEXT_VERSION="$MAJOR.$MINOR.$((PATCH + 1))"
            ;;
        "minor")
            NEXT_VERSION="$MAJOR.$((MINOR + 1)).0"
            ;;
        "major")
            NEXT_VERSION="$((MAJOR + 1)).0.0"
            ;;
        *)
            print_error "잘못된 릴리스 타입: $RELEASE_TYPE"
            exit 1
            ;;
    esac

    print_info "다음 버전: $NEXT_VERSION ($RELEASE_TYPE 릴리스)"
}

# 버전 업데이트
update_version() {
    print_info "버전 업데이트 중: $CURRENT_VERSION -> $NEXT_VERSION"

    if [[ "$DRY_RUN" == "true" ]]; then
        print_info "[DRY RUN] 버전 업데이트 시뮬레이션"
        return
    fi

    # CMakeLists.txt 업데이트
    sed -i.bak "s/VERSION $CURRENT_VERSION/VERSION $NEXT_VERSION/g" "$PROJECT_ROOT/CMakeLists.txt"

    # RELEASE_NOTES.md 업데이트 (있는 경우)
    if [[ -f "$PROJECT_ROOT/RELEASE_NOTES.md" ]]; then
        RELEASE_DATE=$(date +"%Y-%m-%d")
        TEMP_FILE=$(mktemp)

        echo "# LibEtude $NEXT_VERSION ($RELEASE_DATE)" > "$TEMP_FILE"
        echo "" >> "$TEMP_FILE"
        echo "## 변경사항" >> "$TEMP_FILE"
        echo "" >> "$TEMP_FILE"
        echo "- 플랫폼 추상화 레이어 개선" >> "$TEMP_FILE"
        echo "- 성능 최적화" >> "$TEMP_FILE"
        echo "- 버그 수정" >> "$TEMP_FILE"
        echo "" >> "$TEMP_FILE"
        cat "$PROJECT_ROOT/RELEASE_NOTES.md" >> "$TEMP_FILE"

        mv "$TEMP_FILE" "$PROJECT_ROOT/RELEASE_NOTES.md"
    fi

    print_success "버전 업데이트 완료"
}

# 테스트 실행
run_tests() {
    if [[ "$SKIP_TESTS" == "true" ]]; then
        print_warning "테스트 건너뜀"
        return
    fi

    print_info "테스트 실행 중..."

    # 빌드 및 테스트
    BUILD_DIR="$PROJECT_ROOT/build-release-test"
    mkdir -p "$BUILD_DIR"

    cd "$BUILD_DIR"
    cmake -DCMAKE_BUILD_TYPE=Release -DLIBETUDE_ENABLE_PLATFORM_ABSTRACTION=ON "$PROJECT_ROOT"
    cmake --build . --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    if [[ "$DRY_RUN" != "true" ]]; then
        ctest --output-on-failure --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        print_info "[DRY RUN] 테스트 실행 시뮬레이션"
    fi

    # 정리
    cd "$PROJECT_ROOT"
    rm -rf "$BUILD_DIR"

    print_success "테스트 완료"
}

# 문서 생성
generate_docs() {
    if [[ "$SKIP_DOCS" == "true" ]]; then
        print_warning "문서 생성 건너뜀"
        return
    fi

    print_info "문서 생성 중..."

    if [[ "$DRY_RUN" == "true" ]]; then
        print_info "[DRY RUN] 문서 생성 시뮬레이션"
        return
    fi

    # Doxygen 문서 생성 (있는 경우)
    if command -v doxygen &> /dev/null && [[ -f "$PROJECT_ROOT/Doxyfile" ]]; then
        cd "$PROJECT_ROOT"
        doxygen Doxyfile
        print_success "Doxygen 문서 생성 완료"
    fi

    print_success "문서 생성 완료"
}

# 플랫폼별 빌드
build_platforms() {
    print_info "플랫폼별 빌드 시작"

    mkdir -p "$OUTPUT_DIR"

    IFS=',' read -ra PLATFORM_LIST <<< "$PLATFORMS"

    for platform in "${PLATFORM_LIST[@]}"; do
        print_info "플랫폼 빌드: $platform"

        case "$platform" in
            "linux")
                build_linux
                ;;
            "macos")
                build_macos
                ;;
            "windows")
                build_windows
                ;;
            *)
                print_warning "지원하지 않는 플랫폼: $platform"
                ;;
        esac
    done

    print_success "플랫폼별 빌드 완료"
}

# Linux 빌드
build_linux() {
    if [[ "$(uname)" != "Linux" ]]; then
        print_warning "Linux 빌드는 Linux 환경에서만 가능합니다"
        return
    fi

    print_info "Linux 빌드 중..."

    if [[ "$DRY_RUN" == "true" ]]; then
        print_info "[DRY RUN] Linux 빌드 시뮬레이션"
        return
    fi

    # DEB 패키지 빌드
    "$SCRIPT_DIR/package.sh" -t DEB -b Release -o "$OUTPUT_DIR"

    # RPM 패키지 빌드 (가능한 경우)
    if command -v rpmbuild &> /dev/null; then
        "$SCRIPT_DIR/package.sh" -t RPM -b Release -o "$OUTPUT_DIR"
    fi

    # TGZ 패키지 빌드
    "$SCRIPT_DIR/package.sh" -t TGZ -b Release -o "$OUTPUT_DIR"
}

# macOS 빌드
build_macos() {
    if [[ "$(uname)" != "Darwin" ]]; then
        print_warning "macOS 빌드는 macOS 환경에서만 가능합니다"
        return
    fi

    print_info "macOS 빌드 중..."

    if [[ "$DRY_RUN" == "true" ]]; then
        print_info "[DRY RUN] macOS 빌드 시뮬레이션"
        return
    fi

    # DMG 패키지 빌드
    UNIVERSAL_BINARY=true "$SCRIPT_DIR/package.sh" -t DMG -b Release -o "$OUTPUT_DIR"

    # TGZ 패키지 빌드
    "$SCRIPT_DIR/package.sh" -t TGZ -b Release -o "$OUTPUT_DIR"
}

# Windows 빌드
build_windows() {
    print_info "Windows 빌드는 별도의 Windows 환경에서 실행해야 합니다"
    print_info "Windows에서 다음 명령을 실행하세요:"
    print_info "  scripts\\package.bat -t NSIS -b Release -o releases"
    print_info "  scripts\\package.bat -t ZIP -b Release -o releases"
}

# Git 커밋 및 태그
commit_and_tag() {
    if [[ "$DRY_RUN" == "true" ]]; then
        print_info "[DRY RUN] Git 커밋 및 태그 시뮬레이션"
        print_info "[DRY RUN] 커밋 메시지: Release version $NEXT_VERSION"
        print_info "[DRY RUN] 태그: v$NEXT_VERSION"
        return
    fi

    print_info "Git 커밋 및 태그 생성 중..."

    # 변경사항 커밋
    git add .
    git commit -m "Release version $NEXT_VERSION"

    # 태그 생성
    git tag -a "v$NEXT_VERSION" -m "Release version $NEXT_VERSION"

    # 원격 저장소에 푸시
    git push origin "$RELEASE_BRANCH"
    git push origin "v$NEXT_VERSION"

    print_success "Git 커밋 및 태그 완료"
}

# 릴리스 정보 생성
generate_release_info() {
    print_info "릴리스 정보 생성 중..."

    RELEASE_INFO_FILE="$OUTPUT_DIR/RELEASE_INFO_$NEXT_VERSION.md"

    cat > "$RELEASE_INFO_FILE" << EOF
# LibEtude $NEXT_VERSION 릴리스

## 릴리스 정보
- **버전**: $NEXT_VERSION
- **릴리스 타입**: $RELEASE_TYPE
- **릴리스 날짜**: $(date +"%Y-%m-%d")
- **Git 커밋**: $(git rev-parse --short HEAD)
- **Git 브랜치**: $RELEASE_BRANCH

## 빌드된 플랫폼
$PLATFORMS

## 패키지 파일
EOF

    # 생성된 패키지 파일 목록 추가
    if [[ -d "$OUTPUT_DIR" ]]; then
        find "$OUTPUT_DIR" -name "*$NEXT_VERSION*" -type f | while read -r file; do
            echo "- $(basename "$file")" >> "$RELEASE_INFO_FILE"
        done
    fi

    cat >> "$RELEASE_INFO_FILE" << EOF

## 설치 방법

### Linux (DEB)
\`\`\`bash
sudo dpkg -i libetude_${NEXT_VERSION}_amd64.deb
\`\`\`

### Linux (RPM)
\`\`\`bash
sudo rpm -i libetude-${NEXT_VERSION}-1.x86_64.rpm
\`\`\`

### macOS
\`\`\`bash
# DMG 파일을 다운로드하고 마운트하여 설치
\`\`\`

### Windows
\`\`\`cmd
REM NSIS 설치 프로그램 실행
libetude-${NEXT_VERSION}-win64.exe
\`\`\`

## 변경사항
- 플랫폼 추상화 레이어 개선
- 성능 최적화
- 버그 수정

자세한 변경사항은 RELEASE_NOTES.md를 참조하세요.
EOF

    print_success "릴리스 정보 생성 완료: $RELEASE_INFO_FILE"
}

# 정리 작업
cleanup() {
    print_info "정리 작업 중..."

    # 임시 파일 정리
    find "$PROJECT_ROOT" -name "*.bak" -delete 2>/dev/null || true

    print_success "정리 완료"
}

# 메인 함수
main() {
    print_info "LibEtude 자동화된 릴리스 파이프라인 시작"

    if [[ "$DRY_RUN" == "true" ]]; then
        print_warning "DRY RUN 모드 - 실제 변경사항은 적용되지 않습니다"
    fi

    check_git_status
    get_current_version
    calculate_next_version
    update_version
    run_tests
    generate_docs
    build_platforms
    commit_and_tag
    generate_release_info
    cleanup

    print_success "릴리스 파이프라인 완료!"
    print_info "릴리스 버전: $NEXT_VERSION"
    print_info "출력 디렉토리: $OUTPUT_DIR"

    if [[ "$DRY_RUN" != "true" ]]; then
        print_info "GitHub 릴리스 페이지에서 릴리스를 게시하세요"
    fi
}

# 명령행 인수 처리
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--version)
            RELEASE_VERSION="$2"
            shift 2
            ;;
        -t|--type)
            RELEASE_TYPE="$2"
            shift 2
            ;;
        -b|--branch)
            RELEASE_BRANCH="$2"
            shift 2
            ;;
        -p|--platforms)
            PLATFORMS="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -d|--dry-run)
            DRY_RUN="true"
            shift
            ;;
        --skip-tests)
            SKIP_TESTS="true"
            shift
            ;;
        --skip-docs)
            SKIP_DOCS="true"
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

# 메인 실행
main