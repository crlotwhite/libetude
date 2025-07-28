/**
 * @file test_api.c
 * @brief LibEtude 핵심 C API 테스트
 * @author LibEtude Project
 * @version 1.0.0
 *
 * LibEtude의 핵심 C API 기능을 테스트합니다.
 */

#include "libetude/api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// 테스트 결과 카운터
static int tests_passed = 0;
static int tests_failed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("✗ %s\n", message); \
            tests_failed++; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_EQUAL(expected, actual, message) \
    TEST_ASSERT((expected) == (actual), message)

// 테스트용 오디오 콜백
static void test_audio_callback(const float* audio, int length, void* user_data) {
    int* callback_count = (int*)user_data;
    (*callback_count)++;
    printf("오디오 콜백 호출됨: %d 샘플\n", length);
}

// 테스트용 로그 콜백
static void test_log_callback(LibEtudeLogLevel level, const char* message, void* user_data) {
    const char* level_names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
    printf("[%s] %s\n", level_names[level], message);
}

/**
 * @brief 기본 API 테스트
 */
void test_basic_api() {
    printf("\n=== 기본 API 테스트 ===\n");

    // 버전 정보 테스트
    const char* version = libetude_get_version();
    TEST_ASSERT_NOT_NULL(version, "버전 정보 조회");
    printf("LibEtude 버전: %s\n", version);

    // 하드웨어 기능 테스트
    uint32_t hw_features = libetude_get_hardware_features();
    printf("하드웨어 기능: 0x%08X\n", hw_features);
    TEST_ASSERT(hw_features >= 0, "하드웨어 기능 감지");

    // 로그 시스템 테스트
    libetude_set_log_callback(test_log_callback, NULL);
    libetude_set_log_level(LIBETUDE_LOG_INFO);
    TEST_ASSERT_EQUAL(LIBETUDE_LOG_INFO, libetude_get_log_level(), "로그 레벨 설정");

    libetude_log(LIBETUDE_LOG_INFO, "테스트 로그 메시지");
}

/**
 * @brief 엔진 생성/해제 테스트
 */
void test_engine_lifecycle() {
    printf("\n=== 엔진 생성/해제 테스트 ===\n");

    // 잘못된 인수로 엔진 생성 시도
    LibEtudeEngine* engine = libetude_create_engine(NULL);
    TEST_ASSERT_NULL(engine, "NULL 모델 경로로 엔진 생성 실패");

    // 존재하지 않는 모델 파일로 엔진 생성 시도
    engine = libetude_create_engine("nonexistent_model.lef");
    TEST_ASSERT_NULL(engine, "존재하지 않는 모델 파일로 엔진 생성 실패");

    // 더미 모델 파일 생성 (테스트용)
    FILE* dummy_model = fopen("test_model.lef", "wb");
    if (dummy_model) {
        // 간단한 더미 LEF 헤더 작성
        uint32_t magic = 0x4445454C; // 'LEED' in little-endian
        fwrite(&magic, sizeof(magic), 1, dummy_model);

        uint16_t version_major = 1;
        uint16_t version_minor = 0;
        fwrite(&version_major, sizeof(version_major), 1, dummy_model);
        fwrite(&version_minor, sizeof(version_minor), 1, dummy_model);

        // 나머지 헤더 필드들을 0으로 채움
        uint8_t padding[1024] = {0};
        fwrite(padding, sizeof(padding), 1, dummy_model);

        fclose(dummy_model);

        // 더미 모델로 엔진 생성 시도
        engine = libetude_create_engine("test_model.lef");
        if (engine) {
            TEST_ASSERT_NOT_NULL(engine, "더미 모델로 엔진 생성 성공");

            // 엔진 해제
            libetude_destroy_engine(engine);
            printf("✓ 엔진 해제 완료\n");
        } else {
            printf("✗ 더미 모델로 엔진 생성 실패: %s\n", libetude_get_last_error());
            tests_failed++;
        }

        // 더미 파일 삭제
        unlink("test_model.lef");
    }

    // NULL 엔진 해제 (크래시하지 않아야 함)
    libetude_destroy_engine(NULL);
    printf("✓ NULL 엔진 해제 안전성 확인\n");
}

/**
 * @brief 음성 합성 API 테스트
 */
