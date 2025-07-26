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
#include <math.h>
#include <float.h>
#include <stdio.h>

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

// ============================================================================
// 차분 모델 시스템 구현
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
                                       float similarity_threshold) {
    if (!base_model || !speaker_model) {
        return NULL;
    }

    // 유사도 임계값 범위 검증
    if (similarity_threshold < 0.0f || similarity_threshold > 1.0f) {
        return NULL;
    }

    // 모델 호환성 검증 (레이어 수가 같아야 함)
    if (base_model->num_layers != speaker_model->num_layers) {
        return NULL;
    }

    LEFDiffContext* ctx = (LEFDiffContext*)malloc(sizeof(LEFDiffContext));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(LEFDiffContext));

    // 기본 설정
    ctx->base_model = base_model;
    ctx->speaker_model = speaker_model;
    ctx->similarity_threshold = similarity_threshold;
    ctx->compression_threshold = 0.8f; // 기본 압축 임계값
    ctx->enable_sparse_diff = true;
    ctx->enable_quantization = false;

    // 차분 배열 초기화
    ctx->diff_capacity = base_model->num_layers;
    ctx->layer_diffs = (LEFLayerDiff*)calloc(ctx->diff_capacity, sizeof(LEFLayerDiff));
    if (!ctx->layer_diffs) {
        free(ctx);
        return NULL;
    }

    // 통계 정보 초기화
    ctx->total_original_size = 0;
    ctx->total_diff_size = 0;
    ctx->average_similarity = 0.0f;
    ctx->layers_compressed = 0;
    ctx->layers_skipped = 0;

    return ctx;
}

/**
 * 차분 모델 생성 컨텍스트 해제
 * @param ctx 차분 컨텍스트 포인터
 */
void lef_destroy_diff_context(LEFDiffContext* ctx) {
    if (!ctx) return;

    // 차분 데이터 해제
    if (ctx->layer_diffs) {
        for (size_t i = 0; i < ctx->num_diffs; i++) {
            if (ctx->layer_diffs[i].diff_data && ctx->layer_diffs[i].diff_size > 0) {
                free(ctx->layer_diffs[i].diff_data);
                ctx->layer_diffs[i].diff_data = NULL;
            }
        }
        free(ctx->layer_diffs);
    }

    free(ctx);
}

/**
 * 레이어 간 유사도 계산 (코사인 유사도 사용)
 * @param base_layer_data 기본 레이어 데이터
 * @param speaker_layer_data 화자 레이어 데이터
 * @param data_size 데이터 크기
 * @param layer_kind 레이어 타입
 * @return 유사도 점수 (0.0-1.0)
 */
float lef_calculate_layer_similarity(const void* base_layer_data,
                                    const void* speaker_layer_data,
                                    size_t data_size,
                                    LEFLayerKind layer_kind) {
    if (!base_layer_data || !speaker_layer_data || data_size == 0) {
        return 0.0f;
    }

    const float* base_data = (const float*)base_layer_data;
    const float* speaker_data = (const float*)speaker_layer_data;
    size_t num_elements = data_size / sizeof(float);

    // 코사인 유사도 계산
    double dot_product = 0.0;
    double base_norm = 0.0;
    double speaker_norm = 0.0;

    for (size_t i = 0; i < num_elements; i++) {
        dot_product += (double)base_data[i] * (double)speaker_data[i];
        base_norm += (double)base_data[i] * (double)base_data[i];
        speaker_norm += (double)speaker_data[i] * (double)speaker_data[i];
    }

    base_norm = sqrt(base_norm);
    speaker_norm = sqrt(speaker_norm);

    if (base_norm == 0.0 || speaker_norm == 0.0) {
        return 0.0f;
    }

    double cosine_similarity = dot_product / (base_norm * speaker_norm);

    // 코사인 유사도를 0-1 범위로 정규화 ([-1, 1] -> [0, 1])
    float normalized_similarity = (float)((cosine_similarity + 1.0) / 2.0);

    // 레이어 타입에 따른 가중치 적용
    switch (layer_kind) {
        case LEF_LAYER_EMBEDDING:
            // 임베딩 레이어는 화자별 차이가 클 수 있음
            normalized_similarity *= 0.9f;
            break;
        case LEF_LAYER_ATTENTION:
            // 어텐션 레이어는 상대적으로 안정적
            normalized_similarity *= 1.1f;
            break;
        case LEF_LAYER_VOCODER:
            // 보코더 레이어는 화자별 차이가 매우 클 수 있음
            normalized_similarity *= 0.8f;
            break;
        default:
            // 기본 가중치 유지
            break;
    }

    // 범위 제한
    if (normalized_similarity > 1.0f) normalized_similarity = 1.0f;
    if (normalized_similarity < 0.0f) normalized_similarity = 0.0f;

    return normalized_similarity;
}

