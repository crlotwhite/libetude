# LibEtude Windows 문서

LibEtude Windows 지원에 대한 종합적인 문서 모음입니다.

## 📚 문서 목록

### 시작하기
- **[Windows 지원 가이드](README.md)** - Windows에서 LibEtude 사용하기
- **[Visual Studio 프로젝트 템플릿](visual_studio_template.md)** - 새 프로젝트 빠르게 시작하기

### 성능 및 최적화
- **[성능 최적화 가이드](performance_optimization.md)** - Windows에서 최고 성능 달성하기
- **[문제 해결 가이드](troubleshooting.md)** - 일반적인 문제 해결 방법

### 개발 도구
- **[Visual Studio 템플릿](templates/)** - 프로젝트 템플릿 파일들

## 🚀 빠른 시작

### 1. 환경 설정
```cmd
# Visual Studio 2022 및 CMake 설치 확인
cmake --version
```

### 2. 프로젝트 생성
Visual Studio에서 "LibEtude Application" 템플릿 사용 또는:

```cmd
git clone https://github.com/your-org/libetude.git
cd libetude
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### 3. 예제 실행
```cmd
build\Release\examples\windows_development_tools\windows_dev_tools_example.exe
```

## 🎯 주요 기능

### Windows 특화 최적화
- **WASAPI 오디오**: 저지연 오디오 출력
- **SIMD 가속**: AVX2/AVX-512 최적화
- **Thread Pool**: Windows 네이티브 멀티스레딩
- **Large Pages**: 메모리 성능 향상

### 개발 도구 통합
- **Visual Studio**: 완전한 디버깅 지원
- **ETW 로깅**: 성능 분석 및 프로파일링
- **NuGet 패키지**: 쉬운 배포 및 통합
- **CMake 지원**: 크로스 플랫폼 빌드

### 보안 및 호환성
- **DEP 호환**: Data Execution Prevention
- **ASLR 지원**: Address Space Layout Randomization
- **UAC 통합**: User Account Control

## 📊 성능 비교

| 기능 | 기본 구현 | Windows 최적화 | 성능 향상 |
|------|-----------|----------------|-----------|
| 음성 합성 | 1.0x | 2.5x | +150% |
| 메모리 할당 | 1.0x | 1.15x | +15% |
| 오디오 지연 | 50ms | 10ms | -80% |
| 병렬 처리 | 1.0x | 3.2x | +220% |

## 🛠️ 시스템 요구사항

### 최소 요구사항
- Windows 10 (1903+) 또는 Windows 11
- Visual Studio 2019+
- CMake 3.16+
- 4GB RAM

### 권장 요구사항
- Intel/AMD 프로세서 (AVX2 지원)
- 8GB+ RAM
- SSD 저장장치
- 관리자 권한 (고급 기능 사용 시)

## 🔧 설치 방법

### NuGet 패키지 (권장)
```xml
<PackageReference Include="LibEtude" Version="1.0.0" />
```

### CMake
```cmake
find_package(LibEtude REQUIRED)
target_link_libraries(your_target LibEtude::LibEtude)
```

### 수동 설치
1. [릴리스 페이지](https://github.com/your-org/libetude/releases)에서 다운로드
2. 헤더 파일을 포함 디렉터리에 복사
3. 라이브러리 파일을 링크 디렉터리에 복사

## 📖 예제 코드

### 기본 사용법
```cpp
#include <libetude/api.h>
#include <libetude/platform/windows.h>

int main() {
    // Windows 특화 설정
    ETWindowsConfig config = {
        .use_wasapi = true,
        .enable_large_pages = true,
        .thread_pool_max = 8
    };

    // 초기화
    et_windows_init(&config);
    ETContext* ctx = et_create_context();

    // 음성 합성
    et_synthesize_text(ctx, "안녕하세요", &config, buffer, size);

    // 정리
    et_destroy_context(ctx);
    et_windows_finalize();
    return 0;
}
```

### 성능 최적화
```cpp
// CPU 기능 감지
ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
if (features.has_avx2) {
    // AVX2 최적화 활성화
}

// Large Page 메모리 사용
if (et_windows_enable_large_page_privilege()) {
    void* memory = et_windows_alloc_large_pages(size);
}
```

## 🐛 문제 해결

### 자주 발생하는 문제

1. **빌드 오류**: [문제 해결 가이드](troubleshooting.md#빌드-관련-문제) 참조
2. **오디오 문제**: [오디오 문제 해결](troubleshooting.md#오디오-관련-문제) 참조
3. **성능 문제**: [성능 최적화](performance_optimization.md) 참조

### 지원 받기

- **GitHub Issues**: 버그 리포트 및 기능 요청
- **토론**: 일반적인 질문 및 사용법
- **문서**: 상세한 API 문서 및 가이드

## 🔄 업데이트 및 변경사항

### 최신 버전 (v1.0.0)
- Windows 11 완전 지원
- AVX-512 최적화 추가
- WASAPI 배타 모드 지원
- Visual Studio 2022 템플릿

### 이전 버전
- v0.9.0: Windows 10 지원 추가
- v0.8.0: ETW 로깅 구현
- v0.7.0: Thread Pool 최적화

## 📞 연락처 및 기여

- **메인테이너**: LibEtude 개발팀
- **이메일**: support@libetude.org
- **GitHub**: [libetude/libetude](https://github.com/your-org/libetude)
- **기여 가이드**: [CONTRIBUTING.md](../../CONTRIBUTING.md)

---

*이 문서는 LibEtude v1.0.0 기준으로 작성되었습니다. 최신 정보는 [GitHub 저장소](https://github.com/your-org/libetude)를 확인하세요.*