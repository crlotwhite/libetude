/**
 * @file test_lefx_format.c
 * @brief LEFX 포맷 단위 테스트
 *
 * LEFX 포맷의 구조체 초기화, 검증 등 기본 기능을 테스트합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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

/**
 * LEFX 헤더 초기화 테스트
 */
void test_lefx_header_initialization() {
    printf("\n=== LEFX 헤더 초기화 테스트 ===\n");

    LEFXHeader header;
    lefx_init_header(&header);

    // 매직 넘버 확인
    TEST_ASSERT(header.magic == LEFX_MAGIC, "LEFX 매직 넘버 초기화");

    // 버전 확인
    TEST_ASSERT(header.version_major == LEFX_VERSION_MAJOR, "LEFX 주 버전 초기화");
    TEST_ASSERT(header.version_minor == LEFX_VERSION_MINOR, "LEFX 부 버전 초기화");

    // 기본값 확인
    TEST_ASSERT(header.extension_flags == 0, "확장 플래그 초기화");
    TEST_ASSERT(header.file_size == sizeof(LEFXHeader), "파일 크기 초기화");
    TEST_ASSERT(header.extension_type == LEFX_EXT_CUSTOM, "확장 타입 초기화");
    TEST_ASSERT(header.extension_id == 0, "확장 ID 초기화");

    // 오프셋 확인
    TEST_ASSERT(header.meta_offset == sizeof(LEFXHeader), "메타데이터 오프셋 초기화");
    TEST_ASSERT(header.dependency_offset == 0, "의존성 오프셋 초기화");
    TEST_ASSERT(header.layer_index_offset == 0, "레이어 인덱스 오프셋 초기화");
    TEST_ASSERT(header.layer_data_offset == 0, "레이어 데이터 오프셋 초기화");
    TEST_ASSERT(header.plugin_data_offset == 0, "플러그인 데이터 오프셋 초기화");

    // 타임스탬프 확인 (0이 아니어야 함)
    TEST_ASSERT(header.timestamp > 0, "타임스탬프 초기화");
}

/**
 * LEFX 확장 메타데이터 초기화 테스트
 */
void test_lefx_extension_meta_initialization() {
    printf("\n=== LEFX 확장 메타데이터 초기화 테스트 ===\n");

    LEFXExtensionMeta meta;
    lefx_init_extension_meta(&meta);

    // 호환성 정보 확인
    TEST_ASSERT(meta.min_base_version_major == 0, "최소 기본 모델 주 버전 초기화");
    TEST_ASSERT(meta.min_base_version_minor == 0, "최소 기본 모델 부 버전 초기화");
    TEST_ASSERT(meta.max_base_version_major == 65535, "최대 기본 모델 주 버전 초기화");
    TEST_ASSERT(meta.max_base_version_minor == 65535, "최대 기본 모델 부 버전 초기화");

    // 확장 특성 정보 확인
    TEST_ASSERT(meta.extension_capabilities == 0, "확장 기능 플래그 초기화");
    TEST_ASSERT(meta.priority == 1000, "우선순위 초기화");
    TEST_ASSERT(meta.num_layers == 0, "레이어 수 초기화");
    TEST_ASSERT(meta.total_params == 0, "총 파라미터 수 초기화");
    TEST_ASSERT(meta.memory_requirement == 0, "메모리 요구사항 초기화");

    // 음성 특화 정보 확인 (해당없음으로 초기화)
    TEST_ASSERT(meta.gender == 255, "성별 초기화");
    TEST_ASSERT(meta.age_range == 255, "연령대 초기화");

    // 품질 및 성능 정보 확인
    TEST_ASSERT(meta.quality_score == 0.5f, "품질 점수 초기화");
    TEST_ASSERT(meta.performance_impact == 0.1f, "성능 영향도 초기화");
    TEST_ASSERT(meta.inference_time_ms == 0, "추론 시간 초기화");
    TEST_ASSERT(meta.loading_time_ms == 0, "로딩 시간 초기화");
}

/**
 * LEFX 레이어 헤더 초기화 테스트
 */
