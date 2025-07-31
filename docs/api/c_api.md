# LibEtude C API 문서

## 개요

LibEtude는 음성 합성에 특화된 AI 추론 엔진으로, 고성능 실시간 처리와 크로스 플랫폼 지원을 제공합니다. 이 문서는 LibEtude의 핵심 C API에 대한 상세한 설명과 사용 예제를 제공합니다.

### 주요 특징

- **도메인 특화 최적화**: 음성 합성에 필요한 연산에 집중하여 불필요한 오버헤드 제거
- **실시간 처리**: 100ms 이내의 저지연 음성 합성
- **크로스 플랫폼**: Windows, macOS, Linux, Android, iOS 지원
- **하드웨어 최적화**: SIMD, GPU 가속 지원
- **확장 가능**: 플러그인 시스템을 통한 기능 확장

## 헤더 파일 포함

```c
#include <libetude/api.h>
#include <libetude/types.h>
#include <libetude/error.h>
```

## 기본 사용법

### 1. 엔진 생성 및 초기화

```c
#include <libetude/api.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    // 엔진 생성
    LibEtudeEngine* engine = libetude_create_engine("model.lef");
    if (!engine) {
        fprintf(stderr, "엔진 생성 실패: %s\n", libetude_get_last_error());
        return -1;
    }

    // 사용 후 정리
    libetude_destroy_engine(engine);
    return 0;
}
```

### 2. 기본 텍스트 음성 합성

```c
#include <libetude/api.h>
#include <stdio.h>
#include <stdlib.h>

int synthesize_text_example() {
    LibEtudeEngine* engine = libetude_create_engine("model.lef");
    if (!engine) {
        return -1;
    }

    const char* text = "안녕하세요, LibEtude입니다.";
    float audio_buffer[44100 * 5]; // 5초 분량 버퍼
    int audio_length = sizeof(audio_buffer) / sizeof(float);

    int result = libetude_synthesize_text(engine, text, audio_buffer, &audio_length);
    if (result == LIBETUDE_SUCCESS) {
        printf("음성 합성 성공: %d 샘플 생성\n", audio_length);

        // 오디오 데이터를 파일로 저장하거나 재생
        // save_audio_to_file(audio_buffer, audio_length);
    } else {
        fprintf(stderr, "음성 합성 실패: %s\n", libetude_get_last_error());
    }

    libetude_destroy_engine(engine);
    return result;
}
```

### 3. 실시간 스트리밍

```c
#include <libetude/api.h>
#include <stdio.h>

// 오디오 스트림 콜백 함수
void audio_stream_callback(const float* audio, int length, void* user_data) {
    printf("오디오 청크 수신: %d 샘플\n", length);

    // 실제 구현에서는 오디오를 재생하거나 파일에 저장
    // play_audio(audio, length);
}

int streaming_example() {
    LibEtudeEngine* engine = libetude_create_engine("model.lef");
    if (!engine) {
        return -1;
    }

    // 스트리밍 시작
    int result = libetude_start_streaming(engine, audio_stream_callback, NULL);
    if (result != LIBETUDE_SUCCESS) {
        fprintf(stderr, "스트리밍 시작 실패: %s\n", libetude_get_last_error());
        libetude_destroy_engine(engine);
        return result;
    }

    // 텍스트 스트리밍
    libetude_stream_text(engine, "첫 번째 문장입니다.");
    libetude_stream_text(engine, "두 번째 문장입니다.");
    libetude_stream_text(engine, "마지막 문장입니다.");

    // 잠시 대기 (실제로는 다른 작업 수행)
    // sleep(5);

    // 스트리밍 중지
    libetude_stop_streaming(engine);
    libetude_destroy_engine(engine);

    return LIBETUDE_SUCCESS;
}
```

## API 참조

### 엔진 관리

#### libetude_create_engine

```c
LibEtudeEngine* libetude_create_engine(const char* model_path);
```

**설명**: LibEtude 엔진을 생성하고 모델을 로드합니다.

**매개변수**:
- `model_path`: 모델 파일 경로 (.lef 또는 .lefx 파일)

