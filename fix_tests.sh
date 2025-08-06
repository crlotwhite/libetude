#!/bin/bash
# LibEtude 테스트 시스템 수정 스크립트

echo "🔧 LibEtude 테스트 시스템 수정 중..."

# 백업 생성
if [ ! -f "tests/CMakeLists.txt.backup" ]; then
    cp tests/CMakeLists.txt tests/CMakeLists.txt.backup
    echo "✅ CMakeLists.txt 백업 생성"
fi

# 잘못된 타겟 참조 수정
echo "🔄 타겟 참조 수정 중..."
sed -i 's/test_fast_math_minimal/fast_math_minimal_test/g' tests/CMakeLists.txt

# 중복된 테스트 정의 제거 (라인 88-102)
echo "🗑️  중복 정의 제거 중..."
sed -i '/^if(EXISTS.*test_fast_math_minimal.c)/,/^endif()/d' tests/CMakeLists.txt

# 빌드 디렉토리 정리
echo "🧹 빌드 캐시 정리 중..."
rm -rf build
mkdir build

# CMake 재설정
echo "⚙️  CMake 설정 중..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 빌드 실행
echo "🔨 빌드 실행 중..."
if cmake --build build; then
    echo "✅ 빌드 성공"
else
    echo "❌ 빌드 실패"
    exit 1
fi

# 테스트 실행
echo "🧪 테스트 실행 중..."
cd build

echo "📋 사용 가능한 테스트 확인 중..."
if ctest -N | grep -q "Total Tests:"; then
    echo "✅ 테스트 발견됨"
    
    echo "🚀 독립형 테스트 실행 중..."
    if ctest -L standalone --output-on-failure; then
        echo "✅ 독립형 테스트 성공"
    else
        echo "⚠️  일부 독립형 테스트 실패 (상세 로그 확인 필요)"
    fi
    
    echo "🧮 수학 테스트 실행 중..."
    if ctest -L math --output-on-failure; then
        echo "✅ 수학 테스트 성공"
    else
        echo "⚠️  일부 수학 테스트 실패"
    fi
    
else
    echo "❌ 테스트를 찾을 수 없음"
fi

cd ..

echo ""
echo "🎉 테스트 시스템 수정 완료!"
echo ""
echo "📊 결과 요약:"
echo "  - CMakeLists.txt 수정 완료"
echo "  - 빌드 시스템 재설정 완료"
echo "  - 테스트 실행 시도 완료"
echo ""
echo "🔍 다음 단계:"
echo "  - 실패한 테스트가 있다면 상세 로그 확인"
echo "  - 컴파일 경고 수정 (fix_warnings.sh 실행)"
echo "  - 코드 품질 검사 실행"
echo ""
echo "💡 유용한 명령어:"
echo "  cd build && ctest -L standalone --verbose  # 상세 테스트 로그"
echo "  cd build && ctest -R TensorTest             # 특정 테스트 실행"
echo "  cmake --build build --target test_info      # 테스트 정보 확인"