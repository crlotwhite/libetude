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

// LEFX 매직 넘버 ('LEEX' in little-endian) - 확장 모델용
#define LEFX_MAGIC 0x5845454C

// LEF 버전 정보
#define LEF_VERSION_MAJOR 1
#define LEF_VERSION_MINOR 0

// LEFX 버전 정보
#define LEFX_VERSION_MAJOR 1
#define LEFX_VERSION_MINOR 0

// LEF 플래그 정의
#define LEF_FLAG_COMPRESSED     (1 << 0)  // 압축 사용
#define LEF_FLAG_QUANTIZED      (1 << 1)  // 양자화 사용
#define LEF_FLAG_EXTENDED       (1 << 2)  // 확장 모델 지원
#define LEF_FLAG_STREAMING      (1 << 3)  // 스트리밍 로딩 지원
#define LEF_FLAG_ENCRYPTED      (1 << 4)  // 암호화 사용
#define LEF_FLAG_DIFFERENTIAL   (1 << 5)  // 차분 모델

// LEFX 확장 플래그 정의
#define LEFX_FLAG_SPEAKER_EXT   (1 << 0)  // 화자 확장
#define LEFX_FLAG_LANGUAGE_EXT  (1 << 1)  // 언어 확장
#define LEFX_FLAG_EFFECT_EXT    (1 << 2)  // 효과 확장
#define LEFX_FLAG_VOICE_EXT     (1 << 3)  // 음성 특성 확장
#define LEFX_FLAG_PLUGIN_EXT    (1 << 4)  // 플러그인 확장
#define LEFX_FLAG_CONDITIONAL   (1 << 5)  // 조건부 활성화
#define LEFX_FLAG_DIFFERENTIAL  (1 << 6)  // 차분 확장
#define LEFX_FLAG_COMPRESSED    (1 << 7)  // 압축된 확장

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

// LEFX 확장 타입 정의
typedef enum {
    LEFX_EXT_SPEAKER = 0,        // 화자 확장
    LEFX_EXT_LANGUAGE = 1,       // 언어 확장
    LEFX_EXT_VOICE_EFFECT = 2,   // 음성 효과 확장
    LEFX_EXT_AUDIO_EFFECT = 3,   // 오디오 효과 확장
    LEFX_EXT_STYLE = 4,          // 스타일 확장
    LEFX_EXT_EMOTION = 5,        // 감정 확장
    LEFX_EXT_PLUGIN = 6,         // 플러그인 확장
    LEFX_EXT_CUSTOM = 255        // 사용자 정의 확장
} LEFXExtensionType;

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

// ============================================================================
// LEFX (확장 모델) 포맷 구조체들
// ============================================================================

/**
 * LEFX 확장 모델 헤더 구조체
 * 확장 모델 파일의 시작 부분에 위치하며 확장 정보를 포함
 */
typedef struct {
    uint32_t magic;                    // LEFX 매직 넘버 ('LEEX')
    uint16_t version_major;            // 확장 포맷 주 버전
    uint16_t version_minor;            // 확장 포맷 부 버전
    uint32_t extension_flags;          // 확장 타입 플래그
    uint32_t file_size;                // 확장 파일 크기

    // 기본 모델 참조 정보
    uint32_t base_model_hash;          // 기본 모델 해시 (호환성 검증)
    char base_model_version[16];       // 기본 모델 버전
    char base_model_name[32];          // 기본 모델 이름
    uint32_t required_base_size;       // 필요한 기본 모델 크기

    // 확장 메타데이터
    uint32_t extension_type;           // 확장 타입 (LEFXExtensionType)
    uint16_t extension_id;             // 확장 고유 식별자
    char extension_name[32];           // 확장 이름
    char extension_author[32];         // 확장 제작자
    char extension_version[16];        // 확장 버전

    // 확장 데이터 오프셋
    uint32_t meta_offset;              // 확장 메타데이터 오프셋
    uint32_t dependency_offset;        // 의존성 정보 오프셋
    uint32_t layer_index_offset;       // 확장 레이어 인덱스 오프셋
    uint32_t layer_data_offset;        // 확장 레이어 데이터 오프셋
    uint32_t plugin_data_offset;       // 플러그인 데이터 오프셋

    uint64_t timestamp;                // 생성 타임스탬프
    uint8_t reserved[24];              // 향후 확장용 예약 공간
} __attribute__((packed)) LEFXHeader;

/**
 * LEFX 확장 메타데이터 구조체
 * 확장 모델의 상세 정보를 포함
 */
