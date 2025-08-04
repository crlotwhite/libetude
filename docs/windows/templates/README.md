# $projectname$

LibEtude를 사용하는 Windows 애플리케이션입니다.

## 프로젝트 개요

이 프로젝트는 LibEtude AI 음성 합성 엔진을 활용하여 Windows 환경에서 최적화된 성능을 제공하는 애플리케이션입니다.

## 주요 기능

- **WASAPI 오디오 백엔드**: Windows 네이티브 오디오 API를 사용한 저지연 출력
- **SIMD 최적화**: AVX2/AVX-512 명령어를 활용한 고성능 연산
- **Thread Pool**: Windows Thread Pool API를 사용한 효율적인 병렬 처리
- **Large Page 메모리**: 메모리 성능 최적화
- **ETW 로깅**: Windows 성능 분석 도구와 통합

## 시스템 요구사항

### 최소 요구사항
- Windows 10 (버전 1903 이상) 또는 Windows 11
- Visual Studio 2019 또는 2022
- CMake 3.16 이상
- 4GB RAM

### 권장 요구사항
- Intel/AMD 프로세서 (AVX2 지원)
- 8GB 이상 RAM
- SSD 저장장치
- WASAPI 호환 오디오 디바이스

## 빌드 방법

### Visual Studio 사용

1. **프로젝트 열기**:
   ```cmd
   # Visual Studio에서 .sln 파일 열기
   start $safeprojectname$.sln
   ```

2. **빌드 구성 선택**:
   - Release (권장): 최적화된 성능
   - Debug: 디버깅 및 개발용

3. **빌드 실행**:
   - `Ctrl+Shift+B` 또는 "빌드 > 솔루션 빌드"

### CMake 사용

```cmd
# 프로젝트 설정
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 빌드
cmake --build build --config Release

# 실행
build\Release\$safeprojectname$.exe
```

### MinGW 사용

```cmd
# 프로젝트 설정
cmake -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# 빌드
cmake --build build-mingw

# 실행
build-mingw\$safeprojectname$.exe
```

## 사용 방법

### 기본 실행

```cmd
$safeprojectname$.exe
```

애플리케이션을 실행하면 다음과 같은 기능을 사용할 수 있습니다:

1. **음성 합성 데모**: 미리 정의된 텍스트를 음성으로 변환
2. **시스템 정보 확인**: CPU 기능, 메모리 상태 등 확인
3. **사용자 정의 텍스트 합성**: 원하는 텍스트를 입력하여 음성 변환
4. **대화형 메뉴**: 다양한 기능을 쉽게 접근

### 고급 설정

애플리케이션의 동작을 커스터마이징하려면 `main.cpp`에서 다음 설정을 수정하세요:

```cpp
ETWindowsConfig windows_config = {
    .use_wasapi = true,           // WASAPI 사용 여부
    .enable_large_pages = true,   // Large Page 메모리 사용
    .enable_etw_logging = false,  // ETW 로깅 (개발 시에만)
    .thread_pool_min = 2,         // 최소 스레드 수
    .thread_pool_max = 8          // 최대 스레드 수
};
```

## 성능 최적화

### 컴파일 시간 최적화

- Release 모드로 빌드
- 전체 프로그램 최적화 활성화 (`/GL`, `/LTCG`)
- 타겟 CPU 아키텍처 지정 (`/arch:AVX2`)

### 런타임 최적화

- Large Page 권한 활성화 (관리자 권한 필요)
- WASAPI 배타 모드 사용
- 적절한 스레드 풀 크기 설정

### 성능 측정

애플리케이션은 자동으로 다음 성능 지표를 측정합니다:

- **처리 시간**: 음성 합성에 소요된 시간 (ms)
- **RTF (Real-Time Factor)**: 실시간 대비 처리 속도
- **메모리 사용량**: 현재 메모리 사용 상태

## 문제 해결

### 일반적인 문제

1. **빌드 오류**:
   - Windows SDK 설치 확인
   - Visual Studio 버전 확인 (2019 이상)
   - CMake 버전 확인 (3.16 이상)

2. **런타임 오류**:
   - Visual C++ 재배포 패키지 설치
   - 오디오 드라이버 업데이트
   - 관리자 권한으로 실행 (Large Page 사용 시)

3. **성능 문제**:
   - Release 모드로 빌드 확인
   - CPU 기능 지원 확인 (AVX2/AVX-512)
   - 메모리 사용량 모니터링

### 로그 및 디버깅

Debug 빌드에서는 상세한 로그 정보가 출력됩니다:

```cpp
#ifdef _DEBUG
    // 디버그 정보 출력
    std::cout << "디버그 모드에서 실행 중" << std::endl;
#endif
```

ETW 로깅을 활성화하면 Windows Performance Toolkit으로 성능 분석이 가능합니다.

## 개발 가이드

### 코드 구조

```cpp
class $safeprojectname$App {
    // 애플리케이션 초기화
    bool initialize();

    // 시스템 정보 출력
    void printSystemInfo();

    // 음성 합성 데모
    bool runSynthesisDemo();

    // 대화형 메뉴
    void runInteractiveMenu();

    // 리소스 정리
    void cleanup();
};
```

### 확장 방법

새로운 기능을 추가하려면:

1. `$safeprojectname$App` 클래스에 메서드 추가
2. `runInteractiveMenu()`에 메뉴 항목 추가
3. 필요한 LibEtude API 호출 구현

### 테스트

단위 테스트를 추가하려면:

```cpp
// test_$safeprojectname$.cpp
#include <gtest/gtest.h>
#include "$safeprojectname$App.h"

TEST($safeprojectname$Test, InitializationTest) {
    $safeprojectname$App app;
    EXPECT_TRUE(app.initialize());
}
```

## 라이선스

이 프로젝트는 LibEtude 라이선스를 따릅니다.

## 지원 및 문의

- **GitHub Issues**: 버그 리포트 및 기능 요청
- **문서**: [LibEtude Windows 가이드](https://github.com/crlotwhite/libetude/docs/windows/)
- **예제**: [Windows 개발 도구 예제](https://github.com/crlotwhite/libetude/examples/windows_development_tools/)

## 버전 히스토리

- **v1.0.0**: 초기 릴리스
  - 기본 음성 합성 기능
  - Windows 특화 최적화
  - 대화형 사용자 인터페이스