#!/bin/bash
# LibEtude 릴리스 테스트 스크립트
# Copyright (c) 2025 LibEtude Project

set -e

# 기본 설정
BUILD_DIR="build"
TEST_DIR="release_test"
PLATFORMS=("linux" "macos" "windows")
ARCHITECTURES=("x64" "arm64")
CONFIGURATIONS=("Release" "Debug")
ENABLE_PERFORMANCE_TEST="ON"
ENABLE_MEMORY_TEST="ON"
ENABLE_INTEGRATION_TEST="ON"
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 로그 함수
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 도움말 함수
show_help() {
    cat << EOF
LibEtude 릴리스 테스트 스크립트

사용법: $0 [옵션]

옵션:
    -h, --help              이 도움말 표시
    -d, --build-dir DIR     빌드 디렉토리 (기본값: build)
    -t, --test-dir DIR      테스트 디렉토리 (기본값: release_test)
    -j, --jobs N            병렬 작업 수 (기본값: CPU 코어 수)
    --no-performance        성능 테스트 비활성화
    --no-memory             메모리 테스트 비활성화
    --no-integration        통합 테스트 비활성화
    --quick                 빠른 테스트 (기본 설정만)
    --full                  전체 테스트 (모든 플랫폼/아키텍처)

테스트 단계:
    1. 빌드 테스트 (모든 설정)
    2. 단위 테스트
    3. 통합 테스트
    4. 성능 테스트
    5. 메모리 누수 테스트
    6. 패키지 테스트
    7. 설치 테스트

예제:
    $0                      기본 릴리스 테스트
    $0 --quick              빠른 테스트
    $0 --full               전체 테스트
    $0 --no-performance     성능 테스트 제외
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
        -t|--test-dir)
            TEST_DIR="$2"
            shift 2
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --no-performance)
            ENABLE_PERFORMANCE_TEST="OFF"
            shift
            ;;
        --no-memory)
            ENABLE_MEMORY_TEST="OFF"
            shift
            ;;
        --no-integration)
            ENABLE_INTEGRATION_TEST="OFF"
            shift
            ;;
        --quick)
            PLATFORMS=("$(uname -s | tr '[:upper:]' '[:lower:]')")
            ARCHITECTURES=("$(uname -m)")
            CONFIGURATIONS=("Release")
            shift
            ;;
        --full)
            ENABLE_PERFORMANCE_TEST="ON"
            ENABLE_MEMORY_TEST="ON"
            ENABLE_INTEGRATION_TEST="ON"
            shift
            ;;
        *)
            echo "알 수 없는 옵션: $1"
            show_help
            exit 1
            ;;
    esac
done

# 테스트 결과 추적
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
TEST_RESULTS=()

# 테스트 결과 기록 함수
record_test_result() {
    local test_name="$1"
    local result="$2"
    local details="$3"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if [[ "$result" == "PASS" ]]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        log_success "$test_name: PASS"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log_error "$test_name: FAIL - $details"
    fi

    TEST_RESULTS+=("$test_name: $result - $details")
}

# 빌드 테스트 함수
test_build() {
    local platform="$1"
    local arch="$2"
    local config="$3"

    log_info "빌드 테스트: $platform-$arch-$config"

    local build_dir="${BUILD_DIR}-${platform}-${arch}-${config}"

    # 빌드 디렉토리 정리
    if [[ -d "$build_dir" ]]; then
        rm -rf "$build_dir"
    fi

    # CMake 구성
    local cmake_args=(
        -B "$build_dir"
        -DCMAKE_BUILD_TYPE="$config"
        -DLIBETUDE_BUILD_TESTS=ON
        -DLIBETUDE_BUILD_EXAMPLES=ON
        -DLIBETUDE_BUILD_TOOLS=ON
    )

    if cmake "${cmake_args[@]}" . >/dev/null 2>&1; then
        # 빌드 실행
        if cmake --build "$build_dir" --config "$config" --parallel "$PARALLEL_JOBS" >/dev/null 2>&1; then
            record_test_result "Build-$platform-$arch-$config" "PASS" ""
            return 0
        else
            record_test_result "Build-$platform-$arch-$config" "FAIL" "빌드 실패"
            return 1
        fi
    else
        record_test_result "Build-$platform-$arch-$config" "FAIL" "CMake 구성 실패"
        return 1
    fi
}

# 단위 테스트 함수
test_unit_tests() {
    local build_dir="$1"

    log_info "단위 테스트 실행"

    if [[ ! -d "$build_dir" ]]; then
        record_test_result "Unit-Tests" "FAIL" "빌드 디렉토리 없음"
        return 1
    fi

    cd "$build_dir"
    if ctest --output-on-failure --parallel "$PARALLEL_JOBS" -R "unit_" >/dev/null 2>&1; then
        record_test_result "Unit-Tests" "PASS" ""
        cd ..
        return 0
    else
        record_test_result "Unit-Tests" "FAIL" "단위 테스트 실패"
        cd ..
        return 1
    fi
}

# 통합 테스트 함수
test_integration() {
    local build_dir="$1"

    if [[ "$ENABLE_INTEGRATION_TEST" != "ON" ]]; then
        return 0
    fi

    log_info "통합 테스트 실행"

    cd "$build_dir"
    if ctest --output-on-failure --parallel "$PARALLEL_JOBS" -R "integration_" >/dev/null 2>&1; then
        record_test_result "Integration-Tests" "PASS" ""
        cd ..
        return 0
    else
        record_test_result "Integration-Tests" "FAIL" "통합 테스트 실패"
        cd ..
        return 1
    fi
}