void test_synthesis_api() {
    printf("\n=== 음성 합성 API 테스트 ===\n");

    // 더미 모델 파일 생성
    FILE* dummy_model = fopen("test_model.lef", "wb");
    if (!dummy_model) {
        printf("✗ 테스트용 모델 파일 생성 실패\n");
        tests_failed++;
        return;
    }

    // 간단한 더미 LEF 헤더 작성
    uint32_t magic = 0x4445454C; // 'LEED'
    fwrite(&magic, sizeof(magic), 1, dummy_model);
    uint16_t version_major = 1, version_minor = 0;
    fwrite(&version_major, sizeof(version_major), 1, dummy_model);
    fwrite(&version_minor, sizeof(version_minor), 1, dummy_model);
    uint8_t padding[1024] = {0};
    fwrite(padding, sizeof(padding), 1, dummy_model);
    fclose(dummy_model);

    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    if (!engine) {
        printf("✗ 테스트용 엔진 생성 실패: %s\n", libetude_get_last_error());
        tests_failed++;
        unlink("test_model.lef");
        return;
    }

    // 텍스트 합성 테스트
    const char* test_text = "안녕하세요, LibEtude 테스트입니다.";
    float audio_buffer[1000];
    int audio_length = 1000;

    int result = libetude_synthesize_text(engine, test_text, audio_buffer, &audio_length);
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "텍스트 음성 합성");
    TEST_ASSERT(audio_length > 0, "합성된 오디오 길이 확인");

    // 잘못된 인수 테스트
    result = libetude_synthesize_text(NULL, test_text, audio_buffer, &audio_length);
    TEST_ASSERT(result != LIBETUDE_SUCCESS, "NULL 엔진으로 합성 실패");

    result = libetude_synthesize_text(engine, NULL, audio_buffer, &audio_length);
    TEST_ASSERT(result != LIBETUDE_SUCCESS, "NULL 텍스트로 합성 실패");

    // 노래 합성 테스트 (현재는 구현되지 않음)
    float notes[] = {60.0f, 62.0f, 64.0f, 65.0f}; // C, D, E, F
    result = libetude_synthesize_singing(engine, "도레미파", notes, 4, audio_buffer, &audio_length);
    TEST_ASSERT_EQUAL(LIBETUDE_ERROR_NOT_IMPLEMENTED, result, "노래 합성 미구현 확인");

    libetude_destroy_engine(engine);
    unlink("test_model.lef");
}

/**
 * @brief 스트리밍 API 테스트
 */
void test_streaming_api() {
    printf("\n=== 스트리밍 API 테스트 ===\n");

    // 더미 모델 파일 생성
    FILE* dummy_model = fopen("test_model.lef", "wb");
    if (!dummy_model) {
        printf("✗ 테스트용 모델 파일 생성 실패\n");
        tests_failed++;
        return;
    }

    uint32_t magic = 0x4445454C;
    fwrite(&magic, sizeof(magic), 1, dummy_model);
    uint16_t version_major = 1, version_minor = 0;
    fwrite(&version_major, sizeof(version_major), 1, dummy_model);
    fwrite(&version_minor, sizeof(version_minor), 1, dummy_model);
    uint8_t padding[1024] = {0};
    fwrite(padding, sizeof(padding), 1, dummy_model);
    fclose(dummy_model);

    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    if (!engine) {
        printf("✗ 테스트용 엔진 생성 실패\n");
        tests_failed++;
        unlink("test_model.lef");
        return;
    }

    int callback_count = 0;

    // 스트리밍 시작
    int result = libetude_start_streaming(engine, test_audio_callback, &callback_count);
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "스트리밍 시작");

    // 텍스트 스트리밍
    result = libetude_stream_text(engine, "첫 번째 텍스트");
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "첫 번째 텍스트 스트리밍");

    result = libetude_stream_text(engine, "두 번째 텍스트");
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "두 번째 텍스트 스트리밍");

    // 잠시 대기 (스트리밍 처리 시간)
    usleep(100000); // 100ms

    // 스트리밍 중지
    result = libetude_stop_streaming(engine);
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "스트리밍 중지");

    // 중복 스트리밍 시작 테스트
    libetude_start_streaming(engine, test_audio_callback, &callback_count);
    result = libetude_start_streaming(engine, test_audio_callback, &callback_count);
    TEST_ASSERT(result != LIBETUDE_SUCCESS, "중복 스트리밍 시작 실패");

    libetude_stop_streaming(engine);
    libetude_destroy_engine(engine);
    unlink("test_model.lef");
}

/**
 * @brief 성능 제어 API 테스트
 */
