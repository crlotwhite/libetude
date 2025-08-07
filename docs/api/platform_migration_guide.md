# 플랫폼 추상화 레이어 마이그레이션 가이드

## 개요

이 문서는 기존 플랫폼별 코드를 LibEtude 플랫폼 추상화 레이어로 마이그레이션하는 방법을 설명합니다. 단계별 마이그레이션 과정과 호환성 정보를 제공합니다.

## 목차

1. [마이그레이션 전략](#마이그레이션-전략)
2. [단계별 마이그레이션](#단계별-마이그레이션)
3. [플랫폼별 마이그레이션](#플랫폼별-마이그레이션)
4. [호환성 매트릭스](#호환성-매트릭스)
5. [일반적인 마이그레이션 문제](#일반적인-마이그레이션-문제)
6. [성능 영향 분석](#성능-영향-분석)

## 마이그레이션 전략

### 1. 점진적 마이그레이션

기존 코드를 한 번에 모두 변경하는 대신 모듈별로 점진적으로 마이그레이션하는 것을 권장합니다.

#### 마이그레이션 순서
1. **초기화 코드**: 플랫폼 감지 및 인터페이스 획득
2. **시스템 정보**: 하드웨어 정보 조회 코드
3. **메모리 관리**: 메모리 할당/해제 코드
4. **파일 I/O**: 파일 시스템 접근 코드
5. **오디오 I/O**: 오디오 입출력 코드 (가장 복잡)
6. **스레딩**: 스레드 및 동기화 코드
7. **네트워크**: 네트워크 통신 코드

#### 호환성 레이어 사용
```c
// 마이그레이션 중 호환성을 위한 래퍼 함수
#ifdef LIBETUDE_USE_LEGACY_API
    #define et_malloc(size) malloc(size)
    #define et_free(ptr) free(ptr)
#else
    #define et_malloc(size) memory_interface->malloc(size)
    #define et_free(ptr) memory_interface->free(ptr)
#endif
```

### 2. 테스트 주도 마이그레이션

각 모듈 마이그레이션 후 철저한 테스트를 통해 기능 동등성을 확인합니다.

```c
// 마이그레이션 검증을 위한 테스트 프레임워크
typedef struct MigrationTest {
    const char* name;
    void (*legacy_impl)(void);
    void (*new_impl)(void);
    bool (*compare_results)(void);
} MigrationTest;

void run_migration_test(MigrationTest* test) {
    printf("마이그레이션 테스트: %s\n", test->name);

    // 기존 구현 실행
    test->legacy_impl();
    void* legacy_result = capture_result();

    // 새로운 구현 실행
    test->new_impl();
    void* new_result = capture_result();

    // 결과 비교
    if (test->compare_results()) {
        printf("✓ 테스트 통과\n");
    } else {
        printf("✗ 테스트 실패\n");
    }
}
```

## 단계별 마이그레이션

### 1단계: 플랫폼 초기화

#### 기존 코드
```c
// 기존 플랫폼별 초기화
#ifdef _WIN32
    #include <windows.h>
    void init_platform(void) {
        CoInitialize(NULL);
        timeBeginPeriod(1);
    }
#elif __linux__
    #include <alsa/asoundlib.h>
    void init_platform(void) {
        // ALSA 초기화
    }
#elif __APPLE__
    #include <CoreAudio/CoreAudio.h>
    void init_platform(void) {
        // CoreAudio 초기화
    }
#endif
```

#### 마이그레이션된 코드
```c
// 플랫폼 추상화 레이어 사용
#include <libetude/platform/factory.h>

void init_platform(void) {
    // 플랫폼 자동 감지 및 초기화
    ETResult result = et_platform_initialize();
    if (result != ET_SUCCESS) {
        handle_initialization_error(result);
        return;
    }

    // 인터페이스 획득
    audio_interface = et_platform_get_audio_interface();
    system_interface = et_platform_get_system_interface();
    memory_interface = et_platform_get_memory_interface();
    // ... 기타 인터페이스
}
```

### 2단계: 시스템 정보 조회

#### 기존 코드
```c
// Windows
#ifdef _WIN32
int get_cpu_count(void) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}

uint64_t get_total_memory(void) {
    MEMORYSTATUSEX meminfo;
    meminfo.dwLength = sizeof(meminfo);
    GlobalMemoryStatusEx(&meminfo);
    return meminfo.ullTotalPhys;
}
#endif

// Linux
#ifdef __linux__
int get_cpu_count(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

uint64_t get_total_memory(void) {
    struct sysinfo info;
    sysinfo(&info);
    return info.totalram * info.mem_unit;
}
#endif
```

#### 마이그레이션된 코드
```c
// 플랫폼 독립적 코드
int get_cpu_count(void) {
    ETSystemInfo info;
    ETResult result = system_interface->get_system_info(&info);
    if (result == ET_SUCCESS) {
        return info.cpu_count;
    }
    return -1; // 오류
}

uint64_t get_total_memory(void) {
    ETSystemInfo info;
    ETResult result = system_interface->get_system_info(&info);
    if (result == ET_SUCCESS) {
        return info.total_memory;
    }
    return 0; // 오류
}
```

### 3단계: 메모리 관리

#### 기존 코드
```c
// 플랫폼별 정렬된 메모리 할당
void* allocate_aligned_memory(size_t size, size_t alignment) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#elif defined(__linux__) || defined(__APPLE__)
    void* ptr;
    if (posix_memalign(&ptr, alignment, size) == 0) {
        return ptr;
    }
    return NULL;
#endif
}

void free_aligned_memory(void* ptr) {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
```

#### 마이그레이션된 코드
```c
// 플랫폼 독립적 메모리 관리
void* allocate_aligned_memory(size_t size, size_t alignment) {
    return memory_interface->aligned_malloc(size, alignment);
}

void free_aligned_memory(void* ptr) {
    memory_interface->aligned_free(ptr);
}
```

### 4단계: 파일 시스템 접근

#### 기존 코드
```c
// 플랫폼별 경로 처리
char* normalize_path(const char* path) {
    char* normalized = malloc(strlen(path) + 1);
    strcpy(normalized, path);

#ifdef _WIN32
    // 슬래시를 백슬래시로 변환
    for (char* p = normalized; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#else
    // 백슬래시를 슬래시로 변환
    for (char* p = normalized; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#endif

    return normalized;
}
```

#### 마이그레이션된 코드
```c
// 플랫폼 독립적 경로 처리
char* normalize_path(const char* path) {
    char* normalized = malloc(strlen(path) + 1);
    ETResult result = filesystem_interface->normalize_path(path, normalized, strlen(path) + 1);

    if (result == ET_SUCCESS) {
        return normalized;
    }

    free(normalized);
    return NULL;
}
```

### 5단계: 오디오 I/O (가장 복잡)

#### 기존 Windows 코드
```c
// Windows DirectSound 코드
typedef struct {
    LPDIRECTSOUND8 ds;
    LPDIRECTSOUNDBUFFER primary_buffer;
    LPDIRECTSOUNDBUFFER secondary_buffer;
} WindowsAudioContext;

int init_windows_audio(WindowsAudioContext* ctx, int sample_rate, int channels) {
    HRESULT hr = DirectSoundCreate8(NULL, &ctx->ds, NULL);
    if (FAILED(hr)) return -1;

    // 기본 버퍼 생성
    DSBUFFERDESC desc = {0};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    hr = ctx->ds->CreateSoundBuffer(&desc, &ctx->primary_buffer, NULL);
    if (FAILED(hr)) return -1;

    // 포맷 설정
    WAVEFORMATEX format = {0};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = sample_rate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = channels * 2;
    format.nAvgBytesPerSec = sample_rate * format.nBlockAlign;

    hr = ctx->primary_buffer->SetFormat(&format);
    return SUCCEEDED(hr) ? 0 : -1;
}
```

#### 마이그레이션된 코드
```c
// 플랫폼 독립적 오디오 코드
typedef struct {
    ETAudioDevice* device;
    ETAudioFormat format;
} AudioContext;

int init_audio(AudioContext* ctx, int sample_rate, int channels) {
    // 오디오 포맷 설정
    ctx->format.sample_rate = sample_rate;
    ctx->format.channels = channels;
    ctx->format.bits_per_sample = 16;
    ctx->format.format = ET_AUDIO_FORMAT_PCM;

    // 디바이스 열기
    ETResult result = audio_interface->open_output_device(
        NULL, &ctx->format, &ctx->device);

    return (result == ET_SUCCESS) ? 0 : -1;
}

void set_audio_callback(AudioContext* ctx, ETAudioCallback callback, void* user_data) {
    audio_interface->set_callback(ctx->device, callback, user_data);
}

void start_audio(AudioContext* ctx) {
    audio_interface->start_stream(ctx->device);
}

void stop_audio(AudioContext* ctx) {
    audio_interface->stop_stream(ctx->device);
    audio_interface->close_device(ctx->device);
}
```

### 6단계: 스레딩

#### 기존 코드
```c
// 플랫폼별 스레드 생성
#ifdef _WIN32
typedef struct {
    HANDLE handle;
    DWORD thread_id;
} PlatformThread;

int create_thread(PlatformThread* thread, void* (*func)(void*), void* arg) {
    thread->handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, &thread->thread_id);
    return thread->handle ? 0 : -1;
}
#else
typedef struct {
    pthread_t handle;
} PlatformThread;

int create_thread(PlatformThread* thread, void* (*func)(void*), void* arg) {
    return pthread_create(&thread->handle, NULL, func, arg);
}
#endif
```

#### 마이그레이션된 코드
```c
// 플랫폼 독립적 스레드
typedef struct {
    ETThread* handle;
} PlatformThread;

int create_thread(PlatformThread* thread, void* (*func)(void*), void* arg) {
    ETResult result = thread_interface->create_thread(&thread->handle, func, arg);
    return (result == ET_SUCCESS) ? 0 : -1;
}

int join_thread(PlatformThread* thread) {
    void* result;
    ETResult ret = thread_interface->join_thread(thread->handle, &result);
    return (ret == ET_SUCCESS) ? 0 : -1;
}
```

## 플랫폼별 마이그레이션

### Windows 마이그레이션

#### 주요 변경사항
- **DirectSound/WASAPI** → `ETAudioInterface`
- **Win32 API** → 플랫폼 추상화 함수
- **COM 초기화** → `et_platform_initialize()`

#### 마이그레이션 체크리스트
- [ ] COM 초기화 코드 제거
- [ ] DirectSound 코드를 ETAudioInterface로 변경
- [ ] Win32 파일 API를 ETFilesystemInterface로 변경
- [ ] Windows 스레드를 ETThreadInterface로 변경
- [ ] Winsock을 ETNetworkInterface로 변경

### Linux 마이그레이션

#### 주요 변경사항
- **ALSA** → `ETAudioInterface`
- **POSIX API** → 플랫폼 추상화 함수
- **pthread** → `ETThreadInterface`

#### 마이그레이션 체크리스트
- [ ] ALSA 초기화 코드 제거
- [ ] PCM 디바이스 코드를 ETAudioInterface로 변경
- [ ] POSIX 파일 API를 ETFilesystemInterface로 변경
- [ ] pthread를 ETThreadInterface로 변경
- [ ] Linux 소켓을 ETNetworkInterface로 변경

### macOS 마이그레이션

#### 주요 변경사항
- **CoreAudio** → `ETAudioInterface`
- **BSD API** → 플랫폼 추상화 함수
- **Grand Central Dispatch** → `ETThreadInterface`

#### 마이그레이션 체크리스트
- [ ] CoreAudio 초기화 코드 제거
- [ ] AudioUnit 코드를 ETAudioInterface로 변경
- [ ] BSD 파일 API를 ETFilesystemInterface로 변경
- [ ] GCD를 ETThreadInterface로 변경
- [ ] BSD 소켓을 ETNetworkInterface로 변경

## 호환성 매트릭스

### API 버전 호환성

| 기능 | v1.0 | v1.1 | v1.2 | 비고 |
|------|------|------|------|------|
| 기본 오디오 I/O | ✓ | ✓ | ✓ | 완전 호환 |
| 고급 오디오 기능 | - | ✓ | ✓ | v1.1부터 지원 |
| 네트워크 인터페이스 | - | - | ✓ | v1.2부터 지원 |
| GPU 가속 | - | - | ✓ | v1.2부터 지원 |

### 플랫폼 지원 매트릭스

| 플랫폼 | 오디오 | 시스템 | 스레딩 | 메모리 | 파일시스템 | 네트워크 |
|--------|--------|--------|--------|--------|------------|----------|
| Windows 10+ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Windows 7/8 | ✓ | ✓ | ✓ | ✓ | ✓ | ⚠️ |
| Ubuntu 18.04+ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| CentOS 7+ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| macOS 10.14+ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| macOS 10.12-10.13 | ✓ | ✓ | ✓ | ✓ | ✓ | ⚠️ |

범례: ✓ 완전 지원, ⚠️ 제한적 지원, ✗ 미지원

### 컴파일러 호환성

| 컴파일러 | Windows | Linux | macOS | 비고 |
|----------|---------|-------|-------|------|
| MSVC 2019+ | ✓ | - | - | 권장 |
| MSVC 2017 | ⚠️ | - | - | 제한적 지원 |
| GCC 7+ | ✓ | ✓ | ✓ | 완전 지원 |
| GCC 5-6 | ⚠️ | ⚠️ | ⚠️ | C11 기능 제한 |
| Clang 8+ | ✓ | ✓ | ✓ | 완전 지원 |
| Intel ICC | ✓ | ✓ | - | 성능 최적화 |

## 일반적인 마이그레이션 문제

### 1. 함수 시그니처 불일치

#### 문제
```c
// 기존 코드
int old_function(char* buffer, int size);

// 새로운 인터페이스
ETResult new_function(void* buffer, size_t size);
```

#### 해결책
```c
// 래퍼 함수 생성
int old_function_wrapper(char* buffer, int size) {
    ETResult result = new_function(buffer, (size_t)size);
    return (result == ET_SUCCESS) ? size : -1;
}
```

### 2. 오류 코드 매핑

#### 문제
```c
// 기존 플랫폼별 오류 코드
#ifdef _WIN32
    #define AUDIO_ERROR_DEVICE_NOT_FOUND DSERR_NODRIVER
#elif __linux__
    #define AUDIO_ERROR_DEVICE_NOT_FOUND -ENODEV
#endif
```

#### 해결책
```c
// 공통 오류 코드로 변환
int map_platform_error(ETResult result) {
    switch (result) {
        case ET_SUCCESS:
            return 0;
        case ET_ERROR_DEVICE_NOT_FOUND:
            return AUDIO_ERROR_DEVICE_NOT_FOUND;
        case ET_ERROR_INVALID_PARAMETER:
            return AUDIO_ERROR_INVALID_PARAM;
        default:
            return AUDIO_ERROR_UNKNOWN;
    }
}
```

### 3. 콜백 함수 시그니처 변경

#### 문제
```c
// 기존 Windows DirectSound 콜백
void CALLBACK old_audio_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
                                DWORD_PTR dwParam1, DWORD_PTR dwParam2);

// 새로운 플랫폼 독립적 콜백
void new_audio_callback(void* output_buffer, int frame_count, void* user_data);
```

#### 해결책
```c
// 어댑터 패턴 사용
typedef struct {
    void (*old_callback)(HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
    HWAVEOUT hwo;
    DWORD_PTR instance;
} CallbackAdapter;

void callback_adapter(void* output_buffer, int frame_count, void* user_data) {
    CallbackAdapter* adapter = (CallbackAdapter*)user_data;
    // 기존 콜백 형식으로 변환하여 호출
    adapter->old_callback(adapter->hwo, WOM_DONE, adapter->instance,
                         (DWORD_PTR)output_buffer, frame_count);
}
```

### 4. 스레드 모델 차이

#### 문제
```c
// Windows 스레드 우선순위
SetThreadPriority(thread_handle, THREAD_PRIORITY_TIME_CRITICAL);

// Linux 스레드 우선순위
struct sched_param param;
param.sched_priority = 99;
pthread_setschedparam(thread, SCHED_FIFO, &param);
```

#### 해결책
```c
// 플랫폼 독립적 우선순위 설정
ETResult set_high_priority(ETThread* thread) {
    return thread_interface->set_thread_priority(thread, ET_THREAD_PRIORITY_HIGH);
}
```

## 성능 영향 분석

### 1. 함수 호출 오버헤드

#### 측정 방법
```c
void benchmark_function_call_overhead(void) {
    const int iterations = 1000000;
    uint64_t start, end;

    // 직접 호출 측정
    system_interface->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        direct_function_call();
    }
    system_interface->get_high_resolution_time(&end);
    uint64_t direct_time = end - start;

    // 함수 포인터 호출 측정
    system_interface->get_high_resolution_time(&start);
    for (int i = 0; i < iterations; i++) {
        interface->function_call();
    }
    system_interface->get_high_resolution_time(&end);
    uint64_t indirect_time = end - start;

    printf("직접 호출: %llu ns\n", direct_time / iterations);
    printf("간접 호출: %llu ns\n", indirect_time / iterations);
    printf("오버헤드: %.2f%%\n",
           ((double)(indirect_time - direct_time) / direct_time) * 100);
}
```

#### 일반적인 오버헤드
- **함수 포인터 호출**: 1-3% 오버헤드
- **오류 코드 변환**: 0.5-1% 오버헤드
- **매개변수 변환**: 0.1-0.5% 오버헤드

### 2. 메모리 사용량 증가

#### 측정 방법
```c
void analyze_memory_overhead(void) {
    size_t before = get_memory_usage();

    // 플랫폼 추상화 레이어 초기화
    et_platform_initialize();

    size_t after = get_memory_usage();

    printf("메모리 오버헤드: %zu bytes\n", after - before);
}
```

#### 일반적인 메모리 오버헤드
- **인터페이스 구조체**: ~1KB per interface
- **플랫폼 데이터**: ~4-8KB per platform
- **캐시된 정보**: ~2-4KB

### 3. 최적화 권장사항

#### 핫 패스 최적화
```c
// 자주 호출되는 함수는 인라인 래퍼 사용
LIBETUDE_INLINE ETResult et_audio_write_fast(ETAudioDevice* device,
                                            const float* samples,
                                            int count) {
    // 컴파일 타임에 직접 구현체로 변환
    return AUDIO_IMPL->write_samples(device, samples, count);
}
```

#### 배치 처리
```c
// 개별 호출 대신 배치 처리
void process_audio_batch(float* buffers[], int buffer_count, int samples_per_buffer) {
    // 한 번의 인터페이스 호출로 여러 버퍼 처리
    audio_interface->process_buffers(buffers, buffer_count, samples_per_buffer);
}
```

이 마이그레이션 가이드를 따르면 기존 코드를 안전하고 효율적으로 플랫폼 추상화 레이어로 이전할 수 있습니다. 각 단계에서 충분한 테스트를 수행하고 성능 영향을 모니터링하는 것이 중요합니다.