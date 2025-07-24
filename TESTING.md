# LibEtude 테스트 가이드

LibEtude 프로젝트는 Unity 테스트 프레임워크를 사용하여 크로스 플랫폼 테스트를 지원합니다.

## 🚀 빠른 시작

### 1. 프로젝트 빌드
```bash
# 프로젝트 설정
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 빌드 (테스트 포함)
cmake --build build
```

### 2. 테스트 실행
```bash
# 모든 독립적인 테스트 실행
cmake --build build --target run_standalone_tests

# 또는 CTest 사용
cd build && ctest -L standalone --output-on-failure
```

## 📋 사용 가능한 테스트

### 독립적인 테스트 (권장)
이 테스트들은 LibEtude 소스 코드에 의존하지 않고 독립적으로 실행됩니다:

1. **UnityBasicTest** - Unity 프레임워크 기본 기능 테스트
2. **MathFunctionsTest** - 기본 수학 함수 테스트
3. **FastMathTest** - 고속 수학 함수 테스트

## 🛠️ CMake 타겟

### 테스트 실행 타겟
```bash
# 모든 독립적인 테스트
cmake --build build --target run_standalone_tests

# Unity 프레임워크 테스트만
cmake --build build --target run_unity_tests

# 수학 함수 테스트만
cmake --build build --target run_math_tests

# 개별 테스트 실행
cmake --build build --target run_unity_basic
cmake --build build --target run_math_functions
cmake --build build --target run_fast_math
```

### 테스트 정보 확인
```bash
cmake --build build --target test_info
```

## 🔍 CTest 사용법

### 기본 사용법
```bash
cd build

# 모든 테스트 실행
ctest

# 독립적인 테스트만 실행
ctest -L standalone

# 수학 관련 테스트만 실행
ctest -L math

# Unity 관련 테스트만 실행
ctest -L unity

# 상세한 출력으로 실행
ctest --output-on-failure --verbose

# 특정 테스트만 실행
ctest -R UnityBasicTest
```

### 테스트 라벨
- `standalone`: 독립적으로 실행 가능한 테스트
- `unity`: Unity 프레임워크 관련 테스트
- `math`: 수학 함수 관련 테스트
- `basic`: 기본 기능 테스트
- `fast`: 고속 처리 관련 테스트
- `functions`: 함수 테스트

## 🌍 플랫폼별 사용법

### Windows
```cmd
REM 프로젝트 설정
cmake -B build -DCMAKE_BUILD_TYPE=Debug

REM 빌드
cmake --build build --config Debug

REM 테스트 실행
cmake --build build --target run_standalone_tests --config Debug

REM 또는 CTest 사용
cd build
ctest -C Debug -L standalone --output-on-failure
```

### macOS/Linux
```bash
# 프로젝트 설정
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 빌드
cmake --build build

# 테스트 실행
cmake --build build --target run_standalone_tests

# 또는 CTest 사용
cd build && ctest -L standalone --output-on-failure
```

## 📊 테스트 결과 해석

### 성공적인 테스트 출력 예시
```
Test project /path/to/libetude/build
    Start 1: UnityBasicTest
1/3 Test #1: UnityBasicTest ...................   Passed    0.00 sec
    Start 2: MathFunctionsTest
2/3 Test #2: MathFunctionsTest ................   Passed    0.00 sec
    Start 3: FastMathTest
3/3 Test #3: FastMathTest .....................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 3
```

### 테스트 통계
- **UnityBasicTest**: 4개 개별 테스트 (assertion, float, string, null pointer)
- **MathFunctionsTest**: 5개 개별 테스트 (기본 연산, 경계값, 벡터, 표준 함수, 정밀도)
- **FastMathTest**: 7개 개별 테스트 (초기화, 지수, 로그, 삼각함수, 활성화 함수, 성능)

**총 16개의 개별 테스트가 실행됩니다.**

## 🔧 문제 해결

### 일반적인 문제들

1. **빌드 오류**
   ```bash
   # 빌드 캐시 정리
   rm -rf build
   cmake -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ```

2. **테스트 실행 파일을 찾을 수 없음**
   - 먼저 빌드가 완료되었는지 확인
   - `cmake --build build --target test_unity_simple` 등으로 개별 빌드 시도

3. **윈도우에서 수학 라이브러리 오류**
   - CMake가 자동으로 플랫폼별 수학 라이브러리 링크를 처리합니다
   - 윈도우에서는 별도의 수학 라이브러리가 필요하지 않습니다

## 📝 테스트 추가하기

새로운 독립적인 테스트를 추가하려면:

1. 테스트 소스 파일 생성 (예: `test_new_feature.c`)
2. `tests/CMakeLists.txt`에 다음 추가:
   ```cmake
   add_executable(test_new_feature ../test_new_feature.c)
   target_link_libraries(test_new_feature unity)
   target_include_directories(test_new_feature PRIVATE ${unity_SOURCE_DIR}/src)
   target_compile_definitions(test_new_feature PRIVATE UNITY_INCLUDE_DOUBLE UNITY_INCLUDE_FLOAT)

   add_test(NAME NewFeatureTest COMMAND test_new_feature)
   set_tests_properties(NewFeatureTest PROPERTIES
       TIMEOUT 30
       LABELS "standalone;new_feature"
   )
   ```

## 🎯 권장 워크플로우

1. **개발 중**: `cmake --build build --target run_standalone_tests`
2. **CI/CD**: `cd build && ctest -L standalone --output-on-failure`
3. **디버깅**: `cd build && ctest -R SpecificTest --verbose`
4. **전체 검증**: `cd build && ctest --output-on-failure`

이 가이드를 따르면 Windows, macOS, Linux에서 동일한 방식으로 테스트를 실행할 수 있습니다.