/**
 * 레이어 차분 생성
 * @param ctx 차분 컨텍스트
 * @param base_layer_id 기본 레이어 ID
 * @param speaker_layer_id 화자 레이어 ID
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_create_layer_diff(LEFDiffContext* ctx,
                         uint16_t base_layer_id,
                         uint16_t speaker_layer_id) {
    if (!ctx || !ctx->base_model || !ctx->speaker_model) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 레이어 데이터 가져오기
    void* base_data = lef_get_layer_data(ctx->base_model, base_layer_id);
    void* speaker_data = lef_get_layer_data(ctx->speaker_model, speaker_layer_id);

    if (!base_data || !speaker_data) {
        return LEF_ERROR_LAYER_NOT_FOUND;
    }

    // 레이어 헤더 가져오기
    const LEFLayerHeader* base_header = lef_get_layer_header(ctx->base_model, base_layer_id);
    const LEFLayerHeader* speaker_header = lef_get_layer_header(ctx->speaker_model, speaker_layer_id);

    if (!base_header || !speaker_header) {
        return LEF_ERROR_LAYER_NOT_FOUND;
    }

    // 레이어 크기가 같은지 확인
    if (base_header->data_size != speaker_header->data_size) {
        return LEF_ERROR_INVALID_FORMAT;
    }

    // 유사도 계산
    float similarity = lef_calculate_layer_similarity(
        base_data, speaker_data, base_header->data_size,
        (LEFLayerKind)base_header->layer_kind
    );

    // 차분 정보 생성
    LEFLayerDiff* diff = &ctx->layer_diffs[ctx->num_diffs];
    diff->base_layer_id = base_layer_id;
    diff->speaker_layer_id = speaker_layer_id;
    diff->similarity_score = similarity;
    diff->original_size = base_header->data_size;

    // 유사도가 임계값보다 높으면 차분 생성 건너뛰기
    if (similarity >= ctx->similarity_threshold) {
        diff->diff_size = 0;
        diff->diff_data = NULL;
        diff->diff_type = 0;
        diff->compression_ratio = 0.0f; // 차분 없음 (0% 크기)
        ctx->layers_skipped++;
    } else {
        // 차분 데이터 생성 (단순 델타 압축)
        size_t num_elements = base_header->data_size / sizeof(float);
        float* base_floats = (float*)base_data;
        float* speaker_floats = (float*)speaker_data;

        // 델타 계산
        float* delta_data = (float*)malloc(base_header->data_size);
        if (!delta_data) {
            return LEF_ERROR_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < num_elements; i++) {
            delta_data[i] = speaker_floats[i] - base_floats[i];
        }

        // 스파스 차분 적용 (옵션)
        if (ctx->enable_sparse_diff) {
            void* sparse_diff = NULL;
            size_t sparse_size = 0;

            int result = lef_create_sparse_diff(
                base_floats, speaker_floats, num_elements,
                0.001f, // 스파스 임계값
                &sparse_diff, &sparse_size
            );

            if (result == LEF_SUCCESS && sparse_size < base_header->data_size) {
                // 스파스 차분이 더 효율적인 경우
                free(delta_data);
                diff->diff_data = sparse_diff;
                diff->diff_size = sparse_size;
                diff->diff_type = LEF_DIFF_SPARSE_MASK;
            } else {
                // 일반 델타 차분 사용
                if (sparse_diff) free(sparse_diff);
                diff->diff_data = delta_data;
                diff->diff_size = base_header->data_size;
                diff->diff_type = LEF_DIFF_WEIGHT_DELTA;
            }
        } else {
            // 일반 델타 차분 사용
            diff->diff_data = delta_data;
            diff->diff_size = base_header->data_size;
            diff->diff_type = LEF_DIFF_WEIGHT_DELTA;
        }

        diff->compression_ratio = (float)diff->diff_size / (float)diff->original_size;
        ctx->layers_compressed++;
    }

    // 통계 업데이트
    ctx->total_original_size += diff->original_size;
    ctx->total_diff_size += diff->diff_size;
    ctx->num_diffs++;

    return LEF_SUCCESS;
}

/**
 * 전체 모델 차분 분석 수행
 * @param ctx 차분 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_analyze_model_differences(LEFDiffContext* ctx) {
    if (!ctx || !ctx->base_model || !ctx->speaker_model) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 모든 레이어에 대해 차분 생성
    for (size_t i = 0; i < ctx->base_model->num_layers; i++) {
        uint16_t base_layer_id = ctx->base_model->layer_headers[i].layer_id;
        uint16_t speaker_layer_id = ctx->speaker_model->layer_headers[i].layer_id;

        int result = lef_create_layer_diff(ctx, base_layer_id, speaker_layer_id);
        if (result != LEF_SUCCESS) {
            return result;
        }
    }

    // 평균 유사도 계산
    if (ctx->num_diffs > 0) {
        float total_similarity = 0.0f;
        for (size_t i = 0; i < ctx->num_diffs; i++) {
            total_similarity += ctx->layer_diffs[i].similarity_score;
        }
        ctx->average_similarity = total_similarity / (float)ctx->num_diffs;
    }

    return LEF_SUCCESS;
}

/**
 * 스파스 차분 생성 (가중치가 거의 0인 부분 제거)
 * @param base_data 기본 데이터
 * @param speaker_data 화자 데이터
 * @param data_size 데이터 크기 (요소 수)
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
                          size_t* sparse_size) {
    if (!base_data || !speaker_data || !sparse_diff || !sparse_size) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 유의미한 차분 요소 개수 계산
    size_t significant_count = 0;
    for (size_t i = 0; i < data_size; i++) {
        float diff = speaker_data[i] - base_data[i];
        if (fabs(diff) > sparsity_threshold) {
            significant_count++;
        }
    }

    // 스파스 포맷: [인덱스(4바이트), 값(4바이트)] 쌍들의 배열
    size_t sparse_data_size = significant_count * (sizeof(uint32_t) + sizeof(float));

    // 헤더 추가: [유의미한 요소 수(4바이트), 전체 크기(4바이트)]
    size_t total_sparse_size = sizeof(uint32_t) * 2 + sparse_data_size;

    // 스파스 압축이 효율적인지 확인
    if (total_sparse_size >= data_size * sizeof(float)) {
        return LEF_ERROR_COMPRESSION_FAILED; // 압축 효과 없음
    }

    uint8_t* sparse_buffer = (uint8_t*)malloc(total_sparse_size);
    if (!sparse_buffer) {
        return LEF_ERROR_OUT_OF_MEMORY;
    }

    // 헤더 작성
    uint32_t* header = (uint32_t*)sparse_buffer;
    header[0] = (uint32_t)significant_count;
    header[1] = (uint32_t)data_size;

    // 스파스 데이터 작성
    uint8_t* data_ptr = sparse_buffer + sizeof(uint32_t) * 2;
    uint32_t* indices = (uint32_t*)data_ptr;
    float* values = (float*)(data_ptr + significant_count * sizeof(uint32_t));

    size_t sparse_idx = 0;
    for (size_t i = 0; i < data_size; i++) {
        float diff = speaker_data[i] - base_data[i];
        if (fabs(diff) > sparsity_threshold) {
            indices[sparse_idx] = (uint32_t)i;
            values[sparse_idx] = diff;
            sparse_idx++;
        }
    }

    *sparse_diff = sparse_buffer;
    *sparse_size = total_sparse_size;

    return LEF_SUCCESS;
}

/**
 * 양자화된 차분 생성
 * @param base_data 기본 데이터
 * @param speaker_data 화자 데이터
 * @param data_size 데이터 크기 (요소 수)
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
                             size_t* quantized_size) {
    if (!base_data || !speaker_data || !quantized_diff || !quantized_size) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (quantization_bits < 1 || quantization_bits > 16) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 차분 계산 및 범위 찾기
    float* diffs = (float*)malloc(data_size * sizeof(float));
    if (!diffs) {
        return LEF_ERROR_OUT_OF_MEMORY;
    }

    float min_diff = FLT_MAX;
    float max_diff = -FLT_MAX;

    for (size_t i = 0; i < data_size; i++) {
        diffs[i] = speaker_data[i] - base_data[i];
        if (diffs[i] < min_diff) min_diff = diffs[i];
        if (diffs[i] > max_diff) max_diff = diffs[i];
    }

    // 양자화 파라미터 계산
    int32_t max_quant_value = (1 << quantization_bits) - 1;
    float scale = (max_diff - min_diff) / (float)max_quant_value;

    if (scale == 0.0f) {
        // 모든 차분이 동일한 경우
        free(diffs);
        return LEF_ERROR_COMPRESSION_FAILED;
    }

    // 양자화된 데이터 크기 계산
    size_t bytes_per_element = (quantization_bits + 7) / 8; // 비트를 바이트로 변환
    size_t quantized_data_size = data_size * bytes_per_element;

    // 헤더 크기: 스케일(4바이트) + 최소값(4바이트) + 비트수(1바이트) + 요소수(4바이트)
    size_t header_size = sizeof(float) * 2 + sizeof(uint8_t) + sizeof(uint32_t);
    size_t total_size = header_size + quantized_data_size;

    uint8_t* quantized_buffer = (uint8_t*)malloc(total_size);
    if (!quantized_buffer) {
        free(diffs);
        return LEF_ERROR_OUT_OF_MEMORY;
    }

    // 헤더 작성
    uint8_t* ptr = quantized_buffer;
    *(float*)ptr = scale; ptr += sizeof(float);
    *(float*)ptr = min_diff; ptr += sizeof(float);
    *(uint8_t*)ptr = (uint8_t)quantization_bits; ptr += sizeof(uint8_t);
    *(uint32_t*)ptr = (uint32_t)data_size; ptr += sizeof(uint32_t);

    // 양자화 및 패킹
    for (size_t i = 0; i < data_size; i++) {
        int32_t quantized_value = (int32_t)((diffs[i] - min_diff) / scale + 0.5f);
        if (quantized_value < 0) quantized_value = 0;
        if (quantized_value > max_quant_value) quantized_value = max_quant_value;

        // 비트 패킹 (간단한 구현 - 바이트 단위)
        if (quantization_bits <= 8) {
            ptr[i] = (uint8_t)quantized_value;
        } else if (quantization_bits <= 16) {
            ((uint16_t*)ptr)[i] = (uint16_t)quantized_value;
        }
    }

    free(diffs);

    *quantized_diff = quantized_buffer;
    *quantized_size = total_size;

    return LEF_SUCCESS;
}

/**
 * 유사도 기반 최적화 수행
 * @param ctx 차분 컨텍스트
 * @param optimization_level 최적화 레벨 (1-5)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lef_optimize_diff_model(LEFDiffContext* ctx, int optimization_level) {
    if (!ctx || optimization_level < 1 || optimization_level > 5) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 최적화 레벨에 따른 설정 조정
    switch (optimization_level) {
        case 1: // 기본 최적화
            ctx->similarity_threshold = 0.95f;
            ctx->enable_sparse_diff = false;
            ctx->enable_quantization = false;
            break;
        case 2: // 중간 최적화
            ctx->similarity_threshold = 0.90f;
            ctx->enable_sparse_diff = true;
            ctx->enable_quantization = false;
            break;
        case 3: // 고급 최적화
            ctx->similarity_threshold = 0.85f;
            ctx->enable_sparse_diff = true;
            ctx->enable_quantization = true;
            break;
        case 4: // 최대 압축
            ctx->similarity_threshold = 0.80f;
            ctx->enable_sparse_diff = true;
            ctx->enable_quantization = true;
            break;
        case 5: // 극한 압축
            ctx->similarity_threshold = 0.75f;
            ctx->enable_sparse_diff = true;
            ctx->enable_quantization = true;
            break;
    }

    // 기존 차분 데이터 정리
    for (size_t i = 0; i < ctx->num_diffs; i++) {
        if (ctx->layer_diffs[i].diff_data && ctx->layer_diffs[i].diff_size > 0) {
            free(ctx->layer_diffs[i].diff_data);
            ctx->layer_diffs[i].diff_data = NULL;
        }
    }

    // 통계 초기화
    ctx->num_diffs = 0;
    ctx->total_original_size = 0;
    ctx->total_diff_size = 0;
    ctx->layers_compressed = 0;
    ctx->layers_skipped = 0;

    // 재분석 수행
    return lef_analyze_model_differences(ctx);
}

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
                      float* avg_similarity) {
    if (!ctx) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (total_savings) {
        *total_savings = ctx->total_original_size - ctx->total_diff_size;
    }

    if (compression_ratio) {
        if (ctx->total_original_size > 0) {
            *compression_ratio = (float)ctx->total_diff_size / (float)ctx->total_original_size;
        } else {
            *compression_ratio = 1.0f;
        }
    }

    if (avg_similarity) {
        *avg_similarity = ctx->average_similarity;
    }

    return LEF_SUCCESS;
}

/**
 * 차분 모델 정보 출력
 * @param ctx 차분 컨텍스트
 */
