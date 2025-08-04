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

// ============================================================================
// LEF 헤더 검증
// ============================================================================

/**
 * LEF 헤더 검증
 * @param header 헤더 포인터
 * @return 유효하면 true, 아니면 false
 */
bool lef_validate_header(const LEFHeader* header) {
    if (!header) return false;

    // 매직 넘버 확인
    if (header->magic != LEF_MAGIC) {
        return false;
    }

    // 버전 확인 (현재는 1.0만 지원)
    if (header->version_major != 1 || header->version_minor != 0) {
        return false;
    }

    // 플래그 검증 (예약된 비트는 0이어야 함)
    if (header->flags & 0xF0) {
        return false;
    }

    return true;
}

/**
 * 모델 메타데이터 검증
 * @param meta 메타데이터 포인터
 * @return 유효하면 true, 아니면 false
 */
bool lef_validate_model_meta(const LEFModelMeta* meta) {
    if (!meta) return false;

    // 기본 차원 검증
    if (meta->input_dim == 0 || meta->output_dim == 0) {
        return false;
    }

    // 레이어 수 검증
    if (meta->num_layers == 0 || meta->num_layers > 1000) {
        return false;
    }

    // 샘플링 레이트 검증
    if (meta->sample_rate < 8000 || meta->sample_rate > 48000) {
        return false;
    }

    // Mel 채널 수 검증
    if (meta->mel_channels == 0 || meta->mel_channels > 512) {
        return false;
    }

    return true;
}

// ============================================================================
// LEF 헤더 생성
// ============================================================================

/**
 * LEF 헤더 생성
 * @param header 헤더 포인터 (출력)
 * @param model_size 모델 데이터 크기
 * @param num_layers 레이어 수
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
LEFErrorCode lef_create_header(LEFHeader* header, size_t model_size, uint16_t num_layers) {
    if (!header) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 헤더 초기화
    memset(header, 0, sizeof(LEFHeader));

    // 매직 넘버 설정
    header->magic = LEF_MAGIC;

    // 버전 설정
    header->version_major = 1;
    header->version_minor = 0;

    // 플래그 설정 (기본값)
    header->flags = 0;

    // 크기 정보 설정
    header->file_size = model_size;

    // 오프셋 계산
    header->layer_index_offset = sizeof(LEFHeader) + sizeof(LEFModelMeta);
    header->layer_data_offset = header->layer_index_offset + (num_layers * sizeof(LEFLayerHeader));

    // 타임스탬프 설정
    header->timestamp = (uint64_t)time(NULL);

    // 해시는 나중에 계산됨
    header->model_hash = 0;

    return LEF_SUCCESS;
}

/**
 * 모델 메타데이터 생성
 * @param meta 메타데이터 포인터 (출력)
 * @param model_name 모델 이름
 * @param model_version 모델 버전
 * @param author 제작자
 * @param description 설명
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
LEFErrorCode lef_create_model_meta(LEFModelMeta* meta,
                                   const char* model_name,
                                   const char* model_version,
                                   const char* author,
                                   const char* description) {
    if (!meta) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 메타데이터 초기화
    memset(meta, 0, sizeof(LEFModelMeta));

    // 문자열 복사 (안전하게)
    if (model_name) {
        strncpy(meta->model_name, model_name, sizeof(meta->model_name) - 1);
    }
    if (model_version) {
        strncpy(meta->model_version, model_version, sizeof(meta->model_version) - 1);
    }
    if (author) {
        strncpy(meta->author, author, sizeof(meta->author) - 1);
    }
    if (description) {
        strncpy(meta->description, description, sizeof(meta->description) - 1);
    }

    // 기본값 설정
    meta->input_dim = 80;  // 기본 Mel 채널 수
    meta->output_dim = 1;  // 기본 출력 차원
    meta->hidden_dim = 256; // 기본 은닉 차원
    meta->num_layers = 1;   // 기본 레이어 수
    meta->sample_rate = 22050; // 기본 샘플링 레이트
    meta->mel_channels = 80;   // 기본 Mel 채널 수
    meta->hop_length = 256;    // 기본 Hop 길이
    meta->win_length = 1024;   // 기본 윈도우 길이

    return LEF_SUCCESS;
}

// ============================================================================
// 에러 메시지 함수
// ============================================================================

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
 * 모델 해시 계산
 * @param meta 모델 메타데이터 포인터
 * @return 계산된 해시값
 */
uint32_t lef_calculate_model_hash(const LEFModelMeta* meta) {
    if (!meta) return 0;

    // 메타데이터의 주요 필드들을 기반으로 해시 계산
    return lef_calculate_crc32(meta, sizeof(LEFModelMeta));
}

// 참고: 다음 함수들은 model_loader.c에서 구현됨:
// - lef_load_model()
// - lef_load_model_from_memory()
// - lef_unload_model()
// - lef_get_layer_data()
// - lef_get_layer_header()
// - lef_get_model_stats()
// - lef_print_model_info()
// - lef_print_layer_info()