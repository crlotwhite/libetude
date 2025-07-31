# LibEtude 모바일 플랫폼 가이드

## 개요

LibEtude는 iOS와 Android 플랫폼에서 네이티브 성능을 제공하는 모바일 바인딩을 지원합니다. 각 플랫폼의 특성에 맞춘 최적화와 배터리 효율성을 고려한 설계를 제공합니다.

### 지원 플랫폼

- **iOS**: iOS 12.0 이상, Objective-C/Swift 지원
- **Android**: API 레벨 21 (Android 5.0) 이상, Java/Kotlin 지원

## iOS 플랫폼 (Objective-C/Swift)

### 프로젝트 설정

#### CocoaPods 사용

```ruby
# Podfile
platform :ios, '12.0'

target 'YourApp' do
  use_frameworks!
  pod 'LibEtude', '~> 1.0'
end
```

#### 수동 설치

1. LibEtude.framework를 프로젝트에 추가
2. Build Settings에서 Framework Search Paths 설정
3. Other Linker Flags에 `-ObjC` 추가

### Objective-C 사용법

#### 기본 설정

```objc
#import <LibEtude/LibEtudeEngine.h>

@interface ViewController ()
@property (nonatomic, strong) LibEtudeEngine *engine;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    // 네이티브 라이브러리 초기화
    if (![LibEtudeEngine initializeNativeLibrary]) {
        NSLog(@"네이티브 라이브러리 초기화 실패");
        return;
    }

    // 모델 파일 경로 설정
    NSString *modelPath = [[NSBundle mainBundle] pathForResource:@"model" ofType:@"lef"];

    // 엔진 생성
    NSError *error;
    self.engine = [[LibEtudeEngine alloc] initWithModelPath:modelPath error:&error];

    if (!self.engine) {
        NSLog(@"엔진 생성 실패: %@", error.localizedDescription);
        return;
    }

    // 모바일 최적화 적용
    [self.engine applyMobileOptimizationsWithLowMemory:YES
                                      batteryOptimized:YES
                                            maxThreads:2
                                                 error:&error];

    NSLog(@"LibEtude 엔진 초기화 완료");
}

- (void)dealloc {
    [LibEtudeEngine cleanupNativeLibrary];
}

@end
```

#### 텍스트 음성 합성

```objc
- (void)synthesizeText:(NSString *)text {
    if (!self.engine.isInitialized) {
        NSLog(@"엔진이 초기화되지 않았습니다");
        return;
    }

    NSError *error;
    NSData *audioData = [self.engine synthesizeText:text error:&error];

    if (audioData) {
        NSLog(@"음성 합성 성공: %lu 바이트", (unsigned long)audioData.length);

        // 오디오 재생
        [self playAudioData:audioData];

    } else {
        NSLog(@"음성 합성 실패: %@", error.localizedDescription);
    }
}

- (void)playAudioData:(NSData *)audioData {
    // AVAudioPlayer를 사용한 오디오 재생
    NSError *error;
    AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithData:audioData error:&error];

    if (player) {
        [player play];
    } else {
        NSLog(@"오디오 재생 실패: %@", error.localizedDescription);
    }
}
```

#### 실시간 스트리밍

```objc
// 스트림 델리게이트 프로토콜 구현
@interface ViewController () <LibEtudeAudioStreamDelegate>
@property (nonatomic, strong) AVAudioEngine *audioEngine;
@property (nonatomic, strong) AVAudioPlayerNode *playerNode;
@end

@implementation ViewController

- (void)startStreaming {
    // 오디오 엔진 설정
    self.audioEngine = [[AVAudioEngine alloc] init];
    self.playerNode = [[AVAudioPlayerNode alloc] init];

    [self.audioEngine attachNode:self.playerNode];
    [self.audioEngine connect:self.playerNode
                           to:self.audioEngine.mainMixerNode
                       format:nil];

    NSError *error;
    if (![self.audioEngine startAndReturnError:&error]) {
        NSLog(@"오디오 엔진 시작 실패: %@", error.localizedDescription);
        return;
    }

    // 스트리밍 시작
    if ([self.engine startStreamingWithDelegate:self error:&error]) {
        NSLog(@"스트리밍 시작됨");

        // 텍스트 스트리밍
        [self.engine streamText:@"첫 번째 문장입니다." error:nil];
        [self.engine streamText:@"두 번째 문장입니다." error:nil];

    } else {
        NSLog(@"스트리밍 시작 실패: %@", error.localizedDescription);
    }
}

// LibEtudeAudioStreamDelegate 메서드
- (void)audioStreamDidReceiveData:(NSData *)audioData {
    // 메인 스레드에서 UI 업데이트
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"오디오 청크 수신: %lu 바이트", (unsigned long)audioData.length);
    });

    // 오디오 재생 (백그라운드 스레드에서)
    [self playStreamingAudio:audioData];
}

- (void)audioStreamDidFinish {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"스트리밍 완료");
    });
}

- (void)audioStreamDidFailWithError:(NSError *)error {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"스트리밍 오류: %@", error.localizedDescription);
    });
}

@end
```

