/**
 * @file test_onnx_validator_simple.c
 * @brief ONNX 검증 시스템 간단한 단위 테스트
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/onnx_to_lef/core/onnx_validator.h"
#include "../../src/onnx_to_lef/core/onnx_parser.h"

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
 * @brief 기본 설정 테스트
 */
void test_default_config() {
    printf("\n=== 기본 설정 테스트 ===\n");

    ONNXValidationConfig config = onnx_create_default_validation_config();

    TEST_ASSERT(config.min_ir_version == 3, "기본 최소 IR 버전이 3");
    TEST_ASSERT(config.min_opset_version == 11, "기본 최소 Opset 버전이 11");
    TEST_ASSERT(config.max_opset_version == 18, "기본 최대 Opset 버전이 18");
    TEST_ASSERT(config.require_inputs == true, "기본적으로 입력 필수");
    TEST_ASSERT(config.require_outputs == true, "기본적으로 출력 필수");
    TEST_ASSERT(config.require_nodes == true, "기본적으로 노드 필수");
    TEST_ASSERT(config.check_tts_compatibility == true, "기본적으로 TTS 호환성 검사");
    TEST_ASSERT(config.allow_dynamic_shapes == true, "기본적으로 동적 형태 허용");
    TEST_ASSERT(config.include_suggestions == true, "기본적으로 제안 포함");
}

/**
 * @brief 검증 보고서 생성/해제 테스트
 */
void test_validation_report() {
    printf("\n=== 검증 보고서 테스트 ===\n");

    ONNXValidationReport* report = onnx_create_validation_report();

    TEST_ASSERT(report != NULL, "검증 보고서 생성 성공");
    TEST_ASSERT(report->overall_result == ONNX_VALIDATION_SUCCESS, "초기 결과가 성공");
    TEST_ASSERT(report->num_issues == 0, "초기 이슈 개수가 0");
    TEST_ASSERT(report->num_errors == 0, "초기 에러 개수가 0");
    TEST_ASSERT(report->num_warnings == 0, "초기 경고 개수가 0");
    TEST_ASSERT(report->num_infos == 0, "초기 정보 개수가 0");

    // 이슈 추가 테스트
    int result = onnx_add_validation_issue(report, ONNX_VALIDATION_SEVERITY_WARNING,
                                           "테스트 경고", "test_location",
                                           "테스트 제안", 0);

    TEST_ASSERT(result == 0, "이슈 추가 성공");
    TEST_ASSERT(report->num_issues == 1, "이슈 개수가 1로 증가");
    TEST_ASSERT(report->num_warnings == 1, "경고 개수가 1로 증가");
    TEST_ASSERT(report->issues[0].severity == ONNX_VALIDATION_SEVERITY_WARNING,
                "이슈 심각도가 올바름");
    TEST_ASSERT(strcmp(report->issues[0].message, "테스트 경고") == 0,
                "이슈 메시지가 올바름");

    onnx_free_validation_report(report);
}

/**
 * @brief 연산자 지원 여부 테스트
 */
void test_operator_support() {
    printf("\n=== 연산자 지원 여부 테스트 ===\n");

    // 지원되는 연산자 테스트
    TEST_ASSERT(onnx_is_operator_supported("Conv"), "Conv 연산자 지원됨");
    TEST_ASSERT(onnx_is_operator_supported("MatMul"), "MatMul 연산자 지원됨");
    TEST_ASSERT(onnx_is_operator_supported("Relu"), "Relu 연산자 지원됨");
    TEST_ASSERT(onnx_is_operator_supported("LSTM"), "LSTM 연산자 지원됨");

    // 지원되지 않는 연산자 테스트
    TEST_ASSERT(!onnx_is_operator_supported("UnknownOp"), "UnknownOp 연산자 지원되지 않음");
    TEST_ASSERT(!onnx_is_operator_supported("CustomOp"), "CustomOp 연산자 지원되지 않음");
    TEST_ASSERT(!onnx_is_operator_supported(NULL), "NULL 연산자 지원되지 않음");

    // TTS 관련 연산자 테스트
    TEST_ASSERT(onnx_is_tts_related_operator("Conv"), "Conv는 TTS 관련 연산자");
    TEST_ASSERT(onnx_is_tts_related_operator("LSTM"), "LSTM은 TTS 관련 연산자");
    TEST_ASSERT(onnx_is_tts_related_operator("Attention"), "Attention은 TTS 관련 연산자");
    TEST_ASSERT(!onnx_is_tts_related_operator("UnknownOp"), "UnknownOp는 TTS 관련 연산자 아님");
    TEST_ASSERT(!onnx_is_tts_related_operator(NULL), "NULL은 TTS 관련 연산자 아님");
}

