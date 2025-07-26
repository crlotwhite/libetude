/**
 * @file lef_format.c
 * @brief LEF (LibEtude Efficient Format) 기본 포맷 구현
 *
 * LEF 포맷의 기본 기능들을 구현합니다.
 */

#include "libetude/lef_format.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// ============================================================================
// CRC32 체크섬 계산
// ============================================================================

// CRC32 테이블 (IEEE 802.3 표준)
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

/**
 * CRC32 테이블 초기화
 */
static void init_crc32_table() {
    if (crc32_table_initialized) return;

    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 8; j > 0; j--) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

/**
 * CRC32 체크섬 계산
 * @param data 데이터 포인터
 * @param size 데이터 크기
 * @return CRC32 체크섬
 */
uint32_t lef_calculate_crc32(const void* data, size_t size) {
    if (!data || size == 0) return 0;

    init_crc32_table();

    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = (const uint8_t*)data;

    for (size_t i = 0; i < size; i++) {
        uint8_t table_index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[table_index];
    }

    return crc ^ 0xFFFFFFFF;
}

/**
 * 모델 해시 계산
 * @param meta 모델 메타데이터
 * @return 모델 해시값
 */
uint32_t lef_calculate_model_hash(const LEFModelMeta* meta) {
    if (!meta) return 0;

    // 모델 이름, 버전, 아키텍처 정보를 조합하여 해시 계산
    char hash_input[512];
    snprintf(hash_input, sizeof(hash_input),
             "%s_%s_%d_%d_%d_%d_%d_%d_%d_%d_%d_%d",
             meta->model_name,
             meta->model_version,
             meta->input_dim,
             meta->output_dim,
             meta->hidden_dim,
             meta->num_layers,
             meta->num_heads,
             meta->vocab_size,
             meta->sample_rate,
             meta->mel_channels,
             meta->hop_length,
             meta->win_length);

    return lef_calculate_crc32(hash_input, strlen(hash_input));
}

// ============================================================================
// 헤더 초기화 함수들
// ============================================================================

/**
 * LEF 헤더 초기화
 * @param header 초기화할 헤더 포인터
 */
void lef_init_header(LEFHeader* header) {
    if (!header) return;

    memset(header, 0, sizeof(LEFHeader));

    header->magic = LEF_MAGIC;
    header->version_major = LEF_VERSION_MAJOR;
    header->version_minor = LEF_VERSION_MINOR;
    header->flags = 0;
    header->file_size = sizeof(LEFHeader);
    header->model_hash = 0;
    header->timestamp = (uint64_t)time(NULL);
    header->compression_dict_offset = 0;
    header->layer_index_offset = sizeof(LEFHeader) + sizeof(LEFModelMeta);
    header->layer_data_offset = 0; // 나중에 계산
}

/**
 * LEF 모델 메타데이터 초기화
 * @param meta 초기화할 메타데이터 포인터
 */
void lef_init_model_meta(LEFModelMeta* meta) {
    if (!meta) return;

    memset(meta, 0, sizeof(LEFModelMeta));

    // 기본값 설정
    strcpy(meta->model_name, "untitled_model");
    strcpy(meta->model_version, "1.0.0");
    strcpy(meta->author, "unknown");
    strcpy(meta->description, "LibEtude TTS model");

    meta->input_dim = 256;
    meta->output_dim = 80; // Mel channels
    meta->hidden_dim = 512;
    meta->num_layers = 6;
    meta->num_heads = 8;
    meta->vocab_size = 1000;

    meta->sample_rate = 22050;
    meta->mel_channels = 80;
    meta->hop_length = 256;
    meta->win_length = 1024;

    meta->default_quantization = LEF_QUANT_NONE;
    meta->mixed_precision = 0;
    meta->quantization_params_size = 0;
}

/**
 * LEF 레이어 헤더 초기화
 * @param layer_header 초기화할 레이어 헤더 포인터
 * @param layer_id 레이어 ID
 * @param kind 레이어 타입
 */
void lef_init_layer_header(LEFLayerHeader* layer_header, uint16_t layer_id, LEFLayerKind kind) {
    if (!layer_header) return;

    memset(layer_header, 0, sizeof(LEFLayerHeader));

    layer_header->layer_id = layer_id;
    layer_header->layer_kind = (uint8_t)kind;
    layer_header->quantization_type = LEF_QUANT_NONE;
    layer_header->meta_size = 0;
    layer_header->data_size = 0;
    layer_header->compressed_size = 0;
    layer_header->data_offset = 0;
    layer_header->checksum = 0;
}

// ============================================================================
// 헤더 검증 함수들
// ============================================================================

