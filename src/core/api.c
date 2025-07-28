/**
 * @file api.c
 * @brief LibEtude 핵심 C API 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * LibEtude의 외부 C API 구현을 제공합니다.
 * 엔진 생성/해제, 음성 합성, 스트리밍 등의 핵심 기능을 구현합니다.
 */

#include "libetude/api.h"
#include "libetude/error.h"
#include "libetude/memory.h"
#include "libetude/graph.h"
#include "libetude/lef_format.h"
#include "libetude/audio_io.h"
#include "libetude/profiler.h"
#include "libetude/task_scheduler.h"
#include "libetude/hardware.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 내부 구조체 정의
// ============================================================================

/**
 * @brief 스트리밍 상태 구조체
 */
typedef struct {
    bool active;                    /**< 스트리밍 활성 상태 */
    AudioStreamCallback callback;   /**< 오디오 콜백 함수 */
    void* user_data;               /**< 사용자 데이터 */
    pthread_t streaming_thread;     /**< 스트리밍 스레드 */
    pthread_mutex_t text_queue_mutex; /**< 텍스트 큐 뮤텍스 */
    char** text_queue;             /**< 텍스트 큐 */
    int queue_size;                /**< 큐 크기 */
    int queue_head;                /**< 큐 헤드 */
    int queue_tail;                /**< 큐 테일 */
    bool should_stop;              /**< 중지 플래그 */
} StreamingState;

/**
 * @brief LibEtude 엔진 내부 구조체
 */
struct LibEtudeEngine {
    // 모델 관련
    LEFModel* base_model;          /**< 기본 모델 */
    LEFModel** extensions;         /**< 확장 모델 배열 */
    int num_extensions;            /**< 확장 모델 수 */

    // 그래프 실행 엔진
    ETGraph* text_encoder;         /**< 텍스트 인코더 그래프 */
    ETGraph* duration_predictor;   /**< 지속시간 예측기 그래프 */
    ETGraph* pitch_predictor;      /**< 피치 예측기 그래프 */
    ETGraph* mel_decoder;          /**< Mel 디코더 그래프 */
    ETGraph* vocoder;              /**< 보코더 그래프 */

    // 메모리 관리
    ETMemoryPool* memory_pool;     /**< 메모리 풀 */

    // 런타임 시스템
    ETTaskScheduler* scheduler;      /**< 작업 스케줄러 */
    Profiler* profiler;           /**< 성능 프로파일러 */

    // 오디오 처리
    ETAudioDevice* audio_device;     /**< 오디오 디바이스 */
    LibEtudeAudioFormat audio_format; /**< 오디오 포맷 */

    // 설정
    QualityMode quality_mode;      /**< 품질 모드 */
    bool gpu_acceleration;         /**< GPU 가속 활성화 */
    uint32_t hardware_features;    /**< 하드웨어 기능 */

    // 스트리밍
    StreamingState streaming;      /**< 스트리밍 상태 */

    // 상태
    bool initialized;              /**< 초기화 상태 */
    pthread_mutex_t mutex;         /**< 엔진 뮤텍스 */
};

// ============================================================================
// 전역 변수
// ============================================================================

/** 마지막 오류 메시지 */
static char g_last_error[LIBETUDE_MAX_ERROR_MESSAGE_LEN] = {0};

// 로그 관련 전역 변수들은 init.c에서 관리됨

// ============================================================================
// 내부 함수 선언
// ============================================================================

static void set_last_error(const char* format, ...);
static void* streaming_thread_func(void* arg);
static int load_model_graphs(LibEtudeEngine* engine);
static int initialize_audio_system(LibEtudeEngine* engine);
static int process_text_to_audio(LibEtudeEngine* engine, const char* text,
                                 float* output_audio, int* output_length);

// ============================================================================
// 엔진 생성 및 관리
// ============================================================================

