/**
 * @file main.c
 * @brief LibEtude 기본 TTS 데모 애플리케이션
 *
 * 이 데모는 다음 기능을 제공합니다:
 * - 텍스트 입력 및 음성 출력
 * - 화자 선택 및 조정
 * - 성능 모니터링
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "libetude/api.h"
#include "libetude/audio_io.h"
#include "libetude/performance_analyzer.h"
#include "libetude/profiler.h"
#include "libetude/error.h"

// 최대 텍스트 길이
#define MAX_TEXT_LENGTH 1024
#define MAX_AUDIO_LENGTH 48000 * 10  // 10초 분량 (48kHz 기준)
#define MAX_SPEAKERS 10

// 화자 정보 구조체
typedef struct {
    int id;
    char name[64];
    char description[128];
    float pitch_scale;
    float speed_scale;
} SpeakerInfo;

// 애플리케이션 상태 구조체
typedef struct {
    LibEtudeEngine* engine;
    AudioDevice* audio_device;
    PerformanceAnalyzer* perf_analyzer;
    Profiler* profiler;

    SpeakerInfo speakers[MAX_SPEAKERS];
    int num_speakers;
    int current_speaker;

    bool monitoring_enabled;
    bool verbose_mode;
} TTSDemo;

// 기본 화자 정보 초기화
static void init_default_speakers(TTSDemo* demo) {
    demo->num_speakers = 4;

    // 화자 1: 기본 여성 음성
    demo->speakers[0] = (SpeakerInfo){
        .id = 0,
        .name = "여성 기본",
        .description = "표준 여성 음성 (중간 톤)",
        .pitch_scale = 1.0f,
        .speed_scale = 1.0f
    };

    // 화자 2: 기본 남성 음성
    demo->speakers[1] = (SpeakerInfo){
        .id = 1,
        .name = "남성 기본",
        .description = "표준 남성 음성 (낮은 톤)",
        .pitch_scale = 0.8f,
        .speed_scale = 1.0f
    };

    // 화자 3: 높은 톤 여성
    demo->speakers[2] = (SpeakerInfo){
        .id = 2,
        .name = "여성 높은톤",
        .description = "밝고 높은 톤의 여성 음성",
        .pitch_scale = 1.2f,
        .speed_scale = 1.1f
    };

    // 화자 4: 낮은 톤 남성
    demo->speakers[3] = (SpeakerInfo){
        .id = 3,
        .name = "남성 낮은톤",
        .description = "깊고 낮은 톤의 남성 음성",
        .pitch_scale = 0.7f,
        .speed_scale = 0.9f
    };

    demo->current_speaker = 0;
}

// 도움말 출력
static void print_help() {
    printf("\n=== LibEtude 기본 TTS 데모 ===\n");
    printf("사용법:\n");
    printf("  help     - 이 도움말 표시\n");
    printf("  speakers - 사용 가능한 화자 목록 표시\n");
    printf("  speaker <id> - 화자 선택 (0-3)\n");
    printf("  monitor  - 성능 모니터링 토글\n");
    printf("  verbose  - 상세 모드 토글\n");
    printf("  stats    - 현재 성능 통계 표시\n");
    printf("  quit     - 프로그램 종료\n");
    printf("  <텍스트> - 입력한 텍스트를 음성으로 변환\n");
    printf("\n예시:\n");
    printf("  > 안녕하세요, LibEtude입니다.\n");
    printf("  > speaker 1\n");
    printf("  > 이제 남성 음성으로 말합니다.\n");
    printf("\n");
}

// 화자 목록 출력
static void print_speakers(TTSDemo* demo) {
    printf("\n=== 사용 가능한 화자 ===\n");
    for (int i = 0; i < demo->num_speakers; i++) {
        char marker = (i == demo->current_speaker) ? '*' : ' ';
        printf("%c %d: %s\n", marker, i, demo->speakers[i].name);
        printf("     %s\n", demo->speakers[i].description);
        printf("     피치: %.1f, 속도: %.1f\n",
               demo->speakers[i].pitch_scale,
               demo->speakers[i].speed_scale);
    }
    printf("\n");
}

// 성능 통계 출력
static void print_performance_stats(TTSDemo* demo) {
    if (!demo->perf_analyzer) {
        printf("성능 분석기가 초기화되지 않았습니다.\n");
        return;
    }

    PerformanceStats stats;
    if (performance_analyzer_get_stats(demo->perf_analyzer, &stats) == 0) {
        printf("\n=== 성능 통계 ===\n");
        printf("총 추론 시간: %.2f ms\n", stats.total_inference_time_ms);
        printf("평균 추론 시간: %.2f ms\n", stats.avg_inference_time_ms);
        printf("최대 추론 시간: %.2f ms\n", stats.max_inference_time_ms);
        printf("최소 추론 시간: %.2f ms\n", stats.min_inference_time_ms);
        printf("총 처리된 요청: %d\n", stats.total_requests);
        printf("메모리 사용량: %.2f MB\n", stats.memory_usage_mb);
        printf("CPU 사용률: %.1f%%\n", stats.cpu_usage_percent);
        printf("GPU 사용률: %.1f%%\n", stats.gpu_usage_percent);
        printf("\n");
    } else {
        printf("성능 통계를 가져올 수 없습니다.\n");
    }
}

// 오디오 콜백 함수
static void audio_callback(const float* audio_data, int num_samples, void* user_data) {
    TTSDemo* demo = (TTSDemo*)user_data;

    if (demo->verbose_mode) {
        printf("오디오 출력: %d 샘플\n", num_samples);
    }

    // 실제 오디오 출력은 오디오 디바이스에서 처리됨
    // 여기서는 추가적인 후처리나 모니터링을 수행할 수 있음
}

// TTS 엔진 초기화
static int init_tts_engine(TTSDemo* demo, const char* model_path) {
    printf("TTS 엔진 초기화 중...\n");

    // LibEtude 엔진 생성
    demo->engine = libetude_create_engine(model_path);
    if (!demo->engine) {
        fprintf(stderr, "오류: TTS 엔진 생성 실패\n");
        return -1;
    }

    // 성능 분석기 초기화
    demo->perf_analyzer = performance_analyzer_create();
    if (!demo->perf_analyzer) {
        fprintf(stderr, "경고: 성능 분석기 초기화 실패\n");
    }

    // 프로파일러 초기화
    demo->profiler = profiler_create(1000);  // 최대 1000개 엔트리
    if (!demo->profiler) {
        fprintf(stderr, "경고: 프로파일러 초기화 실패\n");
    }

    // 오디오 디바이스 초기화
    AudioFormat audio_format = {
        .sample_rate = 22050,
        .bit_depth = 16,
        .num_channels = 1,
        .frame_size = 512,
        .buffer_size = 2048
    };

    demo->audio_device = audio_open_output_device(NULL, &audio_format);
    if (!demo->audio_device) {
        fprintf(stderr, "경고: 오디오 디바이스 초기화 실패\n");
    } else {
        audio_set_callback(demo->audio_device, audio_callback, demo);
        audio_start(demo->audio_device);
    }

    printf("TTS 엔진 초기화 완료\n");
    return 0;
}

// TTS 엔진 정리
static void cleanup_tts_engine(TTSDemo* demo) {
    printf("TTS 엔진 정리 중...\n");

    if (demo->audio_device) {
        audio_stop(demo->audio_device);
        audio_close_device(demo->audio_device);
        demo->audio_device = NULL;
    }

    if (demo->profiler) {
        profiler_destroy(demo->profiler);
        demo->profiler = NULL;
    }

    if (demo->perf_analyzer) {
        performance_analyzer_destroy(demo->perf_analyzer);
        demo->perf_analyzer = NULL;
    }

    if (demo->engine) {
        libetude_destroy_engine(demo->engine);
        demo->engine = NULL;
    }

    printf("TTS 엔진 정리 완료\n");
}

// 텍스트를 음성으로 변환
static int synthesize_text(TTSDemo* demo, const char* text) {
    if (!demo->engine) {
        fprintf(stderr, "오류: TTS 엔진이 초기화되지 않았습니다.\n");
        return -1;
    }

    printf("음성 합성 중: \"%s\"\n", text);

    // 성능 모니터링 시작
    clock_t start_time = clock();
    if (demo->profiler) {
        profiler_start_profile(demo->profiler, "text_synthesis");
    }

    // 현재 화자 설정 적용
    SpeakerInfo* speaker = &demo->speakers[demo->current_speaker];
    if (demo->verbose_mode) {
        printf("화자: %s (피치: %.1f, 속도: %.1f)\n",
               speaker->name, speaker->pitch_scale, speaker->speed_scale);
    }

    // 화자 파라미터 설정 (실제 구현에서는 엔진 API를 통해 설정)
    // libetude_set_speaker_params(demo->engine, speaker->pitch_scale, speaker->speed_scale);

    // 음성 합성 수행
    float audio_buffer[MAX_AUDIO_LENGTH];
    int audio_length = MAX_AUDIO_LENGTH;

    int result = libetude_synthesize_text(demo->engine, text, audio_buffer, &audio_length);

    // 성능 모니터링 종료
    clock_t end_time = clock();
    double synthesis_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;

    if (demo->profiler) {
        profiler_end_profile(demo->profiler, "text_synthesis");
    }

    if (result != 0) {
        fprintf(stderr, "오류: 음성 합성 실패 (코드: %d)\n", result);
        ETError* error = et_get_last_error();
        if (error) {
            fprintf(stderr, "상세 오류: %s\n", error->message);
        }
        return -1;
    }

    printf("음성 합성 완료 (%.2f ms, %d 샘플)\n", synthesis_time, audio_length);

    // 성능 통계 업데이트
    if (demo->perf_analyzer) {
        performance_analyzer_record_inference(demo->perf_analyzer, synthesis_time);
    }

    // 모니터링이 활성화된 경우 실시간 통계 출력
    if (demo->monitoring_enabled) {
        printf("  - 합성 시간: %.2f ms\n", synthesis_time);
        printf("  - 오디오 길이: %.2f 초\n", (float)audio_length / 22050.0f);
        printf("  - 실시간 비율: %.2fx\n",
               ((float)audio_length / 22050.0f) / (synthesis_time / 1000.0f));
    }

    // 오디오 출력 (실제 구현에서는 오디오 디바이스로 전송)
    printf("오디오 재생 중...\n");

    // 시뮬레이션을 위한 대기 (실제로는 오디오 재생 시간)
    float playback_duration = (float)audio_length / 22050.0f;
    usleep((int)(playback_duration * 1000000));  // 마이크로초 단위로 대기

    printf("재생 완료\n\n");

    return 0;
}

// 명령어 처리
static int process_command(TTSDemo* demo, const char* input) {
    char command[256];
    char args[768];

    // 입력을 명령어와 인수로 분리
    if (sscanf(input, "%255s %767[^\n]", command, args) < 1) {
        return 0;  // 빈 입력
    }

    if (strcmp(command, "help") == 0) {
        print_help();
    }
    else if (strcmp(command, "speakers") == 0) {
        print_speakers(demo);
    }
    else if (strcmp(command, "speaker") == 0) {
        int speaker_id;
        if (sscanf(args, "%d", &speaker_id) == 1) {
            if (speaker_id >= 0 && speaker_id < demo->num_speakers) {
                demo->current_speaker = speaker_id;
                printf("화자를 '%s'로 변경했습니다.\n",
                       demo->speakers[speaker_id].name);
            } else {
                printf("오류: 잘못된 화자 ID입니다. (0-%d 범위)\n",
                       demo->num_speakers - 1);
            }
        } else {
            printf("사용법: speaker <id>\n");
        }
    }
    else if (strcmp(command, "monitor") == 0) {
        demo->monitoring_enabled = !demo->monitoring_enabled;
        printf("성능 모니터링: %s\n",
               demo->monitoring_enabled ? "활성화" : "비활성화");
    }
    else if (strcmp(command, "verbose") == 0) {
        demo->verbose_mode = !demo->verbose_mode;
        printf("상세 모드: %s\n",
               demo->verbose_mode ? "활성화" : "비활성화");
    }
    else if (strcmp(command, "stats") == 0) {
        print_performance_stats(demo);
    }
    else if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0) {
        return -1;  // 종료 신호
    }
    else {
        // 명령어가 아닌 경우 텍스트로 간주하여 음성 합성
        synthesize_text(demo, input);
    }

    return 0;
}

// 메인 함수
int main(int argc, char* argv[]) {
    TTSDemo demo = {0};
    char input[MAX_TEXT_LENGTH];
    const char* model_path = "models/default.lef";

    printf("=== LibEtude 기본 TTS 데모 ===\n");
    printf("버전: 1.0.0\n");
    printf("빌드 날짜: %s %s\n", __DATE__, __TIME__);
    printf("\n");

    // 명령행 인수 처리
    if (argc > 1) {
        model_path = argv[1];
    }

    printf("모델 경로: %s\n", model_path);

    // 기본 화자 정보 초기화
    init_default_speakers(&demo);

    // TTS 엔진 초기화
    if (init_tts_engine(&demo, model_path) != 0) {
        fprintf(stderr, "TTS 엔진 초기화 실패\n");
        return 1;
    }

    // 초기 도움말 출력
    print_help();
    printf("현재 화자: %s\n", demo.speakers[demo.current_speaker].name);
    printf("성능 모니터링: %s\n", demo.monitoring_enabled ? "활성화" : "비활성화");
    printf("\n");

    // 메인 루프
    printf("명령어를 입력하세요 ('help'로 도움말 확인):\n");
    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;  // EOF 또는 오류
        }

        // 개행 문자 제거
        input[strcspn(input, "\n")] = 0;

        // 빈 입력 무시
        if (strlen(input) == 0) {
            continue;
        }

        // 명령어 처리
        if (process_command(&demo, input) < 0) {
            break;  // 종료 요청
        }
    }

    printf("\n프로그램을 종료합니다.\n");

    // 최종 성능 통계 출력
    if (demo.monitoring_enabled) {
        print_performance_stats(&demo);
    }

    // 정리
    cleanup_tts_engine(&demo);

    return 0;
}