# LibEtude C++ 바인딩 문서

## 개요

LibEtude C++ 바인딩은 LibEtude C API를 위한 모던 C++ 래퍼를 제공합니다. RAII 패턴을 사용하여 자동 리소스 관리를 제공하고, 예외 처리와 타입 안전성을 보장합니다.

### 주요 특징

- **RAII 패턴**: 자동 리소스 관리로 메모리 누수 방지
- **예외 처리**: C 오류 코드를 C++ 예외로 변환
- **타입 안전성**: 강타입 열거형과 STL 컨테이너 사용
- **모던 C++**: C++17 표준 기능 활용 (std::future, std::function 등)
- **스레드 안전성**: 내장된 동기화 메커니즘

## 헤더 파일 포함

```cpp
#include <libetude/cpp/engine.hpp>
```

## 네임스페이스

모든 C++ 바인딩은 `libetude` 네임스페이스에 정의되어 있습니다.

```cpp
using namespace libetude;
```

## 예외 처리

### 예외 클래스 계층

```cpp
libetude::Exception                    // 기본 예외 클래스
├── InvalidArgumentException           // 잘못된 인수
├── OutOfMemoryException              // 메모리 부족
├── RuntimeException                  // 런타임 오류
├── ModelException                    // 모델 관련 오류
└── HardwareException                 // 하드웨어 관련 오류
```

### 예외 처리 예제

```cpp
try {
    libetude::Engine engine("model.lef");
    auto audio = engine.synthesizeText("안녕하세요");

} catch (const libetude::ModelException& e) {
    std::cerr << "모델 오류: " << e.what() << std::endl;
    std::cerr << "오류 코드: " << e.getErrorCode() << std::endl;

} catch (const libetude::OutOfMemoryException& e) {
    std::cerr << "메모리 부족: " << e.what() << std::endl;

} catch (const libetude::Exception& e) {
    std::cerr << "LibEtude 오류: " << e.what() << std::endl;

} catch (const std::exception& e) {
    std::cerr << "일반 오류: " << e.what() << std::endl;
}
```

## Engine 클래스

### 생성자 및 소멸자

#### Engine(const std::string& model_path)

```cpp
// 모델 파일로부터 엔진 생성
libetude::Engine engine("path/to/model.lef");

// 예외 처리와 함께
try {
    libetude::Engine engine("model.lef");
    // 엔진 사용...
} catch (const libetude::ModelException& e) {
    std::cerr << "모델 로딩 실패: " << e.what() << std::endl;
}
```

**매개변수**:
- `model_path`: 모델 파일 경로 (.lef 또는 .lefx)

**예외**:
- `ModelException`: 모델 로딩 실패
- `OutOfMemoryException`: 메모리 부족

#### 이동 생성자 및 대입 연산자

```cpp
// 이동 생성자 사용
libetude::Engine createEngine(const std::string& path) {
    return libetude::Engine(path);  // 이동 생성자 호출
}

auto engine = createEngine("model.lef");

// 이동 대입 연산자 사용
libetude::Engine engine1("model1.lef");
libetude::Engine engine2("model2.lef");
engine1 = std::move(engine2);  // engine2는 더 이상 유효하지 않음
```

### 음성 합성 (동기 처리)

#### synthesizeText

```cpp
std::vector<float> synthesizeText(const std::string& text);
```

**설명**: 텍스트를 음성으로 합성합니다.

**매개변수**:
- `text`: 합성할 텍스트 (UTF-8 인코딩)

**반환값**: 생성된 오디오 데이터 (float 벡터)

**예외**:
- `InvalidArgumentException`: 잘못된 텍스트 입력
- `RuntimeException`: 합성 실패

**사용 예제**:
```cpp
libetude::Engine engine("model.lef");

try {
    auto audio = engine.synthesizeText("안녕하세요, LibEtude입니다.");

    std::cout << "생성된 오디오 길이: " << audio.size() << " 샘플" << std::endl;

    // 오디오 데이터를 파일로 저장
    saveAudioToFile(audio, "output.wav");

} catch (const libetude::Exception& e) {
    std::cerr << "합성 실패: " << e.what() << std::endl;
}
```

