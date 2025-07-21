/**
 * @file test_engine.c
 * @brief LibEtude 엔진 단위 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "framework/test_framework.h"
#include "libetude/api.h"
#include <stdlib.h>
#include <string.h>

// 테스트용 더미 모델 파일 경로
static const char* TEST_MODEL_PATH = "test_model.lef";

/**
 * @brief 테스트 설정 함수
 */
void setup_engine_tests() {
    // 더미 모델 파일 생성 (실제 구현에서는 유효한 모델 파일 사용)
    FILE* file = fopen(TEST_MODEL_PATH, "wb");
    if (file) {
        // 더미 데이터 작성
        const char dummy_data[] = "DUMMY_MODEL_DATA";
        fwrite(dummy_data, 1, sizeof(dummy_data), file);
        fclose(file);
    }
}

/**
 * @brief 테스트 정리 함수
 */
void teardown_engine_tests() {
    // 더미 모델 파일 삭제
    remove(TEST_MODEL_PATH);
}

/**
 * @brief 엔진 생성 및 해제 테스트
 */
void test_engine_create_destroy() {
    LibEtudeEngine* engine = libetude_create_engine(TEST_MODEL_PATH);
    TEST_ASSERT_NOT_NULL(engine);

    libetude_destroy_engine(engine);
    TEST_PASS();
}

/**
 * @brief 잘못된 모델 경로로 엔진 생성 테스트
 */
void test_engine_create_invalid_path() {
    LibEtudeEngine* engine = libetude_create_engine("nonexistent_model.lef");
    TEST_ASSERT_NULL(engine);
    TEST_PASS();
}

/**
 * @brief NULL 모델 경로로 엔진 생성 테스트
 */
void test_engine_create_null_path() {
    LibEtudeEngine* engine = libetude_create_engine(NULL);
    TEST_ASSERT_NULL(engine);
    TEST_PASS();
}

/**
 * @brief 버전 정보 테스트
 */
void test_get_version() {
    const char* version = libetude_get_version();
    TEST_ASSERT_NOT_NULL(version);
    TEST_ASSERT_EQUAL_STRING("1.0.0", version);
    TEST_PASS();
}

/**
 * @brief 하드웨어 기능 감지 테스트
 */
void test_get_hardware_features() {
    uint32_t features = libetude_get_hardware_features();
    // 최소한 NONE이 아니어야 함 (실제로는 플랫폼에 따라 다름)
    TEST_ASSERT(features >= LIBETUDE_SIMD_NONE);
    TEST_PASS();
}

/**
 * @brief 텍스트 합성 기본 테스트
 */
void test_synthesize_text_basic() {
    LibEtudeEngine* engine = libetude_create_engine(TEST_MODEL_PATH);
    TEST_ASSERT_NOT_NULL(engine);

    const char* text = "Hello, world!";
    float audio_buffer[22050]; // 1초 분량
    int audio_length = sizeof(audio_buffer) / sizeof(float);

    int result = libetude_synthesize_text(engine, text, audio_buffer, &audio_length);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);
    TEST_ASSERT(audio_length > 0);

    libetude_destroy_engine(engine);
    TEST_PASS();
}

/**
 * @brief 텍스트 합성 잘못된 인수 테스트
 */
void test_synthesize_text_invalid_args() {
    LibEtudeEngine* engine = libetude_create_engine(TEST_MODEL_PATH);
    TEST_ASSERT_NOT_NULL(engine);

    float audio_buffer[1024];
    int audio_length = sizeof(audio_buffer) / sizeof(float);

    // NULL 텍스트
    int result = libetude_synthesize_text(engine, NULL, audio_buffer, &audio_length);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_ERROR_INVALID_ARGUMENT, result);

    // NULL 오디오 버퍼
    result = libetude_synthesize_text(engine, "test", NULL, &audio_length);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_ERROR_INVALID_ARGUMENT, result);

    // NULL 길이 포인터
    result = libetude_synthesize_text(engine, "test", audio_buffer, NULL);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_ERROR_INVALID_ARGUMENT, result);

    libetude_destroy_engine(engine);
    TEST_PASS();
}

/**
 * @brief 품질 모드 설정 테스트
 */
