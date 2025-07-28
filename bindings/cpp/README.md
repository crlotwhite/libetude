# LibEtude C++ 바인딩

LibEtude C++ 바인딩은 LibEtude C API를 위한 모던 C++ 래퍼입니다. RAII 패턴을 사용하여 자동 리소스 관리를 제공하고, 예외 처리와 타입 안전성을 보장합니다.

## 주요 특징

- **RAII 패턴**: 자동 리소스 관리로 메모리 누수 방지
- **예외 처리**: C 오류 코드를 C++ 예외로 변환
- **타입 안전성**: 강타입 시스템으로 런타임 오류 방지
- **모던 C++**: C++17 표준 기능 활용
- **스레드 안전성**: 멀티스레드 환경에서 안전한 사용
- **비동기 지원**: std::future를 사용한 비동기 처리

## 빌드 요구사항

- C++17 호환 컴파일러 (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.12 이상
- LibEtude 핵심 라이브러리

## 빌드 방법

```bash
# 프로젝트 루트에서
mkdir build && cd build
cmake .. -DLIBETUDE_BUILD_CPP_BINDINGS=ON
make libetude_cpp

# 또는 전체 프로젝트 빌드
make
```

## 설치

```bash
# 빌드 후
sudo make install

# pkg-config 사용 가능
pkg-config --cflags --libs libetude-cpp
```

## 기본 사용법

### 1. 헤더 포함

```cpp
#include <libetude/cpp/engine.hpp>
using namespace libetude;
```

### 2. 엔진 생성 및 기본 음성 합성

```cpp
try {
    // 엔진 생성 (RAII 패턴)
    Engine engine("path/to/model.lef");

    // 텍스트 음성 합성
    std::vector<float> audio = engine.synthesizeText("안녕하세요!");

    // 오디오 데이터 사용
    std::cout << "생성된 오디오 샘플 수: " << audio.size() << std::endl;

} catch (const libetude::Exception& e) {
    std::cerr << "LibEtude 오류: " << e.what() << std::endl;
    std::cerr << "오류 코드: " << e.getErrorCode() << std::endl;
}
```

### 3. 비동기 음성 합성

```cpp
Engine engine("model.lef");

// 비동기 합성 시작
auto future = engine.synthesizeTextAsync("비동기 테스트");

// 다른 작업 수행...

// 결과 대기 및 획득
std::vector<float> audio = future.get();
```

### 4. 실시간 스트리밍

```cpp
Engine engine("model.lef");

// 스트리밍 콜백 설정
engine.startStreaming([](const std::vector<float>& audio) {
    std::cout << "오디오 청크 수신: " << audio.size() << " 샘플" << std::endl;
    // 오디오 재생 또는 저장
});

// 텍스트 스트리밍
engine.streamText("첫 번째 문장");
engine.streamText("두 번째 문장");

// 스트리밍 중지
engine.stopStreaming();
```

### 5. 품질 모드 및 GPU 가속

```cpp
Engine engine("model.lef");

// 품질 모드 설정
engine.setQualityMode(QualityMode::High);

// GPU 가속 활성화
try {
    engine.enableGPUAcceleration();
    std::cout << "GPU 가속 활성화됨" << std::endl;
} catch (const HardwareException& e) {
    std::cout << "GPU 가속 불가: " << e.what() << std::endl;
}

// 성능 통계 확인
PerformanceStats stats = engine.getPerformanceStats();
std::cout << "추론 시간: " << stats.inference_time_ms << "ms" << std::endl;
std::cout << "메모리 사용량: " << stats.memory_usage_mb << "MB" << std::endl;
```

### 6. 확장 모델 사용

```cpp
Engine engine("base_model.lef");

// 확장 모델 로드
int extension_id = engine.loadExtension("speaker_extension.lefx");

// 확장 모델을 사용한 합성
std::vector<float> audio = engine.synthesizeText("확장 모델 테스트");

// 확장 모델 언로드
engine.unloadExtension(extension_id);
```

### 7. 편의 함수 사용

```cpp
// 간단한 텍스트-음성 변환
std::vector<float> audio = textToSpeech("model.lef", "간단한 테스트");

// 엔진 팩토리 함수
auto engine = createEngine("model.lef");
std::vector<float> audio2 = engine->synthesizeText("팩토리 테스트");
```

## 예외 처리

LibEtude C++ 바인딩은 다음과 같은 예외 타입을 제공합니다:

- `Exception`: 기본 예외 클래스
- `InvalidArgumentException`: 잘못된 인수
- `OutOfMemoryException`: 메모리 부족
- `RuntimeException`: 런타임 오류
- `ModelException`: 모델 관련 오류
- `HardwareException`: 하드웨어 관련 오류

```cpp
try {
    Engine engine("model.lef");
    // 작업 수행
} catch (const ModelException& e) {
    // 모델 관련 오류 처리
    std::cerr << "모델 오류: " << e.what() << std::endl;
} catch (const HardwareException& e) {
    // 하드웨어 관련 오류 처리
    std::cerr << "하드웨어 오류: " << e.what() << std::endl;
} catch (const Exception& e) {
    // 기타 LibEtude 오류 처리
    std::cerr << "LibEtude 오류: " << e.what() << std::endl;
    std::cerr << "오류 코드: " << e.getErrorCode() << std::endl;
}
```

## 스레드 안전성

- `Engine` 객체는 스레드 안전하지 않습니다. 여러 스레드에서 동일한 엔진 객체를 사용할 때는 동기화가 필요합니다.
- 정적 유틸리티 함수들(`getVersion()`, `getHardwareFeatures()` 등)은 스레드 안전합니다.
- 스트리밍 콜백은 별도 스레드에서 호출될 수 있으므로 스레드 안전한 코드를 작성해야 합니다.

```cpp
// 스레드 안전한 사용 예제
std::mutex engine_mutex;
Engine engine("model.lef");

// 여러 스레드에서 사용
std::thread t1([&]() {
    std::lock_guard<std::mutex> lock(engine_mutex);
    auto audio = engine.synthesizeText("스레드 1");
});

std::thread t2([&]() {
    std::lock_guard<std::mutex> lock(engine_mutex);
    auto audio = engine.synthesizeText("스레드 2");
});
```

## 성능 최적화 팁

1. **엔진 재사용**: 엔진 객체 생성 비용이 높으므로 가능한 한 재사용하세요.
2. **메모리 예약**: 큰 오디오 데이터를 처리할 때는 벡터 크기를 미리 예약하세요.
3. **비동기 처리**: 긴 텍스트 처리 시 비동기 함수를 사용하세요.
4. **품질 모드 조정**: 실시간 처리가 필요한 경우 `QualityMode::Fast`를 사용하세요.
5. **GPU 가속**: 가능한 경우 GPU 가속을 활성화하세요.

```cpp
// 성능 최적화 예제
Engine engine("model.lef");
engine.setQualityMode(QualityMode::Fast);
engine.enableGPUAcceleration();

// 메모리 예약
std::vector<float> audio;
audio.reserve(22050 * 10); // 10초 분량 예약

// 비동기 처리
auto future = engine.synthesizeTextAsync("긴 텍스트...");
```

## 디버깅

디버그 빌드에서는 추가적인 검증과 로깅이 활성화됩니다:

```bash
# 디버그 빌드
cmake .. -DCMAKE_BUILD_TYPE=Debug -DLIBETUDE_BUILD_CPP_BINDINGS=ON
make
```

## 예제 프로그램

전체 예제는 `examples/cpp_binding/main.cpp`에서 확인할 수 있습니다:

```bash
# 예제 빌드 및 실행
make libetude_cpp_example
./bin/libetude_cpp_example
```

## API 참조

자세한 API 문서는 헤더 파일 `include/libetude/cpp/engine.hpp`의 주석을 참조하세요.

## 문제 해결

### 컴파일 오류

1. **C++17 지원 확인**: 컴파일러가 C++17을 지원하는지 확인하세요.
2. **헤더 경로**: 헤더 파일 경로가 올바르게 설정되었는지 확인하세요.
3. **라이브러리 링크**: libetude_core와 libetude_cpp가 올바르게 링크되었는지 확인하세요.

### 런타임 오류

1. **모델 파일**: 모델 파일 경로가 올바른지 확인하세요.
2. **메모리 부족**: 시스템 메모리가 충분한지 확인하세요.
3. **하드웨어 지원**: GPU 가속 사용 시 하드웨어 지원 여부를 확인하세요.

### 성능 문제

1. **릴리스 빌드**: 성능 측정 시 릴리스 빌드를 사용하세요.
2. **프로파일링**: 성능 통계를 확인하여 병목 지점을 파악하세요.
3. **하드웨어 최적화**: SIMD 및 GPU 가속이 활성화되었는지 확인하세요.

## 라이선스

LibEtude C++ 바인딩은 LibEtude 프로젝트와 동일한 라이선스를 따릅니다.