#### synthesizeSinging

```cpp
std::vector<float> synthesizeSinging(const std::string& lyrics,
                                    const std::vector<float>& notes);
```

**설명**: 가사와 음표를 노래로 합성합니다.

**매개변수**:
- `lyrics`: 가사 텍스트 (UTF-8 인코딩)
- `notes`: 음표 배열 (MIDI 노트 번호)

**반환값**: 생성된 오디오 데이터 (float 벡터)

**사용 예제**:
```cpp
libetude::Engine engine("singing_model.lef");

std::string lyrics = "도레미파솔라시도";
std::vector<float> notes = {60, 62, 64, 65, 67, 69, 71, 72}; // C4-C5

try {
    auto audio = engine.synthesizeSinging(lyrics, notes);
    std::cout << "노래 합성 완료: " << audio.size() << " 샘플" << std::endl;

} catch (const libetude::Exception& e) {
    std::cerr << "노래 합성 실패: " << e.what() << std::endl;
}
```

### 비동기 음성 합성

#### synthesizeTextAsync

```cpp
std::future<std::vector<float>> synthesizeTextAsync(const std::string& text);
```

**설명**: 텍스트를 비동기로 음성 합성합니다.

**반환값**: 오디오 데이터를 담은 future 객체

**사용 예제**:
```cpp
libetude::Engine engine("model.lef");

// 비동기 합성 시작
auto future = engine.synthesizeTextAsync("긴 텍스트를 비동기로 처리합니다.");

// 다른 작업 수행
std::cout << "합성 중... 다른 작업을 수행합니다." << std::endl;
performOtherTasks();

// 결과 대기 및 가져오기
try {
    auto audio = future.get();  // 완료될 때까지 대기
    std::cout << "비동기 합성 완료: " << audio.size() << " 샘플" << std::endl;

} catch (const libetude::Exception& e) {
    std::cerr << "비동기 합성 실패: " << e.what() << std::endl;
}
```

#### 타임아웃과 함께 사용

```cpp
libetude::Engine engine("model.lef");

auto future = engine.synthesizeTextAsync("텍스트");

// 5초 타임아웃
if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
    auto audio = future.get();
    std::cout << "합성 완료" << std::endl;
} else {
    std::cout << "타임아웃 발생" << std::endl;
}
```

### 실시간 스트리밍

#### AudioStreamCallback

```cpp
using AudioStreamCallback = std::function<void(const std::vector<float>& audio)>;
```

**설명**: 스트리밍 중 생성된 오디오 데이터를 받는 콜백 함수 타입입니다.

#### startStreaming

```cpp
void startStreaming(AudioStreamCallback callback);
```

**설명**: 실시간 스트리밍을 시작합니다.

**매개변수**:
- `callback`: 오디오 데이터 콜백 함수

**사용 예제**:
```cpp
libetude::Engine engine("model.lef");

// 콜백 함수 정의
auto audioCallback = [](const std::vector<float>& audio) {
    std::cout << "오디오 청크 수신: " << audio.size() << " 샘플" << std::endl;

    // 실제로는 오디오를 재생하거나 파일에 저장
    playAudio(audio);
};

try {
    // 스트리밍 시작
    engine.startStreaming(audioCallback);

    // 텍스트 스트리밍
    engine.streamText("첫 번째 문장입니다.");
    engine.streamText("두 번째 문장입니다.");

    // 잠시 대기
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 스트리밍 중지
    engine.stopStreaming();

} catch (const libetude::Exception& e) {
    std::cerr << "스트리밍 오류: " << e.what() << std::endl;
}
```

#### 람다 캡처를 사용한 고급 콜백

```cpp
class AudioPlayer {
public:
    void playChunk(const std::vector<float>& audio) {
        // 오디오 재생 로직
    }
};

AudioPlayer player;
libetude::Engine engine("model.lef");

// 멤버 함수를 콜백으로 사용
auto callback = [&player](const std::vector<float>& audio) {
    player.playChunk(audio);
};

engine.startStreaming(callback);
```

### 성능 제어 및 모니터링

#### QualityMode 열거형

