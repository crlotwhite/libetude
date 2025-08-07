#!/bin/bash

# LibEtude 플랫폼 추상화 레이어 테스트 실행 스크립트

set -e  # 오류 발생 시 스크립트 중단

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 로그 함수들
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

# 사용법 출력
usage() {
    echo "사용법: $0 [옵션]"
    echo "옵션:"
    echo "  -u, --unit          단위 테스트만 실행"
    echo "  -i, --integration   통합 테스트만 실행"
    echo "  -m, --mocking       모킹 테스트만 실행"
    echo "  -a, --all           모든 테스트 실행 (기본값)"
    echo "  -v, --verbose       상세 출력"
    echo "  -c, --ci            CI 환경 모드"
    echo "  -r, --report FILE   테스트 보고서 파일 생성"
    echo "  -h, --help          도움말 출력"
    exit 1
}

# 기본 설정
RUN_UNIT=false
RUN_INTEGRATION=false
RUN_MOCKING=false
RUN_ALL=true
VERBOSE=false
CI_MODE=false
REPORT_FILE=""
BUILD_DIR="build"

# 명령행 인수 파싱
while [[ $# -gt 0 ]]; do
    case $1 in
        -u|--unit)
            RUN_UNIT=true
            RUN_ALL=false
            shift
            ;;
        -i|--integration)
            RUN_INTEGRATION=true
            RUN_ALL=false
            shift
            ;;
        -m|--mocking)
            RUN_MOCKING=true
            RUN_ALL=false
            shift
            ;;
        -a|--all)
            RUN_ALL=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -c|--ci)
            CI_MODE=true
            shift
            ;;
        -r|--report)
            REPORT_FILE="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            log_error "알 수 없는 옵션: $1"
            usage
            ;;
    esac
done

# CI 환경 감지
if [[ -n "$CI" || -n "$GITHUB_ACTIONS" || -n "$JENKINS_URL" ]]; then
    CI_MODE=true
    log_info "CI 환경이 감지되었습니다"
fi

# 빌드 디렉토리 확인
if [[ ! -d "$BUILD_DIR" ]]; then
    log_error "빌드 디렉토리를 찾을 수 없습니다: $BUILD_DIR"
    log_info "먼저 프로젝트를 빌드해주세요:"
    log_info "  cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Debug"
    log_info "  cmake --build $BUILD_DIR"
    exit 1
fi

# 테스트 실행 함수
run_test() {
    local test_name=$1
    local test_executable=$2
    local description=$3

    log_info "실행 중: $description"

    if [[ ! -f "$BUILD_DIR/tests/$test_executable" ]]; then
        log_warning "테스트 실행 파일을 찾을 수 없습니다: $test_executable"
        return 1
    fi

    local cmd="$BUILD_DIR/tests/$test_executable"
    if [[ "$VERBOSE" == "true" ]]; then
        cmd="$cmd --verbose"
    fi

    if [[ "$CI_MODE" == "true" ]]; then
        # CI 환경에서는 타임아웃 설정
        timeout 300 $cmd
        local exit_code=$?
        if [[ $exit_code -eq 124 ]]; then
            log_error "$test_name 테스트가 타임아웃되었습니다 (5분)"
            return 1
        elif [[ $exit_code -ne 0 ]]; then
            log_error "$test_name 테스트가 실패했습니다 (종료 코드: $exit_code)"
            return 1
        fi
    else
        if ! $cmd; then
            log_error "$test_name 테스트가 실패했습니다"
            return 1
        fi
    fi

    log_success "$test_name 테스트 완료"
    return 0
}

# 테스트 결과 추적
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# 테스트 실행
log_info "LibEtude 플랫폼 추상화 레이어 테스트 시작"
log_info "========================================"

if [[ "$RUN_ALL" == "true" || "$RUN_UNIT" == "true" ]]; then
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if run_test "단위 테스트" "test_platform_abstraction" "플랫폼 추상화 레이어 단위 테스트"; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
fi

if [[ "$RUN_ALL" == "true" || "$RUN_INTEGRATION" == "true" ]]; then
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if run_test "통합 테스트" "test_platform_integration" "플랫폼 추상화 레이어 통합 테스트"; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
fi

if [[ "$RUN_ALL" == "true" || "$RUN_MOCKING" == "true" ]]; then
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    if run_test "모킹 테스트" "test_platform_mocking" "플랫폼 추상화 레이어 모킹 프레임워크 테스트"; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
fi

# 결과 요약
echo ""
log_info "테스트 결과 요약"
log_info "================"
echo "총 테스트: $TOTAL_TESTS"
echo "통과: $PASSED_TESTS"
echo "실패: $FAILED_TESTS"

# 테스트 보고서 생성
if [[ -n "$REPORT_FILE" ]]; then
    log_info "테스트 보고서 생성 중: $REPORT_FILE"
    cat > "$REPORT_FILE" << EOF
LibEtude 플랫폼 추상화 레이어 테스트 보고서
==========================================

실행 시간: $(date)
총 테스트: $TOTAL_TESTS
통과: $PASSED_TESTS
실패: $FAILED_TESTS
성공률: $(( PASSED_TESTS * 100 / TOTAL_TESTS ))%

테스트 환경:
- OS: $(uname -s)
- 아키텍처: $(uname -m)
- 빌드 디렉토리: $BUILD_DIR
- CI 모드: $CI_MODE
EOF
    log_success "테스트 보고서가 생성되었습니다: $REPORT_FILE"
fi

# CI 환경용 출력
if [[ "$CI_MODE" == "true" ]]; then
    # GitHub Actions용 출력
    if [[ -n "$GITHUB_ACTIONS" ]]; then
        echo "::set-output name=total_tests::$TOTAL_TESTS"
        echo "::set-output name=passed_tests::$PASSED_TESTS"
        echo "::set-output name=failed_tests::$FAILED_TESTS"

        if [[ $FAILED_TESTS -gt 0 ]]; then
            echo "::error::$FAILED_TESTS개의 테스트가 실패했습니다"
        fi
    fi

    # Jenkins용 출력
    if [[ -n "$JENKINS_URL" ]]; then
        echo "JENKINS_TEST_RESULTS=total:$TOTAL_TESTS,passed:$PASSED_TESTS,failed:$FAILED_TESTS"
    fi
fi

# 최종 결과
if [[ $FAILED_TESTS -eq 0 ]]; then
    log_success "모든 테스트가 성공적으로 완료되었습니다! ✓"
    exit 0
else
    log_error "$FAILED_TESTS개의 테스트가 실패했습니다 ✗"
    exit 1
fi