typedef struct {
    // 확장 기본 정보
    char description[128];             // 확장 설명
    char license[64];                  // 라이선스 정보
    char website[128];                 // 웹사이트 URL
    char contact[64];                  // 연락처 정보

    // 호환성 정보
    uint16_t min_base_version_major;   // 최소 기본 모델 주 버전
    uint16_t min_base_version_minor;   // 최소 기본 모델 부 버전
    uint16_t max_base_version_major;   // 최대 기본 모델 주 버전
    uint16_t max_base_version_minor;   // 최대 기본 모델 부 버전

    // 확장 특성 정보
    uint32_t extension_capabilities;   // 확장 기능 플래그
    uint16_t priority;                 // 확장 우선순위 (0-65535)
    uint16_t num_layers;               // 확장 레이어 수
    uint32_t total_params;             // 총 파라미터 수
    uint32_t memory_requirement;       // 메모리 요구사항 (KB)

    // 음성 특화 정보 (화자/언어 확장용)
    uint8_t gender;                    // 성별 (0=남성, 1=여성, 2=중성, 255=해당없음)
    uint8_t age_range;                 // 연령대 (0=어린이, 1=청년, 2=중년, 3=노년, 255=해당없음)
    uint8_t language_code[4];          // ISO 639-1 언어 코드 (예: "ko", "en")
    uint8_t accent_code[4];            // 억양 코드 (예: "KR", "US")

    // 품질 및 성능 정보
    float quality_score;               // 품질 점수 (0.0-1.0)
    float performance_impact;          // 성능 영향도 (0.0-1.0, 높을수록 무거움)
    uint32_t inference_time_ms;        // 예상 추론 시간 (밀리초)
    uint32_t loading_time_ms;          // 예상 로딩 시간 (밀리초)

    uint8_t reserved[32];              // 향후 확장용 예약 공간
} __attribute__((packed)) LEFXExtensionMeta;

/**
 * LEFX 확장 레이어 구조체
 * 확장 모델의 개별 레이어 정보
 */
typedef struct {
    uint16_t extension_layer_id;       // 확장 레이어 고유 ID
    uint16_t base_layer_id;            // 연결될 기본 모델 레이어 ID (차분 모델용)
    uint8_t layer_kind;                // 레이어 타입 (LEFLayerKind)
    uint8_t quantization_type;         // 양자화 타입
    uint8_t blend_mode;                // 블렌딩 모드 (0=교체, 1=덧셈, 2=곱셈, 3=보간)
    uint8_t activation_condition;      // 활성화 조건 (0=항상, 1=조건부)

    // 레이어 데이터 정보
    uint32_t meta_size;                // 레이어 메타데이터 크기
    uint32_t data_size;                // 실제 데이터 크기
    uint32_t compressed_size;          // 압축된 크기 (압축 시)
    uint32_t data_offset;              // 데이터 시작 오프셋
    uint32_t checksum;                 // 데이터 체크섬

    // 차분 모델 정보
    float similarity_threshold;        // 유사도 임계값 (차분 모델용)
    float blend_weight;                // 블렌딩 가중치 (0.0-1.0)
    uint16_t dependency_count;         // 의존성 레이어 수
    uint16_t reserved_flags;           // 예약된 플래그

    uint8_t reserved[8];               // 향후 확장용 예약 공간
} __attribute__((packed)) LEFXLayerHeader;

/**
 * LEFX 의존성 정보 구조체
 * 확장 간의 의존성 관계를 정의
 */
typedef struct {
    uint16_t dependency_id;            // 의존하는 확장 ID
    char dependency_name[32];          // 의존하는 확장 이름
    char min_version[16];              // 최소 요구 버전
    char max_version[16];              // 최대 지원 버전
    uint8_t dependency_type;           // 의존성 타입 (0=필수, 1=선택, 2=충돌)
    uint8_t load_order;                // 로드 순서 (0=먼저, 1=나중에, 2=상관없음)
    uint8_t reserved[6];               // 예약 공간
} __attribute__((packed)) LEFXDependency;

/**
 * LEFX 조건부 활성화 규칙 구조체
 * 컨텍스트에 따른 확장 활성화 조건을 정의
 */
typedef struct {
    uint16_t rule_id;                  // 규칙 고유 ID
    uint8_t condition_type;            // 조건 타입 (0=텍스트, 1=화자, 2=언어, 3=시간, 4=사용자정의)
    uint8_t operator_type;             // 연산자 (0=같음, 1=포함, 2=범위, 3=정규식)
    char condition_value[64];          // 조건 값
    float activation_weight;           // 활성화 가중치 (0.0-1.0)
    uint8_t priority;                  // 규칙 우선순위
    uint8_t reserved[7];               // 예약 공간
} __attribute__((packed)) LEFXActivationRule;

/**
 * LEFX 플러그인 데이터 구조체
 * 플러그인 확장을 위한 추가 데이터
 */
typedef struct {
    char plugin_interface[32];         // 플러그인 인터페이스 이름
    char plugin_version[16];           // 플러그인 버전
    uint32_t plugin_data_size;         // 플러그인 데이터 크기
    uint32_t plugin_data_offset;       // 플러그인 데이터 오프셋
    uint32_t init_function_offset;     // 초기화 함수 오프셋
    uint32_t process_function_offset;  // 처리 함수 오프셋
    uint32_t cleanup_function_offset;  // 정리 함수 오프셋
    uint8_t reserved[16];              // 예약 공간
} __attribute__((packed)) LEFXPluginData;

// 헤더 검증 함수들
bool lef_validate_header(const LEFHeader* header);
bool lef_validate_model_meta(const LEFModelMeta* meta);
bool lef_validate_layer_header(const LEFLayerHeader* layer_header);

// LEFX 헤더 검증 함수들
bool lefx_validate_header(const LEFXHeader* header);
bool lefx_validate_extension_meta(const LEFXExtensionMeta* meta);
bool lefx_validate_layer_header(const LEFXLayerHeader* layer_header);
bool lefx_validate_dependency(const LEFXDependency* dependency);
bool lefx_validate_activation_rule(const LEFXActivationRule* rule);

