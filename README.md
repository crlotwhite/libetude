# LibEtude

LibEtude는 음성 합성(voice synthesis)에 특화된 고성능 AI 추론 엔진입니다. 범용 AI 프레임워크와 달리, 음성 합성 도메인에 최적화되어 실시간 처리와 크로스 플랫폼 지원을 위한 모듈러 아키텍처와 하드웨어 최적화를 제공합니다.

## 주요 특징

- **도메인 특화 최적화**: 음성 합성 연산에 집중하여 불필요한 오버헤드 제거
- **모듈러 커널 시스템**: 플랫폼별 하드웨어 최적화 (SIMD, GPU)
- **LEF (LibEtude Efficient Format)**: 파일 크기를 30% 줄이는 효율적인 모델 포맷
- **실시간 음성 합성**: 스트리밍 기능을 갖춘 저지연 처리
- **크로스 플랫폼 지원**: 데스크톱, 모바일, 임베디드 환경 지원
- **고성능 수학 라이브러리**: 음성 합성 연산에 최적화된 수학 함수
- **메모리 효율성**: 제한된 리소스 환경을 위한 설계
- **확장성**: 플러그인 시스템을 통한 동적 기능 추가

## 빠른 시작

### 필요 조건

- CMake 3.15 이상
- C11 호환 컴파일러 (GCC, Clang, MSVC)
- C++17 호환 컴파일러 (바인딩용)

### 빌드 방법

```bash
# 빌드 구성
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 빌드 실행
cmake --build build --config Release

# 설치
cmake --install build
```

### 기본 사용법

```c
#include <libetude/api.h>

int main() {
    // LibEtude 엔진 초기화
    LibEtudeEngine* engine = libetude_create_engine();

    // 모델 로드
    LibEtudeModel* model = libetude_load_model(engine, "model.lef");

    // 텍스트를 음성으로 변환
    const char* text = "안녕하세요, LibEtude입니다.";
    LibEtudeAudio* audio = libetude_synthesize(model, text);

    // 오디오 재생 또는 저장
    libetude_play_audio(audio);

    // 리소스 정리
    libetude_free_audio(audio);
    libetude_free_model(model);
    libetude_free_engine(engine);

    return 0;
}
```

## 플랫폼별 빌드

### Android
```bash
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a
```

### iOS
```bash
cmake -B build-ios \
  -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
  -DPLATFORM=OS64
```

### 임베디드 시스템
```bash
cmake -B build-embedded \
  -DCMAKE_TOOLCHAIN_FILE=<embedded-toolchain>.cmake \
  -DLIBETUDE_MINIMAL=ON
```

## 프로젝트 구조

```
libetude/
├── include/libetude/     # 공개 API 헤더
├── src/                  # 구현 소스 파일
│   ├── core/            # 핵심 엔진 컴포넌트
│   ├── lef/             # LEF 포맷 구현
│   ├── runtime/         # 런타임 시스템
│   ├── audio/           # 오디오 처리
│   └── platform/        # 플랫폼별 코드
├── bindings/            # 언어 바인딩
├── tools/               # 개발 도구
├── tests/               # 테스트 스위트
├── examples/            # 예제 애플리케이션
└── docs/                # 문서
```

## 언어 바인딩

LibEtude는 다양한 프로그래밍 언어를 지원합니다:

- **C++**: 네이티브 C++ 바인딩
- **Java/Android**: JNI를 통한 Android 지원
- **Objective-C/Swift**: iOS 및 macOS 지원

### C++ 사용 예제

```cpp
#include <libetude/cpp/engine.hpp>

int main() {
    libetude::Engine engine;
    auto model = engine.loadModel("model.lef");
    auto audio = model.synthesize("Hello, LibEtude!");
    audio.play();
    return 0;
}
```

## 테스트 실행

```bash
# 모든 테스트 실행
cd build && ctest

# 특정 테스트 스위트 실행
cd build && ctest -R kernel_tests

# 벤치마크 실행
cd build && ./bin/libetude_benchmarks
```

## 성능 프로파일링

```bash
# 성능 프로파일 생성
./bin/libetude_profile --model=<model_path> --output=profile.json

# 메모리 사용량 분석
./bin/libetude_memcheck --model=<model_path>
```

## 예제

프로젝트에는 다양한 사용 사례를 보여주는 예제들이 포함되어 있습니다:

- `examples/basic_tts/`: 기본 텍스트-음성 변환
- `examples/streaming/`: 스트리밍 합성
- `examples/plugins/`: 플러그인 사용법
- `examples/embedded_optimization/`: 임베디드 최적화

## 기여하기

1. 이 저장소를 포크합니다
2. 기능 브랜치를 생성합니다 (`git checkout -b feature/amazing-feature`)
3. 변경사항을 커밋합니다 (`git commit -m 'Add amazing feature'`)
4. 브랜치에 푸시합니다 (`git push origin feature/amazing-feature`)
5. Pull Request를 생성합니다

### 개발 가이드라인

- C11 표준 준수 (플랫폼별 확장 필요시 예외)
- C++17 표준 준수 (바인딩 및 도구)
- 모든 공개 API에 Doxygen 스타일 주석 작성
- 새로운 기능에 대한 단위 테스트 작성

## 라이선스

이 프로젝트는 [라이선스 이름] 라이선스 하에 배포됩니다. 자세한 내용은 `LICENSE` 파일을 참조하세요.

## 지원 및 문의

- 이슈 트래커: [GitHub Issues](https://github.com/crlotwhite/libetude/issues)
- 문서: [docs/](docs/)
- 예제: [examples/](examples/)

## 로드맵

- [ ] GPU 가속 지원 확장 (CUDA, OpenCL, Metal)
- [ ] 추가 언어 바인딩 (Python, Rust)
- [ ] 실시간 스트리밍 최적화
- [ ] 모바일 플랫폼 배터리 최적화
- [ ] 클라우드 배포 지원

---

LibEtude로 고품질 음성 합성을 경험해보세요! 🎵