/**
 * @brief 버전 검증 테스트
 */
void test_version_validation() {
    printf("\n=== 버전 검증 테스트 ===\n");

    ONNXValidationConfig config = onnx_create_default_validation_config();
    ONNXValidationReport* report = onnx_create_validation_report();

    // 유효한 버전 테스트
    ONNXModel valid_model = {0};
    valid_model.ir_version = 7;
    valid_model.opset_version = 11;

    ONNXValidationResult result = onnx_validate_versions(&valid_model, &config, report);
    TEST_ASSERT(result == ONNX_VALIDATION_SUCCESS, "유효한 버전 검증 성공");
    TEST_ASSERT(report->num_errors == 0, "유효한 버전에서 에러 없음");

    // 낮은 IR 버전 테스트
    onnx_free_validation_report(report);
    report = onnx_create_validation_report();

    ONNXModel low_ir_model = {0};
    low_ir_model.ir_version = 2;
    low_ir_model.opset_version = 11;

    result = onnx_validate_versions(&low_ir_model, &config, report);
    TEST_ASSERT(result == ONNX_VALIDATION_ERROR_UNSUPPORTED_IR, "낮은 IR 버전 검증 실패");
    TEST_ASSERT(report->num_errors > 0, "낮은 IR 버전에서 에러 발생");

    // 낮은 Opset 버전 테스트
    onnx_free_validation_report(report);
    report = onnx_create_validation_report();

    ONNXModel low_opset_model = {0};
    low_opset_model.ir_version = 7;
    low_opset_model.opset_version = 10;

    result = onnx_validate_versions(&low_opset_model, &config, report);
    TEST_ASSERT(result == ONNX_VALIDATION_ERROR_UNSUPPORTED_OPSET, "낮은 Opset 버전 검증 실패");
    TEST_ASSERT(report->num_errors > 0, "낮은 Opset 버전에서 에러 발생");

    onnx_free_validation_report(report);
}

/**
 * @brief 문자열 변환 함수 테스트
 */
void test_string_conversion() {
    printf("\n=== 문자열 변환 테스트 ===\n");

    // 검증 결과 문자열 테스트
    TEST_ASSERT(strcmp(onnx_get_validation_result_string(ONNX_VALIDATION_SUCCESS), "성공") == 0,
                "성공 결과 문자열 올바름");
    TEST_ASSERT(strcmp(onnx_get_validation_result_string(ONNX_VALIDATION_ERROR_NULL_MODEL), "NULL 모델") == 0,
                "NULL 모델 결과 문자열 올바름");

    // 심각도 문자열 테스트
    TEST_ASSERT(strcmp(onnx_get_validation_severity_string(ONNX_VALIDATION_SEVERITY_INFO), "정보") == 0,
                "정보 심각도 문자열 올바름");
    TEST_ASSERT(strcmp(onnx_get_validation_severity_string(ONNX_VALIDATION_SEVERITY_ERROR), "에러") == 0,
                "에러 심각도 문자열 올바름");
}

/**
 * @brief 종합 검증 테스트 (간단한 버전)
 */
void test_comprehensive_validation_simple() {
    printf("\n=== 종합 검증 테스트 (간단) ===\n");

    ONNXValidationConfig config = onnx_create_default_validation_config();
    ONNXValidationReport* report = onnx_create_validation_report();

    // NULL 모델 테스트
    ONNXValidationResult result = onnx_validate_model_comprehensive(NULL, &config, report);
    TEST_ASSERT(result == ONNX_VALIDATION_ERROR_NULL_MODEL, "NULL 모델 검증 실패");
    TEST_ASSERT(report->overall_result == ONNX_VALIDATION_ERROR_NULL_MODEL, "전체 결과가 NULL 모델 에러");

    onnx_free_validation_report(report);
}

/**
 * @brief 메인 테스트 함수
 */
int main() {
    printf("ONNX 검증 시스템 간단한 단위 테스트 시작\n");
    printf("==========================================\n");

    test_default_config();
    test_validation_report();
    test_operator_support();
    test_version_validation();
    test_string_conversion();
    test_comprehensive_validation_simple();

    printf("\n==========================================\n");
    printf("테스트 결과: %d/%d 통과\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("모든 테스트가 성공했습니다! ✓\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다. ✗\n");
        return 1;
    }
}