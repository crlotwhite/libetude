/**
 * @file utau_interface.c
 * @brief UTAU 호환 인터페이스 구현
 */

#include "utau_interface.h"
#include <libetude/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// 기본값 정의
#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_BIT_DEPTH 16
#define DEFAULT_VELOCITY 1.0f
#define DEFAULT_VOLUME 1.0f
#define DEFAULT_MODULATION 0.0f
#define DEFAULT_CONSONANT_VELOCITY 1.0f
#define DEFAULT_PRE_UTTERANCE 0.0f
#define DEFAULT_OVERLAP 0.0f
#define DEFAULT_START_POINT 0.0f

ETResult utau_parameters_init(UTAUParameters* params) {
    if (!params) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 메모리 초기화
    memset(params, 0, sizeof(UTAUParameters));

    // 기본값 설정
    params->target_pitch = 440.0f;  // A4
    params->velocity = DEFAULT_VELOCITY;
    params->volume = DEFAULT_VOLUME;
    params->modulation = DEFAULT_MODULATION;
    params->consonant_velocity = DEFAULT_CONSONANT_VELOCITY;
    params->pre_utterance = DEFAULT_PRE_UTTERANCE;
    params->overlap = DEFAULT_OVERLAP;
    params->start_point = DEFAULT_START_POINT;
    params->sample_rate = DEFAULT_SAMPLE_RATE;
    params->bit_depth = DEFAULT_BIT_DEPTH;
    params->enable_cache = true;
    params->enable_optimization = true;
    params->verbose_mode = false;
    params->owns_memory = false;

    return ET_SUCCESS;
}