### Swift 사용법

#### 기본 설정

```swift
import LibEtude

class ViewController: UIViewController {
    private var engine: LibEtudeEngine?

    override func viewDidLoad() {
        super.viewDidLoad()
        setupEngine()
    }

    private func setupEngine() {
        // 네이티브 라이브러리 초기화
        guard LibEtudeEngine.initializeNativeLibrary() else {
            print("네이티브 라이브러리 초기화 실패")
            return
        }

        // 모델 파일 경로
        guard let modelPath = Bundle.main.path(forResource: "model", ofType: "lef") else {
            print("모델 파일을 찾을 수 없습니다")
            return
        }

        // 엔진 생성
        do {
            engine = try LibEtudeEngine(modelPath: modelPath)

            // 모바일 최적화 적용
            try engine?.applyMobileOptimizations(lowMemory: true,
                                               batteryOptimized: true,
                                               maxThreads: 2)

            print("LibEtude 엔진 초기화 완료")

        } catch {
            print("엔진 생성 실패: \(error.localizedDescription)")
        }
    }

    deinit {
        LibEtudeEngine.cleanupNativeLibrary()
    }
}
```

#### 비동기 음성 합성

```swift
extension ViewController {

    func synthesizeTextAsync(_ text: String) {
        guard let engine = engine, engine.isInitialized else {
            print("엔진이 초기화되지 않았습니다")
            return
        }

        // 백그라운드 큐에서 합성 수행
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let audioData = try engine.synthesizeText(text)

                // 메인 스레드에서 UI 업데이트
                DispatchQueue.main.async {
                    print("음성 합성 성공: \(audioData.count) 바이트")
                    self.playAudioData(audioData)
                }

            } catch {
                DispatchQueue.main.async {
                    print("음성 합성 실패: \(error.localizedDescription)")
                }
            }
        }
    }

    private func playAudioData(_ audioData: Data) {
        do {
            let player = try AVAudioPlayer(data: audioData)
            player.play()
        } catch {
            print("오디오 재생 실패: \(error.localizedDescription)")
        }
    }
}
```

#### 스트리밍 델리게이트 (Swift)

```swift
extension ViewController: LibEtudeAudioStreamDelegate {

    func startStreaming() {
        guard let engine = engine else { return }

        do {
            try engine.startStreaming(with: self)
            print("스트리밍 시작됨")

            // 텍스트 스트리밍
            try engine.streamText("안녕하세요, Swift에서 스트리밍입니다.")

        } catch {
            print("스트리밍 시작 실패: \(error.localizedDescription)")
        }
    }

    func audioStreamDidReceiveData(_ audioData: Data) {
        DispatchQueue.main.async {
            print("오디오 청크 수신: \(audioData.count) 바이트")
        }

        // 오디오 재생
        playStreamingAudio(audioData)
    }

    func audioStreamDidFinish() {
        DispatchQueue.main.async {
            print("스트리밍 완료")
        }
    }

    func audioStreamDidFail(with error: Error) {
        DispatchQueue.main.async {
            print("스트리밍 오류: \(error.localizedDescription)")
        }
    }
}
```

### iOS 최적화 팁

#### 백그라운드 처리

