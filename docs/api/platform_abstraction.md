# 플랫폼 추상화 레이어 API 문서

## 개요

LibEtude의 플랫폼 추상화 레이어는 Windows, Linux, macOS 등 다양한 플랫폼에서 일관된 API를 제공합니다. 이 문서는 각 인터페이스의 상세한 사용법과 플랫폼별 구현 차이점을 설명합니다.

## 목차

1. [오디오 I/O 인터페이스](#오디오-io-인터페이스)
2. [시스템 정보 인터페이스](#시스템-정보-인터페이스)
3. [스레딩 인터페이스](#스레딩-인터페이스)
4. [메모리 관리 인터페이스](#메모리-관리-인터페이스)
5. [파일 시스템 인터페이스](#파일-시스템-인터페이스)
6. [네트워크 인터페이스](#네트워크-인터페이스)
7. [동적 라이브러리 인터페이스](#동적-라이브러리-인터페이스)
8. [플랫폼별 구현 차이점](#플랫폼별-구현-차이점)
9. [성능 최적화 가이드](#성능-최적화-가이드)

## 오디오 I/O 인터페이스

### ETAudioInterface

오디오 입출력을 위한 플랫폼 독립적 인터페이스입니다.

#### 함수 목록

##### `open_output_device`

```c
ETResult (*open_output_device)(const char* device_name, const ETAudioFormat* format, ETAudioDevice** device);
```

**설명**: 오디오 출력 디바이스를 엽니다.

**매개변수**:
- `device_name`: 디바이스 이름 (NULL이면 기본 디바이스)
- `format`: 오디오 포맷 설정
- `device`: 생성된 디바이스 핸들 반환

**반환값**:
- `ET_SUCCESS`: 성공
- `ET_ERROR_DEVICE_NOT_FOUND`: 디바이스를 찾을 수 없음
- `ET_ERROR_FORMAT_NOT_SUPPORTED`: 지원하지 않는 포맷

**사용 예제**:
```c
ETAudioFormat format = {
    .sample_rate = 44100,
    .channels = 2,
    .bits_per_sample = 16
};

ETAudioDevice* device;
ETResult result = audio_interface->open_output_device(NULL, &format, &device);
if (result != ET_SUCCESS) {
    // 오류 처리
}
```

**플랫폼별 주의사항**:
- **Windows**: DirectSound 또는 WASAPI 사용
- **Linux**: ALSA PCM 디바이스 사용
- **macOS**: CoreAudio AudioUnit 사용

##### `set_callback`

```c
ETResult (*set_callback)(ETAudioDevice* device, ETAudioCallback callback, void* user_data);
```

**설명**: 오디오 콜백 함수를 설정합니다.

**매개변수**:
- `device`: 오디오 디바이스 핸들
- `callback`: 콜백 함수 포인터
- `user_data`: 사용자 데이터

**콜백 함수 시그니처**:
```c
typedef void (*ETAudioCallback)(void* output_buffer, int frame_count, void* user_data);
```

**사용 예제**:
```c
void audio_callback(void* output_buffer, int frame_count, void* user_data) {
    // 오디오 데이터 생성
    float* buffer = (float*)output_buffer;
    for (int i = 0; i < frame_count * 2; i++) {
        buffer[i] = generate_sample();
    }
}

ETResult result = audio_interface->set_callback(device, audio_callback, NULL);
```

## 시스템 정보 인터페이스

### ETSystemInterface

시스템 정보 조회를 위한 플랫폼 독립적 인터페이스입니다.

#### 함수 목록

##### `get_system_info`

```c
ETResult (*get_system_info)(ETSystemInfo* info);
```

**설명**: 시스템 기본 정보를 조회합니다.

**매개변수**:
- `info`: 시스템 정보를 저장할 구조체

**ETSystemInfo 구조체**:
```c
typedef struct {
    uint64_t total_memory;      // 총 메모리 (바이트)
    uint64_t available_memory;  // 사용 가능한 메모리
    uint32_t cpu_count;         // CPU 코어 수
    uint32_t cpu_frequency;     // CPU 주파수 (MHz)
    char cpu_name[128];         // CPU 이름
    char system_name[64];       // 시스템 이름
} ETSystemInfo;
```

**사용 예제**:
```c
ETSystemInfo info;
ETResult result = system_interface->get_system_info(&info);
if (result == ET_SUCCESS) {
    printf("CPU 코어 수: %u\n", info.cpu_count);
    printf("총 메모리: %llu MB\n", info.total_memory / (1024 * 1024));
}
```

##### `get_high_resolution_time`

```c
ETResult (*get_high_resolution_time)(uint64_t* time_ns);
```

**설명**: 고해상도 타이머 값을 나노초 단위로 반환합니다.

**매개변수**:
- `time_ns`: 시간 값을 저장할 포인터 (나노초)

**플랫폼별 구현**:
- **Windows**: QueryPerformanceCounter 사용
- **Linux**: clock_gettime(CLOCK_MONOTONIC) 사용
- **macOS**: mach_absolute_time 사용

**사용 예제**:
```c
uint64_t start_time, end_time;
system_interface->get_high_resolution_time(&start_time);

// 측정할 작업 수행
perform_operation();

system_interface->get_high_resolution_time(&end_time);
uint64_t elapsed_ns = end_time - start_time;
printf("소요 시간: %llu ns\n", elapsed_ns);
```

## 스레딩 인터페이스

### ETThreadInterface

스레드 관리 및 동기화를 위한 플랫폼 독립적 인터페이스입니다.

#### 함수 목록

##### `create_thread`

```c
ETResult (*create_thread)(ETThread** thread, ETThreadFunc func, void* arg);
```

**설명**: 새로운 스레드를 생성합니다.

**매개변수**:
- `thread`: 생성된 스레드 핸들 반환
- `func`: 스레드 함수
- `arg`: 스레드 함수에 전달할 인수

**스레드 함수 시그니처**:
```c
typedef void* (*ETThreadFunc)(void* arg);
```

**사용 예제**:
```c
void* worker_thread(void* arg) {
    int* data = (int*)arg;
    printf("스레드에서 받은 데이터: %d\n", *data);
    return NULL;
}

int data = 42;
ETThread* thread;
ETResult result = thread_interface->create_thread(&thread, worker_thread, &data);
```

##### `create_mutex`

```c
ETResult (*create_mutex)(ETMutex** mutex);
```

**설명**: 뮤텍스를 생성합니다.

**사용 예제**:
```c
ETMutex* mutex;
ETResult result = thread_interface->create_mutex(&mutex);

// 임계 영역 보호
thread_interface->lock_mutex(mutex);
// 공유 자원 접근
shared_resource++;
thread_interface->unlock_mutex(mutex);

// 정리
thread_interface->destroy_mutex(mutex);
```

## 메모리 관리 인터페이스

### ETMemoryInterface

메모리 할당 및 관리를 위한 플랫폼 독립적 인터페이스입니다.

#### 함수 목록

##### `aligned_malloc`

```c
void* (*aligned_malloc)(size_t size, size_t alignment);
```

**설명**: 정렬된 메모리를 할당합니다.

**매개변수**:
- `size`: 할당할 메모리 크기
- `alignment`: 정렬 바이트 수 (2의 거듭제곱)

**사용 예제**:
```c
// SIMD 연산을 위한 32바이트 정렬 메모리 할당
void* aligned_buffer = memory_interface->aligned_malloc(1024, 32);
if (aligned_buffer) {
    // SIMD 연산 수행
    memory_interface->aligned_free(aligned_buffer);
}
```

##### `create_shared_memory`

```c
ETResult (*create_shared_memory)(const char* name, size_t size, ETSharedMemory** shm);
```

**설명**: 공유 메모리를 생성합니다.

**사용 예제**:
```c
ETSharedMemory* shm;
ETResult result = memory_interface->create_shared_memory("my_shared_mem", 4096, &shm);
if (result == ET_SUCCESS) {
    void* ptr = memory_interface->map_shared_memory(shm);
    // 공유 메모리 사용
    memory_interface->unmap_shared_memory(shm, ptr);
    memory_interface->destroy_shared_memory(shm);
}
```

## 파일 시스템 인터페이스

### ETFilesystemInterface

파일 시스템 조작을 위한 플랫폼 독립적 인터페이스입니다.

#### 함수 목록

##### `normalize_path`

```c
ETResult (*normalize_path)(const char* path, char* normalized, size_t size);
```

**설명**: 경로를 정규화합니다 (플랫폼별 구분자 통일).

**사용 예제**:
```c
char normalized[256];
ETResult result = filesystem_interface->normalize_path("path\\to\\file", normalized, sizeof(normalized));
// Windows에서는 "path/to/file"로 변환
```

##### `open_file`

```c
ETResult (*open_file)(const char* path, ETFileMode mode, ETFile** file);
```

**설명**: 파일을 엽니다.

**파일 모드**:
```c
typedef enum {
    ET_FILE_MODE_READ = 1,
    ET_FILE_MODE_WRITE = 2,
    ET_FILE_MODE_APPEND = 4,
    ET_FILE_MODE_BINARY = 8
} ETFileMode;
```

**사용 예제**:
```c
ETFile* file;
ETResult result = filesystem_interface->open_file("data.bin",
    ET_FILE_MODE_READ | ET_FILE_MODE_BINARY, &file);
if (result == ET_SUCCESS) {
    char buffer[1024];
    size_t bytes_read;
    filesystem_interface->read_file(file, buffer, sizeof(buffer), &bytes_read);
    filesystem_interface->close_file(file);
}
```

## 네트워크 인터페이스

### ETNetworkInterface

네트워크 통신을 위한 플랫폼 독립적 인터페이스입니다.

#### 함수 목록

##### `create_socket`

```c
ETResult (*create_socket)(ETSocketType type, ETSocket** socket);
```

**설명**: 소켓을 생성합니다.

**소켓 타입**:
```c
typedef enum {
    ET_SOCKET_TCP,
    ET_SOCKET_UDP
} ETSocketType;
```

**사용 예제**:
```c
ETSocket* socket;
ETResult result = network_interface->create_socket(ET_SOCKET_TCP, &socket);
if (result == ET_SUCCESS) {
    // 소켓 사용
    network_interface->close_socket(socket);
}
```

## 동적 라이브러리 인터페이스

### ETDynlibInterface

동적 라이브러리 로딩을 위한 플랫폼 독립적 인터페이스입니다.

#### 함수 목록

##### `load_library`

```c
ETResult (*load_library)(const char* path, ETDynamicLibrary** lib);
```

**설명**: 동적 라이브러리를 로드합니다.

**사용 예제**:
```c
ETDynamicLibrary* lib;
ETResult result = dynlib_interface->load_library("plugin.dll", &lib);
if (result == ET_SUCCESS) {
    void* symbol;
    result = dynlib_interface->get_symbol(lib, "plugin_init", &symbol);
    if (result == ET_SUCCESS) {
        // 심볼 사용
        typedef int (*plugin_init_func)(void);
        plugin_init_func init = (plugin_init_func)symbol;
        init();
    }
    dynlib_interface->unload_library(lib);
}
```

## 플랫폼별 구현 차이점

### Windows 플랫폼

#### 특징
- DirectSound/WASAPI 기반 오디오
- Win32 API 사용
- UTF-16 문자열 처리 필요
- 백슬래시 경로 구분자

#### 주의사항
- COM 초기화 필요 (오디오)
- 스레드 모델 고려
- DLL 로딩 시 경로 해석

### Linux 플랫폼

#### 특징
- ALSA 기반 오디오
- POSIX API 사용
- UTF-8 문자열
- 슬래시 경로 구분자

#### 주의사항
- 권한 관리
- 시그널 처리
- 공유 라이브러리 의존성

### macOS 플랫폼

#### 특징
- CoreAudio 기반 오디오
- BSD/Darwin API 사용
- UTF-8 문자열
- 슬래시 경로 구분자

#### 주의사항
- 샌드박스 제한
- 앱 번들 구조
- Metal 성능 셰이더 활용

## 성능 최적화 가이드

### 컴파일 타임 최적화

#### 매크로 기반 선택
```c
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_AUDIO_IMPL windows_audio_impl
#elif defined(LIBETUDE_PLATFORM_LINUX)
    #define ET_AUDIO_IMPL linux_audio_impl
#endif
```

#### 인라인 함수 활용
```c
LIBETUDE_INLINE ETResult et_audio_start_stream(ETAudioDevice* device) {
    return ET_AUDIO_IMPL.start_stream(device);
}
```

### 런타임 최적화

#### 기능 캐싱
```c
static uint32_t cached_features = 0;
static bool features_initialized = false;

uint32_t get_hardware_features(void) {
    if (!features_initialized) {
        cached_features = detect_hardware_features();
        features_initialized = true;
    }
    return cached_features;
}
```

#### 메모리 풀 사용
```c
// 자주 할당되는 객체에 대해 메모리 풀 사용
ETMemoryPool* audio_device_pool = create_memory_pool(sizeof(ETAudioDevice), 16);
ETAudioDevice* device = (ETAudioDevice*)pool_alloc(audio_device_pool);
```

### 베스트 프랙티스

1. **초기화 순서**: 플랫폼 → 시스템 → 오디오 → 애플리케이션
2. **오류 처리**: 모든 API 호출에 대해 반환값 확인
3. **리소스 정리**: RAII 패턴 또는 명시적 정리 함수 호출
4. **스레드 안전성**: 공유 자원 접근 시 동기화 사용
5. **성능 측정**: 프로파일링을 통한 병목 지점 식별

### 일반적인 실수와 해결책

#### 실수 1: 플랫폼별 경로 구분자 혼용
```c
// 잘못된 예
char path[] = "data\\models\\voice.lef";  // Windows 전용

// 올바른 예
char normalized_path[256];
filesystem_interface->normalize_path("data/models/voice.lef", normalized_path, sizeof(normalized_path));
```

#### 실수 2: 메모리 정렬 무시
```c
// 잘못된 예
float* buffer = malloc(1024);  // 정렬 보장 안됨

// 올바른 예
float* buffer = memory_interface->aligned_malloc(1024, 32);  // 32바이트 정렬
```

#### 실수 3: 오류 코드 무시
```c
// 잘못된 예
audio_interface->open_output_device(NULL, &format, &device);

// 올바른 예
ETResult result = audio_interface->open_output_device(NULL, &format, &device);
if (result != ET_SUCCESS) {
    handle_error(result);
    return;
}
```

## 마이그레이션 가이드

### 기존 코드에서 플랫폼 추상화 레이어로 이전

#### 1단계: 인터페이스 획득
```c
// 기존 코드
#ifdef _WIN32
    init_windows_audio();
#elif __linux__
    init_alsa_audio();
#endif

// 새로운 코드
ETAudioInterface* audio = et_platform_get_audio_interface();
```

#### 2단계: 함수 호출 변경
```c
// 기존 코드
#ifdef _WIN32
    windows_open_audio_device(&device);
#elif __linux__
    alsa_open_pcm_device(&device);
#endif

// 새로운 코드
ETAudioDevice* device;
audio->open_output_device(NULL, &format, &device);
```

#### 3단계: 오류 처리 통합
```c
// 기존 코드
#ifdef _WIN32
    if (hr != S_OK) handle_windows_error(hr);
#elif __linux__
    if (err < 0) handle_alsa_error(err);
#endif

// 새로운 코드
ETResult result = audio->open_output_device(NULL, &format, &device);
if (result != ET_SUCCESS) {
    handle_common_error(result);
}
```

### 호환성 정보

#### API 버전 관리
```c
// 인터페이스 버전 확인
uint32_t version = et_platform_get_interface_version();
if (version < ET_PLATFORM_API_VERSION_1_0) {
    // 구버전 처리
}
```

#### 기능 지원 확인
```c
// 특정 기능 지원 여부 확인
if (system_interface->has_feature(ET_FEATURE_HIGH_RES_TIMER)) {
    // 고해상도 타이머 사용
} else {
    // 대체 방법 사용
}
```

이 문서는 LibEtude 플랫폼 추상화 레이어의 완전한 API 참조서입니다. 각 인터페이스의 상세한 사용법과 플랫폼별 고려사항을 포함하여 개발자가 효율적으로 크로스 플랫폼 애플리케이션을 개발할 수 있도록 돕습니다.