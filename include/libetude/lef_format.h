#ifndef LIBETUDE_LEF_FORMAT_H
#define LIBETUDE_LEF_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// LEF 매직 넘버 ('LEED' in little-endian)
#define LEF_MAGIC 0x4445454C

// LEF 버전 정보
#define LEF_VERSION_MAJOR 1
#define LEF_VERSION_MINOR 0

// LEF 플래그 정의
#define LEF_FLAG_COMPRESSED     (1 << 0)  // 압축 사용
#define LEF_FLAG_QUANTIZED      (1 << 1)  // 양자화 사용
#define LEF_FLAG_EXTENDED       (1 << 2)  // 확장 모델 지원
#define LEF_FLAG_STREAMING      (1 << 3)  // 스트리밍 로딩 지원
#define LEF_FLAG_ENCRYPTED      (1 << 4)  // 암호화 사용
#define LEF_FLAG_DIFFERENTIAL   (1 << 5)  // 차분 모델

// 양자화 타입 정의
typedef enum {
    LEF_QUANT_NONE = 0,      // 양자화 없음 (FP32)
    LEF_QUANT_FP16 = 1,      // FP16 양자화
    LEF_QUANT_BF16 = 2,      // BF16 양자화
    LEF_QUANT_INT8 = 3,      // INT8 양자화
    LEF_QUANT_INT4 = 4,      // INT4 양자화
    LEF_QUANT_MIXED = 5      // 혼합 정밀도
} LEFQuantizationType;

// 레이어 타입 정의
typedef enum {
    LEF_LAYER_LINEAR = 0,        // 선형 레이어
    LEF_LAYER_CONV1D = 1,        // 1D 컨볼루션
    LEF_LAYER_ATTENTION = 2,     // 어텐션 레이어
    LEF_LAYER_EMBEDDING = 3,     // 임베딩 레이어
    LEF_LAYER_NORMALIZATION = 4, // 정규화 레이어
    LEF_LAYER_ACTIVATION = 5,    // 활성화 함수
    LEF_LAYER_VOCODER = 6,       // 보코더 레이어
    LEF_LAYER_CUSTOM = 255       // 사용자 정의 레이어
} LEFLayerKind;

/**
 * LEF 파일 헤더 구조체
 * 파일의 시작 부분에 위치하며 전체 파일의 메타정보를 포함
 */
typedef struct {
    uint32_t magic;                    // LEF 매직 넘버 ('LEED')
    uint16_t version_major;            // 주 버전 번호
    uint16_t version_minor;            // 부 버전 번호
    uint32_t flags;                    // 파일 플래그 (압축, 양자화 등)
    uint32_t file_size;                // 전체 파일 크기 (바이트)
    uint32_t model_hash;               // 모델 검증 해시 (CRC32)
    uint64_t timestamp;                // 생성 타임스탬프 (Unix time)
    uint32_t compression_dict_offset;  // 압축 사전 오프셋
    uint32_t layer_index_offset;       // 레이어 인덱스 테이블 오프셋
    uint32_t layer_data_offset;        // 레이어 데이터 시작 오프셋
    uint8_t  reserved[16];             // 향후 확장용 예약 공간
} __attribute__((packed)) LEFHeader;

/**
 * 모델 메타데이터 구조체
 * 모델의 기본 정보와 아키텍처 정보를 포함
 */
typedef struct {
    // 모델 기본 정보
    char model_name[64];               // 모델 이름
    char model_version[16];            // 모델 버전
    char author[32];                   // 제작자
    char description[128];             // 모델 설명

    // 모델 아키텍처 정보
    uint16_t input_dim;                // 입력 차원 크기
    uint16_t output_dim;               // 출력 차원 크기
    uint16_t hidden_dim;               // 은닉층 차원 크기
    uint16_t num_layers;               // 총 레이어 수
    uint16_t num_heads;                // 어텐션 헤드 수
    uint16_t vocab_size;               // 어휘 사전 크기

    // 음성 합성 특화 정보
    uint16_t sample_rate;              // 오디오 샘플링 레이트 (Hz)
    uint16_t mel_channels;             // Mel 스펙트로그램 채널 수
    uint16_t hop_length;               // STFT hop 길이
    uint16_t win_length;               // STFT 윈도우 길이

    // 양자화 설정 정보
    uint8_t default_quantization;      // 기본 양자화 타입
    uint8_t mixed_precision;           // 혼합 정밀도 사용 여부
    uint16_t quantization_params_size; // 양자화 파라미터 크기

    uint8_t reserved[32];              // 향후 확장용 예약 공간
} __attribute__((packed)) LEFModelMeta;

/**
 * 레이어 헤더 구조체
 * 각 레이어의 메타정보와 데이터 위치 정보를 포함
 */
typedef struct {
    uint16_t layer_id;                 // 레이어 고유 식별자
    uint8_t  layer_kind;               // 레이어 타입 (LEFLayerKind)
    uint8_t  quantization_type;        // 레이어별 양자화 타입
    uint32_t meta_size;                // 레이어 메타데이터 크기
    uint32_t data_size;                // 실제 가중치 데이터 크기
    uint32_t compressed_size;          // 압축된 데이터 크기 (압축 시)
    uint32_t data_offset;              // 데이터 시작 오프셋
    uint32_t checksum;                 // 데이터 무결성 검증용 체크섬
} __attribute__((packed)) LEFLayerHeader;

/**
 * 레이어 인덱스 엔트리
 * 빠른 레이어 검색을 위한 인덱스 테이블 엔트리
 */
typedef struct {
    uint16_t layer_id;                 // 레이어 ID
    uint32_t header_offset;            // 레이어 헤더 오프셋
    uint32_t data_offset;              // 레이어 데이터 오프셋
    uint32_t data_size;                // 데이터 크기
} __attribute__((packed)) LEFLayerIndexEntry;

/**
 * 압축 사전 헤더
 * 압축에 사용되는 사전 정보
 */
typedef struct {
    uint32_t dict_size;                // 사전 크기
    uint32_t dict_checksum;            // 사전 체크섬
    uint8_t  compression_algorithm;    // 압축 알고리즘 (0=LZ4, 1=ZSTD)
    uint8_t  compression_level;        // 압축 레벨
    uint8_t  reserved[6];              // 예약 공간
} __attribute__((packed)) LEFCompressionDict;

/**
 * 양자화 파라미터 구조체
 * 레이어별 양자화 설정 정보
 */
typedef struct {
    float scale;                       // 양자화 스케일
    float zero_point;                  // 양자화 제로 포인트
    float min_value;                   // 최소값
    float max_value;                   // 최대값
    uint8_t bits;                      // 양자화 비트 수
    uint8_t signed_quant;              // 부호 있는 양자화 여부
    uint8_t reserved[2];               // 예약 공간
} __attribute__((packed)) LEFQuantizationParams;

// 헤더 검증 함수들
bool lef_validate_header(const LEFHeader* header);
bool lef_validate_model_meta(const LEFModelMeta* meta);
bool lef_validate_layer_header(const LEFLayerHeader* layer_header);

// 체크섬 계산 함수들
uint32_t lef_calculate_crc32(const void* data, size_t size);
uint32_t lef_calculate_model_hash(const LEFModelMeta* meta);

// 헤더 초기화 함수들
void lef_init_header(LEFHeader* header);
void lef_init_model_meta(LEFModelMeta* meta);
void lef_init_layer_header(LEFLayerHeader* layer_header, uint16_t layer_id, LEFLayerKind kind);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_LEF_FORMAT_H