#ifndef LIBETUDE_COMPRESSION_H
#define LIBETUDE_COMPRESSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 압축 알고리즘 타입
typedef enum {
    COMPRESSION_NONE = 0,        // 압축 없음
    COMPRESSION_LZ4 = 1,         // LZ4 압축
    COMPRESSION_ZSTD = 2,        // ZSTD 압축
    COMPRESSION_VOICE_OPTIMIZED = 3  // 음성 특화 압축
} CompressionAlgorithm;

// 압축 레벨 정의
#define COMPRESSION_LEVEL_FASTEST   1
#define COMPRESSION_LEVEL_FAST      3
#define COMPRESSION_LEVEL_DEFAULT   6
#define COMPRESSION_LEVEL_BEST      9

// 압축 컨텍스트 구조체
typedef struct {
    CompressionAlgorithm algorithm;  // 사용할 압축 알고리즘
    int level;                       // 압축 레벨 (1-9)
    size_t block_size;              // 블록 크기
    bool use_dictionary;            // 사전 사용 여부
    void* dictionary_data;          // 압축 사전 데이터
    size_t dictionary_size;         // 사전 크기
    void* internal_context;         // 내부 압축 컨텍스트
} CompressionContext;

// 압축 통계 정보
typedef struct {
    size_t original_size;           // 원본 크기
    size_t compressed_size;         // 압축된 크기
    double compression_ratio;       // 압축률
    double compression_time_ms;     // 압축 시간 (밀리초)
    double decompression_time_ms;   // 압축 해제 시간 (밀리초)
} CompressionStats;

// 레이어별 압축 전략
typedef struct {
    CompressionAlgorithm algorithm; // 레이어별 압축 알고리즘
    int level;                      // 레이어별 압축 레벨
    bool use_quantization;          // 양자화와 함께 사용 여부
    float weight_threshold;         // 가중치 임계값 (희소성 활용)
} LayerCompressionStrategy;

// 음성 특화 압축 파라미터
typedef struct {
    float mel_frequency_weight;     // Mel 주파수 가중치
    float temporal_correlation;     // 시간적 상관관계 활용도
    bool use_perceptual_model;      // 지각 모델 사용 여부
    float quality_threshold;        // 품질 임계값
} VoiceCompressionParams;

// ============================================================================
// 기본 압축 함수들
// ============================================================================

/**
 * 압축 컨텍스트 생성
 * @param algorithm 압축 알고리즘
 * @param level 압축 레벨 (1-9)
 * @return 생성된 컨텍스트 포인터 (실패 시 NULL)
 */
CompressionContext* compression_create_context(CompressionAlgorithm algorithm, int level);

/**
 * 압축 컨텍스트 해제
 * @param ctx 해제할 컨텍스트
 */
void compression_destroy_context(CompressionContext* ctx);

/**
 * 데이터 압축
 * @param ctx 압축 컨텍스트
 * @param input 입력 데이터
 * @param input_size 입력 크기
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param compressed_size 압축된 크기 (출력)
 * @return 성공 시 0, 실패 시 음수 에러 코드
 */
int compression_compress(CompressionContext* ctx, const void* input, size_t input_size,
                        void* output, size_t output_capacity, size_t* compressed_size);

/**
 * 데이터 압축 해제
 * @param ctx 압축 컨텍스트
 * @param input 압축된 데이터
 * @param input_size 압축된 크기
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param decompressed_size 압축 해제된 크기 (출력)
 * @return 성공 시 0, 실패 시 음수 에러 코드
 */
int compression_decompress(CompressionContext* ctx, const void* input, size_t input_size,
                          void* output, size_t output_capacity, size_t* decompressed_size);

/**
 * 압축된 크기 추정
 * @param algorithm 압축 알고리즘
 * @param input_size 입력 크기
 * @param level 압축 레벨
 * @return 추정된 압축 크기
 */
size_t compression_estimate_size(CompressionAlgorithm algorithm, size_t input_size, int level);

