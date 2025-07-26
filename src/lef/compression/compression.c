#include "libetude/compression.h"
#include "libetude/lef_format.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

// 간단한 LZ4 스타일 압축 구현 (실제 LZ4 라이브러리 대신 사용)
// 실제 프로덕션에서는 외부 LZ4/ZSTD 라이브러리를 사용해야 함

// 해시 테이블 크기 (2^16)
#define HASH_TABLE_SIZE 65536
#define HASH_MASK (HASH_TABLE_SIZE - 1)

// 최소 매치 길이
#define MIN_MATCH_LENGTH 4
#define MAX_MATCH_LENGTH 255

// 압축 블록 크기
#define DEFAULT_BLOCK_SIZE (64 * 1024)  // 64KB

/**
 * 간단한 해시 함수
 */
static inline uint32_t hash_function(const uint8_t* data) {
    uint32_t hash = (data[0] << 16) | (data[1] << 8) | data[2];
    return (hash * 2654435761U) >> (32 - 16);
}

/**
 * 메모리 비교 (매치 길이 계산)
 */
static size_t find_match_length(const uint8_t* src, const uint8_t* match,
                               const uint8_t* src_end, size_t max_len) {
    size_t len = 0;
    while (src < src_end && len < max_len && *src == *match) {
        src++;
        match++;
        len++;
    }
    return len;
}

// ============================================================================
// 기본 압축 함수 구현
// ============================================================================

/**
 * 압축 컨텍스트 생성
 */
CompressionContext* compression_create_context(CompressionAlgorithm algorithm, int level) {
    if (level < 1 || level > 9) {
        return NULL;
    }

    CompressionContext* ctx = (CompressionContext*)malloc(sizeof(CompressionContext));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(CompressionContext));

    ctx->algorithm = algorithm;
    ctx->level = level;
    ctx->block_size = DEFAULT_BLOCK_SIZE;
    ctx->use_dictionary = false;
    ctx->dictionary_data = NULL;
    ctx->dictionary_size = 0;
    ctx->internal_context = NULL;

    // 알고리즘별 초기화
    switch (algorithm) {
        case COMPRESSION_LZ4:
            // LZ4 컨텍스트 초기화 (해시 테이블)
            ctx->internal_context = calloc(HASH_TABLE_SIZE, sizeof(uint32_t));
            break;
        case COMPRESSION_ZSTD:
            // ZSTD 컨텍스트 초기화
            ctx->internal_context = calloc(HASH_TABLE_SIZE * 2, sizeof(uint32_t));
            break;
        case COMPRESSION_VOICE_OPTIMIZED:
            // 음성 특화 압축 컨텍스트 초기화
            ctx->internal_context = malloc(sizeof(VoiceCompressionParams));
            if (ctx->internal_context) {
                VoiceCompressionParams* params = (VoiceCompressionParams*)ctx->internal_context;
                params->mel_frequency_weight = 1.2f;
                params->temporal_correlation = 0.8f;
                params->use_perceptual_model = true;
                params->quality_threshold = 0.95f;
            }
            break;
        default:
            break;
    }

    if (algorithm != COMPRESSION_NONE && !ctx->internal_context) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * 압축 컨텍스트 해제
 */
void compression_destroy_context(CompressionContext* ctx) {
    if (!ctx) {
        return;
    }

    free(ctx->internal_context);
    free(ctx->dictionary_data);
    free(ctx);
}

/**
 * 간단한 LZ4 스타일 압축 구현
 */
static size_t simple_lz4_compress(const uint8_t* input, size_t input_size,
                                 uint8_t* output, size_t output_capacity,
                                 uint32_t* hash_table) {
    if (!input || !output || input_size == 0 || output_capacity < input_size + 16) {
        return 0;
    }

    // 해시 테이블 초기화
    memset(hash_table, 0, HASH_TABLE_SIZE * sizeof(uint32_t));

    const uint8_t* input_end = input + input_size;
    const uint8_t* src = input;
    uint8_t* dst = output;
    uint8_t* dst_end = output + output_capacity;

    // 압축 시작
    while (src < input_end - MIN_MATCH_LENGTH) {
        // 현재 위치의 해시 계산
        uint32_t hash = hash_function(src) & HASH_MASK;
        uint32_t match_pos = hash_table[hash];
        hash_table[hash] = (uint32_t)(src - input);

        // 매치 검사
        if (match_pos > 0 && (src - input) - match_pos < 65535) {
            const uint8_t* match = input + match_pos;
            size_t match_len = find_match_length(src, match, input_end, MAX_MATCH_LENGTH);

            if (match_len >= MIN_MATCH_LENGTH) {
                // 매치 발견 - 압축 토큰 생성
                if (dst + 3 >= dst_end) break;

                uint32_t offset = (uint32_t)(src - match);

                // 토큰 형식: [길이(1바이트)] [오프셋(2바이트)]
                *dst++ = (uint8_t)match_len;
                *dst++ = (uint8_t)(offset & 0xFF);
                *dst++ = (uint8_t)((offset >> 8) & 0xFF);

                src += match_len;
                continue;
            }
        }

        // 매치 없음 - 리터럴 복사
        if (dst >= dst_end) break;
        *dst++ = *src++;
    }

    // 남은 데이터 복사
    while (src < input_end && dst < dst_end) {
        *dst++ = *src++;
    }

    return (size_t)(dst - output);
}