```swift
class BackgroundTTSManager {
    private var engine: LibEtudeEngine?
    private var backgroundTask: UIBackgroundTaskIdentifier = .invalid

    func synthesizeInBackground(_ text: String, completion: @escaping (Data?) -> Void) {
        // 백그라운드 작업 시작
        backgroundTask = UIApplication.shared.beginBackgroundTask { [weak self] in
            self?.endBackgroundTask()
        }

        DispatchQueue.global(qos: .background).async { [weak self] in
            defer { self?.endBackgroundTask() }

            do {
                let audioData = try self?.engine?.synthesizeText(text)

                DispatchQueue.main.async {
                    completion(audioData)
                }

            } catch {
                DispatchQueue.main.async {
                    completion(nil)
                }
            }
        }
    }

    private func endBackgroundTask() {
        if backgroundTask != .invalid {
            UIApplication.shared.endBackgroundTask(backgroundTask)
            backgroundTask = .invalid
        }
    }
}
```

#### 메모리 관리

```swift
class MemoryEfficientTTS {
    private var engine: LibEtudeEngine?

    func synthesizeWithMemoryManagement(_ text: String) -> Data? {
        autoreleasepool {
            do {
                let audioData = try engine?.synthesizeText(text)

                // 메모리 사용량 체크
                if let memoryStats = LibEtudeEngine.memoryStats(),
                   memoryStats[0].intValue > 100_000_000 { // 100MB 초과
                    print("메모리 사용량이 높습니다: \(memoryStats[0]) 바이트")

                    // 품질 모드를 낮춰서 메모리 절약
                    try engine?.setQualityMode(.fast)
                }

                return audioData

            } catch {
                print("합성 실패: \(error.localizedDescription)")
                return nil
            }
        }
    }
}
```

## Android 플랫폼 (Java/Kotlin)

### 프로젝트 설정

#### Gradle 설정

```gradle
// app/build.gradle
android {
    compileSdkVersion 33

    defaultConfig {
        minSdkVersion 21
        targetSdkVersion 33

        ndk {
            abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'
        }
    }

    packagingOptions {
        pickFirst '**/libc++_shared.so'
        pickFirst '**/libetude.so'
    }
}

dependencies {
    implementation 'com.libetude:android:1.0.0'
}
```

#### 권한 설정

```xml
<!-- AndroidManifest.xml -->
<uses-permission android:name="android.permission.RECORD_AUDIO" />
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
```

### Java 사용법

#### 기본 설정

```java
import com.libetude.Engine;
import com.libetude.AudioStreamCallback;
import com.libetude.PerformanceStats;

public class MainActivity extends AppCompatActivity {
    private Engine engine;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 네이티브 라이브러리 초기화
        if (!Engine.initializeNativeLibrary()) {
            Log.e("LibEtude", "네이티브 라이브러리 초기화 실패");
            return;
        }

        setupEngine();
    }

    private void setupEngine() {
        try {
            // 모델 파일을 assets에서 내부 저장소로 복사
            String modelPath = copyAssetToInternalStorage("model.lef");

            // 엔진 생성
            engine = new Engine(modelPath);

            // 모바일 최적화 적용
            engine.applyMobileOptimizations(
                true,  // lowMemory
                true,  // batteryOptimized
                2      // maxThreads
            );

            Log.i("LibEtude", "엔진 초기화 완료");

        } catch (Exception e) {
            Log.e("LibEtude", "엔진 생성 실패", e);
        }
    }

    private String copyAssetToInternalStorage(String assetName) throws IOException {
        File outputFile = new File(getFilesDir(), assetName);

        try (InputStream inputStream = getAssets().open(assetName);
             FileOutputStream outputStream = new FileOutputStream(outputFile)) {

            byte[] buffer = new byte[1024];
            int length;
            while ((length = inputStream.read(buffer)) > 0) {
                outputStream.write(buffer, 0, length);
            }
        }

        return outputFile.getAbsolutePath();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (engine != null) {
            engine.destroy();
        }
        Engine.cleanupNativeLibrary();
    }
}
```

#### 텍스트 음성 합성

