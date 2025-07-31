# LibEtude C API 모범 사례 가이드

## 개요

이 문서는 LibEtude C API를 효과적이고 안전하게 사용하기 위한 모범 사례를 제공합니다. 성능 최적화, 메모리 관리, 오류 처리, 스레드 안전성 등 실제 개발에서 중요한 주제들을 다룹니다.

## 1. 기본 원칙

### 1.1 리소스 관리 (RAII 패턴)

LibEtude는 C 라이브러리이지만 RAII(Resource Acquisition Is Initialization) 패턴을 적용할 수 있습니다.

```c
// 좋은 예: 래퍼 구조체 사용
typedef struct {
    LibEtudeEngine* engine;
    bool is_initialized;
    char model_path[256];
} TTSContext;

TTSContext* tts_context_create(const char* model_path) {
    TTSContext* ctx = calloc(1, sizeof(TTSContext));
    if (!ctx) {
        return NULL;
    }

    strncpy(ctx->model_path, model_path, sizeof(ctx->model_path) - 1);

    ctx->engine = libetude_create_engine(model_path);
    if (!ctx->engine) {
        free(ctx);
        return NULL;
    }

    ctx->is_initialized = true;
    return ctx;
}

void tts_context_destroy(TTSContext* ctx) {
    if (ctx) {
        if (ctx->engine) {
            libetude_destroy_engine(ctx->engine);
        }
        ctx->is_initialized = false;
        free(ctx);
    }
}

// 사용 예
int main() {
    TTSContext* tts = tts_context_create("model.lef");
    if (!tts) {
        fprintf(stderr, "TTS 컨텍스트 생성 실패\n");
        return -1;
    }

    // TTS 사용...

    tts_context_destroy(tts); // 자동으로 모든 리소스 정리
    return 0;
}
```

### 1.2 오류 처리 일관성

모든 API 호출에 대해 일관된 오류 처리를 적용하세요.

```c
// 좋은 예: 매크로를 사용한 일관된 오류 처리
#define CHECK_RESULT(call, context) \
    do { \
        int _result = (call); \
        if (_result != LIBETUDE_SUCCESS) { \
            fprintf(stderr, "[ERROR] %s failed in %s: %s\n", \
                    #call, context, libetude_get_last_error()); \
            return _result; \
        } \
    } while(0)

int synthesize_with_error_handling(LibEtudeEngine* engine, const char* text) {
    float audio_buffer[44100 * 10];
    int buffer_size = sizeof(audio_buffer) / sizeof(float);

    CHECK_RESULT(libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH),
                 "quality mode setting");
    CHECK_RESULT(libetude_synthesize_text(engine, text, audio_buffer, &buffer_size),
                 "text synthesis");

    printf("합성 성공: %d 샘플 생성\n", buffer_size);
    return LIBETUDE_SUCCESS;
}
```

### 1.3 매개변수 검증

모든 공개 함수에서 매개변수를 검증하세요.

```c
// 좋은 예: 철저한 매개변수 검증
int safe_synthesize_text(LibEtudeEngine* engine, const char* text,
                        float* output, int* length) {
    // NULL 포인터 검사
    if (!engine) {
        fprintf(stderr, "엔진이 NULL입니다\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!text) {
        fprintf(stderr, "텍스트가 NULL입니다\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!output) {
        fprintf(stderr, "출력 버퍼가 NULL입니다\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (!length || *length <= 0) {
        fprintf(stderr, "버퍼 크기가 유효하지 않습니다\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 텍스트 길이 검사
    size_t text_len = strlen(text);
    if (text_len == 0) {
        fprintf(stderr, "텍스트가 비어있습니다\n");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    if (text_len > LIBETUDE_MAX_TEXT_LENGTH) {
        fprintf(stderr, "텍스트가 너무 깁니다 (최대 %d자)\n", LIBETUDE_MAX_TEXT_LENGTH);
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    return libetude_synthesize_text(engine, text, output, length);
}
```

