# 플랫폼 추상화 레이어 성능 최적화 가이드

## 개요

이 문서는 LibEtude 플랫폼 추상화 레이어를 사용할 때 최적의 성능을 얻기 위한 가이드입니다. 컴파일 타임 최적화부터 런타임 최적화까지 다양한 기법을 다룹니다.

## 목차

1. [컴파일 타임 최적화](#컴파일-타임-최적화)
2. [런타임 최적화](#런타임-최적화)
3. [메모리 최적화](#메모리-최적화)
4. [플랫폼별 최적화](#플랫폼별-최적화)
5. [프로파일링 및 벤치마킹](#프로파일링-및-벤치마킹)
6. [일반적인 성능 함정](#일반적인-성능-함정)

## 컴파일 타임 최적화

### 1. 플랫폼별 조건부 컴파일

#### 매크로 기반 인터페이스 선택
```c
// include/libetude/platform/config.h
#ifdef LIBETUDE_PLATFORM_WINDOWS
    #define ET_AUDIO_IMPL (&windows_audio_interface)
    #define ET_SYSTEM_IMPL (&windows_system_interface)
#elif defined(LIBETUDE_PLATFORM_LINUX)
    #define ET_AUDIO_IMPL (&linux_audio_interface)
    #define ET_SYSTEM_IMPL (&linux_system_interface)
#elif defined(LIBETUDE_PLATFORM_MACOS)
    #define ET_AUDIO_IMPL (&macos_audio_interface)
    #define ET_SYSTEM_IMPL (&macos_system_interface)
#endif
```

#### 인라인 함수를 통한 함수 호출 오버헤드 제거
```c
// 고성능 경로에서 사용할 인라인 래퍼
LIBETUDE_INLINE ETResult et_audio_write_samples(ETAudioDevice* device,
                                                const float* samples,
                                                int count) {
    // 컴파일 타임에 직접 구현체 호출로 변환
    return ET_AUDIO_IMPL->write_samples(device, samples, count);
}

// 사용 예제
void audio_callback(void* output, int frame_count, void* user_data) {
    float* buffer = (float*)output;
    // 인라인 함수 사용으로 함수 포인터 호출 오버헤드 제거
    et_audio_write_samples(device, buffer, frame_count);
}
```

### 2. 컴파일러별 최적화 힌트

#### GCC/Clang 최적화
```c
// 자주 호출되는 함수에 대한 힌트
__attribute__((hot))
ETResult et_system_get_time_fast(uint64_t* time_ns) {
    return ET_SYSTEM_IMPL->get_high_resolution_time(time_ns);
}

// 거의 호출되지 않는 오류 처리 함수
__attribute__((cold))
void et_handle_critical_error(ETResult error) {
    // 오류 처리 로직
}

// 분기 예측 힌트
LIBETUDE_INLINE bool et_likely_success(ETResult result) {
    return __builtin_expect(result == ET_SUCCESS, 1);
}
```

#### MSVC 최적화
```c
// Windows에서 최적화 힌트
#ifdef _MSC_VER
    #define ET_LIKELY(x) (x)
    #define ET_UNLIKELY(x) (x)
    #define ET_FORCE_INLINE __forceinline
#else
    #define ET_LIKELY(x) __builtin_expect(!!(x), 1)
    #define ET_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define ET_FORCE_INLINE __attribute__((always_inline)) inline
#endif

// 사용 예제
ET_FORCE_INLINE ETResult et_check_result(ETResult result) {
    if (ET_LIKELY(result == ET_SUCCESS)) {
        return ET_SUCCESS;
    }
    return handle_error(result);
}
```

### 3. 템플릿 기반 특화 (C++)

```cpp
// C++ 바인딩에서 템플릿 특화 사용
template<ETPlatformType Platform>
struct PlatformTraits;

template<>
struct PlatformTraits<ET_PLATFORM_WINDOWS> {
    using AudioImpl = WindowsAudioInterface;
    using SystemImpl = WindowsSystemInterface;
    static constexpr bool HasHighResTimer = true;
};

template<>
struct PlatformTraits<ET_PLATFORM_LINUX> {
    using AudioImpl = LinuxAudioInterface;
    using SystemImpl = LinuxSystemInterface;
    static constexpr bool HasHighResTimer = true;
};

// 컴파일 타임 플랫폼 선택
template<ETPlatformType Platform>
class OptimizedEngine {
    using Traits = PlatformTraits<Platform>;
    typename Traits::AudioImpl audio;
    typename Traits::SystemImpl system;

public:
    void process_audio() {
        if constexpr (Traits::HasHighResTimer) {
            // 고해상도 타이머 사용
            uint64_t start_time;
            system.get_high_resolution_time(&start_time);
            // 처리 로직
        }
    }
};
```

## 런타임 최적화

### 1. 기능 감지 및 캐싱

#### SIMD 기능 캐싱
```c
// 전역 캐시 변수
static uint32_t g_cached_simd_features = 0;
static bool g_features_initialized = false;
static ETMutex* g_features_mutex = NULL;

// 초기화 함수
void et_initialize_feature_cache(void) {
    if (!g_features_mutex) {
        thread_interface->create_mutex(&g_features_mutex);
    }
}

// 최적화된 기능 조회
uint32_t et_get_simd_features_cached(void) {
    if (ET_LIKELY(g_features_initialized)) {
        return g_cached_simd_features;
    }

    // 첫 번째 호출 시에만 뮤텍스 사용
    thread_interface->lock_mutex(g_features_mutex);
    if (!g_features_initialized) {
        g_cached_simd_features = system_interface->get_simd_features();
        g_features_initialized = true;
    }
    thread_interface->unlock_mutex(g_features_mutex);

    return g_cached_simd_features;
}
```

#### 동적 함수 디스패치
```c
// 함수 포인터 캐싱
typedef void (*ProcessAudioFunc)(float* buffer, int count);

static ProcessAudioFunc g_process_audio_func = NULL;

void initialize_audio_processor(void) {
    uint32_t features = et_get_simd_features_cached();

    if (features & ET_SIMD_AVX2) {
        g_process_audio_func = process_audio_avx2;
    } else if (features & ET_SIMD_SSE4_1) {
        g_process_audio_func = process_audio_sse41;
    } else if (features & ET_SIMD_NEON) {
        g_process_audio_func = process_audio_neon;
    } else {
        g_process_audio_func = process_audio_scalar;
    }
}

// 최적화된 오디오 처리
LIBETUDE_INLINE void process_audio_optimized(float* buffer, int count) {
    g_process_audio_func(buffer, count);
}
```

### 2. 메모리 접근 최적화

#### 캐시 친화적 데이터 구조
```c
// 캐시 라인 크기 고려한 구조체 정렬
#define ET_CACHE_LINE_SIZE 64

typedef struct ETAudioDevice {
    // 자주 접근하는 데이터를 앞쪽에 배치
    ETAudioState state;           // 4 bytes
    uint32_t sample_rate;         // 4 bytes
    uint16_t channels;            // 2 bytes
    uint16_t bits_per_sample;     // 2 bytes

    // 패딩으로 캐시 라인 정렬
    char padding1[ET_CACHE_LINE_SIZE - 12];

    // 덜 자주 접근하는 데이터
    char device_name[256];
    void* platform_data;

    // 다음 캐시 라인으로 정렬
    char padding2[ET_CACHE_LINE_SIZE - ((256 + sizeof(void*)) % ET_CACHE_LINE_SIZE)];
} ETAudioDevice;
```

#### 메모리 프리페칭
```c
// 메모리 프리페칭을 통한 성능 향상
void process_audio_buffer_optimized(float* input, float* output, int count) {
    const int prefetch_distance = 64; // 64 샘플 앞서 프리페치

    for (int i = 0; i < count; i++) {
        // 미래 데이터 프리페치
        if (i + prefetch_distance < count) {
            __builtin_prefetch(&input[i + prefetch_distance], 0, 3);
            __builtin_prefetch(&output[i + prefetch_distance], 1, 3);
        }

        // 현재 데이터 처리
        output[i] = process_sample(input[i]);
    }
}
```

### 3. 스레드 최적화

#### 스레드 풀 사용
```c
typedef struct ETThreadPool {
    ETThread** threads;
    int thread_count;
    ETMutex* queue_mutex;
    ETCondition* work_available;
    ETCondition* work_complete;
    bool shutdown;
} ETThreadPool;

// 스레드 풀 기반 병렬 처리
void process_audio_parallel(float* buffer, int total_samples) {
    int samples_per_thread = total_samples / thread_pool->thread_count;

    for (int i = 0; i < thread_pool->thread_count; i++) {
        AudioTask* task = create_audio_task();
        task->buffer = buffer + (i * samples_per_thread);
        task->count = (i == thread_pool->thread_count - 1) ?
                      total_samples - (i * samples_per_thread) : samples_per_thread;

        submit_task_to_pool(thread_pool, task);
    }

    wait_for_all_tasks(thread_pool);
}
```

## 메모리 최적화

### 1. 메모리 풀 사용

#### 객체 풀
```c
typedef struct ETObjectPool {
    void* memory_block;
    size_t object_size;
    int capacity;
    int* free_indices;
    int free_count;
    ETMutex* mutex;
} ETObjectPool;

// 오디오 디바이스용 객체 풀
static ETObjectPool* g_audio_device_pool = NULL;

void initialize_audio_device_pool(void) {
    g_audio_device_pool = create_object_pool(sizeof(ETAudioDevice), 16);
}

ETAudioDevice* allocate_audio_device_fast(void) {
    return (ETAudioDevice*)pool_allocate(g_audio_device_pool);
}

void free_audio_device_fast(ETAudioDevice* device) {
    pool_free(g_audio_device_pool, device);
}
```

#### 링 버퍼 최적화
```c
typedef struct ETRingBuffer {
    float* buffer;
    int capacity;
    volatile int write_pos;
    volatile int read_pos;
    int mask; // capacity - 1 (2의 거듭제곱일 때)
} ETRingBuffer;

// 2의 거듭제곱 크기로 최적화된 링 버퍼
ETRingBuffer* create_optimized_ring_buffer(int capacity) {
    // 2의 거듭제곱으로 반올림
    int actual_capacity = 1;
    while (actual_capacity < capacity) {
        actual_capacity <<= 1;
    }

    ETRingBuffer* rb = malloc(sizeof(ETRingBuffer));
    rb->buffer = memory_interface->aligned_malloc(
        actual_capacity * sizeof(float), 32);
    rb->capacity = actual_capacity;
    rb->mask = actual_capacity - 1;
    rb->write_pos = 0;
    rb->read_pos = 0;

    return rb;
}

// 모듈로 연산 대신 비트 마스킹 사용
LIBETUDE_INLINE int ring_buffer_write(ETRingBuffer* rb, const float* data, int count) {
    int write_pos = rb->write_pos;
    int available = rb->capacity - ((write_pos - rb->read_pos) & rb->mask);
    int to_write = (count < available) ? count : available;

    for (int i = 0; i < to_write; i++) {
        rb->buffer[(write_pos + i) & rb->mask] = data[i];
    }

    rb->write_pos = (write_pos + to_write) & rb->mask;
    return to_write;
}
```

### 2. 메모리 정렬 최적화

#### SIMD 친화적 메모리 할당
```c
// SIMD 연산을 위한 정렬된 메모리 할당
void* allocate_simd_buffer(size_t size) {
    // AVX-512를 위한 64바이트 정렬
    void* ptr = memory_interface->aligned_malloc(size, 64);
    if (!ptr) {
        // 폴백: 32바이트 정렬 (AVX2)
        ptr = memory_interface->aligned_malloc(size, 32);
        if (!ptr) {
            // 폴백: 16바이트 정렬 (SSE)
            ptr = memory_interface->aligned_malloc(size, 16);
        }
    }
    return ptr;
}

// 정렬 확인 매크로
#define IS_ALIGNED(ptr, alignment) (((uintptr_t)(ptr) & ((alignment) - 1)) == 0)

// 사용 예제
void process_samples_simd(float* input, float* output, int count) {
    assert(IS_ALIGNED(input, 32));
    assert(IS_ALIGNED(output, 32));

    // SIMD 연산 수행
    process_samples_avx2(input, output, count);
}
```

## 플랫폼별 최적화

### Windows 최적화

#### WASAPI 최적화
```c
// Windows에서 WASAPI 독점 모드 사용
ETResult optimize_windows_audio(ETAudioDevice* device) {
    WindowsAudioData* data = (WindowsAudioData*)device->platform_data;

    // 독점 모드로 설정하여 지연시간 최소화
    HRESULT hr = data->audio_client->Initialize(
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        0, 0, data->wave_format, NULL);

    if (SUCCEEDED(hr)) {
        // 하드웨어 버퍼 크기 최적화
        UINT32 buffer_frames;
        data->audio_client->GetBufferSize(&buffer_frames);
        device->buffer_size = buffer_frames;
        return ET_SUCCESS;
    }

    return ET_ERROR_PLATFORM_SPECIFIC;
}

// Windows 고해상도 타이머 최적화
void optimize_windows_timer(void) {
    // 타이머 해상도 향상
    timeBeginPeriod(1);

    // 성능 카운터 주파수 캐싱
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
}
```

### Linux 최적화

#### ALSA 최적화
```c
// Linux에서 ALSA 최적화 설정
ETResult optimize_linux_audio(ETAudioDevice* device) {
    LinuxAudioData* data = (LinuxAudioData*)device->platform_data;

    // 하드웨어 매개변수 최적화
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(data->pcm_handle, hw_params);

    // 인터리브 모드 설정
    snd_pcm_hw_params_set_access(data->pcm_handle, hw_params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);

    // 버퍼 크기 최적화 (지연시간 vs 안정성)
    snd_pcm_uframes_t buffer_size = 1024;
    snd_pcm_hw_params_set_buffer_size_near(data->pcm_handle, hw_params, &buffer_size);

    // 피리어드 크기 설정
    snd_pcm_uframes_t period_size = 256;
    snd_pcm_hw_params_set_period_size_near(data->pcm_handle, hw_params,
                                           &period_size, NULL);

    return snd_pcm_hw_params(data->pcm_handle, hw_params) >= 0 ?
           ET_SUCCESS : ET_ERROR_PLATFORM_SPECIFIC;
}

// Linux 스케줄링 최적화
void optimize_linux_scheduling(void) {
    // 실시간 스케줄링 설정
    struct sched_param param;
    param.sched_priority = 80;

    if (sched_setscheduler(0, SCHED_FIFO, &param) == 0) {
        // 성공: 실시간 우선순위 설정됨
    } else {
        // 실패: 일반 우선순위로 폴백
        setpriority(PRIO_PROCESS, 0, -10);
    }
}
```

### macOS 최적화

#### CoreAudio 최적화
```c
// macOS에서 CoreAudio 최적화
ETResult optimize_macos_audio(ETAudioDevice* device) {
    MacOSAudioData* data = (MacOSAudioData*)device->platform_data;

    // 하드웨어 I/O 버퍼 크기 최적화
    UInt32 buffer_size = 256;
    UInt32 size = sizeof(buffer_size);

    OSStatus status = AudioUnitSetProperty(
        data->audio_unit,
        kAudioDevicePropertyBufferFrameSize,
        kAudioUnitScope_Global,
        0,
        &buffer_size,
        size);

    if (status == noErr) {
        device->buffer_size = buffer_size;
        return ET_SUCCESS;
    }

    return ET_ERROR_PLATFORM_SPECIFIC;
}

// Metal Performance Shaders 활용
void optimize_macos_dsp(float* input, float* output, int count) {
    // Metal 컴퓨트 셰이더를 통한 DSP 가속
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];

    // GPU 기반 오디오 처리
    dispatch_dsp_on_gpu(device, commandQueue, input, output, count);
}
```

## 프로파일링 및 벤치마킹

### 1. 내장 프로파일러 사용

```c
// 성능 측정 매크로
#define ET_PROFILE_BEGIN(name) \
    uint64_t profile_start_##name; \
    system_interface->get_high_resolution_time(&profile_start_##name);

#define ET_PROFILE_END(name) \
    do { \
        uint64_t profile_end_##name; \
        system_interface->get_high_resolution_time(&profile_end_##name); \
        uint64_t elapsed = profile_end_##name - profile_start_##name; \
        et_profiler_record(#name, elapsed); \
    } while(0)

// 사용 예제
void process_audio_with_profiling(float* buffer, int count) {
    ET_PROFILE_BEGIN(audio_processing);

    // 오디오 처리 로직
    for (int i = 0; i < count; i++) {
        buffer[i] = apply_effects(buffer[i]);
    }

    ET_PROFILE_END(audio_processing);
}
```

### 2. 벤치마킹 프레임워크

```c
typedef struct ETBenchmark {
    const char* name;
    void (*setup)(void);
    void (*benchmark)(void);
    void (*teardown)(void);
    int iterations;
    uint64_t total_time;
    uint64_t min_time;
    uint64_t max_time;
} ETBenchmark;

// 벤치마크 실행
void run_benchmark(ETBenchmark* bench) {
    if (bench->setup) bench->setup();

    bench->total_time = 0;
    bench->min_time = UINT64_MAX;
    bench->max_time = 0;

    for (int i = 0; i < bench->iterations; i++) {
        uint64_t start, end;
        system_interface->get_high_resolution_time(&start);

        bench->benchmark();

        system_interface->get_high_resolution_time(&end);

        uint64_t elapsed = end - start;
        bench->total_time += elapsed;

        if (elapsed < bench->min_time) bench->min_time = elapsed;
        if (elapsed > bench->max_time) bench->max_time = elapsed;
    }

    if (bench->teardown) bench->teardown();

    // 결과 출력
    printf("벤치마크: %s\n", bench->name);
    printf("  평균: %llu ns\n", bench->total_time / bench->iterations);
    printf("  최소: %llu ns\n", bench->min_time);
    printf("  최대: %llu ns\n", bench->max_time);
}
```

## 일반적인 성능 함정

### 1. 함수 포인터 호출 오버헤드

```c
// 문제: 핫 패스에서 함수 포인터 사용
void slow_audio_callback(void* output, int frame_count, void* user_data) {
    for (int i = 0; i < frame_count; i++) {
        // 매 샘플마다 함수 포인터 호출
        ((float*)output)[i] = audio_interface->generate_sample();
    }
}

// 해결: 배치 처리 또는 인라인 함수 사용
void fast_audio_callback(void* output, int frame_count, void* user_data) {
    // 배치로 처리
    audio_interface->generate_samples((float*)output, frame_count);
}
```

### 2. 불필요한 메모리 할당

```c
// 문제: 매번 메모리 할당
void slow_process_audio(const float* input, int count) {
    float* temp_buffer = malloc(count * sizeof(float));
    // 처리 로직
    free(temp_buffer);
}

// 해결: 정적 버퍼 또는 메모리 풀 사용
static float g_temp_buffer[MAX_BUFFER_SIZE];

void fast_process_audio(const float* input, int count) {
    assert(count <= MAX_BUFFER_SIZE);
    // 정적 버퍼 사용
    memcpy(g_temp_buffer, input, count * sizeof(float));
    // 처리 로직
}
```

### 3. 캐시 미스

```c
// 문제: 캐시 비친화적 메모리 접근
void slow_matrix_multiply(float** a, float** b, float** c, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                c[i][j] += a[i][k] * b[k][j]; // b[k][j]가 캐시 미스 유발
            }
        }
    }
}

// 해결: 루프 순서 변경 및 블록화
void fast_matrix_multiply(float** a, float** b, float** c, int n) {
    const int block_size = 64;

    for (int ii = 0; ii < n; ii += block_size) {
        for (int jj = 0; jj < n; jj += block_size) {
            for (int kk = 0; kk < n; kk += block_size) {
                // 블록 단위로 처리하여 캐시 효율성 향상
                for (int i = ii; i < ii + block_size && i < n; i++) {
                    for (int j = jj; j < jj + block_size && j < n; j++) {
                        for (int k = kk; k < kk + block_size && k < n; k++) {
                            c[i][j] += a[i][k] * b[k][j];
                        }
                    }
                }
            }
        }
    }
}
```

### 4. 동기화 오버헤드

```c
// 문제: 과도한 뮤텍스 사용
ETMutex* g_counter_mutex;
int g_counter = 0;

void slow_increment(void) {
    thread_interface->lock_mutex(g_counter_mutex);
    g_counter++;
    thread_interface->unlock_mutex(g_counter_mutex);
}

// 해결: 원자적 연산 사용
#include <stdatomic.h>
atomic_int g_atomic_counter = 0;

void fast_increment(void) {
    atomic_fetch_add(&g_atomic_counter, 1);
}
```

이 가이드를 따르면 LibEtude 플랫폼 추상화 레이어를 사용할 때 최적의 성능을 얻을 수 있습니다. 각 최적화 기법은 실제 사용 사례에 맞게 적용하고, 프로파일링을 통해 효과를 검증하는 것이 중요합니다.