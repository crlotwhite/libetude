# LibEtude 플랫폼 추상화 레이어 테스트 가이드

이 문서는 LibEtude의 플랫폼 추상화 레이어에 대한 포괄적인 테스트 시스템을 설명합니다.

## 개요

플랫폼 추상화 레이어 테스트 시스템은 다음 세 가지 주요 구성 요소로 이루어져 있습니다:

1. **단위 테스트 프레임워크**: 각 플랫폼별 구현체와 인터페이스 계약 검증
2. **크로스 플랫폼 통합 테스트**: 실제 하드웨어에서의 동작 검증 및 성능 벤치마크
3. **모킹 프레임워크**: 테스트용 모킹 인터페이스와 자동화된 테스트 유틸리티

## 테스트 구조

```
tests/
├── unit/                           # 단위 테스트
│   ├── test_platform_abstraction.h     # 테스트 프레임워크 헤더
│   ├── test_platform_abstraction.c     # 메인 테스트 실행기
│   ├── test_platform_audio_contract.c  # 오디오 인터페이스 계약 테스트
│   ├── test_platform_system_contract.c # 시스템 인터페이스 계약 테스트
│   ├── test_platform_threading_contract.c # 스레딩 인터페이스 계약 테스트
│   ├── test_platform_contracts.c       # 기타 인터페이스 계약 테스트
│   ├── test_platform_mocking.h         # 모킹 프레임워크 헤더
│   └── test_platform_mocking.c         # 모킹 프레임워크 구현
├── integration/                    # 통합 테스트
│   ├── test_platform_integration.h     # 통합 테스트 헤더
│   ├── test_platform_integration.c     # 메인 통합 테스트 실행기
│   ├── test_hardware_validation.c      # 실제 하드웨어 검증 테스트
│   ├── test_performance_benchmarks.c   # 성능 벤치마크 테스트
│   └── test_stress_tests.c             # 스트레스 및 안정성 테스트
└── scripts/                       # 테스트 실행 스크립트
    ├── run_platform_tests.sh          # Unix/Linux/macOS용 스크립트
    └── run_platform_tests.bat         # Windows용 스크립트
```

## 테스트 실행 방법

### 1. 빌드

먼저 프로젝트를 빌드해야 합니다:

```bash
# Debug 빌드 (테스트용 권장)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release 빌드 (성능 테스트용)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 2. 개별 테스트 실행

각 테스트는 독립적으로 실행할 수 있습니다:

```bash
# 단위 테스트
./build/tests/test_platform_abstraction

# 통합 테스트
./build/tests/test_platform_integration

# 모킹 프레임워크 테스트
./build/tests/test_platform_mocking
```

### 3. 스크립트를 통한 실행

편의를 위해 제공되는 스크립트를 사용할 수 있습니다:

```bash
# 모든 테스트 실행
./scripts/run_platform_tests.sh

# 단위 테스트만 실행
./scripts/run_platform_tests.sh --unit

# 통합 테스트만 실행
./scripts/run_platform_tests.sh --integration

# 상세 출력과 함께 실행
./scripts/run_platform_tests.sh --verbose

# 테스트 보고서 생성
./scripts/run_platform_tests.sh --report test_report.txt
```

Windows에서는:

```cmd
REM 모든 테스트 실행
scripts\run_platform_tests.bat

REM 단위 테스트만 실행
scripts\run_platform_tests.bat --unit
```

### 4. CMake/CTest를 통한 실행

CMake의 CTest를 사용할 수도 있습니다:

```bash
cd build
ctest -R Platform  # 플랫폼 관련 테스트만 실행
ctest --verbose     # 상세 출력
```

## 테스트 유형별 설명

### 단위 테스트 (Unit Tests)

단위 테스트는 각 플랫폼별 구현체와 인터페이스 계약을 검증합니다:

- **인터페이스 계약 검증**: 모든 필수 함수 포인터가 올바르게 설정되었는지 확인
- **기본 기능 테스트**: 각 인터페이스의 기본 동작 검증
- **오류 조건 테스트**: NULL 포인터, 잘못된 매개변수 등에 대한 처리 확인
- **경계값 테스트**: 극한 값들에 대한 동작 검증
- **메모리 누수 감지**: 메모리 할당/해제 추적

### 통합 테스트 (Integration Tests)

통합 테스트는 실제 환경에서의 동작을 검증합니다:

- **크로스 플랫폼 호환성**: 플랫폼 간 일관된 동작 확인
- **실제 하드웨어 검증**: 실제 오디오 디바이스, CPU, 메모리 등에서의 동작 테스트
- **성능 벤치마크**: 지연시간, 처리량, 메모리 사용량 등 성능 측정
- **스트레스 테스트**: 높은 부하 상황에서의 안정성 검증
- **장시간 안정성**: 메모리 누수, 리소스 고갈 등 장기 실행 문제 확인

### 모킹 프레임워크 (Mocking Framework)

모킹 프레임워크는 테스트 격리와 제어된 환경을 제공합니다:

- **모킹 인터페이스**: 실제 구현체를 대체하는 테스트용 구현
- **호출 기록**: 함수 호출 횟수, 매개변수, 순서 등 추적
- **기대값 설정**: 예상되는 함수 호출과 반환값 설정
- **매개변수 매처**: 다양한 매개변수 검증 방식 제공
- **자동화된 테스트 실행**: 테스트 스위트 실행 및 보고서 생성

## CI/CD 통합

### GitHub Actions

```yaml
- name: Run Platform Tests
  run: |
    ./scripts/run_platform_tests.sh --ci --report platform_test_results.txt