void lef_print_diff_info(const LEFDiffContext* ctx) {
    if (!ctx) {
        printf("차분 컨텍스트가 NULL입니다.\n");
        return;
    }

    printf("=== 차분 모델 정보 ===\n");
    printf("총 레이어 수: %zu\n", ctx->num_diffs);
    printf("압축된 레이어: %d\n", ctx->layers_compressed);
    printf("건너뛴 레이어: %d (유사도 %.2f%% 이상)\n",
           ctx->layers_skipped, ctx->similarity_threshold * 100.0f);

    printf("\n=== 크기 정보 ===\n");
    printf("원본 크기: %zu 바이트 (%.2f MB)\n",
           ctx->total_original_size, ctx->total_original_size / (1024.0f * 1024.0f));
    printf("차분 크기: %zu 바이트 (%.2f MB)\n",
           ctx->total_diff_size, ctx->total_diff_size / (1024.0f * 1024.0f));

    size_t savings = ctx->total_original_size - ctx->total_diff_size;
    float compression_ratio = ctx->total_original_size > 0 ?
        (float)ctx->total_diff_size / (float)ctx->total_original_size : 1.0f;

    printf("절약된 크기: %zu 바이트 (%.2f MB)\n",
           savings, savings / (1024.0f * 1024.0f));
    printf("압축 비율: %.2f%% (%.2fx 압축)\n",
           compression_ratio * 100.0f, 1.0f / compression_ratio);

    printf("\n=== 유사도 정보 ===\n");
    printf("평균 유사도: %.2f%%\n", ctx->average_similarity * 100.0f);

    // 레이어별 상세 정보
    printf("\n=== 레이어별 상세 정보 ===\n");
    for (size_t i = 0; i < ctx->num_diffs && i < 10; i++) { // 최대 10개만 출력
        const LEFLayerDiff* diff = &ctx->layer_diffs[i];
        printf("레이어 %d: 유사도 %.2f%%, 원본 %zu -> 차분 %zu (%.2fx)\n",
               diff->base_layer_id,
               diff->similarity_score * 100.0f,
               diff->original_size,
               diff->diff_size,
               diff->original_size > 0 ? (float)diff->original_size / (float)diff->diff_size : 1.0f);
    }

    if (ctx->num_diffs > 10) {
        printf("... (총 %zu개 레이어 중 10개만 표시)\n", ctx->num_diffs);
    }
}

