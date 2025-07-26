/**
 * @file test_lefx_loader.c
 * @brief LEFX 확장 모델 로더 및 적용 시스템 단위 테스트
 *
 * 확장 모델 로딩, 호환성 검증, 동적 적용 등을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "libetude/lef_format.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;

// 테스트 매크로
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s\n", message); \
    } \
} while(0)

// 테스트 헬퍼 함수들
static void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

// 테스트용 기본 모델 생성
static LEFModel* create_test_base_model() {
    LEFModel* model = (LEFModel*)malloc(sizeof(LEFModel));
    if (!model) return NULL;

    memset(model, 0, sizeof(LEFModel));

    // 헤더 초기화
    lef_init_header(&model->header);
    model->header.version_major = 1;
    model->header.version_minor = 0;

    // 메타데이터 초기화
    lef_init_model_meta(&model->meta);
    strcpy(model->meta.model_name, "test_base_model");
    strcpy(model->meta.model_version, "1.0.0");
    strcpy(model->meta.author, "test_author");
    model->meta.sample_rate = 22050;
    model->meta.mel_channels = 80;

    // 3개의 테스트 레이어 생성
    model->num_layers = 3;
    model->layer_headers = (LEFLayerHeader*)calloc(3, sizeof(LEFLayerHeader));
    model->layer_data = (void**)calloc(3, sizeof(void*));
    model->layer_index = (LEFLayerIndexEntry*)calloc(3, sizeof(LEFLayerIndexEntry));

    if (!model->layer_headers || !model->layer_data || !model->layer_index) {
        free(model->layer_headers);
        free(model->layer_data);
        free(model->layer_index);
        free(model);
        return NULL;
    }

    // 각 레이어 초기화
    for (int i = 0; i < 3; i++) {
        lef_init_layer_header(&model->layer_headers[i], i, LEF_LAYER_LINEAR);
        model->layer_headers[i].data_size = 1000 * sizeof(float);

        // 레이어 인덱스 설정
        model->layer_index[i].layer_id = i;
        model->layer_index[i].data_size = 1000 * sizeof(float);

        // 테스트 데이터 생성
        float* layer_data = (float*)calloc(1000, sizeof(float));
        if (!layer_data) {
            // 정리
            for (int k = 0; k < i; k++) {
                free(model->layer_data[k]);
            }
            free(model->layer_headers);
            free(model->layer_data);
            free(model->layer_index);
            free(model);
            return NULL;
        }

        // 기본 패턴으로 데이터 채우기
        for (int j = 0; j < 1000; j++) {
            layer_data[j] = (float)(j + i * 1000) / 10000.0f;
        }

        model->layer_data[i] = layer_data;
    }

    model->owns_memory = true;
    return model;
}

// 테스트용 LEFX 확장 모델 생성 (메모리에서)
static LEFXModel* create_test_extension_model() {
    // LEFX 데이터를 메모리에 구성
    size_t total_size = sizeof(LEFXHeader) + sizeof(LEFXExtensionMeta) +
                       2 * sizeof(LEFXLayerHeader) + 2 * 1000 * sizeof(float);

    uint8_t* buffer = (uint8_t*)calloc(total_size, 1);
    if (!buffer) return NULL;

    uint8_t* ptr = buffer;

    // LEFX 헤더 작성
    LEFXHeader* header = (LEFXHeader*)ptr;
    lefx_init_header(header);
    strcpy(header->base_model_name, "test_base_model");
    strcpy(header->base_model_version, "1.0.0");
    strcpy(header->extension_name, "test_extension");
    strcpy(header->extension_version, "1.0.0");
    strcpy(header->extension_author, "test_author");
    header->extension_type = LEFX_EXT_SPEAKER;
    header->extension_id = 1;
    header->file_size = total_size;
    header->base_model_hash = 0x12345678; // 테스트용 해시

    ptr += sizeof(LEFXHeader);

    // 확장 메타데이터 작성
    LEFXExtensionMeta* meta = (LEFXExtensionMeta*)ptr;
    lefx_init_extension_meta(meta);
    strcpy(meta->description, "Test speaker extension");
    meta->num_layers = 2;
    meta->total_params = 2000;
    meta->quality_score = 0.8f;
    meta->performance_impact = 0.2f;
    meta->gender = 1; // 여성
    meta->age_range = 1; // 청년

    ptr += sizeof(LEFXExtensionMeta);

    // 레이어 헤더들 작성
    header->layer_index_offset = ptr - buffer;
    LEFXLayerHeader* layer_headers = (LEFXLayerHeader*)ptr;

    for (int i = 0; i < 2; i++) {
        lefx_init_layer_header(&layer_headers[i], i + 100, i); // 확장 레이어 ID, 기본 레이어 ID
        layer_headers[i].layer_kind = LEF_LAYER_LINEAR;
        layer_headers[i].data_size = 1000 * sizeof(float);
        layer_headers[i].blend_mode = 0; // 교체 모드
        layer_headers[i].blend_weight = 1.0f;
        layer_headers[i].data_offset = ptr - buffer + 2 * sizeof(LEFXLayerHeader) + i * 1000 * sizeof(float);
    }

    ptr += 2 * sizeof(LEFXLayerHeader);

    // 레이어 데이터 작성
    header->layer_data_offset = ptr - buffer;
    float* layer_data = (float*)ptr;

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 1000; j++) {
            // 기본 모델과 약간 다른 패턴
            layer_data[i * 1000 + j] = (float)(j + i * 1000) / 10000.0f + 0.1f * sinf(j * 0.01f);
        }
    }

    // 메모리에서 확장 모델 로드
    LEFXModel* extension = lefx_load_extension_from_memory(buffer, total_size);
    if (!extension) {
        free(buffer);
        return NULL;
    }

    return extension;
}

static void destroy_test_base_model(LEFModel* model) {
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

/**
 * 테스트 1: 확장 모델 메모리 로딩
 */