// 체크섬 계산 함수들
uint32_t lef_calculate_crc32(const void* data, size_t size);
uint32_t lef_calculate_model_hash(const LEFModelMeta* meta);

// 헤더 초기화 함수들
void lef_init_header(LEFHeader* header);
void lef_init_model_meta(LEFModelMeta* meta);
void lef_init_layer_header(LEFLayerHeader* layer_header, uint16_t layer_id, LEFLayerKind kind);

// LEFX 헤더 초기화 함수들
void lefx_init_header(LEFXHeader* header);
void lefx_init_extension_meta(LEFXExtensionMeta* meta);
void lefx_init_layer_header(LEFXLayerHeader* layer_header, uint16_t extension_layer_id, uint16_t base_layer_id);
void lefx_init_dependency(LEFXDependency* dependency);
void lefx_init_activation_rule(LEFXActivationRule* rule);
void lefx_init_plugin_data(LEFXPluginData* plugin_data);

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
 * 로드된 LEFX 확장 모델 구조체
 * 메모리에 로드된 LEFX 확장 모델의 정보를 포함
 */
typedef struct {
    LEFXHeader header;                 // LEFX 파일 헤더
    LEFXExtensionMeta meta;            // 확장 메타데이터
    LEFXLayerHeader* layer_headers;    // 확장 레이어 헤더 배열
    LEFXDependency* dependencies;      // 의존성 정보 배열
    LEFXActivationRule* activation_rules; // 활성화 규칙 배열
    LEFXPluginData* plugin_data;       // 플러그인 데이터 (필요시)
    void** layer_data;                 // 확장 레이어 데이터 포인터 배열

    size_t num_layers;                 // 확장 레이어 수
    size_t num_dependencies;           // 의존성 수
    size_t num_activation_rules;       // 활성화 규칙 수

    // 기본 모델 참조
    LEFModel* base_model;              // 기본 모델 포인터 (연결 시)
    bool base_model_owned;             // 기본 모델 소유권 여부

    // 메모리 관리
    void* file_data;                   // 전체 파일 데이터
    size_t file_size;                  // 파일 크기
    bool owns_memory;                  // 메모리 소유권 여부
    bool memory_mapped;                // 메모리 매핑 여부

    // 파일 정보
    char* file_path;                   // 파일 경로 (복사본)
    FILE* file_handle;                 // 파일 핸들 (스트리밍 시)

    // 런타임 상태
    bool is_active;                    // 확장 활성화 상태
    float current_weight;              // 현재 블렌딩 가중치
    void* runtime_context;             // 런타임 컨텍스트 (플러그인용)
} LEFXModel;

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

// ============================================================================
// LEFX (확장 모델) 관련 함수들
// ============================================================================

/**
 * LEFX 확장 모델 로드
 * @param path 확장 모델 파일 경로
 * @return 로드된 확장 모델 포인터 (실패 시 NULL)
 */
LEFXModel* lefx_load_extension(const char* path);

/**
 * 메모리에서 LEFX 확장 모델 로드
 * @param data 확장 모델 데이터 포인터
 * @param size 데이터 크기
 * @return 로드된 확장 모델 포인터 (실패 시 NULL)
 */
LEFXModel* lefx_load_extension_from_memory(const void* data, size_t size);

/**
 * LEFX 확장 모델 언로드 및 메모리 해제
 * @param extension 해제할 확장 모델 포인터
 */
void lefx_unload_extension(LEFXModel* extension);

/**
 * 기본 모델과 확장 모델 호환성 검증
 * @param base_model 기본 모델 포인터
 * @param extension 확장 모델 포인터
 * @return 호환 시 true, 비호환 시 false
 */
bool lefx_check_compatibility(const LEFModel* base_model, const LEFXModel* extension);

/**
 * 확장 모델을 기본 모델에 적용
 * @param base_model 기본 모델 포인터
 * @param extension 확장 모델 포인터
 * @param blend_weight 블렌딩 가중치 (0.0-1.0)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_apply_extension(LEFModel* base_model, LEFXModel* extension, float blend_weight);

/**
 * 확장 모델 비활성화
 * @param base_model 기본 모델 포인터
 * @param extension 확장 모델 포인터
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_deactivate_extension(LEFModel* base_model, LEFXModel* extension);

/**
 * 확장 레이어 데이터 가져오기
 * @param extension 확장 모델 포인터
 * @param layer_id 확장 레이어 ID
 * @return 레이어 데이터 포인터 (실패 시 NULL)
 */
void* lefx_get_layer_data(LEFXModel* extension, uint16_t layer_id);

/**
 * 확장 레이어 헤더 가져오기
 * @param extension 확장 모델 포인터
 * @param layer_id 확장 레이어 ID
 * @return 레이어 헤더 포인터 (실패 시 NULL)
 */
const LEFXLayerHeader* lefx_get_layer_header(LEFXModel* extension, uint16_t layer_id);

/**
 * 의존성 검증
 * @param extension 확장 모델 포인터
 * @param available_extensions 사용 가능한 확장 모델 배열
 * @param num_available 사용 가능한 확장 수
 * @return 의존성 만족 시 true, 불만족 시 false
 */
