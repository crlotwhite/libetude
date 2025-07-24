# LibEtude Unity 테스트 가이드

LibEtude 프로젝트의 Unity 기반 테스트 시스템에 대한 가이드입니다.

## 테스트 구조

```
tests/
├── unit/              # 단위 테스트 (Unity)
│   ├── test_engine.c
│   ├── test_fast_math.c
│   ├── test_hardware.c
│   ├── test_kernel_registry.c
│   ├── test_kernels.c
│   ├── test_memory_allocator.c
│   ├── test_memory_optimization.c
│   ├── test_memory.c
│   ├── test_simd_kernels.c
│   └── test_tensor.c
├── integration/       # 통합 테스트 (Unity)
│   ├── test_pipeline.c
│   └── test_streaming.c
├── performance/       # 성능 테스트 (Unity + 시간 측정)
│   ├── benchmark_inference.c
│   └── benchmark_kernels.c
└── data/             # 테스트 데이터
```

## 테스트 실행 방법

### 1. 전체 테스트 실행

```bash
# 프로젝트 루트에서
./run_tests.sh
```

### 2. CMake를 통한 테스트 실행

```bash
# 빌드 후
cd build
ctest --output-on-failure
```

### 3. Unity 개별 테스트 실행

```bash
cd build

# 단위 테스트 (모든 테스트)
./unit_tests

# 통합 테스트
./integration_tests

# 성능 테스트
./performance_tests

# 개별 컴포넌트 테스트
./hardware_test
./memory_allocator_test
./memory_optimization_test
./fast_math_test
./simd_kernels_test
./kernel_registry_test
```

### 4. 특정 라벨의 테스트만 실행

```bash
cd build

# 메모리 관련 테스트만
ctest -L memory

# 성능 테스트만
ctest -L performance

# 단위 테스트만
ctest -L unit
```

## GoogleTest 작성 가이드

### 기본 테스트 작성

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "libetude/your_module.h"
}

// 테스트 픽스처 클래스
class YourModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트 전 초기화
    }

    void TearDown() override {
        // 테스트 후 정리
    }
};

// 테스트 케이스
TEST_F(YourModuleTest, TestFunctionName) {
    // Arrange
    int expected = 42;

    // Act
    int actual = your_function();

    // Assert
    EXPECT_EQ(expected, actual);
    EXPECT_TRUE(condition);
    EXPECT_FLOAT_EQ(expected_float, actual_float);
    EXPECT_NEAR(expected_float, actual_float, tolerance);
}
```

### 주요 GoogleTest 매크로

- `EXPECT_EQ(expected, actual)` - 값 비교
- `EXPECT_NE(val1, val2)` - 값 불일치 확인
- `EXPECT_TRUE(condition)` - 조건 참 확인
- `EXPECT_FALSE(condition)` - 조건 거짓 확인
- `EXPECT_FLOAT_EQ(expected, actual)` - 부동소수점 비교
- `EXPECT_NEAR(val1, val2, tolerance)` - 허용 오차 내 비교
- `ASSERT_*` - 실패 시 테스트 중단 (EXPECT_*는 계속 진행)

## 테스트 작성 가이드

### 테스트 프레임워크 사용

```c
#include "framework/test_framework.h"

void test_example_function(void) {
    // 테스트 코드
    TEST_ASSERT(condition, "테스트 설명");
    TEST_ASSERT_EQ(expected, actual, "값 비교 테스트");
    TEST_ASSERT_FLOAT_EQ(expected, actual, tolerance, "부동소수점 비교");
}

int main(void) {
    TEST_SUITE_START("예제 테스트 스위트");

    RUN_TEST(test_example_function);

    TEST_SUITE_END();
    return 0;
}
```

### 새로운 테스트 추가

1. 적절한 디렉토리에 테스트 파일 생성
2. `tests/CMakeLists.txt`에 테스트 파일 추가
3. 필요시 새로운 실행 파일 및 CTest 항목 추가

## 코드 커버리지

디버그 빌드에서 커버리지 활성화:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build
cd build
ctest
# 커버리지 리포트 생성
gcov ../tests/unit/*.c
```

## 성능 테스트

성능 테스트는 별도로 실행되며, 벤치마크 결과를 제공합니다:

```bash
cd build
./performance_tests
```

## 메모리 누수 검사

Valgrind를 사용한 메모리 누수 검사:

```bash
cd build
valgrind --leak-check=full ./unit_tests
```

## 테스트 데이터

테스트에 필요한 데이터 파일은 `tests/data/` 디렉토리에 저장하며, 빌드 시 자동으로 복사됩니다.
## U
nity 사용 가이드

### 기본 테스트 작성

```c
#include "unity.h"
#include "libetude/your_module.h"

// 테스트 설정 함수
void setUp(void) {
    // 테스트 전 초기화
}

// 테스트 정리 함수
void tearDown(void) {
    // 테스트 후 정리
}

// 테스트 함수
void test_your_function_name(void) {
    // Arrange
    int expected = 42;

    // Act
    int actual = your_function();

    // Assert
    TEST_ASSERT_EQUAL(expected, actual);
    TEST_ASSERT_TRUE(condition);
    TEST_ASSERT_FLOAT_WITHIN(tolerance, expected_float, actual_float);
}

// Unity 테스트 러너
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_your_function_name);

    return UNITY_END();
}
```

### 주요 Unity 매크로

- `TEST_ASSERT_EQUAL(expected, actual)` - 값 비교
- `TEST_ASSERT_NOT_EQUAL(expected, actual)` - 값 불일치 확인
- `TEST_ASSERT_TRUE(condition)` - 조건 참 확인
- `TEST_ASSERT_FALSE(condition)` - 조건 거짓 확인
- `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)` - 부동소수점 비교
- `TEST_ASSERT_NULL(pointer)` - NULL 포인터 확인
- `TEST_ASSERT_NOT_NULL(pointer)` - 비NULL 포인터 확인
- `TEST_PASS()` - 테스트 성공 표시
- `TEST_FAIL_MESSAGE(message)` - 메시지와 함께 실패

### Unity의 장점

- **C 언어 전용**: C 프로젝트에 완벽하게 맞춤
- **가벼움**: 최소한의 오버헤드로 빠른 실행
- **임베디드 친화적**: 메모리 제약이 있는 환경에서도 사용 가능
- **간단한 문법**: 직관적이고 배우기 쉬운 API
- **포터블**: 다양한 플랫폼에서 동작
- **명확한 출력**: 테스트 결과를 명확하게 표시