// ============================================================================
// LEFX 확장 모델 로더 및 적용 시스템 구현
// ============================================================================

/**
 * LEFX 확장 모델 로드
 * @param path 확장 모델 파일 경로
 * @return 로드된 확장 모델 포인터 (실패 시 NULL)
 */
LEFXModel* lefx_load_extension(const char* path) {
    if (!path) {
        return NULL;
    }

    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    // 파일 크기 확인
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < (long)sizeof(LEFXHeader)) {
        fclose(file);
        return NULL;
    }

    // 전체 파일을 메모리로 로드
    void* file_data = malloc(file_size);
    if (!file_data) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(file_data, 1, file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(file_data);
        return NULL;
    }

    // 메모리에서 확장 모델 로드
    LEFXModel* extension = lefx_load_extension_from_memory(file_data, file_size);
    if (!extension) {
        free(file_data);
        return NULL;
    }

    // 파일 경로 저장
    extension->file_path = (char*)malloc(strlen(path) + 1);
    if (extension->file_path) {
        strcpy(extension->file_path, path);
    }

    // 메모리 소유권 설정
    extension->file_data = file_data;
    extension->file_size = file_size;
    extension->owns_memory = true;
    extension->memory_mapped = false;

    return extension;
}

/**
 * 메모리에서 LEFX 확장 모델 로드
 * @param data 확장 모델 데이터 포인터
 * @param size 데이터 크기
 * @return 로드된 확장 모델 포인터 (실패 시 NULL)
 */