## 2. 성능 최적화

### 2.1 하드웨어 기능 활용

시스템의 하드웨어 기능을 최대한 활용하세요.

```c
void optimize_for_hardware(LibEtudeEngine* engine) {
    // 하드웨어 기능 감지
    uint32_t features = libetude_get_hardware_features();

    printf("지원되는 SIMD 기능:\n");
    if (features & LIBETUDE_SIMD_SSE) printf("- SSE\n");
    if (features & LIBETUDE_SIMD_SSE2) printf("- SSE2\n");
    if (features & LIBETUDE_SIMD_AVX) printf("- AVX\n");
    if (features & LIBETUDE_SIMD_AVX2) printf("- AVX2\n");
    if (features & LIBETUDE_SIMD_NEON) printf("- NEON\n");

    // GPU 가속 시도
    int gpu_result = libetude_enable_gpu_acceleration(engine);
    if (gpu_result == LIBETUDE_SUCCESS) {
        printf("GPU 가속 활성화됨\n");
        // GPU가 있으면 고품질 모드 사용 가능
        libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH);
    } else {
        printf("GPU 가속 불가능, CPU 모드 사용\n");

        // CPU 성능에 따라 품질 모드 조정
        if (features & LIBETUDE_SIMD_AVX2) {
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH);
        } else if (features & LIBETUDE_SIMD_SSE2) {
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
        } else {
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
        }
    }
}
```

### 2.2 메모리 사용량 모니터링

정기적으로 메모리 사용량을 모니터링하고 최적화하세요.

```c
typedef struct {
    double peak_memory_mb;
    double avg_memory_mb;
    int sample_count;
    time_t last_check;
} MemoryMonitor;

void monitor_memory_usage(LibEtudeEngine* engine, MemoryMonitor* monitor) {
    PerformanceStats stats;
    int result = libetude_get_performance_stats(engine, &stats);

    if (result == LIBETUDE_SUCCESS) {
        // 피크 메모리 업데이트
        if (stats.memory_usage_mb > monitor->peak_memory_mb) {
            monitor->peak_memory_mb = stats.memory_usage_mb;
        }

        // 평균 메모리 계산
        monitor->avg_memory_mb =
            (monitor->avg_memory_mb * monitor->sample_count + stats.memory_usage_mb) /
            (monitor->sample_count + 1);
        monitor->sample_count++;

        // 메모리 사용량이 높으면 경고
        if (stats.memory_usage_mb > 1000.0) { // 1GB 초과
            printf("경고: 메모리 사용량이 높습니다 (%.2f MB)\n", stats.memory_usage_mb);

            // 품질 모드를 낮춰서 메모리 절약
            libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
        }

        monitor->last_check = time(NULL);
    }
}

// 주기적 모니터링
void periodic_memory_check(LibEtudeEngine* engine, MemoryMonitor* monitor) {
    time_t now = time(NULL);
    if (now - monitor->last_check >= 10) { // 10초마다 체크
        monitor_memory_usage(engine, monitor);
    }
}
```

### 2.3 배치 처리 최적화

여러 텍스트를 처리할 때는 배치 처리를 고려하세요.

```c
typedef struct {
    char* text;
    float* audio_buffer;
    int buffer_size;
    int actual_length;
    int result;
} BatchItem;

int batch_synthesize(LibEtudeEngine* engine, BatchItem* items, int item_count) {
    if (!engine || !items || item_count <= 0) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 엔진을 한 번만 최적화
    optimize_for_hardware(engine);

    int success_count = 0;
    int total_samples = 0;

    for (int i = 0; i < item_count; i++) {
        BatchItem* item = &items[i];

        // 각 항목 처리
        item->actual_length = item->buffer_size;
        item->result = libetude_synthesize_text(engine, item->text,
                                               item->audio_buffer, &item->actual_length);

        if (item->result == LIBETUDE_SUCCESS) {
            success_count++;
            total_samples += item->actual_length;
        } else {
            printf("항목 %d 처리 실패: %s\n", i, libetude_get_last_error());
        }

        // 진행률 표시
        if ((i + 1) % 10 == 0 || i == item_count - 1) {
            printf("진행률: %d/%d (%.1f%%)\n", i + 1, item_count,
                   100.0 * (i + 1) / item_count);
        }
    }

    printf("배치 처리 완료: %d/%d 성공, 총 %d 샘플 생성\n",
           success_count, item_count, total_samples);

    return success_count == item_count ? LIBETUDE_SUCCESS : LIBETUDE_ERROR_RUNTIME;
}
```