bool lefx_check_dependencies(const LEFXModel* extension,
                            const LEFXModel** available_extensions,
                            size_t num_available);

/**
 * 조건부 활성화 평가
 * @param extension 확장 모델 포인터
 * @param context_data 컨텍스트 데이터
 * @param context_size 컨텍스트 크기
 * @return 활성화 가중치 (0.0-1.0)
 */
float lefx_evaluate_activation_conditions(const LEFXModel* extension,
                                         const void* context_data,
                                         size_t context_size);

/**
 * 확장 모델 정보 출력
 * @param extension 확장 모델 포인터
 */
void lefx_print_extension_info(const LEFXModel* extension);

/**
 * 확장 모델 통계 정보 가져오기
 * @param extension 확장 모델 포인터
 * @param total_params 총 파라미터 수 (출력)
 * @param total_size 총 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_get_extension_stats(const LEFXModel* extension, size_t* total_params, size_t* total_size);

// ============================================================================
// 차분 모델 시스템 관련 구조체 및 함수들
// ============================================================================

/**
 * 레이어 차분 정보 구조체
 * 기본 모델과 화자별 모델 간의 차이 정보를 저장
 */
typedef struct {
    uint16_t base_layer_id;            // 기본 모델 레이어 ID
    uint16_t speaker_layer_id;         // 화자별 모델 레이어 ID
    float similarity_score;            // 유사도 점수 (0.0-1.0)
    float compression_ratio;           // 압축 비율
    size_t original_size;              // 원본 레이어 크기
    size_t diff_size;                  // 차분 데이터 크기
    void* diff_data;                   // 차분 데이터 포인터
    uint8_t diff_type;                 // 차분 타입 (0=가중치 차분, 1=델타 압축, 2=스파스 차분)
} LEFLayerDiff;

/**
 * 차분 모델 생성 컨텍스트
 * 차분 모델 생성 과정에서 사용되는 상태 정보
 */
typedef struct {
    LEFModel* base_model;              // 기본 모델 포인터
    LEFModel* speaker_model;           // 화자별 모델 포인터
    LEFLayerDiff* layer_diffs;         // 레이어 차분 정보 배열
    size_t num_diffs;                  // 차분 레이어 수
    size_t diff_capacity;              // 차분 배열 용량

    // 최적화 설정
    float similarity_threshold;        // 유사도 임계값 (이하는 차분 저장)
    float compression_threshold;       // 압축 임계값 (이하는 원본 저장)
    bool enable_sparse_diff;           // 스파스 차분 활성화
    bool enable_quantization;          // 차분 양자화 활성화

    // 통계 정보
    size_t total_original_size;        // 총 원본 크기
    size_t total_diff_size;            // 총 차분 크기
    float average_similarity;          // 평균 유사도
    int layers_compressed;             // 압축된 레이어 수
    int layers_skipped;                // 건너뛴 레이어 수 (높은 유사도)
} LEFDiffContext;

/**
 * 차분 압축 알고리즘 타입
 */
typedef enum {
    LEF_DIFF_WEIGHT_DELTA = 0,         // 가중치 델타 압축
    LEF_DIFF_SPARSE_MASK = 1,          // 스파스 마스크 압축
    LEF_DIFF_QUANTIZED_DELTA = 2,      // 양자화된 델타 압축
    LEF_DIFF_HUFFMAN_CODED = 3,        // 허프만 코딩 압축
    LEF_DIFF_CUSTOM = 255              // 사용자 정의 압축
} LEFDiffCompressionType;

/**
 * 차분 메타데이터 구조체
 * 차분 모델의 메타정보를 저장
 */
typedef struct {
    uint32_t base_model_hash;          // 기본 모델 해시
    char base_model_version[16];       // 기본 모델 버전
    char speaker_id[32];               // 화자 식별자
    float overall_similarity;          // 전체 유사도 점수
    uint16_t diff_layers_count;        // 차분 레이어 수
    uint16_t skipped_layers_count;     // 건너뛴 레이어 수
    uint32_t compression_savings;      // 압축으로 절약된 바이트 수
    uint8_t diff_algorithm;            // 사용된 차분 알고리즘
    uint8_t reserved[15];              // 예약 공간
} LEFDiffMetadata;

// ============================================================================
// 차분 모델 시스템 함수들
// ============================================================================

/**
 * 차분 모델 생성 컨텍스트 생성
 * @param base_model 기본 모델 포인터
 * @param speaker_model 화자별 모델 포인터
 * @param similarity_threshold 유사도 임계값 (0.0-1.0)
 * @return 차분 컨텍스트 포인터 (실패 시 NULL)
 */
LEFDiffContext* lef_create_diff_context(LEFModel* base_model,
                                       LEFModel* speaker_model,
                                       float similarity_threshold);

/**
 * 차분 모델 생성 컨텍스트 해제
 * @param ctx 차분 컨텍스트 포인터
 */
void lef_destroy_diff_context(LEFDiffContext* ctx);

/**
 * 레이어 간 유사도 계산
 * @param base_layer_data 기본 레이어 데이터
 * @param speaker_layer_data 화자 레이어 데이터
 * @param data_size 데이터 크기
 * @param layer_kind 레이어 타입
 * @return 유사도 점수 (0.0-1.0)
 */