/**
 * LEF 헤더 검증
 * @param header 검증할 헤더 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lef_validate_header(const LEFHeader* header) {
    if (!header) return false;

    // 매직 넘버 검증
    if (header->magic != LEF_MAGIC) return false;

    // 버전 검증 (현재 버전과 호환되는지 확인)
    if (header->version_major > LEF_VERSION_MAJOR) return false;

    // 파일 크기 검증
    if (header->file_size < sizeof(LEFHeader)) return false;

    // 오프셋 검증
    if (header->layer_index_offset < sizeof(LEFHeader) + sizeof(LEFModelMeta)) return false;
    if (header->layer_data_offset != 0 && header->layer_data_offset <= header->layer_index_offset) return false;

    return true;
}

/**
 * LEF 모델 메타데이터 검증
 * @param meta 검증할 메타데이터 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lef_validate_model_meta(const LEFModelMeta* meta) {
    if (!meta) return false;

    // 모델 이름과 버전이 설정되어 있는지 확인
    if (strlen(meta->model_name) == 0 || strlen(meta->model_version) == 0) return false;

    // 아키텍처 정보 검증
    if (meta->input_dim == 0 || meta->output_dim == 0 || meta->hidden_dim == 0) return false;
    if (meta->num_layers == 0) return false;

    // 오디오 설정 검증
    if (meta->sample_rate == 0 || meta->mel_channels == 0) return false;
    if (meta->hop_length == 0 || meta->win_length == 0) return false;

    // 양자화 타입 검증
    if (meta->default_quantization > LEF_QUANT_MIXED) return false;

    return true;
}

/**
 * LEF 레이어 헤더 검증
 * @param layer_header 검증할 레이어 헤더 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lef_validate_layer_header(const LEFLayerHeader* layer_header) {
    if (!layer_header) return false;

    // 레이어 타입 검증
    if (layer_header->layer_kind > LEF_LAYER_CUSTOM && layer_header->layer_kind != LEF_LAYER_CUSTOM) return false;

    // 양자화 타입 검증
    if (layer_header->quantization_type > LEF_QUANT_MIXED) return false;

    // 데이터 크기 일관성 검증
    if (layer_header->compressed_size > 0 && layer_header->compressed_size > layer_header->data_size) return false;

    return true;
}

// ============================================================================
// 기본 모델 로딩 함수들 (스텁 구현)
// ============================================================================

/**
 * 파일에서 모델 로드
 * @param path 모델 파일 경로
 * @return 로드된 모델 포인터 (실패 시 NULL)
 */
LEFModel* lef_load_model(const char* path) {
    if (!path) return NULL;

    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    // 파일 크기 확인
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < (long)(sizeof(LEFHeader) + sizeof(LEFModelMeta))) {
        fclose(file);
        return NULL;
    }

    // 모델 구조체 할당
    LEFModel* model = (LEFModel*)calloc(1, sizeof(LEFModel));
    if (!model) {
        fclose(file);
        return NULL;
    }

    // 헤더 읽기
    if (fread(&model->header, sizeof(LEFHeader), 1, file) != 1) {
        free(model);
        fclose(file);
        return NULL;
    }

    // 헤더 검증
    if (!lef_validate_header(&model->header)) {
        free(model);
        fclose(file);
        return NULL;
    }

    // 메타데이터 읽기
    if (fread(&model->meta, sizeof(LEFModelMeta), 1, file) != 1) {
        free(model);
        fclose(file);
        return NULL;
    }

    // 메타데이터 검증
    if (!lef_validate_model_meta(&model->meta)) {
        free(model);
        fclose(file);
        return NULL;
    }

    // 레이어 수 설정
    model->num_layers = model->meta.num_layers;

    // 레이어 헤더 배열 할당
    if (model->num_layers > 0) {
        model->layer_headers = (LEFLayerHeader*)calloc(model->num_layers, sizeof(LEFLayerHeader));
        model->layer_data = (void**)calloc(model->num_layers, sizeof(void*));

        if (!model->layer_headers || !model->layer_data) {
            if (model->layer_headers) free(model->layer_headers);
            if (model->layer_data) free(model->layer_data);
            free(model);
            fclose(file);
            return NULL;
        }

        // 레이어 인덱스로 이동
        fseek(file, model->header.layer_index_offset, SEEK_SET);

        // 레이어 헤더들 읽기 (간단한 구현)
        for (size_t i = 0; i < model->num_layers; i++) {
            if (fread(&model->layer_headers[i], sizeof(LEFLayerHeader), 1, file) != 1) {
                lef_unload_model(model);
                fclose(file);
                return NULL;
            }
        }
    }

    // 파일 정보 저장
    model->file_size = file_size;
    model->file_path = strdup(path);
    model->owns_memory = true;
    model->memory_mapped = false;

    fclose(file);
    return model;
}