## 3. 스레드 안전성

### 3.1 스레드 안전한 래퍼

LibEtude 엔진을 여러 스레드에서 안전하게 사용하기 위한 래퍼를 구현하세요.

```c
#include <pthread.h>

typedef struct {
    LibEtudeEngine* engine;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_busy;
    int thread_count;
} ThreadSafeEngine;

ThreadSafeEngine* thread_safe_engine_create(const char* model_path) {
    ThreadSafeEngine* tse = calloc(1, sizeof(ThreadSafeEngine));
    if (!tse) {
        return NULL;
    }

    tse->engine = libetude_create_engine(model_path);
    if (!tse->engine) {
        free(tse);
        return NULL;
    }

    if (pthread_mutex_init(&tse->mutex, NULL) != 0) {
        libetude_destroy_engine(tse->engine);
        free(tse);
        return NULL;
    }

    if (pthread_cond_init(&tse->cond, NULL) != 0) {
        pthread_mutex_destroy(&tse->mutex);
        libetude_destroy_engine(tse->engine);
        free(tse);
        return NULL;
    }

    return tse;
}

int thread_safe_synthesize(ThreadSafeEngine* tse, const char* text,
                          float* output, int* length) {
    if (!tse) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&tse->mutex);

    // 엔진이 사용 중이면 대기
    while (tse->is_busy) {
        pthread_cond_wait(&tse->cond, &tse->mutex);
    }

    tse->is_busy = true;
    tse->thread_count++;

    pthread_mutex_unlock(&tse->mutex);

    // 실제 합성 수행 (뮤텍스 해제 상태에서)
    int result = libetude_synthesize_text(tse->engine, text, output, length);

    pthread_mutex_lock(&tse->mutex);

    tse->is_busy = false;
    tse->thread_count--;

    // 대기 중인 스레드에게 신호
    pthread_cond_signal(&tse->cond);

    pthread_mutex_unlock(&tse->mutex);

    return result;
}

void thread_safe_engine_destroy(ThreadSafeEngine* tse) {
    if (tse) {
        pthread_mutex_lock(&tse->mutex);

        // 모든 스레드가 완료될 때까지 대기
        while (tse->thread_count > 0) {
            pthread_cond_wait(&tse->cond, &tse->mutex);
        }

        pthread_mutex_unlock(&tse->mutex);

        libetude_destroy_engine(tse->engine);
        pthread_mutex_destroy(&tse->mutex);
        pthread_cond_destroy(&tse->cond);
        free(tse);
    }
}
```

### 3.2 스레드 풀 패턴

여러 작업을 효율적으로 처리하기 위한 스레드 풀을 구현하세요.