/**
 * 간단한 LZ4 스타일 압축 해제
 */
static size_t simple_lz4_decompress(const uint8_t* input, size_t input_size,
                                   uint8_t* output, size_t output_capacity) {
    if (!input || !output || input_size == 0) {
        return 0;
    }

    const uint8_t* src = input;
    const uint8_t* src_end = input + input_size;
    uint8_t* dst = output;
    uint8_t* dst_end = output + output_capacity;

    while (src < src_end && dst < dst_end) {
        uint8_t token = *src++;

        // 압축 토큰인지 확인 (길이가 MIN_MATCH_LENGTH 이상)
        if (token >= MIN_MATCH_LENGTH && src + 2 <= src_end) {
            // 압축된 데이터 - 오프셋과 길이 읽기
            uint32_t offset = src[0] | (src[1] << 8);
            src += 2;

            if (offset > 0 && dst - offset >= output) {
                const uint8_t* match = dst - offset;
                size_t len = token;

                // 매치 데이터 복사
                for (size_t i = 0; i < len && dst < dst_end; i++) {
                    *dst++ = match[i];
                }
            } else {
                // 잘못된 오프셋 - 리터럴로 처리
                if (dst < dst_end) *dst++ = token;
            }
        } else {
            // 리터럴 데이터
            *dst++ = token;
        }
    }

    return (size_t)(dst - output);
}

/**
 * 데이터 압축
 */
int compression_compress(CompressionContext* ctx, const void* input, size_t input_size,
                        void* output, size_t output_capacity, size_t* compressed_size) {
    if (!ctx || !input || !output || !compressed_size || input_size == 0) {
        return COMPRESSION_ERROR_INVALID_ARGUMENT;
    }

    *compressed_size = 0;

    switch (ctx->algorithm) {
        case COMPRESSION_NONE:
            if (output_capacity < input_size) {
                return COMPRESSION_ERROR_BUFFER_TOO_SMALL;
            }
            memcpy(output, input, input_size);
            *compressed_size = input_size;
            return COMPRESSION_SUCCESS;

        case COMPRESSION_LZ4: {
            uint32_t* hash_table = (uint32_t*)ctx->internal_context;
            size_t result = simple_lz4_compress((const uint8_t*)input, input_size,
                                              (uint8_t*)output, output_capacity, hash_table);
            if (result == 0) {
                return COMPRESSION_ERROR_COMPRESSION_FAILED;
            }
            *compressed_size = result;
            return COMPRESSION_SUCCESS;
        }

        case COMPRESSION_ZSTD: {
            // ZSTD 압축 (간단한 구현)
            size_t result = zstd_compress_data(input, input_size, output, output_capacity, ctx->level);
            if (result == 0) {
                return COMPRESSION_ERROR_COMPRESSION_FAILED;
            }
            *compressed_size = result;
            return COMPRESSION_SUCCESS;
        }

        case COMPRESSION_VOICE_OPTIMIZED: {
            // 음성 특화 압축
            VoiceCompressionParams* params = (VoiceCompressionParams*)ctx->internal_context;
            return voice_compress_mel_weights((const float*)input, input_size / sizeof(float),
                                            params, output, output_capacity, compressed_size);
        }

        default:
            return COMPRESSION_ERROR_UNSUPPORTED_ALGORITHM;
    }
}

/**
 * 데이터 압축 해제
 */
