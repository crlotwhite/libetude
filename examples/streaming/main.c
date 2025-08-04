/**
 * @file main.c
 * @brief LibEtude 실시간 스트리밍 데모 애플리케이션
 *
 * 이 데모는 다음 기능을 제공합니다:
 * - 저지연 스트리밍 시연
 * - 실시간 파라미터 조정
 * - 다양한 플랫폼 지원
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "libetude/api.h"
#include "libetude/audio_io.h"
#include "libetude/performance_analyzer.h"
#include "libetude/profiler.h"
#include "libetude/error.h"
#include "libetude/task_scheduler.h"

// 상수 정의
#define MAX_TEXT_LENGTH 1024
#define MAX_AUDIO_BUFFER_SIZE 4096
#define STREAMING_CHUNK_SIZE 512
#define MAX_CONCURRENT_STREAMS 4
#define LATENCY_TARGET_MS 100

// 스트리밍 상태
typedef enum {
    STREAM_IDLE = 0,
    STREAM_ACTIVE = 1,
    STREAM_PAUSED = 2,
    STREAM_ERROR = 3
} StreamState;

// 실시간 파라미터 구조체
typedef struct {
    float pitch_scale;
    float speed_scale;
    float volume_scale;
    int quality_level;      // 0: 최고 품질, 1: 균형, 2: 최고 속도
    bool noise_reduction;
    bool echo_cancellation;
} StreamingParams;

// 오디오 청크 구조체
typedef struct AudioChunk {
    float* data;
    int num_samples;
    int sample_rate;
    double timestamp;
    struct AudioChunk* next;
} AudioChunk;

// 오디오 큐 구조체
typedef struct {
    AudioChunk* head;
    AudioChunk* tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int max_size;
} AudioQueue;

// 스트리밍 컨텍스트
typedef struct {
    LibEtudeEngine* engine;
    ETAudioDevice* audio_device;
    ETPerformanceAnalyzer* perf_analyzer;
    Profiler* profiler;
    ETTaskScheduler* scheduler;

    StreamState state;
    StreamingParams params;

    // 스레드 관리
    pthread_t synthesis_thread;
    pthread_t audio_thread;
    pthread_t control_thread;

    // 오디오 큐
    AudioQueue* audio_queue;

    // 통계
    struct {
        int total_chunks_processed;
        double total_synthesis_time;
        double total_audio_time;
        double avg_latency;
        double max_latency;
        double min_latency;
        int buffer_underruns;
        int buffer_overruns;
    } stats;

    // 제어 플래그
    volatile bool running;
    volatile bool synthesis_active;
    volatile bool audio_active;

    // 실시간 제어
    char pending_text[MAX_TEXT_LENGTH];
    pthread_mutex_t text_mutex;
    bool text_pending;

} StreamingContext;

// 전역 변수
static StreamingContext* g_streaming_ctx = NULL;
static volatile bool g_shutdown_requested = false;

// 시그널 핸들러
static void signal_handler(int sig) {
    printf("\n종료 신호 수신 (%d). 정리 중...\n", sig);
    g_shutdown_requested = true;
    if (g_streaming_ctx) {
        g_streaming_ctx->running = false;
    }
}

// 오디오 큐 생성
static AudioQueue* audio_queue_create(int max_size) {
    AudioQueue* queue = calloc(1, sizeof(AudioQueue));
    if (!queue) return NULL;

    queue->max_size = max_size;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    return queue;
}

// 오디오 큐 해제
static void audio_queue_destroy(AudioQueue* queue) {
    if (!queue) return;

    pthread_mutex_lock(&queue->mutex);

    // 모든 청크 해제
    AudioChunk* chunk = queue->head;
    while (chunk) {
        AudioChunk* next = chunk->next;
        free(chunk->data);
        free(chunk);
        chunk = next;
    }

    pthread_mutex_unlock(&queue->mutex);

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);

    free(queue);
}

// 오디오 청크 큐에 추가
static int audio_queue_push(AudioQueue* queue, AudioChunk* chunk) {
    pthread_mutex_lock(&queue->mutex);

    // 큐가 가득 찬 경우 대기
    while (queue->count >= queue->max_size) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    if (!queue->head) {
        queue->head = queue->tail = chunk;
    } else {
        queue->tail->next = chunk;
        queue->tail = chunk;
    }

    queue->count++;
    chunk->next = NULL;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

// 오디오 청크 큐에서 제거
static AudioChunk* audio_queue_pop(AudioQueue* queue) {
    pthread_mutex_lock(&queue->mutex);

    // 큐가 비어있는 경우 대기
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    AudioChunk* chunk = queue->head;
    queue->head = chunk->next;
    if (!queue->head) {
        queue->tail = NULL;
    }

    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return chunk;
}

// 기본 스트리밍 파라미터 초기화
static void init_streaming_params(StreamingParams* params) {
    params->pitch_scale = 1.0f;
    params->speed_scale = 1.0f;
    params->volume_scale = 1.0f;
    params->quality_level = 1;  // 균형 모드
    params->noise_reduction = true;
    params->echo_cancellation = false;
}

// 현재 시간 (밀리초)
static double get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// 음성 합성 스레드
static void* synthesis_thread_func(void* arg) {
    StreamingContext* ctx = (StreamingContext*)arg;
    char text_buffer[MAX_TEXT_LENGTH];

    printf("음성 합성 스레드 시작\n");

    while (ctx->running && ctx->synthesis_active) {
        // 대기 중인 텍스트 확인
        pthread_mutex_lock(&ctx->text_mutex);
        if (!ctx->text_pending) {
            pthread_mutex_unlock(&ctx->text_mutex);
            usleep(10000);  // 10ms 대기
            continue;
        }

        strcpy(text_buffer, ctx->pending_text);
        ctx->text_pending = false;
        pthread_mutex_unlock(&ctx->text_mutex);

        printf("음성 합성 시작: \"%s\"\n", text_buffer);

        double start_time = get_current_time_ms();

        // 프로파일링 시작
        if (ctx->profiler) {
            et_profiler_start_profile(ctx->profiler, "streaming_synthesis");
        }

        // 스트리밍 음성 합성 시작
        int result = libetude_start_streaming(ctx->engine, NULL, ctx);
        if (result != 0) {
            fprintf(stderr, "스트리밍 시작 실패: %d\n", result);
            continue;
        }

        // 텍스트를 청크 단위로 처리
        result = libetude_stream_text(ctx->engine, text_buffer);
        if (result != 0) {
            fprintf(stderr, "텍스트 스트리밍 실패: %d\n", result);
            libetude_stop_streaming(ctx->engine);
            continue;
        }

        // 스트리밍 완료 대기
        libetude_stop_streaming(ctx->engine);

        double end_time = get_current_time_ms();
        double synthesis_time = end_time - start_time;

        // 프로파일링 종료
        if (ctx->profiler) {
            et_profiler_end_profile(ctx->profiler, "streaming_synthesis");
        }

        // 통계 업데이트
        ctx->stats.total_chunks_processed++;
        ctx->stats.total_synthesis_time += synthesis_time;

        if (ctx->perf_analyzer) {
            et_performance_analyzer_record_inference(ctx->perf_analyzer, synthesis_time);
        }

        printf("음성 합성 완료 (%.2f ms)\n", synthesis_time);
    }

    printf("음성 합성 스레드 종료\n");
    return NULL;
}

// 오디오 출력 스레드
static void* audio_thread_func(void* arg) {
    StreamingContext* ctx = (StreamingContext*)arg;

    printf("오디오 출력 스레드 시작\n");

    while (ctx->running && ctx->audio_active) {
        AudioChunk* chunk = audio_queue_pop(ctx->audio_queue);
        if (!chunk) continue;

        double start_time = get_current_time_ms();

        // 오디오 출력 (실제 구현에서는 오디오 디바이스로 전송)
        // 여기서는 시뮬레이션을 위해 재생 시간만큼 대기
        double playback_duration = (double)chunk->num_samples / chunk->sample_rate * 1000.0;
        usleep((int)(playback_duration * 1000));

        double end_time = get_current_time_ms();
        double audio_time = end_time - start_time;

        // 지연 시간 계산
        double latency = start_time - chunk->timestamp;

        // 통계 업데이트
        ctx->stats.total_audio_time += audio_time;
        ctx->stats.avg_latency = (ctx->stats.avg_latency * (ctx->stats.total_chunks_processed - 1) + latency) / ctx->stats.total_chunks_processed;

        if (latency > ctx->stats.max_latency) {
            ctx->stats.max_latency = latency;
        }
        if (ctx->stats.min_latency == 0 || latency < ctx->stats.min_latency) {
            ctx->stats.min_latency = latency;
        }

        // 지연 시간 경고
        if (latency > LATENCY_TARGET_MS) {
            printf("경고: 높은 지연 시간 감지 (%.2f ms)\n", latency);
        }

        // 청크 해제
        free(chunk->data);
        free(chunk);
    }

    printf("오디오 출력 스레드 종료\n");
    return NULL;
}

// 제어 스레드 (사용자 입력 처리)
static void* control_thread_func(void* arg) {
    StreamingContext* ctx = (StreamingContext*)arg;
    char input[MAX_TEXT_LENGTH];

    printf("제어 스레드 시작\n");
    printf("명령어를 입력하세요 ('help'로 도움말 확인):\n");

    while (ctx->running) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        // 개행 문자 제거
        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) {
            continue;
        }

        // 명령어 처리
        if (strcmp(input, "help") == 0) {
            printf("\n=== 실시간 스트리밍 데모 명령어 ===\n");
            printf("help        - 이 도움말 표시\n");
            printf("start       - 스트리밍 시작\n");
            printf("stop        - 스트리밍 중지\n");
            printf("pause       - 스트리밍 일시정지\n");
            printf("resume      - 스트리밍 재개\n");
            printf("stats       - 성능 통계 표시\n");
            printf("params      - 현재 파라미터 표시\n");
            printf("pitch <값>  - 피치 스케일 조정 (0.5-2.0)\n");
            printf("speed <값>  - 속도 스케일 조정 (0.5-2.0)\n");
            printf("volume <값> - 볼륨 스케일 조정 (0.0-2.0)\n");
            printf("quality <값>- 품질 레벨 설정 (0-2)\n");
            printf("quit        - 프로그램 종료\n");
            printf("<텍스트>    - 텍스트를 음성으로 스트리밍\n");
            printf("\n");
        }
        else if (strcmp(input, "start") == 0) {
            if (ctx->state == STREAM_IDLE) {
                ctx->state = STREAM_ACTIVE;
                printf("스트리밍 시작됨\n");
            } else {
                printf("스트리밍이 이미 활성화되어 있습니다\n");
            }
        }
        else if (strcmp(input, "stop") == 0) {
            if (ctx->state == STREAM_ACTIVE || ctx->state == STREAM_PAUSED) {
                ctx->state = STREAM_IDLE;
                printf("스트리밍 중지됨\n");
            } else {
                printf("스트리밍이 활성화되어 있지 않습니다\n");
            }
        }
        else if (strcmp(input, "pause") == 0) {
            if (ctx->state == STREAM_ACTIVE) {
                ctx->state = STREAM_PAUSED;
                printf("스트리밍 일시정지됨\n");
            } else {
                printf("스트리밍이 활성화되어 있지 않습니다\n");
            }
        }
        else if (strcmp(input, "resume") == 0) {
            if (ctx->state == STREAM_PAUSED) {
                ctx->state = STREAM_ACTIVE;
                printf("스트리밍 재개됨\n");
            } else {
                printf("스트리밍이 일시정지되어 있지 않습니다\n");
            }
        }
        else if (strcmp(input, "stats") == 0) {
            printf("\n=== 스트리밍 성능 통계 ===\n");
            printf("처리된 청크 수: %d\n", ctx->stats.total_chunks_processed);
            printf("총 합성 시간: %.2f ms\n", ctx->stats.total_synthesis_time);
            printf("총 오디오 시간: %.2f ms\n", ctx->stats.total_audio_time);
            printf("평균 지연 시간: %.2f ms\n", ctx->stats.avg_latency);
            printf("최대 지연 시간: %.2f ms\n", ctx->stats.max_latency);
            printf("최소 지연 시간: %.2f ms\n", ctx->stats.min_latency);
            printf("버퍼 언더런: %d\n", ctx->stats.buffer_underruns);
            printf("버퍼 오버런: %d\n", ctx->stats.buffer_overruns);
            printf("목표 지연 시간: %d ms\n", LATENCY_TARGET_MS);
            printf("\n");
        }
        else if (strcmp(input, "params") == 0) {
            printf("\n=== 현재 스트리밍 파라미터 ===\n");
            printf("피치 스케일: %.2f\n", ctx->params.pitch_scale);
            printf("속도 스케일: %.2f\n", ctx->params.speed_scale);
            printf("볼륨 스케일: %.2f\n", ctx->params.volume_scale);
            printf("품질 레벨: %d ", ctx->params.quality_level);
            switch (ctx->params.quality_level) {
                case 0: printf("(최고 품질)\n"); break;
                case 1: printf("(균형)\n"); break;
                case 2: printf("(최고 속도)\n"); break;
            }
            printf("노이즈 감소: %s\n", ctx->params.noise_reduction ? "활성화" : "비활성화");
            printf("에코 제거: %s\n", ctx->params.echo_cancellation ? "활성화" : "비활성화");
            printf("\n");
        }
        else if (strncmp(input, "pitch ", 6) == 0) {
            float value;
            if (sscanf(input + 6, "%f", &value) == 1 && value >= 0.5f && value <= 2.0f) {
                ctx->params.pitch_scale = value;
                printf("피치 스케일을 %.2f로 설정했습니다\n", value);
            } else {
                printf("잘못된 피치 값입니다. 0.5-2.0 범위로 입력하세요\n");
            }
        }
        else if (strncmp(input, "speed ", 6) == 0) {
            float value;
            if (sscanf(input + 6, "%f", &value) == 1 && value >= 0.5f && value <= 2.0f) {
                ctx->params.speed_scale = value;
                printf("속도 스케일을 %.2f로 설정했습니다\n", value);
            } else {
                printf("잘못된 속도 값입니다. 0.5-2.0 범위로 입력하세요\n");
            }
        }
        else if (strncmp(input, "volume ", 7) == 0) {
            float value;
            if (sscanf(input + 7, "%f", &value) == 1 && value >= 0.0f && value <= 2.0f) {
                ctx->params.volume_scale = value;
                printf("볼륨 스케일을 %.2f로 설정했습니다\n", value);
            } else {
                printf("잘못된 볼륨 값입니다. 0.0-2.0 범위로 입력하세요\n");
            }
        }
        else if (strncmp(input, "quality ", 8) == 0) {
            int value;
            if (sscanf(input + 8, "%d", &value) == 1 && value >= 0 && value <= 2) {
                ctx->params.quality_level = value;
                const char* quality_names[] = {"최고 품질", "균형", "최고 속도"};
                printf("품질 레벨을 %d (%s)로 설정했습니다\n", value, quality_names[value]);
            } else {
                printf("잘못된 품질 값입니다. 0-2 범위로 입력하세요\n");
            }
        }
        else if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            ctx->running = false;
            break;
        }
        else {
            // 텍스트 스트리밍 요청
            if (ctx->state != STREAM_ACTIVE) {
                printf("스트리밍이 활성화되어 있지 않습니다. 'start' 명령어로 시작하세요\n");
                continue;
            }

            pthread_mutex_lock(&ctx->text_mutex);
            if (ctx->text_pending) {
                printf("이전 텍스트가 아직 처리 중입니다. 잠시 후 다시 시도하세요\n");
                pthread_mutex_unlock(&ctx->text_mutex);
                continue;
            }

            strcpy(ctx->pending_text, input);
            ctx->text_pending = true;
            pthread_mutex_unlock(&ctx->text_mutex);

            printf("텍스트 스트리밍 요청: \"%s\"\n", input);
        }
    }

    printf("제어 스레드 종료\n");
    return NULL;
}

// 스트리밍 오디오 콜백
static void streaming_audio_callback(const float* audio_data, int num_samples, void* user_data) {
    StreamingContext* ctx = (StreamingContext*)user_data;

    // 오디오 청크 생성
    AudioChunk* chunk = malloc(sizeof(AudioChunk));
    if (!chunk) return;

    chunk->data = malloc(num_samples * sizeof(float));
    if (!chunk->data) {
        free(chunk);
        return;
    }

    memcpy(chunk->data, audio_data, num_samples * sizeof(float));
    chunk->num_samples = num_samples;
    chunk->sample_rate = 22050;  // 기본 샘플 레이트
    chunk->timestamp = get_current_time_ms();
    chunk->next = NULL;

    // 볼륨 스케일 적용
    for (int i = 0; i < num_samples; i++) {
        chunk->data[i] *= ctx->params.volume_scale;
    }

    // 큐에 추가
    audio_queue_push(ctx->audio_queue, chunk);
}

// 스트리밍 컨텍스트 초기화
static int init_streaming_context(StreamingContext* ctx, const char* model_path) {
    printf("스트리밍 컨텍스트 초기화 중...\n");

    memset(ctx, 0, sizeof(StreamingContext));

    // 기본 파라미터 설정
    init_streaming_params(&ctx->params);
    ctx->state = STREAM_IDLE;
    ctx->running = true;
    ctx->synthesis_active = true;
    ctx->audio_active = true;

    // LibEtude 엔진 생성
    ctx->engine = libetude_create_engine(model_path);
    if (!ctx->engine) {
        fprintf(stderr, "TTS 엔진 생성 실패\n");
        return -1;
    }

    // 성능 분석기 초기화
    ctx->perf_analyzer = et_performance_analyzer_create();
    if (!ctx->perf_analyzer) {
        fprintf(stderr, "경고: 성능 분석기 초기화 실패\n");
    }

    // 프로파일러 초기화
    ctx->profiler = et_profiler_create(2000);
    if (!ctx->profiler) {
        fprintf(stderr, "경고: 프로파일러 초기화 실패\n");
    }

    // 작업 스케줄러 초기화
    ctx->scheduler = et_task_scheduler_create(100, 4);
    if (!ctx->scheduler) {
        fprintf(stderr, "경고: 작업 스케줄러 초기화 실패\n");
    }

    // 오디오 큐 생성
    ctx->audio_queue = audio_queue_create(20);  // 최대 20개 청크
    if (!ctx->audio_queue) {
        fprintf(stderr, "오디오 큐 생성 실패\n");
        return -1;
    }

    // 오디오 디바이스 초기화
    ETAudioFormat audio_format = {
        .sample_rate = 22050,
        .bit_depth = 16,
        .num_channels = 1,
        .frame_size = STREAMING_CHUNK_SIZE,
        .buffer_size = STREAMING_CHUNK_SIZE * 4
    };

    ctx->audio_device = et_audio_open_output_device(NULL, &audio_format);
    if (!ctx->audio_device) {
        fprintf(stderr, "경고: 오디오 디바이스 초기화 실패\n");
    }

    // 뮤텍스 초기화
    pthread_mutex_init(&ctx->text_mutex, NULL);

    // 통계 초기화
    ctx->stats.min_latency = 0;

    printf("스트리밍 컨텍스트 초기화 완료\n");
    return 0;
}

// 스트리밍 컨텍스트 정리
static void cleanup_streaming_context(StreamingContext* ctx) {
    printf("스트리밍 컨텍스트 정리 중...\n");

    // 스레드 종료 대기
    ctx->running = false;
    ctx->synthesis_active = false;
    ctx->audio_active = false;

    if (ctx->synthesis_thread) {
        pthread_join(ctx->synthesis_thread, NULL);
    }
    if (ctx->audio_thread) {
        pthread_join(ctx->audio_thread, NULL);
    }
    if (ctx->control_thread) {
        pthread_join(ctx->control_thread, NULL);
    }

    // 리소스 정리
    if (ctx->audio_device) {
        et_audio_close_device(ctx->audio_device);
    }

    if (ctx->audio_queue) {
        audio_queue_destroy(ctx->audio_queue);
    }

    // 성능 분석기, 프로파일러, 스케줄러는 엔진과 함께 해제됨

    if (ctx->engine) {
        libetude_destroy_engine(ctx->engine);
    }

    pthread_mutex_destroy(&ctx->text_mutex);

    printf("스트리밍 컨텍스트 정리 완료\n");
}

// 메인 함수
int main(int argc, char* argv[]) {
    StreamingContext streaming_ctx;
    const char* model_path = "models/default.lef";

    printf("=== LibEtude 실시간 스트리밍 데모 ===\n");
    printf("버전: 1.0.0\n");
    printf("빌드 날짜: %s %s\n", __DATE__, __TIME__);
    printf("목표 지연 시간: %d ms\n", LATENCY_TARGET_MS);
    printf("\n");

    // 시그널 핸들러 설정
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 명령행 인수 처리
    if (argc > 1) {
        model_path = argv[1];
    }

    printf("모델 경로: %s\n", model_path);

    // 스트리밍 컨텍스트 초기화
    if (init_streaming_context(&streaming_ctx, model_path) != 0) {
        fprintf(stderr, "스트리밍 컨텍스트 초기화 실패\n");
        return 1;
    }

    g_streaming_ctx = &streaming_ctx;

    // 스레드 생성
    if (pthread_create(&streaming_ctx.synthesis_thread, NULL, synthesis_thread_func, &streaming_ctx) != 0) {
        fprintf(stderr, "음성 합성 스레드 생성 실패\n");
        cleanup_streaming_context(&streaming_ctx);
        return 1;
    }

    if (pthread_create(&streaming_ctx.audio_thread, NULL, audio_thread_func, &streaming_ctx) != 0) {
        fprintf(stderr, "오디오 출력 스레드 생성 실패\n");
        cleanup_streaming_context(&streaming_ctx);
        return 1;
    }

    if (pthread_create(&streaming_ctx.control_thread, NULL, control_thread_func, &streaming_ctx) != 0) {
        fprintf(stderr, "제어 스레드 생성 실패\n");
        cleanup_streaming_context(&streaming_ctx);
        return 1;
    }

    printf("모든 스레드가 시작되었습니다.\n");
    printf("'help' 명령어로 사용법을 확인하세요.\n");
    printf("'start' 명령어로 스트리밍을 시작하세요.\n\n");

    // 메인 루프 (종료 대기)
    while (!g_shutdown_requested && streaming_ctx.running) {
        usleep(100000);  // 100ms 대기
    }

    printf("\n프로그램을 종료합니다.\n");

    // 최종 통계 출력
    printf("\n=== 최종 성능 통계 ===\n");
    printf("처리된 청크 수: %d\n", streaming_ctx.stats.total_chunks_processed);
    printf("총 합성 시간: %.2f ms\n", streaming_ctx.stats.total_synthesis_time);
    printf("평균 지연 시간: %.2f ms\n", streaming_ctx.stats.avg_latency);
    printf("최대 지연 시간: %.2f ms\n", streaming_ctx.stats.max_latency);
    printf("최소 지연 시간: %.2f ms\n", streaming_ctx.stats.min_latency);

    // 정리
    cleanup_streaming_context(&streaming_ctx);

    return 0;
}