LEFXModel* lefx_load_extension_from_memory(const void* data, size_t size) {
    if (!data || size < sizeof(LEFXHeader)) {
        return NULL;
    }

    const uint8_t* buffer = (const uint8_t*)data;

    // LEFX 헤더 읽기
    const LEFXHeader* header = (const LEFXHeader*)buffer;

    // 헤더 검증
    if (!lefx_validate_header(header)) {
        return NULL;
    }

    // 파일 크기 검증
    if (header->file_size != size) {
        return NULL;
    }

    // LEFX 모델 구조체 생성
    LEFXModel* extension = (LEFXModel*)malloc(sizeof(LEFXModel));
    if (!extension) {
        return NULL;
    }

    memset(extension, 0, sizeof(LEFXModel));

    // 헤더 복사
    memcpy(&extension->header, header, sizeof(LEFXHeader));

    // 확장 메타데이터 로드
    if (header->meta_offset > 0 && header->meta_offset < size) {
        const LEFXExtensionMeta* meta = (const LEFXExtensionMeta*)(buffer + header->meta_offset);
        if (!lefx_validate_extension_meta(meta)) {
            free(extension);
            return NULL;
        }
        memcpy(&extension->meta, meta, sizeof(LEFXExtensionMeta));
        extension->num_layers = meta->num_layers;
    }

    // 의존성 정보 로드
    if (header->dependency_offset > 0 && header->dependency_offset < size) {
        // 의존성 수는 메타데이터에서 추정 (간단한 구현)
        size_t dependency_section_size = header->layer_index_offset - header->dependency_offset;
        extension->num_dependencies = dependency_section_size / sizeof(LEFXDependency);

        if (extension->num_dependencies > 0) {
            extension->dependencies = (LEFXDependency*)malloc(
                extension->num_dependencies * sizeof(LEFXDependency)
            );
            if (!extension->dependencies) {
                free(extension);
                return NULL;
            }

            const LEFXDependency* deps = (const LEFXDependency*)(buffer + header->dependency_offset);
            for (size_t i = 0; i < extension->num_dependencies; i++) {
                if (!lefx_validate_dependency(&deps[i])) {
                    free(extension->dependencies);
                    free(extension);
                    return NULL;
                }
                memcpy(&extension->dependencies[i], &deps[i], sizeof(LEFXDependency));
            }
        }
    }

    // 레이어 헤더 로드
    if (extension->num_layers > 0) {
        extension->layer_headers = (LEFXLayerHeader*)malloc(
            extension->num_layers * sizeof(LEFXLayerHeader)
        );
        extension->layer_data = (void**)calloc(extension->num_layers, sizeof(void*));

        if (!extension->layer_headers || !extension->layer_data) {
            free(extension->dependencies);
            free(extension->layer_headers);
            free(extension->layer_data);
            free(extension);
            return NULL;
        }

        // 레이어 인덱스에서 헤더 정보 로드
        if (header->layer_index_offset > 0 && header->layer_index_offset < size) {
            const LEFXLayerHeader* layer_headers = (const LEFXLayerHeader*)(buffer + header->layer_index_offset);

            for (size_t i = 0; i < extension->num_layers; i++) {
                if (!lefx_validate_layer_header(&layer_headers[i])) {
                    lefx_unload_extension(extension);
                    return NULL;
                }
                memcpy(&extension->layer_headers[i], &layer_headers[i], sizeof(LEFXLayerHeader));

                // 레이어 데이터 포인터 설정 (실제 로딩은 나중에)
                if (layer_headers[i].data_offset > 0 &&
                    layer_headers[i].data_offset + layer_headers[i].data_size <= size) {
                    extension->layer_data[i] = (void*)(buffer + layer_headers[i].data_offset);
                }
            }
        }
    }

    // 활성화 규칙 로드 (간단한 구현 - 고정 개수 가정)
    extension->num_activation_rules = 0; // 실제 구현에서는 헤더에서 읽어야 함

    // 플러그인 데이터 로드
    if (header->plugin_data_offset > 0 && header->plugin_data_offset < size) {
        extension->plugin_data = (LEFXPluginData*)malloc(sizeof(LEFXPluginData));
        if (extension->plugin_data) {
            const LEFXPluginData* plugin = (const LEFXPluginData*)(buffer + header->plugin_data_offset);
            memcpy(extension->plugin_data, plugin, sizeof(LEFXPluginData));
        }
    }

    // 런타임 상태 초기화
    extension->is_active = false;
    extension->current_weight = 0.0f;
    extension->runtime_context = NULL;
    extension->base_model = NULL;
    extension->base_model_owned = false;

    // 파일 크기 설정
    extension->file_size = size;
    extension->owns_memory = false; // 메모리에서 로드된 경우 소유권 없음
    extension->memory_mapped = false;

    return extension;
}

/**
 * LEFX 확장 모델 언로드 및 메모리 해제
 * @param extension 해제할 확장 모델 포인터
 */
void lefx_unload_extension(LEFXModel* extension) {
    if (!extension) return;

    // 기본 모델 해제 (소유권이 있는 경우)
    if (extension->base_model_owned && extension->base_model) {
        lef_unload_model(extension->base_model);
    }

    // 레이어 데이터 해제 (메모리 소유권이 있는 경우만)
    if (extension->owns_memory && extension->layer_data) {
        // 메모리 매핑된 경우 개별 레이어 데이터는 해제하지 않음
        if (!extension->memory_mapped) {
            for (size_t i = 0; i < extension->num_layers; i++) {
                // 실제 구현에서는 개별 할당된 레이어만 해제
                // 현재는 파일 데이터 내 포인터이므로 해제하지 않음
            }
        }
        free(extension->layer_data);
    }

    // 구조체 배열들 해제
    free(extension->layer_headers);
    free(extension->dependencies);
    free(extension->activation_rules);
    free(extension->plugin_data);

    // 파일 데이터 해제
    if (extension->owns_memory && extension->file_data) {
        free(extension->file_data);
    }

    // 파일 경로 해제
    free(extension->file_path);

    // 파일 핸들 닫기
    if (extension->file_handle) {
        fclose(extension->file_handle);
    }

    // 런타임 컨텍스트 해제
    if (extension->runtime_context) {
        // 플러그인별 정리 함수 호출 필요
        free(extension->runtime_context);
    }

    free(extension);
}