ETResult parse_utau_parameters(int argc, char* argv[], UTAUParameters* params) {
    if (!params || argc < 3) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    // 파라미터 초기화
    ETResult result = utau_parameters_init(params);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 기본 파라미터 (입력 파일, 출력 파일, 목표 피치)
    if (argc >= 2) {
        params->input_wav_path = strdup(argv[1]);
        if (!params->input_wav_path) {
            return ET_ERROR_OUT_OF_MEMORY;
        }
    }

    if (argc >= 3) {
        params->output_wav_path = strdup(argv[2]);
        if (!params->output_wav_path) {
            free(params->input_wav_path);
            return ET_ERROR_OUT_OF_MEMORY;
        }
    }

    if (argc >= 4) {
        params->target_pitch = atof(argv[3]);
    }

    if (argc >= 5) {
        params->velocity = atof(argv[4]) / 100.0f;  // UTAU는 0-100 범위
    }

    // 옵션 파라미터 파싱
    int opt;
    static struct option long_options[] = {
        {"pitch-bend", required_argument, 0, 'p'},
        {"volume", required_argument, 0, 'v'},
        {"modulation", required_argument, 0, 'm'},
        {"consonant", required_argument, 0, 'c'},
        {"pre-utterance", required_argument, 0, 'u'},
        {"overlap", required_argument, 0, 'o'},
        {"start-point", required_argument, 0, 's'},
        {"sample-rate", required_argument, 0, 'r'},
        {"bit-depth", required_argument, 0, 'b'},
        {"no-cache", no_argument, 0, 'n'},
        {"no-optimization", no_argument, 0, 'x'},
        {"verbose", no_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    // getopt 초기화
    optind = 5;  // 기본 파라미터 이후부터 시작

    while ((opt = getopt_long(argc, argv, "p:v:m:c:u:o:s:r:b:nxVhw", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                // 피치 벤드 파일 로드
                result = load_pitch_bend_file(optarg, &params->pitch_bend, &params->pitch_bend_length);
                if (result != ET_SUCCESS) {
                    utau_parameters_cleanup(params);
                    return result;
                }
                break;

            case 'v':
                params->volume = atof(optarg);
                break;

            case 'm':
                params->modulation = atof(optarg);
                break;

            case 'c':
                params->consonant_velocity = atof(optarg) / 100.0f;
                break;

            case 'u':
                params->pre_utterance = atof(optarg);
                break;

            case 'o':
                params->overlap = atof(optarg);
                break;

            case 's':
                params->start_point = atof(optarg);
                break;

            case 'r':
                params->sample_rate = atoi(optarg);
                break;

            case 'b':
                params->bit_depth = atoi(optarg);
                break;

            case 'n':
                params->enable_cache = false;
                break;

            case 'x':
                params->enable_optimization = false;
                break;

            case 'V':
                params->verbose_mode = true;
                break;

            case 'h':
                print_usage(argv[0]);
                utau_parameters_cleanup(params);
                return ET_ERROR_INVALID_PARAMETER;

            case 'w':
                print_version_info();
                utau_parameters_cleanup(params);
                return ET_ERROR_INVALID_PARAMETER;

            default:
                utau_parameters_cleanup(params);
                return ET_ERROR_INVALID_PARAMETER;
        }
    }

    params->owns_memory = true;
    return ET_SUCCESS;
}

bool validate_utau_parameters(const UTAUParameters* params) {
    if (!params) {
        return false;
    }

    // 필수 파라미터 확인
    if (!params->input_wav_path || !params->output_wav_path) {
        return false;
    }

    // 수치 범위 확인
    if (params->target_pitch <= 0.0f || params->target_pitch > 20000.0f) {
        return false;
    }

    if (params->velocity < 0.0f || params->velocity > 1.0f) {
        return false;
    }

    if (params->volume < 0.0f || params->volume > 2.0f) {
        return false;
    }

    if (params->modulation < 0.0f || params->modulation > 1.0f) {
        return false;
    }

    if (params->consonant_velocity < 0.0f || params->consonant_velocity > 1.0f) {
        return false;
    }

    if (params->sample_rate < 8000 || params->sample_rate > 192000) {
        return false;
    }

    if (params->bit_depth != 16 && params->bit_depth != 24 && params->bit_depth != 32) {
        return false;
    }

    return true;
}

void utau_parameters_cleanup(UTAUParameters* params) {
    if (!params) {
        return;
    }

    if (params->owns_memory) {
        free(params->input_wav_path);
        free(params->output_wav_path);
        free(params->pitch_bend);
    }

    memset(params, 0, sizeof(UTAUParameters));
}

void print_usage(const char* program_name) {
    printf("Usage: %s <input.wav> <output.wav> <target_pitch> <velocity> [options]\n\n", program_name);
    printf("Arguments:\n");
    printf("  input.wav      입력 WAV 파일 경로\n");
    printf("  output.wav     출력 WAV 파일 경로\n");
    printf("  target_pitch   목표 피치 (Hz)\n");
    printf("  velocity       벨로시티 (0-100)\n\n");
    printf("Options:\n");
    printf("  -p, --pitch-bend FILE    피치 벤드 파일 경로\n");
    printf("  -v, --volume FLOAT       볼륨 (0.0-2.0, 기본값: 1.0)\n");
    printf("  -m, --modulation FLOAT   모듈레이션 강도 (0.0-1.0, 기본값: 0.0)\n");
    printf("  -c, --consonant INT      자음 벨로시티 (0-100, 기본값: 100)\n");
    printf("  -u, --pre-utterance FLOAT 선행발성 (ms, 기본값: 0.0)\n");
    printf("  -o, --overlap FLOAT      오버랩 (ms, 기본값: 0.0)\n");
    printf("  -s, --start-point FLOAT  시작점 (ms, 기본값: 0.0)\n");
    printf("  -r, --sample-rate INT    샘플링 레이트 (Hz, 기본값: 44100)\n");
    printf("  -b, --bit-depth INT      비트 깊이 (16/24/32, 기본값: 16)\n");
    printf("  -n, --no-cache           캐시 비활성화\n");
    printf("  -x, --no-optimization    최적화 비활성화\n");
    printf("  -V, --verbose            상세 출력 모드\n");
    printf("  -h, --help               이 도움말 출력\n");
    printf("  -w, --version            버전 정보 출력\n\n");
    printf("Examples:\n");
    printf("  %s voice.wav output.wav 440.0 100\n", program_name);
    printf("  %s voice.wav output.wav 440.0 100 -p pitch.txt -v 0.8\n", program_name);
}

void print_version_info(void) {
    printf("world4utau (libetude integration) version %s\n", WORLD4UTAU_VERSION);
    printf("Built with libetude %s\n", et_get_version_string());
    printf("Copyright (c) 2024 LibEtude Project\n");
}

ETResult load_pitch_bend_file(const char* file_path, float** pitch_bend, int* length) {
    if (!file_path || !pitch_bend || !length) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    FILE* file = fopen(file_path, "r");
    if (!file) {
        return ET_ERROR_FILE_NOT_FOUND;
    }

    // 파일 크기 확인
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return ET_ERROR_INVALID_FILE;
    }

    // 임시 버퍼 할당
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 파일 읽기
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);

    // 라인 수 계산
    int line_count = 0;
    for (size_t i = 0; i < read_size; i++) {
        if (buffer[i] == '\n') {
            line_count++;
        }
    }

    if (line_count == 0) {
        free(buffer);
        return ET_ERROR_INVALID_FILE;
    }

    // 피치 벤드 배열 할당
    *pitch_bend = malloc(line_count * sizeof(float));
    if (!*pitch_bend) {
        free(buffer);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 데이터 파싱
    char* line = strtok(buffer, "\n");
    int index = 0;

    while (line && index < line_count) {
        (*pitch_bend)[index] = atof(line);
        line = strtok(NULL, "\n");
        index++;
    }

    *length = index;
    free(buffer);

    return ET_SUCCESS;
}

void debug_print_parameters(const UTAUParameters* params) {
    if (!params) {
        return;
    }

    printf("=== UTAU Parameters Debug Info ===\n");
    printf("Input WAV: %s\n", params->input_wav_path ? params->input_wav_path : "(null)");
    printf("Output WAV: %s\n", params->output_wav_path ? params->output_wav_path : "(null)");
    printf("Target Pitch: %.2f Hz\n", params->target_pitch);
    printf("Velocity: %.2f\n", params->velocity);
    printf("Volume: %.2f\n", params->volume);
    printf("Modulation: %.2f\n", params->modulation);
    printf("Consonant Velocity: %.2f\n", params->consonant_velocity);
    printf("Pre-utterance: %.2f ms\n", params->pre_utterance);
    printf("Overlap: %.2f ms\n", params->overlap);
    printf("Start Point: %.2f ms\n", params->start_point);
    printf("Sample Rate: %d Hz\n", params->sample_rate);
    printf("Bit Depth: %d bits\n", params->bit_depth);
    printf("Pitch Bend Length: %d\n", params->pitch_bend_length);
    printf("Cache Enabled: %s\n", params->enable_cache ? "Yes" : "No");
    printf("Optimization Enabled: %s\n", params->enable_optimization ? "Yes" : "No");
    printf("Verbose Mode: %s\n", params->verbose_mode ? "Yes" : "No");
    printf("================================\n");
}