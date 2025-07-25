#ifndef LIBETUDE_LEF_FORMAT_H
#define LIBETUDE_LEF_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

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

// 모델 직렬화 관련 구조체 및 함수들

/**
 * 모델 직렬화 컨텍스트
 * 모델 저장 과정에서 사용되는 상태 정보
 */
typedef struct {
    FILE* file;                        // 출력 파일 포인터
    LEFHeader header;                  // 파일 헤더
    LEFModelMeta meta;                 // 모델 메타데이터
    LEFLayerHeader* layer_headers;     // 레이어 헤더 배열
    LEFLayerIndexEntry* layer_index;   // 레이어 인덱스 배열
    size_t num_layers;                 // 레이어 수
    size_t layer_capacity;             // 레이어 배열 용량
    size_t current_offset;             // 현재 파일 오프셋
    bool compression_enabled;          // 압축 사용 여부
    uint8_t compression_level;         // 압축 레벨 (1-9)
    bool checksum_enabled;             // 체크섬 검증 사용 여부
} LEFSerializationContext;

/**
 * 레이어 데이터 구조체
 * 레이어의 실제 데이터와 메타정보를 포함
 */
typedef struct {
    uint16_t layer_id;                 // 레이어 ID
    LEFLayerKind layer_kind;           // 레이어 타입
    LEFQuantizationType quant_type;    // 양자화 타입
    void* layer_meta;                  // 레이어별 메타데이터
    size_t meta_size;                  // 메타데이터 크기
    void* weight_data;                 // 가중치 데이터
    size_t data_size;                  // 데이터 크기
    LEFQuantizationParams* quant_params; // 양자화 파라미터 (필요시)
} LEFLayerData;

/**
 * 버전 호환성 정보
 */
typedef struct {
    uint16_t min_major;                // 최소 지원 주 버전
    uint16_t min_minor;                // 최소 지원 부 버전
    uint16_t max_major;                // 최대 지원 주 버전
    uint16_t max_minor;                // 최대 지원 부 버전
} LEFVersionCompatibility;

// 모델 직렬화 함수들
LEFSerializationContext* lef_create_serialization_context(const char* filename);
void lef_destroy_serialization_context(LEFSerializationContext* ctx);

// 모델 메타데이터 설정
int lef_set_model_info(LEFSerializationContext* ctx, const char* name, const char* version,
                       const char* author, const char* description);
int lef_set_model_architecture(LEFSerializationContext* ctx, uint16_t input_dim, uint16_t output_dim,
                               uint16_t hidden_dim, uint16_t num_layers, uint16_t num_heads, uint16_t vocab_size);
int lef_set_audio_config(LEFSerializationContext* ctx, uint16_t sample_rate, uint16_t mel_channels,
                         uint16_t hop_length, uint16_t win_length);

// 레이어 추가 및 데이터 저장
int lef_add_layer(LEFSerializationContext* ctx, const LEFLayerData* layer_data);
int lef_write_layer_data(LEFSerializationContext* ctx, uint16_t layer_id, const void* data, size_t size);

// 압축 및 양자화 설정
int lef_enable_compression(LEFSerializationContext* ctx, uint8_t level);
int lef_disable_compression(LEFSerializationContext* ctx);
int lef_set_default_quantization(LEFSerializationContext* ctx, LEFQuantizationType quant_type);

// 모델 저장 완료
int lef_finalize_model(LEFSerializationContext* ctx);

// 버전 관리 함수들
bool lef_check_version_compatibility(uint16_t file_major, uint16_t file_minor,
                                     const LEFVersionCompatibility* compat);
LEFVersionCompatibility lef_get_current_compatibility();
const char* lef_get_version_string();

// 체크섬 및 검증 함수들
int lef_verify_file_integrity(const char* filename);
int lef_verify_layer_integrity(const LEFLayerHeader* header, const void* data);
uint32_t lef_calculate_file_checksum(const char* filename);

