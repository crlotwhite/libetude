#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "libetude/lef_format.h"

// 테스트 결과 카운터
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            printf("✗ %s\n", message); \
        } \
    } while(0)

/**
 * LEF 헤더 초기화 및 검증 테스트
 */
void test_lef_header_init_and_validation() {
    printf("\n=== LEF Header 초기화 및 검증 테스트 ===\n");

    LEFHeader header;
    lef_init_header(&header);

    // 초기화 검증
    TEST_ASSERT(header.magic == LEF_MAGIC, "헤더 매직 넘버 초기화");
    TEST_ASSERT(header.version_major == LEF_VERSION_MAJOR, "주 버전 초기화");
    TEST_ASSERT(header.version_minor == LEF_VERSION_MINOR, "부 버전 초기화");
    TEST_ASSERT(header.timestamp > 0, "타임스탬프 초기화");

    // 헤더 검증 테스트
    TEST_ASSERT(lef_validate_header(&header) == false, "불완전한 헤더 검증 실패");

    // 완전한 헤더 설정
    header.file_size = sizeof(LEFHeader) + sizeof(LEFModelMeta) + 1024;
    header.layer_index_offset = sizeof(LEFHeader) + sizeof(LEFModelMeta);
    header.layer_data_offset = header.layer_index_offset + 256;
    header.model_hash = 0x12345678;

    TEST_ASSERT(lef_validate_header(&header) == true, "완전한 헤더 검증 성공");

    // 잘못된 매직 넘버 테스트
    header.magic = 0xDEADBEEF;
    TEST_ASSERT(lef_validate_header(&header) == false, "잘못된 매직 넘버 검증 실패");

    // 잘못된 버전 테스트
    header.magic = LEF_MAGIC;
    header.version_major = 999;
    TEST_ASSERT(lef_validate_header(&header) == false, "잘못된 버전 검증 실패");
}

/**
 * 모델 메타데이터 초기화 및 검증 테스트
 */
void test_model_meta_init_and_validation() {
    printf("\n=== 모델 메타데이터 초기화 및 검증 테스트 ===\n");

    LEFModelMeta meta;
    lef_init_model_meta(&meta);

    // 초기화 검증
    TEST_ASSERT(strlen(meta.model_name) > 0, "모델 이름 초기화");
    TEST_ASSERT(strlen(meta.model_version) > 0, "모델 버전 초기화");
    TEST_ASSERT(meta.input_dim > 0, "입력 차원 초기화");
    TEST_ASSERT(meta.output_dim > 0, "출력 차원 초기화");
    TEST_ASSERT(meta.sample_rate > 0, "샘플링 레이트 초기화");
    TEST_ASSERT(meta.mel_channels > 0, "Mel 채널 수 초기화");

    // 메타데이터 검증 테스트
    TEST_ASSERT(lef_validate_model_meta(&meta) == true, "기본 메타데이터 검증 성공");

    // 잘못된 아키텍처 정보 테스트
    meta.input_dim = 0;
    TEST_ASSERT(lef_validate_model_meta(&meta) == false, "잘못된 입력 차원 검증 실패");

    meta.input_dim = 256;
    meta.sample_rate = 0;
    TEST_ASSERT(lef_validate_model_meta(&meta) == false, "잘못된 샘플링 레이트 검증 실패");

    // hop_length > win_length 테스트
    meta.sample_rate = 22050;
    meta.hop_length = 2048;
    meta.win_length = 1024;
    TEST_ASSERT(lef_validate_model_meta(&meta) == false, "잘못된 hop/win length 검증 실패");

    // 빈 모델 이름 테스트
    meta.hop_length = 256;
    meta.win_length = 1024;
    meta.model_name[0] = '\0';
    TEST_ASSERT(lef_validate_model_meta(&meta) == false, "빈 모델 이름 검증 실패");
}

/**
 * 레이어 헤더 초기화 및 검증 테스트
 */