**반환값**:
- 성공 시: 엔진 핸들 포인터
- 실패 시: NULL (오류 정보는 `libetude_get_last_error()`로 확인)

**사용 예제**:
```c
LibEtudeEngine* engine = libetude_create_engine("/path/to/model.lef");
if (!engine) {
    fprintf(stderr, "모델 로드 실패: %s\n", libetude_get_last_error());
}
```

#### libetude_destroy_engine

```c
void libetude_destroy_engine(LibEtudeEngine* engine);
```

**설명**: LibEtude 엔진을 해제하고 관련 리소스를 정리합니다.

**매개변수**:
- `engine`: 해제할 엔진 핸들

**주의사항**:
- NULL 포인터를 전달해도 안전합니다
- 엔진 해제 후에는 해당 핸들을 사용하면 안 됩니다

### 음성 합성 (동기 처리)

#### libetude_synthesize_text

```c
int libetude_synthesize_text(LibEtudeEngine* engine,
                            const char* text,
                            float* output_audio,
                            int* output_length);
```

**설명**: 텍스트를 음성으로 합성합니다 (동기 처리).

**매개변수**:
- `engine`: 엔진 핸들
- `text`: 합성할 텍스트 (UTF-8 인코딩)
- `output_audio`: 출력 오디오 버퍼 (호출자가 할당)
- `output_length`: 입력 시 버퍼 크기, 출력 시 실제 오디오 길이

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드 (자세한 내용은 오류 코드 섹션 참조)

**사용 예제**:
```c
const char* text = "안녕하세요";
float audio_buffer[44100 * 10]; // 10초 분량
int buffer_size = sizeof(audio_buffer) / sizeof(float);

int result = libetude_synthesize_text(engine, text, audio_buffer, &buffer_size);
if (result == LIBETUDE_SUCCESS) {
    printf("생성된 오디오 길이: %d 샘플\n", buffer_size);
}
```

#### libetude_synthesize_singing

```c
int libetude_synthesize_singing(LibEtudeEngine* engine,
                               const char* lyrics,
                               const float* notes,
                               int note_count,
                               float* output_audio,
                               int* output_length);
```

**설명**: 가사와 음표를 노래로 합성합니다.

**매개변수**:
- `engine`: 엔진 핸들
- `lyrics`: 가사 텍스트 (UTF-8 인코딩)
- `notes`: 음표 배열 (MIDI 노트 번호, 0-127)
- `note_count`: 음표 개수
- `output_audio`: 출력 오디오 버퍼
- `output_length`: 입력 시 버퍼 크기, 출력 시 실제 오디오 길이

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드

**사용 예제**:
```c
const char* lyrics = "도레미파솔라시도";
float notes[] = {60, 62, 64, 65, 67, 69, 71, 72}; // C4-C5
int note_count = 8;
float audio_buffer[44100 * 10];
int buffer_size = sizeof(audio_buffer) / sizeof(float);

int result = libetude_synthesize_singing(engine, lyrics, notes, note_count,
                                        audio_buffer, &buffer_size);
```

### 실시간 스트리밍 (비동기 처리)

#### AudioStreamCallback

```c
typedef void (*AudioStreamCallback)(const float* audio, int length, void* user_data);
```

**설명**: 스트리밍 중 생성된 오디오 데이터를 받는 콜백 함수 타입입니다.

**매개변수**:
- `audio`: 생성된 오디오 데이터 (float 배열)
- `length`: 오디오 샘플 수
- `user_data`: 사용자 정의 데이터

**구현 예제**:
```c
void my_audio_callback(const float* audio, int length, void* user_data) {
    // 오디오 재생 또는 파일 저장
    AudioPlayer* player = (AudioPlayer*)user_data;
    audio_player_write(player, audio, length);
}
```

#### libetude_start_streaming

```c
int libetude_start_streaming(LibEtudeEngine* engine,
                            AudioStreamCallback callback,
                            void* user_data);
```

**설명**: 실시간 스트리밍을 시작합니다.

**매개변수**:
- `engine`: 엔진 핸들
- `callback`: 오디오 데이터 콜백 함수
- `user_data`: 콜백에 전달할 사용자 데이터

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드

