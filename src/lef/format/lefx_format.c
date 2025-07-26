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