void test_layer_header_init_and_validation() {
    printf("\n=== 레이어 헤더 초기화 및 검증 테스트 ===\n");

    LEFLayerHeader layer_header;
    lef_init_layer_header(&layer_header, 1, LEF_LAYER_LINEAR);

    // 초기화 검증
    TEST_ASSERT(layer_header.layer_id == 1, "레이어 ID 초기화");
    TEST_ASSERT(layer_header.layer_kind == LEF_LAYER_LINEAR, "레이어 타입 초기화");
    TEST_ASSERT(layer_header.quantization_type == LEF_QUANT_NONE, "양자화 타입 초기화");

    // 검증을 위해 필수 필드 설정
    layer_header.data_size = 1024;
    layer_header.data_offset = 2048;
    layer_header.checksum = 0x12345678;

    TEST_ASSERT(lef_validate_layer_header(&layer_header) == true, "기본 레이어 헤더 검증 성공");

    // 잘못된 레이어 타입 테스트
    layer_header.layer_kind = 255;  // LEF_LAYER_CUSTOM보다 큰 값은 아니지만 경계값 테스트
    TEST_ASSERT(lef_validate_layer_header(&layer_header) == true, "커스텀 레이어 타입 검증 성공");

    layer_header.layer_kind = 0;  // 유효한 값으로 복원

    // 잘못된 데이터 크기 테스트
    layer_header.data_size = 0;
    TEST_ASSERT(lef_validate_layer_header(&layer_header) == false, "잘못된 데이터 크기 검증 실패");

    // 잘못된 압축 크기 테스트
    layer_header.data_size = 1024;
    layer_header.compressed_size = 2048;  // 원본보다 큰 압축 크기
    TEST_ASSERT(lef_validate_layer_header(&layer_header) == false, "잘못된 압축 크기 검증 실패");

    // 올바른 압축 크기 테스트
    layer_header.compressed_size = 512;  // 원본보다 작은 압축 크기
    TEST_ASSERT(lef_validate_layer_header(&layer_header) == true, "올바른 압축 크기 검증 성공");
}

/**
 * CRC32 체크섬 계산 테스트
 */
void test_crc32_calculation() {
    printf("\n=== CRC32 체크섬 계산 테스트 ===\n");

    // 알려진 테스트 벡터
    const char* test_data = "Hello, LibEtude!";
    uint32_t crc = lef_calculate_crc32(test_data, strlen(test_data));

    TEST_ASSERT(crc != 0, "CRC32 계산 결과가 0이 아님");

    // 같은 데이터에 대해 같은 결과 반환
    uint32_t crc2 = lef_calculate_crc32(test_data, strlen(test_data));
    TEST_ASSERT(crc == crc2, "동일한 데이터에 대해 동일한 CRC32 값");

    // 다른 데이터에 대해 다른 결과 반환
    const char* test_data2 = "Hello, LibEtude?";
    uint32_t crc3 = lef_calculate_crc32(test_data2, strlen(test_data2));
    TEST_ASSERT(crc != crc3, "다른 데이터에 대해 다른 CRC32 값");

    // NULL 포인터 처리
    uint32_t crc_null = lef_calculate_crc32(NULL, 100);
    TEST_ASSERT(crc_null == 0, "NULL 포인터에 대해 0 반환");

    // 크기 0 처리
    uint32_t crc_zero = lef_calculate_crc32(test_data, 0);
    TEST_ASSERT(crc_zero == 0, "크기 0에 대해 0 반환");
}

/**
 * 모델 해시 계산 테스트
 */
