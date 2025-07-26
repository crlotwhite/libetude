#include "libetude/lef_format.h"
#include "libetude/compression.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// CRC32 테이블 (IEEE 802.3 표준)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/**
 * CRC32 체크섬 계산
 * @param data 데이터 포인터
 * @param size 데이터 크기
 * @return CRC32 체크섬 값
 */
uint32_t lef_calculate_crc32(const void* data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = (const uint8_t*)data;

    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

/**
 * 모델 해시 계산
 * 모델 메타데이터를 기반으로 고유 해시 생성
 * @param meta 모델 메타데이터
 * @return 모델 해시 값
 */
uint32_t lef_calculate_model_hash(const LEFModelMeta* meta) {
    if (!meta) {
        return 0;
    }

    // 모델 이름, 버전, 아키텍처 정보를 조합하여 해시 계산
    uint32_t hash = 0;

    // 모델 이름 해시
    hash ^= lef_calculate_crc32(meta->model_name, strlen(meta->model_name));

    // 모델 버전 해시
    hash ^= lef_calculate_crc32(meta->model_version, strlen(meta->model_version));

    // 아키텍처 정보 해시
    struct {
        uint16_t input_dim;
        uint16_t output_dim;
        uint16_t hidden_dim;
        uint16_t num_layers;
        uint16_t num_heads;
        uint16_t vocab_size;
    } arch_info = {
        meta->input_dim,
        meta->output_dim,
        meta->hidden_dim,
        meta->num_layers,
        meta->num_heads,
        meta->vocab_size
    };

    hash ^= lef_calculate_crc32(&arch_info, sizeof(arch_info));

    return hash;
}

/**
 * LEF 헤더 검증
 * @param header LEF 헤더 포인터
 * @return 검증 성공 시 true, 실패 시 false
 */
bool lef_validate_header(const LEFHeader* header) {
    if (!header) {
        return false;
    }

    // 매직 넘버 검증
    if (header->magic != LEF_MAGIC) {
        return false;
    }

    // 버전 검증 (현재 버전과 호환되는지 확인)
    if (header->version_major > LEF_VERSION_MAJOR) {
        return false;
    }

    // 파일 크기 검증 (최소 크기 확인)
    if (header->file_size < sizeof(LEFHeader) + sizeof(LEFModelMeta)) {
        return false;
    }

    // 오프셋 검증
    if (header->layer_index_offset >= header->file_size ||
        header->layer_data_offset >= header->file_size ||
        header->layer_index_offset >= header->layer_data_offset) {
        return false;
    }

    return true;
}

/**
 * 모델 메타데이터 검증
 * @param meta 모델 메타데이터 포인터
 * @return 검증 성공 시 true, 실패 시 false
 */
bool lef_validate_model_meta(const LEFModelMeta* meta) {
    if (!meta) {
        return false;
    }

    // 모델 이름이 비어있지 않은지 확인
    if (strlen(meta->model_name) == 0) {
        return false;
    }

    // 아키텍처 정보 검증
    if (meta->input_dim == 0 || meta->output_dim == 0 ||
        meta->hidden_dim == 0 || meta->num_layers == 0) {
        return false;
    }

    // 음성 합성 관련 파라미터 검증
    if (meta->sample_rate == 0 || meta->mel_channels == 0 ||
        meta->hop_length == 0 || meta->win_length == 0) {
        return false;
    }

    // hop_length가 win_length보다 작거나 같아야 함
    if (meta->hop_length > meta->win_length) {
        return false;
    }

    // 양자화 타입 검증
    if (meta->default_quantization > LEF_QUANT_MIXED) {
        return false;
    }

    return true;
}

/**
 * 레이어 헤더 검증
 * @param layer_header 레이어 헤더 포인터
 * @return 검증 성공 시 true, 실패 시 false
 */
bool lef_validate_layer_header(const LEFLayerHeader* layer_header) {
    if (!layer_header) {
        return false;
    }

    // 레이어 타입 검증
    if (layer_header->layer_kind > LEF_LAYER_CUSTOM) {
        return false;
    }

    // 양자화 타입 검증
    if (layer_header->quantization_type > LEF_QUANT_MIXED) {
        return false;
    }

    // 데이터 크기 검증
    if (layer_header->data_size == 0) {
        return false;
    }

    // 압축된 크기가 원본보다 클 수 없음 (압축 사용 시)
    if (layer_header->compressed_size > 0 &&
        layer_header->compressed_size > layer_header->data_size) {
        return false;
    }

    return true;
}

/**
 * LEF 헤더 초기화
 * @param header 초기화할 헤더 포인터
 */
void lef_init_header(LEFHeader* header) {
    if (!header) {
        return;
    }

    memset(header, 0, sizeof(LEFHeader));

    header->magic = LEF_MAGIC;
    header->version_major = LEF_VERSION_MAJOR;
    header->version_minor = LEF_VERSION_MINOR;
    header->timestamp = (uint64_t)time(NULL);

    // 기본 오프셋 설정
    header->compression_dict_offset = 0;  // 압축 사전 없음
    header->layer_index_offset = sizeof(LEFHeader) + sizeof(LEFModelMeta);
    header->layer_data_offset = header->layer_index_offset;  // 초기값, 나중에 업데이트
}

/**
 * 모델 메타데이터 초기화
 * @param meta 초기화할 메타데이터 포인터
 */
void lef_init_model_meta(LEFModelMeta* meta) {
    if (!meta) {
        return;
    }

    memset(meta, 0, sizeof(LEFModelMeta));

    // 기본값 설정
    strcpy(meta->model_name, "untitled");
    strcpy(meta->model_version, "1.0.0");
    strcpy(meta->author, "unknown");
    strcpy(meta->description, "LibEtude TTS model");

    // 기본 아키텍처 설정 (일반적인 TTS 모델 기준)
    meta->input_dim = 256;
    meta->output_dim = 80;    // Mel channels
    meta->hidden_dim = 512;
    meta->num_layers = 6;
    meta->num_heads = 8;
    meta->vocab_size = 1000;

    // 기본 음성 설정
    meta->sample_rate = 22050;
    meta->mel_channels = 80;
    meta->hop_length = 256;
    meta->win_length = 1024;

    // 기본 양자화 설정
    meta->default_quantization = LEF_QUANT_NONE;
    meta->mixed_precision = 0;
    meta->quantization_params_size = 0;
}

/**
 * 레이어 헤더 초기화
 * @param layer_header 초기화할 레이어 헤더 포인터
 * @param layer_id 레이어 ID
 * @param kind 레이어 타입
 */
void lef_init_layer_header(LEFLayerHeader* layer_header, uint16_t layer_id, LEFLayerKind kind) {
    if (!layer_header) {
        return;
    }

    memset(layer_header, 0, sizeof(LEFLayerHeader));

    layer_header->layer_id = layer_id;
    layer_header->layer_kind = (uint8_t)kind;
    layer_header->quantization_type = LEF_QUANT_NONE;  // 기본값
    layer_header->meta_size = 0;
    layer_header->data_size = 0;
    layer_header->compressed_size = 0;
    layer_header->data_offset = 0;
    layer_header->checksum = 0;
}

// ============================================================================
// 모델 직렬화 구현
// ============================================================================

/**
 * 직렬화 컨텍스트 생성
 * @param filename 출력 파일명
 * @return 생성된 컨텍스트 포인터 (실패 시 NULL)
 */
LEFSerializationContext* lef_create_serialization_context(const char* filename) {
    if (!filename) {
        return NULL;
    }

    LEFSerializationContext* ctx = (LEFSerializationContext*)malloc(sizeof(LEFSerializationContext));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(LEFSerializationContext));

    // 파일 열기
    ctx->file = fopen(filename, "wb");
    if (!ctx->file) {
        free(ctx);
        return NULL;
    }

    // 헤더 및 메타데이터 초기화
    lef_init_header(&ctx->header);
    lef_init_model_meta(&ctx->meta);

    // 레이어 배열 초기 용량 설정
    ctx->layer_capacity = 16;
    ctx->layer_headers = (LEFLayerHeader*)malloc(ctx->layer_capacity * sizeof(LEFLayerHeader));
    ctx->layer_index = (LEFLayerIndexEntry*)malloc(ctx->layer_capacity * sizeof(LEFLayerIndexEntry));

    if (!ctx->layer_headers || !ctx->layer_index) {
        lef_destroy_serialization_context(ctx);
        return NULL;
    }

    // 기본 설정
    ctx->num_layers = 0;
    ctx->current_offset = sizeof(LEFHeader) + sizeof(LEFModelMeta);
    ctx->compression_enabled = false;
    ctx->compression_level = 6;  // 기본 압축 레벨
    ctx->checksum_enabled = true;

    return ctx;
}