```cpp
enum class QualityMode {
    Fast = 0,      // 빠른 처리 (낮은 품질)
    Balanced = 1,  // 균형 모드
    High = 2       // 고품질 (느린 처리)
};
```

#### setQualityMode

```cpp
void setQualityMode(QualityMode mode);
```

**사용 예제**:
```cpp
libetude::Engine engine("model.lef");

// 품질 모드 설정
engine.setQualityMode(libetude::QualityMode::High);

// 현재 품질 모드 확인
auto currentMode = engine.getQualityMode();
if (currentMode == libetude::QualityMode::High) {
    std::cout << "고품질 모드 활성화됨" << std::endl;
}
```

#### enableGPUAcceleration

```cpp
void enableGPUAcceleration(bool enable = true);
```

**사용 예제**:
```cpp
libetude::Engine engine("model.lef");

try {
    engine.enableGPUAcceleration();
    std::cout << "GPU 가속 활성화됨" << std::endl;

} catch (const libetude::HardwareException& e) {
    std::cout << "GPU 가속 불가능: " << e.what() << std::endl;
    // CPU 모드로 계속 진행
}

// GPU 가속 상태 확인
if (engine.isGPUAccelerationEnabled()) {
    std::cout << "GPU 가속 사용 중" << std::endl;
}
```

#### PerformanceStats 구조체

```cpp
struct PerformanceStats {
    double inference_time_ms;      // 추론 시간 (밀리초)
    double memory_usage_mb;        // 메모리 사용량 (MB)
    double cpu_usage_percent;      // CPU 사용률 (%)
    double gpu_usage_percent;      // GPU 사용률 (%)
    int active_threads;            // 활성 스레드 수
};
```

#### getPerformanceStats

```cpp
PerformanceStats getPerformanceStats();
```

**사용 예제**:
```cpp
libetude::Engine engine("model.lef");

try {
    auto stats = engine.getPerformanceStats();

    std::cout << "성능 통계:" << std::endl;
    std::cout << "  추론 시간: " << stats.inference_time_ms << " ms" << std::endl;
    std::cout << "  메모리 사용량: " << stats.memory_usage_mb << " MB" << std::endl;
    std::cout << "  CPU 사용률: " << stats.cpu_usage_percent << "%" << std::endl;
    std::cout << "  GPU 사용률: " << stats.gpu_usage_percent << "%" << std::endl;
    std::cout << "  활성 스레드: " << stats.active_threads << std::endl;

} catch (const libetude::Exception& e) {
    std::cerr << "통계 수집 실패: " << e.what() << std::endl;
}
```

### 확장 모델 관리

#### loadExtension

```cpp
int loadExtension(const std::string& extension_path);
```

**설명**: 확장 모델(.lefx)을 로드합니다.

**반환값**: 로드된 확장 모델 ID

**사용 예제**:
```cpp
libetude::Engine engine("base_model.lef");

try {
    int extensionId = engine.loadExtension("speaker_extension.lefx");
    std::cout << "확장 모델 로드됨 (ID: " << extensionId << ")" << std::endl;

    // 확장 모델 사용하여 합성
    auto audio = engine.synthesizeText("새로운 화자로 말합니다.");

    // 확장 모델 언로드
    engine.unloadExtension(extensionId);

} catch (const libetude::ModelException& e) {
    std::cerr << "확장 모델 로드 실패: " << e.what() << std::endl;
}
```

#### 여러 확장 모델 관리

```cpp
libetude::Engine engine("base_model.lef");

std::vector<int> extensionIds;

try {
    // 여러 확장 모델 로드
    extensionIds.push_back(engine.loadExtension("speaker1.lefx"));
    extensionIds.push_back(engine.loadExtension("speaker2.lefx"));
    extensionIds.push_back(engine.loadExtension("effect1.lefx"));

    // 로드된 확장 목록 확인
    auto loadedExtensions = engine.getLoadedExtensions();
    std::cout << "로드된 확장 수: " << loadedExtensions.size() << std::endl;

    // 모든 확장 언로드
    for (int id : extensionIds) {
        engine.unloadExtension(id);
    }

} catch (const libetude::Exception& e) {
    std::cerr << "확장 관리 오류: " << e.what() << std::endl;
}
```

