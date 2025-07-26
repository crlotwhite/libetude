/**
 * @file test_diff_model.c
 * @brief 차분 모델 시스템 단위 테스트
 *
 * 기본 모델과 화자별 모델 간의 차분 생성, 유사도 계산, 압축 최적화 등을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "libetude/lef_format.h"

// 테스트 헬퍼 함수들
static void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

// 테스트용 모델 데이터 생성
static LEFModel* create_test_model(const char* model_name, int num_layers, int layer_size) {
    LEFModel* model = (LEFModel*)malloc(sizeof(LEFModel));
    if (!model) return NULL;

    memset(model, 0, sizeof(LEFModel));

    // 기본 헤더 설정
    lef_init_header(&model->header);
    lef_init_model_meta(&model->meta);
    strncpy(model->meta.model_name, model_name, sizeof(model->meta.model_name) - 1);

    // 레이어 설정
    model->num_layers = num_layers;
    model->layer_headers = (LEFLayerHeader*)calloc(num_layers, sizeof(LEFLayerHeader));
    model->layer_data = (void**)calloc(num_layers, sizeof(void*));
    model->layer_index = (LEFLayerIndexEntry*)calloc(num_layers, sizeof(LEFLayerIndexEntry));

    if (!model->layer_headers || !model->layer_data || !model->layer_index) {
        free(model->layer_headers);
        free(model->layer_data);
        free(model->layer_index);
        free(model);
        return NULL;
    }

    // 각 레이어 초기화
    for (int i = 0; i < num_layers; i++) {
        lef_init_layer_header(&model->layer_headers[i], i, LEF_LAYER_LINEAR);
        model->layer_headers[i].data_size = layer_size * sizeof(float);

        // 레이어 인덱스 설정
        model->layer_index[i].layer_id = i;
        model->layer_index[i].header_offset = 0;
        model->layer_index[i].data_offset = 0;
        model->layer_index[i].data_size = layer_size * sizeof(float);

        // 테스트 데이터 생성
        float* layer_data = (float*)calloc(layer_size, sizeof(float));
        if (!layer_data) {
            // 정리 코드
            for (int k = 0; k < i; k++) {
                free(model->layer_data[k]);
            }
            free(model->layer_headers);
            free(model->layer_data);
            free(model->layer_index);
            free(model);
            return NULL;
        }

        // 기본 모델과 화자 모델에 따라 다른 데이터 생성
        for (int j = 0; j < layer_size; j++) {
            if (strcmp(model_name, "base_model") == 0) {
                layer_data[j] = (float)(j % 100) / 100.0f; // 기본 패턴
            } else {
                // 화자 모델은 기본 모델에 약간의 변화 추가
                float base_value = (float)(j % 100) / 100.0f;
                float variation = sinf((float)j * 0.1f) * 0.1f; // 작은 변화
                layer_data[j] = base_value + variation;
            }
        }

        model->layer_data[i] = layer_data;
    }

    model->owns_memory = true;
    return model;
}

static void destroy_test_model(LEFModel* model) {
    if (!model) return;

    if (model->layer_data) {
        for (size_t i = 0; i < model->num_layers; i++) {
            free(model->layer_data[i]);
        }
        free(model->layer_data);
    }

    free(model->layer_headers);
    free(model->layer_index);
    free(model);
}

// 테스트 1: 차분 컨텍스트 생성 및 해제
static bool test_diff_context_creation() {
    print_test_header("차분 컨텍스트 생성 및 해제 테스트");

    LEFModel* base_model = create_test_model("base_model", 5, 1000);
    LEFModel* speaker_model = create_test_model("speaker_model", 5, 1000);

    if (!base_model || !speaker_model) {
        printf("테스트 모델 생성 실패\n");
        return false;
    }

    // 정상적인 컨텍스트 생성
    LEFDiffContext* ctx = lef_create_diff_context(base_model, speaker_model, 0.9f);
    bool success = (ctx != NULL);

    if (success) {
        // 컨텍스트 설정 확인
        success = (ctx->base_model == base_model) &&
                 (ctx->speaker_model == speaker_model) &&
                 (ctx->similarity_threshold == 0.9f) &&
                 (ctx->layer_diffs != NULL) &&
                 (ctx->diff_capacity == base_model->num_layers);
    }

    // 컨텍스트 해제
    lef_destroy_diff_context(ctx);

    // 잘못된 인자로 생성 시도
    LEFDiffContext* invalid_ctx1 = lef_create_diff_context(NULL, speaker_model, 0.9f);
    LEFDiffContext* invalid_ctx2 = lef_create_diff_context(base_model, NULL, 0.9f);
    LEFDiffContext* invalid_ctx3 = lef_create_diff_context(base_model, speaker_model, 1.5f);

    success = success && (invalid_ctx1 == NULL) && (invalid_ctx2 == NULL) && (invalid_ctx3 == NULL);

    destroy_test_model(base_model);
    destroy_test_model(speaker_model);

    print_test_result("차분 컨텍스트 생성 및 해제", success);
    return success;
}

// 테스트 2: 레이어 유사도 계산
static bool test_layer_similarity_calculation() {
    print_test_header("레이어 유사도 계산 테스트");

    const int data_size = 1000;
    float* identical_data1 = (float*)malloc(data_size * sizeof(float));
    float* identical_data2 = (float*)malloc(data_size * sizeof(float));
    float* different_data = (float*)malloc(data_size * sizeof(float));

    if (!identical_data1 || !identical_data2 || !different_data) {
        printf("메모리 할당 실패\n");
        return false;
    }

    // 동일한 데이터 생성
    for (int i = 0; i < data_size; i++) {
        identical_data1[i] = (float)i / 100.0f;
        identical_data2[i] = (float)i / 100.0f;
        different_data[i] = -(float)i / 100.0f; // 완전히 반대 데이터
    }

    // 동일한 데이터 간 유사도 (1.0에 가까워야 함)
    float similarity_identical = lef_calculate_layer_similarity(
        identical_data1, identical_data2, data_size * sizeof(float), LEF_LAYER_LINEAR
    );

    // 다른 데이터 간 유사도 (낮아야 함)
    float similarity_different = lef_calculate_layer_similarity(
        identical_data1, different_data, data_size * sizeof(float), LEF_LAYER_LINEAR
    );

    // 약간 다른 데이터 생성
    float* slightly_different = (float*)malloc(data_size * sizeof(float));
    for (int i = 0; i < data_size; i++) {
        slightly_different[i] = identical_data1[i] + 0.01f; // 작은 차이
    }

    float similarity_slight = lef_calculate_layer_similarity(
        identical_data1, slightly_different, data_size * sizeof(float), LEF_LAYER_LINEAR
    );

    bool success = (similarity_identical > 0.99f) &&
                   (similarity_different < 0.1f) &&  // 반대 데이터는 매우 낮은 유사도
                   (similarity_slight > 0.95f) &&
                   (similarity_slight < similarity_identical);

    printf("동일한 데이터 유사도: %.4f\n", similarity_identical);
    printf("다른 데이터 유사도: %.4f\n", similarity_different);
    printf("약간 다른 데이터 유사도: %.4f\n", similarity_slight);

    free(identical_data1);
    free(identical_data2);
    free(different_data);
    free(slightly_different);

    print_test_result("레이어 유사도 계산", success);
    return success;
}

// 테스트 3: 레이어 차분 생성
static bool test_layer_diff_creation() {
    print_test_header("레이어 차분 생성 테스트");

    LEFModel* base_model = create_test_model("base_model", 3, 500);
    LEFModel* speaker_model = create_test_model("speaker_model", 3, 500);

    if (!base_model || !speaker_model) {
        printf("테스트 모델 생성 실패\n");
        return false;
    }

    LEFDiffContext* ctx = lef_create_diff_context(base_model, speaker_model, 0.8f);
    if (!ctx) {
        printf("차분 컨텍스트 생성 실패\n");
        destroy_test_model(base_model);
        destroy_test_model(speaker_model);
        return false;
    }

    // 첫 번째 레이어 차분 생성
    int result = lef_create_layer_diff(ctx, 0, 0);
    bool success = (result == LEF_SUCCESS) && (ctx->num_diffs == 1);

    if (success) {
        LEFLayerDiff* diff = &ctx->layer_diffs[0];
        success = (diff->base_layer_id == 0) &&
                 (diff->speaker_layer_id == 0) &&
                 (diff->similarity_score >= 0.0f && diff->similarity_score <= 1.0f) &&
                 (diff->original_size == 500 * sizeof(float));

        printf("레이어 0 유사도: %.4f\n", diff->similarity_score);
        printf("원본 크기: %zu, 차분 크기: %zu\n", diff->original_size, diff->diff_size);
    }

    // 단순 테스트만 수행 (전체 분석은 건너뛰기)
    printf("단일 레이어 차분 테스트 완료\n");

    lef_destroy_diff_context(ctx);
    destroy_test_model(base_model);
    destroy_test_model(speaker_model);

    print_test_result("레이어 차분 생성", success);
    return success;
}

// 테스트 4: 스파스 차분 생성
static bool test_sparse_diff_creation() {
    print_test_header("스파스 차분 생성 테스트");

    const int data_size = 1000;
    float* base_data = (float*)malloc(data_size * sizeof(float));
    float* speaker_data = (float*)malloc(data_size * sizeof(float));

    if (!base_data || !speaker_data) {
        printf("메모리 할당 실패\n");
        return false;
    }

    // 스파스한 차분을 가진 데이터 생성 (대부분 동일, 일부만 다름)
    for (int i = 0; i < data_size; i++) {
        base_data[i] = (float)i / 100.0f;
        speaker_data[i] = base_data[i];

        // 10%의 요소만 다르게 설정
        if (i % 10 == 0) {
            speaker_data[i] += 0.1f;
        }
    }

    void* sparse_diff = NULL;
    size_t sparse_size = 0;

    int result = lef_create_sparse_diff(base_data, speaker_data, data_size, 0.05f,
                                       &sparse_diff, &sparse_size);

    bool success = (result == LEF_SUCCESS) && (sparse_diff != NULL) &&
                   (sparse_size < data_size * sizeof(float));

    if (success) {
        float compression_ratio = (float)sparse_size / (float)(data_size * sizeof(float));
        printf("스파스 압축 비율: %.2f%% (%.2fx 압축)\n",
               compression_ratio * 100.0f, 1.0f / compression_ratio);

        // 스파스 데이터 검증
        uint32_t* header = (uint32_t*)sparse_diff;
        uint32_t significant_count = header[0];
        uint32_t total_size = header[1];

        success = (significant_count == 100) && (total_size == data_size); // 10%가 다름
        printf("유의미한 요소 수: %u / %u\n", significant_count, total_size);
    }

    free(base_data);
    free(speaker_data);
    if (sparse_diff) free(sparse_diff);

    print_test_result("스파스 차분 생성", success);
    return success;
}

// 테스트 5: 양자화된 차분 생성
static bool test_quantized_diff_creation() {
    print_test_header("양자화된 차분 생성 테스트");

    const int data_size = 500;
    float* base_data = (float*)malloc(data_size * sizeof(float));
    float* speaker_data = (float*)malloc(data_size * sizeof(float));

    if (!base_data || !speaker_data) {
        printf("메모리 할당 실패\n");
        return false;
    }

    // 테스트 데이터 생성
    for (int i = 0; i < data_size; i++) {
        base_data[i] = (float)i / 100.0f;
        speaker_data[i] = base_data[i] + sin((float)i * 0.1f) * 0.1f; // 작은 변화
    }

    void* quantized_diff = NULL;
    size_t quantized_size = 0;

    // 8비트 양자화 테스트
    int result = lef_create_quantized_diff(base_data, speaker_data, data_size, 8,
                                          &quantized_diff, &quantized_size);

    bool success = (result == LEF_SUCCESS) && (quantized_diff != NULL) &&
                   (quantized_size < data_size * sizeof(float));

    if (success) {
        float compression_ratio = (float)quantized_size / (float)(data_size * sizeof(float));
        printf("8비트 양자화 압축 비율: %.2f%% (%.2fx 압축)\n",
               compression_ratio * 100.0f, 1.0f / compression_ratio);

        // 예상 크기 검증 (헤더 + 8비트 데이터)
        size_t expected_size = sizeof(float) * 2 + sizeof(uint8_t) + sizeof(uint32_t) + data_size;
        success = (quantized_size == expected_size);
    }

    free(base_data);
    free(speaker_data);
    if (quantized_diff) free(quantized_diff);

    print_test_result("양자화된 차분 생성", success);
    return success;
}

// 테스트 6: 유사도 기반 최적화
static bool test_similarity_optimization() {
    print_test_header("유사도 기반 최적화 테스트");

    LEFModel* base_model = create_test_model("base_model", 5, 1000);
    LEFModel* speaker_model = create_test_model("speaker_model", 5, 1000);

    if (!base_model || !speaker_model) {
        printf("테스트 모델 생성 실패\n");
        return false;
    }

    LEFDiffContext* ctx = lef_create_diff_context(base_model, speaker_model, 0.9f);
    if (!ctx) {
        printf("차분 컨텍스트 생성 실패\n");
        destroy_test_model(base_model);
        destroy_test_model(speaker_model);
        return false;
    }

    // 기본 분석
    int result = lef_analyze_model_differences(ctx);
    bool success = (result == LEF_SUCCESS);

    size_t original_savings = 0;
    float original_compression = 0.0f;
    float original_similarity = 0.0f;

    if (success) {
        lef_get_diff_stats(ctx, &original_savings, &original_compression, &original_similarity);
        printf("기본 설정 - 절약: %zu 바이트, 압축률: %.2f%%, 유사도: %.2f%%\n",
               original_savings, original_compression * 100.0f, original_similarity * 100.0f);
    }

    // 최적화 레벨 3 적용
    result = lef_optimize_diff_model(ctx, 3);
    success = success && (result == LEF_SUCCESS);

    if (success) {
        size_t optimized_savings = 0;
        float optimized_compression = 0.0f;
        float optimized_similarity = 0.0f;

        lef_get_diff_stats(ctx, &optimized_savings, &optimized_compression, &optimized_similarity);
        printf("최적화 후 - 절약: %zu 바이트, 압축률: %.2f%%, 유사도: %.2f%%\n",
               optimized_savings, optimized_compression * 100.0f, optimized_similarity * 100.0f);

        // 최적화로 인해 압축률이 개선되었는지 확인
        success = (optimized_compression <= original_compression);
    }

    lef_destroy_diff_context(ctx);
    destroy_test_model(base_model);
    destroy_test_model(speaker_model);

    print_test_result("유사도 기반 최적화", success);
    return success;
}

// 테스트 7: 차분 정보 출력
static bool test_diff_info_printing() {
    print_test_header("차분 정보 출력 테스트");

    LEFModel* base_model = create_test_model("base_model", 3, 500);
    LEFModel* speaker_model = create_test_model("speaker_model", 3, 500);

    if (!base_model || !speaker_model) {
        printf("테스트 모델 생성 실패\n");
        return false;
    }

    LEFDiffContext* ctx = lef_create_diff_context(base_model, speaker_model, 0.8f);
    if (!ctx) {
        printf("차분 컨텍스트 생성 실패\n");
        destroy_test_model(base_model);
        destroy_test_model(speaker_model);
        return false;
    }

    int result = lef_analyze_model_differences(ctx);
    bool success = (result == LEF_SUCCESS);

    if (success) {
        // 차분 정보 출력 (크래시 없이 실행되는지 확인)
        lef_print_diff_info(ctx);
        printf("\n차분 정보 출력 완료\n");
    }

    lef_destroy_diff_context(ctx);
    destroy_test_model(base_model);
    destroy_test_model(speaker_model);

    print_test_result("차분 정보 출력", success);
    return success;
}

// 메인 테스트 실행 함수
int main() {
    printf("=== LibEtude 차분 모델 시스템 단위 테스트 ===\n");

    bool all_passed = true;

    all_passed &= test_diff_context_creation();
    all_passed &= test_layer_similarity_calculation();
    all_passed &= test_layer_diff_creation();
    all_passed &= test_sparse_diff_creation();
    all_passed &= test_quantized_diff_creation();
    all_passed &= test_similarity_optimization();
    all_passed &= test_diff_info_printing();

    printf("\n=== 테스트 결과 요약 ===\n");
    printf("전체 테스트: %s\n", all_passed ? "통과" : "실패");

    return all_passed ? 0 : 1;
}