```c
typedef struct {
    char* text;
    float* output;
    int buffer_size;
    int result;
    bool completed;
} WorkItem;

typedef struct {
    ThreadSafeEngine* engine;
    WorkItem* work_queue;
    int queue_size;
    int queue_head;
    int queue_tail;
    int queue_count;

    pthread_t* threads;
    int thread_count;
    bool shutdown;

    pthread_mutex_t queue_mutex;
    pthread_cond_t work_cond;
    pthread_cond_t done_cond;
} ThreadPool;

void* worker_thread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;

    while (!pool->shutdown) {
        pthread_mutex_lock(&pool->queue_mutex);

        // 작업이 없으면 대기
        while (pool->queue_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->work_cond, &pool->queue_mutex);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        // 작업 가져오기
        WorkItem* item = &pool->work_queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1) % pool->queue_size;
        pool->queue_count--;

        pthread_mutex_unlock(&pool->queue_mutex);

        // 작업 수행
        int length = item->buffer_size;
        item->result = thread_safe_synthesize(pool->engine, item->text,
                                             item->output, &length);
        item->completed = true;

        // 완료 신호
        pthread_cond_signal(&pool->done_cond);
    }

    return NULL;
}

ThreadPool* thread_pool_create(const char* model_path, int thread_count, int queue_size) {
    ThreadPool* pool = calloc(1, sizeof(ThreadPool));
    if (!pool) return NULL;

    pool->engine = thread_safe_engine_create(model_path);
    if (!pool->engine) {
        free(pool);
        return NULL;
    }

    pool->work_queue = calloc(queue_size, sizeof(WorkItem));
    pool->threads = calloc(thread_count, sizeof(pthread_t));
    pool->queue_size = queue_size;
    pool->thread_count = thread_count;

    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->work_cond, NULL);
    pthread_cond_init(&pool->done_cond, NULL);

    // 워커 스레드 생성
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }

    return pool;
}
```

## 4. 실시간 처리

### 4.1 저지연 스트리밍

실시간 애플리케이션을 위한 저지연 스트리밍을 구현하세요.

```c
typedef struct {
    LibEtudeEngine* engine;
    pthread_t streaming_thread;
    pthread_mutex_t text_mutex;

    char* text_queue[100];
    int queue_head;
    int queue_tail;
    int queue_count;

    AudioStreamCallback user_callback;
    void* user_data;

    bool is_streaming;
    bool should_stop;
} LowLatencyStreamer;

void* streaming_thread_func(void* arg) {
    LowLatencyStreamer* streamer = (LowLatencyStreamer*)arg;

    while (!streamer->should_stop) {
        pthread_mutex_lock(&streamer->text_mutex);

        if (streamer->queue_count == 0) {
            pthread_mutex_unlock(&streamer->text_mutex);
            usleep(1000); // 1ms 대기
            continue;
        }

        // 텍스트 가져오기
        char* text = streamer->text_queue[streamer->queue_head];
        streamer->queue_head = (streamer->queue_head + 1) % 100;
        streamer->queue_count--;

        pthread_mutex_unlock(&streamer->text_mutex);

        // 빠른 합성 (작은 청크 단위)
        float audio_chunk[1024]; // 작은 버퍼 사용
        int chunk_size = 1024;

        int result = libetude_synthesize_text(streamer->engine, text,
                                             audio_chunk, &chunk_size);

        if (result == LIBETUDE_SUCCESS && streamer->user_callback) {
            streamer->user_callback(audio_chunk, chunk_size, streamer->user_data);
        }

        free(text);
    }

    return NULL;
}

int low_latency_stream_text(LowLatencyStreamer* streamer, const char* text) {
    if (!streamer || !text) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&streamer->text_mutex);

    if (streamer->queue_count >= 100) {
        pthread_mutex_unlock(&streamer->text_mutex);
        return LIBETUDE_ERROR_BUFFER_FULL;
    }

    // 텍스트 복사 및 큐에 추가
    streamer->text_queue[streamer->queue_tail] = strdup(text);
    streamer->queue_tail = (streamer->queue_tail + 1) % 100;
    streamer->queue_count++;

    pthread_mutex_unlock(&streamer->text_mutex);

    return LIBETUDE_SUCCESS;
}
```

### 4.2 적응적 품질 조정

네트워크 상황이나 시스템 부하에 따라 품질을 동적으로 조정하세요.