float lef_calculate_layer_similarity(const void* base_layer_data,
                                    const void* speaker_layer_data,
                                    size_t data_size,
                                    LEFLayerKind layer_kind);

/**
 * 레이어 차분 생성
 * @param ctx 차분 컨텍스트
 * @param base_layer_id 기본 레이어 ID
 * @param speaker_layer_id 화자 레이어 ID
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_create_layer_diff(LEFDiffContext* ctx,
                         uint16_t base_layer_id,
                         uint16_t speaker_layer_id);

/**
 * 전체 모델 차분 분석 수행
 * @param ctx 차분 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_analyze_model_differences(LEFDiffContext* ctx);

/**
 * 차분 데이터 압축
 * @param diff_data 차분 데이터
 * @param data_size 데이터 크기
 * @param compression_type 압축 타입
 * @param compressed_data 압축된 데이터 (출력)
 * @param compressed_size 압축된 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_compress_diff_data(const void* diff_data,
                          size_t data_size,
                          LEFDiffCompressionType compression_type,
                          void** compressed_data,
                          size_t* compressed_size);

/**
 * 차분 데이터 압축 해제
 * @param compressed_data 압축된 데이터
 * @param compressed_size 압축된 크기
 * @param compression_type 압축 타입
 * @param diff_data 차분 데이터 (출력)
 * @param data_size 데이터 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_decompress_diff_data(const void* compressed_data,
                            size_t compressed_size,
                            LEFDiffCompressionType compression_type,
                            void** diff_data,
                            size_t* data_size);

/**
 * 유사도 기반 최적화 수행
 * @param ctx 차분 컨텍스트
 * @param optimization_level 최적화 레벨 (1-5)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_optimize_diff_model(LEFDiffContext* ctx, int optimization_level);

/**
 * 차분 모델을 LEFX 파일로 저장
 * @param ctx 차분 컨텍스트
 * @param output_path 출력 파일 경로
 * @param speaker_info 화자 정보
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_save_diff_model(LEFDiffContext* ctx,
                       const char* output_path,
                       const char* speaker_info);

/**
 * 차분 모델 통계 정보 가져오기
 * @param ctx 차분 컨텍스트
 * @param total_savings 총 절약된 바이트 수 (출력)
 * @param compression_ratio 압축 비율 (출력)
 * @param avg_similarity 평균 유사도 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_get_diff_stats(const LEFDiffContext* ctx,
                      size_t* total_savings,
                      float* compression_ratio,
                      float* avg_similarity);

/**
 * 차분 모델 정보 출력
 * @param ctx 차분 컨텍스트
 */
void lef_print_diff_info(const LEFDiffContext* ctx);

/**
 * 스파스 차분 생성 (가중치가 거의 0인 부분 제거)
 * @param base_data 기본 데이터
 * @param speaker_data 화자 데이터
 * @param data_size 데이터 크기
 * @param sparsity_threshold 스파스 임계값
 * @param sparse_diff 스파스 차분 데이터 (출력)
 * @param sparse_size 스파스 데이터 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_create_sparse_diff(const float* base_data,
                          const float* speaker_data,
                          size_t data_size,
                          float sparsity_threshold,
                          void** sparse_diff,
                          size_t* sparse_size);

/**
 * 양자화된 차분 생성
 * @param base_data 기본 데이터
 * @param speaker_data 화자 데이터
 * @param data_size 데이터 크기
 * @param quantization_bits 양자화 비트 수
 * @param quantized_diff 양자화된 차분 데이터 (출력)
 * @param quantized_size 양자화된 데이터 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_create_quantized_diff(const float* base_data,
                             const float* speaker_data,
                             size_t data_size,
                             int quantization_bits,
                             void** quantized_diff,
                             size_t* quantized_size);

// ============================================================================
// LEFX 직렬화 관련 구조체 및 함수들
// ============================================================================

/**
 * LEFX 확장 모델 직렬화 컨텍스트
 * 확장 모델 저장 과정에서 사용되는 상태 정보
 */
typedef struct {
    FILE* file;                        // 출력 파일 포인터
    LEFXHeader header;                 // LEFX 파일 헤더
    LEFXExtensionMeta meta;            // 확장 메타데이터
    LEFXLayerHeader* layer_headers;    // 확장 레이어 헤더 배열
    LEFXDependency* dependencies;      // 의존성 정보 배열
    LEFXActivationRule* activation_rules; // 활성화 규칙 배열
    LEFXPluginData* plugin_data;       // 플러그인 데이터

    size_t num_layers;                 // 레이어 수
    size_t layer_capacity;             // 레이어 배열 용량
    size_t num_dependencies;           // 의존성 수
    size_t dependency_capacity;        // 의존성 배열 용량
    size_t num_activation_rules;       // 활성화 규칙 수
    size_t activation_rule_capacity;   // 활성화 규칙 배열 용량

    size_t current_offset;             // 현재 파일 오프셋
    bool compression_enabled;          // 압축 사용 여부
    uint8_t compression_level;         // 압축 레벨 (1-9)
    bool checksum_enabled;             // 체크섬 검증 사용 여부
} LEFXSerializationContext;