void test_extension_memory_loading() {
    print_test_header("확장 모델 메모리 로딩 테스트");

    LEFXModel* extension = create_test_extension_model();
    TEST_ASSERT(extension != NULL, "확장 모델 생성 성공");

    if (extension) {
        // 헤더 검증
        TEST_ASSERT(extension->header.magic == LEFX_MAGIC, "LEFX 매직 넘버 확인");
        TEST_ASSERT(strcmp(extension->header.extension_name, "test_extension") == 0, "확장 이름 확인");
        TEST_ASSERT(extension->header.extension_type == LEFX_EXT_SPEAKER, "확장 타입 확인");

        // 메타데이터 검증
        TEST_ASSERT(extension->meta.num_layers == 2, "레이어 수 확인");
        TEST_ASSERT(extension->meta.gender == 1, "성별 정보 확인");
        TEST_ASSERT(extension->meta.quality_score == 0.8f, "품질 점수 확인");

        // 레이어 데이터 검증
        TEST_ASSERT(extension->num_layers == 2, "로드된 레이어 수 확인");
        TEST_ASSERT(extension->layer_headers != NULL, "레이어 헤더 배열 확인");
        TEST_ASSERT(extension->layer_data != NULL, "레이어 데이터 배열 확인");

        lefx_unload_extension(extension);
    }
}

/**
 * 테스트 2: 호환성 검증
 */
void test_compatibility_check() {
    print_test_header("호환성 검증 테스트");

    LEFModel* base_model = create_test_base_model();
    LEFXModel* extension = create_test_extension_model();

    TEST_ASSERT(base_model != NULL && extension != NULL, "테스트 모델 생성 성공");

    if (base_model && extension) {
        // 기본 모델 해시 설정 (호환성을 위해)
        uint32_t base_hash = lef_calculate_model_hash(&base_model->meta);
        extension->header.base_model_hash = base_hash;

        // 호환성 검증
        bool compatible = lefx_check_compatibility(base_model, extension);
        TEST_ASSERT(compatible, "기본 호환성 검증 성공");

        // 잘못된 모델 이름으로 테스트
        strcpy(extension->header.base_model_name, "wrong_model");
        compatible = lefx_check_compatibility(base_model, extension);
        TEST_ASSERT(!compatible, "잘못된 모델 이름 호환성 검증 실패");

        // 원래 이름으로 복원
        strcpy(extension->header.base_model_name, "test_base_model");

        // 버전 호환성 테스트
        extension->meta.min_base_version_major = 2; // 기본 모델보다 높은 버전 요구
        compatible = lefx_check_compatibility(base_model, extension);
        TEST_ASSERT(!compatible, "버전 비호환성 검증 실패");
    }

    destroy_test_base_model(base_model);
    lefx_unload_extension(extension);
}

/**
 * 테스트 3: 확장 모델 적용
 */
