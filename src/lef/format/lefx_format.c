/**
 * @file lefx_format.c
 * @brief LEFX (LibEtude Extension Format) 구현
 *
 * LEFX 포맷은 LibEtude의 확장 모델을 위한 파일 포맷입니다.
 * 기본 모델에 화자, 언어, 효과 등의 확장을 추가할 수 있습니다.
 */

#include "libetude/lef_format.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ============================================================================
// LEFX 헤더 초기화 함수들
// ============================================================================

/**
 * LEFX 헤더 초기화
 * @param header 초기화할 LEFX 헤더 포인터
 */
void lefx_init_header(LEFXHeader* header) {
    if (!header) return;

    memset(header, 0, sizeof(LEFXHeader));

    // 기본 헤더 정보 설정
    header->magic = LEFX_MAGIC;
    header->version_major = LEFX_VERSION_MAJOR;
    header->version_minor = LEFX_VERSION_MINOR;
    header->extension_flags = 0;
    header->file_size = sizeof(LEFXHeader);

    // 기본 모델 참조 정보 초기화
    header->base_model_hash = 0;
    memset(header->base_model_version, 0, sizeof(header->base_model_version));
    memset(header->base_model_name, 0, sizeof(header->base_model_name));
    header->required_base_size = 0;

    // 확장 메타데이터 초기화
    header->extension_type = LEFX_EXT_CUSTOM;
    header->extension_id = 0;
    memset(header->extension_name, 0, sizeof(header->extension_name));
    memset(header->extension_author, 0, sizeof(header->extension_author));
    memset(header->extension_version, 0, sizeof(header->extension_version));

    // 오프셋 초기화 (헤더 다음부터 시작)
    header->meta_offset = sizeof(LEFXHeader);
    header->dependency_offset = 0;
    header->layer_index_offset = 0;
    header->layer_data_offset = 0;
    header->plugin_data_offset = 0;

    // 타임스탬프 설정
    header->timestamp = (uint64_t)time(NULL);
}

/**
 * LEFX 확장 메타데이터 초기화
 * @param meta 초기화할 확장 메타데이터 포인터
 */
void lefx_init_extension_meta(LEFXExtensionMeta* meta) {
    if (!meta) return;

    memset(meta, 0, sizeof(LEFXExtensionMeta));

    // 기본 정보 초기화
    memset(meta->description, 0, sizeof(meta->description));
    memset(meta->license, 0, sizeof(meta->license));
    memset(meta->website, 0, sizeof(meta->website));
    memset(meta->contact, 0, sizeof(meta->contact));

    // 호환성 정보 초기화 (모든 버전과 호환)
    meta->min_base_version_major = 0;
    meta->min_base_version_minor = 0;
    meta->max_base_version_major = 65535;
    meta->max_base_version_minor = 65535;

    // 확장 특성 정보 초기화
    meta->extension_capabilities = 0;
    meta->priority = 1000; // 기본 우선순위
    meta->num_layers = 0;
    meta->total_params = 0;
    meta->memory_requirement = 0;

    // 음성 특화 정보 초기화 (해당없음으로 설정)
    meta->gender = 255;
    meta->age_range = 255;
    memset(meta->language_code, 0, sizeof(meta->language_code));
    memset(meta->accent_code, 0, sizeof(meta->accent_code));

    // 품질 및 성능 정보 초기화
    meta->quality_score = 0.5f; // 기본 품질 점수
    meta->performance_impact = 0.1f; // 낮은 성능 영향
    meta->inference_time_ms = 0;
    meta->loading_time_ms = 0;
}

/**
 * LEFX 레이어 헤더 초기화
 * @param layer_header 초기화할 레이어 헤더 포인터
 * @param extension_layer_id 확장 레이어 ID
 * @param base_layer_id 기본 레이어 ID (차분 모델용)
 */