#### libetude_stream_text

```c
int libetude_stream_text(LibEtudeEngine* engine, const char* text);
```

**설명**: 스트리밍 중에 텍스트를 추가합니다.

**매개변수**:
- `engine`: 엔진 핸들
- `text`: 합성할 텍스트

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드

#### libetude_stop_streaming

```c
int libetude_stop_streaming(LibEtudeEngine* engine);
```

**설명**: 실시간 스트리밍을 중지합니다.

**매개변수**:
- `engine`: 엔진 핸들

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드

### 성능 제어 및 모니터링

#### QualityMode

```c
typedef enum {
    LIBETUDE_QUALITY_FAST = 0,     // 빠른 처리 (낮은 품질)
    LIBETUDE_QUALITY_BALANCED = 1, // 균형 모드
    LIBETUDE_QUALITY_HIGH = 2      // 고품질 (느린 처리)
} QualityMode;
```

#### libetude_set_quality_mode

```c
int libetude_set_quality_mode(LibEtudeEngine* engine, QualityMode quality_mode);
```

**설명**: 품질 모드를 설정합니다.

**매개변수**:
- `engine`: 엔진 핸들
- `quality_mode`: 품질 모드

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드

#### libetude_enable_gpu_acceleration

```c
int libetude_enable_gpu_acceleration(LibEtudeEngine* engine);
```

**설명**: GPU 가속을 활성화합니다.

**매개변수**:
- `engine`: 엔진 핸들

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- `LIBETUDE_ERROR_HARDWARE`: GPU를 사용할 수 없음
- 기타: 오류 코드

#### PerformanceStats

```c
typedef struct {
    double inference_time_ms;      // 추론 시간 (밀리초)
    double memory_usage_mb;        // 메모리 사용량 (MB)
    double cpu_usage_percent;      // CPU 사용률 (%)
    double gpu_usage_percent;      // GPU 사용률 (%)
    int active_threads;            // 활성 스레드 수
} PerformanceStats;
```

#### libetude_get_performance_stats

```c
int libetude_get_performance_stats(LibEtudeEngine* engine, PerformanceStats* stats);
```

**설명**: 성능 통계를 가져옵니다.

**매개변수**:
- `engine`: 엔진 핸들
- `stats`: 성능 통계를 저장할 구조체

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드

**사용 예제**:
```c
PerformanceStats stats;
int result = libetude_get_performance_stats(engine, &stats);
if (result == LIBETUDE_SUCCESS) {
    printf("추론 시간: %.2f ms\n", stats.inference_time_ms);
    printf("메모리 사용량: %.2f MB\n", stats.memory_usage_mb);
    printf("CPU 사용률: %.1f%%\n", stats.cpu_usage_percent);
}
```

### 확장 모델 관리

#### libetude_load_extension

```c
int libetude_load_extension(LibEtudeEngine* engine, const char* extension_path);
```

**설명**: 확장 모델(.lefx)을 로드합니다.

**매개변수**:
- `engine`: 엔진 핸들
- `extension_path`: 확장 모델 파일 경로

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- `LIBETUDE_ERROR_MODEL`: 모델 호환성 오류
- 기타: 오류 코드

#### libetude_unload_extension

```c
int libetude_unload_extension(LibEtudeEngine* engine, int extension_id);
```

**설명**: 확장 모델을 언로드합니다.

**매개변수**:
- `engine`: 엔진 핸들
- `extension_id`: 확장 모델 ID

**반환값**:
- `LIBETUDE_SUCCESS`: 성공
- 기타: 오류 코드

### 유틸리티 함수

#### libetude_get_version

```c
const char* libetude_get_version();
```

**설명**: LibEtude 버전 문자열을 반환합니다.

**반환값**: 버전 문자열 (예: "1.0.0")

#### libetude_get_hardware_features

```c
uint32_t libetude_get_hardware_features();
```

**설명**: 지원되는 하드웨어 기능을 반환합니다.

**반환값**: 하드웨어 기능 플래그 (`LibEtudeSIMDFeatures`의 조합)