/**
 * 기본 모델과 확장 모델 호환성 검증
 * @param base_model 기본 모델 포인터
 * @param extension 확장 모델 포인터
 * @return 호환 시 true, 비호환 시 false
 */
bool lefx_check_compatibility(const LEFModel* base_model, const LEFXModel* extension) {
    if (!base_model || !extension) {
        return false;
    }

    // 기본 모델 해시 검증
    uint32_t base_hash = lef_calculate_model_hash(&base_model->meta);
    if (extension->header.base_model_hash != 0 &&
        extension->header.base_model_hash != base_hash) {
        return false;
    }

    // 모델 이름 검증
    if (strlen(extension->header.base_model_name) > 0) {
        if (strcmp(base_model->meta.model_name, extension->header.base_model_name) != 0) {
            return false;
        }
    }

    // 버전 호환성 검증
    uint16_t base_major = base_model->header.version_major;
    uint16_t base_minor = base_model->header.version_minor;

    if (base_major < extension->meta.min_base_version_major ||
        (base_major == extension->meta.min_base_version_major &&
         base_minor < extension->meta.min_base_version_minor)) {
        return false; // 기본 모델 버전이 너무 낮음
    }

    if (base_major > extension->meta.max_base_version_major ||
        (base_major == extension->meta.max_base_version_major &&
         base_minor > extension->meta.max_base_version_minor)) {
        return false; // 기본 모델 버전이 너무 높음
    }

    // 아키텍처 호환성 검증 (기본적인 차원 확인)
    if (extension->header.extension_type == LEFX_EXT_SPEAKER ||
        extension->header.extension_type == LEFX_EXT_LANGUAGE) {
        // 음성 합성 관련 확장의 경우 샘플 레이트 등 확인
        if (base_model->meta.sample_rate != 0 && extension->meta.inference_time_ms != 0) {
            // 기본적인 호환성 확인 (실제로는 더 복잡한 검증 필요)
        }
    }

    // 필요한 기본 모델 크기 검증
    if (extension->header.required_base_size > 0) {
        size_t base_model_size = 0;
        lef_get_model_stats(base_model, NULL, &base_model_size);
        if (base_model_size < extension->header.required_base_size) {
            return false;
        }
    }

    return true;
}

/**
 * 확장 모델을 기본 모델에 적용
 * @param base_model 기본 모델 포인터
 * @param extension 확장 모델 포인터
 * @param blend_weight 블렌딩 가중치 (0.0-1.0)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_apply_extension(LEFModel* base_model, LEFXModel* extension, float blend_weight) {
    if (!base_model || !extension) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 블렌딩 가중치 범위 검증
    if (blend_weight < 0.0f || blend_weight > 1.0f) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 호환성 검증
    if (!lefx_check_compatibility(base_model, extension)) {
        return LEF_ERROR_VERSION_INCOMPATIBLE;
    }

    // 의존성 검증 (간단한 구현)
    if (extension->num_dependencies > 0) {
        // 실제 구현에서는 다른 확장들과의 의존성 확인 필요
        // 현재는 기본적인 검증만 수행
        for (size_t i = 0; i < extension->num_dependencies; i++) {
            if (extension->dependencies[i].dependency_type == 0) { // 필수 의존성
                // 필수 의존성이 만족되지 않으면 실패
                // 실제로는 로드된 다른 확장들을 확인해야 함
            }
        }
    }

    // 확장 레이어들을 기본 모델에 적용
    for (size_t i = 0; i < extension->num_layers; i++) {
        const LEFXLayerHeader* ext_layer = &extension->layer_headers[i];
        void* ext_data = extension->layer_data[i];

        if (!ext_data) {
            continue; // 데이터가 없는 레이어는 건너뛰기
        }

        // 차분 모델인 경우 기본 레이어와 블렌딩
        if (ext_layer->base_layer_id != 0xFFFF) { // 유효한 기본 레이어 ID
            void* base_data = lef_get_layer_data(base_model, ext_layer->base_layer_id);
            const LEFLayerHeader* base_layer = lef_get_layer_header(base_model, ext_layer->base_layer_id);

            if (base_data && base_layer) {
                // 레이어 크기 확인
                if (base_layer->data_size != ext_layer->data_size) {
                    continue; // 크기가 다르면 건너뛰기
                }

                // 블렌딩 모드에 따른 적용
                int result = lefx_apply_layer_blending(
                    base_data, ext_data, base_layer->data_size,
                    ext_layer->blend_mode, blend_weight * ext_layer->blend_weight
                );

                if (result != LEF_SUCCESS) {
                    return result;
                }
            }
        } else {
            // 새로운 레이어 추가 (현재 구현에서는 지원하지 않음)
            // 실제 구현에서는 동적 레이어 추가 지원 필요
        }
    }

    // 확장 상태 업데이트
    extension->is_active = true;
    extension->current_weight = blend_weight;
    extension->base_model = base_model;

    return LEF_SUCCESS;
}

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
                             size_t data_size, uint8_t blend_mode, float weight) {
    if (!base_data || !ext_data || data_size == 0) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    float* base_floats = (float*)base_data;
    const float* ext_floats = (const float*)ext_data;
    size_t num_elements = data_size / sizeof(float);

    switch (blend_mode) {
        case 0: // 교체 모드
            for (size_t i = 0; i < num_elements; i++) {
                base_floats[i] = base_floats[i] * (1.0f - weight) + ext_floats[i] * weight;
            }
            break;

        case 1: // 덧셈 모드
            for (size_t i = 0; i < num_elements; i++) {
                base_floats[i] += ext_floats[i] * weight;
            }
            break;

        case 2: // 곱셈 모드
            for (size_t i = 0; i < num_elements; i++) {
                base_floats[i] *= (1.0f + ext_floats[i] * weight);
            }
            break;

        case 3: // 보간 모드 (선형 보간)
            for (size_t i = 0; i < num_elements; i++) {
                base_floats[i] = base_floats[i] * (1.0f - weight) + ext_floats[i] * weight;
            }
            break;

        default:
            return LEF_ERROR_INVALID_ARGUMENT;
    }

    return LEF_SUCCESS;
}

/**
 * 확장 모델 비활성화
 * @param base_model 기본 모델 포인터
 * @param extension 확장 모델 포인터
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_deactivate_extension(LEFModel* base_model, LEFXModel* extension) {
    if (!base_model || !extension) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (!extension->is_active) {
        return LEF_SUCCESS; // 이미 비활성화됨
    }

    // 확장 효과를 되돌리기 위해 역방향 블렌딩 적용
    // 실제 구현에서는 원본 데이터를 백업해두거나 역연산을 수행해야 함
    // 현재는 간단한 구현으로 가중치를 0으로 재적용
    int result = lefx_apply_extension(base_model, extension, 0.0f);
    if (result != LEF_SUCCESS) {
        return result;
    }

    // 확장 상태 업데이트
    extension->is_active = false;
    extension->current_weight = 0.0f;

    return LEF_SUCCESS;
}

/**
 * 확장 레이어 데이터 가져오기
 * @param extension 확장 모델 포인터
 * @param layer_id 확장 레이어 ID
 * @return 레이어 데이터 포인터 (실패 시 NULL)
 */
