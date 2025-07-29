/**
 * @file test_lefx_extension_comprehensive.c
 * @brief LEFX 확장 모델 포괄적 테스트
 *
 * LEFX 확장 모델의 생성, 로딩, 호환성 검증, 차분 모델,
 * 조건부 활성화 등 모든 기능을 종합적으로 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "libetude/lef_format.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;

// 테스트 파일 경로들
#define TEST_BASE_MODEL_PATH "test_base_model.lef"
#define TEST_SPEAKER_EXT_PATH "test_speaker_extension.lefx"
#define TEST_LANGUAGE_EXT_PATH "test_language_extension.lefx"
#define TEST_EFFECT_EXT_PATH "test_effect_extension.lefx"
#define TEST_DIFF_MODEL_PATH "test_diff_model.lefx"
#define TEST_PLUGIN_EXT_PATH "test_plugin_extension.lefx"

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
 * 테스트용 기본 모델 생성
 */
static int create_test_base_model() {
    printf("=== 테스트 기본 모델 생성 ===\n");

    LEFSerializationContext* ctx = lef_create_serialization_context(TEST_BASE_MODEL_PATH);
    TEST_ASSERT(ctx != NULL, "기본 모델 직렬화 컨텍스트 생성");

    // 기본 모델 정보 설정
    int result = lef_set_model_info(ctx, "BaseVoiceModel", "1.0.0",
                                   "LibEtude Team", "확장 테스트용 기본 음성 모델");
    TEST_ASSERT(result == LEF_SUCCESS, "기본 모델 정보 설정");

    result = lef_set_model_architecture(ctx, 512, 128, 1024, 6, 12, 30000);
    TEST_ASSERT(result == LEF_SUCCESS, "기본 모델 아키텍처 설정");

    result = lef_set_audio_config(ctx, 22050, 80, 256, 1024);
    TEST_ASSERT(result == LEF_SUCCESS, "기본 모델 오디오 설정");

    // 기본 레이어들 추가
    for (int i = 0; i < 6; i++) {
        size_t data_size = (1000 + i * 500) * sizeof(float);
        float* layer_data = (float*)malloc(data_size);
        TEST_ASSERT(layer_data != NULL, "기본 레이어 데이터 할당");

        // 기본 패턴으로 데이터 채우기
        size_t float_count = data_size / sizeof(float);
        for (size_t j = 0; j < float_count; j++) {
            layer_data[j] = sinf((float)j * 0.01f) * (0.5f + i * 0.1f);
        }

        LEFLayerData layer = {
            .layer_id = i,
            .layer_kind = (i < 3) ? LEF_LAYER_LINEAR : LEF_LAYER_ATTENTION,
            .quant_type = LEF_QUANT_BF16,
            .weight_data = layer_data,
            .data_size = data_size,
            .layer_meta = NULL,
            .meta_size = 0,
            .quant_params = NULL
        };

        result = lef_add_layer(ctx, &layer);
        TEST_ASSERT(result == LEF_SUCCESS, "기본 레이어 추가");

        free(layer_data);
    }

    result = lef_finalize_model(ctx);
    TEST_ASSERT(result == LEF_SUCCESS, "기본 모델 저장 완료");

    lef_destroy_serialization_context(ctx);

    printf("테스트 기본 모델 생성 완료\n");
    return 1;
}

/**
 * 화자 확장 모델 생성 및 테스트
 */
