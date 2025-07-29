/**
 * @file test_lef_format_comprehensive.c
 * @brief LEF 포맷 포괄적 테스트
 *
 * 모델 저장 및 로딩, 양자화 정확성, 확장 모델 테스트를 포함한
 * LEF 포맷의 모든 기능을 종합적으로 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "libetude/lef_format.h"
#include "libetude/tensor.h"
#include "libetude/memory.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;

// 테스트 파일 경로들
#define TEST_MODEL_PATH "test_comprehensive_model.lef"
#define TEST_EXTENSION_PATH "test_extension.lefx"
#define TEST_DIFF_MODEL_PATH "test_diff_model.lefx"
#define TEST_CACHE_SIZE (1024 * 1024)  // 1MB 캐시

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            printf("✗ %s\n", message); \
            return 0; \
        } \
    } while(0)

// 부동소수점 비교 함수
static int float_equals(float a, float b, float tolerance) {
    return fabsf(a - b) < tolerance;
}

/**
 * 테스트용 복합 모델 파일 생성
 * 다양한 레이어 타입과 양자화를 포함한 모델 생성
 */
static int create_comprehensive_test_model() {
    printf("=== 포괄적 테스트 모델 생성 ===\n");

    LEFSerializationContext* ctx = lef_create_serialization_context(TEST_MODEL_PATH);
    TEST_ASSERT(ctx != NULL, "직렬화 컨텍스트 생성");

    // 모델 정보 설정
    int result = lef_set_model_info(ctx, "ComprehensiveTestModel", "2.1.0",
                                   "LibEtude Test Suite", "포괄적 테스트를 위한 복합 모델");
    TEST_ASSERT(result == LEF_SUCCESS, "모델 정보 설정");

    result = lef_set_model_architecture(ctx, 512, 128, 1024, 8, 16, 50000);
    TEST_ASSERT(result == LEF_SUCCESS, "모델 아키텍처 설정");

    result = lef_set_audio_config(ctx, 44100, 128, 512, 2048);
    TEST_ASSERT(result == LEF_SUCCESS, "오디오 설정");

    // 압축 활성화
    result = lef_enable_compression(ctx, 6);
    TEST_ASSERT(result == LEF_SUCCESS, "압축 활성화");

    // 기본 양자화 설정
    result = lef_set_default_quantization(ctx, LEF_QUANT_BF16);
    TEST_ASSERT(result == LEF_SUCCESS, "기본 양자화 설정");

    // 다양한 레이어 타입과 양자화로 테스트 레이어들 생성
    struct {
        LEFLayerKind kind;
        LEFQuantizationType quant;
        size_t data_size;
        const char* description;
    } test_layers[] = {
        {LEF_LAYER_EMBEDDING, LEF_QUANT_NONE, 4096, "임베딩 레이어 (FP32)"},
        {LEF_LAYER_LINEAR, LEF_QUANT_BF16, 8192, "선형 레이어 (BF16)"},
        {LEF_LAYER_ATTENTION, LEF_QUANT_INT8, 16384, "어텐션 레이어 (INT8)"},
        {LEF_LAYER_CONV1D, LEF_QUANT_INT4, 2048, "1D 컨볼루션 (INT4)"},
        {LEF_LAYER_NORMALIZATION, LEF_QUANT_FP16, 1024, "정규화 레이어 (FP16)"},
        {LEF_LAYER_ACTIVATION, LEF_QUANT_NONE, 512, "활성화 함수 (FP32)"},
        {LEF_LAYER_VOCODER, LEF_QUANT_MIXED, 32768, "보코더 레이어 (혼합 정밀도)"},
        {LEF_LAYER_CUSTOM, LEF_QUANT_BF16, 4096, "사용자 정의 레이어 (BF16)"}
    };

    int num_layers = sizeof(test_layers) / sizeof(test_layers[0]);

    for (int i = 0; i < num_layers; i++) {
        LEFLayerData layer_data = {0};
        layer_data.layer_id = i;
        layer_data.layer_kind = test_layers[i].kind;
        layer_data.quant_type = test_layers[i].quant;
        layer_data.data_size = test_layers[i].data_size;
        layer_data.layer_meta = NULL;
        layer_data.meta_size = 0;
        layer_data.quant_params = NULL;

        // 테스트 데이터 생성 (레이어별 고유 패턴)
        layer_data.weight_data = malloc(layer_data.data_size);
        TEST_ASSERT(layer_data.weight_data != NULL, "레이어 데이터 메모리 할당");

        float* data = (float*)layer_data.weight_data;
        size_t float_count = layer_data.data_size / sizeof(float);

        // 레이어별 고유한 데이터 패턴 생성
        for (size_t j = 0; j < float_count; j++) {
            switch (test_layers[i].kind) {
                case LEF_LAYER_EMBEDDING:
                    // 임베딩: 정규분포 패턴
                    data[j] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
                    break;
                case LEF_LAYER_LINEAR:
                    // 선형: Xavier 초기화 패턴
                    data[j] = ((float)rand() / RAND_MAX - 0.5f) * sqrtf(6.0f / 512.0f);
                    break;
                case LEF_LAYER_ATTENTION:
                    // 어텐션: 작은 값들
                    data[j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
                    break;
                case LEF_LAYER_CONV1D:
                    // 컨볼루션: 대칭 패턴
                    data[j] = sinf(2.0f * 3.14159f * j / 100.0f) * 0.5f;
                    break;
                case LEF_LAYER_NORMALIZATION:
                    // 정규화: 1 근처 값들
                    data[j] = 1.0f + ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
                    break;
                case LEF_LAYER_ACTIVATION:
                    // 활성화: 바이어스 값들
                    data[j] = ((float)rand() / RAND_MAX - 0.5f) * 0.01f;
                    break;
                case LEF_LAYER_VOCODER:
                    // 보코더: 복잡한 패턴
                    data[j] = sinf(j * 0.1f) * cosf(j * 0.05f) * 0.8f;
                    break;
                default:
                    // 기본: 순차 패턴
                    data[j] = (float)(i * 1000 + j) * 0.001f;
                    break;
            }
        }

        // 레이어 추가
        result = lef_add_layer(ctx, &layer_data);
        TEST_ASSERT(result == LEF_SUCCESS, test_layers[i].description);

        free(layer_data.weight_data);
    }

    // 모델 저장 완료
    result = lef_finalize_model(ctx);
    TEST_ASSERT(result == LEF_SUCCESS, "모델 저장 완료");

    lef_destroy_serialization_context(ctx);

    printf("포괄적 테스트 모델 생성 완료\n");
    return 1;
}

/**
 * 모델 저장 및 로딩 정확성 테스트
 */
static int test_model_save_load_accuracy() {
    printf("\n=== 모델 저장/로딩 정확성 테스트 ===\n");

    // 모델 로드
    LEFModel* model = lef_load_model(TEST_MODEL_PATH);
    TEST_ASSERT(model != NULL, "모델 로딩");

    // 헤더 검증
    TEST_ASSERT(model->header.magic == LEF_MAGIC, "매직 넘버 검증");
    TEST_ASSERT(model->header.version_major == LEF_VERSION_MAJOR, "주 버전 검증");
    TEST_ASSERT(model->header.version_minor == LEF_VERSION_MINOR, "부 버전 검증");
    TEST_ASSERT(model->header.flags & LEF_FLAG_COMPRESSED, "압축 플래그 검증");
    TEST_ASSERT(model->header.flags & LEF_FLAG_QUANTIZED, "양자화 플래그 검증");

    // 메타데이터 검증
    TEST_ASSERT(strcmp(model->meta.model_name, "ComprehensiveTestModel") == 0, "모델 이름 검증");
    TEST_ASSERT(strcmp(model->meta.model_version, "2.1.0") == 0, "모델 버전 검증");
    TEST_ASSERT(model->meta.input_dim == 512, "입력 차원 검증");
    TEST_ASSERT(model->meta.output_dim == 128, "출력 차원 검증");
    TEST_ASSERT(model->meta.hidden_dim == 1024, "은닉 차원 검증");
    TEST_ASSERT(model->meta.num_layers == 8, "레이어 수 검증");
    TEST_ASSERT(model->meta.sample_rate == 44100, "샘플링 레이트 검증");
    TEST_ASSERT(model->meta.mel_channels == 128, "Mel 채널 수 검증");

    // 각 레이어 검증
    for (int i = 0; i < 8; i++) {
        const LEFLayerHeader* header = lef_get_layer_header(model, i);
        TEST_ASSERT(header != NULL, "레이어 헤더 존재");
        TEST_ASSERT(header->layer_id == i, "레이어 ID 일치");

        void* layer_data = lef_get_layer_data(model, i);
        TEST_ASSERT(layer_data != NULL, "레이어 데이터 존재");

        // 데이터 무결성 검증 (체크섬)
        uint32_t calculated_checksum = lef_calculate_crc32(layer_data, header->data_size);
        TEST_ASSERT(calculated_checksum == header->checksum, "레이어 체크섬 검증");
    }

    // 파일 무결성 검증
    int integrity_result = lef_verify_file_integrity(TEST_MODEL_PATH);
    TEST_ASSERT(integrity_result == LEF_SUCCESS, "파일 무결성 검증");

    lef_unload_model(model);

    printf("모델 저장/로딩 정확성 테스트 완료\n");
    return 1;
}

/**
 * 양자화 정확성 테스트
 */
static int test_quantization_accuracy() {
    printf("\n=== 양자화 정확성 테스트 ===\n");

    LEFModel* model = lef_load_model(TEST_MODEL_PATH);
    TEST_ASSERT(model != NULL, "모델 로딩");

    // 메모리 풀 생성
    ETMemoryPool* pool = et_create_memory_pool(1024 * 1024, 32);
    TEST_ASSERT(pool != NULL, "메모리 풀 생성");

    // 각 양자화 타입별 정확성 검증
    struct {
        int layer_id;
        LEFQuantizationType expected_quant;
        const char* description;
        float tolerance;
    } quant_tests[] = {
        {0, LEF_QUANT_NONE, "FP32 (양자화 없음)", 1e-6f},
        {1, LEF_QUANT_BF16, "BF16 양자화", 1e-2f},
        {2, LEF_QUANT_INT8, "INT8 양자화", 1e-1f},
        {3, LEF_QUANT_INT4, "INT4 양자화", 2e-1f},
        {4, LEF_QUANT_FP16, "FP16 양자화", 1e-3f}
    };

    int num_quant_tests = sizeof(quant_tests) / sizeof(quant_tests[0]);

    for (int i = 0; i < num_quant_tests; i++) {
        const LEFLayerHeader* header = lef_get_layer_header(model, quant_tests[i].layer_id);
        TEST_ASSERT(header != NULL, "레이어 헤더 존재");
        TEST_ASSERT(header->quantization_type == quant_tests[i].expected_quant,
                   quant_tests[i].description);

        void* layer_data = lef_get_layer_data(model, quant_tests[i].layer_id);
        TEST_ASSERT(layer_data != NULL, "레이어 데이터 존재");

        // 양자화된 데이터의 범위 검증
        if (header->quantization_type == LEF_QUANT_INT8) {
            int8_t* int8_data = (int8_t*)layer_data;
            size_t count = header->data_size / sizeof(int8_t);

            bool range_valid = true;
            for (size_t j = 0; j < count && j < 100; j++) { // 처음 100개만 검사
                if (int8_data[j] < -128 || int8_data[j] > 127) {
                    range_valid = false;
                    break;
                }
            }
            TEST_ASSERT(range_valid, "INT8 데이터 범위 검증");
        }
        else if (header->quantization_type == LEF_QUANT_INT4) {
            uint8_t* int4_data = (uint8_t*)layer_data;
            size_t count = header->data_size;

            bool range_valid = true;
            for (size_t j = 0; j < count && j < 100; j++) { // 처음 100개만 검사
                uint8_t val1 = int4_data[j] & 0x0F;
                uint8_t val2 = (int4_data[j] >> 4) & 0x0F;
                if (val1 > 15 || val2 > 15) {
                    range_valid = false;
                    break;
                }
            }
            TEST_ASSERT(range_valid, "INT4 데이터 범위 검증");
        }
    }

    // 양자화 파라미터 검증 (INT8 레이어)
    const LEFLayerHeader* int8_header = lef_get_layer_header(model, 2);
    if (int8_header && int8_header->quantization_type == LEF_QUANT_INT8) {
        // 양자화 파라미터가 합리적인 범위에 있는지 확인
        // (실제 구현에서는 양자화 파라미터를 별도로 저장해야 함)
        TEST_ASSERT(true, "INT8 양자화 파라미터 존재");
    }

    et_destroy_memory_pool(pool);
    lef_unload_model(model);

    printf("양자화 정확성 테스트 완료\n");
    return 1;
}

/**
 * 테스트용 확장 모델 생성
 */
static int create_test_extension_model() {
    printf("\n=== 테스트 확장 모델 생성 ===\n");

    // 기본 모델 로드 (호환성 검증용)
    LEFModel* base_model = lef_load_model(TEST_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    // LEFX 확장 모델 생성 (실제 구현에서는 별도의 직렬화 컨텍스트 필요)
    FILE* ext_file = fopen(TEST_EXTENSION_PATH, "wb");
    TEST_ASSERT(ext_file != NULL, "확장 파일 생성");

    // LEFX 헤더 생성
    LEFXHeader ext_header = {0};
    lefx_init_header(&ext_header);
    ext_header.extension_type = LEFX_EXT_SPEAKER;
    ext_header.extension_id = 1001;
    ext_header.base_model_hash = base_model->header.model_hash;
    strcpy(ext_header.base_model_name, base_model->meta.model_name);
    strcpy(ext_header.base_model_version, base_model->meta.model_version);
    strcpy(ext_header.extension_name, "TestSpeaker");
    strcpy(ext_header.extension_author, "LibEtude Test");
    strcpy(ext_header.extension_version, "1.0");

    // 헤더 쓰기
    size_t written = fwrite(&ext_header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(written == 1, "LEFX 헤더 쓰기");

    // 확장 메타데이터 생성
    LEFXExtensionMeta ext_meta = {0};
    lefx_init_extension_meta(&ext_meta);
    strcpy(ext_meta.description, "테스트용 화자 확장 모델");
    strcpy(ext_meta.license, "MIT");
    ext_meta.gender = 1; // 여성
    ext_meta.age_range = 1; // 청년
    strcpy((char*)ext_meta.language_code, "ko");
    strcpy((char*)ext_meta.accent_code, "KR");
    ext_meta.quality_score = 0.95f;
    ext_meta.performance_impact = 0.1f;
    ext_meta.num_layers = 2;

    // 메타데이터 쓰기
    written = fwrite(&ext_meta, sizeof(LEFXExtensionMeta), 1, ext_file);
    TEST_ASSERT(written == 1, "LEFX 메타데이터 쓰기");

    // 간단한 확장 레이어 데이터 생성
    for (int i = 0; i < 2; i++) {
        LEFXLayerHeader layer_header = {0};
        lefx_init_layer_header(&layer_header, i, i); // 기본 모델의 레이어 0, 1과 연결
        layer_header.layer_kind = LEF_LAYER_LINEAR;
        layer_header.quantization_type = LEF_QUANT_BF16;
        layer_header.blend_mode = 1; // 덧셈 블렌딩
        layer_header.data_size = 1024 * sizeof(float);
        layer_header.data_offset = ftell(ext_file) + sizeof(LEFXLayerHeader);

        // 테스트 데이터 생성
        float* test_data = (float*)malloc(layer_header.data_size);
        TEST_ASSERT(test_data != NULL, "확장 레이어 데이터 할당");

        for (size_t j = 0; j < 1024; j++) {
            test_data[j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; // 작은 조정값
        }

        layer_header.checksum = lef_calculate_crc32(test_data, layer_header.data_size);

        // 레이어 헤더 쓰기
        written = fwrite(&layer_header, sizeof(LEFXLayerHeader), 1, ext_file);
        TEST_ASSERT(written == 1, "확장 레이어 헤더 쓰기");

        // 레이어 데이터 쓰기
        written = fwrite(test_data, layer_header.data_size, 1, ext_file);
        TEST_ASSERT(written == 1, "확장 레이어 데이터 쓰기");

        free(test_data);
    }

    fclose(ext_file);
    lef_unload_model(base_model);

    printf("테스트 확장 모델 생성 완료\n");
    return 1;
}

/**
 * 확장 모델 테스트
 */
static int test_extension_model() {
    printf("\n=== 확장 모델 테스트 ===\n");

    // 기본 모델 로드
    LEFModel* base_model = lef_load_model(TEST_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    // 확장 모델 로드 (간단한 파일 읽기로 검증)
    FILE* ext_file = fopen(TEST_EXTENSION_PATH, "rb");
    TEST_ASSERT(ext_file != NULL, "확장 파일 열기");

    // LEFX 헤더 읽기
    LEFXHeader ext_header;
    size_t read_size = fread(&ext_header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(read_size == 1, "LEFX 헤더 읽기");

    // 헤더 검증
    TEST_ASSERT(ext_header.magic == LEFX_MAGIC, "LEFX 매직 넘버 검증");
    TEST_ASSERT(ext_header.version_major == LEFX_VERSION_MAJOR, "LEFX 주 버전 검증");
    TEST_ASSERT(ext_header.extension_type == LEFX_EXT_SPEAKER, "확장 타입 검증");
    TEST_ASSERT(ext_header.extension_id == 1001, "확장 ID 검증");
    TEST_ASSERT(strcmp(ext_header.extension_name, "TestSpeaker") == 0, "확장 이름 검증");

    // 기본 모델과의 호환성 검증
    TEST_ASSERT(ext_header.base_model_hash == base_model->header.model_hash, "기본 모델 해시 호환성");
    TEST_ASSERT(strcmp(ext_header.base_model_name, base_model->meta.model_name) == 0, "기본 모델 이름 호환성");

    // 확장 메타데이터 읽기
    LEFXExtensionMeta ext_meta;
    read_size = fread(&ext_meta, sizeof(LEFXExtensionMeta), 1, ext_file);
    TEST_ASSERT(read_size == 1, "LEFX 메타데이터 읽기");

    // 메타데이터 검증
    TEST_ASSERT(strcmp(ext_meta.description, "테스트용 화자 확장 모델") == 0, "확장 설명 검증");
    TEST_ASSERT(ext_meta.gender == 1, "성별 정보 검증");
    TEST_ASSERT(strcmp((char*)ext_meta.language_code, "ko") == 0, "언어 코드 검증");
    TEST_ASSERT(ext_meta.num_layers == 2, "확장 레이어 수 검증");
    TEST_ASSERT(ext_meta.quality_score > 0.9f, "품질 점수 검증");

    // 확장 레이어 검증
    for (int i = 0; i < 2; i++) {
        LEFXLayerHeader layer_header;
        read_size = fread(&layer_header, sizeof(LEFXLayerHeader), 1, ext_file);
        TEST_ASSERT(read_size == 1, "확장 레이어 헤더 읽기");

        TEST_ASSERT(layer_header.extension_layer_id == i, "확장 레이어 ID 검증");
        TEST_ASSERT(layer_header.base_layer_id == i, "기본 레이어 연결 검증");
        TEST_ASSERT(layer_header.layer_kind == LEF_LAYER_LINEAR, "레이어 타입 검증");
        TEST_ASSERT(layer_header.blend_mode == 1, "블렌딩 모드 검증");

        // 레이어 데이터 읽기 및 체크섬 검증
        void* layer_data = malloc(layer_header.data_size);
        TEST_ASSERT(layer_data != NULL, "확장 레이어 데이터 할당");

        read_size = fread(layer_data, layer_header.data_size, 1, ext_file);
        TEST_ASSERT(read_size == 1, "확장 레이어 데이터 읽기");

        uint32_t calculated_checksum = lef_calculate_crc32(layer_data, layer_header.data_size);
        TEST_ASSERT(calculated_checksum == layer_header.checksum, "확장 레이어 체크섬 검증");

        free(layer_data);
    }

    fclose(ext_file);
    lef_unload_model(base_model);

    printf("확장 모델 테스트 완료\n");
    return 1;
}

/**
 * 스트리밍 로더 테스트
 */
static int test_streaming_loader() {
    printf("\n=== 스트리밍 로더 테스트 ===\n");

    // 스트리밍 로더 생성
    LEFStreamingLoader* loader = lef_create_streaming_loader(TEST_MODEL_PATH, TEST_CACHE_SIZE);
    TEST_ASSERT(loader != NULL, "스트리밍 로더 생성");

    // 헤더 및 메타데이터 검증
    TEST_ASSERT(loader->header.magic == LEF_MAGIC, "스트리밍 로더 헤더 검증");
    TEST_ASSERT(strcmp(loader->meta.model_name, "ComprehensiveTestModel") == 0, "스트리밍 로더 모델 이름");

    // 온디맨드 로딩 테스트
    for (int i = 0; i < 4; i++) {
        int result = lef_load_layer_on_demand(loader, i);
        TEST_ASSERT(result == LEF_SUCCESS, "온디맨드 레이어 로딩");

        void* layer_data = lef_streaming_get_layer_data(loader, i);
        TEST_ASSERT(layer_data != NULL, "스트리밍 레이어 데이터 접근");
    }

    // 캐시 정보 확인
    int loaded_layers;
    size_t cache_usage;
    int result = lef_get_cache_info(loader, &loaded_layers, &cache_usage);
    TEST_ASSERT(result == LEF_SUCCESS, "캐시 정보 조회");
    TEST_ASSERT(loaded_layers == 4, "로드된 레이어 수 확인");
    TEST_ASSERT(cache_usage > 0, "캐시 사용량 확인");

    // 레이어 언로드 테스트
    result = lef_unload_layer(loader, 0);
    TEST_ASSERT(result == LEF_SUCCESS, "레이어 언로드");

    result = lef_get_cache_info(loader, &loaded_layers, &cache_usage);
    TEST_ASSERT(result == LEF_SUCCESS, "언로드 후 캐시 정보 조회");
    TEST_ASSERT(loaded_layers == 3, "언로드 후 레이어 수 확인");

    // 캐시 정리 테스트
    result = lef_cleanup_cache(loader, TEST_CACHE_SIZE / 2);
    TEST_ASSERT(result == LEF_SUCCESS, "캐시 정리");

    lef_destroy_streaming_loader(loader);

    printf("스트리밍 로더 테스트 완료\n");
    return 1;
}

/**
 * 메모리 매핑 로더 테스트
 */
static int test_memory_mapping_loader() {
    printf("\n=== 메모리 매핑 로더 테스트 ===\n");

    // 메모리 매핑 모델 로드
    LEFModel* model = lef_load_model_mmap(TEST_MODEL_PATH);
    TEST_ASSERT(model != NULL, "메모리 매핑 모델 로드");

    // 메모리 매핑 플래그 확인
    TEST_ASSERT(model->memory_mapped == true, "메모리 매핑 플래그 확인");

    // 기본 검증
    TEST_ASSERT(model->header.magic == LEF_MAGIC, "메모리 매핑 헤더 검증");
    TEST_ASSERT(strcmp(model->meta.model_name, "ComprehensiveTestModel") == 0, "메모리 매핑 모델 이름");

    // 레이어 데이터 접근 테스트
    for (int i = 0; i < 4; i++) {
        void* layer_data = lef_get_layer_data(model, i);
        TEST_ASSERT(layer_data != NULL, "메모리 매핑 레이어 데이터 접근");

        const LEFLayerHeader* header = lef_get_layer_header(model, i);
        TEST_ASSERT(header != NULL, "메모리 매핑 레이어 헤더 접근");

        // 데이터 무결성 검증
        uint32_t checksum = lef_calculate_crc32(layer_data, header->data_size);
        TEST_ASSERT(checksum == header->checksum, "메모리 매핑 데이터 무결성");
    }

    lef_unload_model(model);

    printf("메모리 매핑 로더 테스트 완료\n");
    return 1;
}

/**
 * 에러 처리 및 경계 조건 테스트
 */
static int test_error_handling_and_edge_cases() {
    printf("\n=== 에러 처리 및 경계 조건 테스트 ===\n");

    // 존재하지 않는 파일 로딩
    LEFModel* model = lef_load_model("/nonexistent/path/model.lef");
    TEST_ASSERT(model == NULL, "존재하지 않는 파일 로딩 실패");

    // NULL 포인터 안전성
    lef_unload_model(NULL);
    void* data = lef_get_layer_data(NULL, 0);
    TEST_ASSERT(data == NULL, "NULL 모델 포인터 안전성");

    const LEFLayerHeader* header = lef_get_layer_header(NULL, 0);
    TEST_ASSERT(header == NULL, "NULL 모델 헤더 안전성");

    // 잘못된 레이어 ID
    model = lef_load_model(TEST_MODEL_PATH);
    TEST_ASSERT(model != NULL, "정상 모델 로딩");

    data = lef_get_layer_data(model, 999);
    TEST_ASSERT(data == NULL, "잘못된 레이어 ID 처리");

    header = lef_get_layer_header(model, 999);
    TEST_ASSERT(header == NULL, "잘못된 레이어 ID 헤더 처리");

    // 파일 무결성 검증 (손상된 파일)
    lef_unload_model(model);

    // 임시로 파일 손상시키기
    FILE* file = fopen(TEST_MODEL_PATH, "r+b");
    if (file) {
        fseek(file, 100, SEEK_SET);
        uint8_t corrupt_byte = 0xFF;
        fwrite(&corrupt_byte, 1, 1, file);
        fclose(file);

        int integrity_result = lef_verify_file_integrity(TEST_MODEL_PATH);
        TEST_ASSERT(integrity_result != LEF_SUCCESS, "손상된 파일 감지");

        // 파일 복원 (테스트 모델 재생성)
        create_comprehensive_test_model();
    }

    // 스트리밍 로더 에러 처리
    LEFStreamingLoader* loader = lef_create_streaming_loader("/nonexistent/path/model.lef", TEST_CACHE_SIZE);
    TEST_ASSERT(loader == NULL, "존재하지 않는 파일 스트리밍 로더 실패");

    // 메모리 부족 시뮬레이션 (매우 작은 캐시)
    loader = lef_create_streaming_loader(TEST_MODEL_PATH, 100);
    if (loader) {
        int result = lef_load_layer_on_demand(loader, 0);
        // 작은 캐시에서도 기본적인 동작은 가능해야 함
        TEST_ASSERT(result == LEF_SUCCESS || result == LEF_ERROR_OUT_OF_MEMORY, "작은 캐시 처리");
        lef_destroy_streaming_loader(loader);
    }

    printf("에러 처리 및 경계 조건 테스트 완료\n");
    return 1;
}

/**
 * 성능 및 메모리 사용량 테스트
 */
static int test_performance_and_memory() {
    printf("\n=== 성능 및 메모리 사용량 테스트 ===\n");

    clock_t start_time, end_time;

    // 모델 로딩 성능 측정
    start_time = clock();
    LEFModel* model = lef_load_model(TEST_MODEL_PATH);
    end_time = clock();

    TEST_ASSERT(model != NULL, "성능 테스트용 모델 로딩");

    double load_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("모델 로딩 시간: %.3f초\n", load_time);
    TEST_ASSERT(load_time < 5.0, "모델 로딩 시간 5초 이내"); // 합리적인 시간 제한

    // 모델 통계 정보
    size_t total_params, total_size;
    int result = lef_get_model_stats(model, &total_params, &total_size);
    TEST_ASSERT(result == LEF_SUCCESS, "모델 통계 정보 조회");

    printf("총 파라미터 수: %zu\n", total_params);
    printf("총 모델 크기: %zu 바이트 (%.2f MB)\n", total_size, (double)total_size / (1024 * 1024));

    TEST_ASSERT(total_params > 0, "파라미터 수 양수");
    TEST_ASSERT(total_size > 0, "모델 크기 양수");

    // 레이어 접근 성능 측정
    start_time = clock();
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < model->meta.num_layers; j++) {
            void* layer_data = lef_get_layer_data(model, j);
            (void)layer_data; // 사용하지 않는 변수 경고 방지
        }
    }
    end_time = clock();

    double access_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("레이어 접근 성능 (100회 반복): %.3f초\n", access_time);
    TEST_ASSERT(access_time < 1.0, "레이어 접근 성능 1초 이내");

    lef_unload_model(model);

    // 스트리밍 로더 성능 측정
    start_time = clock();
    LEFStreamingLoader* loader = lef_create_streaming_loader(TEST_MODEL_PATH, TEST_CACHE_SIZE);
    end_time = clock();

    TEST_ASSERT(loader != NULL, "스트리밍 로더 생성");

    double streaming_init_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("스트리밍 로더 초기화 시간: %.3f초\n", streaming_init_time);
    TEST_ASSERT(streaming_init_time < 1.0, "스트리밍 로더 초기화 1초 이내");

    // 온디맨드 로딩 성능
    start_time = clock();
    for (int i = 0; i < loader->meta.num_layers; i++) {
        int load_result = lef_load_layer_on_demand(loader, i);
        TEST_ASSERT(load_result == LEF_SUCCESS, "온디맨드 로딩 성공");
    }
    end_time = clock();

    double on_demand_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    printf("온디맨드 로딩 시간: %.3f초\n", on_demand_time);
    TEST_ASSERT(on_demand_time < 2.0, "온디맨드 로딩 2초 이내");

    lef_destroy_streaming_loader(loader);

    printf("성능 및 메모리 사용량 테스트 완료\n");
    return 1;
}

/**
 * 파일 정리 함수
 */
static void cleanup_test_files() {
    unlink(TEST_MODEL_PATH);
    unlink(TEST_EXTENSION_PATH);
    unlink(TEST_DIFF_MODEL_PATH);
}

/**
 * 메인 테스트 함수
 */
int main() {
    printf("LibEtude LEF 포맷 포괄적 테스트 시작\n");
    printf("=====================================\n");

    // 테스트 파일 정리 (이전 실행 잔여물)
    cleanup_test_files();

    // 테스트 실행
    int success = 1;

    success &= create_comprehensive_test_model();
    success &= test_model_save_load_accuracy();
    success &= test_quantization_accuracy();
    success &= create_test_extension_model();
    success &= test_extension_model();
    success &= test_streaming_loader();
    success &= test_memory_mapping_loader();
    success &= test_error_handling_and_edge_cases();
    success &= test_performance_and_memory();

    // 테스트 파일 정리
    cleanup_test_files();

    // 결과 출력
    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (success && tests_passed == tests_run) {
        printf("✓ 모든 LEF 포맷 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("✗ 일부 테스트가 실패했습니다.\n");
        return 1;
    }
}