void* lefx_get_layer_data(LEFXModel* extension, uint16_t layer_id) {
    if (!extension || !extension->layer_data) {
        return NULL;
    }

    // 레이어 ID로 검색
    for (size_t i = 0; i < extension->num_layers; i++) {
        if (extension->layer_headers[i].extension_layer_id == layer_id) {
            return extension->layer_data[i];
        }
    }

    return NULL;
}

/**
 * 확장 레이어 헤더 가져오기
 * @param extension 확장 모델 포인터
 * @param layer_id 확장 레이어 ID
 * @return 레이어 헤더 포인터 (실패 시 NULL)
 */
const LEFXLayerHeader* lefx_get_layer_header(LEFXModel* extension, uint16_t layer_id) {
    if (!extension || !extension->layer_headers) {
        return NULL;
    }

    // 레이어 ID로 검색
    for (size_t i = 0; i < extension->num_layers; i++) {
        if (extension->layer_headers[i].extension_layer_id == layer_id) {
            return &extension->layer_headers[i];
        }
    }

    return NULL;
}

/**
 * 의존성 검증
 * @param extension 확장 모델 포인터
 * @param available_extensions 사용 가능한 확장 모델 배열
 * @param num_available 사용 가능한 확장 수
 * @return 의존성 만족 시 true, 불만족 시 false
 */
bool lefx_check_dependencies(const LEFXModel* extension,
                            const LEFXModel** available_extensions,
                            size_t num_available) {
    if (!extension) {
        return false;
    }

    if (extension->num_dependencies == 0) {
        return true; // 의존성이 없으면 항상 만족
    }

    if (!extension->dependencies) {
        return false;
    }

    // 각 의존성 확인
    for (size_t i = 0; i < extension->num_dependencies; i++) {
        const LEFXDependency* dep = &extension->dependencies[i];
        bool dependency_satisfied = false;

        // 필수 의존성인 경우 반드시 만족되어야 함
        if (dep->dependency_type == 0) { // 필수 의존성
            // 사용 가능한 확장들 중에서 해당 의존성을 만족하는지 확인
            for (size_t j = 0; j < num_available; j++) {
                const LEFXModel* available = available_extensions[j];
                if (!available) continue;

                // 이름 매칭
                if (strcmp(available->header.extension_name, dep->dependency_name) == 0) {
                    // 버전 호환성 확인 (간단한 문자열 비교)
                    if (strcmp(available->header.extension_version, dep->min_version) >= 0 &&
                        strcmp(available->header.extension_version, dep->max_version) <= 0) {
                        dependency_satisfied = true;
                        break;
                    }
                }
            }

            if (!dependency_satisfied) {
                return false; // 필수 의존성이 만족되지 않음
            }
        }
        // 선택적 의존성(1)과 충돌(2)은 현재 구현에서 무시
    }

    return true;
}

/**
 * 조건부 활성화 평가
 * @param extension 확장 모델 포인터
 * @param context_data 컨텍스트 데이터
 * @param context_size 컨텍스트 크기
 * @return 활성화 가중치 (0.0-1.0)
 */