void test_model_hash_calculation() {
    printf("\n=== 모델 해시 계산 테스트 ===\n");

    LEFModelMeta meta1, meta2;
    lef_init_model_meta(&meta1);
    lef_init_model_meta(&meta2);

    // 동일한 메타데이터에 대해 동일한 해시
    uint32_t hash1 = lef_calculate_model_hash(&meta1);
    uint32_t hash2 = lef_calculate_model_hash(&meta2);
    TEST_ASSERT(hash1 == hash2, "동일한 메타데이터에 대해 동일한 해시");
    TEST_ASSERT(hash1 != 0, "해시 값이 0이 아님");

    // 다른 모델 이름에 대해 다른 해시
    strcpy(meta2.model_name, "different_model");
    uint32_t hash3 = lef_calculate_model_hash(&meta2);
    TEST_ASSERT(hash1 != hash3, "다른 모델 이름에 대해 다른 해시");

    // 다른 아키텍처에 대해 다른 해시
    strcpy(meta2.model_name, meta1.model_name);  // 이름 복원
    meta2.hidden_dim = 1024;  // 다른 은닉 차원
    uint32_t hash4 = lef_calculate_model_hash(&meta2);
    TEST_ASSERT(hash1 != hash4, "다른 아키텍처에 대해 다른 해시");

    // NULL 포인터 처리
    uint32_t hash_null = lef_calculate_model_hash(NULL);
    TEST_ASSERT(hash_null == 0, "NULL 포인터에 대해 0 반환");
}

/**
 * 구조체 크기 및 패킹 테스트
 */
void test_struct_sizes_and_packing() {
    printf("\n=== 구조체 크기 및 패킹 테스트 ===\n");

    // 구조체 크기가 예상과 일치하는지 확인 (실제 컴파일러 패딩 고려)
    TEST_ASSERT(sizeof(LEFHeader) == 56, "LEFHeader 크기 확인");
    TEST_ASSERT(sizeof(LEFModelMeta) == 296, "LEFModelMeta 크기 확인");
    TEST_ASSERT(sizeof(LEFLayerHeader) == 24, "LEFLayerHeader 크기 확인");
    TEST_ASSERT(sizeof(LEFLayerIndexEntry) == 14, "LEFLayerIndexEntry 크기 확인");
    TEST_ASSERT(sizeof(LEFCompressionDict) == 16, "LEFCompressionDict 크기 확인");
    TEST_ASSERT(sizeof(LEFQuantizationParams) == 20, "LEFQuantizationParams 크기 확인");

    // 구조체가 올바르게 패킹되었는지 확인 (패딩 없음)
    LEFHeader header;
    char* header_bytes = (char*)&header;

    // magic 필드 오프셋 확인
    TEST_ASSERT((char*)&header.magic - header_bytes == 0, "magic 필드 오프셋");

    // version_major 필드 오프셋 확인
    TEST_ASSERT((char*)&header.version_major - header_bytes == 4, "version_major 필드 오프셋");

    // timestamp 필드 오프셋 확인
    TEST_ASSERT((char*)&header.timestamp - header_bytes == 20, "timestamp 필드 오프셋");
}

/**
 * 열거형 값 테스트
 */
void test_enum_values() {
    printf("\n=== 열거형 값 테스트 ===\n");

    // 양자화 타입 열거형
    TEST_ASSERT(LEF_QUANT_NONE == 0, "LEF_QUANT_NONE 값");
    TEST_ASSERT(LEF_QUANT_FP16 == 1, "LEF_QUANT_FP16 값");
    TEST_ASSERT(LEF_QUANT_BF16 == 2, "LEF_QUANT_BF16 값");
    TEST_ASSERT(LEF_QUANT_INT8 == 3, "LEF_QUANT_INT8 값");
    TEST_ASSERT(LEF_QUANT_INT4 == 4, "LEF_QUANT_INT4 값");
    TEST_ASSERT(LEF_QUANT_MIXED == 5, "LEF_QUANT_MIXED 값");

    // 레이어 타입 열거형
    TEST_ASSERT(LEF_LAYER_LINEAR == 0, "LEF_LAYER_LINEAR 값");
    TEST_ASSERT(LEF_LAYER_CONV1D == 1, "LEF_LAYER_CONV1D 값");
    TEST_ASSERT(LEF_LAYER_ATTENTION == 2, "LEF_LAYER_ATTENTION 값");
    TEST_ASSERT(LEF_LAYER_CUSTOM == 255, "LEF_LAYER_CUSTOM 값");

    // 플래그 값
    TEST_ASSERT(LEF_FLAG_COMPRESSED == (1 << 0), "LEF_FLAG_COMPRESSED 값");
    TEST_ASSERT(LEF_FLAG_QUANTIZED == (1 << 1), "LEF_FLAG_QUANTIZED 값");
    TEST_ASSERT(LEF_FLAG_EXTENDED == (1 << 2), "LEF_FLAG_EXTENDED 값");
}