**사용 예제**:
```c
uint32_t features = libetude_get_hardware_features();
if (features & LIBETUDE_SIMD_AVX2) {
    printf("AVX2 지원\n");
}
if (features & LIBETUDE_SIMD_NEON) {
    printf("NEON 지원\n");
}
```

#### libetude_get_last_error

```c
const char* libetude_get_last_error();
```

**설명**: 마지막 오류 메시지를 반환합니다.

**반환값**: 오류 메시지 문자열

### 로그 관련 함수

#### LibEtudeLogLevel

```c
typedef enum {
    LIBETUDE_LOG_DEBUG = 0,    // 디버그
    LIBETUDE_LOG_INFO = 1,     // 정보
    LIBETUDE_LOG_WARNING = 2,  // 경고
    LIBETUDE_LOG_ERROR = 3,    // 오류
    LIBETUDE_LOG_FATAL = 4     // 치명적 오류
} LibEtudeLogLevel;
```

#### LibEtudeLogCallback

```c
typedef void (*LibEtudeLogCallback)(LibEtudeLogLevel level, const char* message, void* user_data);
```

#### libetude_set_log_level

```c
void libetude_set_log_level(LibEtudeLogLevel level);
```

**설명**: 로그 레벨을 설정합니다.

#### libetude_set_log_callback

```c
void libetude_set_log_callback(LibEtudeLogCallback callback, void* user_data);
```

**설명**: 로그 콜백을 설정합니다.

**사용 예제**:
```c
void my_log_callback(LibEtudeLogLevel level, const char* message, void* user_data) {
    const char* level_str;
    switch (level) {
        case LIBETUDE_LOG_DEBUG: level_str = "DEBUG"; break;
        case LIBETUDE_LOG_INFO: level_str = "INFO"; break;
        case LIBETUDE_LOG_WARNING: level_str = "WARNING"; break;
        case LIBETUDE_LOG_ERROR: level_str = "ERROR"; break;
        case LIBETUDE_LOG_FATAL: level_str = "FATAL"; break;
    }
    printf("[%s] %s\n", level_str, message);
}

// 로그 콜백 설정
libetude_set_log_callback(my_log_callback, NULL);
libetude_set_log_level(LIBETUDE_LOG_INFO);
```

## 오류 코드

LibEtude는 다음과 같은 오류 코드를 사용합니다:

| 오류 코드 | 값 | 설명 |
|-----------|----|----- |
| `LIBETUDE_SUCCESS` | 0 | 성공 |
| `LIBETUDE_ERROR_INVALID_ARGUMENT` | -1 | 잘못된 인수 |
| `LIBETUDE_ERROR_OUT_OF_MEMORY` | -2 | 메모리 부족 |
| `LIBETUDE_ERROR_IO` | -3 | 입출력 오류 |
| `LIBETUDE_ERROR_NOT_IMPLEMENTED` | -4 | 구현되지 않음 |
| `LIBETUDE_ERROR_RUNTIME` | -5 | 런타임 오류 |
| `LIBETUDE_ERROR_HARDWARE` | -6 | 하드웨어 오류 |
| `LIBETUDE_ERROR_MODEL` | -7 | 모델 오류 |
| `LIBETUDE_ERROR_TIMEOUT` | -8 | 시간 초과 |
| `LIBETUDE_ERROR_NOT_INITIALIZED` | -9 | 초기화되지 않음 |
| `LIBETUDE_ERROR_ALREADY_INITIALIZED` | -10 | 이미 초기화됨 |
| `LIBETUDE_ERROR_UNSUPPORTED` | -11 | 지원되지 않음 |
| `LIBETUDE_ERROR_NOT_FOUND` | -12 | 찾을 수 없음 |
| `LIBETUDE_ERROR_INVALID_STATE` | -13 | 잘못된 상태 |
| `LIBETUDE_ERROR_BUFFER_FULL` | -14 | 버퍼 가득 참 |

### 오류 처리 모범 사례