// ============================================================================
// LZ4 압축 함수들
// ============================================================================

/**
 * LZ4 압축
 * @param input 입력 데이터
 * @param input_size 입력 크기
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param level 압축 레벨
 * @return 압축된 크기 (실패 시 0)
 */
size_t lz4_compress_data(const void* input, size_t input_size, void* output,
                        size_t output_capacity, int level);

/**
 * LZ4 압축 해제
 * @param input 압축된 데이터
 * @param input_size 압축된 크기
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @return 압축 해제된 크기 (실패 시 0)
 */
size_t lz4_decompress_data(const void* input, size_t input_size, void* output,
                          size_t output_capacity);

// ============================================================================
// ZSTD 압축 함수들
// ============================================================================

/**
 * ZSTD 압축
 * @param input 입력 데이터
 * @param input_size 입력 크기
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param level 압축 레벨
 * @return 압축된 크기 (실패 시 0)
 */
size_t zstd_compress_data(const void* input, size_t input_size, void* output,
                         size_t output_capacity, int level);

/**
 * ZSTD 압축 해제
 * @param input 압축된 데이터
 * @param input_size 압축된 크기
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @return 압축 해제된 크기 (실패 시 0)
 */
size_t zstd_decompress_data(const void* input, size_t input_size, void* output,
                           size_t output_capacity);

/**
 * ZSTD 사전 생성
 * @param samples 샘플 데이터 배열
 * @param sample_sizes 각 샘플 크기 배열
 * @param num_samples 샘플 수
 * @param dict_buffer 사전 버퍼
 * @param dict_capacity 사전 버퍼 용량
 * @return 생성된 사전 크기 (실패 시 0)
 */
size_t zstd_create_dictionary(const void** samples, const size_t* sample_sizes,
                             size_t num_samples, void* dict_buffer, size_t dict_capacity);

// ============================================================================
// 레이어별 압축 전략 함수들
// ============================================================================

/**
 * 레이어 타입에 따른 최적 압축 전략 선택
 * @param layer_kind 레이어 타입
 * @param data_size 데이터 크기
 * @param quantization_type 양자화 타입
 * @return 최적 압축 전략
 */
LayerCompressionStrategy select_optimal_compression_strategy(uint8_t layer_kind,
                                                           size_t data_size,
                                                           uint8_t quantization_type);

/**
 * 가중치 데이터 분석 및 압축 전략 추천
 * @param weights 가중치 데이터
 * @param size 데이터 크기
 * @param dtype 데이터 타입
 * @return 추천 압축 전략
 */
LayerCompressionStrategy analyze_weights_for_compression(const void* weights,
                                                        size_t size,
                                                        uint8_t dtype);

/**
 * 레이어별 압축 적용
 * @param weights 가중치 데이터
 * @param size 데이터 크기
 * @param strategy 압축 전략
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param compressed_size 압축된 크기 (출력)
 * @param stats 압축 통계 (출력, 선택사항)
 * @return 성공 시 0, 실패 시 음수 에러 코드
 */
int apply_layer_compression(const void* weights, size_t size,
                           const LayerCompressionStrategy* strategy,
                           void* output, size_t output_capacity,
                           size_t* compressed_size, CompressionStats* stats);

// ============================================================================
// 음성 특화 압축 함수들
// ============================================================================

/**
 * 음성 특화 압축 컨텍스트 생성
 * @param params 음성 압축 파라미터
 * @return 생성된 컨텍스트 포인터 (실패 시 NULL)
 */
CompressionContext* voice_compression_create_context(const VoiceCompressionParams* params);

/**
 * Mel 스펙트로그램 가중치 압축
 * @param mel_weights Mel 관련 가중치
 * @param size 데이터 크기
 * @param params 음성 압축 파라미터
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param compressed_size 압축된 크기 (출력)
 * @return 성공 시 0, 실패 시 음수 에러 코드
 */
