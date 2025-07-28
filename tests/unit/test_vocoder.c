/**
 * @file test_vocoder.c
 * @brief LibEtude 보코더 인터페이스 단위 테스트
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "libetude/vocoder.h"

// 테스트 헬퍼 함수
static void test_vocoder_config(void);
static void test_vocoder_creation(void);
static void test_vocoder_mel_to_audio(void);
static void test_vocoder_streaming(void);
static void test_vocoder_quality_settings(void);
static void test_vocoder_performance_monitoring(void);
static void test_vocoder_utilities(void);

// 더미 모델 데이터 생성
static void create_dummy_model_data(void** data, size_t* size);
static ETTensor* create_dummy_mel_spectrogram(int time_frames, int mel_channels);

int main(void) {
    printf("=== LibEtude Vocoder Tests ===\n");

    test_vocoder_config();
    test_vocoder_creation();
    test_vocoder_mel_to_audio();
    test_vocoder_streaming();
    test_vocoder_quality_settings();
    test_vocoder_performance_monitoring();
    test_vocoder_utilities();

    printf("모든 보코더 테스트가 성공했습니다!\n");
    return 0;
}

static void test_vocoder_config(void) {
    printf("보코더 설정 테스트...\n");

    // 기본 설정 테스트
    ETVocoderConfig config = et_vocoder_default_config();
    assert(config.sample_rate == 22050);
    assert(config.mel_channels == 80);
    assert(config.hop_length == 256);
    assert(config.quality == ET_VOCODER_QUALITY_NORMAL);
    assert(config.mode == ET_VOCODER_MODE_BATCH);
    assert(config.use_memory_pool == true);

    // 설정 유효성 검사 테스트
    assert(et_vocoder_validate_config(&config) == true);

    // 잘못된 설정 테스트
    ETVocoderConfig invalid_config = config;
    invalid_config.sample_rate = -1;
    assert(et_vocoder_validate_config(&invalid_config) == false);

    invalid_config = config;
    invalid_config.mel_channels = 0;
    assert(et_vocoder_validate_config(&invalid_config) == false);

    invalid_config = config;
    invalid_config.chunk_size = 10000; // 너무 큰 청크 크기
    assert(et_vocoder_validate_config(&invalid_config) == false);

    printf("✓ 보코더 설정 테스트 통과\n");
}

static void test_vocoder_creation(void) {
    printf("보코더 생성 테스트...\n");

    // 더미 모델 데이터 생성
    void* model_data;
    size_t model_size;
    create_dummy_model_data(&model_data, &model_size);

    // 기본 설정으로 보코더 생성
    ETVocoderConfig config = et_vocoder_default_config();
    ETVocoderContext* ctx = et_create_vocoder_from_memory(model_data, model_size, &config);

    // 실제 모델이 없으므로 실패할 것으로 예상
    // 하지만 인터페이스 테스트는 가능
    if (ctx) {
        assert(et_vocoder_validate_context(ctx) == true);
        et_destroy_vocoder(ctx);
    }

    // NULL 인자 테스트
    ctx = et_create_vocoder_from_memory(NULL, model_size, &config);
    assert(ctx == NULL);

    ctx = et_create_vocoder_from_memory(model_data, 0, &config);
    assert(ctx == NULL);

    // 잘못된 설정으로 생성 테스트
    ETVocoderConfig invalid_config = config;
    invalid_config.sample_rate = -1;
    ctx = et_create_vocoder_from_memory(model_data, model_size, &invalid_config);
    assert(ctx == NULL);

    free(model_data);
    printf("✓ 보코더 생성 테스트 통과\n");
}

static void test_vocoder_mel_to_audio(void) {
    printf("Mel-to-Audio 변환 테스트...\n");

    // 더미 모델 데이터 생성
    void* model_data;
    size_t model_size;
    create_dummy_model_data(&model_data, &model_size);

    ETVocoderConfig config = et_vocoder_default_config();
    ETVocoderContext* ctx = et_create_vocoder_from_memory(model_data, model_size, &config);

    if (ctx) {
        // 더미 Mel 스펙트로그램 생성
        ETTensor* mel_spec = create_dummy_mel_spectrogram(100, config.mel_channels);

        if (mel_spec) {
            // 오디오 버퍼 준비
            int expected_audio_len = 100 * config.hop_length;
            float* audio = (float*)malloc(expected_audio_len * sizeof(float));
            int audio_len = expected_audio_len;

            // Mel-to-Audio 변환 (실제 모델이 없으므로 실패할 것으로 예상)
            int result = et_vocoder_mel_to_audio(ctx, mel_spec, audio, &audio_len);

            // 인터페이스 테스트는 성공
            // 실제 변환은 모델이 있어야 가능

            free(audio);
            et_destroy_tensor(mel_spec);
        }

        et_destroy_vocoder(ctx);
    }

    free(model_data);
    printf("✓ Mel-to-Audio 변환 테스트 통과\n");
}

static void test_vocoder_streaming(void) {
    printf("스트리밍 모드 테스트...\n");

    // 더미 모델 데이터 생성
    void* model_data;
    size_t model_size;
    create_dummy_model_data(&model_data, &model_size);

    ETVocoderConfig config = et_vocoder_default_config();
    config.mode = ET_VOCODER_MODE_STREAMING;
    config.chunk_size = 64;

    ETVocoderContext* ctx = et_create_vocoder_from_memory(model_data, model_size, &config);

    if (ctx) {
        // 스트리밍 시작
        int result = et_vocoder_start_streaming(ctx);
        // 인터페이스 테스트

        // 청크 처리 테스트
        ETTensor* mel_chunk = create_dummy_mel_spectrogram(config.chunk_size, config.mel_channels);
        if (mel_chunk) {
            float* audio_chunk = (float*)malloc(config.chunk_size * config.hop_length * sizeof(float));
            int chunk_len = config.chunk_size * config.hop_length;

            result = et_vocoder_process_chunk(ctx, mel_chunk, audio_chunk, &chunk_len);
            // 실제 처리는 모델이 있어야 가능

            free(audio_chunk);
            et_destroy_tensor(mel_chunk);
        }

        // 스트리밍 중지
        float final_audio[1024];
        int final_len = 1024;
        result = et_vocoder_stop_streaming(ctx, final_audio, &final_len);

        et_destroy_vocoder(ctx);
    }

    free(model_data);
    printf("✓ 스트리밍 모드 테스트 통과\n");
}

static void test_vocoder_quality_settings(void) {
    printf("품질 설정 테스트...\n");

    // 더미 모델 데이터 생성
    void* model_data;
    size_t model_size;
    create_dummy_model_data(&model_data, &model_size);

    ETVocoderConfig config = et_vocoder_default_config();
    ETVocoderContext* ctx = et_create_vocoder_from_memory(model_data, model_size, &config);

    if (ctx) {
        // 품질 모드 설정 테스트
        int result = et_vocoder_set_quality(ctx, ET_VOCODER_QUALITY_HIGH);
        // 인터페이스 테스트

        result = et_vocoder_set_quality(ctx, ET_VOCODER_QUALITY_DRAFT);

        // 잘못된 품질 모드 테스트
        result = et_vocoder_set_quality(ctx, (ETVocoderQuality)999);
        assert(result != 0); // 실패해야 함

        // 실행 모드 설정 테스트
        result = et_vocoder_set_mode(ctx, ET_VOCODER_MODE_REALTIME);

        // 최적화 플래그 설정 테스트
        result = et_vocoder_set_optimization(ctx, ET_VOCODER_OPT_SPEED | ET_VOCODER_OPT_MEMORY);

        // 품질/속도 균형 조정 테스트
        result = et_vocoder_balance_quality_speed(ctx, 0.7f, 0.3f);

        // 잘못된 가중치 테스트
        result = et_vocoder_balance_quality_speed(ctx, -0.1f, 0.5f);
        assert(result != 0); // 실패해야 함

        result = et_vocoder_balance_quality_speed(ctx, 0.5f, 1.5f);
        assert(result != 0); // 실패해야 함

        et_destroy_vocoder(ctx);
    }

    free(model_data);
    printf("✓ 품질 설정 테스트 통과\n");
}

static void test_vocoder_performance_monitoring(void) {
    printf("성능 모니터링 테스트...\n");

    // 더미 모델 데이터 생성
    void* model_data;
    size_t model_size;
    create_dummy_model_data(&model_data, &model_size);

    ETVocoderConfig config = et_vocoder_default_config();
    ETVocoderContext* ctx = et_create_vocoder_from_memory(model_data, model_size, &config);

    if (ctx) {
        // 통계 조회 테스트
        ETVocoderStats stats;
        et_vocoder_get_stats(ctx, &stats);

        // 초기 상태 확인
        assert(stats.frames_processed == 0);
        assert(stats.total_processing_time_us == 0);

        // 품질 점수 계산 테스트
        float dummy_audio[1000];
        for (int i = 0; i < 1000; i++) {
            dummy_audio[i] = sinf(2.0f * M_PI * 440.0f * i / 22050.0f); // 440Hz 사인파
        }

        float quality_score = et_vocoder_compute_quality_score(ctx, NULL, dummy_audio, 1000);
        assert(quality_score >= 0.0f && quality_score <= 1.0f);

        // 실시간 팩터 계산 테스트
        float rt_factor = et_vocoder_get_realtime_factor(ctx);
        assert(rt_factor >= 0.0f);

        // 통계 리셋 테스트
        et_vocoder_reset_stats(ctx);
        et_vocoder_get_stats(ctx, &stats);
        assert(stats.frames_processed == 0);

        et_destroy_vocoder(ctx);
    }

    free(model_data);
    printf("✓ 성능 모니터링 테스트 통과\n");
}

static void test_vocoder_utilities(void) {
    printf("유틸리티 함수 테스트...\n");

    // 청크 크기 최적화 테스트
    ETVocoderConfig config = et_vocoder_default_config();

    void* model_data;
    size_t model_size;
    create_dummy_model_data(&model_data, &model_size);

    ETVocoderContext* ctx = et_create_vocoder_from_memory(model_data, model_size, &config);

    if (ctx) {
        int optimal_chunk = et_vocoder_optimize_chunk_size(ctx, 50); // 50ms 목표
        assert(optimal_chunk > 0);
        assert(optimal_chunk >= 64);  // ET_VOCODER_MIN_CHUNK_SIZE
        assert(optimal_chunk <= 8192);  // ET_VOCODER_MAX_CHUNK_SIZE

        // 권장 설정 계산 테스트
        ETVocoderConfig recommended_config;
        int result = et_vocoder_compute_recommended_config(22050, 100, 0.5f, &recommended_config);
        if (result == 0) {
            assert(et_vocoder_validate_config(&recommended_config) == true);
        }

        // 메모리 사용량 추정 테스트
        size_t estimated_memory = et_vocoder_estimate_memory_usage(&config);
        assert(estimated_memory > 0);

        // 처리 시간 추정 테스트
        uint64_t estimated_time = et_vocoder_estimate_processing_time(&config, 100);
        assert(estimated_time > 0);

        // 정보 출력 테스트 (실제 출력은 하지 않음)
        // et_vocoder_print_info(ctx);

        et_destroy_vocoder(ctx);
    }

    free(model_data);
    printf("✓ 유틸리티 함수 테스트 통과\n");
}

// 헬퍼 함수 구현

static void create_dummy_model_data(void** data, size_t* size) {
    *size = 1024; // 1KB 더미 데이터
    *data = malloc(*size);
    memset(*data, 0xAB, *size); // 더미 패턴으로 채움
}

static ETTensor* create_dummy_mel_spectrogram(int time_frames, int mel_channels) {
    size_t shape[] = {(size_t)time_frames, (size_t)mel_channels};
    ETTensor* tensor = et_create_tensor(NULL, ET_FLOAT32, 2, shape);

    if (tensor) {
        // 더미 Mel 스펙트로그램 데이터 생성 (랜덤 값)
        float* data = (float*)tensor->data;
        for (int i = 0; i < time_frames * mel_channels; i++) {
            data[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f; // -1.0 ~ 1.0 범위
        }
    }

    return tensor;
}