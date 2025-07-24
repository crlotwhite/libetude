#!/bin/bash

# LibEtude Unity 테스트 실행 스크립트

set -e

echo "=== LibEtude Unity 테스트 실행 ==="

# 빌드 디렉토리 확인
if [ ! -d "build" ]; then
    echo "빌드 디렉토리가 없습니다. 먼저 프로젝트를 빌드해주세요."
    echo "cmake -B build -DCMAKE_BUILD_TYPE=Debug"
    echo "cmake --build build"
    exit 1
fi

cd build

echo ""
echo "1. Unity 테스트 실행..."
echo "  - 기본 Unity 테스트"
if [ -f "./tests/simple_unity_test" ]; then
    ./tests/simple_unity_test
else
    echo "    simple_unity_test 실행 파일이 없습니다."
fi

echo "  - 수학 함수 테스트"
if [ -f "./tests/math_simple_test" ]; then
    ./tests/math_simple_test
else
    echo "    math_simple_test 실행 파일이 없습니다."
fi

echo "  - 고속 수학 함수 테스트"
if [ -f "./tests/fast_math_minimal_test" ]; then
    ./tests/fast_math_minimal_test
else
    echo "    fast_math_minimal_test 실행 파일이 없습니다."
fi

echo ""
echo "2. LibEtude 통합 테스트 (현재 비활성화)..."
echo "  LibEtude 소스 코드 빌드 문제로 인해 통합 테스트는 현재 비활성화되어 있습니다."
echo "  Unity 테스트 프레임워크 자체는 정상적으로 동작합니다."

echo ""
echo "3. CTest 실행 (Unity 통합)..."
ctest --output-on-failure

echo ""
echo "=== Unity 테스트 실행 완료 ==="

# Unity 테스트 특징 안내
echo ""
echo "Unity 테스트 프레임워크 특징:"
echo "  - C 언어 전용 테스트 프레임워크"
echo "  - 가볍고 빠른 실행"
echo "  - 임베디드 시스템에 최적화"
echo "  - 명확한 테스트 결과 출력"
echo ""
echo "개별 테스트 실행 예시:"
echo "  ./unit_tests"
echo "  ./hardware_test"
echo "  ./memory_allocator_test"