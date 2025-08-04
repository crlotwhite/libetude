# Visual Studio 프로젝트 템플릿

이 문서는 Visual Studio에서 LibEtude를 사용하는 프로젝트를 빠르게 설정하는 방법을 설명합니다.

## 목차

- [프로젝트 템플릿 설치](#프로젝트-템플릿-설치)
- [새 프로젝트 생성](#새-프로젝트-생성)
- [프로젝트 설정](#프로젝트-설정)
- [예제 코드](#예제-코드)
- [빌드 및 실행](#빌드-및-실행)

## 프로젝트 템플릿 설치

### 1. 템플릿 파일 다운로드

LibEtude Visual Studio 템플릿을 다운로드하여 다음 위치에 저장:

```
%USERPROFILE%\Documents\Visual Studio 2022\Templates\ProjectTemplates\
```

### 2. 템플릿 구조

```
LibEtudeTemplate/
├── LibEtudeTemplate.vstemplate
├── main.cpp
├── CMakeLists.txt
├── vcpkg.json
└── README.md
```

## 새 프로젝트 생성

### 1. Visual Studio에서 새 프로젝트 생성

1. Visual Studio 2022 실행
2. "새 프로젝트 만들기" 선택
3. "LibEtude Application" 템플릿 선택
4. 프로젝트 이름과 위치 설정

### 2. 자동 생성되는 파일들

#### main.cpp
```cpp
#include <iostream>
#include <libetude/api.h>
#include <libetude/platform/windows.h>

int main() {
    std::cout << "LibEtude Windows Application\n";

    // Windows 특화 설정
    ETWindowsConfig windows_config = {
        .use_wasapi = true,
        .enable_large_pages = true,
        .enable_etw_logging = true,
        .thread_pool_min = 2,
        .thread_pool_max = 8
    };

    // LibEtude 초기화
    ETResult result = et_windows_init(&windows_config);
    if (result != ET_SUCCESS) {
        std::cerr << "LibEtude 초기화 실패: " << result << std::endl;
        return -1;
    }

    // LibEtude 컨텍스트 생성
    ETContext* ctx = et_create_context();
    if (!ctx) {
        std::cerr << "컨텍스트 생성 실패" << std::endl;
        et_windows_finalize();
        return -1;
    }

    // 여기에 음성 합성 코드 추가
    std::cout << "LibEtude가 성공적으로 초기화되었습니다!" << std::endl;

    // 정리
    et_destroy_context(ctx);
    et_windows_finalize();

    return 0;
}
```

#### CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.16)
project(LibEtudeApp VERSION 1.0.0)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LibEtude 찾기
find_package(LibEtude REQUIRED)

# 실행 파일 생성
add_executable(${PROJECT_NAME} main.cpp)

# LibEtude 링크
target_link_libraries(${PROJECT_NAME} PRIVATE LibEtude::LibEtude)

# Windows 특화 설정
if(WIN32)
    # Windows 서브시스템 설정 (콘솔 애플리케이션)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )

    # Windows API 링크
    target_link_libraries(${PROJECT_NAME} PRIVATE
        kernel32 user32 ole32 oleaut32 uuid
        dsound winmm ksuser
    )

    # MSVC 최적화 설정
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:/O2 /Oi /Ot /Oy /GL /arch:AVX2>
            $<$<CONFIG:Debug>:/Zi>
        )

        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:/LTCG>
            $<$<CONFIG:Debug>:/DEBUG:FULL>
        )
    endif()
endif()

# 컴파일러별 경고 설정
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

#### vcpkg.json (선택사항)
```json
{
    "name": "libetude-app",
    "version": "1.0.0",
    "description": "LibEtude Application",
    "dependencies": [
        "libetude"
    ]
}
```

## 프로젝트 설정

### 1. LibEtude 라이브러리 설정

#### 방법 1: NuGet 패키지 사용 (권장)

1. 솔루션 탐색기에서 프로젝트 우클릭
2. "NuGet 패키지 관리" 선택
3. "LibEtude" 검색 후 설치

#### 방법 2: CMake 사용

```cmd
# 프로젝트 디렉토리에서
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

#### 방법 3: 수동 설정

프로젝트 속성에서:

1. **C/C++ > 일반 > 추가 포함 디렉터리**:
   ```
   C:\LibEtude\include
   ```

2. **링커 > 일반 > 추가 라이브러리 디렉터리**:
   ```
   C:\LibEtude\lib\x64\Release
   ```

3. **링커 > 입력 > 추가 종속성**:
   ```
   libetude.lib
   kernel32.lib
   user32.lib
   ole32.lib
   oleaut32.lib
   uuid.lib
   dsound.lib
   winmm.lib
   ksuser.lib
   ```

### 2. 디버그 설정

#### Debug 구성
- **최적화**: 사용 안 함 (/Od)
- **디버그 정보**: 프로그램 데이터베이스 (/Zi)
- **런타임 라이브러리**: 다중 스레드 디버그 DLL (/MDd)

#### Release 구성
- **최적화**: 최대 최적화 (/O2)
- **디버그 정보**: 없음
- **런타임 라이브러리**: 다중 스레드 DLL (/MD)
- **전체 프로그램 최적화**: 예 (/GL)

## 예제 코드

### 기본 음성 합성 예제

```cpp
#include <iostream>
#include <vector>
#include <libetude/api.h>
#include <libetude/platform/windows.h>

class LibEtudeApp {
private:
    ETContext* context_;

public:
    LibEtudeApp() : context_(nullptr) {}

    ~LibEtudeApp() {
        cleanup();
    }

    bool initialize() {
        // Windows 특화 설정
        ETWindowsConfig windows_config = {
            .use_wasapi = true,
            .enable_large_pages = true,
            .enable_etw_logging = false,  // 프로덕션에서는 비활성화
            .thread_pool_min = 2,
            .thread_pool_max = 8
        };

        // LibEtude 초기화
        ETResult result = et_windows_init(&windows_config);
        if (result != ET_SUCCESS) {
            std::cerr << "Windows 초기화 실패: " << result << std::endl;
            return false;
        }

        // 컨텍스트 생성
        context_ = et_create_context();
        if (!context_) {
            std::cerr << "컨텍스트 생성 실패" << std::endl;
            return false;
        }

        return true;
    }

    bool synthesizeText(const std::string& text, std::vector<float>& output) {
        if (!context_) {
            std::cerr << "컨텍스트가 초기화되지 않았습니다" << std::endl;
            return false;
        }

        // 음성 합성 실행
        ETSynthesisConfig config = {
            .sample_rate = 22050,
            .channels = 1,
            .format = ET_AUDIO_FORMAT_FLOAT32
        };

        ETResult result = et_synthesize_text(context_, text.c_str(), &config,
                                           output.data(), output.size());

        if (result != ET_SUCCESS) {
            std::cerr << "음성 합성 실패: " << result << std::endl;
            return false;
        }

        return true;
    }

    void cleanup() {
        if (context_) {
            et_destroy_context(context_);
            context_ = nullptr;
        }
        et_windows_finalize();
    }
};

int main() {
    std::cout << "LibEtude Windows 애플리케이션 시작\n";

    LibEtudeApp app;

    if (!app.initialize()) {
        std::cerr << "애플리케이션 초기화 실패" << std::endl;
        return -1;
    }

    // 음성 합성 테스트
    std::string text = "안녕하세요, LibEtude입니다.";
    std::vector<float> audio_output(22050 * 5); // 5초 분량

    if (app.synthesizeText(text, audio_output)) {
        std::cout << "음성 합성 성공!" << std::endl;
        std::cout << "오디오 샘플 수: " << audio_output.size() << std::endl;
    } else {
        std::cerr << "음성 합성 실패" << std::endl;
        return -1;
    }

    std::cout << "애플리케이션 종료\n";
    return 0;
}
```

### Windows 특화 기능 사용 예제

```cpp
#include <iostream>
#include <libetude/platform/windows_simd.h>
#include <libetude/platform/windows_threading.h>
#include <libetude/platform/windows_etw.h>

void demonstrateWindowsFeatures() {
    // CPU 기능 감지
    ETWindowsCPUFeatures cpu_features = et_windows_detect_cpu_features();

    std::cout << "=== CPU 기능 정보 ===" << std::endl;
    std::cout << "SSE4.1 지원: " << (cpu_features.has_sse41 ? "예" : "아니오") << std::endl;
    std::cout << "AVX 지원: " << (cpu_features.has_avx ? "예" : "아니오") << std::endl;
    std::cout << "AVX2 지원: " << (cpu_features.has_avx2 ? "예" : "아니오") << std::endl;
    std::cout << "AVX-512 지원: " << (cpu_features.has_avx512 ? "예" : "아니오") << std::endl;

    // Thread Pool 초기화
    ETWindowsThreadPool thread_pool;
    ETResult result = et_windows_threadpool_init(&thread_pool, 4, 16);

    if (result == ET_SUCCESS) {
        std::cout << "\n=== Thread Pool 정보 ===" << std::endl;
        std::cout << "Thread Pool 초기화 성공" << std::endl;
    }

    // ETW 로깅 (선택사항)
    if (et_windows_register_etw_provider() == ET_SUCCESS) {
        std::cout << "\n=== ETW 로깅 ===" << std::endl;
        std::cout << "ETW 프로바이더 등록 성공" << std::endl;

        // 성능 이벤트 로깅
        et_windows_log_performance_event("app_startup", 150.5);
    }
}
```

## 빌드 및 실행

### 1. Visual Studio에서 빌드

1. **솔루션 구성**: Release 또는 Debug 선택
2. **솔루션 플랫폼**: x64 선택 (권장)
3. **빌드 > 솔루션 빌드** (Ctrl+Shift+B)

### 2. 명령줄에서 빌드

```cmd
# CMake 설정
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 빌드
cmake --build build --config Release

# 실행
build\Release\LibEtudeApp.exe
```

### 3. 디버깅

1. **F5** 키로 디버깅 시작
2. 중단점 설정으로 코드 흐름 확인
3. **출력** 창에서 ETW 이벤트 확인

## 성능 최적화 팁

### 1. 컴파일러 최적화

Release 빌드에서 다음 설정 확인:
- **최적화**: 최대 최적화 (/O2)
- **내장 함수**: 예 (/Oi)
- **전체 프로그램 최적화**: 예 (/GL)

### 2. 런타임 최적화

```cpp
// 애플리케이션 시작 시 우선순위 설정
SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

// CPU 친화성 설정 (고성능 코어 사용)
DWORD_PTR process_affinity, system_affinity;
GetProcessAffinityMask(GetCurrentProcess(), &process_affinity, &system_affinity);
SetProcessAffinityMask(GetCurrentProcess(), system_affinity);
```

### 3. 메모리 최적화

```cpp
// Large Page 권한 활성화 (관리자 권한 필요)
if (et_windows_enable_large_page_privilege()) {
    std::cout << "Large Page 메모리 사용 가능" << std::endl;
}
```

## 문제 해결

일반적인 문제와 해결 방법:

1. **링크 오류**: 라이브러리 경로와 종속성 확인
2. **런타임 오류**: Visual C++ 재배포 패키지 설치
3. **성능 문제**: Release 빌드 및 최적화 설정 확인
4. **오디오 문제**: 오디오 드라이버 업데이트

자세한 문제 해결 방법은 [troubleshooting.md](troubleshooting.md)를 참조하세요.

## 추가 리소스

- [LibEtude API 문서](../api/)
- [Windows 성능 최적화 가이드](performance_optimization.md)
- [예제 프로젝트](../../examples/windows_development_tools/)
- [GitHub 저장소](https://github.com/crlotwhite/libetude)