/**
 * LEFX 확장 레이어 데이터 구조체
 * 확장 레이어의 실제 데이터와 메타정보를 포함
 */
typedef struct {
    uint16_t extension_layer_id;       // 확장 레이어 ID
    uint16_t base_layer_id;            // 기본 레이어 ID (차분 모델용)
    LEFLayerKind layer_kind;           // 레이어 타입
    LEFQuantizationType quant_type;    // 양자화 타입
    uint8_t blend_mode;                // 블렌딩 모드
    uint8_t activation_condition;      // 활성화 조건

    void* layer_meta;                  // 레이어별 메타데이터
    size_t meta_size;                  // 메타데이터 크기
    void* weight_data;                 // 가중치 데이터
    size_t data_size;                  // 데이터 크기
    LEFQuantizationParams* quant_params; // 양자화 파라미터 (필요시)

    float similarity_threshold;        // 유사도 임계값
    float blend_weight;                // 블렌딩 가중치
    uint16_t* dependency_layers;       // 의존성 레이어 ID 배열
    size_t dependency_count;           // 의존성 레이어 수
} LEFXLayerData;

/**
 * LEFX 확장 모델 직렬화 컨텍스트 생성
 * @param filename 출력 파일명
 * @param base_model_hash 기본 모델 해시
 * @param extension_type 확장 타입
 * @return 직렬화 컨텍스트 포인터 (실패 시 NULL)
 */
LEFXSerializationContext* lefx_create_serialization_context(const char* filename,
                                                           uint32_t base_model_hash,
                                                           LEFXExtensionType extension_type);

/**
 * LEFX 직렬화 컨텍스트 해제
 * @param ctx 직렬화 컨텍스트 포인터
 */
void lefx_destroy_serialization_context(LEFXSerializationContext* ctx);

/**
 * 확장 모델 기본 정보 설정
 * @param ctx 직렬화 컨텍스트
 * @param name 확장 이름
 * @param version 확장 버전
 * @param author 제작자
 * @param description 설명
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_set_extension_info(LEFXSerializationContext* ctx, const char* name, const char* version,
                           const char* author, const char* description);

/**
 * 기본 모델 참조 정보 설정
 * @param ctx 직렬화 컨텍스트
 * @param base_model_name 기본 모델 이름
 * @param base_model_version 기본 모델 버전
 * @param required_base_size 필요한 기본 모델 크기
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_set_base_model_info(LEFXSerializationContext* ctx, const char* base_model_name,
                            const char* base_model_version, uint32_t required_base_size);

/**
 * 확장 레이어 추가
 * @param ctx 직렬화 컨텍스트
 * @param layer_data 확장 레이어 데이터
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_add_extension_layer(LEFXSerializationContext* ctx, const LEFXLayerData* layer_data);

/**
 * 의존성 정보 추가
 * @param ctx 직렬화 컨텍스트
 * @param dependency 의존성 정보
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_add_dependency(LEFXSerializationContext* ctx, const LEFXDependency* dependency);

/**
 * 활성화 규칙 추가
 * @param ctx 직렬화 컨텍스트
 * @param rule 활성화 규칙
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_add_activation_rule(LEFXSerializationContext* ctx, const LEFXActivationRule* rule);

/**
 * 플러그인 데이터 설정
 * @param ctx 직렬화 컨텍스트
 * @param plugin_data 플러그인 데이터
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_set_plugin_data(LEFXSerializationContext* ctx, const LEFXPluginData* plugin_data);

/**
 * 확장 모델 저장 완료
 * @param ctx 직렬화 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_finalize_extension(LEFXSerializationContext* ctx);

// ============================================================================
// 조건부 확장 활성화 시스템 구조체 및 함수들
// ============================================================================

/**
 * 컨텍스트 데이터 타입 정의
 */
typedef enum {
    LEFX_CONTEXT_TEXT = 0,             // 텍스트 컨텍스트
    LEFX_CONTEXT_SPEAKER = 1,          // 화자 컨텍스트
    LEFX_CONTEXT_LANGUAGE = 2,         // 언어 컨텍스트
    LEFX_CONTEXT_TIME = 3,             // 시간 컨텍스트
    LEFX_CONTEXT_EMOTION = 4,          // 감정 컨텍스트
    LEFX_CONTEXT_STYLE = 5,            // 스타일 컨텍스트
    LEFX_CONTEXT_CUSTOM = 255          // 사용자 정의 컨텍스트
} LEFXContextType;

/**
 * 조건부 활성화 컨텍스트 구조체
 * 확장 활성화 결정에 사용되는 컨텍스트 정보
 */