```java
public class TTSHelper {
    private Engine engine;
    private MediaPlayer mediaPlayer;

    public void synthesizeText(String text, TTSCallback callback) {
        if (engine == null || !engine.isInitialized()) {
            callback.onError("엔진이 초기화되지 않았습니다");
            return;
        }

        // 백그라운드 스레드에서 합성 수행
        new AsyncTask<String, Void, float[]>() {
            @Override
            protected float[] doInBackground(String... texts) {
                try {
                    return engine.synthesizeText(texts[0]);
                } catch (Exception e) {
                    Log.e("LibEtude", "합성 실패", e);
                    return null;
                }
            }

            @Override
            protected void onPostExecute(float[] audioData) {
                if (audioData != null) {
                    callback.onSuccess(audioData);
                    playAudioData(audioData);
                } else {
                    callback.onError("음성 합성 실패");
                }
            }
        }.execute(text);
    }

    private void playAudioData(float[] audioData) {
        // AudioTrack을 사용한 오디오 재생
        int sampleRate = 22050;
        int channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        int audioFormat = AudioFormat.ENCODING_PCM_FLOAT;

        int bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig, audioFormat);

        AudioTrack audioTrack = new AudioTrack(
            AudioManager.STREAM_MUSIC,
            sampleRate,
            channelConfig,
            audioFormat,
            bufferSize,
            AudioTrack.MODE_STREAM
        );

        audioTrack.play();
        audioTrack.write(audioData, 0, audioData.length, AudioTrack.WRITE_BLOCKING);
        audioTrack.stop();
        audioTrack.release();
    }

    public interface TTSCallback {
        void onSuccess(float[] audioData);
        void onError(String error);
    }
}
```

#### 실시간 스트리밍

```java
public class StreamingTTSManager {
    private Engine engine;
    private AudioTrack audioTrack;
    private boolean isStreaming = false;

    public void startStreaming() {
        if (engine == null || !engine.isInitialized()) {
            Log.e("LibEtude", "엔진이 초기화되지 않았습니다");
            return;
        }

        // AudioTrack 설정
        setupAudioTrack();

        // 스트리밍 콜백 설정
        AudioStreamCallback callback = new AudioStreamCallback() {
            @Override
            public void onAudioData(float[] audioData) {
                if (audioTrack != null && isStreaming) {
                    audioTrack.write(audioData, 0, audioData.length, AudioTrack.WRITE_NON_BLOCKING);
                }

                // UI 업데이트 (메인 스레드에서)
                new Handler(Looper.getMainLooper()).post(() -> {
                    Log.d("LibEtude", "오디오 청크 수신: " + audioData.length + " 샘플");
                });
            }

            @Override
            public void onStreamFinished() {
                new Handler(Looper.getMainLooper()).post(() -> {
                    Log.i("LibEtude", "스트리밍 완료");
                    stopStreaming();
                });
            }

            @Override
            public void onError(String error) {
                new Handler(Looper.getMainLooper()).post(() -> {
                    Log.e("LibEtude", "스트리밍 오류: " + error);
                    stopStreaming();
                });
            }
        };

        // 스트리밍 시작
        if (engine.startStreaming(callback)) {
            isStreaming = true;
            audioTrack.play();

            // 텍스트 스트리밍
            engine.streamText("안녕하세요, Android에서 스트리밍입니다.");

        } else {
            Log.e("LibEtude", "스트리밍 시작 실패");
        }
    }

    private void setupAudioTrack() {
        int sampleRate = 22050;
        int channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        int audioFormat = AudioFormat.ENCODING_PCM_FLOAT;

        int bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig, audioFormat) * 2;

        audioTrack = new AudioTrack(
            AudioManager.STREAM_MUSIC,
            sampleRate,
            channelConfig,
            audioFormat,
            bufferSize,
            AudioTrack.MODE_STREAM
        );
    }

    public void stopStreaming() {
        isStreaming = false;

        if (engine != null) {
            engine.stopStreaming();
        }

        if (audioTrack != null) {
            audioTrack.stop();
            audioTrack.release();
            audioTrack = null;
        }
    }
}
```

### Kotlin 사용법

#### 기본 설정 (Kotlin)

```kotlin
import com.libetude.Engine
import com.libetude.AudioStreamCallback
import kotlinx.coroutines.*

class MainActivity : AppCompatActivity() {
    private var engine: Engine? = null
    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // 네이티브 라이브러리 초기화
        if (!Engine.initializeNativeLibrary()) {
            Log.e("LibEtude", "네이티브 라이브러리 초기화 실패")
            return
        }

        setupEngine()
    }

    private fun setupEngine() {
        scope.launch {
            try {
                // 모델 파일 복사 (IO 스레드에서)
                val modelPath = withContext(Dispatchers.IO) {
                    copyAssetToInternalStorage("model.lef")
                }

                // 엔진 생성
                engine = Engine(modelPath).apply {
                    // 모바일 최적화 적용
                    applyMobileOptimizations(
                        lowMemory = true,
                        batteryOptimized = true,
                        maxThreads = 2
                    )
                }

                Log.i("LibEtude", "엔진 초기화 완료")

            } catch (e: Exception) {
                Log.e("LibEtude", "엔진 생성 실패", e)
            }
        }
    }

    private suspend fun copyAssetToInternalStorage(assetName: String): String =
        withContext(Dispatchers.IO) {
            val outputFile = File(filesDir, assetName)

            assets.open(assetName).use { inputStream ->
                outputFile.outputStream().use { outputStream ->
                    inputStream.copyTo(outputStream)
                }
            }

            outputFile.absolutePath
        }

    override fun onDestroy() {
        super.onDestroy()
        engine?.destroy()
        Engine.cleanupNativeLibrary()
        scope.cancel()
    }
}
```