/**
 * 직렬화 컨텍스트 해제
 * @param ctx 해제할 컨텍스트
 */
void lef_destroy_serialization_context(LEFSerializationContext* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->file) {
        fclose(ctx->file);
    }

    free(ctx->layer_headers);
    free(ctx->layer_index);
    free(ctx);
}

/**
 * 모델 기본 정보 설정
 * @param ctx 직렬화 컨텍스트
 * @param name 모델 이름
 * @param version 모델 버전
 * @param author 제작자
 * @param description 설명
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_set_model_info(LEFSerializationContext* ctx, const char* name, const char* version,
                       const char* author, const char* description) {
    if (!ctx || !name || !version) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 문자열 길이 검증
    if (strlen(name) >= sizeof(ctx->meta.model_name) ||
        strlen(version) >= sizeof(ctx->meta.model_version)) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (author && strlen(author) >= sizeof(ctx->meta.author)) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (description && strlen(description) >= sizeof(ctx->meta.description)) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 정보 복사
    strncpy(ctx->meta.model_name, name, sizeof(ctx->meta.model_name) - 1);
    strncpy(ctx->meta.model_version, version, sizeof(ctx->meta.model_version) - 1);

    if (author) {
        strncpy(ctx->meta.author, author, sizeof(ctx->meta.author) - 1);
    }

    if (description) {
        strncpy(ctx->meta.description, description, sizeof(ctx->meta.description) - 1);
    }

    return LEF_SUCCESS;
}

/**
 * 모델 아키텍처 정보 설정
 * @param ctx 직렬화 컨텍스트
 * @param input_dim 입력 차원
 * @param output_dim 출력 차원
 * @param hidden_dim 은닉 차원
 * @param num_layers 레이어 수
 * @param num_heads 어텐션 헤드 수
 * @param vocab_size 어휘 크기
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_set_model_architecture(LEFSerializationContext* ctx, uint16_t input_dim, uint16_t output_dim,
                               uint16_t hidden_dim, uint16_t num_layers, uint16_t num_heads, uint16_t vocab_size) {
    if (!ctx) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 아키텍처 정보 검증
    if (input_dim == 0 || output_dim == 0 || hidden_dim == 0 || num_layers == 0) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    ctx->meta.input_dim = input_dim;
    ctx->meta.output_dim = output_dim;
    ctx->meta.hidden_dim = hidden_dim;
    ctx->meta.num_layers = num_layers;
    ctx->meta.num_heads = num_heads;
    ctx->meta.vocab_size = vocab_size;

    return LEF_SUCCESS;
}

/**
 * 오디오 설정 정보 설정
 * @param ctx 직렬화 컨텍스트
 * @param sample_rate 샘플링 레이트
 * @param mel_channels Mel 채널 수
 * @param hop_length Hop 길이
 * @param win_length 윈도우 길이
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_set_audio_config(LEFSerializationContext* ctx, uint16_t sample_rate, uint16_t mel_channels,
                         uint16_t hop_length, uint16_t win_length) {
    if (!ctx) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 오디오 설정 검증
    if (sample_rate == 0 || mel_channels == 0 || hop_length == 0 || win_length == 0) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (hop_length > win_length) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    ctx->meta.sample_rate = sample_rate;
    ctx->meta.mel_channels = mel_channels;
    ctx->meta.hop_length = hop_length;
    ctx->meta.win_length = win_length;

    return LEF_SUCCESS;
}

/**
 * 레이어 배열 크기 확장
 * @param ctx 직렬화 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
static int lef_expand_layer_arrays(LEFSerializationContext* ctx) {
    size_t new_capacity = ctx->layer_capacity * 2;

    LEFLayerHeader* new_headers = (LEFLayerHeader*)realloc(ctx->layer_headers,
                                                           new_capacity * sizeof(LEFLayerHeader));
    if (!new_headers) {
        return LEF_ERROR_OUT_OF_MEMORY;
    }

    LEFLayerIndexEntry* new_index = (LEFLayerIndexEntry*)realloc(ctx->layer_index,
                                                                 new_capacity * sizeof(LEFLayerIndexEntry));
    if (!new_index) {
        free(new_headers);
        return LEF_ERROR_OUT_OF_MEMORY;
    }

    ctx->layer_headers = new_headers;
    ctx->layer_index = new_index;
    ctx->layer_capacity = new_capacity;

    return LEF_SUCCESS;
}

/**
 * 레이어 추가
 * @param ctx 직렬화 컨텍스트
 * @param layer_data 레이어 데이터
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_add_layer(LEFSerializationContext* ctx, const LEFLayerData* layer_data) {
    if (!ctx || !layer_data || !layer_data->weight_data || layer_data->data_size == 0) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 배열 크기 확장 필요 시
    if (ctx->num_layers >= ctx->layer_capacity) {
        int result = lef_expand_layer_arrays(ctx);
        if (result != LEF_SUCCESS) {
            return result;
        }
    }

    // 레이어 헤더 설정
    LEFLayerHeader* header = &ctx->layer_headers[ctx->num_layers];
    lef_init_layer_header(header, layer_data->layer_id, layer_data->layer_kind);

    header->quantization_type = (uint8_t)layer_data->quant_type;
    header->meta_size = layer_data->meta_size;
    header->data_size = layer_data->data_size;
    header->data_offset = ctx->current_offset;

    // 체크섬 계산
    if (ctx->checksum_enabled) {
        header->checksum = lef_calculate_crc32(layer_data->weight_data, layer_data->data_size);
    }

    // 압축 처리
    if (ctx->compression_enabled) {
        // 레이어별 최적 압축 전략 선택
        LayerCompressionStrategy strategy = select_optimal_compression_strategy(
            header->layer_kind, header->data_size, header->quantization_type);

        // 압축 버퍼 할당
        size_t max_compressed_size = compression_estimate_size(strategy.algorithm,
                                                             header->data_size, strategy.level);
        void* compressed_buffer = malloc(max_compressed_size);
        if (!compressed_buffer) {
            return LEF_ERROR_OUT_OF_MEMORY;
        }

        // 압축 수행
        size_t compressed_size;
        CompressionStats stats;
        int compress_result = apply_layer_compression(layer_data->weight_data, header->data_size,
                                                    &strategy, compressed_buffer, max_compressed_size,
                                                    &compressed_size, &stats);

        if (compress_result == COMPRESSION_SUCCESS && compressed_size < header->data_size) {
            // 압축이 효과적인 경우에만 사용
            header->compressed_size = compressed_size;

            // 압축된 데이터로 교체하여 저장
            if (fseek(ctx->file, header->data_offset, SEEK_SET) != 0) {
                free(compressed_buffer);
                return LEF_ERROR_FILE_IO;
            }

            size_t written = fwrite(compressed_buffer, 1, compressed_size, ctx->file);
            if (written != compressed_size) {
                free(compressed_buffer);
                return LEF_ERROR_FILE_IO;
            }

            // 압축 정보를 헤더에 저장
            ctx->header.flags |= LEF_FLAG_COMPRESSED;

            // 압축된 크기로 오프셋 업데이트
            ctx->current_offset = header->data_offset + compressed_size;
        } else {
            // 압축이 효과적이지 않으면 원본 사용
            header->compressed_size = 0;  // 압축 안됨 표시
            free(compressed_buffer);
        }

        free(compressed_buffer);
    }

    // 레이어 인덱스 엔트리 설정
    LEFLayerIndexEntry* index_entry = &ctx->layer_index[ctx->num_layers];
    index_entry->layer_id = layer_data->layer_id;
    index_entry->header_offset = sizeof(LEFHeader) + sizeof(LEFModelMeta) +
                                 ctx->num_layers * sizeof(LEFLayerHeader);
    index_entry->data_offset = header->data_offset;
    index_entry->data_size = header->data_size;

    // 데이터 쓰기
    if (fseek(ctx->file, header->data_offset, SEEK_SET) != 0) {
        return LEF_ERROR_FILE_IO;
    }

    size_t written = fwrite(layer_data->weight_data, 1, layer_data->data_size, ctx->file);
    if (written != layer_data->data_size) {
        return LEF_ERROR_FILE_IO;
    }

    // 메타데이터 쓰기 (있는 경우)
    if (layer_data->layer_meta && layer_data->meta_size > 0) {
        written = fwrite(layer_data->layer_meta, 1, layer_data->meta_size, ctx->file);
        if (written != layer_data->meta_size) {
            return LEF_ERROR_FILE_IO;
        }
    }

    // 오프셋 업데이트
    ctx->current_offset += header->data_size + header->meta_size;
    ctx->num_layers++;

    return LEF_SUCCESS;
}

/**
 * 압축 활성화
 * @param ctx 직렬화 컨텍스트
 * @param level 압축 레벨 (1-9)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_enable_compression(LEFSerializationContext* ctx, uint8_t level) {
    if (!ctx) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (level < 1 || level > 9) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    ctx->compression_enabled = true;
    ctx->compression_level = level;
    ctx->header.flags |= LEF_FLAG_COMPRESSED;

    return LEF_SUCCESS;
}

/**
 * 압축 비활성화
 * @param ctx 직렬화 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_disable_compression(LEFSerializationContext* ctx) {
    if (!ctx) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    ctx->compression_enabled = false;
    ctx->header.flags &= ~LEF_FLAG_COMPRESSED;

    return LEF_SUCCESS;
}

/**
 * 기본 양자화 타입 설정
 * @param ctx 직렬화 컨텍스트
 * @param quant_type 양자화 타입
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_set_default_quantization(LEFSerializationContext* ctx, LEFQuantizationType quant_type) {
    if (!ctx) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (quant_type > LEF_QUANT_MIXED) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    ctx->meta.default_quantization = (uint8_t)quant_type;

    if (quant_type != LEF_QUANT_NONE) {
        ctx->header.flags |= LEF_FLAG_QUANTIZED;
    } else {
        ctx->header.flags &= ~LEF_FLAG_QUANTIZED;
    }

    return LEF_SUCCESS;
}

/**
 * 모델 저장 완료
 * @param ctx 직렬화 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_finalize_model(LEFSerializationContext* ctx) {
    if (!ctx || !ctx->file) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 헤더 정보 업데이트
    ctx->header.layer_index_offset = sizeof(LEFHeader) + sizeof(LEFModelMeta);
    ctx->header.layer_data_offset = ctx->header.layer_index_offset +
                                    ctx->num_layers * sizeof(LEFLayerIndexEntry);

    // 모델 해시 계산
    ctx->header.model_hash = lef_calculate_model_hash(&ctx->meta);

    // 파일 크기 계산
    fseek(ctx->file, 0, SEEK_END);
    ctx->header.file_size = (uint32_t)ftell(ctx->file);

    // 헤더 쓰기
    fseek(ctx->file, 0, SEEK_SET);
    if (fwrite(&ctx->header, sizeof(LEFHeader), 1, ctx->file) != 1) {
        return LEF_ERROR_FILE_IO;
    }

    // 메타데이터 쓰기
    if (fwrite(&ctx->meta, sizeof(LEFModelMeta), 1, ctx->file) != 1) {
        return LEF_ERROR_FILE_IO;
    }

    // 레이어 인덱스 쓰기
    if (ctx->num_layers > 0) {
        if (fwrite(ctx->layer_index, sizeof(LEFLayerIndexEntry), ctx->num_layers, ctx->file) != ctx->num_layers) {
            return LEF_ERROR_FILE_IO;
        }
    }

    // 파일 플러시
    fflush(ctx->file);

    return LEF_SUCCESS;
}

// ============================================================================
// 버전 관리 구현
// ============================================================================

/**
 * 버전 호환성 확인
 * @param file_major 파일의 주 버전
 * @param file_minor 파일의 부 버전
 * @param compat 호환성 정보
 * @return 호환 가능 시 true, 불가능 시 false
 */
