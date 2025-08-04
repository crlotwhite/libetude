/**
 * @file test_utau_parameter_parsing.c
 * @brief UTAU 파라미터 파싱 및 검증 단위 테스트
 *
 * world4utau-example의 UTAU 파라미터 파싱 기능을 테스트합니다.
 * 다양한 파라미터 조합과 에러 케이스를 검증합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>

// world4utau 인터페이스 포함
#include "../../examples/world4utau/include/utau_interface.h"

// 테스트 헬퍼 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 0; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "FAIL: %s (expected: %d, actual: %d)\n", message, (int)(expected), (int)(actual)); \
            return 0; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_EQ(expected, actual, tolerance, message) \
    do { \
        float diff = (expected) - (actual); \
        if (diff < 0) diff = -diff; \
        if (diff > (tolerance)) { \
            fprintf(stderr, "FAIL: %s (expected: %.3f, actual: %.3f)\n", message, (expected), (actual)); \
            return 0; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

// 테스트용 임시 파일 생성
static void create_test_wav_file(const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (file) {
        // 최소한의 WAV 헤더 작성 (44바이트)
        char wav_header[44] = {
            'R', 'I', 'F', 'F',  // ChunkID
            40, 0, 0, 0,         // ChunkSize (36 + data size)
            'W', 'A', 'V', 'E',  // Format
            'f', 'm', 't', ' ',  // Subchunk1ID
            16, 0, 0, 0,         // Subchunk1Size
            1, 0,                // AudioFormat (PCM)
            1, 0,                // NumChannels (mono)
            0x44, 0xAC, 0, 0,    // SampleRate (44100)
            0x88, 0x58, 0x01, 0, // ByteRate
            2, 0,                // BlockAlign
            16, 0,               // BitsPerSample
            'd', 'a', 't', 'a',  // Subchunk2ID
            4, 0, 0, 0           // Subchunk2Size
        };
        fwrite(wav_header, 1, 44, file);
        // 더미 오디오 데이터 (2바이트 * 2샘플)
        short dummy_data[2] = {0, 0};
        fwrite(dummy_data, sizeof(short), 2, file);
        fclose(file);
    }
}

// 테스트용 피치 벤드 파일 생성
static void create_test_pitch_bend_file(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file) {
        fprintf(file, "0.0\n");
        fprintf(file, "100.0\n");
        fprintf(file, "-50.0\n");
        fprintf(file, "200.0\n");
        fprintf(file, "0.0\n");
        fclose(file);
    }
}

// 테스트 1: 기본 파라미터 초기화 테스트
static int test_parameter_initialization(void) {
    printf("\n=== 테스트 1: 파라미터 초기화 ===\n");

    UTAUParameters params;
    ETResult result = utau_parameters_init(&params);

    TEST_ASSERT_EQ(ET_SUCCESS, result, "파라미터 초기화 성공");
    TEST_ASSERT_FLOAT_EQ(440.0f, params.target_pitch, 0.1f, "기본 피치 값 확인");
    TEST_ASSERT_FLOAT_EQ(1.0f, params.velocity, 0.01f, "기본 벨로시티 값 확인");
    TEST_ASSERT_FLOAT_EQ(1.0f, params.volume, 0.01f, "기본 볼륨 값 확인");
    TEST_ASSERT_EQ(44100, params.sample_rate, "기본 샘플링 레이트 확인");
    TEST_ASSERT_EQ(16, params.bit_depth, "기본 비트 깊이 확인");
    TEST_ASSERT(params.enable_cache, "기본 캐시 활성화 확인");
    TEST_ASSERT(params.enable_optimization, "기본 최적화 활성화 확인");
    TEST_ASSERT(!params.verbose_mode, "기본 상세 모드 비활성화 확인");

    return 1;
}

// 테스트 2: 기본 명령줄 파라미터 파싱 테스트
static int test_basic_parameter_parsing(void) {
    printf("\n=== 테스트 2: 기본 파라미터 파싱 ===\n");

    // 테스트 파일 생성
    create_test_wav_file("test_input.wav");

    char* argv[] = {
        "world4utau",
        "test_input.wav",
        "test_output.wav",
        "523.25",  // C5
        "80"       // 80% velocity
    };
    int argc = 5;

    // getopt 전역 변수 초기화
    optind = 1;
    opterr = 1;
    optopt = 0;

    UTAUParameters params;
    ETResult result = parse_utau_parameters(argc, argv, &params);

    TEST_ASSERT_EQ(ET_SUCCESS, result, "기본 파라미터 파싱 성공");
    TEST_ASSERT(strcmp(params.input_wav_path, "test_input.wav") == 0, "입력 파일 경로 확인");
    TEST_ASSERT(strcmp(params.output_wav_path, "test_output.wav") == 0, "출력 파일 경로 확인");
    TEST_ASSERT_FLOAT_EQ(523.25f, params.target_pitch, 0.01f, "목표 피치 확인");
    TEST_ASSERT_FLOAT_EQ(0.8f, params.velocity, 0.01f, "벨로시티 확인 (80% -> 0.8)");

    utau_parameters_cleanup(&params);
    remove("test_input.wav");

    return 1;
}

// 테스트 3: 옵션 파라미터 파싱 테스트
static int test_option_parameter_parsing(void) {
    printf("\n=== 테스트 3: 옵션 파라미터 파싱 ===\n");

    // 테스트 파일 생성
    create_test_wav_file("test_input2.wav");
    create_test_pitch_bend_file("test_pitch.txt");

    char* argv[] = {
        "world4utau",          // 0
        "test_input2.wav",     // 1
        "test_output2.wav",    // 2
        "440.0",               // 3
        "100",                 // 4
        "-v", "0.9",           // 5, 6 - 볼륨
        "-m", "0.3",           // 7, 8 - 모듈레이션
        "-c", "90",            // 9, 10 - 자음 벨로시티
        "-u", "50.0",          // 11, 12 - 선행발성
        "-o", "20.0",          // 13, 14 - 오버랩
        "-r", "48000",         // 15, 16 - 샘플링 레이트
        "-b", "24",            // 17, 18 - 비트 깊이
        "-p", "test_pitch.txt", // 19, 20 - 피치 벤드 파일
        "-V"                   // 21 - 상세 모드
    };
    int argc = 22;

    // getopt 전역 변수 초기화
    optind = 1;
    opterr = 1;
    optopt = 0;

    UTAUParameters params;
    ETResult result = parse_utau_parameters(argc, argv, &params);

    TEST_ASSERT_EQ(ET_SUCCESS, result, "옵션 파라미터 파싱 성공");
    TEST_ASSERT_FLOAT_EQ(0.9f, params.volume, 0.01f, "볼륨 옵션 확인");
    TEST_ASSERT_FLOAT_EQ(0.3f, params.modulation, 0.01f, "모듈레이션 옵션 확인");
    TEST_ASSERT_FLOAT_EQ(0.9f, params.consonant_velocity, 0.01f, "자음 벨로시티 옵션 확인");
    TEST_ASSERT_FLOAT_EQ(50.0f, params.pre_utterance, 0.01f, "선행발성 옵션 확인");
    TEST_ASSERT_FLOAT_EQ(20.0f, params.overlap, 0.01f, "오버랩 옵션 확인");
    TEST_ASSERT_EQ(48000, params.sample_rate, "샘플링 레이트 옵션 확인");
    TEST_ASSERT_EQ(24, params.bit_depth, "비트 깊이 옵션 확인");
    TEST_ASSERT(params.verbose_mode, "상세 모드 옵션 확인");
    TEST_ASSERT(params.pitch_bend != NULL, "피치 벤드 데이터 로드 확인");
    TEST_ASSERT_EQ(5, params.pitch_bend_length, "피치 벤드 데이터 길이 확인");

    utau_parameters_cleanup(&params);
    remove("test_input2.wav");
    remove("test_pitch.txt");

    return 1;
}

// 테스트 4: 파라미터 유효성 검사 테스트
static int test_parameter_validation(void) {
    printf("\n=== 테스트 4: 파라미터 유효성 검사 ===\n");

    // 테스트 파일 생성
    create_test_wav_file("test_valid.wav");

    // 유효한 파라미터 테스트
    UTAUParameters valid_params;
    utau_parameters_init(&valid_params);
    valid_params.input_wav_path = strdup("test_valid.wav");
    valid_params.output_wav_path = strdup("test_output_valid.wav");
    valid_params.target_pitch = 440.0f;
    valid_params.velocity = 0.8f;
    valid_params.volume = 1.0f;
    valid_params.owns_memory = true;

    TEST_ASSERT(validate_utau_parameters(&valid_params), "유효한 파라미터 검증 통과");

    utau_parameters_cleanup(&valid_params);

    // 무효한 파라미터 테스트들
    UTAUParameters invalid_params;

    // NULL 파라미터
    TEST_ASSERT(!validate_utau_parameters(NULL), "NULL 파라미터 검증 실패");

    // 잘못된 피치 범위
    utau_parameters_init(&invalid_params);
    invalid_params.input_wav_path = strdup("test_valid.wav");
    invalid_params.output_wav_path = strdup("test_output_invalid.wav");
    invalid_params.target_pitch = 30.0f;  // 너무 낮음
    invalid_params.owns_memory = true;

    TEST_ASSERT(!validate_utau_parameters(&invalid_params), "낮은 피치 값 검증 실패");
    utau_parameters_cleanup(&invalid_params);

    // 잘못된 벨로시티 범위
    utau_parameters_init(&invalid_params);
    invalid_params.input_wav_path = strdup("test_valid.wav");
    invalid_params.output_wav_path = strdup("test_output_invalid2.wav");
    invalid_params.velocity = 1.5f;  // 범위 초과
    invalid_params.owns_memory = true;

    TEST_ASSERT(!validate_voice_parameters(&invalid_params), "높은 벨로시티 값 검증 실패");
    utau_parameters_cleanup(&invalid_params);

    // 잘못된 샘플링 레이트
    utau_parameters_init(&invalid_params);
    invalid_params.sample_rate = 5000;  // 너무 낮음

    TEST_ASSERT(!validate_audio_settings(&invalid_params), "낮은 샘플링 레이트 검증 실패");

    remove("test_valid.wav");

    return 1;
}

// 테스트 5: 피치 벤드 파일 로딩 테스트
static int test_pitch_bend_loading(void) {
    printf("\n=== 테스트 5: 피치 벤드 파일 로딩 ===\n");

    // 유효한 피치 벤드 파일 생성
    FILE* pitch_file = fopen("test_pitch_valid.txt", "w");
    fprintf(pitch_file, "0.0\n");
    fprintf(pitch_file, "100.5\n");
    fprintf(pitch_file, "-200.3\n");
    fprintf(pitch_file, "50.0\n");
    fclose(pitch_file);

    float* pitch_bend = NULL;
    int length = 0;

    ETResult result = load_pitch_bend_file("test_pitch_valid.txt", &pitch_bend, &length);

    TEST_ASSERT_EQ(ET_SUCCESS, result, "피치 벤드 파일 로딩 성공");
    TEST_ASSERT_EQ(4, length, "피치 벤드 데이터 길이 확인");
    TEST_ASSERT(pitch_bend != NULL, "피치 벤드 데이터 포인터 확인");
    TEST_ASSERT_FLOAT_EQ(0.0f, pitch_bend[0], 0.01f, "첫 번째 피치 벤드 값 확인");
    TEST_ASSERT_FLOAT_EQ(100.5f, pitch_bend[1], 0.01f, "두 번째 피치 벤드 값 확인");
    TEST_ASSERT_FLOAT_EQ(-200.3f, pitch_bend[2], 0.01f, "세 번째 피치 벤드 값 확인");
    TEST_ASSERT_FLOAT_EQ(50.0f, pitch_bend[3], 0.01f, "네 번째 피치 벤드 값 확인");

    free(pitch_bend);

    // 존재하지 않는 파일 테스트
    result = load_pitch_bend_file("nonexistent_file.txt", &pitch_bend, &length);
    TEST_ASSERT_EQ(ET_ERROR_NOT_FOUND, result, "존재하지 않는 파일 에러 확인");

    // 빈 파일 테스트
    FILE* empty_file = fopen("test_pitch_empty.txt", "w");
    fclose(empty_file);

    result = load_pitch_bend_file("test_pitch_empty.txt", &pitch_bend, &length);
    TEST_ASSERT_EQ(ET_ERROR_IO, result, "빈 파일 에러 확인");

    remove("test_pitch_valid.txt");
    remove("test_pitch_empty.txt");

    return 1;
}

// 테스트 6: 에러 케이스 테스트
static int test_error_cases(void) {
    printf("\n=== 테스트 6: 에러 케이스 테스트 ===\n");

    UTAUParameters params;
    ETResult result;

    // getopt 전역 변수 초기화
    optind = 1;
    opterr = 1;
    optopt = 0;

    // 인수 부족 테스트
    char* argv_insufficient[] = {"world4utau", "input.wav"};
    result = parse_utau_parameters(2, argv_insufficient, &params);
    TEST_ASSERT_EQ(ET_ERROR_INVALID_ARGUMENT, result, "인수 부족 에러 확인");

    // NULL 파라미터 테스트
    char* argv_valid[] = {"world4utau", "input.wav", "output.wav"};
    result = parse_utau_parameters(3, argv_valid, NULL);
    TEST_ASSERT_EQ(ET_ERROR_INVALID_ARGUMENT, result, "NULL 파라미터 에러 확인");

    // 잘못된 피치 값 테스트
    create_test_wav_file("test_error.wav");
    char* argv_bad_pitch[] = {"world4utau", "test_error.wav", "output.wav", "invalid_pitch"};
    result = parse_utau_parameters(4, argv_bad_pitch, &params);
    TEST_ASSERT_EQ(ET_ERROR_INVALID_ARGUMENT, result, "잘못된 피치 값 에러 확인");

    // 잘못된 벨로시티 값 테스트
    char* argv_bad_velocity[] = {"world4utau", "test_error.wav", "output.wav", "440", "150"};
    result = parse_utau_parameters(5, argv_bad_velocity, &params);
    TEST_ASSERT_EQ(ET_ERROR_INVALID_ARGUMENT, result, "잘못된 벨로시티 값 에러 확인");

    remove("test_error.wav");

    return 1;
}

// 테스트 7: 메모리 관리 테스트
static int test_memory_management(void) {
    printf("\n=== 테스트 7: 메모리 관리 테스트 ===\n");

    create_test_wav_file("test_memory.wav");
    create_test_pitch_bend_file("test_memory_pitch.txt");

    char* argv[] = {
        "world4utau",
        "test_memory.wav",
        "test_memory_output.wav",
        "440.0",
        "100",
        "-p", "test_memory_pitch.txt"
    };
    int argc = 7;

    // getopt 전역 변수 초기화
    optind = 1;
    opterr = 1;
    optopt = 0;

    UTAUParameters params;
    ETResult result = parse_utau_parameters(argc, argv, &params);

    TEST_ASSERT_EQ(ET_SUCCESS, result, "메모리 관리 테스트 파싱 성공");
    TEST_ASSERT(params.owns_memory, "메모리 소유권 플래그 확인");
    TEST_ASSERT(params.input_wav_path != NULL, "입력 파일 경로 메모리 할당 확인");
    TEST_ASSERT(params.output_wav_path != NULL, "출력 파일 경로 메모리 할당 확인");
    TEST_ASSERT(params.pitch_bend != NULL, "피치 벤드 메모리 할당 확인");

    // 메모리 정리
    utau_parameters_cleanup(&params);

    // 정리 후 포인터들이 NULL이 되는지 확인 (구현에 따라 다를 수 있음)
    TEST_ASSERT(params.input_wav_path == NULL || strlen(params.input_wav_path) == 0, "입력 파일 경로 정리 확인");

    remove("test_memory.wav");
    remove("test_memory_pitch.txt");

    return 1;
}

// 메인 테스트 실행 함수
int main(void) {
    printf("UTAU 파라미터 파싱 단위 테스트 시작\n");
    printf("=====================================\n");

    int passed = 0;
    int total = 7;

    if (test_parameter_initialization()) passed++;
    if (test_basic_parameter_parsing()) passed++;
    if (test_option_parameter_parsing()) passed++;
    if (test_parameter_validation()) passed++;
    if (test_pitch_bend_loading()) passed++;
    if (test_error_cases()) passed++;
    if (test_memory_management()) passed++;

    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", passed, total);

    if (passed == total) {
        printf("모든 테스트가 성공했습니다! ✅\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다. ❌\n");
        return 1;
    }
}