/**
 * NULL 포인터 안전성 테스트
 */
void test_null_pointer_safety() {
    printf("\n=== NULL 포인터 안전성 테스트 ===\n");

    // 모든 함수가 NULL 포인터를 안전하게 처리하는지 확인
    TEST_ASSERT(lef_validate_header(NULL) == false, "lef_validate_header NULL 안전성");
    TEST_ASSERT(lef_validate_model_meta(NULL) == false, "lef_validate_model_meta NULL 안전성");
    TEST_ASSERT(lef_validate_layer_header(NULL) == false, "lef_validate_layer_header NULL 안전성");
    TEST_ASSERT(lef_calculate_model_hash(NULL) == 0, "lef_calculate_model_hash NULL 안전성");

    // 초기화 함수들의 NULL 안전성 (크래시하지 않아야 함)
    lef_init_header(NULL);  // 크래시하지 않으면 성공
    lef_init_model_meta(NULL);  // 크래시하지 않으면 성공
    lef_init_layer_header(NULL, 0, LEF_LAYER_LINEAR);  // 크래시하지 않으면 성공

    TEST_ASSERT(true, "초기화 함수들의 NULL 안전성");

    // 직렬화 함수들의 NULL 안전성
    TEST_ASSERT(lef_create_serialization_context(NULL) == NULL, "lef_create_serialization_context NULL 안전성");
    TEST_ASSERT(lef_set_model_info(NULL, "test", "1.0", NULL, NULL) == LEF_ERROR_INVALID_ARGUMENT,
                "lef_set_model_info NULL 컨텍스트 안전성");
    TEST_ASSERT(lef_verify_file_integrity(NULL) == LEF_ERROR_INVALID_ARGUMENT,
                "lef_verify_file_integrity NULL 안전성");
}

/**
 * 모델 직렬화 컨텍스트 테스트
 */
void test_serialization_context() {
    printf("\n=== 모델 직렬화 컨텍스트 테스트 ===\n");

    const char* test_filename = "test_model.lef";

    // 컨텍스트 생성
    LEFSerializationContext* ctx = lef_create_serialization_context(test_filename);
    TEST_ASSERT(ctx != NULL, "직렬화 컨텍스트 생성");

    if (ctx) {
        // 기본 설정 확인
        TEST_ASSERT(ctx->file != NULL, "파일 포인터 초기화");
        TEST_ASSERT(ctx->num_layers == 0, "레이어 수 초기화");
        TEST_ASSERT(ctx->layer_capacity == 16, "레이어 용량 초기화");
        TEST_ASSERT(ctx->compression_enabled == false, "압축 기본값");
        TEST_ASSERT(ctx->checksum_enabled == true, "체크섬 기본값");

        // 모델 정보 설정
        int result = lef_set_model_info(ctx, "TestModel", "1.0.0", "TestAuthor", "Test Description");
        TEST_ASSERT(result == LEF_SUCCESS, "모델 정보 설정");

        // 아키텍처 정보 설정
        result = lef_set_model_architecture(ctx, 256, 80, 512, 6, 8, 1000);
        TEST_ASSERT(result == LEF_SUCCESS, "아키텍처 정보 설정");

        // 오디오 설정
        result = lef_set_audio_config(ctx, 22050, 80, 256, 1024);
        TEST_ASSERT(result == LEF_SUCCESS, "오디오 설정");

        // 압축 설정
        result = lef_enable_compression(ctx, 6);
        TEST_ASSERT(result == LEF_SUCCESS, "압축 활성화");
        TEST_ASSERT(ctx->compression_enabled == true, "압축 상태 확인");

        result = lef_disable_compression(ctx);
        TEST_ASSERT(result == LEF_SUCCESS, "압축 비활성화");
        TEST_ASSERT(ctx->compression_enabled == false, "압축 상태 확인");

        // 양자화 설정
        result = lef_set_default_quantization(ctx, LEF_QUANT_BF16);
        TEST_ASSERT(result == LEF_SUCCESS, "양자화 설정");
        TEST_ASSERT(ctx->meta.default_quantization == LEF_QUANT_BF16, "양자화 타입 확인");

        lef_destroy_serialization_context(ctx);
    }

    // 파일 정리
    remove(test_filename);
}

