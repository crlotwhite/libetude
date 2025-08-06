# LibEtude 개선 실행 계획

## 🎯 즉시 수정이 필요한 문제들

### 1. 테스트 시스템 수정 (우선순위: 높음)

**문제**: CMakeLists.txt에서 잘못된 파일 경로 참조
```cmake
# 현재 문제 코드 (라인 542):
DEPENDS tensor_test test_unity_simple test_math_simple test_fast_math_minimal
#                                                      ^^^^^^^^^^^^^^^^^^^^
# 존재하지 않는 타겟 참조

# 올바른 코드:
DEPENDS tensor_test test_unity_simple test_math_simple fast_math_minimal_test
```

**해결 방법**:
```bash
# tests/CMakeLists.txt 수정
# 1. 중복된 테스트 정의 제거
# 2. 타겟 이름 일관성 확보
# 3. 의존성 경로 수정
```

### 2. 컴파일 경고 수정 (우선순위: 중간)

**quantization.c 수정**:
```c
// 현재 (라인 1464):
size_t batch_shape[ET_MAX_TENSOR_DIMS];  // 설정되지만 미사용

// 수정안:
(void)batch_shape; // 의도적으로 미사용임을 표시
// 또는 실제로 사용하도록 로직 완성
```

**graph.c 수정**:
```c
// 현재 (라인 877):
static int schedule_ready_nodes(ETGraph* graph) {  // 정의되었지만 미사용

// 수정안:
// 1. 함수를 실제로 사용하도록 호출 추가
// 2. 또는 #ifdef DEBUG로 감싸기
// 3. 또는 __attribute__((unused)) 추가
```

### 3. 빌드 시스템 안정화

**cppcheck 설치 및 설정**:
```bash
# Ubuntu/Debian
sudo apt-get install cppcheck

# 스크립트 수정으로 자동 설치 체크 추가
```

## 📊 상세 개선 로드맵

### Phase 1: 기반 안정화 (2주)

#### Week 1: 테스트 시스템 복구
- [ ] **CMakeLists.txt 수정**
  - 중복 테스트 정의 제거
  - 타겟 이름 일관성 확보
  - 의존성 경로 수정
- [ ] **테스트 실행 검증**
  - 모든 독립형 테스트 실행 확인
  - CTest 라벨 시스템 정리
  - 테스트 타임아웃 적절히 설정

#### Week 2: 코드 품질 개선
- [ ] **컴파일 경고 해결**
  - 미사용 변수 제거/표시
  - 미사용 함수 정리
  - 초기화 순서 수정
- [ ] **정적 분석 도구 설정**
  - cppcheck 자동 설치 스크립트
  - GitHub Actions CI 설정
  - 코드 스타일 검사 추가

### Phase 2: 아키텍처 강화 (4주)

#### Week 3-4: 성능 최적화
- [ ] **메모리 관리 개선**
  - 메모리 풀 최적화
  - 누수 감지 개선
  - 할당 전략 최적화
- [ ] **SIMD 최적화 확장**
  - AVX-512 지원 추가
  - ARM NEON 최적화 완성
  - 자동 벡터화 힌트 추가

#### Week 5-6: API 및 사용성 개선
- [ ] **오류 처리 강화**
  - 구조화된 오류 코드 시스템
  - 상세한 오류 메시지
  - 복구 가능한 오류 처리
- [ ] **스트리밍 API 개선**
  - 비동기 처리 지원
  - 백프레셰 처리
  - 실시간 처리 최적화

### Phase 3: 생태계 확장 (8주)

#### Week 7-10: 문서화 및 도구
- [ ] **포괄적인 문서 작성**
  - 아키텍처 가이드
  - 성능 벤치마크 결과
  - 플랫폼별 최적화 가이드
  - API 참조 문서 완성
- [ ] **개발 도구 개선**
  - 모델 변환 도구
  - 성능 프로파일러 UI
  - 디버깅 헬퍼 도구

#### Week 11-14: 언어 바인딩 확장
- [ ] **Python 바인딩**
  - NumPy 통합
  - 딥러닝 프레임워크 호환성
  - Jupyter 노트북 지원
- [ ] **웹 지원**
  - WebAssembly 컴파일
  - JavaScript 바인딩
  - 브라우저 최적화

## 🔧 즉시 실행 가능한 스크립트

### 1. 테스트 수정 스크립트
```bash
#!/bin/bash
# fix_tests.sh

echo "LibEtude 테스트 시스템 수정 중..."

# CMakeLists.txt 백업
cp tests/CMakeLists.txt tests/CMakeLists.txt.backup

# 잘못된 타겟 참조 수정
sed -i 's/test_fast_math_minimal/fast_math_minimal_test/g' tests/CMakeLists.txt

# 중복 정의 제거
sed -i '/^if(EXISTS.*test_fast_math_minimal.c)/,/^endif()/d' tests/CMakeLists.txt

echo "테스트 시스템 수정 완료"
echo "빌드 테스트: cmake --build build --target run_standalone_tests"
```

### 2. 코드 품질 개선 스크립트
```bash
#!/bin/bash
# fix_warnings.sh

echo "컴파일 경고 수정 중..."

# quantization.c 미사용 변수 수정
sed -i '/size_t batch_shape\[ET_MAX_TENSOR_DIMS\];/a\        (void)batch_shape; // 미사용 변수 의도적 표시' src/core/tensor/quantization.c

# graph.c 미사용 함수 표시
sed -i 's/static int schedule_ready_nodes/static int __attribute__((unused)) schedule_ready_nodes/' src/core/graph/graph.c

echo "코드 품질 개선 완료"
```

### 3. 개발 환경 설정 스크립트
```bash
#!/bin/bash
# setup_dev_env.sh

echo "LibEtude 개발 환경 설정 중..."

# 필요한 도구 설치 (Ubuntu/Debian)
if command -v apt-get &> /dev/null; then
    sudo apt-get update
    sudo apt-get install -y cppcheck valgrind cmake-doc
fi

# 빌드 디렉토리 정리 및 재생성
rm -rf build
mkdir build

# CMake 설정
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DLIBETUDE_BUILD_EXAMPLES=ON -DLIBETUDE_BUILD_TOOLS=ON

# 빌드 실행
cmake --build build

echo "개발 환경 설정 완료"
echo "테스트 실행: cd build && ctest -L standalone --output-on-failure"
```

## 📈 성공 지표

### 단기 목표 (2주)
- [ ] 모든 테스트가 성공적으로 실행됨
- [ ] 컴파일 경고 0개
- [ ] CI/CD 파이프라인 정상 작동

### 중기 목표 (6주)
- [ ] 테스트 커버리지 80% 이상
- [ ] 벤치마크 결과 문서화
- [ ] 크로스 플랫폼 빌드 검증

### 장기 목표 (14주)
- [ ] Python 바인딩 완성
- [ ] 웹 지원 (WebAssembly)
- [ ] 포괄적인 문서 및 예제

## 💡 지속적 개선 권장사항

### 1. 코드 리뷰 프로세스
- Pull Request 템플릿 작성
- 코드 리뷰 체크리스트 준비
- 자동화된 코드 품질 검사

### 2. 성능 모니터링
- 정기적인 벤치마크 실행
- 성능 회귀 감지 시스템
- 메모리 사용량 모니터링

### 3. 사용자 피드백 수집
- 이슈 템플릿 개선
- 사용자 설문조사
- 성능 요구사항 수집

---

이 계획을 순차적으로 실행하면 LibEtude는 안정적이고 확장 가능한 고품질 음성 합성 엔진으로 발전할 수 있습니다.