static int test_speaker_extension() {
    printf("\n=== 화자 확장 모델 테스트 ===\n");

    // 기본 모델 로드
    LEFModel* base_model = lef_load_model(TEST_BASE_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    // 화자 확장 파일 생성
    FILE* ext_file = fopen(TEST_SPEAKER_EXT_PATH, "wb");
    TEST_ASSERT(ext_file != NULL, "화자 확장 파일 생성");

    // LEFX 헤더 생성
    LEFXHeader header = {0};
    lefx_init_header(&header);
    header.extension_type = LEFX_EXT_SPEAKER;
    header.extension_id = 2001;
    header.extension_flags = LEFX_FLAG_SPEAKER_EXT | LEFX_FLAG_DIFFERENTIAL;
    header.base_model_hash = base_model->header.model_hash;
    strcpy(header.base_model_name, base_model->meta.model_name);
    strcpy(header.base_model_version, base_model->meta.model_version);
    strcpy(header.extension_name, "FemaleVoice01");
    strcpy(header.extension_author, "LibEtude Voice Team");
    strcpy(header.extension_version, "1.2.0");

    // 오프셋 설정
    header.meta_offset = sizeof(LEFXHeader);
    header.layer_index_offset = header.meta_offset + sizeof(LEFXExtensionMeta);
    header.layer_data_offset = header.layer_index_offset + 3 * sizeof(LEFXLayerHeader);
    header.file_size = header.layer_data_offset + 3 * 2000; // 대략적인 크기

    size_t written = fwrite(&header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(written == 1, "LEFX 헤더 쓰기");

    // 확장 메타데이터 생성
    LEFXExtensionMeta meta = {0};
    lefx_init_extension_meta(&meta);
    strcpy(meta.description, "젊은 여성 화자의 음성 특성을 반영한 확장 모델");
    strcpy(meta.license, "Commercial");
    strcpy(meta.website, "https://libetude.ai/voices/female01");
    strcpy(meta.contact, "voices@libetude.ai");

    meta.min_base_version_major = 1;
    meta.min_base_version_minor = 0;
    meta.max_base_version_major = 1;
    meta.max_base_version_minor = 9;

    meta.extension_capabilities = LEFX_FLAG_SPEAKER_EXT | LEFX_FLAG_CONDITIONAL;
    meta.priority = 100;
    meta.num_layers = 3;
    meta.total_params = 15000;
    meta.memory_requirement = 512; // KB

    meta.gender = 1; // 여성
    meta.age_range = 1; // 청년
    strcpy((char*)meta.language_code, "ko");
    strcpy((char*)meta.accent_code, "KR");

    meta.quality_score = 0.92f;
    meta.performance_impact = 0.15f;
    meta.inference_time_ms = 50;
    meta.loading_time_ms = 200;

    written = fwrite(&meta, sizeof(LEFXExtensionMeta), 1, ext_file);
    TEST_ASSERT(written == 1, "LEFX 메타데이터 쓰기");

    // 확장 레이어들 생성
    for (int i = 0; i < 3; i++) {
        LEFXLayerHeader layer_header = {0};
        lefx_init_layer_header(&layer_header, i, i); // 기본 모델의 레이어 0,1,2와 연결
        layer_header.layer_kind = LEF_LAYER_LINEAR;
        layer_header.quantization_type = LEF_QUANT_BF16;
        layer_header.blend_mode = 2; // 곱셈 블렌딩
        layer_header.activation_condition = 1; // 조건부 활성화
        layer_header.data_size = 2000;
        layer_header.similarity_threshold = 0.8f;
        layer_header.blend_weight = 0.3f + i * 0.1f;
        layer_header.dependency_count = 0;

        // 테스트 데이터 생성 (화자 특성 반영)
        float* layer_data = (float*)malloc(layer_header.data_size);
        TEST_ASSERT(layer_data != NULL, "화자 확장 레이어 데이터 할당");

        size_t float_count = layer_header.data_size / sizeof(float);
        for (size_t j = 0; j < float_count; j++) {
            // 여성 화자 특성을 시뮬레이션한 조정값
            layer_data[j] = ((float)rand() / RAND_MAX - 0.5f) * 0.2f * (1.0f + i * 0.1f);
        }

        layer_header.checksum = lef_calculate_crc32(layer_data, layer_header.data_size);

        written = fwrite(&layer_header, sizeof(LEFXLayerHeader), 1, ext_file);
        TEST_ASSERT(written == 1, "화자 확장 레이어 헤더 쓰기");

        written = fwrite(layer_data, layer_header.data_size, 1, ext_file);
        TEST_ASSERT(written == 1, "화자 확장 레이어 데이터 쓰기");

        free(layer_data);
    }

    fclose(ext_file);

    // 생성된 확장 모델 검증
    ext_file = fopen(TEST_SPEAKER_EXT_PATH, "rb");
    TEST_ASSERT(ext_file != NULL, "화자 확장 파일 재열기");

    LEFXHeader read_header;
    size_t read_size = fread(&read_header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(read_size == 1, "화자 확장 헤더 읽기");

    // 헤더 검증
    TEST_ASSERT(read_header.magic == LEFX_MAGIC, "LEFX 매직 넘버 검증");
    TEST_ASSERT(read_header.extension_type == LEFX_EXT_SPEAKER, "화자 확장 타입 검증");
    TEST_ASSERT(read_header.extension_id == 2001, "화자 확장 ID 검증");
    TEST_ASSERT(strcmp(read_header.extension_name, "FemaleVoice01") == 0, "화자 이름 검증");

    // 호환성 검증
    TEST_ASSERT(read_header.base_model_hash == base_model->header.model_hash, "기본 모델 해시 호환성");
    TEST_ASSERT(strcmp(read_header.base_model_name, base_model->meta.model_name) == 0, "기본 모델 이름 호환성");

    fclose(ext_file);
    lef_unload_model(base_model);

    printf("화자 확장 모델 테스트 완료\n");
    return 1;
}

/**
 * 언어 확장 모델 생성 및 테스트
 */
static int test_language_extension() {
    printf("\n=== 언어 확장 모델 테스트 ===\n");

    LEFModel* base_model = lef_load_model(TEST_BASE_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    FILE* ext_file = fopen(TEST_LANGUAGE_EXT_PATH, "wb");
    TEST_ASSERT(ext_file != NULL, "언어 확장 파일 생성");

    // LEFX 헤더 생성
    LEFXHeader header = {0};
    lefx_init_header(&header);
    header.extension_type = LEFX_EXT_LANGUAGE;
    header.extension_id = 3001;
    header.extension_flags = LEFX_FLAG_LANGUAGE_EXT | LEFX_FLAG_COMPRESSED;
    header.base_model_hash = base_model->header.model_hash;
    strcpy(header.base_model_name, base_model->meta.model_name);
    strcpy(header.base_model_version, base_model->meta.model_version);
    strcpy(header.extension_name, "EnglishLanguagePack");
    strcpy(header.extension_author, "LibEtude Localization Team");
    strcpy(header.extension_version, "2.0.0");

    size_t written = fwrite(&header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(written == 1, "언어 확장 헤더 쓰기");

    // 언어 확장 메타데이터
    LEFXExtensionMeta meta = {0};
    lefx_init_extension_meta(&meta);
    strcpy(meta.description, "영어 발음 및 언어 특성을 위한 확장 모델");
    strcpy(meta.license, "GPL-3.0");
    strcpy((char*)meta.language_code, "en");
    strcpy((char*)meta.accent_code, "US");

    meta.gender = 255; // 성별 무관
    meta.age_range = 255; // 연령 무관
    meta.extension_capabilities = LEFX_FLAG_LANGUAGE_EXT;
    meta.priority = 200;
    meta.num_layers = 4;
    meta.quality_score = 0.88f;
    meta.performance_impact = 0.25f;

    written = fwrite(&meta, sizeof(LEFXExtensionMeta), 1, ext_file);
    TEST_ASSERT(written == 1, "언어 확장 메타데이터 쓰기");

    // 언어별 특화 레이어들
    for (int i = 0; i < 4; i++) {
        LEFXLayerHeader layer_header = {0};
        lefx_init_layer_header(&layer_header, i, i + 2); // 기본 모델의 레이어 2,3,4,5와 연결
        layer_header.layer_kind = (i < 2) ? LEF_LAYER_EMBEDDING : LEF_LAYER_ATTENTION;
        layer_header.quantization_type = LEF_QUANT_INT8;
        layer_header.blend_mode = 1; // 덧셈 블렌딩
        layer_header.activation_condition = 1; // 조건부 활성화
        layer_header.data_size = 1500 + i * 200;
        layer_header.blend_weight = 0.4f;

        // 언어 특화 데이터 생성
        float* layer_data = (float*)malloc(layer_header.data_size);
        TEST_ASSERT(layer_data != NULL, "언어 확장 레이어 데이터 할당");

        size_t float_count = layer_header.data_size / sizeof(float);
        for (size_t j = 0; j < float_count; j++) {
            // 영어 발음 특성을 시뮬레이션한 조정값
            layer_data[j] = cosf((float)j * 0.02f) * 0.15f * (1.0f - i * 0.05f);
        }

        layer_header.checksum = lef_calculate_crc32(layer_data, layer_header.data_size);

        written = fwrite(&layer_header, sizeof(LEFXLayerHeader), 1, ext_file);
        TEST_ASSERT(written == 1, "언어 확장 레이어 헤더 쓰기");

        written = fwrite(layer_data, layer_header.data_size, 1, ext_file);
        TEST_ASSERT(written == 1, "언어 확장 레이어 데이터 쓰기");

        free(layer_data);
    }

    fclose(ext_file);

    // 언어 확장 검증
    ext_file = fopen(TEST_LANGUAGE_EXT_PATH, "rb");
    TEST_ASSERT(ext_file != NULL, "언어 확장 파일 재열기");

    LEFXHeader read_header;
    size_t read_size = fread(&read_header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(read_size == 1, "언어 확장 헤더 읽기");

    TEST_ASSERT(read_header.extension_type == LEFX_EXT_LANGUAGE, "언어 확장 타입 검증");
    TEST_ASSERT(read_header.extension_flags & LEFX_FLAG_LANGUAGE_EXT, "언어 확장 플래그 검증");
    TEST_ASSERT(strcmp(read_header.extension_name, "EnglishLanguagePack") == 0, "언어 팩 이름 검증");

    LEFXExtensionMeta read_meta;
    read_size = fread(&read_meta, sizeof(LEFXExtensionMeta), 1, ext_file);
    TEST_ASSERT(read_size == 1, "언어 확장 메타데이터 읽기");

    TEST_ASSERT(strcmp((char*)read_meta.language_code, "en") == 0, "언어 코드 검증");
    TEST_ASSERT(strcmp((char*)read_meta.accent_code, "US") == 0, "억양 코드 검증");
    TEST_ASSERT(read_meta.num_layers == 4, "언어 확장 레이어 수 검증");

    fclose(ext_file);
    lef_unload_model(base_model);

    printf("언어 확장 모델 테스트 완료\n");
    return 1;
}

/**
 * 오디오 효과 확장 모델 테스트
 */
static int test_audio_effect_extension() {
    printf("\n=== 오디오 효과 확장 모델 테스트 ===\n");

    LEFModel* base_model = lef_load_model(TEST_BASE_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    FILE* ext_file = fopen(TEST_EFFECT_EXT_PATH, "wb");
    TEST_ASSERT(ext_file != NULL, "효과 확장 파일 생성");

    // 효과 확장 헤더
    LEFXHeader header = {0};
    lefx_init_header(&header);
    header.extension_type = LEFX_EXT_AUDIO_EFFECT;
    header.extension_id = 4001;
    header.extension_flags = LEFX_FLAG_EFFECT_EXT | LEFX_FLAG_CONDITIONAL;
    header.base_model_hash = base_model->header.model_hash;
    strcpy(header.base_model_name, base_model->meta.model_name);
    strcpy(header.base_model_version, base_model->meta.model_version);
    strcpy(header.extension_name, "ReverbEffect");
    strcpy(header.extension_author, "LibEtude Audio Team");
    strcpy(header.extension_version, "1.5.0");

    size_t written = fwrite(&header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(written == 1, "효과 확장 헤더 쓰기");

    // 효과 확장 메타데이터
    LEFXExtensionMeta meta = {0};
    lefx_init_extension_meta(&meta);
    strcpy(meta.description, "실시간 리버브 효과를 위한 확장 모델");
    strcpy(meta.license, "Proprietary");
    meta.extension_capabilities = LEFX_FLAG_EFFECT_EXT | LEFX_FLAG_CONDITIONAL;
    meta.priority = 50;
    meta.num_layers = 2;
    meta.quality_score = 0.85f;
    meta.performance_impact = 0.3f;
    meta.inference_time_ms = 20;

    written = fwrite(&meta, sizeof(LEFXExtensionMeta), 1, ext_file);
    TEST_ASSERT(written == 1, "효과 확장 메타데이터 쓰기");

    // 효과 레이어들
    for (int i = 0; i < 2; i++) {
        LEFXLayerHeader layer_header = {0};
        lefx_init_layer_header(&layer_header, i, 5); // 보코더 레이어와 연결
        layer_header.layer_kind = LEF_LAYER_VOCODER;
        layer_header.quantization_type = LEF_QUANT_FP16;
        layer_header.blend_mode = 3; // 보간 블렌딩
        layer_header.activation_condition = 1; // 조건부 활성화
        layer_header.data_size = 800 + i * 400;
        layer_header.blend_weight = 0.2f + i * 0.1f;

        // 효과 파라미터 데이터
        float* layer_data = (float*)malloc(layer_header.data_size);
        TEST_ASSERT(layer_data != NULL, "효과 확장 레이어 데이터 할당");

        size_t float_count = layer_header.data_size / sizeof(float);
        for (size_t j = 0; j < float_count; j++) {
            // 리버브 효과 파라미터 시뮬레이션
            layer_data[j] = expf(-(float)j * 0.001f) * sinf((float)j * 0.05f) * 0.1f;
        }

        layer_header.checksum = lef_calculate_crc32(layer_data, layer_header.data_size);

        written = fwrite(&layer_header, sizeof(LEFXLayerHeader), 1, ext_file);
        TEST_ASSERT(written == 1, "효과 확장 레이어 헤더 쓰기");

        written = fwrite(layer_data, layer_header.data_size, 1, ext_file);
        TEST_ASSERT(written == 1, "효과 확장 레이어 데이터 쓰기");

        free(layer_data);
    }

    fclose(ext_file);

    // 효과 확장 검증
    ext_file = fopen(TEST_EFFECT_EXT_PATH, "rb");
    TEST_ASSERT(ext_file != NULL, "효과 확장 파일 재열기");

    LEFXHeader read_header;
    size_t read_size = fread(&read_header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(read_size == 1, "효과 확장 헤더 읽기");

    TEST_ASSERT(read_header.extension_type == LEFX_EXT_AUDIO_EFFECT, "효과 확장 타입 검증");
    TEST_ASSERT(read_header.extension_flags & LEFX_FLAG_EFFECT_EXT, "효과 확장 플래그 검증");
    TEST_ASSERT(strcmp(read_header.extension_name, "ReverbEffect") == 0, "효과 이름 검증");

    fclose(ext_file);
    lef_unload_model(base_model);

    printf("오디오 효과 확장 모델 테스트 완료\n");
    return 1;
}

/**
 * 차분 모델 시스템 테스트
 */
static int test_differential_model() {
    printf("\n=== 차분 모델 시스템 테스트 ===\n");

    LEFModel* base_model = lef_load_model(TEST_BASE_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    // 차분 모델 생성 (화자 변형)
    FILE* diff_file = fopen(TEST_DIFF_MODEL_PATH, "wb");
    TEST_ASSERT(diff_file != NULL, "차분 모델 파일 생성");

    // 차분 모델 헤더
    LEFXHeader header = {0};
    lefx_init_header(&header);
    header.extension_type = LEFX_EXT_SPEAKER;
    header.extension_id = 5001;
    header.extension_flags = LEFX_FLAG_SPEAKER_EXT | LEFX_FLAG_DIFFERENTIAL;
    header.base_model_hash = base_model->header.model_hash;
    strcpy(header.base_model_name, base_model->meta.model_name);
    strcpy(header.base_model_version, base_model->meta.model_version);
    strcpy(header.extension_name, "DifferentialSpeaker");
    strcpy(header.extension_author, "LibEtude Research");
    strcpy(header.extension_version, "1.0.0");

    size_t written = fwrite(&header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(written == 1, "차분 모델 헤더 쓰기");

    // 차분 메타데이터
    LEFXExtensionMeta meta = {0};
    lefx_init_extension_meta(&meta);
    strcpy(meta.description, "기본 모델과의 차분을 이용한 효율적인 화자 모델");
    meta.extension_capabilities = LEFX_FLAG_DIFFERENTIAL;
    meta.priority = 150;
    meta.num_layers = 3;
    meta.gender = 0; // 남성
    meta.age_range = 2; // 중년
    meta.quality_score = 0.90f;
    meta.performance_impact = 0.05f; // 차분 모델은 성능 영향 적음

    written = fwrite(&meta, sizeof(LEFXExtensionMeta), 1, diff_file);
    TEST_ASSERT(written == 1, "차분 모델 메타데이터 쓰기");

    // 차분 레이어들 (작은 조정값들만 저장)
    for (int i = 0; i < 3; i++) {
        LEFXLayerHeader layer_header = {0};
        lefx_init_layer_header(&layer_header, i, i);
        layer_header.layer_kind = LEF_LAYER_LINEAR;
        layer_header.quantization_type = LEF_QUANT_INT8; // 차분값은 작으므로 INT8로 충분
        layer_header.blend_mode = 1; // 덧셈 블렌딩 (차분 적용)
        layer_header.data_size = 500; // 차분 데이터는 작음
        layer_header.similarity_threshold = 0.95f; // 높은 유사도
        layer_header.blend_weight = 0.1f; // 작은 조정

        // 차분 데이터 생성 (작은 조정값들)
        int8_t* diff_data = (int8_t*)malloc(layer_header.data_size);
        TEST_ASSERT(diff_data != NULL, "차분 레이어 데이터 할당");

        for (size_t j = 0; j < layer_header.data_size; j++) {
            // 작은 차분값들 (-10 ~ +10 범위)
            diff_data[j] = (int8_t)((rand() % 21) - 10);
        }

        layer_header.checksum = lef_calculate_crc32(diff_data, layer_header.data_size);

        written = fwrite(&layer_header, sizeof(LEFXLayerHeader), 1, diff_file);
        TEST_ASSERT(written == 1, "차분 레이어 헤더 쓰기");

        written = fwrite(diff_data, layer_header.data_size, 1, diff_file);
        TEST_ASSERT(written == 1, "차분 레이어 데이터 쓰기");

        free(diff_data);
    }

    fclose(diff_file);

    // 차분 모델 검증
    diff_file = fopen(TEST_DIFF_MODEL_PATH, "rb");
    TEST_ASSERT(diff_file != NULL, "차분 모델 파일 재열기");

    LEFXHeader read_header;
    size_t read_size = fread(&read_header, sizeof(LEFXHeader), 1, diff_file);
    TEST_ASSERT(read_size == 1, "차분 모델 헤더 읽기");

    TEST_ASSERT(read_header.extension_flags & LEFX_FLAG_DIFFERENTIAL, "차분 모델 플래그 검증");
    TEST_ASSERT(strcmp(read_header.extension_name, "DifferentialSpeaker") == 0, "차분 모델 이름 검증");

    LEFXExtensionMeta read_meta;
    read_size = fread(&read_meta, sizeof(LEFXExtensionMeta), 1, diff_file);
    TEST_ASSERT(read_size == 1, "차분 모델 메타데이터 읽기");

    TEST_ASSERT(read_meta.performance_impact < 0.1f, "차분 모델 성능 영향 확인");
    TEST_ASSERT(read_meta.extension_capabilities & LEFX_FLAG_DIFFERENTIAL, "차분 기능 플래그 확인");

    // 차분 레이어 검증
    for (int i = 0; i < 3; i++) {
        LEFXLayerHeader layer_header;
        read_size = fread(&layer_header, sizeof(LEFXLayerHeader), 1, diff_file);
        TEST_ASSERT(read_size == 1, "차분 레이어 헤더 읽기");

        TEST_ASSERT(layer_header.similarity_threshold > 0.9f, "높은 유사도 임계값 확인");
        TEST_ASSERT(layer_header.blend_weight < 0.2f, "작은 블렌딩 가중치 확인");
        TEST_ASSERT(layer_header.data_size == 500, "차분 데이터 크기 확인");

        // 차분 데이터 읽기 및 검증
        int8_t* diff_data = (int8_t*)malloc(layer_header.data_size);
        TEST_ASSERT(diff_data != NULL, "차분 데이터 메모리 할당");

        read_size = fread(diff_data, layer_header.data_size, 1, diff_file);
        TEST_ASSERT(read_size == 1, "차분 데이터 읽기");

        // 체크섬 검증
        uint32_t checksum = lef_calculate_crc32(diff_data, layer_header.data_size);
        TEST_ASSERT(checksum == layer_header.checksum, "차분 데이터 체크섬 검증");

        free(diff_data);
    }

    fclose(diff_file);
    lef_unload_model(base_model);

    printf("차분 모델 시스템 테스트 완료\n");
    return 1;
}

/**
 * 조건부 활성화 규칙 테스트
 */
static int test_conditional_activation() {
    printf("\n=== 조건부 활성화 규칙 테스트 ===\n");

    // 조건부 활성화 규칙 생성 및 검증
    LEFXActivationRule rule1 = {0};
    lefx_init_activation_rule(&rule1);
    rule1.rule_id = 1;
    rule1.condition_type = 0; // 텍스트 조건
    rule1.operator_type = 1; // 포함 연산자
    strcpy(rule1.condition_value, "안녕하세요");
    rule1.activation_weight = 0.8f;
    rule1.priority = 100;

    TEST_ASSERT(lefx_validate_activation_rule(&rule1), "텍스트 조건 규칙 검증");

    LEFXActivationRule rule2 = {0};
    lefx_init_activation_rule(&rule2);
    rule2.rule_id = 2;
    rule2.condition_type = 1; // 화자 조건
    rule2.operator_type = 0; // 같음 연산자
    strcpy(rule2.condition_value, "female_young");
    rule2.activation_weight = 0.9f;
    rule2.priority = 200;

    TEST_ASSERT(lefx_validate_activation_rule(&rule2), "화자 조건 규칙 검증");

    LEFXActivationRule rule3 = {0};
    lefx_init_activation_rule(&rule3);
    rule3.rule_id = 3;
    rule3.condition_type = 2; // 언어 조건
    rule3.operator_type = 0; // 같음 연산자
    strcpy(rule3.condition_value, "ko");
    rule3.activation_weight = 1.0f;
    rule3.priority = 300;

    TEST_ASSERT(lefx_validate_activation_rule(&rule3), "언어 조건 규칙 검증");

    // 잘못된 규칙 테스트
    LEFXActivationRule invalid_rule = {0};
    lefx_init_activation_rule(&invalid_rule);
    invalid_rule.activation_weight = 1.5f; // 잘못된 가중치 (> 1.0)

    TEST_ASSERT(!lefx_validate_activation_rule(&invalid_rule), "잘못된 규칙 검증 실패");

    printf("조건부 활성화 규칙 테스트 완료\n");
    return 1;
}

/**
 * 의존성 관리 테스트
 */
static int test_dependency_management() {
    printf("\n=== 의존성 관리 테스트 ===\n");

    // 의존성 정보 생성
    LEFXDependency dep1 = {0};
    lefx_init_dependency(&dep1);
    dep1.dependency_id = 2001; // 화자 확장에 의존
    strcpy(dep1.dependency_name, "FemaleVoice01");
    strcpy(dep1.min_version, "1.0.0");
    strcpy(dep1.max_version, "1.9.9");
    dep1.dependency_type = 0; // 필수 의존성
    dep1.load_order = 0; // 먼저 로드

    TEST_ASSERT(lefx_validate_dependency(&dep1), "필수 의존성 검증");

    LEFXDependency dep2 = {0};
    lefx_init_dependency(&dep2);
    dep2.dependency_id = 3001; // 언어 확장에 의존
    strcpy(dep2.dependency_name, "EnglishLanguagePack");
    strcpy(dep2.min_version, "2.0.0");
    strcpy(dep2.max_version, "2.9.9");
    dep2.dependency_type = 1; // 선택적 의존성
    dep2.load_order = 2; // 상관없음

    TEST_ASSERT(lefx_validate_dependency(&dep2), "선택적 의존성 검증");

    // 충돌 의존성 테스트
    LEFXDependency conflict_dep = {0};
    lefx_init_dependency(&conflict_dep);
    conflict_dep.dependency_id = 4001;
    strcpy(conflict_dep.dependency_name, "ConflictingExtension");
    strcpy(conflict_dep.min_version, "1.0.0");
    strcpy(conflict_dep.max_version, "1.9.9");
    conflict_dep.dependency_type = 2; // 충돌
    conflict_dep.load_order = 2;

    TEST_ASSERT(lefx_validate_dependency(&conflict_dep), "충돌 의존성 검증");

    // 잘못된 의존성 테스트
    LEFXDependency invalid_dep = {0};
    lefx_init_dependency(&invalid_dep);
    invalid_dep.dependency_type = 99; // 잘못된 타입

    TEST_ASSERT(!lefx_validate_dependency(&invalid_dep), "잘못된 의존성 검증 실패");

    printf("의존성 관리 테스트 완료\n");
    return 1;
}

/**
 * 플러그인 확장 테스트
 */
static int test_plugin_extension() {
    printf("\n=== 플러그인 확장 테스트 ===\n");

    LEFModel* base_model = lef_load_model(TEST_BASE_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    FILE* plugin_file = fopen(TEST_PLUGIN_EXT_PATH, "wb");
    TEST_ASSERT(plugin_file != NULL, "플러그인 확장 파일 생성");

    // 플러그인 확장 헤더
    LEFXHeader header = {0};
    lefx_init_header(&header);
    header.extension_type = LEFX_EXT_PLUGIN;
    header.extension_id = 6001;
    header.extension_flags = LEFX_FLAG_PLUGIN_EXT;
    header.base_model_hash = base_model->header.model_hash;
    strcpy(header.base_model_name, base_model->meta.model_name);
    strcpy(header.base_model_version, base_model->meta.model_version);
    strcpy(header.extension_name, "CustomVoicePlugin");
    strcpy(header.extension_author, "Third Party Developer");
    strcpy(header.extension_version, "1.0.0");

    header.plugin_data_offset = sizeof(LEFXHeader) + sizeof(LEFXExtensionMeta);

    size_t written = fwrite(&header, sizeof(LEFXHeader), 1, plugin_file);
    TEST_ASSERT(written == 1, "플러그인 확장 헤더 쓰기");

    // 플러그인 메타데이터
    LEFXExtensionMeta meta = {0};
    lefx_init_extension_meta(&meta);
    strcpy(meta.description, "사용자 정의 음성 처리 플러그인");
    strcpy(meta.license, "MIT");
    meta.extension_capabilities = LEFX_FLAG_PLUGIN_EXT;
    meta.priority = 300;
    meta.num_layers = 0; // 플러그인은 레이어가 없을 수 있음
    meta.quality_score = 0.80f;
    meta.performance_impact = 0.4f;

    written = fwrite(&meta, sizeof(LEFXExtensionMeta), 1, plugin_file);
    TEST_ASSERT(written == 1, "플러그인 메타데이터 쓰기");

    // 플러그인 데이터
    LEFXPluginData plugin_data = {0};
    lefx_init_plugin_data(&plugin_data);
    strcpy(plugin_data.plugin_interface, "LibEtudeVoiceProcessor");
    strcpy(plugin_data.plugin_version, "1.0");
    plugin_data.plugin_data_size = 1024;
    plugin_data.plugin_data_offset = ftell(plugin_file) + sizeof(LEFXPluginData);
    plugin_data.init_function_offset = plugin_data.plugin_data_offset + 512;
    plugin_data.process_function_offset = plugin_data.init_function_offset + 256;
    plugin_data.cleanup_function_offset = plugin_data.process_function_offset + 256;

    written = fwrite(&plugin_data, sizeof(LEFXPluginData), 1, plugin_file);
    TEST_ASSERT(written == 1, "플러그인 데이터 구조체 쓰기");

    // 더미 플러그인 바이너리 데이터
    uint8_t* dummy_plugin_data = (uint8_t*)malloc(1024);
    TEST_ASSERT(dummy_plugin_data != NULL, "플러그인 바이너리 데이터 할당");

    for (int i = 0; i < 1024; i++) {
        dummy_plugin_data[i] = (uint8_t)(i % 256);
    }

    written = fwrite(dummy_plugin_data, 1024, 1, plugin_file);
    TEST_ASSERT(written == 1, "플러그인 바이너리 데이터 쓰기");

    free(dummy_plugin_data);
    fclose(plugin_file);

    // 플러그인 확장 검증
    plugin_file = fopen(TEST_PLUGIN_EXT_PATH, "rb");
    TEST_ASSERT(plugin_file != NULL, "플러그인 확장 파일 재열기");

    LEFXHeader read_header;
    size_t read_size = fread(&read_header, sizeof(LEFXHeader), 1, plugin_file);
    TEST_ASSERT(read_size == 1, "플러그인 확장 헤더 읽기");

    TEST_ASSERT(read_header.extension_type == LEFX_EXT_PLUGIN, "플러그인 확장 타입 검증");
    TEST_ASSERT(read_header.extension_flags & LEFX_FLAG_PLUGIN_EXT, "플러그인 확장 플래그 검증");
    TEST_ASSERT(strcmp(read_header.extension_name, "CustomVoicePlugin") == 0, "플러그인 이름 검증");

    LEFXExtensionMeta read_meta;
    read_size = fread(&read_meta, sizeof(LEFXExtensionMeta), 1, plugin_file);
    TEST_ASSERT(read_size == 1, "플러그인 메타데이터 읽기");

    TEST_ASSERT(read_meta.num_layers == 0, "플러그인 레이어 수 확인");
    TEST_ASSERT(read_meta.extension_capabilities & LEFX_FLAG_PLUGIN_EXT, "플러그인 기능 플래그 확인");

    LEFXPluginData read_plugin_data;
    read_size = fread(&read_plugin_data, sizeof(LEFXPluginData), 1, plugin_file);
    TEST_ASSERT(read_size == 1, "플러그인 데이터 구조체 읽기");

    TEST_ASSERT(strcmp(read_plugin_data.plugin_interface, "LibEtudeVoiceProcessor") == 0, "플러그인 인터페이스 확인");
    TEST_ASSERT(read_plugin_data.plugin_data_size == 1024, "플러그인 데이터 크기 확인");

    fclose(plugin_file);
    lef_unload_model(base_model);

    printf("플러그인 확장 테스트 완료\n");
    return 1;
}

/**
 * 확장 모델 호환성 검증 테스트
 */
static int test_extension_compatibility() {
    printf("\n=== 확장 모델 호환성 검증 테스트 ===\n");

    LEFModel* base_model = lef_load_model(TEST_BASE_MODEL_PATH);
    TEST_ASSERT(base_model != NULL, "기본 모델 로드");

    // 호환 가능한 확장 테스트
    FILE* ext_file = fopen(TEST_SPEAKER_EXT_PATH, "rb");
    TEST_ASSERT(ext_file != NULL, "화자 확장 파일 열기");

    LEFXHeader ext_header;
    size_t read_size = fread(&ext_header, sizeof(LEFXHeader), 1, ext_file);
    TEST_ASSERT(read_size == 1, "확장 헤더 읽기");

    // 호환성 검증
    bool compatible = (ext_header.base_model_hash == base_model->header.model_hash) &&
                     (strcmp(ext_header.base_model_name, base_model->meta.model_name) == 0) &&
                     (strcmp(ext_header.base_model_version, base_model->meta.model_version) == 0);

    TEST_ASSERT(compatible, "화자 확장 호환성 검증");

    fclose(ext_file);

    // 비호환 확장 시뮬레이션
    LEFXHeader incompatible_header = ext_header;
    incompatible_header.base_model_hash = 0xDEADBEEF; // 잘못된 해시
    strcpy(incompatible_header.base_model_name, "WrongModel");

    bool incompatible = (incompatible_header.base_model_hash != base_model->header.model_hash) ||
                       (strcmp(incompatible_header.base_model_name, base_model->meta.model_name) != 0);

    TEST_ASSERT(incompatible, "비호환 확장 감지");

    // 버전 호환성 테스트
    LEFXExtensionMeta ext_meta;
    ext_file = fopen(TEST_SPEAKER_EXT_PATH, "rb");
    fseek(ext_file, sizeof(LEFXHeader), SEEK_SET);
    read_size = fread(&ext_meta, sizeof(LEFXExtensionMeta), 1, ext_file);
    TEST_ASSERT(read_size == 1, "확장 메타데이터 읽기");

    // 현재 기본 모델 버전이 요구 범위 내에 있는지 확인
    bool version_compatible = (base_model->header.version_major >= ext_meta.min_base_version_major) &&
                             (base_model->header.version_major <= ext_meta.max_base_version_major);

    if (base_model->header.version_major == ext_meta.min_base_version_major) {
        version_compatible &= (base_model->header.version_minor >= ext_meta.min_base_version_minor);
    }
    if (base_model->header.version_major == ext_meta.max_base_version_major) {
        version_compatible &= (base_model->header.version_minor <= ext_meta.max_base_version_minor);
    }

    TEST_ASSERT(version_compatible, "버전 호환성 검증");

    fclose(ext_file);
    lef_unload_model(base_model);

    printf("확장 모델 호환성 검증 테스트 완료\n");
    return 1;
}

/**
 * 파일 정리 함수
 */
static void cleanup_test_files() {
    unlink(TEST_BASE_MODEL_PATH);
    unlink(TEST_SPEAKER_EXT_PATH);
    unlink(TEST_LANGUAGE_EXT_PATH);
    unlink(TEST_EFFECT_EXT_PATH);
    unlink(TEST_DIFF_MODEL_PATH);
    unlink(TEST_PLUGIN_EXT_PATH);
}

/**
 * 메인 테스트 함수
 */
int main() {
    printf("LibEtude LEFX 확장 모델 포괄적 테스트 시작\n");
    printf("==========================================\n");

    // 테스트 파일 정리 (이전 실행 잔여물)
    cleanup_test_files();

    // 테스트 실행
    int success = 1;

    success &= create_test_base_model();
    success &= test_speaker_extension();
    success &= test_language_extension();
    success &= test_audio_effect_extension();
    success &= test_differential_model();
    success &= test_conditional_activation();
    success &= test_dependency_management();
    success &= test_plugin_extension();
    success &= test_extension_compatibility();

    // 테스트 파일 정리
    cleanup_test_files();

    // 결과 출력
    printf("\n==========================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (success && tests_passed == tests_run) {
        printf("✓ 모든 LEFX 확장 모델 테스트가 성공했습니다!\n");
        return 0;
    } else {
        printf("✗ 일부 테스트가 실패했습니다.\n");
        return 1;
    }
}