bool lef_check_version_compatibility(uint16_t file_major, uint16_t file_minor,
                                     const LEFVersionCompatibility* compat) {
    if (!compat) {
        return false;
    }

    // 주 버전 확인
    if (file_major < compat->min_major || file_major > compat->max_major) {
        return false;
    }

    // 같은 주 버전에서 부 버전 확인
    if (file_major == compat->min_major && file_minor < compat->min_minor) {
        return false;
    }

    if (file_major == compat->max_major && file_minor > compat->max_minor) {
        return false;
    }

    return true;
}

/**
 * 현재 라이브러리의 호환성 정보 반환
 * @return 호환성 정보 구조체
 */
LEFVersionCompatibility lef_get_current_compatibility() {
    LEFVersionCompatibility compat;

    // 현재 버전은 1.0이므로 1.0만 지원
    compat.min_major = 1;
    compat.min_minor = 0;
    compat.max_major = 1;
    compat.max_minor = 0;

    return compat;
}

/**
 * 버전 문자열 반환
 * @return 버전 문자열
 */
const char* lef_get_version_string() {
    static char version_str[32];
    snprintf(version_str, sizeof(version_str), "%d.%d", LEF_VERSION_MAJOR, LEF_VERSION_MINOR);
    return version_str;
}

// ============================================================================
// 체크섬 및 검증 구현
// ============================================================================

