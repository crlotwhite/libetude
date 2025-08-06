# macOS SDK 호환성 가이드

LibEtude는 macOS 15 SDK의 Objective-C 블록 문법 변경사항과의 호환성을 보장하기 위한 포괄적인 해결책을 제공합니다.

## 문제 배경

macOS 15 (Sequoia) SDK부터 Objective-C 블록 문법이 기본적으로 활성화되어, C 컴파일러에서 다음과 같은 문제가 발생할 수 있습니다:

- 블록 관련 키워드 (`__block`, `^`) 인식 오류
- Nullability 어노테이션 (`_Nullable`, `_Nonnull`) 호환성 문제
- CoreAudio API의 블록 기반 콜백 처리 문제

## 해결 방법

### 1. 자동 호환성 모드 (권장)

LibEtude는 macOS 15+ SDK를 자동으로 감지하여 호환성 모드를 활성화합니다:

```bash
# 일반적인 빌드 - 자동 감지
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 2. 수동 호환성 모드

특정 상황에서 수동으로 호환성 모드를 활성화할 수 있습니다:

```bash
# 블록 문법 강제 비활성화
cmake -B build -DCMAKE_C_FLAGS="-DLIBETUDE_MACOS_BLOCKS_DISABLED=1"
cmake --build build
```

### 3. Objective-C++ 컴파일러 사용

Objective-C 코드가 포함된 파일의 경우 `.mm` 확장자를 사용합니다:

```bash
# 이미 적용됨: bindings/objc/src/*.mm
```

## 호환성 기능

### 자동 SDK 버전 감지

```c
#include "libetude/platform/macos_compat.h"

// SDK 버전 확인
int sdk_version = libetude_get_macos_sdk_version();
if (sdk_version >= 15) {
    // macOS 15+ 특화 처리
}
```

### 블록 문법 비활성화

```c
// 자동으로 적용되는 매크로들
#define __BLOCKS__ 0
#define __block
#define _Nullable
#define _Nonnull
```

### 경고 억제

```c
// 호환성 관련 경고 억제
LIBETUDE_SUPPRESS_BLOCK_WARNINGS
// ... CoreAudio 코드 ...
LIBETUDE_RESTORE_WARNINGS
```

## 테스트 및 검증

### 호환성 테스트 실행

```bash
# 자동 호환성 테스트
./scripts/macos_compatibility_test.sh
```

### 수동 검증

```bash
# 1. 빌드 테스트
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 2. 단위 테스트
cd build && ctest

# 3. 예제 실행
./build/examples/basic_tts/basic_tts_example
```

## 지원되는 macOS 버전

| macOS 버전 | SDK 버전 | 호환성 모드 | 상태 |
|------------|----------|-------------|------|
| macOS 12 (Monterey) | 12.x | 기본 | ✅ 완전 지원 |
| macOS 13 (Ventura) | 13.x | 기본 | ✅ 완전 지원 |
| macOS 14 (Sonoma) | 14.x | 기본 | ✅ 완전 지원 |
| macOS 15 (Sequoia) | 15.x | 자동 활성화 | ✅ 완전 지원 |

## 컴파일러 지원

### 권장 컴파일러

1. **Apple Clang** (권장)
   ```bash
   # Xcode Command Line Tools
   xcode-select --install
   ```

2. **LLVM Clang**
   ```bash
   # Homebrew 설치
   brew install llvm
   ```

### 컴파일러별 설정

```cmake
# CMakeLists.txt에서 자동 적용
if(APPLE AND CMAKE_SYSTEM_VERSION VERSION_GREATER_EQUAL "15.0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__BLOCKS__=0 -Wno-nullability-completeness")
endif()
```

## 문제 해결

### 일반적인 오류

1. **블록 문법 오류**
   ```
   error: blocks support disabled
   ```
   **해결책**: 호환성 모드가 제대로 활성화되었는지 확인

2. **Nullability 경고**
   ```
   warning: nullability completeness
   ```
   **해결책**: `-Wno-nullability-completeness` 플래그 추가됨

3. **CoreAudio 링크 오류**
   ```
   error: undefined reference to AudioUnit functions
   ```
   **해결책**: 프레임워크 링크 확인 (`-framework CoreAudio`)

### 디버깅 방법

```bash
# 1. 상세 빌드 로그
cmake --build build --verbose

# 2. 전처리기 출력 확인
gcc -E -I include src/platform/macos/macos_audio.c | grep -A5 -B5 "__BLOCKS__"

# 3. 호환성 상태 확인
./build/tests/test_macos_audio
```

## 성능 영향

호환성 모드는 성능에 미미한 영향을 미칩니다:

- **컴파일 시간**: +2-5% (매크로 처리)
- **런타임 성능**: 영향 없음 (컴파일 타임 최적화)
- **메모리 사용량**: 영향 없음

## 향후 계획

- **macOS 16 SDK 대응**: 2025년 하반기 예정
- **자동 테스트 확장**: CI/CD 파이프라인 통합
- **성능 최적화**: SDK별 최적화 경로 추가

## 참고 자료

- [Apple Developer Documentation - Blocks](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Blocks/)
- [Clang Language Extensions](https://clang.llvm.org/docs/LanguageExtensions.html#blocks)
- [macOS SDK Release Notes](https://developer.apple.com/documentation/macos-release-notes)

## 지원

문제가 발생하면 다음 정보와 함께 이슈를 제출해주세요:

1. macOS 버전 (`sw_vers -productVersion`)
2. Xcode 버전 (`xcodebuild -version`)
3. SDK 경로 (`xcrun --show-sdk-path`)
4. 빌드 로그 및 오류 메시지
5. 호환성 테스트 결과 (`./scripts/macos_compatibility_test.sh`)