```c
typedef struct {
    LibEtudeEngine* engine;
    QualityMode current_mode;

    // 성능 메트릭
    double avg_latency_ms;
    double avg_cpu_usage;
    int sample_count;

    // 임계값
    double max_latency_ms;
    double max_cpu_usage;
} AdaptiveQualityController;

void update_quality_based_on_performance(AdaptiveQualityController* controller) {
    PerformanceStats stats;
    int result = libetude_get_performance_stats(controller->engine, &stats);

    if (result != LIBETUDE_SUCCESS) {
        return;
    }

    // 평균 계산 업데이트
    controller->avg_latency_ms =
        (controller->avg_latency_ms * controller->sample_count + stats.inference_time_ms) /
        (controller->sample_count + 1);

    controller->avg_cpu_usage =
        (controller->avg_cpu_usage * controller->sample_count + stats.cpu_usage_percent) /
        (controller->sample_count + 1);

    controller->sample_count++;

    // 품질 모드 조정
    QualityMode new_mode = controller->current_mode;

    if (controller->avg_latency_ms > controller->max_latency_ms ||
        controller->avg_cpu_usage > controller->max_cpu_usage) {

        // 성능이 부족하면 품질 낮춤
        if (controller->current_mode == LIBETUDE_QUALITY_HIGH) {
            new_mode = LIBETUDE_QUALITY_BALANCED;
        } else if (controller->current_mode == LIBETUDE_QUALITY_BALANCED) {
            new_mode = LIBETUDE_QUALITY_FAST;
        }

    } else if (controller->avg_latency_ms < controller->max_latency_ms * 0.7 &&
               controller->avg_cpu_usage < controller->max_cpu_usage * 0.7) {

        // 성능에 여유가 있으면 품질 높임
        if (controller->current_mode == LIBETUDE_QUALITY_FAST) {
            new_mode = LIBETUDE_QUALITY_BALANCED;
        } else if (controller->current_mode == LIBETUDE_QUALITY_BALANCED) {
            new_mode = LIBETUDE_QUALITY_HIGH;
        }
    }

    if (new_mode != controller->current_mode) {
        printf("품질 모드 변경: %d -> %d (지연: %.2fms, CPU: %.1f%%)\n",
               controller->current_mode, new_mode,
               controller->avg_latency_ms, controller->avg_cpu_usage);

        libetude_set_quality_mode(controller->engine, new_mode);
        controller->current_mode = new_mode;

        // 통계 리셋
        controller->sample_count = 0;
        controller->avg_latency_ms = 0;
        controller->avg_cpu_usage = 0;
    }
}
```

## 5. 플랫폼별 최적화

### 5.1 모바일 플랫폼 최적화

모바일 환경에서의 배터리 효율성과 성능을 고려하세요.

```c
#ifdef __ANDROID__
#include <android/log.h>

typedef struct {
    LibEtudeEngine* engine;
    bool low_power_mode;
    bool background_mode;
    time_t last_activity;
} MobileOptimizer;

void configure_for_mobile(MobileOptimizer* optimizer) {
    // 모바일 환경에서는 기본적으로 빠른 모드 사용
    libetude_set_quality_mode(optimizer->engine, LIBETUDE_QUALITY_FAST);

    // 배터리 상태에 따른 최적화
    if (optimizer->low_power_mode) {
        // 저전력 모드에서는 최소 품질 사용
        __android_log_print(ANDROID_LOG_INFO, "LibEtude",
                           "저전력 모드 활성화");
    }

    // 백그라운드 모드 처리
    if (optimizer->background_mode) {
        // 백그라운드에서는 처리 빈도 감소
        __android_log_print(ANDROID_LOG_INFO, "LibEtude",
                           "백그라운드 모드로 전환");
    }
}

void handle_app_state_change(MobileOptimizer* optimizer, bool is_foreground) {
    optimizer->background_mode = !is_foreground;

    if (is_foreground) {
        // 포그라운드로 전환 시 정상 모드
        libetude_set_quality_mode(optimizer->engine, LIBETUDE_QUALITY_BALANCED);
        optimizer->last_activity = time(NULL);
    } else {
        // 백그라운드로 전환 시 절전 모드
        libetude_set_quality_mode(optimizer->engine, LIBETUDE_QUALITY_FAST);
    }
}
#endif
```