#### 코루틴을 사용한 비동기 합성

```kotlin
class TTSManager(private val engine: Engine) {

    suspend fun synthesizeTextAsync(text: String): FloatArray? =
        withContext(Dispatchers.Default) {
            try {
                engine.synthesizeText(text)
            } catch (e: Exception) {
                Log.e("LibEtude", "합성 실패", e)
                null
            }
        }

    fun synthesizeAndPlay(text: String) {
        CoroutineScope(Dispatchers.Main).launch {
            val audioData = synthesizeTextAsync(text)

            if (audioData != null) {
                Log.i("LibEtude", "음성 합성 성공: ${audioData.size} 샘플")
                playAudioData(audioData)
            } else {
                Log.e("LibEtude", "음성 합성 실패")
            }
        }
    }

    private suspend fun playAudioData(audioData: FloatArray) =
        withContext(Dispatchers.Default) {
            val sampleRate = 22050
            val channelConfig = AudioFormat.CHANNEL_OUT_MONO
            val audioFormat = AudioFormat.ENCODING_PCM_FLOAT

            val bufferSize = AudioTrack.getMinBufferSize(sampleRate, channelConfig, audioFormat)

            val audioTrack = AudioTrack(
                AudioManager.STREAM_MUSIC,
                sampleRate,
                channelConfig,
                audioFormat,
                bufferSize,
                AudioTrack.MODE_STREAM
            )

            audioTrack.play()
            audioTrack.write(audioData, 0, audioData.size, AudioTrack.WRITE_BLOCKING)
            audioTrack.stop()
            audioTrack.release()
        }
}
```

#### Flow를 사용한 스트리밍

```kotlin
import kotlinx.coroutines.flow.*

class StreamingTTSManager(private val engine: Engine) {

    fun startStreamingFlow(): Flow<FloatArray> = flow {
        val callback = object : AudioStreamCallback {
            override fun onAudioData(audioData: FloatArray) {
                // Flow로 데이터 방출
                trySend(audioData)
            }

            override fun onStreamFinished() {
                // Flow 완료
                close()
            }

            override fun onError(error: String) {
                // Flow 오류
                close(Exception(error))
            }
        }

        if (engine.startStreaming(callback)) {
            // 텍스트 스트리밍
            engine.streamText("Kotlin Flow를 사용한 스트리밍입니다.")
        } else {
            throw Exception("스트리밍 시작 실패")
        }

        // Flow가 완료될 때까지 대기
        awaitClose {
            engine.stopStreaming()
        }
    }.flowOn(Dispatchers.Default)

    fun collectStreamingAudio() {
        CoroutineScope(Dispatchers.Main).launch {
            startStreamingFlow()
                .catch { e ->
                    Log.e("LibEtude", "스트리밍 오류", e)
                }
                .collect { audioData ->
                    Log.d("LibEtude", "오디오 청크 수신: ${audioData.size} 샘플")
                    // 오디오 재생 로직
                }
        }
    }
}
```

### Android 최적화 팁

#### 배터리 최적화

