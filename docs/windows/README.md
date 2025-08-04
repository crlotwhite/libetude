# LibEtude Windows 지원 가이드

LibEtude는 Windows 플랫폼에서 최적화된 성능과 네이티브 기능을 제공합니다. 이 문서는 Windows 환경에서 LibEtude를 효과적으로 사용하는 방법을 설명합니다.

## 목차

- [시스템 요구사항](#시스템-요구사항)
- [빌드 환경 설정](#빌드-환경-설정)
- [Windows 특화 기능](#windows-특화-기능)
- [성능 최적화](#성능-최적화)
- [문제 해결](#문제-해결)
- [예제 프로젝트](#예제-프로젝트)

## 시스템 요구사항

### 최소 요구사항
- Windows 10 (버전 1903 이상) 또는 Windows 11
- Visual Studio 2019 또는 2022 (Community/Professional/Enterprise)
- CMake 3.16 이상
- Windows 10 SDK (최신 버전 권장)

### 권장 요구사항
- Intel/AMD 프로세서 (AVX2 지원)
- 8GB 이상 RAM
- SSD 저장장치
- WASAPI 호환 오디오 디바이스

## 빌드 환경 설정

### Visual Studio를 사용한 빌드

```cmd
# 프로젝트 클론
git clone https://github.com/crlotwhite/libetude.git
cd libetude

# CMake 설정 (Release 빌드)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 빌드 실행
cmake --build build --config Release

# 설치 (선택사항)
cmake --install build --prefix install
```

### MinGW를 사용한 빌드

```cmd
# CMake 설정 (MinGW)
cmake -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# 빌드 실행
cmake --build build-mingw

# 설치
cmake --install build-mingw --prefix install-mingw
```

## Windows 특화 기능

### 1. WASAPI 오디오 백엔드

LibEtude는 Windows에서 WASAPI를 우선적으로 사용하여 저지연 오디오 출력을 제공합니다.

```c
#include <libetude/api.h>
#include <libetude/platform/windows.h>

// Windows 특화 설정
ETWindowsConfig windows_config = {
    .use_wasapi = true,           // WASAPI 사용
    .enable_large_pages = true,   // Large Page 메모리 사용
    .enable_etw_logging = true,   // ETW 로깅 활성화
    .thread_pool_min = 2,         // 최소 스레드 수
    .thread_pool_max = 8          // 최대 스레드 수
};

// LibEtude 초기화
ETResult result = et_windows_init(&windows_config);
if (result != ET_SUCCESS) {
    // 오류 처리
    printf("Windows 초기화 실패: %d\n", result);
    return -1;
}

// 일반적인 LibEtude 사용
ETContext* ctx = et_create_context();
// ... 음성 합성 작업 ...

// 정리
et_destroy_context(ctx);
et_windows_finalize();
```

### 2. 성능 최적화 기능

#### SIMD 최적화 활용

```c
#include <libetude/platform/windows_simd.h>

// CPU 기능 감지
ETWindowsCPUFeatures cpu_features = et_windows_detect_cpu_features();

if (cpu_features.has_avx2) {
    printf("AVX2 지원됨 - 최적화된 연산 사용\n");
    // AVX2 최적화된 함수들이 자동으로 사용됩니다
}

if (cpu_features.has_avx512) {
    printf("AVX-512 지원됨 - 최고 성능 연산 사용\n");
}
```

#### Thread Pool 활용

```c
#include <libetude/platform/windows_threading.h>

// Windows Thread Pool 초기화
ETWindowsThreadPool thread_pool;
ETResult result = et_windows_threadpool_init(&thread_pool, 4, 16);

if (result == ET_SUCCESS) {
    printf("Thread Pool 초기화 성공\n");
    // Thread Pool이 자동으로 병렬 처리에 사용됩니다
}
```

#### Large Page 메모리 사용

```c
#include <libetude/platform/windows_large_pages.h>

// Large Page 권한 활성화 (관리자 권한 필요)
bool large_page_enabled = et_windows_enable_large_page_privilege();

if (large_page_enabled) {
    printf("Large Page 메모리 사용 가능\n");
    // 대용량 메모리 할당 시 자동으로 Large Page 사용
}
```

### 3. 보안 기능 통합

```c
#include <libetude/platform/windows_security.h>

// 보안 기능 확인
bool dep_compatible = et_windows_check_dep_compatibility();
bool uac_permissions = et_windows_check_uac_permissions();

printf("DEP 호환성: %s\n", dep_compatible ? "예" : "아니오");
printf("UAC 권한: %s\n", uac_permissions ? "충분" : "부족");
```

### 4. 개발 도구 통합

#### ETW (Event Tracing for Windows) 로깅

```c
#include <libetude/platform/windows_etw.h>

// ETW 프로바이더 등록
ETResult etw_result = et_windows_register_etw_provider();

if (etw_result == ET_SUCCESS) {
    // 성능 이벤트 로깅
    et_windows_log_performance_event("voice_synthesis", 15.5);

    // 오류 이벤트 로깅
    et_windows_log_error_event(ET_ERROR_INVALID_PARAMETER, "잘못된 매개변수");
}
```

#### Visual Studio 디버깅 지원

Debug 빌드에서는 자동으로 PDB 파일이 생성되어 Visual Studio에서 완전한 디버깅이 가능합니다.

```cmd
# Debug 빌드
cmake -B build-debug -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --config Debug
```

## 성능 최적화

### 1. 컴파일러 최적화

LibEtude는 Windows에서 다음과 같은 최적화를 자동으로 적용합니다:

- **MSVC**: `/O2 /Oi /Ot /Oy /GL /arch:AVX2`
- **MinGW**: `-O3 -march=native -mtune=native`

### 2. 런타임 최적화

```c
// 최적화된 설정 예제
ETWindowsConfig optimized_config = {
    .use_wasapi = true,
    .enable_large_pages = true,
    .enable_etw_logging = false,  // 프로덕션에서는 비활성화
    .thread_pool_min = 4,
    .thread_pool_max = 16
};
```

### 3. 메모리 최적화

- Large Page 메모리 사용으로 TLB 미스 감소
- ASLR 호환 메모리 할당으로 보안성 유지
- 커스텀 메모리 풀로 할당/해제 오버헤드 최소화

## 문제 해결

일반적인 문제와 해결 방법은 [troubleshooting.md](troubleshooting.md)를 참조하세요.

## 예제 프로젝트

- [기본 TTS 예제](../examples/basic_tts/)
- [Windows 개발 도구 예제](../examples/windows_development_tools/)
- [스트리밍 합성 예제](../examples/streaming/)

## 추가 리소스

- [API 문서](../api/)
- [아키텍처 문서](../architecture/)
- [성능 최적화 가이드](performance_optimization.md)