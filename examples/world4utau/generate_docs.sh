#!/bin/bash

# world4utau API 문서 생성 스크립트

set -e

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 함수 정의
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 스크립트 시작
print_status "world4utau API 문서 생성을 시작합니다..."

# 현재 디렉토리 확인
if [ ! -f "Doxyfile" ]; then
    print_error "Doxyfile을 찾을 수 없습니다. examples/world4utau 디렉토리에서 실행해주세요."
    exit 1
fi

# Doxygen 설치 확인
if ! command -v doxygen &> /dev/null; then
    print_error "Doxygen이 설치되어 있지 않습니다."
    print_status "설치 방법:"
    print_status "  Ubuntu/Debian: sudo apt-get install doxygen"
    print_status "  macOS: brew install doxygen"
    print_status "  Windows: https://www.doxygen.nl/download.html"
    exit 1
fi

# 기존 문서 디렉토리 정리
if [ -d "docs" ]; then
    print_status "기존 문서 디렉토리를 정리합니다..."
    rm -rf docs
fi

# 문서 생성
print_status "Doxygen을 사용하여 API 문서를 생성합니다..."
doxygen Doxyfile

# 생성 결과 확인
if [ -d "docs/html" ]; then
    print_success "API 문서가 성공적으로 생성되었습니다!"
    print_status "문서 위치: $(pwd)/docs/html/index.html"

    # 파일 개수 확인
    html_files=$(find docs/html -name "*.html" | wc -l)
    print_status "생성된 HTML 파일 수: $html_files"

    # 브라우저에서 열기 (선택사항)
    if command -v xdg-open &> /dev/null; then
        read -p "브라우저에서 문서를 열까요? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            xdg-open docs/html/index.html
        fi
    elif command -v open &> /dev/null; then
        read -p "브라우저에서 문서를 열까요? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            open docs/html/index.html
        fi
    fi

else
    print_error "문서 생성에 실패했습니다."
    exit 1
fi

# 추가 정보 출력
print_status ""
print_status "생성된 문서 구조:"
print_status "  docs/html/index.html     - 메인 페이지"
print_status "  docs/html/files.html     - 파일 목록"
print_status "  docs/html/globals.html   - 전역 함수/변수"
print_status "  docs/html/annotated.html - 구조체/클래스 목록"
print_status ""
print_status "문서 업데이트 방법:"
print_status "  1. 헤더 파일의 Doxygen 주석 수정"
print_status "  2. 이 스크립트 재실행: ./generate_docs.sh"
print_status ""

print_success "문서 생성 완료!"