/**
 * 메모리에서 모델 로드
 * @param data 모델 데이터 포인터
 * @param size 데이터 크기
 * @return 로드된 모델 포인터 (실패 시 NULL)
 */
LEFModel* lef_load_model_from_memory(const void* data, size_t size) {
    if (!data || size < sizeof(LEFHeader) + sizeof(LEFModelMeta)) {
        return NULL;
    }

    // 모델 구조체 할당
    LEFModel* model = (LEFModel*)calloc(1, sizeof(LEFModel));
    if (!model) return NULL;

    const uint8_t* bytes = (const uint8_t*)data;
    size_t offset = 0;

    // 헤더 복사
    memcpy(&model->header, bytes + offset, sizeof(LEFHeader));
    offset += sizeof(LEFHeader);

    // 헤더 검증
    if (!lef_validate_header(&model->header)) {
        free(model);
        return NULL;
    }

    // 메타데이터 복사
    memcpy(&model->meta, bytes + offset, sizeof(LEFModelMeta));
    offset += sizeof(LEFModelMeta);

    // 메타데이터 검증
    if (!lef_validate_model_meta(&model->meta)) {
        free(model);
        return NULL;
    }

    // 레이어 수 설정
    model->num_layers = model->meta.num_layers;

    // 레이어 헤더 배열 할당
    if (model->num_layers > 0) {
        model->layer_headers = (LEFLayerHeader*)calloc(model->num_layers, sizeof(LEFLayerHeader));
        model->layer_data = (void**)calloc(model->num_layers, sizeof(void*));

        if (!model->layer_headers || !model->layer_data) {
            if (model->layer_headers) free(model->layer_headers);
            if (model->layer_data) free(model->layer_data);
            free(model);
            return NULL;
        }

        // 레이어 헤더들 복사 (간단한 구현)
        offset = model->header.layer_index_offset;
        for (size_t i = 0; i < model->num_layers; i++) {
            if (offset + sizeof(LEFLayerHeader) > size) {
                lef_unload_model(model);
                return NULL;
            }
            memcpy(&model->layer_headers[i], bytes + offset, sizeof(LEFLayerHeader));
            offset += sizeof(LEFLayerHeader);
        }
    }

    // 메모리 정보 저장
    model->file_data = (void*)data; // const 제거 (읽기 전용으로 사용)
    model->file_size = size;
    model->file_path = NULL;
    model->owns_memory = false; // 외부에서 관리
    model->memory_mapped = false;

    return model;
}

/**
 * 모델 언로드 및 메모리 해제
 * @param model 해제할 모델 포인터
 */
void lef_unload_model(LEFModel* model) {
    if (!model) return;

    // 레이어 데이터 해제
    if (model->layer_data) {
        for (size_t i = 0; i < model->num_layers; i++) {
            if (model->layer_data[i] && model->owns_memory) {
                free(model->layer_data[i]);
            }
        }
        free(model->layer_data);
    }

    // 레이어 헤더 해제
    if (model->layer_headers) {
        free(model->layer_headers);
    }

    // 파일 데이터 해제 (소유권이 있는 경우)
    if (model->file_data && model->owns_memory) {
        free(model->file_data);
    }

    // 파일 경로 해제
    if (model->file_path) {
        free(model->file_path);
    }

    // 파일 핸들 닫기
    if (model->file_handle) {
        fclose(model->file_handle);
    }

    free(model);
}

/**
 * 특정 레이어 데이터 가져오기
 * @param model 모델 포인터
 * @param layer_id 레이어 ID
 * @return 레이어 데이터 포인터 (실패 시 NULL)
 */
void* lef_get_layer_data(LEFModel* model, uint16_t layer_id) {
    if (!model || !model->layer_headers || !model->layer_data) {
        return NULL;
    }

    // 레이어 ID로 인덱스 찾기
    for (size_t i = 0; i < model->num_layers; i++) {
        if (model->layer_headers[i].layer_id == layer_id) {
            // 이미 로드된 경우 반환
            if (model->layer_data[i]) {
                return model->layer_data[i];
            }

            // 온디맨드 로딩 (간단한 구현)
            if (model->file_path && model->layer_headers[i].data_size > 0) {
                FILE* file = fopen(model->file_path, "rb");
                if (!file) return NULL;

                // 레이어 데이터 위치로 이동
                fseek(file, model->layer_headers[i].data_offset, SEEK_SET);

                // 메모리 할당 및 데이터 읽기
                void* data = malloc(model->layer_headers[i].data_size);
                if (data) {
                    if (fread(data, model->layer_headers[i].data_size, 1, file) == 1) {
                        model->layer_data[i] = data;
                    } else {
                        free(data);
                        data = NULL;
                    }
                }

                fclose(file);
                return data;
            }

            return NULL;
        }
    }

    return NULL;
}