int compression_decompress(CompressionContext* ctx, const void* input, size_t input_size,
                          void* output, size_t output_capacity, size_t* decompressed_size) {
    if (!ctx || !input || !output || !decompressed_size || input_size == 0) {
        return COMPRESSION_ERROR_INVALID_ARGUMENT;
    }

    *decompressed_size = 0;

    switch (ctx->algorithm) {
        case COMPRESSION_NONE:
            if (output_capacity < input_size) {
                return COMPRESSION_ERROR_BUFFER_TOO_SMALL;
            }
            memcpy(output, input, input_size);
            *decompressed_size = input_size;
            return COMPRESSION_SUCCESS;

        case COMPRESSION_LZ4: {
            size_t result = simple_lz4_decompress((const uint8_t*)input, input_size,
                                                (uint8_t*)output, output_capacity);
            if (result == 0) {
                return COMPRESSION_ERROR_DECOMPRESSION_FAILED;
            }
            *decompressed_size = result;
            return COMPRESSION_SUCCESS;
        }

        case COMPRESSION_ZSTD: {
            size_t result = zstd_decompress_data(input, input_size, output, output_capacity);
            if (result == 0) {
                return COMPRESSION_ERROR_DECOMPRESSION_FAILED;
            }
            *decompressed_size = result;
            return COMPRESSION_SUCCESS;
        }

        default:
            return COMPRESSION_ERROR_UNSUPPORTED_ALGORITHM;
    }
}

/**
 * 압축된 크기 추정
 */
size_t compression_estimate_size(CompressionAlgorithm algorithm, size_t input_size, int level) {
    switch (algorithm) {
        case COMPRESSION_NONE:
            return input_size;
        case COMPRESSION_LZ4:
            // LZ4는 일반적으로 50-70% 압축률
            return (size_t)(input_size * 0.6);
        case COMPRESSION_ZSTD:
            // ZSTD는 레벨에 따라 30-60% 압축률
            return (size_t)(input_size * (0.7 - level * 0.05));
        case COMPRESSION_VOICE_OPTIMIZED:
            // 음성 특화 압축은 더 높은 압축률 (20-40%)
            return (size_t)(input_size * 0.3);
        default:
            return input_size;
    }
}

// ============================================================================
// LZ4 압축 함수 구현
// ============================================================================

/**
 * LZ4 압축
 */
size_t lz4_compress_data(const void* input, size_t input_size, void* output,
                        size_t output_capacity, int level) {
    CompressionContext* ctx = compression_create_context(COMPRESSION_LZ4, level);
    if (!ctx) {
        return 0;
    }

    size_t compressed_size;
    int result = compression_compress(ctx, input, input_size, output, output_capacity, &compressed_size);

    compression_destroy_context(ctx);

    return (result == COMPRESSION_SUCCESS) ? compressed_size : 0;
}

/**
 * LZ4 압축 해제
 */
size_t lz4_decompress_data(const void* input, size_t input_size, void* output,
                          size_t output_capacity) {
    CompressionContext* ctx = compression_create_context(COMPRESSION_LZ4, 1);
    if (!ctx) {
        return 0;
    }

    size_t decompressed_size;
    int result = compression_decompress(ctx, input, input_size, output, output_capacity, &decompressed_size);

    compression_destroy_context(ctx);

    return (result == COMPRESSION_SUCCESS) ? decompressed_size : 0;
}

// ============================================================================
// ZSTD 압축 함수 구현 (간단한 버전)
// ============================================================================

/**
 * 간단한 ZSTD 스타일 압축 (실제로는 개선된 LZ 압축)
 */
size_t zstd_compress_data(const void* input, size_t input_size, void* output,
                         size_t output_capacity, int level) {
    // 실제 ZSTD 라이브러리 대신 개선된 압축 알고리즘 사용
    if (!input || !output || input_size == 0 || output_capacity < input_size / 2) {
        return 0;
    }

    const uint8_t* src = (const uint8_t*)input;
    uint8_t* dst = (uint8_t*)output;
    size_t src_pos = 0;
    size_t dst_pos = 0;

    // 간단한 RLE + LZ 조합 압축
    while (src_pos < input_size && dst_pos < output_capacity - 4) {
        uint8_t current = src[src_pos];
        size_t run_length = 1;

        // RLE 검사
        while (src_pos + run_length < input_size &&
               src[src_pos + run_length] == current &&
               run_length < 255) {
            run_length++;
        }

        if (run_length >= 4) {
            // RLE 토큰: [0xFF] [길이] [값]
            dst[dst_pos++] = 0xFF;
            dst[dst_pos++] = (uint8_t)run_length;
            dst[dst_pos++] = current;
            src_pos += run_length;
        } else {
            // 일반 데이터
            dst[dst_pos++] = current;
            src_pos++;
        }
    }

    return dst_pos;
}

/**
 * 간단한 ZSTD 스타일 압축 해제
 */