void test_set_quality_mode() {
    LibEtudeEngine* engine = libetude_create_engine(TEST_MODEL_PATH);
    TEST_ASSERT_NOT_NULL(engine);

    // 유효한 품질 모드들
    int result = libetude_set_quality_mode(engine, LIBETUDE_QUALITY_FAST);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);

    result = libetude_set_quality_mode(engine, LIBETUDE_QUALITY_BALANCED);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);

    result = libetude_set_quality_mode(engine, LIBETUDE_QUALITY_HIGH);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);

    // 잘못된 품질 모드
    result = libetude_set_quality_mode(engine, (QualityMode)999);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_ERROR_INVALID_ARGUMENT, result);

    libetude_destroy_engine(engine);
    TEST_PASS();
}

/**
 * @brief GPU 가속 활성화 테스트
 */
void test_enable_gpu_acceleration() {
    LibEtudeEngine* engine = libetude_create_engine(TEST_MODEL_PATH);
    TEST_ASSERT_NOT_NULL(engine);

    int result = libetude_enable_gpu_acceleration(engine);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);

    libetude_destroy_engine(engine);
    TEST_PASS();
}

/**
 * @brief 성능 통계 조회 테스트
 */
void test_get_performance_stats() {
    LibEtudeEngine* engine = libetude_create_engine(TEST_MODEL_PATH);
    TEST_ASSERT_NOT_NULL(engine);

    // 먼저 텍스트 합성을 수행하여 통계 생성
    const char* text = "Test text";
    float audio_buffer[1024];
    int audio_length = sizeof(audio_buffer) / sizeof(float);
    libetude_synthesize_text(engine, text, audio_buffer, &audio_length);

    // 성능 통계 조회
    PerformanceStats stats;
    int result = libetude_get_performance_stats(engine, &stats);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);

    // 통계 값들이 유효한지 확인
    TEST_ASSERT(stats.inference_time_ms >= 0.0);
    TEST_ASSERT(stats.memory_usage_mb >= 0.0);
    TEST_ASSERT(stats.cpu_usage_percent >= 0.0);

    libetude_destroy_engine(engine);
    TEST_PASS();
}

/**
 * @brief 스트리밍 시작/중지 테스트
 */
void test_streaming_start_stop() {
    LibEtudeEngine* engine = libetude_create_engine(TEST_MODEL_PATH);
    TEST_ASSERT_NOT_NULL(engine);

    // 더미 콜백 함수
    void dummy_callback(const float* audio, int length, void* user_data) {
        (void)audio; (void)length; (void)user_data;
    }

    // 스트리밍 시작
    int result = libetude_start_streaming(engine, dummy_callback, NULL);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);

    // 스트리밍 중지
    result = libetude_stop_streaming(engine);
    TEST_ASSERT_EQUAL_INT(LIBETUDE_SUCCESS, result);

    libetude_destroy_engine(engine);
    TEST_PASS();
}

/**
 * @brief 메인 함수
 */
int main() {
    TestSuite* suite = test_suite_create("Engine Tests");

    ADD_TEST_WITH_SETUP(suite, test_engine_create_destroy, setup_engine_tests, teardown_engine_tests);
    ADD_TEST_WITH_SETUP(suite, test_engine_create_invalid_path, setup_engine_tests, teardown_engine_tests);
    ADD_TEST_WITH_SETUP(suite, test_engine_create_null_path, setup_engine_tests, teardown_engine_tests);
    ADD_TEST(suite, test_get_version);
    ADD_TEST(suite, test_get_hardware_features);
    ADD_TEST_WITH_SETUP(suite, test_synthesize_text_basic, setup_engine_tests, teardown_engine_tests);
    ADD_TEST_WITH_SETUP(suite, test_synthesize_text_invalid_args, setup_engine_tests, teardown_engine_tests);
    ADD_TEST_WITH_SETUP(suite, test_set_quality_mode, setup_engine_tests, teardown_engine_tests);
    ADD_TEST_WITH_SETUP(suite, test_enable_gpu_acceleration, setup_engine_tests, teardown_engine_tests);
    ADD_TEST_WITH_SETUP(suite, test_get_performance_stats, setup_engine_tests, teardown_engine_tests);
    ADD_TEST_WITH_SETUP(suite, test_streaming_start_stop, setup_engine_tests, teardown_engine_tests);

    test_suite_run(suite);
    test_print_summary();

    int exit_code = test_get_exit_code();
    test_suite_destroy(suite);

    return exit_code;
}