```c
int safe_synthesize_text(LibEtudeEngine* engine, const char* text) {
    if (!engine) {
        fprintf(stderr, "엔진이 NULL입니다\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!text || strlen(text) == 0) {
        fprintf(stderr, "텍스트가 비어있습니다\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    float audio_buffer[44100 * 10];
    int buffer_size = sizeof(audio_buffer) / sizeof(float);

    int result = libetude_synthesize_text(engine, text, audio_buffer, &buffer_size);

    switch (result) {
        case LIBETUDE_SUCCESS:
            printf("음성 합성 성공\n");
            break;
        case LIBETUDE_ERROR_OUT_OF_MEMORY:
            fprintf(stderr, "메모리 부족으로 합성 실패\n");
            break;
        case LIBETUDE_ERROR_MODEL:
            fprintf(stderr, "모델 오류로 합성 실패: %s\n", libetude_get_last_error());
            break;
        default:
            fprintf(stderr, "알 수 없는 오류로 합성 실패: %s\n", libetude_get_last_error());
            break;
    }

    return result;
}
```

## 모범 사례

### 1. 리소스 관리

```c
// 좋은 예: RAII 패턴 사용
typedef struct {
    LibEtudeEngine* engine;
} TTSContext;

TTSContext* tts_context_create(const char* model_path) {
    TTSContext* ctx = malloc(sizeof(TTSContext));
    if (!ctx) return NULL;

    ctx->engine = libetude_create_engine(model_path);
    if (!ctx->engine) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void tts_context_destroy(TTSContext* ctx) {
    if (ctx) {
        if (ctx->engine) {
            libetude_destroy_engine(ctx->engine);
        }
        free(ctx);
    }
}
```

### 2. 스레드 안전성

```c
#include <pthread.h>

typedef struct {
    LibEtudeEngine* engine;
    pthread_mutex_t mutex;
} ThreadSafeTTS;

ThreadSafeTTS* thread_safe_tts_create(const char* model_path) {
    ThreadSafeTTS* tts = malloc(sizeof(ThreadSafeTTS));
    if (!tts) return NULL;

    tts->engine = libetude_create_engine(model_path);
    if (!tts->engine) {
        free(tts);
        return NULL;
    }

    if (pthread_mutex_init(&tts->mutex, NULL) != 0) {
        libetude_destroy_engine(tts->engine);
        free(tts);
        return NULL;
    }

    return tts;
}

int thread_safe_synthesize(ThreadSafeTTS* tts, const char* text,
                          float* output, int* length) {
    pthread_mutex_lock(&tts->mutex);
    int result = libetude_synthesize_text(tts->engine, text, output, length);
    pthread_mutex_unlock(&tts->mutex);
    return result;
}
```

### 3. 메모리 효율적인 스트리밍

```c
typedef struct {
    FILE* output_file;
    size_t total_samples;
} StreamingContext;

void efficient_stream_callback(const float* audio, int length, void* user_data) {
    StreamingContext* ctx = (StreamingContext*)user_data;

    // 오디오 데이터를 직접 파일에 쓰기 (메모리 절약)
    fwrite(audio, sizeof(float), length, ctx->output_file);
    ctx->total_samples += length;

    // 주기적으로 플러시
    if (ctx->total_samples % 4410 == 0) { // 0.1초마다
        fflush(ctx->output_file);
    }
}
```

### 4. 성능 최적화

```c
void optimize_for_performance(LibEtudeEngine* engine) {
    // GPU 가속 활성화 (가능한 경우)
    if (libetude_enable_gpu_acceleration(engine) == LIBETUDE_SUCCESS) {
        printf("GPU 가속 활성화됨\n");
    }

    // 하드웨어 기능 확인
    uint32_t features = libetude_get_hardware_features();
    if (features & LIBETUDE_SIMD_AVX2) {
        printf("AVX2 최적화 사용 가능\n");
        // 고품질 모드 사용 가능
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH);
    } else {
        // 저사양 하드웨어에서는 빠른 모드 사용
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
    }
}
```

### 5. 오류 복구