LIBETUDE_API LibEtudeEngine* libetude_create_engine(const char* model_path) {
    if (!model_path) {
        set_last_error("모델 경로가 NULL입니다");
        return NULL;
    }

    // 엔진 구조체 할당
    LibEtudeEngine* engine = (LibEtudeEngine*)calloc(1, sizeof(LibEtudeEngine));
    if (!engine) {
        set_last_error("엔진 메모리 할당 실패");
        return NULL;
    }

    // 뮤텍스 초기화
    if (pthread_mutex_init(&engine->mutex, NULL) != 0) {
        set_last_error("뮤텍스 초기화 실패");
        free(engine);
        return NULL;
    }

    // 스트리밍 뮤텍스 초기화
    if (pthread_mutex_init(&engine->streaming.text_queue_mutex, NULL) != 0) {
        set_last_error("스트리밍 뮤텍스 초기화 실패");
        pthread_mutex_destroy(&engine->mutex);
        free(engine);
        return NULL;
    }

    // 하드웨어 기능 감지
    engine->hardware_features = libetude_hardware_detect_simd_features();

    // 메모리 풀 생성
    engine->memory_pool = et_create_memory_pool(
        LIBETUDE_DEFAULT_MEMORY_POOL_SIZE_MB * 1024 * 1024, 32);
    if (!engine->memory_pool) {
        set_last_error("메모리 풀 생성 실패");
        pthread_mutex_destroy(&engine->streaming.text_queue_mutex);
        pthread_mutex_destroy(&engine->mutex);
        free(engine);
        return NULL;
    }

    // 작업 스케줄러 생성
    engine->scheduler = et_create_task_scheduler(4);
    if (!engine->scheduler) {
        set_last_error("작업 스케줄러 생성 실패");
        et_destroy_memory_pool(engine->memory_pool);
        pthread_mutex_destroy(&engine->streaming.text_queue_mutex);
        pthread_mutex_destroy(&engine->mutex);
        free(engine);
        return NULL;
    }

    // 프로파일러 생성
    engine->profiler = rt_create_profiler(1000);
    if (!engine->profiler) {
        set_last_error("프로파일러 생성 실패");
        et_destroy_task_scheduler(engine->scheduler);
        et_destroy_memory_pool(engine->memory_pool);
        pthread_mutex_destroy(&engine->streaming.text_queue_mutex);
        pthread_mutex_destroy(&engine->mutex);
        free(engine);
        return NULL;
    }

    // 모델 로드
    engine->base_model = lef_load_model(model_path);
    if (!engine->base_model) {
        set_last_error("모델 로드 실패: %s", model_path);
        rt_destroy_profiler(engine->profiler);
        et_destroy_task_scheduler(engine->scheduler);
        et_destroy_memory_pool(engine->memory_pool);
        pthread_mutex_destroy(&engine->streaming.text_queue_mutex);
        pthread_mutex_destroy(&engine->mutex);
        free(engine);
        return NULL;
    }

    // 모델 그래프 로드
    if (load_model_graphs(engine) != LIBETUDE_SUCCESS) {
        lef_unload_model(engine->base_model);
        rt_destroy_profiler(engine->profiler);
        et_destroy_task_scheduler(engine->scheduler);
        et_destroy_memory_pool(engine->memory_pool);
        pthread_mutex_destroy(&engine->streaming.text_queue_mutex);
        pthread_mutex_destroy(&engine->mutex);
        free(engine);
        return NULL;
    }

    // 오디오 시스템 초기화
    if (initialize_audio_system(engine) != LIBETUDE_SUCCESS) {
        // 그래프 정리는 여기서 추가해야 함
        lef_unload_model(engine->base_model);
        rt_destroy_profiler(engine->profiler);
        et_destroy_task_scheduler(engine->scheduler);
        et_destroy_memory_pool(engine->memory_pool);
        pthread_mutex_destroy(&engine->streaming.text_queue_mutex);
        pthread_mutex_destroy(&engine->mutex);
        free(engine);
        return NULL;
    }

    // 기본 설정
    engine->quality_mode = LIBETUDE_QUALITY_BALANCED;
    engine->gpu_acceleration = false;
    engine->initialized = true;

    // 스트리밍 큐 초기화
    engine->streaming.queue_size = 32;
    engine->streaming.text_queue = (char**)calloc(engine->streaming.queue_size, sizeof(char*));
    if (!engine->streaming.text_queue) {
        set_last_error("스트리밍 큐 할당 실패");
        libetude_destroy_engine(engine);
        return NULL;
    }

    return engine;
}

