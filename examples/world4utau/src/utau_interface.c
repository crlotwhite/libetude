/**
 * @file utau_interface.c
 * @brief UTAU 호환 인터페이스 구현
 */

#include "utau_interface.h"
#include <libetude/api.h>
#include <libetude/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
        return ET_ERROR_INVALID_ARGUMENT;
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
    if (!params) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 최소 인수 확인 (프로그램명 + 입력파일 + 출력파일)
    if (argc < 3) {
        fprintf(stderr, "Error: 최소 입력 파일과 출력 파일이 필요합니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 파라미터 초기화
    ETResult result = utau_parameters_init(params);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 기본 파라미터 파싱 (위치 기반)
    // argv[1]: 입력 WAV 파일
    params->input_wav_path = strdup(argv[1]);
    if (!params->input_wav_path) {
        fprintf(stderr, "Error: 메모리 할당 실패 (input_wav_path)\n");
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // argv[2]: 출력 WAV 파일
    params->output_wav_path = strdup(argv[2]);
    if (!params->output_wav_path) {
        fprintf(stderr, "Error: 메모리 할당 실패 (output_wav_path)\n");
        free(params->input_wav_path);
        params->input_wav_path = NULL;
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // argv[3]: 목표 피치 (선택적)
    if (argc >= 4) {
        char* endptr;
        double pitch = strtod(argv[3], &endptr);
        if (*endptr != '\0' || pitch <= 0.0) {
            fprintf(stderr, "Error: 유효하지 않은 피치 값: %s\n", argv[3]);
            utau_parameters_cleanup(params);
            return ET_ERROR_INVALID_ARGUMENT;
        }
        params->target_pitch = (float)pitch;
    }

    // argv[4]: 벨로시티 (선택적, UTAU는 0-100 범위)
    if (argc >= 5) {
        char* endptr;
        double velocity = strtod(argv[4], &endptr);
        if (*endptr != '\0' || velocity < 0.0 || velocity > 100.0) {
            fprintf(stderr, "Error: 유효하지 않은 벨로시티 값: %s (0-100 범위)\n", argv[4]);
            utau_parameters_cleanup(params);
            return ET_ERROR_INVALID_ARGUMENT;
        }
        params->velocity = (float)(velocity / 100.0);  // 0.0-1.0 범위로 변환
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

    // getopt 초기화 (기본 파라미터 이후부터 시작)
    // argc >= 5인 경우 5번째부터, 그렇지 않으면 현재 위치부터
    optind = (argc >= 5) ? 5 : 3;

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
                return ET_ERROR_INVALID_ARGUMENT;

            case 'w':
                print_version_info();
                utau_parameters_cleanup(params);
                return ET_ERROR_INVALID_ARGUMENT;

            default:
                utau_parameters_cleanup(params);
                return ET_ERROR_INVALID_ARGUMENT;
        }
    }

    params->owns_memory = true;
    return ET_SUCCESS;
}

bool validate_utau_parameters(const UTAUParameters* params) {
    if (!params) {
        fprintf(stderr, "Error: 파라미터 구조체가 NULL입니다.\n");
        return false;
    }

    // 파일 경로 유효성 검사
    if (!validate_file_paths(params->input_wav_path, params->output_wav_path)) {
        return false;
    }

    // 음성 파라미터 유효성 검사
    if (!validate_voice_parameters(params)) {
        return false;
    }

    // 오디오 설정 유효성 검사
    if (!validate_audio_settings(params)) {
        return false;
    }

    // 모든 검증 통과
    if (params->verbose_mode) {
        printf("Info: 모든 UTAU 파라미터 검증을 통과했습니다.\n");
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
    printf("world4utau (libetude integration) - UTAU 호환 음성 합성 엔진\n");
    printf("WORLD 보코더 알고리즘을 libetude 최적화로 구현\n\n");

    printf("사용법:\n");
    printf("  %s <input.wav> <output.wav> [target_pitch] [velocity] [options]\n\n", program_name);

    printf("필수 인수:\n");
    printf("  input.wav      입력 WAV 파일 경로\n");
    printf("  output.wav     출력 WAV 파일 경로\n\n");

    printf("선택적 인수:\n");
    printf("  target_pitch   목표 피치 (Hz, 기본값: 440.0)\n");
    printf("  velocity       벨로시티 (0-100, 기본값: 100)\n\n");

    printf("옵션:\n");
    printf("  -p, --pitch-bend FILE    피치 벤드 파일 경로 (텍스트 파일, 각 라인에 cents 값)\n");
    printf("  -v, --volume FLOAT       볼륨 (0.0-2.0, 기본값: 1.0)\n");
    printf("  -m, --modulation FLOAT   모듈레이션 강도 (0.0-1.0, 기본값: 0.0)\n");
    printf("  -c, --consonant INT      자음 벨로시티 (0-100, 기본값: 100)\n");
    printf("  -u, --pre-utterance FLOAT 선행발성 (ms, 기본값: 0.0)\n");
    printf("  -o, --overlap FLOAT      오버랩 (ms, 기본값: 0.0)\n");
    printf("  -s, --start-point FLOAT  시작점 (ms, 기본값: 0.0)\n");
    printf("  -r, --sample-rate INT    샘플링 레이트 (Hz, 기본값: 44100)\n");
    printf("  -b, --bit-depth INT      비트 깊이 (16/24/32, 기본값: 16)\n");
    printf("  -n, --no-cache           분석 결과 캐시 비활성화\n");
    printf("  -x, --no-optimization    SIMD/GPU 최적화 비활성화\n");
    printf("  -V, --verbose            상세 출력 모드 (처리 과정 표시)\n");
    printf("  -h, --help               이 도움말 출력\n");
    printf("  -w, --version            버전 정보 출력\n\n");

    printf("사용 예시:\n");
    printf("  기본 사용법:\n");
    printf("    %s voice.wav output.wav\n", program_name);
    printf("    %s voice.wav output.wav 440.0 100\n", program_name);
    printf("\n");
    printf("  피치 벤드 적용:\n");
    printf("    %s voice.wav output.wav 440.0 100 -p pitch_bend.txt\n", program_name);
    printf("\n");
    printf("  볼륨 및 모듈레이션 조정:\n");
    printf("    %s voice.wav output.wav 440.0 100 -v 0.8 -m 0.3\n", program_name);
    printf("\n");
    printf("  고품질 설정:\n");
    printf("    %s voice.wav output.wav 440.0 100 -r 48000 -b 24 -V\n", program_name);
    printf("\n");
    printf("  최적화 비활성화 (디버깅용):\n");
    printf("    %s voice.wav output.wav 440.0 100 -x -V\n", program_name);
    printf("\n");

    printf("피치 벤드 파일 형식:\n");
    printf("  각 라인에 cents 단위의 피치 변화량을 기록 (±2400 cents 범위)\n");
    printf("  예시:\n");
    printf("    0.0\n");
    printf("    +100.5\n");
    printf("    -50.2\n");
    printf("    +200.0\n\n");

    printf("지원 오디오 형식:\n");
    printf("  입력: WAV (PCM, 16/24/32비트)\n");
    printf("  출력: WAV (PCM, 지정된 비트 깊이)\n");
    printf("  샘플링 레이트: 8kHz - 192kHz\n\n");

    printf("성능 최적화:\n");
    printf("  - SIMD 최적화 (SSE/AVX/NEON)\n");
    printf("  - GPU 가속 (CUDA/OpenCL/Metal)\n");
    printf("  - 메모리 풀 최적화\n");
    printf("  - 분석 결과 캐싱\n\n");

    printf("호환성:\n");
    printf("  - OpenUTAU 호환\n");
    printf("  - 기존 world4utau 명령줄 호환\n");
    printf("  - 크로스 플랫폼 지원 (Windows/macOS/Linux)\n");
}

void print_version_info(void) {
    printf("world4utau (libetude integration) version %s\n", WORLD4UTAU_VERSION);
    printf("Built with libetude %s\n", libetude_get_version());
    printf("WORLD vocoder algorithm with libetude optimization\n");
    printf("Copyright (c) 2024 LibEtude Project\n");
    printf("\n");

    printf("컴포넌트 버전:\n");
    printf("  - world4utau: %s\n", WORLD4UTAU_VERSION);
    printf("  - libetude: %s\n", libetude_get_version());
    printf("  - WORLD 알고리즘: DIO/Harvest + CheapTrick + D4C\n");
    printf("\n");

    printf("지원 기능:\n");
    printf("  - SIMD 최적화 DSP 처리 (SSE/AVX/NEON)\n");
    printf("  - GPU 가속 지원 (CUDA/OpenCL/Metal)\n");
    printf("  - 크로스 플랫폼 오디오 I/O\n");
    printf("  - 메모리 풀 최적화\n");
    printf("  - 실시간 합성 기능\n");
    printf("  - 분석 결과 캐싱\n");
    printf("  - UTAU 완전 호환\n");
    printf("\n");

    printf("플랫폼 지원:\n");
    printf("  - Windows (x86/x64/ARM64)\n");
    printf("  - macOS (Intel/Apple Silicon)\n");
    printf("  - Linux (x86/x64/ARM/ARM64)\n");
    printf("  - Android (ARM/ARM64)\n");
    printf("  - iOS (ARM64)\n");
    printf("\n");

    printf("성능 특징:\n");
    printf("  - 실시간 처리 가능 (100ms 이내 목표)\n");
    printf("  - 메모리 효율적 설계\n");
    printf("  - 하드웨어 최적화 자동 감지\n");
    printf("  - 배치 처리 최적화\n");
    printf("\n");

    printf("라이선스: MIT License\n");
    printf("프로젝트 홈페이지: https://github.com/libetude/libetude\n");
    printf("문서: https://libetude.readthedocs.io/\n");
}

ETResult load_pitch_bend_file(const char* file_path, float** pitch_bend, int* length) {
    if (!file_path || !pitch_bend || !length) {
        fprintf(stderr, "Error: load_pitch_bend_file에 NULL 파라미터가 전달되었습니다.\n");
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *pitch_bend = NULL;
    *length = 0;

    FILE* file = fopen(file_path, "r");
    if (!file) {
        fprintf(stderr, "Error: 피치 벤드 파일을 열 수 없습니다: %s\n", file_path);
        return ET_ERROR_NOT_FOUND;
    }

    // 파일 크기 확인
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "Error: 피치 벤드 파일이 비어있습니다: %s\n", file_path);
        fclose(file);
        return ET_ERROR_IO;
    }

    // 임시 버퍼 할당 (파일 크기 + 널 종료자)
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: 피치 벤드 파일 읽기용 메모리 할당 실패\n");
        fclose(file);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 파일 읽기
    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);

    // 라인 수 계산 (더 정확한 방법)
    int line_count = 0;
    bool in_line = false;

    for (size_t i = 0; i < read_size; i++) {
        if (buffer[i] == '\n' || buffer[i] == '\r') {
            if (in_line) {
                line_count++;
                in_line = false;
            }
        } else if (!in_line && buffer[i] != ' ' && buffer[i] != '\t') {
            in_line = true;
        }
    }

    // 마지막 라인이 개행 문자로 끝나지 않는 경우
    if (in_line) {
        line_count++;
    }

    if (line_count == 0) {
        fprintf(stderr, "Error: 피치 벤드 파일에 유효한 데이터가 없습니다: %s\n", file_path);
        free(buffer);
        return ET_ERROR_IO;
    }

    // 피치 벤드 배열 할당
    *pitch_bend = malloc(line_count * sizeof(float));
    if (!*pitch_bend) {
        fprintf(stderr, "Error: 피치 벤드 데이터용 메모리 할당 실패\n");
        free(buffer);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 데이터 파싱 (strtok 대신 더 안전한 방법 사용)
    char* current_pos = buffer;
    int index = 0;

    while (current_pos && *current_pos && index < line_count) {
        // 공백 및 탭 건너뛰기
        while (*current_pos == ' ' || *current_pos == '\t') {
            current_pos++;
        }

        if (*current_pos == '\0') break;

        // 라인 끝 찾기
        char* line_end = current_pos;
        while (*line_end && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }

        // 임시로 널 종료자 삽입
        char saved_char = *line_end;
        *line_end = '\0';

        // 숫자 파싱 및 유효성 검사
        char* endptr;
        double value = strtod(current_pos, &endptr);

        if (endptr == current_pos) {
            // 숫자가 아닌 라인은 건너뛰기
            fprintf(stderr, "Warning: 피치 벤드 파일의 %d번째 라인을 파싱할 수 없습니다: %s\n",
                    index + 1, current_pos);
        } else {
            // 피치 벤드 값 범위 확인 (±2400 cents)
            if (value < -2400.0 || value > 2400.0) {
                fprintf(stderr, "Warning: 피치 벤드 값이 범위를 벗어났습니다 (라인 %d): %.2f cents\n",
                        index + 1, value);
                // 범위 내로 클램핑
                value = (value < -2400.0) ? -2400.0 : 2400.0;
            }
            (*pitch_bend)[index] = (float)value;
            index++;
        }

        // 원래 문자 복원
        *line_end = saved_char;

        // 다음 라인으로 이동
        current_pos = line_end;
        while (*current_pos == '\n' || *current_pos == '\r') {
            current_pos++;
        }
    }

    *length = index;
    free(buffer);

    if (index == 0) {
        fprintf(stderr, "Error: 피치 벤드 파일에서 유효한 숫자를 찾을 수 없습니다: %s\n", file_path);
        free(*pitch_bend);
        *pitch_bend = NULL;
        return ET_ERROR_IO;
    }

    printf("Info: 피치 벤드 파일에서 %d개의 값을 로드했습니다: %s\n", index, file_path);
    return ET_SUCCESS;
}

void debug_print_parameters(const UTAUParameters* params) {
    if (!params) {
        printf("=== UTAU Parameters Debug Info ===\n");
        printf("Parameters structure is NULL\n");
        printf("================================\n");
        return;
    }

    printf("=== UTAU Parameters Debug Info ===\n");
    printf("Input WAV: %s\n", params->input_wav_path ? params->input_wav_path : "(null)");
    printf("Output WAV: %s\n", params->output_wav_path ? params->output_wav_path : "(null)");
    printf("Target Pitch: %.2f Hz\n", params->target_pitch);
    printf("Velocity: %.2f (%.0f%%)\n", params->velocity, params->velocity * 100.0f);
    printf("Volume: %.2f\n", params->volume);
    printf("Modulation: %.2f\n", params->modulation);
    printf("Consonant Velocity: %.2f (%.0f%%)\n", params->consonant_velocity, params->consonant_velocity * 100.0f);
    printf("Pre-utterance: %.2f ms\n", params->pre_utterance);
    printf("Overlap: %.2f ms\n", params->overlap);
    printf("Start Point: %.2f ms\n", params->start_point);
    printf("Sample Rate: %d Hz\n", params->sample_rate);
    printf("Bit Depth: %d bits\n", params->bit_depth);
    printf("Pitch Bend Length: %d\n", params->pitch_bend_length);
    if (params->pitch_bend && params->pitch_bend_length > 0) {
        printf("Pitch Bend Range: %.2f to %.2f cents\n",
               params->pitch_bend[0], params->pitch_bend[params->pitch_bend_length - 1]);
    }
    printf("Cache Enabled: %s\n", params->enable_cache ? "Yes" : "No");
    printf("Optimization Enabled: %s\n", params->enable_optimization ? "Yes" : "No");
    printf("Verbose Mode: %s\n", params->verbose_mode ? "Yes" : "No");
    printf("Memory Ownership: %s\n", params->owns_memory ? "Yes" : "No");
    printf("================================\n");
}

bool validate_file_paths(const char* input_path, const char* output_path) {
    if (!input_path || strlen(input_path) == 0) {
        fprintf(stderr, "Error: 입력 파일 경로가 비어있습니다.\n");
        return false;
    }

    if (!output_path || strlen(output_path) == 0) {
        fprintf(stderr, "Error: 출력 파일 경로가 비어있습니다.\n");
        return false;
    }

    // 입력 파일 존재 여부 및 읽기 권한 확인
    FILE* input_file = fopen(input_path, "rb");
    if (!input_file) {
        fprintf(stderr, "Error: 입력 파일을 열 수 없습니다: %s\n", input_path);
        return false;
    }

    // 파일 크기 확인
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    fclose(input_file);

    if (file_size <= 44) {  // WAV 헤더 최소 크기
        fprintf(stderr, "Error: 입력 파일이 너무 작습니다 (WAV 파일이 아닐 수 있음): %s\n", input_path);
        return false;
    }

    // 출력 파일 쓰기 권한 확인
    FILE* output_file = fopen(output_path, "wb");
    if (!output_file) {
        fprintf(stderr, "Error: 출력 파일을 생성할 수 없습니다: %s\n", output_path);
        return false;
    }
    fclose(output_file);
    remove(output_path);  // 테스트 파일 삭제

    // 입력과 출력 파일이 같은지 확인
    if (strcmp(input_path, output_path) == 0) {
        fprintf(stderr, "Error: 입력 파일과 출력 파일이 동일합니다: %s\n", input_path);
        return false;
    }

    return true;
}

bool validate_voice_parameters(const UTAUParameters* params) {
    if (!params) {
        fprintf(stderr, "Error: 파라미터 구조체가 NULL입니다.\n");
        return false;
    }

    // 피치 범위 확인 (일반적인 음성 범위: 50Hz - 2000Hz)
    if (params->target_pitch <= 50.0f || params->target_pitch > 2000.0f) {
        fprintf(stderr, "Error: 목표 피치가 유효 범위를 벗어났습니다: %.2f Hz (50-2000 Hz 범위)\n",
                params->target_pitch);
        return false;
    }

    // 벨로시티 범위 확인 (0.0-1.0)
    if (params->velocity < 0.0f || params->velocity > 1.0f) {
        fprintf(stderr, "Error: 벨로시티가 유효 범위를 벗어났습니다: %.2f (0.0-1.0 범위)\n",
                params->velocity);
        return false;
    }

    // 볼륨 범위 확인 (0.0-2.0, 2.0은 200% 증폭)
    if (params->volume < 0.0f || params->volume > 2.0f) {
        fprintf(stderr, "Error: 볼륨이 유효 범위를 벗어났습니다: %.2f (0.0-2.0 범위)\n",
                params->volume);
        return false;
    }

    // 모듈레이션 범위 확인 (0.0-1.0)
    if (params->modulation < 0.0f || params->modulation > 1.0f) {
        fprintf(stderr, "Error: 모듈레이션이 유효 범위를 벗어났습니다: %.2f (0.0-1.0 범위)\n",
                params->modulation);
        return false;
    }

    // 자음 벨로시티 범위 확인 (0.0-1.0)
    if (params->consonant_velocity < 0.0f || params->consonant_velocity > 1.0f) {
        fprintf(stderr, "Error: 자음 벨로시티가 유효 범위를 벗어났습니다: %.2f (0.0-1.0 범위)\n",
                params->consonant_velocity);
        return false;
    }

    // 시간 관련 파라미터 확인 (음수 불허)
    if (params->pre_utterance < 0.0f) {
        fprintf(stderr, "Error: 선행발성이 음수입니다: %.2f ms\n", params->pre_utterance);
        return false;
    }

    if (params->overlap < 0.0f) {
        fprintf(stderr, "Error: 오버랩이 음수입니다: %.2f ms\n", params->overlap);
        return false;
    }

    if (params->start_point < 0.0f) {
        fprintf(stderr, "Error: 시작점이 음수입니다: %.2f ms\n", params->start_point);
        return false;
    }

    // 시간 파라미터 논리적 일관성 확인
    if (params->overlap > params->pre_utterance) {
        fprintf(stderr, "Warning: 오버랩(%.2f ms)이 선행발성(%.2f ms)보다 큽니다.\n",
                params->overlap, params->pre_utterance);
    }

    // 피치 벤드 데이터 유효성 확인
    if (params->pitch_bend && params->pitch_bend_length > 0) {
        for (int i = 0; i < params->pitch_bend_length; i++) {
            if (params->pitch_bend[i] < -2400.0f || params->pitch_bend[i] > 2400.0f) {
                fprintf(stderr, "Error: 피치 벤드 값이 유효 범위를 벗어났습니다 (인덱스 %d): %.2f cents (±2400 cents 범위)\n",
                        i, params->pitch_bend[i]);
                return false;
            }
        }
    }

    return true;
}

bool validate_audio_settings(const UTAUParameters* params) {
    if (!params) {
        fprintf(stderr, "Error: 파라미터 구조체가 NULL입니다.\n");
        return false;
    }

    // 샘플링 레이트 확인 (일반적인 범위: 8kHz - 192kHz)
    const int valid_sample_rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000};
    const int num_valid_rates = sizeof(valid_sample_rates) / sizeof(valid_sample_rates[0]);

    bool valid_rate = false;
    for (int i = 0; i < num_valid_rates; i++) {
        if (params->sample_rate == valid_sample_rates[i]) {
            valid_rate = true;
            break;
        }
    }

    if (!valid_rate) {
        fprintf(stderr, "Warning: 비표준 샘플링 레이트입니다: %d Hz\n", params->sample_rate);
        // 범위 확인
        if (params->sample_rate < 8000 || params->sample_rate > 192000) {
            fprintf(stderr, "Error: 샘플링 레이트가 지원 범위를 벗어났습니다: %d Hz (8000-192000 Hz 범위)\n",
                    params->sample_rate);
            return false;
        }
    }

    // 비트 깊이 확인 (16, 24, 32비트만 지원)
    if (params->bit_depth != 16 && params->bit_depth != 24 && params->bit_depth != 32) {
        fprintf(stderr, "Error: 지원하지 않는 비트 깊이입니다: %d (16, 24, 32비트만 지원)\n",
                params->bit_depth);
        return false;
    }

    // 샘플링 레이트와 비트 깊이 조합 확인
    if (params->sample_rate > 96000 && params->bit_depth < 24) {
        fprintf(stderr, "Warning: 높은 샘플링 레이트(%d Hz)에는 24비트 이상을 권장합니다 (현재: %d비트)\n",
                params->sample_rate, params->bit_depth);
    }

    return true;
}