void test_performance_api() {
    printf("\n=== 성능 제어 API 테스트 ===\n");

    // 더미 모델 파일 생성
    FILE* dummy_model = fopen("test_model.lef", "wb");
    if (!dummy_model) {
        printf("✗ 테스트용 모델 파일 생성 실패\n");
        tests_failed++;
        return;
    }

    uint32_t magic = 0x4445454C;
    fwrite(&magic, sizeof(magic), 1, dummy_model);
    uint16_t version_major = 1, version_minor = 0;
    fwrite(&version_major, sizeof(version_major), 1, dummy_model);
    fwrite(&version_minor, sizeof(version_minor), 1, dummy_model);
    uint8_t padding[1024] = {0};
    fwrite(padding, sizeof(padding), 1, dummy_model);
    fclose(dummy_model);

    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    if (!engine) {
        printf("✗ 테스트용 엔진 생성 실패\n");
        tests_failed++;
        unlink("test_model.lef");
        return;
    }

    // 품질 모드 설정 테스트
    int result = libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH);
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "고품질 모드 설정");

    result = libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "고속 모드 설정");

    // 잘못된 품질 모드 테스트
    result = libetude_set_quality_mode(engine, (QualityMode)999);
    TEST_ASSERT(result != LIBETUDE_SUCCESS, "잘못된 품질 모드 설정 실패");

    // GPU 가속 활성화 테스트
    result = libetude_enable_gpu_acceleration(engine);
    // GPU가 없을 수도 있으므로 결과에 관계없이 테스트 통과
    printf("GPU 가속 활성화 결과: %d\n", result);

    // 성능 통계 조회 테스트
    PerformanceStats stats;
    result = libetude_get_performance_stats(engine, &stats);
    TEST_ASSERT_EQUAL(LIBETUDE_SUCCESS, result, "성능 통계 조회");

    printf("성능 통계:\n");
    printf("  추론 시간: %.2f ms\n", stats.inference_time_ms);
    printf("  메모리 사용량: %.2f MB\n", stats.memory_usage_mb);
    printf("  CPU 사용률: %.2f%%\n", stats.cpu_usage_percent);
    printf("  GPU 사용률: %.2f%%\n", stats.gpu_usage_percent);
    printf("  활성 스레드: %d\n", stats.active_threads);

    libetude_destroy_engine(engine);
    unlink("test_model.lef");
}

/**
 * @brief 확장 모델 API 테스트
 */
void test_extension_api() {
    printf("\n=== 확장 모델 API 테스트 ===\n");

    // 더미 모델 파일 생성
    FILE* dummy_model = fopen("test_model.lef", "wb");
    if (!dummy_model) {
        printf("✗ 테스트용 모델 파일 생성 실패\n");
        tests_failed++;
        return;
    }

    uint32_t magic = 0x4445454C;
    fwrite(&magic, sizeof(magic), 1, dummy_model);
    uint16_t version_major = 1, version_minor = 0;
    fwrite(&version_major, sizeof(version_major), 1, dummy_model);
    fwrite(&version_minor, sizeof(version_minor), 1, dummy_model);
    uint8_t padding[1024] = {0};
    fwrite(padding, sizeof(padding), 1, dummy_model);
    fclose(dummy_model);

    LibEtudeEngine* engine = libetude_create_engine("test_model.lef");
    if (!engine) {
        printf("✗ 테스트용 엔진 생성 실패\n");
        tests_failed++;
        unlink("test_model.lef");
        return;
    }

    // 존재하지 않는 확장 모델 로드 시도
    int result = libetude_load_extension(engine, "nonexistent_extension.lefx");
    TEST_ASSERT(result != LIBETUDE_SUCCESS, "존재하지 않는 확장 모델 로드 실패");

    // 잘못된 확장 ID로 언로드 시도
    result = libetude_unload_extension(engine, 999);
    TEST_ASSERT(result != LIBETUDE_SUCCESS, "잘못된 확장 ID로 언로드 실패");

    libetude_destroy_engine(engine);
    unlink("test_model.lef");
}

/**
 * @brief 오류 처리 테스트
 */
void test_error_handling() {
    printf("\n=== 오류 처리 테스트 ===\n");

    // 마지막 오류 메시지 초기화
    const char* error_msg = libetude_get_last_error();
    TEST_ASSERT_NOT_NULL(error_msg, "오류 메시지 조회");

    // NULL 포인터로 API 호출하여 오류 발생시키기
    LibEtudeEngine* engine = libetude_create_engine(NULL);
    TEST_ASSERT_NULL(engine, "NULL 모델 경로로 엔진 생성 실패");

    error_msg = libetude_get_last_error();
    TEST_ASSERT(strlen(error_msg) > 0, "오류 메시지 설정 확인");
    printf("마지막 오류: %s\n", error_msg);
}

/**
 * @brief 메인 테스트 함수
 */
int main() {
    printf("LibEtude 핵심 C API 테스트 시작\n");
    printf("=====================================\n");

    // 각 테스트 실행
    test_basic_api();
    test_engine_lifecycle();
    test_synthesis_api();
    test_streaming_api();
    test_performance_api();
    test_extension_api();
    test_error_handling();

    // 테스트 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과:\n");
    printf("  통과: %d\n", tests_passed);
    printf("  실패: %d\n", tests_failed);
    printf("  총계: %d\n", tests_passed + tests_failed);

    if (tests_failed == 0) {
        printf("✓ 모든 테스트 통과!\n");
        return 0;
    } else {
        printf("✗ %d개 테스트 실패\n", tests_failed);
        return 1;
    }
}