LIBETUDE_API void libetude_destroy_engine(LibEtudeEngine* engine) {
    if (!engine) {
        return;
    }

    pthread_mutex_lock(&engine->mutex);

    // 스트리밍 중지
    if (engine->streaming.active) {
        libetude_stop_streaming(engine);
    }

    // 스트리밍 큐 정리
    if (engine->streaming.text_queue) {
        for (int i = 0; i < engine->streaming.queue_size; i++) {
            free(engine->streaming.text_queue[i]);
        }
        free(engine->streaming.text_queue);
    }

    // 확장 모델 정리
    if (engine->extensions) {
        for (int i = 0; i < engine->num_extensions; i++) {
            if (engine->extensions[i]) {
                lef_unload_model(engine->extensions[i]);
            }
        }
        free(engine->extensions);
    }

    // 그래프 정리
    if (engine->text_encoder) et_destroy_graph(engine->text_encoder);
    if (engine->duration_predictor) et_destroy_graph(engine->duration_predictor);
    if (engine->pitch_predictor) et_destroy_graph(engine->pitch_predictor);
    if (engine->mel_decoder) et_destroy_graph(engine->mel_decoder);
    if (engine->vocoder) et_destroy_graph(engine->vocoder);

    // 기본 모델 정리
    if (engine->base_model) {
        lef_unload_model(engine->base_model);
    }

    // 오디오 디바이스 정리
    if (engine->audio_device) {
        et_audio_close_device(engine->audio_device);
    }

    // 런타임 시스템 정리
    if (engine->profiler) {
        rt_destroy_profiler(engine->profiler);
    }

    if (engine->scheduler) {
        et_destroy_task_scheduler(engine->scheduler);
    }

    // 메모리 풀 정리
    if (engine->memory_pool) {
        et_destroy_memory_pool(engine->memory_pool);
    }

    pthread_mutex_unlock(&engine->mutex);

    // 뮤텍스 정리
    pthread_mutex_destroy(&engine->streaming.text_queue_mutex);
    pthread_mutex_destroy(&engine->mutex);

    // 엔진 구조체 해제
    free(engine);
}

// ============================================================================
// 음성 합성 (동기 처리)
// ============================================================================

LIBETUDE_API int libetude_synthesize_text(LibEtudeEngine* engine,
                                          const char* text,
                                          float* output_audio,
                                          int* output_length) {
    LIBETUDE_CHECK_PTR(engine);
    LIBETUDE_CHECK_PTR(text);
    LIBETUDE_CHECK_PTR(output_audio);
    LIBETUDE_CHECK_PTR(output_length);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (strlen(text) > LIBETUDE_MAX_TEXT_LENGTH) {
        set_last_error("텍스트가 너무 깁니다 (최대 %d 문자)", LIBETUDE_MAX_TEXT_LENGTH);
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&engine->mutex);

    // 프로파일링 시작
    rt_start_profile(engine->profiler, "synthesize_text");

    int result = process_text_to_audio(engine, text, output_audio, output_length);

    // 프로파일링 종료
    rt_end_profile(engine->profiler, "synthesize_text");

    pthread_mutex_unlock(&engine->mutex);

    return result;
}

