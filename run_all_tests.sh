#!/bin/bash

echo "=== LibEtude 전체 테스트 실행 ==="
echo

# 테스트 결과 카운터
total_tests=0
passed_tests=0
failed_tests=0

# 함수: 테스트 실행 및 결과 확인
run_test() {
    local test_name="$1"
    local test_executable="$2"

    echo "[$test_name] 실행 중..."

    if [ ! -f "$test_executable" ]; then
        echo "❌ $test_name: 실행 파일을 찾을 수 없습니다 ($test_executable)"
        ((failed_tests++))
        return 1
    fi

    if ./"$test_executable"; then
        echo "✅ $test_name: 성공"
        ((passed_tests++))
    else
        echo "❌ $test_name: 실패"
        ((failed_tests++))
    fi

    ((total_tests++))
    echo
}

# 각 테스트 실행
run_test "Unity 기본 테스트" "test_unity_simple"
run_test "수학 함수 테스트" "test_math_simple"
run_test "고속 수학 함수 테스트" "test_fast_math_minimal"

# 결과 요약
echo "=== 테스트 결과 요약 ==="
echo "총 테스트: $total_tests"
echo "성공: $passed_tests"
echo "실패: $failed_tests"
echo

if [ $failed_tests -eq 0 ]; then
    echo "🎉 모든 테스트가 성공했습니다!"
    exit 0
else
    echo "⚠️  일부 테스트가 실패했습니다."
    exit 1
fi