size_t zstd_decompress_data(const void* input, size_t input_size, void* output,
                           size_t output_capacity) {
    if (!input || !output || input_size == 0) {
        return 0;
    }

    const uint8_t* src = (const uint8_t*)input;
    uint8_t* dst = (uint8_t*)output;
    size_t src_pos = 0;
    size_t dst_pos = 0;

    while (src_pos < input_size && dst_pos < output_capacity) {
        if (src[src_pos] == 0xFF && src_pos + 2 < input_size) {
            // RLE 토큰
            size_t run_length = src[src_pos + 1];
            uint8_t value = src[src_pos + 2];

            for (size_t i = 0; i < run_length && dst_pos < output_capacity; i++) {
                dst[dst_pos++] = value;
            }
            src_pos += 3;
        } else {
            // 일반 데이터
            dst[dst_pos++] = src[src_pos++];
        }
    }

    return dst_pos;
}

/**
 * ZSTD 사전 생성 (간단한 구현)
 */
size_t zstd_create_dictionary(const void** samples, const size_t* sample_sizes,
                             size_t num_samples, void* dict_buffer, size_t dict_capacity) {
    if (!samples || !sample_sizes || !dict_buffer || num_samples == 0) {
        return 0;
    }

    // 간단한 사전 생성: 가장 빈번한 바이트 패턴 수집
    uint32_t byte_freq[256] = {0};
    size_t total_size = 0;

    // 바이트 빈도 계산
    for (size_t i = 0; i < num_samples; i++) {
        const uint8_t* data = (const uint8_t*)samples[i];
        for (size_t j = 0; j < sample_sizes[i]; j++) {
            byte_freq[data[j]]++;
            total_size++;
        }
    }

    // 빈도 순으로 정렬하여 사전 생성
    uint8_t* dict = (uint8_t*)dict_buffer;
    size_t dict_pos = 0;

    for (int freq_threshold = 1000; freq_threshold > 0 && dict_pos < dict_capacity; freq_threshold /= 2) {
        for (int byte_val = 0; byte_val < 256 && dict_pos < dict_capacity; byte_val++) {
            if (byte_freq[byte_val] >= (uint32_t)freq_threshold) {
                dict[dict_pos++] = (uint8_t)byte_val;
            }
        }
    }

    return dict_pos;
}

// ============================================================================
// 레이어별 압축 전략 구현
// ============================================================================

/**
 * 레이어 타입에 따른 최적 압축 전략 선택
 */
LayerCompressionStrategy select_optimal_compression_strategy(uint8_t layer_kind,
                                                           size_t data_size,
                                                           uint8_t quantization_type) {
    LayerCompressionStrategy strategy;

    // 기본값 설정
    strategy.algorithm = COMPRESSION_LZ4;
    strategy.level = COMPRESSION_LEVEL_DEFAULT;
    strategy.use_quantization = (quantization_type != 0);
    strategy.weight_threshold = 0.01f;

    switch (layer_kind) {
        case 0: // LEF_LAYER_LINEAR
            // 선형 레이어는 일반적으로 압축률이 좋음
            strategy.algorithm = COMPRESSION_ZSTD;
            strategy.level = COMPRESSION_LEVEL_BEST;
            break;

        case 1: // LEF_LAYER_CONV1D
            // 컨볼루션 레이어는 패턴이 많아 압축 효과적
            strategy.algorithm = COMPRESSION_ZSTD;
            strategy.level = COMPRESSION_LEVEL_DEFAULT;
            break;

        case 2: // LEF_LAYER_ATTENTION
            // 어텐션 레이어는 희소성 활용
            strategy.algorithm = COMPRESSION_VOICE_OPTIMIZED;
            strategy.level = COMPRESSION_LEVEL_DEFAULT;
            strategy.weight_threshold = 0.001f;
            break;

        case 6: // LEF_LAYER_VOCODER
            // 보코더는 음성 특화 압축 사용
            strategy.algorithm = COMPRESSION_VOICE_OPTIMIZED;
            strategy.level = COMPRESSION_LEVEL_BEST;
            break;

        default:
            // 기본 전략 사용
            if (data_size > 1024 * 1024) {  // 1MB 이상
                strategy.algorithm = COMPRESSION_ZSTD;
                strategy.level = COMPRESSION_LEVEL_DEFAULT;
            } else {
                strategy.algorithm = COMPRESSION_LZ4;
                strategy.level = COMPRESSION_LEVEL_FAST;
            }
            break;
    }

    return strategy;
}

/**
 * 가중치 데이터 분석 및 압축 전략 추천
 */