### 5.2 임베디드 시스템 최적화

제한된 리소스 환경에서의 최적화를 구현하세요.

```c
#ifdef LIBETUDE_MINIMAL
typedef struct {
    LibEtudeEngine* engine;
    size_t max_memory_mb;
    size_t current_memory_mb;
    bool memory_critical;
} EmbeddedOptimizer;

void configure_for_embedded(EmbeddedOptimizer* optimizer) {
    // 임베디드 환경에서는 항상 최소 메모리 모드
    libetude_set_quality_mode(optimizer->engine, LIBETUDE_QUALITY_FAST);

    // 메모리 사용량 엄격 제한
    optimizer->max_memory_mb = 64; // 64MB 제한

    printf("임베디드 모드 활성화 (메모리 제한: %zuMB)\n",
           optimizer->max_memory_mb);
}

int embedded_synthesize_with_memory_check(EmbeddedOptimizer* optimizer,
                                         const char* text,
                                         float* output, int* length) {
    // 메모리 사용량 체크
    PerformanceStats stats;
    int result = libetude_get_performance_stats(optimizer->engine, &stats);

    if (result == LIBETUDE_SUCCESS) {
        optimizer->current_memory_mb = (size_t)stats.memory_usage_mb;

        if (optimizer->current_memory_mb > optimizer->max_memory_mb) {
            printf("메모리 제한 초과: %zuMB > %zuMB\n",
                   optimizer->current_memory_mb, optimizer->max_memory_mb);
            return LIBETUDE_ERROR_OUT_OF_MEMORY;
        }

        // 메모리 사용량이 80% 이상이면 경고
        if (optimizer->current_memory_mb > optimizer->max_memory_mb * 0.8) {
            optimizer->memory_critical = true;
            printf("메모리 사용량 경고: %zuMB (%.1f%%)\n",
                   optimizer->current_memory_mb,
                   100.0 * optimizer->current_memory_mb / optimizer->max_memory_mb);
        }
    }

    return libetude_synthesize_text(optimizer->engine, text, output, length);
}
#endif
```

## 6. 디버깅 및 프로파일링

### 6.1 상세 로깅 시스템

개발 및 디버깅을 위한 상세 로깅을 구현하세요.

```c
typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4
} LogLevel;

typedef struct {
    FILE* log_file;
    LogLevel min_level;
    bool console_output;
    pthread_mutex_t log_mutex;
} Logger;

static Logger g_logger = {0};

void init_detailed_logging(const char* log_file_path, LogLevel min_level) {
    g_logger.log_file = fopen(log_file_path, "a");
    g_logger.min_level = min_level;
    g_logger.console_output = true;
    pthread_mutex_init(&g_logger.log_mutex, NULL);

    // LibEtude 로그 콜백 설정
    libetude_set_log_callback(libetude_log_callback, NULL);
    libetude_set_log_level(LIBETUDE_LOG_DEBUG);
}

void detailed_log(LogLevel level, const char* function, int line,
                 const char* format, ...) {
    if (level < g_logger.min_level) {
        return;
    }

    pthread_mutex_lock(&g_logger.log_mutex);

    // 시간 스탬프
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // 로그 레벨 문자열
    const char* level_str[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};

    // 메시지 포맷팅
    va_list args;
    va_start(args, format);

    char message[1024];
    vsnprintf(message, sizeof(message), format, args);

    // 로그 출력
    char log_line[1200];
    snprintf(log_line, sizeof(log_line), "[%s] [%s] %s:%d - %s\n",
             timestamp, level_str[level], function, line, message);

    if (g_logger.console_output) {
        printf("%s", log_line);
    }

    if (g_logger.log_file) {
        fprintf(g_logger.log_file, "%s", log_line);
        fflush(g_logger.log_file);
    }

    va_end(args);
    pthread_mutex_unlock(&g_logger.log_mutex);
}

#define LOG_TRACE(format, ...) detailed_log(LOG_LEVEL_TRACE, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) detailed_log(LOG_LEVEL_DEBUG, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) detailed_log(LOG_LEVEL_INFO, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) detailed_log(LOG_LEVEL_WARN, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) detailed_log(LOG_LEVEL_ERROR, __func__, __LINE__, format, ##__VA_ARGS__)
```