```kotlin
class BatteryOptimizedTTS(private val context: Context) {
    private var engine: Engine? = null
    private val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager

    fun setupEngineForBatteryMode() {
        val isLowPowerMode = powerManager.isPowerSaveMode

        engine?.apply {
            if (isLowPowerMode) {
                // 저전력 모드에서는 최소 품질 사용
                setQualityMode(Engine.QUALITY_FAST)
                applyMobileOptimizations(
                    lowMemory = true,
                    batteryOptimized = true,
                    maxThreads = 1  // 스레드 수 최소화
                )
                Log.i("LibEtude", "저전력 모드 최적화 적용")
            } else {
                // 일반 모드에서는 균형 품질 사용
                setQualityMode(Engine.QUALITY_BALANCED)
                applyMobileOptimizations(
                    lowMemory = false,
                    batteryOptimized = true,
                    maxThreads = 2
                )
            }
        }
    }

    // 배터리 상태 변경 감지
    private val batteryReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                Intent.ACTION_BATTERY_LOW -> {
                    Log.i("LibEtude", "배터리 부족 - 절전 모드 활성화")
                    setupEngineForBatteryMode()
                }
                Intent.ACTION_POWER_SAVE_MODE_CHANGED -> {
                    Log.i("LibEtude", "전원 절약 모드 변경")
                    setupEngineForBatteryMode()
                }
            }
        }
    }

    fun registerBatteryReceiver() {
        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_BATTERY_LOW)
            addAction(Intent.ACTION_POWER_SAVE_MODE_CHANGED)
        }
        context.registerReceiver(batteryReceiver, filter)
    }

    fun unregisterBatteryReceiver() {
        context.unregisterReceiver(batteryReceiver)
    }
}
```

#### 메모리 관리

```kotlin
class MemoryEfficientTTS {
    private var engine: Engine? = null

    fun synthesizeWithMemoryCheck(text: String): FloatArray? {
        // 메모리 사용량 체크
        val memoryStats = Engine.getMemoryStats()
        val currentMemory = memoryStats[0] // 현재 사용량
        val peakMemory = memoryStats[1]    // 피크 사용량

        Log.d("LibEtude", "메모리 사용량: $currentMemory 바이트 (피크: $peakMemory)")

        // 메모리 사용량이 100MB를 초과하면 경고
        if (currentMemory > 100_000_000) {
            Log.w("LibEtude", "메모리 사용량이 높습니다")

            // 품질 모드를 낮춰서 메모리 절약
            engine?.setQualityMode(Engine.QUALITY_FAST)

            // 가비지 컬렉션 수행
            System.gc()
        }

        return try {
            engine?.synthesizeText(text)
        } catch (e: Exception) {
            Log.e("LibEtude", "합성 실패", e)
            null
        }
    }

    // 앱이 백그라운드로 갈 때 리소스 해제
    fun onAppBackground() {
        Log.i("LibEtude", "앱이 백그라운드로 이동 - 리소스 최적화")

        engine?.apply {
            // 스트리밍 중지
            if (isStreaming()) {
                stopStreaming()
            }

            // 최소 품질 모드로 변경
            setQualityMode(Engine.QUALITY_FAST)
        }
    }

    // 앱이 포그라운드로 올 때 리소스 복원
    fun onAppForeground() {
        Log.i("LibEtude", "앱이 포그라운드로 이동 - 리소스 복원")

        engine?.apply {
            // 균형 품질 모드로 복원
            setQualityMode(Engine.QUALITY_BALANCED)
        }
    }
}
```

## 플랫폼별 모범 사례

### iOS 모범 사례

1. **메모리 관리**: ARC와 함께 autoreleasepool 사용
2. **백그라운드 처리**: Background App Refresh 고려
3. **오디오 세션**: AVAudioSession 적절한 설정
4. **앱 생명주기**: 앱 상태 변화에 따른 리소스 관리

### Android 모범 사례

1. **생명주기 관리**: Activity/Fragment 생명주기에 맞춘 리소스 관리
2. **스레드 관리**: 메인 스레드 블로킹 방지
3. **권한 처리**: 런타임 권한 요청 및 처리
4. **배터리 최적화**: Doze 모드 및 App Standby 고려

## 성능 최적화 가이드

### 공통 최적화

1. **모델 크기 최적화**: 압축된 모델 사용
2. **메모리 풀링**: 메모리 재사용으로 할당 최소화
3. **배치 처리**: 여러 텍스트를 한 번에 처리
4. **캐싱**: 자주 사용되는 결과 캐싱

### 플랫폼별 최적화

#### iOS
- Metal Performance Shaders 활용
- Core ML 통합 고려
- 백그라운드 처리 최적화

#### Android
- NNAPI 활용 (Android 8.1+)
- Vulkan API 지원 고려
- 프로파일 가이드 최적화 (PGO)

이 가이드를 통해 iOS와 Android 플랫폼에서 LibEtude를 효과적으로 활용하여 고품질 음성 합성 애플리케이션을 개발하시기 바랍니다.