// 에러 코드 정의
typedef enum {
    LEF_SUCCESS = 0,
    LEF_ERROR_INVALID_ARGUMENT = -1,
    LEF_ERROR_FILE_IO = -2,
    LEF_ERROR_OUT_OF_MEMORY = -3,
    LEF_ERROR_INVALID_FORMAT = -4,
    LEF_ERROR_COMPRESSION_FAILED = -5,
    LEF_ERROR_CHECKSUM_MISMATCH = -6,
    LEF_ERROR_VERSION_INCOMPATIBLE = -7,
    LEF_ERROR_LAYER_NOT_FOUND = -8,
    LEF_ERROR_BUFFER_TOO_SMALL = -9
} LEFErrorCode;

// 에러 메시지 함수
const char* lef_get_error_string(LEFErrorCode error);

// ============================================================================
// 모델 로더 관련 구조체 및 함수들
// ============================================================================

/**
 * 로드된 모델 구조체
 * 메모리에 로드된 LEF 모델의 정보를 포함
 */
typedef struct {
    LEFHeader header;                  // 파일 헤더
    LEFModelMeta meta;                 // 모델 메타데이터
    LEFLayerHeader* layer_headers;     // 레이어 헤더 배열
    LEFLayerIndexEntry* layer_index;   // 레이어 인덱스 배열
    void** layer_data;                 // 레이어 데이터 포인터 배열
    size_t num_layers;                 // 레이어 수

    // 메모리 관리
    void* file_data;                   // 전체 파일 데이터 (메모리 매핑 시)
    size_t file_size;                  // 파일 크기
    bool owns_memory;                  // 메모리 소유권 여부
    bool memory_mapped;                // 메모리 매핑 여부

    // 파일 정보
    char* file_path;                   // 파일 경로 (복사본)
    FILE* file_handle;                 // 파일 핸들 (스트리밍 시)
} LEFModel;

/**
 * 스트리밍 로더 구조체
 * 대용량 모델의 점진적 로딩을 위한 구조체
 */
typedef struct {
    FILE* file;                        // 파일 핸들
    LEFHeader header;                  // 파일 헤더
    LEFModelMeta meta;                 // 모델 메타데이터
    LEFLayerIndexEntry* layer_index;   // 레이어 인덱스 배열

    // 스트리밍 상태
    int current_layer;                 // 현재 로드된 레이어 인덱스
    bool* layers_loaded;               // 레이어 로드 상태 배열
    void** layer_cache;                // 레이어 캐시 배열
    size_t cache_size;                 // 캐시 크기 제한
    size_t cache_used;                 // 현재 사용된 캐시 크기

    // LRU 캐시 관리
    int* lru_order;                    // LRU 순서 배열
    int lru_head;                      // LRU 헤드 인덱스

    // 비동기 로딩 (향후 확장용)
    bool async_loading;                // 비동기 로딩 활성화 여부
    void* async_context;               // 비동기 컨텍스트
} LEFStreamingLoader;

/**
 * 메모리 매핑 컨텍스트
 * 플랫폼별 메모리 매핑 구현을 위한 구조체
 */
typedef struct {
    void* mapped_memory;               // 매핑된 메모리 주소
    size_t mapped_size;                // 매핑된 크기
    int file_descriptor;               // 파일 디스크립터 (Unix)
    void* file_mapping;                // 파일 매핑 핸들 (Windows)
    bool read_only;                    // 읽기 전용 여부
} LEFMemoryMapping;

// ============================================================================
// 기본 모델 로딩 함수들
// ============================================================================

/**
 * 파일에서 모델 로드
 * @param path 모델 파일 경로
 * @return 로드된 모델 포인터 (실패 시 NULL)
 */
LEFModel* lef_load_model(const char* path);

/**
 * 메모리에서 모델 로드
 * @param data 모델 데이터 포인터
 * @param size 데이터 크기
 * @return 로드된 모델 포인터 (실패 시 NULL)
 */
LEFModel* lef_load_model_from_memory(const void* data, size_t size);

/**
 * 모델 언로드 및 메모리 해제
 * @param model 해제할 모델 포인터
 */