void lefx_init_layer_header(LEFXLayerHeader* layer_header,
                           uint16_t extension_layer_id,
                           uint16_t base_layer_id) {
    if (!layer_header) return;

    memset(layer_header, 0, sizeof(LEFXLayerHeader));

    // 레이어 ID 설정
    layer_header->extension_layer_id = extension_layer_id;
    layer_header->base_layer_id = base_layer_id;

    // 레이어 타입 및 설정 초기화
    layer_header->layer_kind = LEF_LAYER_CUSTOM;
    layer_header->quantization_type = LEF_QUANT_NONE;
    layer_header->blend_mode = 0; // 교체 모드
    layer_header->activation_condition = 0; // 항상 활성화

    // 데이터 크기 정보 초기화
    layer_header->meta_size = 0;
    layer_header->data_size = 0;
    layer_header->compressed_size = 0;
    layer_header->data_offset = 0;
    layer_header->checksum = 0;

    // 차분 모델 정보 초기화
    layer_header->similarity_threshold = 0.0f;
    layer_header->blend_weight = 1.0f; // 완전 적용
    layer_header->dependency_count = 0;
    layer_header->reserved_flags = 0;
}

/**
 * LEFX 의존성 정보 초기화
 * @param dependency 초기화할 의존성 정보 포인터
 */
void lefx_init_dependency(LEFXDependency* dependency) {
    if (!dependency) return;

    memset(dependency, 0, sizeof(LEFXDependency));

    dependency->dependency_id = 0;
    memset(dependency->dependency_name, 0, sizeof(dependency->dependency_name));
    memset(dependency->min_version, 0, sizeof(dependency->min_version));
    memset(dependency->max_version, 0, sizeof(dependency->max_version));
    dependency->dependency_type = 0; // 필수 의존성
    dependency->load_order = 2; // 상관없음
}

/**
 * LEFX 활성화 규칙 초기화
 * @param rule 초기화할 활성화 규칙 포인터
 */
void lefx_init_activation_rule(LEFXActivationRule* rule) {
    if (!rule) return;

    memset(rule, 0, sizeof(LEFXActivationRule));

    rule->rule_id = 0;
    rule->condition_type = 0; // 텍스트 조건
    rule->operator_type = 0; // 같음 연산자
    memset(rule->condition_value, 0, sizeof(rule->condition_value));
    rule->activation_weight = 1.0f; // 완전 활성화
    rule->priority = 100; // 기본 우선순위
}

/**
 * LEFX 플러그인 데이터 초기화
 * @param plugin_data 초기화할 플러그인 데이터 포인터
 */
void lefx_init_plugin_data(LEFXPluginData* plugin_data) {
    if (!plugin_data) return;

    memset(plugin_data, 0, sizeof(LEFXPluginData));

    memset(plugin_data->plugin_interface, 0, sizeof(plugin_data->plugin_interface));
    memset(plugin_data->plugin_version, 0, sizeof(plugin_data->plugin_version));
    plugin_data->plugin_data_size = 0;
    plugin_data->plugin_data_offset = 0;
    plugin_data->init_function_offset = 0;
    plugin_data->process_function_offset = 0;
    plugin_data->cleanup_function_offset = 0;
}

// ============================================================================
// LEFX 헤더 검증 함수들
// ============================================================================

