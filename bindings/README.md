# LibEtude 모바일 바인딩

LibEtude의 모바일 플랫폼 바인딩은 Android와 iOS에서 고성능 음성 합성을 제공합니다.

## 지원 플랫폼

- **Android**: JNI를 통한 네이티브 바인딩
- **iOS/macOS**: Objective-C/Swift 바인딩

## 주요 기능

- 실시간 텍스트-음성 변환
- 노래 합성 (가사 + 음표)
- 실시간 오디오 스트리밍
- 모바일 리소스 최적화
- 배터리 효율성 관리
- 메모리 압박 상황 처리
- 열 제한 관리

## Android 사용법

### 1. 빌드 설정

```bash
# Android NDK 빌드
cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DLIBETUDE_BUILD_JNI_BINDINGS=ON

cmake --build build-android
```

### 2. Java 코드 예제

```java
import com.libetude.Engine;
import com.libetude.AudioStreamCallback;

public class TTSExample {
    private Engine engine;

    public void initialize() {
        try {
            // 엔진 초기화
            engine = new Engine("/path/to/model.lef");

            // 모바일 최적화 적용
            engine.applyMobileOptimizations(true, true, 2);

        } catch (RuntimeException e) {
            Log.e("TTS", "엔진 초기화 실패", e);
        }
    }

    public void synthesizeText() {
        try {
            float[] audioData = engine.synthesizeText("안녕하세요");
            // 오디오 데이터 재생
            playAudio(audioData);

        } catch (RuntimeException e) {
            Log.e("TTS", "음성 합성 실패", e);
        }
    }

    public void startStreaming() {
        AudioStreamCallback callback = new AudioStreamCallback() {
            @Override
            public void onAudioData(float[] audioData) {
                // 실시간 오디오 데이터 처리
                playAudioChunk(audioData);
            }
        };

        try {
            engine.startStreaming(callback);
            engine.streamText("실시간 음성 합성 테스트");

        } catch (RuntimeException e) {
            Log.e("TTS", "스트리밍 실패", e);
        }
    }

    public void cleanup() {
        if (engine != null) {
            engine.destroy();
        }
    }
}
```

### 3. 메모리 관리

```java
// 메모리 압박 상황 처리
public class MemoryManager {
    public void onTrimMemory(int level) {
        switch (level) {
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL:
                // 심각한 메모리 부족
                engine.setQualityMode(Engine.QUALITY_FAST);
                break;
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW:
                // 메모리 부족
                engine.setQualityMode(Engine.QUALITY_BALANCED);
                break;
        }
    }
}
```

## iOS 사용법

### 1. 빌드 설정

```bash
# iOS 빌드
cmake -B build-ios \
    -DCMAKE_TOOLCHAIN_FILE=ios.toolchain.cmake \
    -DPLATFORM=OS64 \
    -DLIBETUDE_BUILD_OBJC_BINDINGS=ON

cmake --build build-ios
```

### 2. Objective-C 코드 예제

```objc
#import "LibEtudeEngine.h"

@interface TTSViewController : UIViewController <LibEtudeAudioStreamDelegate>
@property (nonatomic, strong) LibEtudeEngine *engine;
@end

@implementation TTSViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    // 엔진 초기화
    NSError *error;
    self.engine = [[LibEtudeEngine alloc] initWithModelPath:@"/path/to/model.lef" error:&error];
    if (!self.engine) {
        NSLog(@"엔진 초기화 실패: %@", error.localizedDescription);
        return;
    }

    // 모바일 최적화 적용
    [self.engine applyMobileOptimizationsWithLowMemory:YES
                                       batteryOptimized:YES
                                             maxThreads:2
                                                  error:&error];
}

- (void)synthesizeText {
    NSError *error;
    NSData *audioData = [self.engine synthesizeText:@"안녕하세요" error:&error];
    if (audioData) {
        [self playAudio:audioData];
    } else {
        NSLog(@"음성 합성 실패: %@", error.localizedDescription);
    }
}

- (void)startStreaming {
    NSError *error;
    if ([self.engine startStreamingWithDelegate:self error:&error]) {
        [self.engine streamText:@"실시간 음성 합성 테스트" error:nil];
    } else {
        NSLog(@"스트리밍 시작 실패: %@", error.localizedDescription);
    }
}

// LibEtudeAudioStreamDelegate 구현
- (void)audioStreamDidReceiveData:(NSData *)audioData {
    // 실시간 오디오 데이터 처리
    [self playAudioChunk:audioData];
}

- (void)audioStreamDidStart {
    NSLog(@"스트리밍 시작됨");
}

- (void)audioStreamDidStop {
    NSLog(@"스트리밍 중지됨");
}

@end
```