/**
 * 레이어 추가 및 저장 테스트
 */
void test_layer_serialization() {
    printf("\n=== 레이어 직렬화 테스트 ===\n");

    const char* test_filename = "test_layers.lef";

    LEFSerializationContext* ctx = lef_create_serialization_context(test_filename);
    TEST_ASSERT(ctx != NULL, "직렬화 컨텍스트 생성");

    if (ctx) {
        // 모델 기본 정보 설정
        lef_set_model_info(ctx, "LayerTest", "1.0.0", "Test", "Layer test model");
        lef_set_model_architecture(ctx, 256, 80, 512, 2, 8, 1000);
        lef_set_audio_config(ctx, 22050, 80, 256, 1024);

        // 테스트 레이어 데이터 생성
        float test_weights1[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        float test_weights2[] = {6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f};

        LEFLayerData layer1 = {
            .layer_id = 1,
            .layer_kind = LEF_LAYER_LINEAR,
            .quant_type = LEF_QUANT_NONE,
            .layer_meta = NULL,
            .meta_size = 0,
            .weight_data = test_weights1,
            .data_size = sizeof(test_weights1),
            .quant_params = NULL
        };

        LEFLayerData layer2 = {
            .layer_id = 2,
            .layer_kind = LEF_LAYER_ATTENTION,
            .quant_type = LEF_QUANT_BF16,
            .layer_meta = NULL,
            .meta_size = 0,
            .weight_data = test_weights2,
            .data_size = sizeof(test_weights2),
            .quant_params = NULL
        };

        // 레이어 추가
        int result = lef_add_layer(ctx, &layer1);
        TEST_ASSERT(result == LEF_SUCCESS, "첫 번째 레이어 추가");
        TEST_ASSERT(ctx->num_layers == 1, "레이어 수 확인");

        result = lef_add_layer(ctx, &layer2);
        TEST_ASSERT(result == LEF_SUCCESS, "두 번째 레이어 추가");
        TEST_ASSERT(ctx->num_layers == 2, "레이어 수 확인");

        // 레이어 헤더 확인
        TEST_ASSERT(ctx->layer_headers[0].layer_id == 1, "첫 번째 레이어 ID");
        TEST_ASSERT(ctx->layer_headers[0].layer_kind == LEF_LAYER_LINEAR, "첫 번째 레이어 타입");
        TEST_ASSERT(ctx->layer_headers[1].layer_id == 2, "두 번째 레이어 ID");
        TEST_ASSERT(ctx->layer_headers[1].layer_kind == LEF_LAYER_ATTENTION, "두 번째 레이어 타입");

        // 모델 완료
        result = lef_finalize_model(ctx);
        TEST_ASSERT(result == LEF_SUCCESS, "모델 완료");

        lef_destroy_serialization_context(ctx);

        // 파일 무결성 검증
        result = lef_verify_file_integrity(test_filename);
        TEST_ASSERT(result == LEF_SUCCESS, "파일 무결성 검증");
    }

    // 파일 정리
    remove(test_filename);
}

/**
 * 버전 관리 테스트
 */
void test_version_management() {
    printf("\n=== 버전 관리 테스트 ===\n");

    // 현재 호환성 정보 확인
    LEFVersionCompatibility compat = lef_get_current_compatibility();
    TEST_ASSERT(compat.min_major == 1, "최소 주 버전");
    TEST_ASSERT(compat.min_minor == 0, "최소 부 버전");
    TEST_ASSERT(compat.max_major == 1, "최대 주 버전");
    TEST_ASSERT(compat.max_minor == 0, "최대 부 버전");

    // 버전 호환성 확인
    TEST_ASSERT(lef_check_version_compatibility(1, 0, &compat) == true, "현재 버전 호환성");
    TEST_ASSERT(lef_check_version_compatibility(0, 9, &compat) == false, "이전 버전 비호환성");
    TEST_ASSERT(lef_check_version_compatibility(2, 0, &compat) == false, "미래 버전 비호환성");

    // 버전 문자열 확인
    const char* version_str = lef_get_version_string();
    TEST_ASSERT(version_str != NULL, "버전 문자열 존재");
    TEST_ASSERT(strcmp(version_str, "1.0") == 0, "버전 문자열 내용");
}

/**
 * 에러 처리 테스트
 */
void test_error_handling() {
    printf("\n=== 에러 처리 테스트 ===\n");

    // 에러 메시지 확인
    TEST_ASSERT(strcmp(lef_get_error_string(LEF_SUCCESS), "성공") == 0, "성공 메시지");
    TEST_ASSERT(strcmp(lef_get_error_string(LEF_ERROR_INVALID_ARGUMENT), "잘못된 인수") == 0, "잘못된 인수 메시지");
    TEST_ASSERT(strcmp(lef_get_error_string(LEF_ERROR_FILE_IO), "파일 입출력 오류") == 0, "파일 IO 오류 메시지");

    // 잘못된 인수 처리
    LEFSerializationContext* ctx = lef_create_serialization_context("test.lef");
    if (ctx) {
        // 잘못된 모델 정보
        int result = lef_set_model_info(ctx, NULL, "1.0", NULL, NULL);
        TEST_ASSERT(result == LEF_ERROR_INVALID_ARGUMENT, "NULL 모델 이름 에러");

        // 잘못된 아키텍처 정보
        result = lef_set_model_architecture(ctx, 0, 80, 512, 6, 8, 1000);
        TEST_ASSERT(result == LEF_ERROR_INVALID_ARGUMENT, "잘못된 입력 차원 에러");

        // 잘못된 오디오 설정
        result = lef_set_audio_config(ctx, 22050, 80, 2048, 1024);  // hop > win
        TEST_ASSERT(result == LEF_ERROR_INVALID_ARGUMENT, "잘못된 hop/win 길이 에러");

        lef_destroy_serialization_context(ctx);
    }

    remove("test.lef");
}

int main() {
    printf("LibEtude LEF Format 단위 테스트 시작\n");
    printf("=====================================\n");

    // 기본 기능 테스트
    test_lef_header_init_and_validation();
    test_model_meta_init_and_validation();
    test_layer_header_init_and_validation();
    test_crc32_calculation();
    test_model_hash_calculation();
    test_struct_sizes_and_packing();
    test_enum_values();
    test_null_pointer_safety();

    // 모델 직렬화 테스트
    test_serialization_context();
    test_layer_serialization();
    test_version_management();
    test_error_handling();

    printf("\n=====================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("✓ 모든 테스트 통과!\n");
        return 0;
    } else {
        printf("✗ %d개 테스트 실패\n", tests_run - tests_passed);
        return 1;
    }
}