### 6.2 성능 프로파일링

성능 병목을 찾기 위한 프로파일링 도구를 구현하세요.

```c
typedef struct {
    const char* name;
    struct timespec start_time;
    struct timespec end_time;
    double duration_ms;
} ProfileEntry;

typedef struct {
    ProfileEntry entries[1000];
    int entry_count;
    pthread_mutex_t mutex;
} Profiler;

static Profiler g_profiler = {0};

void profiler_init() {
    g_profiler.entry_count = 0;
    pthread_mutex_init(&g_profiler.mutex, NULL);
}

void profiler_start(const char* name) {
    pthread_mutex_lock(&g_profiler.mutex);

    if (g_profiler.entry_count < 1000) {
        ProfileEntry* entry = &g_profiler.entries[g_profiler.entry_count];
        entry->name = name;
        clock_gettime(CLOCK_MONOTONIC, &entry->start_time);
        g_profiler.entry_count++;
    }

    pthread_mutex_unlock(&g_profiler.mutex);
}

void profiler_end(const char* name) {
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    pthread_mutex_lock(&g_profiler.mutex);

    // 해당 이름의 가장 최근 항목 찾기
    for (int i = g_profiler.entry_count - 1; i >= 0; i--) {
        ProfileEntry* entry = &g_profiler.entries[i];
        if (strcmp(entry->name, name) == 0 && entry->duration_ms == 0) {
            entry->end_time = end_time;

            // 지속 시간 계산
            double start_ms = entry->start_time.tv_sec * 1000.0 +
                             entry->start_time.tv_nsec / 1000000.0;
            double end_ms = entry->end_time.tv_sec * 1000.0 +
                           entry->end_time.tv_nsec / 1000000.0;
            entry->duration_ms = end_ms - start_ms;

            break;
        }
    }

    pthread_mutex_unlock(&g_profiler.mutex);
}

void profiler_report() {
    pthread_mutex_lock(&g_profiler.mutex);

    printf("\n=== 성능 프로파일링 리포트 ===\n");

    // 이름별로 통계 계산
    typedef struct {
        const char* name;
        double total_time;
        double min_time;
        double max_time;
        int count;
    } ProfileStat;

    ProfileStat stats[100] = {0};
    int stat_count = 0;

    for (int i = 0; i < g_profiler.entry_count; i++) {
        ProfileEntry* entry = &g_profiler.entries[i];
        if (entry->duration_ms == 0) continue;

        // 기존 통계 찾기
        ProfileStat* stat = NULL;
        for (int j = 0; j < stat_count; j++) {
            if (strcmp(stats[j].name, entry->name) == 0) {
                stat = &stats[j];
                break;
            }
        }

        // 새 통계 생성
        if (!stat && stat_count < 100) {
            stat = &stats[stat_count++];
            stat->name = entry->name;
            stat->min_time = entry->duration_ms;
            stat->max_time = entry->duration_ms;
        }

        if (stat) {
            stat->total_time += entry->duration_ms;
            stat->count++;
            if (entry->duration_ms < stat->min_time) {
                stat->min_time = entry->duration_ms;
            }
            if (entry->duration_ms > stat->max_time) {
                stat->max_time = entry->duration_ms;
            }
        }
    }

    // 결과 출력
    printf("%-30s %8s %8s %8s %8s %6s\n",
           "함수명", "총시간", "평균", "최소", "최대", "호출수");
    printf("%-30s %8s %8s %8s %8s %6s\n",
           "------------------------------", "--------", "--------", "--------", "--------", "------");

    for (int i = 0; i < stat_count; i++) {
        ProfileStat* stat = &stats[i];
        double avg_time = stat->total_time / stat->count;

        printf("%-30s %8.2f %8.2f %8.2f %8.2f %6d\n",
               stat->name, stat->total_time, avg_time,
               stat->min_time, stat->max_time, stat->count);
    }

    pthread_mutex_unlock(&g_profiler.mutex);
}

// 편의 매크로
#define PROFILE_START(name) profiler_start(name)
#define PROFILE_END(name) profiler_end(name)
#define PROFILE_SCOPE(name) \
    profiler_start(name); \
    __attribute__((cleanup(profiler_cleanup))) const char* _profile_name = name

void profiler_cleanup(const char** name) {
    profiler_end(*name);
}
```