/**
 * LEFX 헤더 검증
 * @param header 검증할 LEFX 헤더 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lefx_validate_header(const LEFXHeader* header) {
    if (!header) return false;

    // 매직 넘버 검증
    if (header->magic != LEFX_MAGIC) {
        return false;
    }

    // 버전 검증 (현재 버전과 호환되는지 확인)
    if (header->version_major > LEFX_VERSION_MAJOR) {
        return false; // 미래 버전은 지원하지 않음
    }

    // 파일 크기 검증
    if (header->file_size < sizeof(LEFXHeader)) {
        return false;
    }

    // 확장 타입 검증
    if (header->extension_type > LEFX_EXT_CUSTOM && header->extension_type != LEFX_EXT_CUSTOM) {
        return false;
    }

    // 오프셋 검증
    if (header->meta_offset < sizeof(LEFXHeader)) {
        return false;
    }

    // 기본 모델 이름과 버전이 설정되어 있는지 확인
    if (strlen(header->base_model_name) == 0 || strlen(header->base_model_version) == 0) {
        return false;
    }

    // 확장 이름과 버전이 설정되어 있는지 확인
    if (strlen(header->extension_name) == 0 || strlen(header->extension_version) == 0) {
        return false;
    }

    return true;
}

/**
 * LEFX 확장 메타데이터 검증
 * @param meta 검증할 확장 메타데이터 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lefx_validate_extension_meta(const LEFXExtensionMeta* meta) {
    if (!meta) return false;

    // 호환성 정보 검증
    if (meta->min_base_version_major > meta->max_base_version_major) {
        return false;
    }

    if (meta->min_base_version_major == meta->max_base_version_major &&
        meta->min_base_version_minor > meta->max_base_version_minor) {
        return false;
    }

    // 품질 점수 범위 검증
    if (meta->quality_score < 0.0f || meta->quality_score > 1.0f) {
        return false;
    }

    // 성능 영향도 범위 검증
    if (meta->performance_impact < 0.0f || meta->performance_impact > 1.0f) {
        return false;
    }

    // 우선순위 범위 검증 (0-65535)
    // uint16_t이므로 자동으로 범위 내에 있음

    return true;
}

/**
 * LEFX 레이어 헤더 검증
 * @param layer_header 검증할 레이어 헤더 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lefx_validate_layer_header(const LEFXLayerHeader* layer_header) {
    if (!layer_header) return false;

    // 레이어 타입 검증
    if (layer_header->layer_kind > LEF_LAYER_CUSTOM &&
        layer_header->layer_kind != LEF_LAYER_CUSTOM) {
        return false;
    }

    // 양자화 타입 검증
    if (layer_header->quantization_type > LEF_QUANT_MIXED &&
        layer_header->quantization_type != LEF_QUANT_MIXED) {
        return false;
    }

    // 블렌딩 모드 검증 (0-3)
    if (layer_header->blend_mode > 3) {
        return false;
    }

    // 활성화 조건 검증 (0-1)
    if (layer_header->activation_condition > 1) {
        return false;
    }

    // 블렌딩 가중치 범위 검증
    if (layer_header->blend_weight < 0.0f || layer_header->blend_weight > 1.0f) {
        return false;
    }

    // 유사도 임계값 범위 검증
    if (layer_header->similarity_threshold < 0.0f || layer_header->similarity_threshold > 1.0f) {
        return false;
    }

    // 데이터 크기 일관성 검증
    if (layer_header->compressed_size > 0 && layer_header->compressed_size > layer_header->data_size) {
        return false;
    }

    return true;
}

/**
 * LEFX 의존성 정보 검증
 * @param dependency 검증할 의존성 정보 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lefx_validate_dependency(const LEFXDependency* dependency) {
    if (!dependency) return false;

    // 의존성 이름이 설정되어 있는지 확인
    if (strlen(dependency->dependency_name) == 0) {
        return false;
    }

    // 의존성 타입 검증 (0-2)
    if (dependency->dependency_type > 2) {
        return false;
    }

    // 로드 순서 검증 (0-2)
    if (dependency->load_order > 2) {
        return false;
    }

    // 버전 정보가 설정되어 있는지 확인
    if (strlen(dependency->min_version) == 0 || strlen(dependency->max_version) == 0) {
        return false;
    }

    return true;
}

/**
 * LEFX 활성화 규칙 검증
 * @param rule 검증할 활성화 규칙 포인터
 * @return 유효하면 true, 무효하면 false
 */
bool lefx_validate_activation_rule(const LEFXActivationRule* rule) {
    if (!rule) return false;

    // 조건 타입 검증 (0-4)
    if (rule->condition_type > 4) {
        return false;
    }

    // 연산자 타입 검증 (0-3)
    if (rule->operator_type > 3) {
        return false;
    }

    // 활성화 가중치 범위 검증
    if (rule->activation_weight < 0.0f || rule->activation_weight > 1.0f) {
        return false;
    }

    // 조건 값이 설정되어 있는지 확인
    if (strlen(rule->condition_value) == 0) {
        return false;
    }

    return true;
}