### 3. Swift 코드 예제

```swift
import Foundation

class TTSManager: LibEtudeAudioStreamSwiftDelegate {
    private var engine: LibEtudeSwift?
    private var delegateBridge: LibEtudeAudioStreamDelegateBridge?

    func initialize() {
        do {
            engine = try LibEtudeSwift(modelPath: "/path/to/model.lef")

            // 모바일 최적화 적용
            try engine?.applyMobileOptimizations(lowMemory: true,
                                               batteryOptimized: true,
                                               maxThreads: 2)
        } catch {
            print("엔진 초기화 실패: \\(error)")
        }
    }

    func synthesizeText() {
        do {
            let audioData = try engine?.synthesizeText("안녕하세요")
            playAudio(audioData)
        } catch {
            print("음성 합성 실패: \\(error)")
        }
    }

    func startStreaming() {
        do {
            delegateBridge = LibEtudeAudioStreamDelegateBridge(swiftDelegate: self)
            try engine?.startStreaming(delegate: delegateBridge!)
            try engine?.streamText("실시간 음성 합성 테스트")
        } catch {
            print("스트리밍 실패: \\(error)")
        }
    }

    // LibEtudeAudioStreamSwiftDelegate 구현
    func audioStreamDidReceiveData(_ audioData: Data) {
        playAudioChunk(audioData)
    }

    func audioStreamDidStart() {
        print("스트리밍 시작됨")
    }

    func audioStreamDidStop() {
        print("스트리밍 중지됨")
    }

    func audioStreamDidEncounterError(_ error: Error) {
        print("스트리밍 오류: \\(error)")
    }

    func audioStreamBufferLevelChanged(_ bufferLevel: Float) {
        print("버퍼 레벨: \\(bufferLevel)")
    }
}
```

## 모바일 최적화

### 메모리 관리

```c
// 메모리 압박 상황 모니터링
void memory_pressure_callback(const MobileResourceStatus* status, void* user_data) {
    LibEtudeEngine* engine = (LibEtudeEngine*)user_data;

    if (status->memory_pressure > 0.8f) {
        mobile_handle_memory_pressure(engine, status->memory_pressure);
    }
}

// 모니터링 시작
mobile_start_resource_monitoring(memory_pressure_callback, engine, 1000);  // 1초 간격
```

### 배터리 최적화

```c
// 배터리 상태에 따른 최적화
void optimize_for_battery(LibEtudeEngine* engine, float battery_level, bool is_charging) {
    mobile_optimize_for_battery(engine, battery_level, is_charging, false);
}
```

### 열 관리

```c
// CPU 온도 모니터링
void thermal_callback(const MobileResourceStatus* status, void* user_data) {
    LibEtudeEngine* engine = (LibEtudeEngine*)user_data;

    if (status->thermal_throttling_active) {
        mobile_handle_thermal_throttling(engine, status->cpu_temperature);
    }
}
```

## 성능 최적화 팁

### Android

1. **ProGuard/R8 설정**: 불필요한 코드 제거
2. **NDK 최적화**: ARM NEON 활용
3. **메모리 관리**: `onTrimMemory()` 콜백 구현
4. **스레드 관리**: 백그라운드 처리 최적화

### iOS

1. **ARC 최적화**: 메모리 자동 관리 활용
2. **Metal 가속**: GPU 연산 활용
3. **백그라운드 모드**: 적절한 백그라운드 처리
4. **메모리 경고**: `didReceiveMemoryWarning` 처리

## 문제 해결

### 일반적인 문제

1. **메모리 부족**: 품질 모드를 FAST로 변경
2. **배터리 소모**: 배터리 최적화 모드 활성화
3. **열 문제**: 열 제한 모드 활성화
4. **성능 저하**: 디바이스 클래스에 맞는 설정 사용

### 디버깅

```c
// 최적화 통계 확인
char* stats = mobile_get_optimization_stats();
printf("%s", stats);
free(stats);
```

## 라이선스

LibEtude 모바일 바인딩은 LibEtude 프로젝트와 동일한 라이선스를 따릅니다.