/**
 * 파일 무결성 검증
 * @param filename 검증할 파일명
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_verify_file_integrity(const char* filename) {
    if (!filename) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    FILE* file = fopen(filename, "rb");
    if (!file) {
        return LEF_ERROR_FILE_IO;
    }

    LEFHeader header;
    if (fread(&header, sizeof(LEFHeader), 1, file) != 1) {
        fclose(file);
        return LEF_ERROR_FILE_IO;
    }

    // 헤더 검증
    if (!lef_validate_header(&header)) {
        fclose(file);
        return LEF_ERROR_INVALID_FORMAT;
    }

    // 버전 호환성 확인
    LEFVersionCompatibility compat = lef_get_current_compatibility();
    if (!lef_check_version_compatibility(header.version_major, header.version_minor, &compat)) {
        fclose(file);
        return LEF_ERROR_VERSION_INCOMPATIBLE;
    }

    // 모델 메타데이터 검증
    LEFModelMeta meta;
    if (fread(&meta, sizeof(LEFModelMeta), 1, file) != 1) {
        fclose(file);
        return LEF_ERROR_FILE_IO;
    }

    if (!lef_validate_model_meta(&meta)) {
        fclose(file);
        return LEF_ERROR_INVALID_FORMAT;
    }

    // 모델 해시 검증
    uint32_t calculated_hash = lef_calculate_model_hash(&meta);
    if (calculated_hash != header.model_hash) {
        fclose(file);
        return LEF_ERROR_CHECKSUM_MISMATCH;
    }

    fclose(file);
    return LEF_SUCCESS;
}

/**
 * 레이어 무결성 검증
 * @param header 레이어 헤더
 * @param data 레이어 데이터
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_verify_layer_integrity(const LEFLayerHeader* header, const void* data) {
    if (!header || !data) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 레이어 헤더 검증
    if (!lef_validate_layer_header(header)) {
        return LEF_ERROR_INVALID_FORMAT;
    }

    // 체크섬 검증
    uint32_t calculated_checksum = lef_calculate_crc32(data, header->data_size);
    if (calculated_checksum != header->checksum) {
        return LEF_ERROR_CHECKSUM_MISMATCH;
    }

    return LEF_SUCCESS;
}

/**
 * 파일 전체 체크섬 계산
 * @param filename 파일명
 * @return 체크섬 값 (실패 시 0)
 */