void lef_unload_model(LEFModel* model);

/**
 * 특정 레이어 데이터 가져오기
 * @param model 모델 포인터
 * @param layer_id 레이어 ID
 * @return 레이어 데이터 포인터 (실패 시 NULL)
 */
void* lef_get_layer_data(LEFModel* model, uint16_t layer_id);

/**
 * 레이어 헤더 가져오기
 * @param model 모델 포인터
 * @param layer_id 레이어 ID
 * @return 레이어 헤더 포인터 (실패 시 NULL)
 */
const LEFLayerHeader* lef_get_layer_header(LEFModel* model, uint16_t layer_id);

// ============================================================================
// 메모리 매핑 기반 로더 함수들
// ============================================================================

/**
 * 메모리 매핑을 사용하여 모델 로드
 * @param path 모델 파일 경로
 * @return 로드된 모델 포인터 (실패 시 NULL)
 */
LEFModel* lef_load_model_mmap(const char* path);

/**
 * 메모리 매핑 생성
 * @param path 파일 경로
 * @param read_only 읽기 전용 여부
 * @return 메모리 매핑 컨텍스트 (실패 시 NULL)
 */
LEFMemoryMapping* lef_create_memory_mapping(const char* path, bool read_only);

/**
 * 메모리 매핑 해제
 * @param mapping 메모리 매핑 컨텍스트
 */
void lef_destroy_memory_mapping(LEFMemoryMapping* mapping);

// ============================================================================
// 스트리밍 로더 함수들
// ============================================================================

/**
 * 스트리밍 로더 생성
 * @param path 모델 파일 경로
 * @param cache_size 캐시 크기 제한 (바이트)
 * @return 스트리밍 로더 포인터 (실패 시 NULL)
 */
LEFStreamingLoader* lef_create_streaming_loader(const char* path, size_t cache_size);

/**
 * 스트리밍 로더 해제
 * @param loader 스트리밍 로더 포인터
 */
void lef_destroy_streaming_loader(LEFStreamingLoader* loader);

/**
 * 온디맨드 레이어 로딩
 * @param loader 스트리밍 로더
 * @param layer_id 로드할 레이어 ID
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_load_layer_on_demand(LEFStreamingLoader* loader, uint16_t layer_id);

/**
 * 레이어 언로드 (캐시에서 제거)
 * @param loader 스트리밍 로더
 * @param layer_id 언로드할 레이어 ID
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_unload_layer(LEFStreamingLoader* loader, uint16_t layer_id);

/**
 * 스트리밍 로더에서 레이어 데이터 가져오기
 * @param loader 스트리밍 로더
 * @param layer_id 레이어 ID
 * @return 레이어 데이터 포인터 (실패 시 NULL)
 */
void* lef_streaming_get_layer_data(LEFStreamingLoader* loader, uint16_t layer_id);

/**
 * 캐시 상태 정보 가져오기
 * @param loader 스트리밍 로더
 * @param loaded_layers 로드된 레이어 수 (출력)
 * @param cache_usage 캐시 사용량 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_get_cache_info(LEFStreamingLoader* loader, int* loaded_layers, size_t* cache_usage);

/**
 * 캐시 정리 (LRU 기반)
 * @param loader 스트리밍 로더
 * @param target_size 목표 캐시 크기
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_cleanup_cache(LEFStreamingLoader* loader, size_t target_size);

// ============================================================================
// 유틸리티 함수들
// ============================================================================

/**
 * 모델 정보 출력
 * @param model 모델 포인터
 */
void lef_print_model_info(const LEFModel* model);

/**
 * 레이어 정보 출력
 * @param model 모델 포인터
 */
void lef_print_layer_info(const LEFModel* model);

/**
 * 모델 통계 정보 가져오기
 * @param model 모델 포인터
 * @param total_params 총 파라미터 수 (출력)
 * @param total_size 총 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_get_model_stats(const LEFModel* model, size_t* total_params, size_t* total_size);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_LEF_FORMAT_H