LIBETUDE_API int libetude_synthesize_singing(LibEtudeEngine* engine,
                                             const char* lyrics,
                                             const float* notes,
                                             int note_count,
                                             float* output_audio,
                                             int* output_length) {
    LIBETUDE_CHECK_PTR(engine);
    LIBETUDE_CHECK_PTR(lyrics);
    LIBETUDE_CHECK_PTR(notes);
    LIBETUDE_CHECK_PTR(output_audio);
    LIBETUDE_CHECK_PTR(output_length);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (note_count <= 0) {
        set_last_error("음표 개수가 유효하지 않습니다");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 현재는 기본 텍스트 합성으로 처리 (향후 노래 합성 로직 추가)
    set_last_error("노래 합성 기능은 아직 구현되지 않았습니다");
    return LIBETUDE_ERROR_NOT_IMPLEMENTED;
}

// ============================================================================
// 실시간 스트리밍 (비동기 처리)
// ============================================================================

LIBETUDE_API int libetude_start_streaming(LibEtudeEngine* engine,
                                          AudioStreamCallback callback,
                                          void* user_data) {
    LIBETUDE_CHECK_PTR(engine);
    LIBETUDE_CHECK_PTR(callback);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&engine->mutex);

    if (engine->streaming.active) {
        set_last_error("스트리밍이 이미 활성화되어 있습니다");
        pthread_mutex_unlock(&engine->mutex);
        return LIBETUDE_ERROR_ALREADY_INITIALIZED;
    }

    // 스트리밍 상태 초기화
    engine->streaming.active = true;
    engine->streaming.callback = callback;
    engine->streaming.user_data = user_data;
    engine->streaming.queue_head = 0;
    engine->streaming.queue_tail = 0;
    engine->streaming.should_stop = false;

    // 스트리밍 스레드 시작
    if (pthread_create(&engine->streaming.streaming_thread, NULL,
                      streaming_thread_func, engine) != 0) {
        set_last_error("스트리밍 스레드 생성 실패");
        engine->streaming.active = false;
        pthread_mutex_unlock(&engine->mutex);
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_unlock(&engine->mutex);
    return LIBETUDE_SUCCESS;
}

LIBETUDE_API int libetude_stream_text(LibEtudeEngine* engine, const char* text) {
    LIBETUDE_CHECK_PTR(engine);
    LIBETUDE_CHECK_PTR(text);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (!engine->streaming.active) {
        set_last_error("스트리밍이 활성화되지 않았습니다");
        return LIBETUDE_ERROR_INVALID_STATE;
    }

    if (strlen(text) > LIBETUDE_MAX_TEXT_LENGTH) {
        set_last_error("텍스트가 너무 깁니다 (최대 %d 문자)", LIBETUDE_MAX_TEXT_LENGTH);
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&engine->streaming.text_queue_mutex);

    // 큐가 가득 찬지 확인
    int next_tail = (engine->streaming.queue_tail + 1) % engine->streaming.queue_size;
    if (next_tail == engine->streaming.queue_head) {
        set_last_error("텍스트 큐가 가득 참");
        pthread_mutex_unlock(&engine->streaming.text_queue_mutex);
        return LIBETUDE_ERROR_BUFFER_FULL;
    }

    // 텍스트 복사
    engine->streaming.text_queue[engine->streaming.queue_tail] = strdup(text);
    if (!engine->streaming.text_queue[engine->streaming.queue_tail]) {
        set_last_error("텍스트 복사 실패");
        pthread_mutex_unlock(&engine->streaming.text_queue_mutex);
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    engine->streaming.queue_tail = next_tail;

    pthread_mutex_unlock(&engine->streaming.text_queue_mutex);
    return LIBETUDE_SUCCESS;
}

LIBETUDE_API int libetude_stop_streaming(LibEtudeEngine* engine) {
    LIBETUDE_CHECK_PTR(engine);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (!engine->streaming.active) {
        return LIBETUDE_SUCCESS; // 이미 중지됨
    }

    // 스트리밍 중지 신호
    engine->streaming.should_stop = true;

    // 스트리밍 스레드 종료 대기
    if (pthread_join(engine->streaming.streaming_thread, NULL) != 0) {
        set_last_error("스트리밍 스레드 종료 대기 실패");
        return LIBETUDE_ERROR_RUNTIME;
    }

    pthread_mutex_lock(&engine->streaming.text_queue_mutex);

    // 큐 정리
    while (engine->streaming.queue_head != engine->streaming.queue_tail) {
        free(engine->streaming.text_queue[engine->streaming.queue_head]);
        engine->streaming.text_queue[engine->streaming.queue_head] = NULL;
        engine->streaming.queue_head = (engine->streaming.queue_head + 1) % engine->streaming.queue_size;
    }

    engine->streaming.active = false;

    pthread_mutex_unlock(&engine->streaming.text_queue_mutex);

    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 성능 제어 및 모니터링
// ============================================================================

LIBETUDE_API int libetude_set_quality_mode(LibEtudeEngine* engine, QualityMode quality_mode) {
    LIBETUDE_CHECK_PTR(engine);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (quality_mode < LIBETUDE_QUALITY_FAST || quality_mode > LIBETUDE_QUALITY_HIGH) {
        set_last_error("유효하지 않은 품질 모드");
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&engine->mutex);
    engine->quality_mode = quality_mode;
    pthread_mutex_unlock(&engine->mutex);

    return LIBETUDE_SUCCESS;
}

LIBETUDE_API int libetude_enable_gpu_acceleration(LibEtudeEngine* engine) {
    LIBETUDE_CHECK_PTR(engine);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

#ifdef LIBETUDE_DISABLE_GPU_ACCELERATION
    set_last_error("GPU 가속이 비활성화되어 컴파일되었습니다");
    return LIBETUDE_ERROR_UNSUPPORTED;
#else
    pthread_mutex_lock(&engine->mutex);

    // GPU 가속 활성화 (현재는 더미 구현)
    // 실제 구현에서는 GPU 컨텍스트 초기화가 필요함
    LibEtudeHardwareGPUInfo gpu_info;
    if (libetude_hardware_detect_gpu(&gpu_info) != LIBETUDE_SUCCESS || !gpu_info.available) {
        set_last_error("사용 가능한 GPU가 없습니다");
        pthread_mutex_unlock(&engine->mutex);
        return LIBETUDE_ERROR_HARDWARE;
    }

    engine->gpu_acceleration = true;

    pthread_mutex_unlock(&engine->mutex);
    return LIBETUDE_SUCCESS;
#endif
}

LIBETUDE_API int libetude_get_performance_stats(LibEtudeEngine* engine, PerformanceStats* stats) {
    LIBETUDE_CHECK_PTR(engine);
    LIBETUDE_CHECK_PTR(stats);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&engine->mutex);

    // 프로파일러에서 통계 수집
    if (engine->profiler) {
        // 기본값 설정
        stats->inference_time_ms = 0.0;
        stats->memory_usage_mb = 0.0;
        stats->cpu_usage_percent = 0.0;
        stats->gpu_usage_percent = 0.0;
        stats->active_threads = 1;

        // 실제 통계는 프로파일러에서 가져와야 함 (향후 구현)
        // 현재는 더미 값 반환
    } else {
        memset(stats, 0, sizeof(PerformanceStats));
    }

    pthread_mutex_unlock(&engine->mutex);
    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 확장 모델 관리
// ============================================================================

LIBETUDE_API int libetude_load_extension(LibEtudeEngine* engine, const char* extension_path) {
    LIBETUDE_CHECK_PTR(engine);
    LIBETUDE_CHECK_PTR(extension_path);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&engine->mutex);

    // 확장 모델 로드
    LEFModel* extension = lef_load_model(extension_path);
    if (!extension) {
        set_last_error("확장 모델 로드 실패: %s", extension_path);
        pthread_mutex_unlock(&engine->mutex);
        return LIBETUDE_ERROR_MODEL;
    }

    // 확장 모델 배열 확장
    LEFModel** new_extensions = (LEFModel**)realloc(engine->extensions,
        (engine->num_extensions + 1) * sizeof(LEFModel*));
    if (!new_extensions) {
        set_last_error("확장 모델 배열 확장 실패");
        lef_unload_model(extension);
        pthread_mutex_unlock(&engine->mutex);
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    engine->extensions = new_extensions;
    engine->extensions[engine->num_extensions] = extension;
    engine->num_extensions++;

    pthread_mutex_unlock(&engine->mutex);
    return LIBETUDE_SUCCESS;
}

LIBETUDE_API int libetude_unload_extension(LibEtudeEngine* engine, int extension_id) {
    LIBETUDE_CHECK_PTR(engine);

    if (!engine->initialized) {
        set_last_error("엔진이 초기화되지 않았습니다");
        return LIBETUDE_ERROR_NOT_INITIALIZED;
    }

    if (extension_id < 0 || extension_id >= engine->num_extensions) {
        set_last_error("유효하지 않은 확장 모델 ID: %d", extension_id);
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&engine->mutex);

    // 확장 모델 언로드
    lef_unload_model(engine->extensions[extension_id]);

    // 배열에서 제거
    for (int i = extension_id; i < engine->num_extensions - 1; i++) {
        engine->extensions[i] = engine->extensions[i + 1];
    }
    engine->num_extensions--;

    // 배열 크기 조정
    if (engine->num_extensions > 0) {
        LEFModel** new_extensions = (LEFModel**)realloc(engine->extensions,
            engine->num_extensions * sizeof(LEFModel*));
        if (new_extensions) {
            engine->extensions = new_extensions;
        }
    } else {
        free(engine->extensions);
        engine->extensions = NULL;
    }

    pthread_mutex_unlock(&engine->mutex);
    return LIBETUDE_SUCCESS;
}

// ============================================================================
// 유틸리티 함수
// ============================================================================

LIBETUDE_API const char* libetude_get_version() {
    return LIBETUDE_VERSION_STRING;
}

LIBETUDE_API uint32_t libetude_get_hardware_features() {
    return libetude_hardware_detect_simd_features();
}

LIBETUDE_API const char* libetude_get_last_error() {
    return g_last_error;
}

// ============================================================================
// 로그 관련 함수
// ============================================================================

// 로그 관련 함수들은 init.c에서 구현됨

// ============================================================================
// 내부 함수 구현
// ============================================================================

/**
 * @brief 마지막 오류 메시지 설정
 */
static void set_last_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_last_error, sizeof(g_last_error), format, args);
    va_end(args);

    // 오류 로그 출력
    libetude_log(LIBETUDE_LOG_ERROR, "%s", g_last_error);
}

/**
 * @brief 스트리밍 스레드 함수
 */
static void* streaming_thread_func(void* arg) {
    LibEtudeEngine* engine = (LibEtudeEngine*)arg;
    float audio_buffer[LIBETUDE_DEFAULT_AUDIO_BUFFER_SIZE];

    while (!engine->streaming.should_stop) {
        char* text = NULL;

        // 큐에서 텍스트 가져오기
        pthread_mutex_lock(&engine->streaming.text_queue_mutex);
        if (engine->streaming.queue_head != engine->streaming.queue_tail) {
            text = engine->streaming.text_queue[engine->streaming.queue_head];
            engine->streaming.text_queue[engine->streaming.queue_head] = NULL;
            engine->streaming.queue_head = (engine->streaming.queue_head + 1) % engine->streaming.queue_size;
        }
        pthread_mutex_unlock(&engine->streaming.text_queue_mutex);

        if (text) {
            // 텍스트를 오디오로 변환
            int audio_length = LIBETUDE_DEFAULT_AUDIO_BUFFER_SIZE;
            if (process_text_to_audio(engine, text, audio_buffer, &audio_length) == LIBETUDE_SUCCESS) {
                // 콜백 호출
                if (engine->streaming.callback) {
                    engine->streaming.callback(audio_buffer, audio_length, engine->streaming.user_data);
                }
            }

            free(text);
        } else {
            // 큐가 비어있으면 잠시 대기
            usleep(10000); // 10ms 대기
        }
    }

    return NULL;
}

/**
 * @brief 모델 그래프 로드
 */
static int load_model_graphs(LibEtudeEngine* engine) {
    // 실제 구현에서는 LEF 모델에서 그래프를 추출해야 함
    // 현재는 더미 그래프 생성

    engine->text_encoder = et_create_graph(16);
    if (!engine->text_encoder) {
        set_last_error("텍스트 인코더 그래프 생성 실패");
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    engine->duration_predictor = et_create_graph(8);
    if (!engine->duration_predictor) {
        set_last_error("지속시간 예측기 그래프 생성 실패");
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    engine->pitch_predictor = et_create_graph(8);
    if (!engine->pitch_predictor) {
        set_last_error("피치 예측기 그래프 생성 실패");
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    engine->mel_decoder = et_create_graph(32);
    if (!engine->mel_decoder) {
        set_last_error("Mel 디코더 그래프 생성 실패");
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    engine->vocoder = et_create_graph(16);
    if (!engine->vocoder) {
        set_last_error("보코더 그래프 생성 실패");
        return LIBETUDE_ERROR_OUT_OF_MEMORY;
    }

    return LIBETUDE_SUCCESS;
}

/**
 * @brief 오디오 시스템 초기화
 */
static int initialize_audio_system(LibEtudeEngine* engine) {
    // 기본 오디오 포맷 설정
    engine->audio_format.sample_rate = LIBETUDE_DEFAULT_SAMPLE_RATE;
    engine->audio_format.bit_depth = 32; // float32
    engine->audio_format.num_channels = 1; // 모노
    engine->audio_format.frame_size = sizeof(float);
    engine->audio_format.buffer_size = LIBETUDE_DEFAULT_AUDIO_BUFFER_SIZE;

    // 오디오 디바이스는 스트리밍 시에만 필요하므로 여기서는 초기화하지 않음

    return LIBETUDE_SUCCESS;
}

/**
 * @brief 텍스트를 오디오로 변환하는 핵심 처리 함수
 */
static int process_text_to_audio(LibEtudeEngine* engine, const char* text,
                                 float* output_audio, int* output_length) {
    if (!engine || !text || !output_audio || !output_length) {
        return LIBETUDE_ERROR_INVALID_ARGUMENT;
    }

    // 현재는 더미 구현 - 실제로는 다음 단계를 거쳐야 함:
    // 1. 텍스트 전처리 및 토큰화
    // 2. 텍스트 인코더 실행
    // 3. 지속시간 및 피치 예측
    // 4. Mel 스펙트로그램 생성
    // 5. 보코더를 통한 오디오 생성

    // 더미 오디오 생성 (사인파)
    int max_length = *output_length;
    int actual_length = LIBETUDE_MIN(max_length, 1000); // 최대 1000 샘플

    for (int i = 0; i < actual_length; i++) {
        output_audio[i] = 0.1f * sinf(2.0f * M_PI * 440.0f * i / engine->audio_format.sample_rate);
    }

    *output_length = actual_length;

    return LIBETUDE_SUCCESS;
}