uint32_t lef_calculate_file_checksum(const char* filename) {
    if (!filename) {
        return 0;
    }

    FILE* file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    // 파일 크기 확인
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return 0;
    }

    // 버퍼 할당
    uint8_t* buffer = (uint8_t*)malloc(file_size);
    if (!buffer) {
        fclose(file);
        return 0;
    }

    // 파일 읽기
    size_t read_size = fread(buffer, 1, file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(buffer);
        return 0;
    }

    // 체크섬 계산
    uint32_t checksum = lef_calculate_crc32(buffer, file_size);
    free(buffer);

    return checksum;
}

/**
 * 에러 코드에 대한 문자열 반환
 * @param error 에러 코드
 * @return 에러 메시지 문자열
 */
const char* lef_get_error_string(LEFErrorCode error) {
    switch (error) {
        case LEF_SUCCESS:
            return "성공";
        case LEF_ERROR_INVALID_ARGUMENT:
            return "잘못된 인수";
        case LEF_ERROR_FILE_IO:
            return "파일 입출력 오류";
        case LEF_ERROR_OUT_OF_MEMORY:
            return "메모리 부족";
        case LEF_ERROR_INVALID_FORMAT:
            return "잘못된 파일 형식";
        case LEF_ERROR_COMPRESSION_FAILED:
            return "압축 실패";
        case LEF_ERROR_CHECKSUM_MISMATCH:
            return "체크섬 불일치";
        case LEF_ERROR_VERSION_INCOMPATIBLE:
            return "버전 호환성 없음";
        case LEF_ERROR_LAYER_NOT_FOUND:
            return "레이어를 찾을 수 없음";
        case LEF_ERROR_BUFFER_TOO_SMALL:
            return "버퍼 크기 부족";
        default:
            return "알 수 없는 오류";
    }
}