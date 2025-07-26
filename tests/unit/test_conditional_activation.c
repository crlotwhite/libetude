/**
 * @file test_conditional_activation.c
 * @brief LEFX 조건부 확장 활성화 시스템 단위 테스트
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include "libetude/lef_format.h"

// 테스트 헬퍼 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, epsilon, message) \
    do { \
        if (fabs((a) - (b)) > (epsilon)) { \
            printf("FAIL: %s - %s (%.6f != %.6f)\n", __func__, message, (float)(a), (float)(b)); \
            return 0; \
        } \
    } while(0)

#define TEST_SUCCESS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return 1; \
    } while(0)

// 테스트용 확장 모델 생성 헬퍼
static LEFXModel* create_test_extension(const char* name, LEFXExtensionType type, bool conditional) {
    LEFXModel* extension = (LEFXModel*)calloc(1, sizeof(LEFXModel));
    if (!extension) return NULL;

    // 헤더 초기화
    lefx_init_header(&extension->header);
    strncpy(extension->header.extension_name, name, sizeof(extension->header.extension_name) - 1);
    extension->header.extension_type = type;

    if (conditional) {
        extension->header.extension_flags |= LEFX_FLAG_CONDITIONAL;
    }

    // 메타데이터 초기화
    lefx_init_extension_meta(&extension->meta);
    extension->meta.performance_impact = 0.1f; // 낮은 성능 영향
    extension->meta.quality_score = 0.8f;

    return extension;
}

// 테스트용 활성화 규칙 생성 헬퍼
static LEFXActivationRule* create_test_rule(uint16_t rule_id, uint8_t condition_type,
                                           uint8_t operator_type, const char* condition_value,
                                           float activation_weight) {
    LEFXActivationRule* rule = (LEFXActivationRule*)malloc(sizeof(LEFXActivationRule));
    if (!rule) return NULL;

    lefx_init_activation_rule(rule);
    rule->rule_id = rule_id;
    rule->condition_type = condition_type;
    rule->operator_type = operator_type;
    strncpy(rule->condition_value, condition_value, sizeof(rule->condition_value) - 1);
    rule->activation_weight = activation_weight;
    rule->priority = 100;

    return rule;
}

// 테스트용 확장 모델에 규칙 추가
static void add_rule_to_extension(LEFXModel* extension, LEFXActivationRule* rule) {
    if (!extension->activation_rules) {
        extension->activation_rules = (LEFXActivationRule*)malloc(sizeof(LEFXActivationRule));
        extension->num_activation_rules = 0;
    } else {
        extension->activation_rules = (LEFXActivationRule*)realloc(extension->activation_rules,
            (extension->num_activation_rules + 1) * sizeof(LEFXActivationRule));
    }

    extension->activation_rules[extension->num_activation_rules] = *rule;
    extension->num_activation_rules++;
    free(rule);
}

// ============================================================================
// 활성화 컨텍스트 테스트
// ============================================================================

static int test_activation_context_init() {
    LEFXActivationContext context;
    lefx_init_activation_context(&context);

    TEST_ASSERT(context.input_text == NULL, "입력 텍스트가 NULL로 초기화되어야 함");
    TEST_ASSERT(context.text_length == 0, "텍스트 길이가 0으로 초기화되어야 함");
    TEST_ASSERT(context.language_hint == NULL, "언어 힌트가 NULL로 초기화되어야 함");
    TEST_ASSERT(context.speaker_id == 0, "화자 ID가 0으로 초기화되어야 함");
    TEST_ASSERT(context.gender == 255, "성별이 255(해당없음)로 초기화되어야 함");
    TEST_ASSERT(context.age_range == 255, "연령대가 255(해당없음)로 초기화되어야 함");
    TEST_ASSERT_FLOAT_EQ(context.pitch_preference, 0.0f, 0.001f, "피치 선호도가 0.0으로 초기화되어야 함");
    TEST_ASSERT(context.emotion_type == 0, "감정 타입이 0(중성)으로 초기화되어야 함");
    TEST_ASSERT_FLOAT_EQ(context.emotion_intensity, 0.0f, 0.001f, "감정 강도가 0.0으로 초기화되어야 함");
    TEST_ASSERT(context.speaking_style == 0, "말하기 스타일이 0(일반)으로 초기화되어야 함");
    TEST_ASSERT_FLOAT_EQ(context.speaking_speed, 1.0f, 0.001f, "말하기 속도가 1.0으로 초기화되어야 함");
    TEST_ASSERT(context.time_of_day == 2, "시간대가 2(오후)로 초기화되어야 함");
    TEST_ASSERT(context.custom_data == NULL, "사용자 정의 데이터가 NULL로 초기화되어야 함");
    TEST_ASSERT(context.custom_data_size == 0, "사용자 정의 데이터 크기가 0으로 초기화되어야 함");
    TEST_ASSERT_FLOAT_EQ(context.quality_preference, 0.5f, 0.001f, "품질 선호도가 0.5로 초기화되어야 함");
    TEST_ASSERT_FLOAT_EQ(context.performance_budget, 1.0f, 0.001f, "성능 예산이 1.0으로 초기화되어야 함");
    TEST_ASSERT(context.realtime_mode == false, "실시간 모드가 false로 초기화되어야 함");

    TEST_SUCCESS();
}

// ============================================================================
// 활성화 매니저 테스트
// ============================================================================

static int test_activation_manager_create_destroy() {
    LEFXActivationManager* manager = lefx_create_activation_manager(4);

    TEST_ASSERT(manager != NULL, "활성화 매니저가 생성되어야 함");
    TEST_ASSERT(manager->num_extensions == 0, "초기 확장 수가 0이어야 함");
    TEST_ASSERT(manager->extensions_capacity == 4, "초기 용량이 4여야 함");
    TEST_ASSERT(manager->extensions != NULL, "확장 배열이 할당되어야 함");
    TEST_ASSERT(manager->activation_results != NULL, "활성화 결과 배열이 할당되어야 함");
    TEST_ASSERT(manager->transition_states != NULL, "전환 상태 배열이 할당되어야 함");
    TEST_ASSERT(manager->cached_context != NULL, "캐시된 컨텍스트가 할당되어야 함");
    TEST_ASSERT_FLOAT_EQ(manager->global_quality_threshold, 0.7f, 0.001f, "전역 품질 임계값이 0.7이어야 함");
    TEST_ASSERT_FLOAT_EQ(manager->global_performance_budget, 1.0f, 0.001f, "전역 성능 예산이 1.0이어야 함");
    TEST_ASSERT(manager->enable_smooth_transitions == true, "부드러운 전환이 활성화되어야 함");
    TEST_ASSERT_FLOAT_EQ(manager->default_transition_duration, 0.5f, 0.001f, "기본 전환 지속 시간이 0.5초여야 함");
    TEST_ASSERT(manager->cache_valid == false, "캐시가 초기에는 무효해야 함");

    lefx_destroy_activation_manager(manager);
    TEST_SUCCESS();
}

static int test_activation_manager_register_unregister() {
    LEFXActivationManager* manager = lefx_create_activation_manager(2);
    TEST_ASSERT(manager != NULL, "활성화 매니저가 생성되어야 함");

    // 확장 등록 테스트
    LEFXModel* ext1 = create_test_extension("test_ext1", LEFX_EXT_SPEAKER, true);
    LEFXModel* ext2 = create_test_extension("test_ext2", LEFX_EXT_LANGUAGE, false);

    TEST_ASSERT(ext1 != NULL && ext2 != NULL, "테스트 확장이 생성되어야 함");

    int result1 = lefx_register_extension(manager, ext1);
    TEST_ASSERT(result1 == LEF_SUCCESS, "첫 번째 확장 등록이 성공해야 함");
    TEST_ASSERT(manager->num_extensions == 1, "확장 수가 1이어야 함");

    int result2 = lefx_register_extension(manager, ext2);
    TEST_ASSERT(result2 == LEF_SUCCESS, "두 번째 확장 등록이 성공해야 함");
    TEST_ASSERT(manager->num_extensions == 2, "확장 수가 2여야 함");

    // 용량 확장 테스트 (초기 용량 2를 초과)
    LEFXModel* ext3 = create_test_extension("test_ext3", LEFX_EXT_EMOTION, true);
    int result3 = lefx_register_extension(manager, ext3);
    TEST_ASSERT(result3 == LEF_SUCCESS, "용량 확장 후 세 번째 확장 등록이 성공해야 함");
    TEST_ASSERT(manager->num_extensions == 3, "확장 수가 3이어야 함");
    TEST_ASSERT(manager->extensions_capacity >= 3, "용량이 확장되어야 함");

    // 확장 제거 테스트
    int unregister_result = lefx_unregister_extension(manager, ext2);
    TEST_ASSERT(unregister_result == LEF_SUCCESS, "확장 제거가 성공해야 함");
    TEST_ASSERT(manager->num_extensions == 2, "확장 수가 2로 감소해야 함");

    // 존재하지 않는 확장 제거 테스트
    int invalid_unregister = lefx_unregister_extension(manager, ext2);
    TEST_ASSERT(invalid_unregister == LEF_ERROR_LAYER_NOT_FOUND, "존재하지 않는 확장 제거는 실패해야 함");

    // 정리
    free(ext1);
    free(ext2);
    free(ext3);
    lefx_destroy_activation_manager(manager);
    TEST_SUCCESS();
}

// ============================================================================
// 조건 매칭 테스트
// ============================================================================

static int test_text_condition_matching() {
    // 같음 연산자 테스트
    float score1 = lefx_match_text_condition("hello", "hello", 0);
    TEST_ASSERT_FLOAT_EQ(score1, 1.0f, 0.001f, "정확히 일치하는 텍스트는 1.0 점수를 받아야 함");

    float score2 = lefx_match_text_condition("hello", "world", 0);
    TEST_ASSERT_FLOAT_EQ(score2, 0.0f, 0.001f, "일치하지 않는 텍스트는 0.0 점수를 받아야 함");

    // 포함 연산자 테스트
    float score3 = lefx_match_text_condition("ell", "hello world", 1);
    TEST_ASSERT_FLOAT_EQ(score3, 1.0f, 0.001f, "포함된 텍스트는 1.0 점수를 받아야 함");

    float score4 = lefx_match_text_condition("xyz", "hello world", 1);
    TEST_ASSERT_FLOAT_EQ(score4, 0.0f, 0.001f, "포함되지 않은 텍스트는 0.0 점수를 받아야 함");

    // 범위 연산자 테스트 (길이 기반)
    float score5 = lefx_match_text_condition("5-15", "hello world", 2); // 11글자
    TEST_ASSERT(score5 > 0.0f, "범위 내 길이는 양수 점수를 받아야 함");

    float score6 = lefx_match_text_condition("20-30", "hello", 2); // 5글자
    TEST_ASSERT_FLOAT_EQ(score6, 0.0f, 0.001f, "범위 밖 길이는 0.0 점수를 받아야 함");

    TEST_SUCCESS();
}

static int test_speaker_condition_matching() {
    LEFXActivationContext context;
    lefx_init_activation_context(&context);
    context.speaker_id = 123;
    context.gender = 1; // 여성
    context.age_range = 2; // 중년
    context.pitch_preference = 0.3f;

    // 화자 ID 같음 테스트
    float score1 = lefx_match_speaker_condition("123", &context, 0);
    TEST_ASSERT_FLOAT_EQ(score1, 1.0f, 0.001f, "일치하는 화자 ID는 1.0 점수를 받아야 함");

    float score2 = lefx_match_speaker_condition("456", &context, 0);
    TEST_ASSERT_FLOAT_EQ(score2, 0.0f, 0.001f, "일치하지 않는 화자 ID는 0.0 점수를 받아야 함");

    // 성별 포함 테스트
    float score3 = lefx_match_speaker_condition("gender:1", &context, 1);
    TEST_ASSERT_FLOAT_EQ(score3, 1.0f, 0.001f, "일치하는 성별은 1.0 점수를 받아야 함");

    float score4 = lefx_match_speaker_condition("gender:0", &context, 1);
    TEST_ASSERT_FLOAT_EQ(score4, 0.0f, 0.001f, "일치하지 않는 성별은 0.0 점수를 받아야 함");

    // 연령대 포함 테스트
    float score5 = lefx_match_speaker_condition("age:2", &context, 1);
    TEST_ASSERT_FLOAT_EQ(score5, 1.0f, 0.001f, "일치하는 연령대는 1.0 점수를 받아야 함");

    // 피치 선호도 범위 테스트
    float score6 = lefx_match_speaker_condition("0.0:0.5", &context, 2); // 0.3은 범위 내
    TEST_ASSERT(score6 > 0.0f, "범위 내 피치 선호도는 양수 점수를 받아야 함");

    float score7 = lefx_match_speaker_condition("0.8:1.0", &context, 2); // 0.3은 범위 밖
    TEST_ASSERT_FLOAT_EQ(score7, 0.0f, 0.001f, "범위 밖 피치 선호도는 0.0 점수를 받아야 함");

    TEST_SUCCESS();
}

static int test_language_condition_matching() {
    LEFXActivationContext context;
    lefx_init_activation_context(&context);
    context.language_hint = "ko-KR";

    // 같음 연산자 테스트
    float score1 = lefx_match_language_condition("ko-KR", &context, 0);
    TEST_ASSERT_FLOAT_EQ(score1, 1.0f, 0.001f, "정확히 일치하는 언어는 1.0 점수를 받아야 함");

    float score2 = lefx_match_language_condition("en-US", &context, 0);
    TEST_ASSERT_FLOAT_EQ(score2, 0.0f, 0.001f, "일치하지 않는 언어는 0.0 점수를 받아야 함");

    // 포함 연산자 테스트 (언어 패밀리)
    float score3 = lefx_match_language_condition("ko", &context, 1);
    TEST_ASSERT_FLOAT_EQ(score3, 1.0f, 0.001f, "언어 패밀리 매칭은 1.0 점수를 받아야 함");

    float score4 = lefx_match_language_condition("en", &context, 1);
    TEST_ASSERT_FLOAT_EQ(score4, 0.0f, 0.001f, "일치하지 않는 언어 패밀리는 0.0 점수를 받아야 함");

    TEST_SUCCESS();
}

static int test_activation_rule_matching() {
    LEFXActivationContext context;
    lefx_init_activation_context(&context);
    context.input_text = "안녕하세요";
    context.language_hint = "ko";
    context.speaker_id = 100;
    context.gender = 1;

    // 텍스트 조건 규칙
    LEFXActivationRule text_rule;
    lefx_init_activation_rule(&text_rule);
    text_rule.condition_type = LEFX_CONTEXT_TEXT;
    text_rule.operator_type = 1; // 포함
    strcpy(text_rule.condition_value, "안녕");
    text_rule.activation_weight = 0.8f;

    float match_score1 = 0.0f;
    bool matched1 = lefx_match_activation_rule(&text_rule, &context, &match_score1);
    TEST_ASSERT(matched1 == true, "텍스트 조건이 매칭되어야 함");
    TEST_ASSERT_FLOAT_EQ(match_score1, 0.8f, 0.001f, "매칭 점수가 활성화 가중치와 일치해야 함");

    // 화자 조건 규칙
    LEFXActivationRule speaker_rule;
    lefx_init_activation_rule(&speaker_rule);
    speaker_rule.condition_type = LEFX_CONTEXT_SPEAKER;
    speaker_rule.operator_type = 0; // 같음
    strcpy(speaker_rule.condition_value, "100");
    speaker_rule.activation_weight = 1.0f;

    float match_score2 = 0.0f;
    bool matched2 = lefx_match_activation_rule(&speaker_rule, &context, &match_score2);
    TEST_ASSERT(matched2 == true, "화자 조건이 매칭되어야 함");
    TEST_ASSERT_FLOAT_EQ(match_score2, 1.0f, 0.001f, "매칭 점수가 1.0이어야 함");

    // 매칭되지 않는 조건
    LEFXActivationRule no_match_rule;
    lefx_init_activation_rule(&no_match_rule);
    no_match_rule.condition_type = LEFX_CONTEXT_LANGUAGE;
    no_match_rule.operator_type = 0; // 같음
    strcpy(no_match_rule.condition_value, "en");
    no_match_rule.activation_weight = 1.0f;

    float match_score3 = 0.0f;
    bool matched3 = lefx_match_activation_rule(&no_match_rule, &context, &match_score3);
    TEST_ASSERT(matched3 == false, "언어 조건이 매칭되지 않아야 함");
    TEST_ASSERT_FLOAT_EQ(match_score3, 0.0f, 0.001f, "매칭 점수가 0.0이어야 함");

    TEST_SUCCESS();
}

// ============================================================================
// 확장 활성화 평가 테스트
// ============================================================================

static int test_single_extension_evaluation() {
    // 무조건 활성화 확장 테스트
    LEFXModel* unconditional_ext = create_test_extension("unconditional", LEFX_EXT_SPEAKER, false);

    LEFXActivationContext context;
    lefx_init_activation_context(&context);
    context.input_text = "테스트 텍스트";

    LEFXActivationResult result;
    int eval_result = lefx_evaluate_single_extension(unconditional_ext, &context, &result);

    TEST_ASSERT(eval_result == LEF_SUCCESS, "평가가 성공해야 함");
    TEST_ASSERT(result.should_activate == true, "무조건 활성화 확장은 항상 활성화되어야 함");
    TEST_ASSERT_FLOAT_EQ(result.activation_weight, 1.0f, 0.001f, "활성화 가중치가 1.0이어야 함");
    TEST_ASSERT_FLOAT_EQ(result.blend_weight, 1.0f, 0.001f, "블렌딩 가중치가 1.0이어야 함");
    TEST_ASSERT_FLOAT_EQ(result.confidence_score, 1.0f, 0.001f, "신뢰도가 1.0이어야 함");

    // 조건부 활성화 확장 테스트 (규칙 없음)
    LEFXModel* conditional_ext_no_rules = create_test_extension("conditional_no_rules", LEFX_EXT_LANGUAGE, true);

    LEFXActivationResult result2;
    int eval_result2 = lefx_evaluate_single_extension(conditional_ext_no_rules, &context, &result2);

    TEST_ASSERT(eval_result2 == LEF_SUCCESS, "평가가 성공해야 함");
    TEST_ASSERT(result2.should_activate == false, "규칙이 없는 조건부 확장은 비활성화되어야 함");
    TEST_ASSERT_FLOAT_EQ(result2.activation_weight, 0.0f, 0.001f, "활성화 가중치가 0.0이어야 함");

    // 조건부 활성화 확장 테스트 (매칭되는 규칙 있음)
    LEFXModel* conditional_ext_with_rules = create_test_extension("conditional_with_rules", LEFX_EXT_EMOTION, true);

    LEFXActivationRule* rule = create_test_rule(1, LEFX_CONTEXT_TEXT, 1, "테스트", 0.7f);
    add_rule_to_extension(conditional_ext_with_rules, rule);

    LEFXActivationResult result3;
    int eval_result3 = lefx_evaluate_single_extension(conditional_ext_with_rules, &context, &result3);

    TEST_ASSERT(eval_result3 == LEF_SUCCESS, "평가가 성공해야 함");
    TEST_ASSERT(result3.should_activate == true, "매칭되는 규칙이 있는 확장은 활성화되어야 함");
    TEST_ASSERT(result3.activation_weight > 0.0f, "활성화 가중치가 양수여야 함");
    TEST_ASSERT(result3.matched_rule_id == 1, "매칭된 규칙 ID가 1이어야 함");

    // 정리
    free(unconditional_ext);
    free(conditional_ext_no_rules);
    if (conditional_ext_with_rules->activation_rules) {
        free(conditional_ext_with_rules->activation_rules);
    }
    free(conditional_ext_with_rules);

    TEST_SUCCESS();
}

static int test_all_extensions_evaluation() {
    LEFXActivationManager* manager = lefx_create_activation_manager(4);
    TEST_ASSERT(manager != NULL, "활성화 매니저가 생성되어야 함");

    // 여러 확장 등록
    LEFXModel* ext1 = create_test_extension("ext1", LEFX_EXT_SPEAKER, false); // 무조건 활성화
    LEFXModel* ext2 = create_test_extension("ext2", LEFX_EXT_LANGUAGE, true); // 조건부 (규칙 없음)
    LEFXModel* ext3 = create_test_extension("ext3", LEFX_EXT_EMOTION, true); // 조건부 (규칙 있음)

    // ext3에 규칙 추가
    LEFXActivationRule* rule = create_test_rule(1, LEFX_CONTEXT_TEXT, 1, "안녕", 0.8f);
    add_rule_to_extension(ext3, rule);

    lefx_register_extension(manager, ext1);
    lefx_register_extension(manager, ext2);
    lefx_register_extension(manager, ext3);

    // 컨텍스트 설정
    LEFXActivationContext context;
    lefx_init_activation_context(&context);
    context.input_text = "안녕하세요";
    context.performance_budget = 1.0f;

    // 모든 확장 평가
    int eval_result = lefx_evaluate_all_extensions(manager, &context);
    TEST_ASSERT(eval_result == LEF_SUCCESS, "모든 확장 평가가 성공해야 함");

    // 결과 확인
    TEST_ASSERT(manager->activation_results[0].should_activate == true, "첫 번째 확장(무조건)이 활성화되어야 함");
    TEST_ASSERT(manager->activation_results[1].should_activate == false, "두 번째 확장(규칙 없음)이 비활성화되어야 함");
    TEST_ASSERT(manager->activation_results[2].should_activate == true, "세 번째 확장(규칙 매칭)이 활성화되어야 함");

    // 통계 확인
    size_t active_count = 0;
    float total_weight = 0.0f;
    float performance_impact = 0.0f;

    int stats_result = lefx_get_activation_stats(manager, &active_count, &total_weight, &performance_impact);
    TEST_ASSERT(stats_result == LEF_SUCCESS, "통계 조회가 성공해야 함");
    TEST_ASSERT(active_count == 2, "활성화된 확장이 2개여야 함");
    TEST_ASSERT(total_weight > 0.0f, "총 가중치가 양수여야 함");

    // 정리
    free(ext1);
    free(ext2);
    if (ext3->activation_rules) {
        free(ext3->activation_rules);
    }
    free(ext3);
    lefx_destroy_activation_manager(manager);

    TEST_SUCCESS();
}

// ============================================================================
// 블렌딩 테스트
// ============================================================================

static int test_layer_blending() {
    const size_t data_size = 4 * sizeof(float);
    float base_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float extension_data[] = {0.5f, 1.0f, 1.5f, 2.0f};
    float output_data[4];

    // 교체 모드 테스트 (가중치 0.5)
    int result1 = lefx_blend_layer_data(base_data, extension_data, output_data,
                                       data_size, LEFX_BLEND_REPLACE, 0.5f);
    TEST_ASSERT(result1 == LEF_SUCCESS, "교체 모드 블렌딩이 성공해야 함");
    TEST_ASSERT_FLOAT_EQ(output_data[0], 0.75f, 0.001f, "교체 모드 결과가 올바라야 함"); // (1.0 * 0.5 + 0.5 * 0.5)
    TEST_ASSERT_FLOAT_EQ(output_data[1], 1.5f, 0.001f, "교체 모드 결과가 올바라야 함"); // (2.0 * 0.5 + 1.0 * 0.5)

    // 덧셈 모드 테스트
    int result2 = lefx_blend_layer_data(base_data, extension_data, output_data,
                                       data_size, LEFX_BLEND_ADD, 0.5f);
    TEST_ASSERT(result2 == LEF_SUCCESS, "덧셈 모드 블렌딩이 성공해야 함");
    TEST_ASSERT_FLOAT_EQ(output_data[0], 1.25f, 0.001f, "덧셈 모드 결과가 올바라야 함"); // (1.0 + 0.5 * 0.5)
    TEST_ASSERT_FLOAT_EQ(output_data[1], 2.5f, 0.001f, "덧셈 모드 결과가 올바라야 함"); // (2.0 + 1.0 * 0.5)

    // 곱셈 모드 테스트
    int result3 = lefx_blend_layer_data(base_data, extension_data, output_data,
                                       data_size, LEFX_BLEND_MULTIPLY, 0.5f);
    TEST_ASSERT(result3 == LEF_SUCCESS, "곱셈 모드 블렌딩이 성공해야 함");
    TEST_ASSERT_FLOAT_EQ(output_data[0], 1.25f, 0.001f, "곱셈 모드 결과가 올바라야 함"); // (1.0 * (1 + 0.5 * 0.5))

    // 보간 모드 테스트
    int result4 = lefx_blend_layer_data(base_data, extension_data, output_data,
                                       data_size, LEFX_BLEND_INTERPOLATE, 0.3f);
    TEST_ASSERT(result4 == LEF_SUCCESS, "보간 모드 블렌딩이 성공해야 함");
    TEST_ASSERT_FLOAT_EQ(output_data[0], 0.85f, 0.001f, "보간 모드 결과가 올바라야 함"); // (1.0 * 0.7 + 0.5 * 0.3)

    // 가중치 범위 테스트
    int result5 = lefx_blend_layer_data(base_data, extension_data, output_data,
                                       data_size, LEFX_BLEND_REPLACE, 1.5f); // 범위 초과
    TEST_ASSERT(result5 == LEF_SUCCESS, "범위 초과 가중치도 처리되어야 함");
    TEST_ASSERT_FLOAT_EQ(output_data[0], 0.5f, 0.001f, "가중치가 1.0으로 제한되어야 함");

    TEST_SUCCESS();
}

// ============================================================================
// 실시간 전환 테스트
// ============================================================================

static int test_transition_curve() {
    // 선형 곡선 테스트
    float linear1 = lefx_calculate_transition_curve(0.0f, 0);
    TEST_ASSERT_FLOAT_EQ(linear1, 0.0f, 0.001f, "선형 곡선 시작점이 0.0이어야 함");

    float linear2 = lefx_calculate_transition_curve(0.5f, 0);
    TEST_ASSERT_FLOAT_EQ(linear2, 0.5f, 0.001f, "선형 곡선 중간점이 0.5여야 함");

    float linear3 = lefx_calculate_transition_curve(1.0f, 0);
    TEST_ASSERT_FLOAT_EQ(linear3, 1.0f, 0.001f, "선형 곡선 끝점이 1.0이어야 함");

    // ease-in 곡선 테스트
    float easein1 = lefx_calculate_transition_curve(0.0f, 1);
    TEST_ASSERT_FLOAT_EQ(easein1, 0.0f, 0.001f, "ease-in 곡선 시작점이 0.0이어야 함");

    float easein2 = lefx_calculate_transition_curve(0.5f, 1);
    TEST_ASSERT_FLOAT_EQ(easein2, 0.25f, 0.001f, "ease-in 곡선 중간점이 0.25여야 함");

    float easein3 = lefx_calculate_transition_curve(1.0f, 1);
    TEST_ASSERT_FLOAT_EQ(easein3, 1.0f, 0.001f, "ease-in 곡선 끝점이 1.0이어야 함");

    // ease-out 곡선 테스트
    float easeout1 = lefx_calculate_transition_curve(0.0f, 2);
    TEST_ASSERT_FLOAT_EQ(easeout1, 0.0f, 0.001f, "ease-out 곡선 시작점이 0.0이어야 함");

    float easeout2 = lefx_calculate_transition_curve(0.5f, 2);
    TEST_ASSERT_FLOAT_EQ(easeout2, 0.75f, 0.001f, "ease-out 곡선 중간점이 0.75여야 함");

    float easeout3 = lefx_calculate_transition_curve(1.0f, 2);
    TEST_ASSERT_FLOAT_EQ(easeout3, 1.0f, 0.001f, "ease-out 곡선 끝점이 1.0이어야 함");

    // 범위 초과 테스트
    float over = lefx_calculate_transition_curve(1.5f, 0);
    TEST_ASSERT_FLOAT_EQ(over, 1.0f, 0.001f, "범위 초과 값이 1.0으로 제한되어야 함");

    float under = lefx_calculate_transition_curve(-0.5f, 0);
    TEST_ASSERT_FLOAT_EQ(under, 0.0f, 0.001f, "범위 미만 값이 0.0으로 제한되어야 함");

    TEST_SUCCESS();
}

static int test_transition_start_update() {
    LEFXActivationManager* manager = lefx_create_activation_manager(2);
    TEST_ASSERT(manager != NULL, "활성화 매니저가 생성되어야 함");

    LEFXModel* ext = create_test_extension("test_ext", LEFX_EXT_SPEAKER, false);
    lefx_register_extension(manager, ext);

    // 초기 상태 설정
    manager->activation_results[0].blend_weight = 0.2f;

    // 전환 시작
    int start_result = lefx_start_transition(manager, 0, 0.8f, 1.0f);
    TEST_ASSERT(start_result == LEF_SUCCESS, "전환 시작이 성공해야 함");

    LEFXTransitionState* state = &manager->transition_states[0];
    TEST_ASSERT(state->is_transitioning == true, "전환 상태가 활성화되어야 함");
    TEST_ASSERT_FLOAT_EQ(state->prev_weight, 0.2f, 0.001f, "이전 가중치가 저장되어야 함");
    TEST_ASSERT_FLOAT_EQ(state->target_weight, 0.8f, 0.001f, "목표 가중치가 설정되어야 함");
    TEST_ASSERT_FLOAT_EQ(state->transition_duration, 1.0f, 0.001f, "전환 지속 시간이 설정되어야 함");

    // 전환 업데이트 (중간 지점)
    uint64_t start_time = state->transition_start_time;
    uint64_t mid_time = start_time + 500; // 0.5초 후

    int update_result = lefx_update_transitions(manager, mid_time);
    TEST_ASSERT(update_result == LEF_SUCCESS, "전환 업데이트가 성공해야 함");
    TEST_ASSERT(state->is_transitioning == true, "전환이 계속 진행 중이어야 함");
    TEST_ASSERT(state->transition_progress > 0.0f && state->transition_progress < 1.0f,
                "전환 진행률이 0과 1 사이여야 함");

    // 현재 블렌딩 가중치가 이전과 목표 사이에 있는지 확인
    float current_weight = manager->activation_results[0].blend_weight;
    TEST_ASSERT(current_weight > 0.2f && current_weight < 0.8f,
                "현재 가중치가 이전과 목표 사이에 있어야 함");

    // 전환 완료
    uint64_t end_time = start_time + 1100; // 1.1초 후 (완료)

    int complete_result = lefx_update_transitions(manager, end_time);
    TEST_ASSERT(complete_result == LEF_SUCCESS, "전환 완료 업데이트가 성공해야 함");
    TEST_ASSERT(state->is_transitioning == false, "전환이 완료되어야 함");
    TEST_ASSERT_FLOAT_EQ(manager->activation_results[0].blend_weight, 0.8f, 0.001f,
                        "최종 가중치가 목표값과 일치해야 함");

    // 정리
    free(ext);
    lefx_destroy_activation_manager(manager);

    TEST_SUCCESS();
}

// ============================================================================
// 성능 최적화 테스트
// ============================================================================

static int test_performance_optimization() {
    LEFXActivationManager* manager = lefx_create_activation_manager(4);
    TEST_ASSERT(manager != NULL, "활성화 매니저가 생성되어야 함");

    // 성능 영향도가 다른 확장들 생성
    LEFXModel* ext1 = create_test_extension("low_impact", LEFX_EXT_SPEAKER, false);
    ext1->meta.performance_impact = 0.1f; // 낮은 영향

    LEFXModel* ext2 = create_test_extension("medium_impact", LEFX_EXT_LANGUAGE, false);
    ext2->meta.performance_impact = 0.3f; // 중간 영향

    LEFXModel* ext3 = create_test_extension("high_impact", LEFX_EXT_EMOTION, false);
    ext3->meta.performance_impact = 0.7f; // 높은 영향

    lefx_register_extension(manager, ext1);
    lefx_register_extension(manager, ext2);
    lefx_register_extension(manager, ext3);

    // 모든 확장 활성화
    for (size_t i = 0; i < manager->num_extensions; i++) {
        manager->activation_results[i].should_activate = true;
        manager->activation_results[i].activation_weight = 1.0f;
        manager->activation_results[i].blend_weight = 1.0f;
    }

    // 성능 예산 0.5로 최적화 (총 영향도 1.1 > 0.5)
    int optimize_result = lefx_optimize_activations(manager, 0.5f);
    TEST_ASSERT(optimize_result == LEF_SUCCESS, "성능 최적화가 성공해야 함");

    // 낮은 영향도 확장은 유지되어야 함
    TEST_ASSERT(manager->activation_results[0].should_activate == true,
                "낮은 영향도 확장은 활성화 상태를 유지해야 함");

    // 중간 영향도 확장도 유지되어야 함 (0.1 + 0.3 = 0.4 < 0.5)
    TEST_ASSERT(manager->activation_results[1].should_activate == true,
                "중간 영향도 확장도 활성화 상태를 유지해야 함");

    // 높은 영향도 확장은 비활성화되거나 가중치가 감소해야 함
    TEST_ASSERT(manager->activation_results[2].should_activate == false ||
                manager->activation_results[2].activation_weight < 1.0f,
                "높은 영향도 확장은 비활성화되거나 가중치가 감소해야 함");

    // 총 성능 영향도가 예산 이하인지 확인
    float total_impact = 0.0f;
    for (size_t i = 0; i < manager->num_extensions; i++) {
        if (manager->activation_results[i].should_activate) {
            total_impact += manager->extensions[i]->meta.performance_impact *
                           manager->activation_results[i].activation_weight;
        }
    }
    TEST_ASSERT(total_impact <= 0.51f, "총 성능 영향도가 예산 이하여야 함"); // 약간의 오차 허용

    // 정리
    free(ext1);
    free(ext2);
    free(ext3);
    lefx_destroy_activation_manager(manager);

    TEST_SUCCESS();
}

// ============================================================================
// 메인 테스트 실행 함수
// ============================================================================

int main() {
    printf("=== LEFX 조건부 확장 활성화 시스템 단위 테스트 ===\n\n");

    int passed = 0;
    int total = 0;

    // 활성화 컨텍스트 테스트
    total++; if (test_activation_context_init()) passed++;

    // 활성화 매니저 테스트
    total++; if (test_activation_manager_create_destroy()) passed++;
    total++; if (test_activation_manager_register_unregister()) passed++;

    // 조건 매칭 테스트
    total++; if (test_text_condition_matching()) passed++;
    total++; if (test_speaker_condition_matching()) passed++;
    total++; if (test_language_condition_matching()) passed++;
    total++; if (test_activation_rule_matching()) passed++;

    // 확장 활성화 평가 테스트
    total++; if (test_single_extension_evaluation()) passed++;
    total++; if (test_all_extensions_evaluation()) passed++;

    // 블렌딩 테스트
    total++; if (test_layer_blending()) passed++;

    // 실시간 전환 테스트
    total++; if (test_transition_curve()) passed++;
    total++; if (test_transition_start_update()) passed++;

    // 성능 최적화 테스트
    total++; if (test_performance_optimization()) passed++;

    printf("\n=== 테스트 결과 ===\n");
    printf("통과: %d/%d\n", passed, total);
    printf("실패: %d/%d\n", total - passed, total);

    if (passed == total) {
        printf("모든 테스트가 통과했습니다! ✅\n");
        return 0;
    } else {
        printf("일부 테스트가 실패했습니다. ❌\n");
        return 1;
    }
}