float lefx_evaluate_activation_conditions(const LEFXModel* extension,
                                         const void* context_data,
                                         size_t context_size) {
    if (!extension || extension->num_activation_rules == 0) {
        return 1.0f; // 조건이 없으면 완전 활성화
    }

    if (!extension->activation_rules) {
        return 1.0f;
    }

    float total_weight = 0.0f;
    float max_weight = 0.0f;
    int active_rules = 0;

    // 각 활성화 규칙 평가
    for (size_t i = 0; i < extension->num_activation_rules; i++) {
        const LEFXActivationRule* rule = &extension->activation_rules[i];
        bool condition_met = false;

        // 컨텍스트 데이터 타입에 따른 조건 평가
        switch (rule->condition_type) {
            case 0: // 텍스트 조건
                if (context_data && context_size > 0) {
                    const char* text = (const char*)context_data;
                    switch (rule->operator_type) {
                        case 0: // 같음
                            condition_met = (strcmp(text, rule->condition_value) == 0);
                            break;
                        case 1: // 포함
                            condition_met = (strstr(text, rule->condition_value) != NULL);
                            break;
                        // 다른 연산자들은 현재 구현에서 생략
                    }
                }
                break;

            case 1: // 화자 조건
            case 2: // 언어 조건
            case 3: // 시간 조건
            case 4: // 사용자 정의 조건
                // 현재 구현에서는 기본적인 처리만 수행
                condition_met = true; // 임시로 항상 만족으로 처리
                break;
        }

        if (condition_met) {
            total_weight += rule->activation_weight;
            if (rule->activation_weight > max_weight) {
                max_weight = rule->activation_weight;
            }
            active_rules++;
        }
    }

    // 활성화된 규칙이 없으면 비활성화
    if (active_rules == 0) {
        return 0.0f;
    }

    // 평균 가중치 반환 (또는 최대값, 정책에 따라)
    return total_weight / (float)active_rules;
}

/**
 * 확장 모델 정보 출력
 * @param extension 확장 모델 포인터
 */
void lefx_print_extension_info(const LEFXModel* extension) {
    if (!extension) {
        printf("확장 모델이 NULL입니다.\n");
        return;
    }

    printf("=== LEFX 확장 모델 정보 ===\n");
    printf("확장 이름: %s\n", extension->header.extension_name);
    printf("확장 버전: %s\n", extension->header.extension_version);
    printf("제작자: %s\n", extension->header.extension_author);
    printf("확장 타입: %u\n", extension->header.extension_type);
    printf("확장 ID: %u\n", extension->header.extension_id);

    printf("\n=== 기본 모델 호환성 ===\n");
    printf("기본 모델 이름: %s\n", extension->header.base_model_name);
    printf("기본 모델 버전: %s\n", extension->header.base_model_version);
    printf("기본 모델 해시: 0x%08X\n", extension->header.base_model_hash);

    printf("\n=== 확장 특성 ===\n");
    printf("레이어 수: %zu\n", extension->num_layers);
    printf("의존성 수: %zu\n", extension->num_dependencies);
    printf("활성화 규칙 수: %zu\n", extension->num_activation_rules);
    printf("우선순위: %u\n", extension->meta.priority);
    printf("품질 점수: %.2f\n", extension->meta.quality_score);
    printf("성능 영향도: %.2f\n", extension->meta.performance_impact);

    if (extension->meta.gender != 255) {
        const char* gender_str[] = {"남성", "여성", "중성"};
        printf("성별: %s\n", gender_str[extension->meta.gender]);
    }

    if (extension->meta.age_range != 255) {
        const char* age_str[] = {"어린이", "청년", "중년", "노년"};
        printf("연령대: %s\n", age_str[extension->meta.age_range]);
    }

    printf("\n=== 런타임 상태 ===\n");
    printf("활성화 상태: %s\n", extension->is_active ? "활성화" : "비활성화");
    printf("현재 가중치: %.2f\n", extension->current_weight);

    // 의존성 정보 출력
    if (extension->num_dependencies > 0 && extension->dependencies) {
        printf("\n=== 의존성 정보 ===\n");
        for (size_t i = 0; i < extension->num_dependencies; i++) {
            const LEFXDependency* dep = &extension->dependencies[i];
            const char* dep_type_str[] = {"필수", "선택", "충돌"};
            printf("의존성 %zu: %s (%s, %s-%s)\n",
                   i + 1, dep->dependency_name,
                   dep_type_str[dep->dependency_type],
                   dep->min_version, dep->max_version);
        }
    }

    // 레이어 정보 출력 (간략하게)
    if (extension->num_layers > 0 && extension->layer_headers) {
        printf("\n=== 레이어 정보 ===\n");
        for (size_t i = 0; i < extension->num_layers && i < 5; i++) { // 최대 5개만 출력
            const LEFXLayerHeader* layer = &extension->layer_headers[i];
            printf("레이어 %u: 타입 %u, 크기 %u, 블렌딩 모드 %u\n",
                   layer->extension_layer_id, layer->layer_kind,
                   layer->data_size, layer->blend_mode);
        }
        if (extension->num_layers > 5) {
            printf("... (총 %zu개 레이어 중 5개만 표시)\n", extension->num_layers);
        }
    }
}

/**
 * 확장 모델 통계 정보 가져오기
 * @param extension 확장 모델 포인터
 * @param total_params 총 파라미터 수 (출력)
 * @param total_size 총 크기 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_get_extension_stats(const LEFXModel* extension, size_t* total_params, size_t* total_size) {
    if (!extension) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (total_params) {
        *total_params = extension->meta.total_params;
    }

    if (total_size) {
        *total_size = extension->file_size;
    }

    return LEF_SUCCESS;
}