#!/bin/bash

# LibEtude cppcheck 실행 스크립트
# 사용법: ./scripts/run_cppcheck.sh

PROJECT_ROOT=$(dirname "$(dirname "$(realpath "$0")")")
cd "$PROJECT_ROOT"

echo "LibEtude cppcheck 정적 분석 실행 중..."
echo "프로젝트 경로: $PROJECT_ROOT"

# 기본 cppcheck 옵션
CPPCHECK_OPTS=(
    --enable=all
    --error-exitcode=1
    -I include
    -I include/libetude
    --suppress=missingInclude
    --suppress=missingIncludeSystem
    --suppress=unusedFunction
    --suppress=staticFunction
    --suppress=variableScope
    --suppress=toomanyconfigs
    --suppress=normalCheckLevelMaxBranches
    --suppress=unmatchedSuppression
    --suppress=knownConditionTrueFalse
    --suppress=unreadVariable
    --suppress=constParameterCallback
    --suppress=constVariablePointer
    --suppress=constParameterPointer
    --suppress=duplicateBreak
    --suppress=redundantCondition
    --suppress=redundantInitialization
    --suppress=invalidPrintfArgType_uint
    --suppress=invalidPointerCast
    --suppress=memleak
    --template=gcc
    --std=c11
    --language=c
)

# 검사할 소스 디렉토리
SRC_DIRS=(
    "src/audio/dsp"
    "src/runtime/plugin"
    "src/runtime/scheduler"
    "src/runtime/profiler"
    "src/lef/format"
    "src/platform"
)

echo "검사 대상 디렉토리:"
printf "  %s\n" "${SRC_DIRS[@]}"
echo

# 각 디렉토리에 대해 cppcheck 실행
exit_code=0
for dir in "${SRC_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        echo "검사 중: $dir"
        if ! cppcheck "${CPPCHECK_OPTS[@]}" "$dir" 2>&1; then
            exit_code=1
        fi
        echo
    else
        echo "경고: 디렉토리를 찾을 수 없습니다: $dir"
    fi
done

if [ $exit_code -eq 0 ]; then
    echo "✅ cppcheck 정적 분석이 성공적으로 완료되었습니다."
else
    echo "❌ cppcheck에서 오류가 발견되었습니다."
fi

exit $exit_code