typedef struct {
    // 텍스트 컨텍스트
    const char* input_text;            // 입력 텍스트
    size_t text_length;                // 텍스트 길이
    const char* language_hint;         // 언어 힌트 (예: "ko", "en")

    // 화자 컨텍스트
    uint16_t speaker_id;               // 화자 ID
    uint8_t gender;                    // 성별 (0=남성, 1=여성, 2=중성)
    uint8_t age_range;                 // 연령대
    float pitch_preference;            // 피치 선호도 (-1.0 ~ 1.0)

    // 감정/스타일 컨텍스트
    uint8_t emotion_type;              // 감정 타입 (0=중성, 1=기쁨, 2=슬픔, 3=분노 등)
    float emotion_intensity;           // 감정 강도 (0.0 ~ 1.0)
    uint8_t speaking_style;            // 말하기 스타일 (0=일반, 1=뉴스, 2=대화 등)
    float speaking_speed;              // 말하기 속도 (0.5 ~ 2.0, 1.0=기본)

    // 시간 컨텍스트
    uint64_t timestamp;                // 현재 타임스탬프
    uint8_t time_of_day;               // 시간대 (0=새벽, 1=오전, 2=오후, 3=저녁, 4=밤)

    // 사용자 정의 컨텍스트
    void* custom_data;                 // 사용자 정의 데이터
    size_t custom_data_size;           // 사용자 정의 데이터 크기

    // 런타임 정보
    float quality_preference;          // 품질 선호도 (0.0=속도 우선, 1.0=품질 우선)
    float performance_budget;          // 성능 예산 (0.0 ~ 1.0)
    bool realtime_mode;                // 실시간 모드 여부
} LEFXActivationContext;

/**
 * 확장 활성화 결과 구조체
 * 조건부 활성화 평가 결과를 포함
 */
typedef struct {
    bool should_activate;              // 활성화 여부
    float activation_weight;           // 활성화 가중치 (0.0 ~ 1.0)
    float blend_weight;                // 블렌딩 가중치 (0.0 ~ 1.0)
    uint16_t matched_rule_id;          // 매칭된 규칙 ID
    float confidence_score;            // 신뢰도 점수 (0.0 ~ 1.0)
    const char* activation_reason;     // 활성화 이유 (디버깅용)
} LEFXActivationResult;

/**
 * 블렌딩 모드 정의
 */
typedef enum {
    LEFX_BLEND_REPLACE = 0,            // 교체 (기본 레이어를 확장 레이어로 교체)
    LEFX_BLEND_ADD = 1,                // 덧셈 (기본 + 확장)
    LEFX_BLEND_MULTIPLY = 2,           // 곱셈 (기본 * 확장)
    LEFX_BLEND_INTERPOLATE = 3,        // 보간 (기본 * (1-w) + 확장 * w)
    LEFX_BLEND_WEIGHTED_SUM = 4,       // 가중합 (기본 * w1 + 확장 * w2)
    LEFX_BLEND_CUSTOM = 255            // 사용자 정의 블렌딩
} LEFXBlendMode;

/**
 * 실시간 전환 상태 구조체
 * 확장 모델의 실시간 전환을 위한 상태 정보
 */
typedef struct {
    bool is_transitioning;             // 전환 중 여부
    float transition_progress;         // 전환 진행률 (0.0 ~ 1.0)
    float transition_duration;         // 전환 지속 시간 (초)
    uint64_t transition_start_time;    // 전환 시작 시간

    // 전환 전/후 상태
    float prev_weight;                 // 이전 가중치
    float target_weight;               // 목표 가중치
    LEFXBlendMode prev_blend_mode;     // 이전 블렌딩 모드
    LEFXBlendMode target_blend_mode;   // 목표 블렌딩 모드

    // 전환 곡선 설정
    uint8_t transition_curve;          // 전환 곡선 (0=선형, 1=ease-in, 2=ease-out, 3=ease-in-out)
    float smoothing_factor;            // 스무딩 팩터 (0.0 ~ 1.0)
} LEFXTransitionState;

/**
 * 확장 활성화 매니저 구조체
 * 여러 확장 모델의 조건부 활성화를 관리
 */
typedef struct {
    LEFXModel** extensions;            // 확장 모델 배열
    size_t num_extensions;             // 확장 모델 수
    size_t extensions_capacity;        // 확장 배열 용량

    LEFXActivationResult* activation_results; // 활성화 결과 배열
    LEFXTransitionState* transition_states;   // 전환 상태 배열

    // 전역 설정
    float global_quality_threshold;    // 전역 품질 임계값
    float global_performance_budget;   // 전역 성능 예산
    bool enable_smooth_transitions;    // 부드러운 전환 활성화
    float default_transition_duration; // 기본 전환 지속 시간

    // 캐시 및 최적화
    LEFXActivationContext* cached_context; // 캐시된 컨텍스트
    uint64_t cache_timestamp;          // 캐시 타임스탬프
    bool cache_valid;                  // 캐시 유효성

    // 통계 정보
    size_t total_activations;          // 총 활성화 횟수
    size_t total_transitions;          // 총 전환 횟수
    float avg_activation_time;         // 평균 활성화 시간
} LEFXActivationManager;

// ============================================================================
// 조건부 확장 활성화 시스템 함수들
// ============================================================================

/**
 * 활성화 컨텍스트 초기화
 * @param context 초기화할 컨텍스트 포인터
 */
void lefx_init_activation_context(LEFXActivationContext* context);

/**
 * 활성화 매니저 생성
 * @param initial_capacity 초기 확장 배열 용량
 * @return 활성화 매니저 포인터 (실패 시 NULL)
 */
LEFXActivationManager* lefx_create_activation_manager(size_t initial_capacity);

/**
 * 활성화 매니저 해제
 * @param manager 활성화 매니저 포인터
 */
