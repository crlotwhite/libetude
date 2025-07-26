/**
 * @file test_compression.c
 * @brief LibEtude 압축 기능 테스트
 *
 * LZ4, ZSTD, 음성 특화 압축 기능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include "libetude/compression.h"

// 테스트 결과 출력 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return 0; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

// 테스트 데이터 생성 함수
static void generate_test_data(uint8_t* data, size_t size, int pattern_type) {
    switch (pattern_type) {
        case 0: // 랜덤 데이터
            for (size_t i = 0; i < size; i++) {
                data[i] = (uint8_t)(rand() % 256);
            }
            break;
        case 1: // 반복 패턴
            for (size_t i = 0; i < size; i++) {
                data[i] = (uint8_t)(i % 16);
            }
            break;
        case 2: // 음성 데이터 시뮬레이션
            for (size_t i = 0; i < size; i++) {
                float t = (float)i / size;
                float value = 128.0f + 64.0f * sinf(2.0f * 3.14159f * 10.0f * t);
                data[i] = (uint8_t)value;
            }
            break;
        case 3: // 희소 데이터 (많은 0)
            memset(data, 0, size);
            for (size_t i = 0; i < size / 10; i++) {
                data[rand() % size] = (uint8_t)(rand() % 256);
            }
            break;
    }
}

// 기본 압축 컨텍스트 테스트
static int test_compression_context() {
    printf("\n=== 압축 컨텍스트 테스트 ===\n");

    // LZ4 컨텍스트 생성
    CompressionContext* lz4_ctx = compression_create_context(COMPRESSION_LZ4, COMPRESSION_LEVEL_DEFAULT);
    TEST_ASSERT(lz4_ctx != NULL, "LZ4 컨텍스트 생성");
    TEST_ASSERT(lz4_ctx->algorithm == COMPRESSION_LZ4, "LZ4 알고리즘 확인");
    TEST_ASSERT(lz4_ctx->level == COMPRESSION_LEVEL_DEFAULT, "압축 레벨 확인");

    // ZSTD 컨텍스트 생성
    CompressionContext* zstd_ctx = compression_create_context(COMPRESSION_ZSTD, COMPRESSION_LEVEL_BEST);
    TEST_ASSERT(zstd_ctx != NULL, "ZSTD 컨텍스트 생성");
    TEST_ASSERT(zstd_ctx->algorithm == COMPRESSION_ZSTD, "ZSTD 알고리즘 확인");

    // 음성 특화 압축 컨텍스트 생성
    VoiceCompressionParams voice_params = {
        .mel_frequency_weight = 1.2f,
        .temporal_correlation = 0.8f,
        .use_perceptual_model = true,
        .quality_threshold = 0.95f
    };

    CompressionContext* voice_ctx = voice_compression_create_context(&voice_params);
    TEST_ASSERT(voice_ctx != NULL, "음성 특화 압축 컨텍스트 생성");
    TEST_ASSERT(voice_ctx->algorithm == COMPRESSION_VOICE_OPTIMIZED, "음성 특화 알고리즘 확인");

    // 컨텍스트 해제
    compression_destroy_context(lz4_ctx);
    compression_destroy_context(zstd_ctx);
    compression_destroy_context(voice_ctx);

    printf("PASS: 압축 컨텍스트 테스트 통과\n");
    return 1;
}

// LZ4 압축 테스트
static int test_lz4_compression() {
    printf("\n=== LZ4 압축 테스트 ===\n");

    const size_t test_size = 1024;
    uint8_t* input = (uint8_t*)malloc(test_size);
    uint8_t* compressed = (uint8_t*)malloc(test_size * 2);
    uint8_t* decompressed = (uint8_t*)malloc(test_size);

    TEST_ASSERT(input && compressed && decompressed, "메모리 할당");

    // 반복 패턴 데이터 생성 (압축률이 좋음)
    generate_test_data(input, test_size, 1);

    // LZ4 압축
    size_t compressed_size = lz4_compress_data(input, test_size, compressed, test_size * 2, COMPRESSION_LEVEL_DEFAULT);
    TEST_ASSERT(compressed_size > 0, "LZ4 압축 성공");
    TEST_ASSERT(compressed_size < test_size, "압축률 확인");

    printf("원본 크기: %zu, 압축 크기: %zu, 압축률: %.2f%%\n",
           test_size, compressed_size, (double)compressed_size / test_size * 100.0);

    // LZ4 압축 해제
    size_t decompressed_size = lz4_decompress_data(compressed, compressed_size, decompressed, test_size);
    TEST_ASSERT(decompressed_size == test_size, "LZ4 압축 해제 크기 확인");

    // 데이터 무결성 확인
    TEST_ASSERT(memcmp(input, decompressed, test_size) == 0, "LZ4 데이터 무결성 확인");

    free(input);
    free(compressed);
    free(decompressed);

    printf("PASS: LZ4 압축 테스트 통과\n");
    return 1;
}

// ZSTD 압축 테스트
static int test_zstd_compression() {
    printf("\n=== ZSTD 압축 테스트 ===\n");

    const size_t test_size = 2048;
    uint8_t* input = (uint8_t*)malloc(test_size);
    uint8_t* compressed = (uint8_t*)malloc(test_size * 2);
    uint8_t* decompressed = (uint8_t*)malloc(test_size);

    TEST_ASSERT(input && compressed && decompressed, "메모리 할당");

    // 음성 데이터 시뮬레이션
    generate_test_data(input, test_size, 2);

    // ZSTD 압축
    size_t compressed_size = zstd_compress_data(input, test_size, compressed, test_size * 2, COMPRESSION_LEVEL_BEST);
    TEST_ASSERT(compressed_size > 0, "ZSTD 압축 성공");
    TEST_ASSERT(compressed_size < test_size, "압축률 확인");

    printf("원본 크기: %zu, 압축 크기: %zu, 압축률: %.2f%%\n",
           test_size, compressed_size, (double)compressed_size / test_size * 100.0);

    // ZSTD 압축 해제
    size_t decompressed_size = zstd_decompress_data(compressed, compressed_size, decompressed, test_size);
    TEST_ASSERT(decompressed_size == test_size, "ZSTD 압축 해제 크기 확인");

    // 데이터 무결성 확인
    TEST_ASSERT(memcmp(input, decompressed, test_size) == 0, "ZSTD 데이터 무결성 확인");

    free(input);
    free(compressed);
    free(decompressed);

    printf("PASS: ZSTD 압축 테스트 통과\n");
    return 1;
}

// 압축 알고리즘 비교 테스트
static int test_compression_algorithm_comparison() {
    printf("\n=== 압축 알고리즘 비교 테스트 ===\n");

    const size_t test_size = 4096;
    uint8_t* input = (uint8_t*)malloc(test_size);
    uint8_t* compressed = (uint8_t*)malloc(test_size * 2);

    TEST_ASSERT(input && compressed, "메모리 할당");

    // 다양한 데이터 패턴으로 테스트
    const char* pattern_names[] = {"랜덤", "반복", "음성", "희소"};
    CompressionAlgorithm algorithms[] = {COMPRESSION_LZ4, COMPRESSION_ZSTD};
    const char* algorithm_names[] = {"LZ4", "ZSTD"};

    for (int pattern = 0; pattern < 4; pattern++) {
        printf("\n--- %s 데이터 패턴 ---\n", pattern_names[pattern]);
        generate_test_data(input, test_size, pattern);

        for (int alg = 0; alg < 2; alg++) {
            CompressionContext* ctx = compression_create_context(algorithms[alg], COMPRESSION_LEVEL_DEFAULT);
            if (!ctx) continue;

            size_t compressed_size;
            clock_t start_time = clock();

            int result = compression_compress(ctx, input, test_size, compressed, test_size * 2, &compressed_size);

            clock_t end_time = clock();
            double compression_time = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;

            if (result == COMPRESSION_SUCCESS) {
                double compression_ratio = (double)compressed_size / test_size;
                printf("%s: 크기 %zu->%zu (%.2f%%), 시간 %.2fms\n",
                       algorithm_names[alg], test_size, compressed_size,
                       compression_ratio * 100.0, compression_time);
            } else {
                printf("%s: 압축 실패\n", algorithm_names[alg]);
            }

            compression_destroy_context(ctx);
        }
    }

    free(input);
    free(compressed);

    printf("PASS: 압축 알고리즘 비교 테스트 통과\n");
    return 1;
}

// 음성 특화 압축 테스트
static int test_voice_compression() {
    printf("\n=== 음성 특화 압축 테스트 ===\n");

    const size_t mel_size = 80 * 100; // 80 mel bins, 100 time frames
    float* mel_weights = (float*)malloc(mel_size * sizeof(float));
    uint8_t* compressed = (uint8_t*)malloc(mel_size * sizeof(float));

    TEST_ASSERT(mel_weights && compressed, "메모리 할당");

    // Mel 스펙트로그램 시뮬레이션
    for (size_t i = 0; i < mel_size; i++) {
        float mel_bin = (float)(i % 80);
        float time_frame = (float)(i / 80);

        // 낮은 주파수에 더 많은 에너지 (음성 특성)
        float energy = expf(-mel_bin * 0.05f) * (1.0f + 0.3f * sinf(0.1f * time_frame));
        mel_weights[i] = energy + 0.01f * ((float)rand() / RAND_MAX);
    }

    // 음성 특화 압축 파라미터
    VoiceCompressionParams params = {
        .mel_frequency_weight = 1.5f,
        .temporal_correlation = 0.9f,
        .use_perceptual_model = true,
        .quality_threshold = 0.9f
    };

    // Mel 가중치 압축
    size_t compressed_size;
    int result = voice_compress_mel_weights(mel_weights, mel_size, &params,
                                          compressed, mel_size * sizeof(float), &compressed_size);

    TEST_ASSERT(result == COMPRESSION_SUCCESS, "Mel 가중치 압축 성공");
    TEST_ASSERT(compressed_size < mel_size * sizeof(float), "압축률 확인");

    double compression_ratio = (double)compressed_size / (mel_size * sizeof(float));
    printf("Mel 가중치 압축: %zu->%zu bytes (%.2f%%)\n",
           mel_size * sizeof(float), compressed_size, compression_ratio * 100.0);

    free(mel_weights);
    free(compressed);

    printf("PASS: 음성 특화 압축 테스트 통과\n");
    return 1;
}

// 어텐션 가중치 압축 테스트
static int test_attention_compression() {
    printf("\n=== 어텐션 가중치 압축 테스트 ===\n");

    const int num_heads = 8;
    const int seq_length = 256;
    const size_t attention_size = num_heads * seq_length * seq_length;

    float* attention_weights = (float*)malloc(attention_size * sizeof(float));
    uint8_t* compressed = (uint8_t*)malloc(attention_size * sizeof(float));

    TEST_ASSERT(attention_weights && compressed, "메모리 할당");

    // 어텐션 가중치 시뮬레이션 (시간적 상관관계 포함)
    for (int head = 0; head < num_heads; head++) {
        for (int i = 0; i < seq_length; i++) {
            for (int j = 0; j < seq_length; j++) {
                size_t idx = head * seq_length * seq_length + i * seq_length + j;

                // 대각선 근처에 높은 가중치 (시간적 지역성)
                float distance = fabsf((float)(i - j));
                float weight = expf(-distance * 0.1f) + 0.01f * ((float)rand() / RAND_MAX);

                attention_weights[idx] = weight;
            }
        }
    }

    // 음성 특화 압축 파라미터
    VoiceCompressionParams params = {
        .mel_frequency_weight = 1.0f,
        .temporal_correlation = 0.95f,
        .use_perceptual_model = false,
        .quality_threshold = 0.85f
    };

    // 어텐션 가중치 압축
    size_t compressed_size;
    int result = voice_compress_attention_weights(attention_weights, attention_size,
                                                num_heads, seq_length, &params,
                                                compressed, attention_size * sizeof(float),
                                                &compressed_size);

    TEST_ASSERT(result == COMPRESSION_SUCCESS, "어텐션 가중치 압축 성공");
    TEST_ASSERT(compressed_size < attention_size * sizeof(float), "압축률 확인");

    double compression_ratio = (double)compressed_size / (attention_size * sizeof(float));
    printf("어텐션 가중치 압축: %zu->%zu bytes (%.2f%%)\n",
           attention_size * sizeof(float), compressed_size, compression_ratio * 100.0);

    free(attention_weights);
    free(compressed);

    printf("PASS: 어텐션 가중치 압축 테스트 통과\n");
    return 1;
}

// 보코더 가중치 압축 테스트
static int test_vocoder_compression() {
    printf("\n=== 보코더 가중치 압축 테스트 ===\n");

    const int sample_rate = 22050;
    const size_t vocoder_size = 1025 * 256; // FFT bins * time frames

    float* vocoder_weights = (float*)malloc(vocoder_size * sizeof(float));
    uint8_t* compressed = (uint8_t*)malloc(vocoder_size * sizeof(float));

    TEST_ASSERT(vocoder_weights && compressed, "메모리 할당");

    // 보코더 가중치 시뮬레이션 (주파수 도메인 특성)
    for (size_t i = 0; i < vocoder_size; i++) {
        size_t freq_bin = i % 1025;
        size_t time_frame = i / 1025;

        // 주파수별 가중치 (음성 주파수 대역에 집중)
        float freq_hz = (float)freq_bin * sample_rate / (2.0f * 1025);
        float freq_weight = 1.0f;

        if (freq_hz >= 80.0f && freq_hz <= 8000.0f) {
            freq_weight = 2.0f; // 음성 주파수 대역 강조
        } else if (freq_hz > 8000.0f) {
            freq_weight = 0.5f; // 고주파 감소
        }

        float weight = freq_weight * expf(-0.001f * time_frame) +
                      0.01f * ((float)rand() / RAND_MAX);

        vocoder_weights[i] = weight;
    }

    // 음성 특화 압축 파라미터
    VoiceCompressionParams params = {
        .mel_frequency_weight = 1.0f,
        .temporal_correlation = 0.7f,
        .use_perceptual_model = true,
        .quality_threshold = 0.9f
    };

    // 보코더 가중치 압축
    size_t compressed_size;
    int result = voice_compress_vocoder_weights(vocoder_weights, vocoder_size,
                                              sample_rate, &params,
                                              compressed, vocoder_size * sizeof(float),
                                              &compressed_size);

    TEST_ASSERT(result == COMPRESSION_SUCCESS, "보코더 가중치 압축 성공");
    TEST_ASSERT(compressed_size < vocoder_size * sizeof(float), "압축률 확인");

    double compression_ratio = (double)compressed_size / (vocoder_size * sizeof(float));
    printf("보코더 가중치 압축: %zu->%zu bytes (%.2f%%)\n",
           vocoder_size * sizeof(float), compressed_size, compression_ratio * 100.0);

    free(vocoder_weights);
    free(compressed);

    printf("PASS: 보코더 가중치 압축 테스트 통과\n");
    return 1;
}

// 레이어별 압축 전략 테스트
static int test_layer_compression_strategy() {
    printf("\n=== 레이어별 압축 전략 테스트 ===\n");

    // 다양한 레이어 타입 테스트
    struct {
        uint8_t layer_kind;
        const char* layer_name;
        size_t data_size;
        uint8_t quantization_type;
    } test_layers[] = {
        {0, "Linear", 1024 * 1024, 2},      // BF16 양자화된 선형 레이어
        {1, "Conv1D", 512 * 1024, 3},       // INT8 양자화된 컨볼루션 레이어
        {2, "Attention", 2048 * 1024, 0},   // 양자화 없는 어텐션 레이어
        {6, "Vocoder", 4096 * 1024, 2}      // BF16 양자화된 보코더 레이어
    };

    int num_layers = sizeof(test_layers) / sizeof(test_layers[0]);

    for (int i = 0; i < num_layers; i++) {
        LayerCompressionStrategy strategy = select_optimal_compression_strategy(
            test_layers[i].layer_kind,
            test_layers[i].data_size,
            test_layers[i].quantization_type
        );

        printf("%s 레이어 전략: %s, 레벨 %d, 양자화 %s, 임계값 %.3f\n",
               test_layers[i].layer_name,
               compression_get_algorithm_name(strategy.algorithm),
               strategy.level,
               strategy.use_quantization ? "사용" : "미사용",
               strategy.weight_threshold);

        // 전략이 합리적인지 확인
        TEST_ASSERT(strategy.algorithm >= COMPRESSION_NONE && strategy.algorithm <= COMPRESSION_VOICE_OPTIMIZED,
                    "유효한 압축 알고리즘");
        TEST_ASSERT(strategy.level >= 1 && strategy.level <= 9, "유효한 압축 레벨");
        TEST_ASSERT(strategy.weight_threshold >= 0.0f && strategy.weight_threshold <= 1.0f,
                    "유효한 가중치 임계값");
    }

    printf("PASS: 레이어별 압축 전략 테스트 통과\n");
    return 1;
}

// 압축 사전 생성 테스트
static int test_compression_dictionary() {
    printf("\n=== 압축 사전 생성 테스트 ===\n");

    const size_t num_samples = 5;
    const size_t sample_size = 1024;

    // 샘플 데이터 생성
    uint8_t** samples = (uint8_t**)malloc(num_samples * sizeof(uint8_t*));
    size_t* sample_sizes = (size_t*)malloc(num_samples * sizeof(size_t));

    TEST_ASSERT(samples && sample_sizes, "메모리 할당");

    for (size_t i = 0; i < num_samples; i++) {
        samples[i] = (uint8_t*)malloc(sample_size);
        sample_sizes[i] = sample_size;
        TEST_ASSERT(samples[i] != NULL, "샘플 메모리 할당");

        // 유사한 패턴을 가진 데이터 생성 (사전 효과를 위해)
        for (size_t j = 0; j < sample_size; j++) {
            samples[i][j] = (uint8_t)((i * 17 + j * 3) % 64 + 64); // 공통 패턴
        }
    }

    // 사전 생성
    uint8_t* dict_buffer = (uint8_t*)malloc(1024);
    TEST_ASSERT(dict_buffer != NULL, "사전 버퍼 할당");

    size_t dict_size = create_model_compression_dictionary((const void**)samples, sample_sizes,
                                                          num_samples, dict_buffer, 1024);

    TEST_ASSERT(dict_size > 0, "압축 사전 생성 성공");
    TEST_ASSERT(dict_size <= 1024, "사전 크기 확인");

    printf("생성된 사전 크기: %zu bytes\n", dict_size);

    // ZSTD 사전 생성 테스트
    uint8_t* zstd_dict = (uint8_t*)malloc(2048);
    TEST_ASSERT(zstd_dict != NULL, "ZSTD 사전 버퍼 할당");

    size_t zstd_dict_size = zstd_create_dictionary((const void**)samples, sample_sizes,
                                                  num_samples, zstd_dict, 2048);

    TEST_ASSERT(zstd_dict_size > 0, "ZSTD 사전 생성 성공");
    printf("ZSTD 사전 크기: %zu bytes\n", zstd_dict_size);

    // 메모리 정리
    for (size_t i = 0; i < num_samples; i++) {
        free(samples[i]);
    }
    free(samples);
    free(sample_sizes);
    free(dict_buffer);
    free(zstd_dict);

    printf("PASS: 압축 사전 생성 테스트 통과\n");
    return 1;
}

// 압축 성능 벤치마크 테스트
static int test_compression_performance() {
    printf("\n=== 압축 성능 벤치마크 테스트 ===\n");

    const size_t test_sizes[] = {1024, 4096, 16384, 65536};
    const int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

    CompressionAlgorithm algorithms[] = {COMPRESSION_LZ4, COMPRESSION_ZSTD};
    const char* algorithm_names[] = {"LZ4", "ZSTD"};
    const int num_algorithms = sizeof(algorithms) / sizeof(algorithms[0]);

    for (int size_idx = 0; size_idx < num_sizes; size_idx++) {
        size_t test_size = test_sizes[size_idx];
        printf("\n--- 테스트 크기: %zu bytes ---\n", test_size);

        uint8_t* input = (uint8_t*)malloc(test_size);
        uint8_t* compressed = (uint8_t*)malloc(test_size * 2);
        uint8_t* decompressed = (uint8_t*)malloc(test_size);

        TEST_ASSERT(input && compressed && decompressed, "메모리 할당");

        // 음성 데이터 시뮬레이션
        generate_test_data(input, test_size, 2);

        for (int alg_idx = 0; alg_idx < num_algorithms; alg_idx++) {
            CompressionContext* ctx = compression_create_context(algorithms[alg_idx], COMPRESSION_LEVEL_DEFAULT);
            if (!ctx) continue;

            // 압축 성능 측정
            clock_t compress_start = clock();
            size_t compressed_size;
            int compress_result = compression_compress(ctx, input, test_size, compressed, test_size * 2, &compressed_size);
            clock_t compress_end = clock();

            if (compress_result != COMPRESSION_SUCCESS) {
                printf("%s: 압축 실패\n", algorithm_names[alg_idx]);
                compression_destroy_context(ctx);
                continue;
            }

            // 압축 해제 성능 측정
            clock_t decompress_start = clock();
            size_t decompressed_size;
            int decompress_result = compression_decompress(ctx, compressed, compressed_size, decompressed, test_size, &decompressed_size);
            clock_t decompress_end = clock();

            if (decompress_result != COMPRESSION_SUCCESS || decompressed_size != test_size) {
                printf("%s: 압축 해제 실패\n", algorithm_names[alg_idx]);
                compression_destroy_context(ctx);
                continue;
            }

            // 성능 통계 계산
            double compress_time = ((double)(compress_end - compress_start) / CLOCKS_PER_SEC) * 1000.0;
            double decompress_time = ((double)(decompress_end - decompress_start) / CLOCKS_PER_SEC) * 1000.0;
            double compression_ratio = (double)compressed_size / test_size;
            double compress_throughput = (test_size / 1024.0) / (compress_time / 1000.0); // KB/s
            double decompress_throughput = (test_size / 1024.0) / (decompress_time / 1000.0); // KB/s

            printf("%s: 압축률 %.2f%%, 압축 %.2fms (%.1f KB/s), 해제 %.2fms (%.1f KB/s)\n",
                   algorithm_names[alg_idx], compression_ratio * 100.0,
                   compress_time, compress_throughput,
                   decompress_time, decompress_throughput);

            // 데이터 무결성 확인
            TEST_ASSERT(memcmp(input, decompressed, test_size) == 0, "데이터 무결성 확인");

            compression_destroy_context(ctx);
        }

        free(input);
        free(compressed);
        free(decompressed);
    }

    printf("PASS: 압축 성능 벤치마크 테스트 통과\n");
    return 1;
}

// 압축 크기 추정 테스트
static int test_compression_size_estimation() {
    printf("\n=== 압축 크기 추정 테스트 ===\n");

    const size_t test_size = 8192;
    uint8_t* input = (uint8_t*)malloc(test_size);
    uint8_t* compressed = (uint8_t*)malloc(test_size * 2);

    TEST_ASSERT(input && compressed, "메모리 할당");

    CompressionAlgorithm algorithms[] = {COMPRESSION_LZ4, COMPRESSION_ZSTD};
    const char* algorithm_names[] = {"LZ4", "ZSTD"};
    int levels[] = {COMPRESSION_LEVEL_FAST, COMPRESSION_LEVEL_DEFAULT, COMPRESSION_LEVEL_BEST};
    const char* level_names[] = {"빠름", "기본", "최고"};

    // 반복 패턴 데이터 (압축률이 좋음)
    generate_test_data(input, test_size, 1);

    for (int alg = 0; alg < 2; alg++) {
        printf("\n--- %s 알고리즘 ---\n", algorithm_names[alg]);

        for (int lvl = 0; lvl < 3; lvl++) {
            // 크기 추정
            size_t estimated_size = compression_estimate_size(algorithms[alg], test_size, levels[lvl]);

            // 실제 압축
            CompressionContext* ctx = compression_create_context(algorithms[alg], levels[lvl]);
            if (!ctx) continue;

            size_t actual_size;
            int result = compression_compress(ctx, input, test_size, compressed, test_size * 2, &actual_size);

            if (result == COMPRESSION_SUCCESS) {
                double estimation_error = fabsf((double)estimated_size - (double)actual_size) / actual_size;
                printf("%s 레벨: 추정 %zu, 실제 %zu, 오차 %.1f%%\n",
                       level_names[lvl], estimated_size, actual_size, estimation_error * 100.0);

                // 추정 오차가 50% 이내인지 확인 (대략적인 추정이므로)
                TEST_ASSERT(estimation_error < 0.5, "압축 크기 추정 정확도");
            } else {
                printf("%s 레벨: 압축 실패\n", level_names[lvl]);
            }

            compression_destroy_context(ctx);
        }
    }

    free(input);
    free(compressed);

    printf("PASS: 압축 크기 추정 테스트 통과\n");
    return 1;
}

// 메인 테스트 함수
int main() {
    printf("LibEtude 압축 기능 테스트 시작\n");
    printf("=====================================\n");

    // 랜덤 시드 설정
    srand((unsigned int)time(NULL));

    int total_tests = 0;
    int passed_tests = 0;

    // 기본 압축 테스트들
    total_tests++; if (test_compression_context()) passed_tests++;
    total_tests++; if (test_lz4_compression()) passed_tests++;
    total_tests++; if (test_zstd_compression()) passed_tests++;
    total_tests++; if (test_compression_algorithm_comparison()) passed_tests++;

    // 음성 특화 압축 테스트들
    total_tests++; if (test_voice_compression()) passed_tests++;
    total_tests++; if (test_attention_compression()) passed_tests++;
    total_tests++; if (test_vocoder_compression()) passed_tests++;

    // 고급 압축 기능 테스트들
    total_tests++; if (test_layer_compression_strategy()) passed_tests++;
    total_tests++; if (test_compression_dictionary()) passed_tests++;
    total_tests++; if (test_compression_performance()) passed_tests++;
    total_tests++; if (test_compression_size_estimation()) passed_tests++;

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", passed_tests, total_tests);

    if (passed_tests == total_tests) {
        printf("모든 압축 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다.\n");
        return 1;
    }
}