```

### Jenkins

```groovy
stage('Platform Tests') {
    steps {
        sh './scripts/run_platform_tests.sh --ci'
    }
    post {
        always {
            archiveArtifacts artifacts: '*.txt', fingerprint: true
        }
    }
}
```

## 테스트 작성 가이드

### 새로운 단위 테스트 추가

1. `tests/unit/test_platform_contracts.c`에 테스트 함수 추가
2. `test_platform_abstraction.c`의 테스트 배열에 등록
3. 필요한 경우 새로운 테스트 파일 생성

```c
ETResult test_new_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->new_interface);

    // 테스트 로직 구현

    return ET_SUCCESS;
}
```

### 새로운 통합 테스트 추가

1. 적절한 통합 테스트 파일에 함수 추가
2. `test_platform_integration.c`에서 호출
3. 필요한 경우 새로운 벤치마크나 스트레스 테스트 추가

```c
ETResult test_new_hardware_feature(void) {
    printf("새로운 하드웨어 기능 테스트...\n");

    // 하드웨어 기능 테스트 로직

    printf("새로운 하드웨어 기능 테스트 완료\n");
    return ET_SUCCESS;
}
```

### 모킹 인터페이스 확장

1. `test_platform_mocking.h`에 새로운 모킹 구조체 정의
2. `test_platform_mocking.c`에 구현 추가
3. 생성/파괴/초기화 함수 구현

## 문제 해결

### 일반적인 문제들

1. **테스트 실행 파일을 찾을 수 없음**
   - 프로젝트가 올바르게 빌드되었는지 확인
   - 빌드 디렉토리 경로가 올바른지 확인

2. **오디오 테스트 실패**
   - 헤드리스 환경에서는 오디오 디바이스가 없을 수 있음
   - CI 환경에서는 오디오 테스트가 건너뛰어질 수 있음

3. **권한 오류**
   - 일부 시스템 기능 테스트는 관리자 권한이 필요할 수 있음
   - 네트워크 테스트는 방화벽 설정의 영향을 받을 수 있음

4. **메모리 누수 감지**
   - Valgrind나 AddressSanitizer 사용 권장
   - 테스트 자체의 메모리 추적 기능 활용

### 디버깅 팁

1. **상세 출력 활성화**
   ```bash
   ./build/tests/test_platform_abstraction --verbose
   ```

2. **특정 테스트만 실행**
   ```bash
   # 오디오 테스트만 실행하려면 코드 수정 필요
   ```

3. **메모리 검사 도구 사용**
   ```bash
   valgrind --leak-check=full ./build/tests/test_platform_abstraction
   ```

## 성능 기준

### 예상 성능 지표

- **오디오 지연시간**: < 100ms
- **메모리 할당 속도**: > 1000 할당/초
- **스레드 생성 오버헤드**: < 10ms
- **파일 I/O 속도**: > 10MB/s (SSD 기준)

### 벤치마크 결과 해석

벤치마크 결과는 다음과 같이 해석됩니다:

- **최소/최대/평균 시간**: 성능의 일관성 확인
- **표준편차**: 성능 변동성 측정
- **반복 횟수**: 통계적 신뢰성 확보

## 기여 가이드

새로운 테스트를 추가하거나 기존 테스트를 개선할 때는:

1. 테스트의 목적과 범위를 명확히 정의
2. 플랫폼별 차이점을 고려한 테스트 작성
3. CI 환경에서의 실행 가능성 확인
4. 적절한 문서화 및 주석 추가
5. 기존 테스트와의 일관성 유지

## 참고 자료

- [LibEtude 플랫폼 추상화 레이어 설계 문서](../.kiro/specs/platform-abstraction/design.md)
- [LibEtude 플랫폼 추상화 레이어 요구사항](../.kiro/specs/platform-abstraction/requirements.md)
- [LibEtude 전체 테스트 가이드](./README.md)