LayerCompressionStrategy analyze_weights_for_compression(const void* weights,
                                                        size_t size,
                                                        uint8_t dtype) {
    LayerCompressionStrategy strategy;
    strategy.algorithm = COMPRESSION_LZ4;
    strategy.level = COMPRESSION_LEVEL_DEFAULT;
    strategy.use_quantization = false;
    strategy.weight_threshold = 0.01f;

    if (!weights || size == 0) {
        return strategy;
    }

    // 데이터 타입에 따른 분석
    if (dtype == 0) {  // float32
        const float* float_weights = (const float*)weights;
        size_t num_elements = size / sizeof(float);

        // 희소성 분석
        size_t zero_count = 0;
        float sum_abs = 0.0f;

        for (size_t i = 0; i < num_elements; i++) {
            float val = float_weights[i];
            if (fabsf(val) < 1e-6f) {
                zero_count++;
            }
            sum_abs += fabsf(val);
        }

        float sparsity = (float)zero_count / num_elements;
        float avg_magnitude = sum_abs / num_elements;

        // 희소성이 높으면 특화 압축 사용
        if (sparsity > 0.5f) {
            strategy.algorithm = COMPRESSION_VOICE_OPTIMIZED;
            strategy.weight_threshold = avg_magnitude * 0.1f;
        } else if (sparsity > 0.2f) {
            strategy.algorithm = COMPRESSION_ZSTD;
            strategy.level = COMPRESSION_LEVEL_BEST;
        }
    }

    return strategy;
}

/**
 * 레이어별 압축 적용
 */
int apply_layer_compression(const void* weights, size_t size,
                           const LayerCompressionStrategy* strategy,
                           void* output, size_t output_capacity,
                           size_t* compressed_size, CompressionStats* stats) {
    if (!weights || !strategy || !output || !compressed_size) {
        return COMPRESSION_ERROR_INVALID_ARGUMENT;
    }

    clock_t start_time = clock();

    CompressionContext* ctx = compression_create_context(strategy->algorithm, strategy->level);
    if (!ctx) {
        return COMPRESSION_ERROR_OUT_OF_MEMORY;
    }

    int result = compression_compress(ctx, weights, size, output, output_capacity, compressed_size);

    clock_t end_time = clock();

    // 통계 정보 업데이트
    if (stats) {
        stats->original_size = size;
        stats->compressed_size = *compressed_size;
        stats->compression_ratio = (double)*compressed_size / size;
        stats->compression_time_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
        stats->decompression_time_ms = 0.0;  // 압축 해제 시간은 별도 측정
    }

    compression_destroy_context(ctx);
    return result;
}

// ============================================================================
// 유틸리티 함수 구현
// ============================================================================

/**
 * 압축 알고리즘 이름 반환
 */
const char* compression_get_algorithm_name(CompressionAlgorithm algorithm) {
    switch (algorithm) {
        case COMPRESSION_NONE:
            return "None";
        case COMPRESSION_LZ4:
            return "LZ4";
        case COMPRESSION_ZSTD:
            return "ZSTD";
        case COMPRESSION_VOICE_OPTIMIZED:
            return "Voice-Optimized";
        default:
            return "Unknown";
    }
}

/**
 * 압축 통계 출력
 */
void compression_print_stats(const CompressionStats* stats) {
    if (!stats) {
        return;
    }

    printf("압축 통계:\n");
    printf("  원본 크기: %zu bytes\n", stats->original_size);
    printf("  압축 크기: %zu bytes\n", stats->compressed_size);
    printf("  압축률: %.2f%%\n", stats->compression_ratio * 100.0);
    printf("  압축 시간: %.2f ms\n", stats->compression_time_ms);
    if (stats->decompression_time_ms > 0) {
        printf("  압축 해제 시간: %.2f ms\n", stats->decompression_time_ms);
    }
}

/**
 * 최적 압축 알고리즘 선택
 */
CompressionAlgorithm select_optimal_algorithm(const void* data, size_t size,
                                             double target_ratio, double max_time_ms) {
    if (!data || size == 0) {
        return COMPRESSION_NONE;
    }

    // 크기가 작으면 압축하지 않음
    if (size < 1024) {
        return COMPRESSION_NONE;
    }

    // 시간 제약이 매우 엄격하면 LZ4 사용
    if (max_time_ms < 10.0) {
        return COMPRESSION_LZ4;
    }

    // 압축률 요구사항에 따라 선택
    if (target_ratio < 0.3) {
        return COMPRESSION_VOICE_OPTIMIZED;
    } else if (target_ratio < 0.6) {
        return COMPRESSION_ZSTD;
    } else {
        return COMPRESSION_LZ4;
    }
}