void test_lefx_layer_header_initialization() {
    printf("\n=== LEFX 레이어 헤더 초기화 테스트 ===\n");

    LEFXLayerHeader layer_header;
    uint16_t extension_layer_id = 100;
    uint16_t base_layer_id = 50;

    lefx_init_layer_header(&layer_header, extension_layer_id, base_layer_id);

    // 레이어 ID 확인
    TEST_ASSERT(layer_header.extension_layer_id == extension_layer_id, "확장 레이어 ID 초기화");
    TEST_ASSERT(layer_header.base_layer_id == base_layer_id, "기본 레이어 ID 초기화");

    // 레이어 타입 및 설정 확인
    TEST_ASSERT(layer_header.layer_kind == LEF_LAYER_CUSTOM, "레이어 타입 초기화");
    TEST_ASSERT(layer_header.quantization_type == LEF_QUANT_NONE, "양자화 타입 초기화");
    TEST_ASSERT(layer_header.blend_mode == 0, "블렌딩 모드 초기화");
    TEST_ASSERT(layer_header.activation_condition == 0, "활성화 조건 초기화");

    // 데이터 크기 정보 확인
    TEST_ASSERT(layer_header.meta_size == 0, "메타데이터 크기 초기화");
    TEST_ASSERT(layer_header.data_size == 0, "데이터 크기 초기화");
    TEST_ASSERT(layer_header.compressed_size == 0, "압축된 크기 초기화");
    TEST_ASSERT(layer_header.data_offset == 0, "데이터 오프셋 초기화");
    TEST_ASSERT(layer_header.checksum == 0, "체크섬 초기화");

    // 차분 모델 정보 확인
    TEST_ASSERT(layer_header.similarity_threshold == 0.0f, "유사도 임계값 초기화");
    TEST_ASSERT(layer_header.blend_weight == 1.0f, "블렌딩 가중치 초기화");
    TEST_ASSERT(layer_header.dependency_count == 0, "의존성 수 초기화");
    TEST_ASSERT(layer_header.reserved_flags == 0, "예약 플래그 초기화");
}

/**
 * LEFX 의존성 정보 초기화 테스트
 */
void test_lefx_dependency_initialization() {
    printf("\n=== LEFX 의존성 정보 초기화 테스트 ===\n");

    LEFXDependency dependency;
    lefx_init_dependency(&dependency);

    // 기본값 확인
    TEST_ASSERT(dependency.dependency_id == 0, "의존성 ID 초기화");
    TEST_ASSERT(dependency.dependency_type == 0, "의존성 타입 초기화 (필수)");
    TEST_ASSERT(dependency.load_order == 2, "로드 순서 초기화 (상관없음)");

    // 문자열 필드가 0으로 초기화되었는지 확인
    TEST_ASSERT(strlen(dependency.dependency_name) == 0, "의존성 이름 초기화");
    TEST_ASSERT(strlen(dependency.min_version) == 0, "최소 버전 초기화");
    TEST_ASSERT(strlen(dependency.max_version) == 0, "최대 버전 초기화");
}

/**
 * LEFX 활성화 규칙 초기화 테스트
 */
void test_lefx_activation_rule_initialization() {
    printf("\n=== LEFX 활성화 규칙 초기화 테스트 ===\n");

    LEFXActivationRule rule;
    lefx_init_activation_rule(&rule);

    // 기본값 확인
    TEST_ASSERT(rule.rule_id == 0, "규칙 ID 초기화");
    TEST_ASSERT(rule.condition_type == 0, "조건 타입 초기화 (텍스트)");
    TEST_ASSERT(rule.operator_type == 0, "연산자 타입 초기화 (같음)");
    TEST_ASSERT(rule.activation_weight == 1.0f, "활성화 가중치 초기화");
    TEST_ASSERT(rule.priority == 100, "우선순위 초기화");

    // 문자열 필드 확인
    TEST_ASSERT(strlen(rule.condition_value) == 0, "조건 값 초기화");
}

/**
 * LEFX 플러그인 데이터 초기화 테스트
 */
void test_lefx_plugin_data_initialization() {
    printf("\n=== LEFX 플러그인 데이터 초기화 테스트 ===\n");

    LEFXPluginData plugin_data;
    lefx_init_plugin_data(&plugin_data);

    // 기본값 확인
    TEST_ASSERT(plugin_data.plugin_data_size == 0, "플러그인 데이터 크기 초기화");
    TEST_ASSERT(plugin_data.plugin_data_offset == 0, "플러그인 데이터 오프셋 초기화");
    TEST_ASSERT(plugin_data.init_function_offset == 0, "초기화 함수 오프셋 초기화");
    TEST_ASSERT(plugin_data.process_function_offset == 0, "처리 함수 오프셋 초기화");
    TEST_ASSERT(plugin_data.cleanup_function_offset == 0, "정리 함수 오프셋 초기화");

    // 문자열 필드 확인
    TEST_ASSERT(strlen(plugin_data.plugin_interface) == 0, "플러그인 인터페이스 초기화");
    TEST_ASSERT(strlen(plugin_data.plugin_version) == 0, "플러그인 버전 초기화");
}

/**
 * LEFX 헤더 검증 테스트
 */