int voice_compress_mel_weights(const float* mel_weights, size_t size,
                              const VoiceCompressionParams* params,
                              void* output, size_t output_capacity,
                              size_t* compressed_size);

/**
 * 어텐션 가중치 압축 (시간적 상관관계 활용)
 * @param attention_weights 어텐션 가중치
 * @param size 데이터 크기
 * @param num_heads 어텐션 헤드 수
 * @param seq_length 시퀀스 길이
 * @param params 음성 압축 파라미터
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param compressed_size 압축된 크기 (출력)
 * @return 성공 시 0, 실패 시 음수 에러 코드
 */
int voice_compress_attention_weights(const float* attention_weights, size_t size,
                                    int num_heads, int seq_length,
                                    const VoiceCompressionParams* params,
                                    void* output, size_t output_capacity,
                                    size_t* compressed_size);

/**
 * 보코더 가중치 압축 (주파수 도메인 최적화)
 * @param vocoder_weights 보코더 가중치
 * @param size 데이터 크기
 * @param sample_rate 샘플링 레이트
 * @param params 음성 압축 파라미터
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param compressed_size 압축된 크기 (출력)
 * @return 성공 시 0, 실패 시 음수 에러 코드
 */
int voice_compress_vocoder_weights(const float* vocoder_weights, size_t size,
                                  int sample_rate,
                                  const VoiceCompressionParams* params,
                                  void* output, size_t output_capacity,
                                  size_t* compressed_size);

// ============================================================================
// 압축 사전 관리 함수들
// ============================================================================

/**
 * 모델별 압축 사전 생성
 * @param model_layers 모델 레이어 데이터 배열
 * @param num_layers 레이어 수
 * @param dict_buffer 사전 버퍼
 * @param dict_capacity 사전 버퍼 용량
 * @return 생성된 사전 크기 (실패 시 0)
 */
size_t create_model_compression_dictionary(const void** model_layers,
                                          const size_t* layer_sizes,
                                          size_t num_layers,
                                          void* dict_buffer,
                                          size_t dict_capacity);

/**
 * 압축 사전을 사용한 압축
 * @param ctx 압축 컨텍스트 (사전 포함)
 * @param input 입력 데이터
 * @param input_size 입력 크기
 * @param output 출력 버퍼
 * @param output_capacity 출력 버퍼 용량
 * @param compressed_size 압축된 크기 (출력)
 * @return 성공 시 0, 실패 시 음수 에러 코드
 */
int compression_compress_with_dictionary(CompressionContext* ctx,
                                        const void* input, size_t input_size,
                                        void* output, size_t output_capacity,
                                        size_t* compressed_size);

// ============================================================================
// 유틸리티 함수들
// ============================================================================

/**
 * 압축 알고리즘 이름 반환
 * @param algorithm 압축 알고리즘
 * @return 알고리즘 이름 문자열
 */
const char* compression_get_algorithm_name(CompressionAlgorithm algorithm);

/**
 * 압축 통계 출력
 * @param stats 압축 통계
 */
void compression_print_stats(const CompressionStats* stats);

/**
 * 최적 압축 알고리즘 선택
 * @param data 데이터
 * @param size 데이터 크기
 * @param target_ratio 목표 압축률
 * @param max_time_ms 최대 허용 시간 (밀리초)
 * @return 최적 압축 알고리즘
 */
CompressionAlgorithm select_optimal_algorithm(const void* data, size_t size,
                                             double target_ratio, double max_time_ms);

// 에러 코드 정의
#define COMPRESSION_SUCCESS                0
#define COMPRESSION_ERROR_INVALID_ARGUMENT -1
#define COMPRESSION_ERROR_OUT_OF_MEMORY    -2
#define COMPRESSION_ERROR_BUFFER_TOO_SMALL -3
#define COMPRESSION_ERROR_COMPRESSION_FAILED -4
#define COMPRESSION_ERROR_DECOMPRESSION_FAILED -5
#define COMPRESSION_ERROR_UNSUPPORTED_ALGORITHM -6

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_COMPRESSION_H