/**
 * 레이어 헤더 가져오기
 * @param model 모델 포인터
 * @param layer_id 레이어 ID
 * @return 레이어 헤더 포인터 (실패 시 NULL)
 */
const LEFLayerHeader* lef_get_layer_header(LEFModel* model, uint16_t layer_id) {
    if (!model || !model->layer_headers) {
        return NULL;
    }

    // 레이어 ID로 헤더 찾기
    for (size_t i = 0; i < model->num_layers; i++) {
        if (model->layer_headers[i].layer_id == layer_id) {
            return &model->layer_headers[i];
        }
    }

    return NULL;
}

/**
 * 모델 통계 정보 가져오기
 * @param model 모델 포인터
 * @param total_params 총 파라미터 수 (출력)
 * @param total_size 총 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_get_model_stats(const LEFModel* model, size_t* total_params, size_t* total_size) {
    if (!model) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    size_t params = 0;
    size_t size = 0;

    // 레이어별 파라미터 수와 크기 계산
    for (size_t i = 0; i < model->num_layers; i++) {
        const LEFLayerHeader* header = &model->layer_headers[i];

        // 파라미터 수 추정 (데이터 크기를 float 크기로 나눔)
        size_t layer_params = header->data_size / sizeof(float);
        params += layer_params;

        // 총 크기
        size += header->data_size;
    }

    if (total_params) *total_params = params;
    if (total_size) *total_size = size;

    return LEF_SUCCESS;
}

/**
 * 에러 메시지 함수
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
            return "버전 비호환";
        case LEF_ERROR_LAYER_NOT_FOUND:
            return "레이어를 찾을 수 없음";
        case LEF_ERROR_BUFFER_TOO_SMALL:
            return "버퍼 크기 부족";
        default:
            return "알 수 없는 오류";
    }
}

/**
 * 모델 정보 출력
 * @param model 모델 포인터
 */
void lef_print_model_info(const LEFModel* model) {
    if (!model) {
        printf("모델이 NULL입니다.\n");
        return;
    }

    printf("=== LEF 모델 정보 ===\n");
    printf("모델 이름: %s\n", model->meta.model_name);
    printf("모델 버전: %s\n", model->meta.model_version);
    printf("제작자: %s\n", model->meta.author);
    printf("설명: %s\n", model->meta.description);

    printf("\n=== 아키텍처 정보 ===\n");
    printf("입력 차원: %d\n", model->meta.input_dim);
    printf("출력 차원: %d\n", model->meta.output_dim);
    printf("은닉 차원: %d\n", model->meta.hidden_dim);
    printf("레이어 수: %d\n", model->meta.num_layers);
    printf("어텐션 헤드 수: %d\n", model->meta.num_heads);
    printf("어휘 크기: %d\n", model->meta.vocab_size);

    printf("\n=== 오디오 설정 ===\n");
    printf("샘플링 레이트: %d Hz\n", model->meta.sample_rate);
    printf("Mel 채널 수: %d\n", model->meta.mel_channels);
    printf("Hop 길이: %d\n", model->meta.hop_length);
    printf("윈도우 길이: %d\n", model->meta.win_length);

    printf("\n=== 파일 정보 ===\n");
    printf("파일 크기: %zu 바이트\n", model->file_size);
    printf("파일 경로: %s\n", model->file_path ? model->file_path : "메모리");
    printf("메모리 매핑: %s\n", model->memory_mapped ? "예" : "아니오");

    size_t total_params, total_size;
    if (lef_get_model_stats(model, &total_params, &total_size) == LEF_SUCCESS) {
        printf("총 파라미터 수: %zu\n", total_params);
        printf("총 데이터 크기: %zu 바이트\n", total_size);
    }
}

/**
 * 레이어 정보 출력
 * @param model 모델 포인터
 */
void lef_print_layer_info(const LEFModel* model) {
    if (!model || !model->layer_headers) {
        printf("모델 또는 레이어 정보가 없습니다.\n");
        return;
    }

    printf("=== 레이어 정보 ===\n");
    for (size_t i = 0; i < model->num_layers; i++) {
        const LEFLayerHeader* header = &model->layer_headers[i];
        printf("레이어 %d: ID=%d, 타입=%d, 크기=%d 바이트\n",
               (int)i, header->layer_id, header->layer_kind, header->data_size);
    }
}