## 7. 테스트 및 검증

### 7.1 단위 테스트 프레임워크

간단한 단위 테스트 프레임워크를 구현하세요.

```c
typedef struct {
    const char* name;
    void (*test_func)(void);
    bool passed;
    char error_message[256];
} TestCase;

typedef struct {
    TestCase* tests;
    int test_count;
    int passed_count;
    int failed_count;
} TestSuite;

static TestSuite g_test_suite = {0};
static char g_current_test_name[256] = {0};

void test_assert(bool condition, const char* message) {
    if (!condition) {
        snprintf(g_test_suite.tests[g_test_suite.test_count - 1].error_message,
                sizeof(g_test_suite.tests[g_test_suite.test_count - 1].error_message),
                "%s", message);
        g_test_suite.tests[g_test_suite.test_count - 1].passed = false;
    }
}

void add_test(const char* name, void (*test_func)(void)) {
    if (g_test_suite.test_count == 0) {
        g_test_suite.tests = malloc(100 * sizeof(TestCase));
    }

    TestCase* test = &g_test_suite.tests[g_test_suite.test_count++];
    test->name = name;
    test->test_func = test_func;
    test->passed = true;
    test->error_message[0] = '\0';
}

void run_all_tests() {
    printf("=== LibEtude API 테스트 실행 ===\n");

    for (int i = 0; i < g_test_suite.test_count; i++) {
        TestCase* test = &g_test_suite.tests[i];
        strcpy(g_current_test_name, test->name);

        printf("테스트: %s ... ", test->name);
        fflush(stdout);

        test->test_func();

        if (test->passed) {
            printf("통과\n");
            g_test_suite.passed_count++;
        } else {
            printf("실패: %s\n", test->error_message);
            g_test_suite.failed_count++;
        }
    }

    printf("\n결과: %d개 통과, %d개 실패\n",
           g_test_suite.passed_count, g_test_suite.failed_count);
}

// 테스트 케이스 예제
void test_engine_creation() {
    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    test_assert(engine != NULL, "엔진 생성 실패");

    if (engine) {
        libetude_destroy_engine(engine);
    }
}

void test_invalid_arguments() {
    LibEtudeEngine* engine = libetude_create_engine(NULL);
    test_assert(engine == NULL, "NULL 경로로 엔진 생성이 성공해서는 안 됨");

    int result = libetude_synthesize_text(NULL, "test", NULL, NULL);
    test_assert(result != LIBETUDE_SUCCESS, "NULL 인수로 합성이 성공해서는 안 됨");
}

// 테스트 등록 및 실행
void setup_tests() {
    add_test("엔진 생성 테스트", test_engine_creation);
    add_test("잘못된 인수 테스트", test_invalid_arguments);
    // 더 많은 테스트 추가...
}
```

이 모범 사례 가이드는 LibEtude C API를 효과적으로 사용하기 위한 실용적인 방법들을 제공합니다. 실제 프로젝트에서 이러한 패턴들을 적용하여 안정적이고 성능이 우수한 음성 합성 애플리케이션을 개발하시기 바랍니다.