### 정적 유틸리티 함수

#### getVersion

```cpp
static std::string getVersion();
```

**사용 예제**:
```cpp
std::string version = libetude::Engine::getVersion();
std::cout << "LibEtude 버전: " << version << std::endl;
```

#### getHardwareFeatures

```cpp
static uint32_t getHardwareFeatures();
```

**사용 예제**:
```cpp
uint32_t features = libetude::Engine::getHardwareFeatures();

if (features & LIBETUDE_SIMD_AVX2) {
    std::cout << "AVX2 지원" << std::endl;
}
if (features & LIBETUDE_SIMD_NEON) {
    std::cout << "NEON 지원" << std::endl;
}
```

## 편의 함수

### createEngine

```cpp
std::unique_ptr<Engine> createEngine(const std::string& model_path);
```

**사용 예제**:
```cpp
auto engine = libetude::createEngine("model.lef");
if (engine) {
    auto audio = engine->synthesizeText("텍스트");
}
```

### textToSpeech

```cpp
std::vector<float> textToSpeech(const std::string& model_path, const std::string& text);
```

**사용 예제**:
```cpp
// 간단한 일회성 합성
try {
    auto audio = libetude::textToSpeech("model.lef", "안녕하세요");
    saveAudioToFile(audio, "output.wav");

} catch (const libetude::Exception& e) {
    std::cerr << "합성 실패: " << e.what() << std::endl;
}
```

## 고급 사용 패턴

### RAII 패턴 활용

```cpp
class TTSService {
private:
    std::unique_ptr<libetude::Engine> engine_;

public:
    TTSService(const std::string& model_path)
        : engine_(std::make_unique<libetude::Engine>(model_path)) {

        // 초기 설정
        engine_->setQualityMode(libetude::QualityMode::Balanced);

        try {
            engine_->enableGPUAcceleration();
        } catch (const libetude::HardwareException&) {
            // GPU 가속 실패는 무시하고 CPU 모드로 계속
        }
    }

    std::vector<float> synthesize(const std::string& text) {
        return engine_->synthesizeText(text);
    }

    // 소멸자에서 자동으로 리소스 해제됨
};

// 사용
TTSService tts("model.lef");
auto audio = tts.synthesize("텍스트");
// tts가 스코프를 벗어나면 자동으로 엔진 해제
```

### 스레드 안전한 래퍼

```cpp
class ThreadSafeTTS {
private:
    std::unique_ptr<libetude::Engine> engine_;
    mutable std::mutex mutex_;

public:
    ThreadSafeTTS(const std::string& model_path)
        : engine_(std::make_unique<libetude::Engine>(model_path)) {}

    std::vector<float> synthesize(const std::string& text) {
        std::lock_guard<std::mutex> lock(mutex_);
        return engine_->synthesizeText(text);
    }

    void setQualityMode(libetude::QualityMode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        engine_->setQualityMode(mode);
    }
};

// 멀티스레드 환경에서 안전하게 사용
ThreadSafeTTS tts("model.lef");

std::vector<std::thread> threads;
for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&tts, i]() {
        auto audio = tts.synthesize("스레드 " + std::to_string(i));
        // 오디오 처리...
    });
}

for (auto& t : threads) {
    t.join();
}
```

### 비동기 배치 처리

```cpp
class AsyncTTSProcessor {
private:
    std::unique_ptr<libetude::Engine> engine_;

public:
    AsyncTTSProcessor(const std::string& model_path)
        : engine_(std::make_unique<libetude::Engine>(model_path)) {}

    std::vector<std::future<std::vector<float>>>
    processBatch(const std::vector<std::string>& texts) {

        std::vector<std::future<std::vector<float>>> futures;

        for (const auto& text : texts) {
            futures.push_back(engine_->synthesizeTextAsync(text));
        }

        return futures;
    }
};

// 사용
AsyncTTSProcessor processor("model.lef");

std::vector<std::string> texts = {
    "첫 번째 텍스트",
    "두 번째 텍스트",
    "세 번째 텍스트"
};

auto futures = processor.processBatch(texts);

// 모든 결과 수집
std::vector<std::vector<float>> results;
for (auto& future : futures) {
    try {
        results.push_back(future.get());
    } catch (const libetude::Exception& e) {
        std::cerr << "배치 처리 오류: " << e.what() << std::endl;
    }
}
```