void test_extension_application() {
    print_test_header("확장 모델 적용 테스트");

    LEFModel* base_model = create_test_base_model();
    LEFXModel* extension = create_test_extension_model();

    TEST_ASSERT(base_model != NULL && extension != NULL, "테스트 모델 생성 성공");

    if (base_model && extension) {
        // 호환성을 위한 설정
        uint32_t base_hash = lef_calculate_model_hash(&base_model->meta);
        extension->header.base_model_hash = base_hash;

        // 원본 데이터 백업 (첫 번째 레이어)
        float* original_data = (float*)malloc(1000 * sizeof(float));
        if (original_data) {
            memcpy(original_data, base_model->layer_data[0], 1000 * sizeof(float));
        }

        // 확장 모델 적용
        int result = lefx_apply_extension(base_model, extension, 0.5f);
        TEST_ASSERT(result == LEF_SUCCESS, "확장 모델 적용 성공");

        if (result == LEF_SUCCESS) {
            // 데이터가 변경되었는지 확인
            float* modified_data = (float*)base_model->layer_data[0];
            bool data_changed = false;

            if (original_data) {
                for (int i = 0; i < 100; i++) { // 일부만 확인
                    if (fabsf(modified_data[i] - original_data[i]) > 0.001f) {
                        data_changed = true;
                        break;
                    }
                }
            }

            TEST_ASSERT(data_changed, "레이어 데이터 변경 확인");
            TEST_ASSERT(extension->is_active, "확장 활성화 상태 확인");
            TEST_ASSERT(extension->current_weight == 0.5f, "현재 가중치 확인");
        }

        // 확장 모델 비활성화
        result = lefx_deactivate_extension(base_model, extension);
        TEST_ASSERT(result == LEF_SUCCESS, "확장 모델 비활성화 성공");
        TEST_ASSERT(!extension->is_active, "확장 비활성화 상태 확인");

        free(original_data);
    }

    destroy_test_base_model(base_model);
    lefx_unload_extension(extension);
}

/**
 * 테스트 4: 레이어 블렌딩 모드
 */
void test_layer_blending_modes() {
    print_test_header("레이어 블렌딩 모드 테스트");

    const int data_size = 100;
    float* base_data = (float*)malloc(data_size * sizeof(float));
    float* ext_data = (float*)malloc(data_size * sizeof(float));
    float* test_data = (float*)malloc(data_size * sizeof(float));

    TEST_ASSERT(base_data != NULL && ext_data != NULL && test_data != NULL, "메모리 할당 성공");

    if (base_data && ext_data && test_data) {
        // 테스트 데이터 초기화
        for (int i = 0; i < data_size; i++) {
            base_data[i] = 1.0f;
            ext_data[i] = 2.0f;
        }

        // 교체 모드 테스트 (blend_mode = 0)
        memcpy(test_data, base_data, data_size * sizeof(float));
        int result = lefx_apply_layer_blending(test_data, ext_data, data_size * sizeof(float), 0, 0.5f);
        TEST_ASSERT(result == LEF_SUCCESS, "교체 모드 블렌딩 성공");
        TEST_ASSERT(fabsf(test_data[0] - 1.5f) < 0.001f, "교체 모드 결과 확인 (1.0 * 0.5 + 2.0 * 0.5 = 1.5)");

        // 덧셈 모드 테스트 (blend_mode = 1)
        memcpy(test_data, base_data, data_size * sizeof(float));
        result = lefx_apply_layer_blending(test_data, ext_data, data_size * sizeof(float), 1, 0.5f);
        TEST_ASSERT(result == LEF_SUCCESS, "덧셈 모드 블렌딩 성공");
        TEST_ASSERT(fabsf(test_data[0] - 2.0f) < 0.001f, "덧셈 모드 결과 확인 (1.0 + 2.0 * 0.5 = 2.0)");

        // 곱셈 모드 테스트 (blend_mode = 2)
        memcpy(test_data, base_data, data_size * sizeof(float));
        result = lefx_apply_layer_blending(test_data, ext_data, data_size * sizeof(float), 2, 0.5f);
        TEST_ASSERT(result == LEF_SUCCESS, "곱셈 모드 블렌딩 성공");
        TEST_ASSERT(fabsf(test_data[0] - 2.0f) < 0.001f, "곱셈 모드 결과 확인 (1.0 * (1 + 2.0 * 0.5) = 2.0)");

        // 잘못된 블렌딩 모드 테스트
        result = lefx_apply_layer_blending(test_data, ext_data, data_size * sizeof(float), 99, 0.5f);
        TEST_ASSERT(result == LEF_ERROR_INVALID_ARGUMENT, "잘못된 블렌딩 모드 에러 확인");
    }

    free(base_data);
    free(ext_data);
    free(test_data);
}

/**
 * 테스트 5: 확장 레이어 데이터 접근
 */