void lefx_destroy_activation_manager(LEFXActivationManager* manager);

/**
 * 확장 모델을 활성화 매니저에 등록
 * @param manager 활성화 매니저
 * @param extension 등록할 확장 모델
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_register_extension(LEFXActivationManager* manager, LEFXModel* extension);

/**
 * 확장 모델을 활성화 매니저에서 제거
 * @param manager 활성화 매니저
 * @param extension 제거할 확장 모델
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_unregister_extension(LEFXActivationManager* manager, LEFXModel* extension);

/**
 * 단일 확장 모델의 조건부 활성화 평가
 * @param extension 확장 모델
 * @param context 활성화 컨텍스트
 * @param result 활성화 결과 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_evaluate_single_extension(const LEFXModel* extension,
                                  const LEFXActivationContext* context,
                                  LEFXActivationResult* result);

/**
 * 모든 등록된 확장 모델의 조건부 활성화 평가
 * @param manager 활성화 매니저
 * @param context 활성화 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_evaluate_all_extensions(LEFXActivationManager* manager,
                                const LEFXActivationContext* context);

/**
 * 활성화 규칙 매칭 검사
 * @param rule 활성화 규칙
 * @param context 활성화 컨텍스트
 * @param match_score 매칭 점수 (출력, 0.0 ~ 1.0)
 * @return 매칭 시 true, 비매칭 시 false
 */
bool lefx_match_activation_rule(const LEFXActivationRule* rule,
                               const LEFXActivationContext* context,
                               float* match_score);

/**
 * 텍스트 조건 매칭
 * @param rule_value 규칙 값
 * @param context_text 컨텍스트 텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_text_condition(const char* rule_value,
                               const char* context_text,
                               uint8_t operator_type);

/**
 * 화자 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_speaker_condition(const char* rule_value,
                                  const LEFXActivationContext* context,
                                  uint8_t operator_type);

/**
 * 언어 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_language_condition(const char* rule_value,
                                   const LEFXActivationContext* context,
                                   uint8_t operator_type);

/**
 * 시간 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_time_condition(const char* rule_value,
                               const LEFXActivationContext* context,
                               uint8_t operator_type);

/**
 * 사용자 정의 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_custom_condition(const char* rule_value,
                                 const LEFXActivationContext* context,
                                 uint8_t operator_type);

/**
 * 레이어 블렌딩 수행
 * @param base_data 기본 레이어 데이터
 * @param extension_data 확장 레이어 데이터
 * @param output_data 출력 데이터
 * @param data_size 데이터 크기
 * @param blend_mode 블렌딩 모드
 * @param blend_weight 블렌딩 가중치
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_blend_layer_data(const void* base_data,
                         const void* extension_data,
                         void* output_data,
                         size_t data_size,
                         LEFXBlendMode blend_mode,
                         float blend_weight);

/**
 * 실시간 전환 시작
 * @param manager 활성화 매니저
 * @param extension_index 확장 인덱스
 * @param target_weight 목표 가중치
 * @param transition_duration 전환 지속 시간 (초)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_start_transition(LEFXActivationManager* manager,
                         size_t extension_index,
                         float target_weight,
                         float transition_duration);

/**
 * 실시간 전환 업데이트
 * @param manager 활성화 매니저
 * @param current_time 현재 시간 (밀리초)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_update_transitions(LEFXActivationManager* manager, uint64_t current_time);

/**
 * 전환 곡선 계산
 * @param progress 진행률 (0.0 ~ 1.0)
 * @param curve_type 곡선 타입
 * @return 조정된 진행률 (0.0 ~ 1.0)
 */
float lefx_calculate_transition_curve(float progress, uint8_t curve_type);

/**
 * 활성화 결과 최적화 (성능 예산 고려)
 * @param manager 활성화 매니저
 * @param performance_budget 성능 예산 (0.0 ~ 1.0)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_optimize_activations(LEFXActivationManager* manager, float performance_budget);

/**
 * 활성화 캐시 무효화
 * @param manager 활성화 매니저
 */
void lefx_invalidate_cache(LEFXActivationManager* manager);

/**
 * 활성화 통계 정보 가져오기
 * @param manager 활성화 매니저
 * @param active_extensions 활성화된 확장 수 (출력)
 * @param total_weight 총 활성화 가중치 (출력)
 * @param performance_impact 성능 영향도 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_get_activation_stats(const LEFXActivationManager* manager,
                             size_t* active_extensions,
                             float* total_weight,
                             float* performance_impact);

/**
 * 활성화 정보 출력 (디버깅용)
 * @param manager 활성화 매니저
 */
void lefx_print_activation_info(const LEFXActivationManager* manager);

// ============================================================================
// 내부 헬퍼 함수들 (구현에서만 사용)
// ============================================================================

/**
 * 레이어 블렌딩 적용 (내부 함수)
 * @param base_data 기본 레이어 데이터
 * @param ext_data 확장 레이어 데이터
 * @param data_size 데이터 크기
 * @param blend_mode 블렌딩 모드
 * @param weight 블렌딩 가중치
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_apply_layer_blending(void* base_data, const void* ext_data,
                             size_t data_size, uint8_t blend_mode, float weight);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_LEF_FORMAT_H