# 성능 테스트 함수
test_performance() {
    local build_dir="$1"

    if [[ "$ENABLE_PERFORMANCE_TEST" != "ON" ]]; then
        return 0
    fi

    log_info "성능 테스트 실행"

    if [[ -f "$build_dir/tools/benchmark/libetude_benchmarks" ]]; then
        if timeout 300 "$build_dir/tools/benchmark/libetude_benchmarks" --quick >/dev/null 2>&1; then
            record_test_result "Performance-Tests" "PASS" ""
            return 0
        else
            record_test_result "Performance-Tests" "FAIL" "성능 테스트 실패 또는 타임아웃"
            return 1
        fi
    else
        record_test_result "Performance-Tests" "SKIP" "벤치마크 도구 없음"
        return 0
    fi
}

# 메모리 테스트 함수
test_memory() {
    local build_dir="$1"

    if [[ "$ENABLE_MEMORY_TEST" != "ON" ]]; then
        return 0
    fi

    log_info "메모리 누수 테스트 실행"

    # Valgrind 사용 가능 확인
    if command -v valgrind >/dev/null 2>&1; then
        cd "$build_dir"
        if valgrind --leak-check=full --error-exitcode=1 ctest -R "unit_test_memory" >/dev/null 2>&1; then
            record_test_result "Memory-Tests" "PASS" ""
            cd ..
            return 0
        else
            record_test_result "Memory-Tests" "FAIL" "메모리 누수 감지"
            cd ..
            return 1
        fi
    else
        # 내장 메모리 누수 감지기 사용
        if [[ -f "$build_dir/tests/unit/test_memory_leak_detector" ]]; then
            if "$build_dir/tests/unit/test_memory_leak_detector" >/dev/null 2>&1; then
                record_test_result "Memory-Tests" "PASS" ""
                return 0
            else
                record_test_result "Memory-Tests" "FAIL" "메모리 누수 감지"
                return 1
            fi
        else
            record_test_result "Memory-Tests" "SKIP" "메모리 테스트 도구 없음"
            return 0
        fi
    fi
}

# 패키지 테스트 함수
test_packaging() {
    local build_dir="$1"

    log_info "패키지 테스트 실행"

    # CPack을 사용한 패키지 생성 테스트
    cd "$build_dir"
    if cpack >/dev/null 2>&1; then
        record_test_result "Packaging-Tests" "PASS" ""
        cd ..
        return 0
    else
        record_test_result "Packaging-Tests" "FAIL" "패키지 생성 실패"
        cd ..
        return 1
    fi
}

# 설치 테스트 함수
test_installation() {
    local build_dir="$1"

    log_info "설치 테스트 실행"

    local install_dir="$TEST_DIR/install_test"
    mkdir -p "$install_dir"

    # 설치 테스트
    if cmake --install "$build_dir" --prefix "$install_dir" >/dev/null 2>&1; then
        # 설치된 파일 확인
        if [[ -f "$install_dir/lib/libetude.so" || -f "$install_dir/lib/libetude.dylib" || -f "$install_dir/bin/libetude.dll" ]]; then
            if [[ -d "$install_dir/include/libetude" ]]; then
                record_test_result "Installation-Tests" "PASS" ""
                return 0
            else
                record_test_result "Installation-Tests" "FAIL" "헤더 파일 설치 실패"
                return 1
            fi
        else
            record_test_result "Installation-Tests" "FAIL" "라이브러리 설치 실패"
            return 1
        fi
    else
        record_test_result "Installation-Tests" "FAIL" "설치 명령 실패"
        return 1
    fi
}

# 메인 테스트 실행
main() {
    log_info "=== LibEtude 릴리스 테스트 시작 ==="

    # 테스트 디렉토리 생성
    mkdir -p "$TEST_DIR"

    # 현재 플랫폼에서만 테스트 (크로스 컴파일 제외)
    local current_platform
    case "$(uname -s)" in
        Linux*) current_platform="linux" ;;
        Darwin*) current_platform="macos" ;;
        *) current_platform="unknown" ;;
    esac

    local current_arch
    case "$(uname -m)" in
        x86_64) current_arch="x64" ;;
        arm64|aarch64) current_arch="arm64" ;;
        *) current_arch="unknown" ;;
    esac

    # 빌드 테스트
    for config in "${CONFIGURATIONS[@]}"; do
        if test_build "$current_platform" "$current_arch" "$config"; then
            local build_dir="${BUILD_DIR}-${current_platform}-${current_arch}-${config}"

            # 단위 테스트
            test_unit_tests "$build_dir"

            # 통합 테스트
            test_integration "$build_dir"

            # 성능 테스트 (Release 빌드에서만)
            if [[ "$config" == "Release" ]]; then
                test_performance "$build_dir"
                test_memory "$build_dir"
                test_packaging "$build_dir"
                test_installation "$build_dir"
            fi
        fi
    done

    # 테스트 결과 요약
    echo ""
    log_info "=== 테스트 결과 요약 ==="
    echo "총 테스트: $TOTAL_TESTS"
    echo "성공: $PASSED_TESTS"
    echo "실패: $FAILED_TESTS"

    if [[ $FAILED_TESTS -eq 0 ]]; then
        log_success "모든 테스트가 성공했습니다!"
        echo ""
        log_info "릴리스 준비 완료 ✓"
        exit 0
    else
        log_error "$FAILED_TESTS 개의 테스트가 실패했습니다."
        echo ""
        log_info "실패한 테스트:"
        for result in "${TEST_RESULTS[@]}"; do
            if [[ "$result" == *"FAIL"* ]]; then
                echo "  - $result"
            fi
        done
        exit 1
    fi
}

# 스크립트 실행
main "$@"