void test_extension_layer_access() {
    print_test_header("확장 레이어 데이터 접근 테스트");

    LEFXModel* extension = create_test_extension_model();
    TEST_ASSERT(extension != NULL, "확장 모델 생성 성공");

    if (extension) {
        // 첫 번째 레이어 데이터 접근
        void* layer_data = lefx_get_layer_data(extension, 100); // 확장 레이어 ID 100
        TEST_ASSERT(layer_data != NULL, "첫 번째 레이어 데이터 접근 성공");

        // 첫 번째 레이어 헤더 접근
        const LEFXLayerHeader* layer_header = lefx_get_layer_header(extension, 100);
        TEST_ASSERT(layer_header != NULL, "첫 번째 레이어 헤더 접근 성공");

        if (layer_header) {
            TEST_ASSERT(layer_header->extension_layer_id == 100, "레이어 ID 확인");
            TEST_ASSERT(layer_header->base_layer_id == 0, "기본 레이어 ID 확인");
            TEST_ASSERT(layer_header->data_size == 1000 * sizeof(float), "레이어 데이터 크기 확인");
        }

        // 존재하지 않는 레이어 접근
        void* invalid_data = lefx_get_layer_data(extension, 999);
        TEST_ASSERT(invalid_data == NULL, "존재하지 않는 레이어 데이터 접근 실패");

        const LEFXLayerHeader* invalid_header = lefx_get_layer_header(extension, 999);
        TEST_ASSERT(invalid_header == NULL, "존재하지 않는 레이어 헤더 접근 실패");

        lefx_unload_extension(extension);
    }
}

/**
 * 테스트 6: 확장 모델 정보 출력
 */
void test_extension_info_printing() {
    print_test_header("확장 모델 정보 출력 테스트");

    LEFXModel* extension = create_test_extension_model();
    TEST_ASSERT(extension != NULL, "확장 모델 생성 성공");

    if (extension) {
        // 정보 출력 (크래시 없이 실행되는지 확인)
        printf("확장 모델 정보 출력 테스트:\n");
        lefx_print_extension_info(extension);

        // 통계 정보 가져오기
        size_t total_params = 0;
        size_t total_size = 0;
        int result = lefx_get_extension_stats(extension, &total_params, &total_size);

        TEST_ASSERT(result == LEF_SUCCESS, "확장 모델 통계 정보 가져오기 성공");
        TEST_ASSERT(total_params == 2000, "총 파라미터 수 확인");
        TEST_ASSERT(total_size > 0, "총 크기 확인");

        printf("총 파라미터 수: %zu, 총 크기: %zu 바이트\n", total_params, total_size);

        lefx_unload_extension(extension);
    }
}

/**
 * 테스트 7: NULL 포인터 안전성
 */
void test_null_pointer_safety() {
    print_test_header("NULL 포인터 안전성 테스트");

    // NULL 포인터로 함수 호출 테스트
    TEST_ASSERT(lefx_load_extension(NULL) == NULL, "NULL 경로로 로딩 실패");
    TEST_ASSERT(lefx_load_extension_from_memory(NULL, 100) == NULL, "NULL 데이터로 로딩 실패");

    lefx_unload_extension(NULL); // 크래시하지 않아야 함
    TEST_ASSERT(true, "NULL 확장 모델 언로드 안전성");

    TEST_ASSERT(!lefx_check_compatibility(NULL, NULL), "NULL 모델들 호환성 검증 실패");
    TEST_ASSERT(lefx_apply_extension(NULL, NULL, 0.5f) == LEF_ERROR_INVALID_ARGUMENT, "NULL 모델들 적용 실패");
    TEST_ASSERT(lefx_deactivate_extension(NULL, NULL) == LEF_ERROR_INVALID_ARGUMENT, "NULL 모델들 비활성화 실패");

    TEST_ASSERT(lefx_get_layer_data(NULL, 0) == NULL, "NULL 확장에서 레이어 데이터 접근 실패");
    TEST_ASSERT(lefx_get_layer_header(NULL, 0) == NULL, "NULL 확장에서 레이어 헤더 접근 실패");

    lefx_print_extension_info(NULL); // 크래시하지 않아야 함
    TEST_ASSERT(true, "NULL 확장 모델 정보 출력 안전성");

    TEST_ASSERT(lefx_get_extension_stats(NULL, NULL, NULL) == LEF_ERROR_INVALID_ARGUMENT, "NULL 확장 통계 실패");
}

/**
 * 메인 테스트 함수
 */
int main() {
    printf("LEFX 확장 모델 로더 및 적용 시스템 단위 테스트 시작\n");
    printf("========================================\n");

    // 테스트 실행
    test_extension_memory_loading();
    test_compatibility_check();
    test_extension_application();
    test_layer_blending_modes();
    test_extension_layer_access();
    test_extension_info_printing();
    test_null_pointer_safety();

    // 테스트 결과 출력
    printf("\n========================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("✓ 모든 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("✗ %d개의 테스트가 실패했습니다.\n", tests_run - tests_passed);
        return 1;
    }
}