void test_lefx_header_validation() {
    printf("\n=== LEFX 헤더 검증 테스트 ===\n");

    LEFXHeader header;
    lefx_init_header(&header);

    // 기본 모델 정보 설정 (검증을 위해 필요)
    strcpy(header.base_model_name, "test_base_model");
    strcpy(header.base_model_version, "1.0.0");
    strcpy(header.extension_name, "test_extension");
    strcpy(header.extension_version, "1.0.0");

    // 유효한 헤더 검증
    TEST_ASSERT(lefx_validate_header(&header), "유효한 LEFX 헤더 검증");

    // 잘못된 매직 넘버 테스트
    uint32_t original_magic = header.magic;
    header.magic = 0x12345678;
    TEST_ASSERT(!lefx_validate_header(&header), "잘못된 매직 넘버 검증");
    header.magic = original_magic;

    // 잘못된 파일 크기 테스트
    uint32_t original_file_size = header.file_size;
    header.file_size = sizeof(LEFXHeader) - 1;
    TEST_ASSERT(!lefx_validate_header(&header), "잘못된 파일 크기 검증");
    header.file_size = original_file_size;

    // 잘못된 오프셋 테스트
    uint32_t original_meta_offset = header.meta_offset;
    header.meta_offset = sizeof(LEFXHeader) - 1;
    TEST_ASSERT(!lefx_validate_header(&header), "잘못된 메타데이터 오프셋 검증");
    header.meta_offset = original_meta_offset;

    // NULL 포인터 테스트
    TEST_ASSERT(!lefx_validate_header(NULL), "NULL 헤더 포인터 검증");
}

/**
 * LEFX 확장 메타데이터 검증 테스트
 */
void test_lefx_extension_meta_validation() {
    printf("\n=== LEFX 확장 메타데이터 검증 테스트 ===\n");

    LEFXExtensionMeta meta;
    lefx_init_extension_meta(&meta);

    // 유효한 메타데이터 검증
    TEST_ASSERT(lefx_validate_extension_meta(&meta), "유효한 확장 메타데이터 검증");

    // 잘못된 호환성 정보 테스트
    meta.min_base_version_major = 2;
    meta.max_base_version_major = 1;
    TEST_ASSERT(!lefx_validate_extension_meta(&meta), "잘못된 버전 호환성 검증");

    // 원래 값으로 복원
    lefx_init_extension_meta(&meta);

    // 잘못된 품질 점수 테스트
    meta.quality_score = 1.5f;
    TEST_ASSERT(!lefx_validate_extension_meta(&meta), "잘못된 품질 점수 검증");
    meta.quality_score = 0.5f;

    // 잘못된 성능 영향도 테스트
    meta.performance_impact = -0.1f;
    TEST_ASSERT(!lefx_validate_extension_meta(&meta), "잘못된 성능 영향도 검증");
    meta.performance_impact = 0.1f;

    // NULL 포인터 테스트
    TEST_ASSERT(!lefx_validate_extension_meta(NULL), "NULL 메타데이터 포인터 검증");
}

/**
 * LEFX 레이어 헤더 검증 테스트
 */
void test_lefx_layer_header_validation() {
    printf("\n=== LEFX 레이어 헤더 검증 테스트 ===\n");

    LEFXLayerHeader layer_header;
    lefx_init_layer_header(&layer_header, 100, 50);

    // 유효한 레이어 헤더 검증
    TEST_ASSERT(lefx_validate_layer_header(&layer_header), "유효한 레이어 헤더 검증");

    // 잘못된 블렌딩 모드 테스트
    layer_header.blend_mode = 4;
    TEST_ASSERT(!lefx_validate_layer_header(&layer_header), "잘못된 블렌딩 모드 검증");
    layer_header.blend_mode = 0;

    // 잘못된 블렌딩 가중치 테스트
    layer_header.blend_weight = 1.5f;
    TEST_ASSERT(!lefx_validate_layer_header(&layer_header), "잘못된 블렌딩 가중치 검증");
    layer_header.blend_weight = 1.0f;

    // 잘못된 유사도 임계값 테스트
    layer_header.similarity_threshold = -0.1f;
    TEST_ASSERT(!lefx_validate_layer_header(&layer_header), "잘못된 유사도 임계값 검증");
    layer_header.similarity_threshold = 0.0f;

    // NULL 포인터 테스트
    TEST_ASSERT(!lefx_validate_layer_header(NULL), "NULL 레이어 헤더 포인터 검증");
}

/**
 * 메인 테스트 함수
 */
int main() {
    printf("LEFX 포맷 단위 테스트 시작\n");
    printf("========================================\n");

    // 초기화 테스트 실행
    test_lefx_header_initialization();
    test_lefx_extension_meta_initialization();
    test_lefx_layer_header_initialization();
    test_lefx_dependency_initialization();
    test_lefx_activation_rule_initialization();
    test_lefx_plugin_data_initialization();

    // 검증 테스트 실행
    test_lefx_header_validation();
    test_lefx_extension_meta_validation();
    test_lefx_layer_header_validation();

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