### 성능 모니터링

```cpp
class PerformanceMonitor {
private:
    libetude::Engine& engine_;
    std::chrono::steady_clock::time_point start_time_;

public:
    PerformanceMonitor(libetude::Engine& engine)
        : engine_(engine), start_time_(std::chrono::steady_clock::now()) {}

    void printStats() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count();

        try {
            auto stats = engine_.getPerformanceStats();

            std::cout << "=== 성능 모니터링 리포트 ===" << std::endl;
            std::cout << "실행 시간: " << elapsed << " ms" << std::endl;
            std::cout << "추론 시간: " << stats.inference_time_ms << " ms" << std::endl;
            std::cout << "메모리 사용량: " << stats.memory_usage_mb << " MB" << std::endl;
            std::cout << "CPU 사용률: " << stats.cpu_usage_percent << "%" << std::endl;

        } catch (const libetude::Exception& e) {
            std::cerr << "성능 통계 수집 실패: " << e.what() << std::endl;
        }
    }
};

// 사용
libetude::Engine engine("model.lef");
PerformanceMonitor monitor(engine);

auto audio = engine.synthesizeText("성능 테스트 텍스트");
monitor.printStats();
```

## 컴파일 및 링크

### CMake 설정

```cmake
find_package(LibEtude REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app LibEtude::cpp)

# C++17 표준 필요
set_property(TARGET my_app PROPERTY CXX_STANDARD 17)
```

### 수동 컴파일

```bash
# 헤더 경로와 라이브러리 링크
g++ -std=c++17 -I/path/to/libetude/include \
    main.cpp -L/path/to/libetude/lib -letude -letude_cpp
```

## 모범 사례

### 1. 예외 안전성

```cpp
// 좋은 예: RAII와 예외 처리
void processAudio(const std::string& model_path, const std::string& text) {
    try {
        libetude::Engine engine(model_path);  // RAII로 자동 관리

        auto audio = engine.synthesizeText(text);
        saveAudioToFile(audio, "output.wav");

        // 예외 발생 시에도 engine은 자동으로 해제됨

    } catch (const libetude::Exception& e) {
        std::cerr << "LibEtude 오류: " << e.what() << std::endl;
        throw;  // 필요시 재던지기
    }
}
```

### 2. 성능 최적화

```cpp
// 좋은 예: 엔진 재사용
class OptimizedTTS {
private:
    libetude::Engine engine_;

public:
    OptimizedTTS(const std::string& model_path) : engine_(model_path) {
        // 초기화 시 최적화 설정
        engine_.setQualityMode(libetude::QualityMode::Balanced);

        try {
            engine_.enableGPUAcceleration();
        } catch (const libetude::HardwareException&) {
            // GPU 실패는 무시
        }
    }

    std::vector<float> synthesize(const std::string& text) {
        return engine_.synthesizeText(text);  // 엔진 재사용
    }
};
```

### 3. 리소스 관리

```cpp
// 좋은 예: 스마트 포인터 사용
class TTSManager {
private:
    std::vector<std::unique_ptr<libetude::Engine>> engines_;

public:
    void addEngine(const std::string& model_path) {
        engines_.push_back(std::make_unique<libetude::Engine>(model_path));
    }

    std::vector<float> synthesizeWithEngine(size_t index, const std::string& text) {
        if (index >= engines_.size()) {
            throw std::out_of_range("엔진 인덱스가 범위를 벗어남");
        }

        return engines_[index]->synthesizeText(text);
    }

    // 소멸자에서 모든 엔진이 자동으로 해제됨
};
```

이 문서는 LibEtude C++ 바인딩의 핵심 기능과 사용법을 다룹니다. 모던 C++ 기능을 활용하여 안전하고 효율적인 음성 합성 애플리케이션을 개발하시기 바랍니다.