```c
int robust_text_synthesis(LibEtudeEngine* engine, const char* text,
                         float* output, int* length) {
    int max_retries = 3;
    int retry_count = 0;

    while (retry_count < max_retries) {
        int result = libetude_synthesize_text(engine, text, output, length);

        if (result == LIBETUDE_SUCCESS) {
            return result;
        }

        // 일시적 오류인 경우 재시도
        if (result == LIBETUDE_ERROR_TIMEOUT ||
            result == LIBETUDE_ERROR_RUNTIME) {
            retry_count++;
            printf("재시도 %d/%d...\n", retry_count, max_retries);

            // 잠시 대기 후 재시도
            usleep(100000); // 100ms
            continue;
        }

        // 복구 불가능한 오류
        return result;
    }

    return LIBETUDE_ERROR_TIMEOUT;
}
```

## 플랫폼별 고려사항

### Windows

```c
#ifdef _WIN32
#include <windows.h>

// Windows에서 UTF-8 텍스트 처리
int synthesize_korean_text_windows(LibEtudeEngine* engine, const char* korean_text) {
    // UTF-8 -> UTF-16 변환
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, korean_text, -1, NULL, 0);
    wchar_t* wide_text = malloc(wide_len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, korean_text, -1, wide_text, wide_len);

    // UTF-16 -> UTF-8 변환 (정규화)
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, NULL, 0, NULL, NULL);
    char* normalized_text = malloc(utf8_len);
    WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, normalized_text, utf8_len, NULL, NULL);

    float audio_buffer[44100 * 10];
    int buffer_size = sizeof(audio_buffer) / sizeof(float);

    int result = libetude_synthesize_text(engine, normalized_text, audio_buffer, &buffer_size);

    free(wide_text);
    free(normalized_text);

    return result;
}
#endif
```

### Android

```c
#ifdef __ANDROID__
#include <android/log.h>

// Android 로그 콜백
void android_log_callback(LibEtudeLogLevel level, const char* message, void* user_data) {
    android_LogPriority priority;
    switch (level) {
        case LIBETUDE_LOG_DEBUG: priority = ANDROID_LOG_DEBUG; break;
        case LIBETUDE_LOG_INFO: priority = ANDROID_LOG_INFO; break;
        case LIBETUDE_LOG_WARNING: priority = ANDROID_LOG_WARN; break;
        case LIBETUDE_LOG_ERROR: priority = ANDROID_LOG_ERROR; break;
        case LIBETUDE_LOG_FATAL: priority = ANDROID_LOG_FATAL; break;
    }
    __android_log_print(priority, "LibEtude", "%s", message);
}

void setup_android_logging() {
    libetude_set_log_callback(android_log_callback, NULL);
    libetude_set_log_level(LIBETUDE_LOG_INFO);
}
#endif
```

### iOS

```c
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE

// iOS에서 백그라운드 처리
void setup_ios_background_processing(LibEtudeEngine* engine) {
    // 백그라운드에서도 오디오 처리가 가능하도록 설정
    // 실제 구현은 Objective-C 코드에서 수행
}

#endif
#endif
```

## 성능 튜닝 가이드

### 1. 메모리 사용량 최적화

```c
// 메모리 사용량 모니터링
void monitor_memory_usage(LibEtudeEngine* engine) {
    PerformanceStats stats;
    if (libetude_get_performance_stats(engine, &stats) == LIBETUDE_SUCCESS) {
        if (stats.memory_usage_mb > 500.0) { // 500MB 초과 시
            printf("경고: 메모리 사용량이 높습니다 (%.2f MB)\n", stats.memory_usage_mb);

            // 품질 모드를 낮춰서 메모리 사용량 감소
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
        }
    }
}
```

### 2. 지연 시간 최적화

```c
// 저지연 설정
void configure_low_latency(LibEtudeEngine* engine) {
    // 빠른 품질 모드 사용
    libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);

    // GPU 가속 활성화 (가능한 경우)
    libetude_enable_gpu_acceleration(engine);

    // 스트리밍 모드에서 작은 청크 크기 사용
    // (실제 구현에서는 내부 버퍼 크기 조정)
}
```

이 문서는 LibEtude C API의 핵심 기능과 사용법을 다룹니다. 더 자세한 정보나 고급 기능에 대해서는 추가 문서를 참조하시기 바랍니다.