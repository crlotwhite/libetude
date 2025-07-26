/**
 * @file conditional_activation.c
 * @brief LEFX 조건부 확장 활성화 시스템 구현
 *
 * 컨텍스트 기반 확장 활성화, 블렌딩 및 보간 메커니즘, 실시간 전환 지원을 제공합니다.
 */

#include "libetude/lef_format.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <regex.h>

// 시간 관련 유틸리티 매크로
#define MILLISECONDS_TO_SECONDS(ms) ((ms) / 1000.0f)
#define SECONDS_TO_MILLISECONDS(s) ((s) * 1000.0f)

// 현재 시간을 밀리초로 가져오기
static uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ============================================================================
// 활성화 컨텍스트 관리 함수들
// ============================================================================

/**
 * 활성화 컨텍스트 초기화
 * @param context 초기화할 컨텍스트 포인터
 */
void lefx_init_activation_context(LEFXActivationContext* context) {
    if (!context) return;

    memset(context, 0, sizeof(LEFXActivationContext));

    // 기본값 설정
    context->input_text = NULL;
    context->text_length = 0;
    context->language_hint = NULL;

    context->speaker_id = 0;
    context->gender = 255; // 해당없음
    context->age_range = 255; // 해당없음
    context->pitch_preference = 0.0f; // 중성

    context->emotion_type = 0; // 중성
    context->emotion_intensity = 0.0f;
    context->speaking_style = 0; // 일반
    context->speaking_speed = 1.0f; // 기본 속도

    context->timestamp = get_current_time_ms();
    context->time_of_day = 2; // 오후 (기본값)

    context->custom_data = NULL;
    context->custom_data_size = 0;

    context->quality_preference = 0.5f; // 균형
    context->performance_budget = 1.0f; // 제한 없음
    context->realtime_mode = false;
}

// ============================================================================
// 활성화 매니저 관리 함수들
// ============================================================================

/**
 * 활성화 매니저 생성
 * @param initial_capacity 초기 확장 배열 용량
 * @return 활성화 매니저 포인터 (실패 시 NULL)
 */
LEFXActivationManager* lefx_create_activation_manager(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 8; // 기본 용량
    }

    LEFXActivationManager* manager = (LEFXActivationManager*)malloc(sizeof(LEFXActivationManager));
    if (!manager) {
        return NULL;
    }

    memset(manager, 0, sizeof(LEFXActivationManager));

    // 확장 배열 초기화
    manager->extensions = (LEFXModel**)calloc(initial_capacity, sizeof(LEFXModel*));
    if (!manager->extensions) {
        free(manager);
        return NULL;
    }

    // 활성화 결과 배열 초기화
    manager->activation_results = (LEFXActivationResult*)calloc(initial_capacity, sizeof(LEFXActivationResult));
    if (!manager->activation_results) {
        free(manager->extensions);
        free(manager);
        return NULL;
    }

    // 전환 상태 배열 초기화
    manager->transition_states = (LEFXTransitionState*)calloc(initial_capacity, sizeof(LEFXTransitionState));
    if (!manager->transition_states) {
        free(manager->activation_results);
        free(manager->extensions);
        free(manager);
        return NULL;
    }

    // 캐시된 컨텍스트 초기화
    manager->cached_context = (LEFXActivationContext*)malloc(sizeof(LEFXActivationContext));
    if (!manager->cached_context) {
        free(manager->transition_states);
        free(manager->activation_results);
        free(manager->extensions);
        free(manager);
        return NULL;
    }

    manager->num_extensions = 0;
    manager->extensions_capacity = initial_capacity;

    // 기본 설정
    manager->global_quality_threshold = 0.7f;
    manager->global_performance_budget = 1.0f;
    manager->enable_smooth_transitions = true;
    manager->default_transition_duration = 0.5f; // 0.5초

    // 캐시 초기화
    lefx_init_activation_context(manager->cached_context);
    manager->cache_timestamp = 0;
    manager->cache_valid = false;

    // 통계 초기화
    manager->total_activations = 0;
    manager->total_transitions = 0;
    manager->avg_activation_time = 0.0f;

    return manager;
}

/**
 * 활성화 매니저 해제
 * @param manager 활성화 매니저 포인터
 */
void lefx_destroy_activation_manager(LEFXActivationManager* manager) {
    if (!manager) return;

    // 배열들 해제
    if (manager->extensions) {
        free(manager->extensions);
    }
    if (manager->activation_results) {
        free(manager->activation_results);
    }
    if (manager->transition_states) {
        free(manager->transition_states);
    }
    if (manager->cached_context) {
        free(manager->cached_context);
    }

    free(manager);
}

/**
 * 확장 모델을 활성화 매니저에 등록
 * @param manager 활성화 매니저
 * @param extension 등록할 확장 모델
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_register_extension(LEFXActivationManager* manager, LEFXModel* extension) {
    if (!manager || !extension) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 용량 확장이 필요한지 확인
    if (manager->num_extensions >= manager->extensions_capacity) {
        size_t new_capacity = manager->extensions_capacity * 2;

        // 확장 배열 재할당
        LEFXModel** new_extensions = (LEFXModel**)realloc(manager->extensions,
                                                         new_capacity * sizeof(LEFXModel*));
        if (!new_extensions) {
            return LEF_ERROR_OUT_OF_MEMORY;
        }
        manager->extensions = new_extensions;

        // 활성화 결과 배열 재할당
        LEFXActivationResult* new_results = (LEFXActivationResult*)realloc(manager->activation_results,
                                                                          new_capacity * sizeof(LEFXActivationResult));
        if (!new_results) {
            return LEF_ERROR_OUT_OF_MEMORY;
        }
        manager->activation_results = new_results;

        // 전환 상태 배열 재할당
        LEFXTransitionState* new_states = (LEFXTransitionState*)realloc(manager->transition_states,
                                                                       new_capacity * sizeof(LEFXTransitionState));
        if (!new_states) {
            return LEF_ERROR_OUT_OF_MEMORY;
        }
        manager->transition_states = new_states;

        // 새로 할당된 메모리 초기화
        memset(&manager->activation_results[manager->extensions_capacity], 0,
               (new_capacity - manager->extensions_capacity) * sizeof(LEFXActivationResult));
        memset(&manager->transition_states[manager->extensions_capacity], 0,
               (new_capacity - manager->extensions_capacity) * sizeof(LEFXTransitionState));

        manager->extensions_capacity = new_capacity;
    }

    // 확장 등록
    manager->extensions[manager->num_extensions] = extension;

    // 활성화 결과 초기화
    LEFXActivationResult* result = &manager->activation_results[manager->num_extensions];
    memset(result, 0, sizeof(LEFXActivationResult));
    result->should_activate = false;
    result->activation_weight = 0.0f;
    result->blend_weight = 0.0f;
    result->matched_rule_id = 0;
    result->confidence_score = 0.0f;
    result->activation_reason = "등록됨";

    // 전환 상태 초기화
    LEFXTransitionState* state = &manager->transition_states[manager->num_extensions];
    memset(state, 0, sizeof(LEFXTransitionState));
    state->is_transitioning = false;
    state->transition_progress = 0.0f;
    state->transition_duration = manager->default_transition_duration;
    state->prev_weight = 0.0f;
    state->target_weight = 0.0f;
    state->prev_blend_mode = LEFX_BLEND_REPLACE;
    state->target_blend_mode = LEFX_BLEND_REPLACE;
    state->transition_curve = 0; // 선형
    state->smoothing_factor = 0.5f;

    manager->num_extensions++;

    // 캐시 무효화
    lefx_invalidate_cache(manager);

    return LEF_SUCCESS;
}

/**
 * 확장 모델을 활성화 매니저에서 제거
 * @param manager 활성화 매니저
 * @param extension 제거할 확장 모델
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_unregister_extension(LEFXActivationManager* manager, LEFXModel* extension) {
    if (!manager || !extension) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 확장 찾기
    size_t found_index = SIZE_MAX;
    for (size_t i = 0; i < manager->num_extensions; i++) {
        if (manager->extensions[i] == extension) {
            found_index = i;
            break;
        }
    }

    if (found_index == SIZE_MAX) {
        return LEF_ERROR_LAYER_NOT_FOUND; // 확장을 찾을 수 없음
    }

    // 배열에서 제거 (뒤의 요소들을 앞으로 이동)
    for (size_t i = found_index; i < manager->num_extensions - 1; i++) {
        manager->extensions[i] = manager->extensions[i + 1];
        manager->activation_results[i] = manager->activation_results[i + 1];
        manager->transition_states[i] = manager->transition_states[i + 1];
    }

    manager->num_extensions--;

    // 캐시 무효화
    lefx_invalidate_cache(manager);

    return LEF_SUCCESS;
}

// ============================================================================
// 조건 매칭 함수들
// ============================================================================

/**
 * 텍스트 조건 매칭
 * @param rule_value 규칙 값
 * @param context_text 컨텍스트 텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_text_condition(const char* rule_value,
                               const char* context_text,
                               uint8_t operator_type) {
    if (!rule_value || !context_text) {
        return 0.0f;
    }

    switch (operator_type) {
        case 0: // 같음
            return (strcmp(rule_value, context_text) == 0) ? 1.0f : 0.0f;

        case 1: // 포함
            return (strstr(context_text, rule_value) != NULL) ? 1.0f : 0.0f;

        case 2: // 범위 (길이 기반)
        {
            // 형식: "min-max" (예: "10-50")
            int min_len = 0, max_len = 0;
            if (sscanf(rule_value, "%d-%d", &min_len, &max_len) == 2) {
                int text_len = (int)strlen(context_text);
                if (text_len >= min_len && text_len <= max_len) {
                    // 범위 내에서의 상대적 위치를 점수로 반환
                    float relative_pos = (float)(text_len - min_len) / (float)(max_len - min_len);
                    return 1.0f - fabs(relative_pos - 0.5f) * 2.0f; // 중앙에 가까울수록 높은 점수
                }
            }
            return 0.0f;
        }

        case 3: // 정규식
        {
            regex_t regex;
            int result = regcomp(&regex, rule_value, REG_EXTENDED | REG_ICASE);
            if (result != 0) {
                return 0.0f; // 정규식 컴파일 실패
            }

            result = regexec(&regex, context_text, 0, NULL, 0);
            regfree(&regex);

            return (result == 0) ? 1.0f : 0.0f;
        }

        default:
            return 0.0f;
    }
}

/**
 * 화자 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_speaker_condition(const char* rule_value,
                                  const LEFXActivationContext* context,
                                  uint8_t operator_type) {
    if (!rule_value || !context) {
        return 0.0f;
    }

    switch (operator_type) {
        case 0: // 같음 (화자 ID)
        {
            uint16_t target_speaker_id = (uint16_t)atoi(rule_value);
            return (context->speaker_id == target_speaker_id) ? 1.0f : 0.0f;
        }

        case 1: // 포함 (성별 포함)
        {
            // 형식: "gender:0" (남성), "gender:1" (여성), "gender:2" (중성)
            if (strncmp(rule_value, "gender:", 7) == 0) {
                uint8_t target_gender = (uint8_t)atoi(rule_value + 7);
                return (context->gender == target_gender) ? 1.0f : 0.0f;
            }
            // 형식: "age:0" (어린이), "age:1" (청년), "age:2" (중년), "age:3" (노년)
            else if (strncmp(rule_value, "age:", 4) == 0) {
                uint8_t target_age = (uint8_t)atoi(rule_value + 4);
                return (context->age_range == target_age) ? 1.0f : 0.0f;
            }
            return 0.0f;
        }

        case 2: // 범위 (피치 선호도 범위)
        {
            // 형식: "-0.5:0.5" (피치 선호도 범위)
            float min_pitch = 0.0f, max_pitch = 0.0f;
            if (sscanf(rule_value, "%f:%f", &min_pitch, &max_pitch) == 2) {
                if (context->pitch_preference >= min_pitch && context->pitch_preference <= max_pitch) {
                    // 범위 내에서의 상대적 위치를 점수로 반환
                    float relative_pos = (context->pitch_preference - min_pitch) / (max_pitch - min_pitch);
                    return 1.0f - fabs(relative_pos - 0.5f) * 2.0f;
                }
            }
            return 0.0f;
        }

        default:
            return 0.0f;
    }
}

/**
 * 언어 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_language_condition(const char* rule_value,
                                   const LEFXActivationContext* context,
                                   uint8_t operator_type) {
    if (!rule_value || !context || !context->language_hint) {
        return 0.0f;
    }

    switch (operator_type) {
        case 0: // 같음
            return (strcmp(rule_value, context->language_hint) == 0) ? 1.0f : 0.0f;

        case 1: // 포함 (언어 패밀리)
        {
            // 예: "ko" 규칙이 "ko-KR", "ko-US" 등과 매칭
            size_t rule_len = strlen(rule_value);
            return (strncmp(rule_value, context->language_hint, rule_len) == 0) ? 1.0f : 0.0f;
        }

        default:
            return 0.0f;
    }
}

/**
 * 시간 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_time_condition(const char* rule_value,
                               const LEFXActivationContext* context,
                               uint8_t operator_type) {
    if (!rule_value || !context) {
        return 0.0f;
    }

    switch (operator_type) {
        case 0: // 같음 (시간대)
        {
            uint8_t target_time = (uint8_t)atoi(rule_value);
            return (context->time_of_day == target_time) ? 1.0f : 0.0f;
        }

        case 2: // 범위 (시간 범위)
        {
            // 형식: "9-17" (9시부터 17시까지)
            int start_hour = 0, end_hour = 0;
            if (sscanf(rule_value, "%d-%d", &start_hour, &end_hour) == 2) {
                // 현재 시간을 시간으로 변환 (간단한 구현)
                time_t now = time(NULL);
                struct tm* tm_info = localtime(&now);
                int current_hour = tm_info->tm_hour;

                if (current_hour >= start_hour && current_hour <= end_hour) {
                    return 1.0f;
                }
            }
            return 0.0f;
        }

        default:
            return 0.0f;
    }
}

/**
 * 사용자 정의 조건 매칭
 * @param rule_value 규칙 값
 * @param context 활성화 컨텍스트
 * @param operator_type 연산자 타입
 * @return 매칭 점수 (0.0 ~ 1.0)
 */
float lefx_match_custom_condition(const char* rule_value,
                                 const LEFXActivationContext* context,
                                 uint8_t operator_type) {
    if (!rule_value || !context || !context->custom_data) {
        return 0.0f;
    }

    // 사용자 정의 조건은 애플리케이션별로 구현해야 함
    // 여기서는 기본적인 문자열 비교만 제공
    const char* custom_string = (const char*)context->custom_data;

    switch (operator_type) {
        case 0: // 같음
            return (strcmp(rule_value, custom_string) == 0) ? 1.0f : 0.0f;

        case 1: // 포함
            return (strstr(custom_string, rule_value) != NULL) ? 1.0f : 0.0f;

        default:
            return 0.0f;
    }
}

/**
 * 활성화 규칙 매칭 검사
 * @param rule 활성화 규칙
 * @param context 활성화 컨텍스트
 * @param match_score 매칭 점수 (출력, 0.0 ~ 1.0)
 * @return 매칭 시 true, 비매칭 시 false
 */
bool lefx_match_activation_rule(const LEFXActivationRule* rule,
                               const LEFXActivationContext* context,
                               float* match_score) {
    if (!rule || !context || !match_score) {
        if (match_score) *match_score = 0.0f;
        return false;
    }

    float score = 0.0f;

    switch (rule->condition_type) {
        case 0: // 텍스트 조건
            score = lefx_match_text_condition(rule->condition_value, context->input_text, rule->operator_type);
            break;

        case 1: // 화자 조건
            score = lefx_match_speaker_condition(rule->condition_value, context, rule->operator_type);
            break;

        case 2: // 언어 조건
            score = lefx_match_language_condition(rule->condition_value, context, rule->operator_type);
            break;

        case 3: // 시간 조건
            score = lefx_match_time_condition(rule->condition_value, context, rule->operator_type);
            break;

        case 4: // 사용자 정의 조건
            score = lefx_match_custom_condition(rule->condition_value, context, rule->operator_type);
            break;

        default:
            score = 0.0f;
            break;
    }

    // 규칙의 활성화 가중치 적용
    score *= rule->activation_weight;

    *match_score = score;
    return score > 0.0f;
}

// ============================================================================
// 확장 활성화 평가 함수들
// ============================================================================

/**
 * 단일 확장 모델의 조건부 활성화 평가
 * @param extension 확장 모델
 * @param context 활성화 컨텍스트
 * @param result 활성화 결과 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_evaluate_single_extension(const LEFXModel* extension,
                                  const LEFXActivationContext* context,
                                  LEFXActivationResult* result) {
    if (!extension || !context || !result) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 결과 초기화
    memset(result, 0, sizeof(LEFXActivationResult));
    result->should_activate = false;
    result->activation_weight = 0.0f;
    result->blend_weight = 0.0f;
    result->matched_rule_id = 0;
    result->confidence_score = 0.0f;
    result->activation_reason = "평가되지 않음";

    // 조건부 활성화 플래그가 설정되어 있지 않으면 항상 활성화
    if (!(extension->header.extension_flags & LEFX_FLAG_CONDITIONAL)) {
        result->should_activate = true;
        result->activation_weight = 1.0f;
        result->blend_weight = 1.0f;
        result->confidence_score = 1.0f;
        result->activation_reason = "무조건 활성화";
        return LEF_SUCCESS;
    }

    // 활성화 규칙이 없으면 비활성화
    if (!extension->activation_rules || extension->num_activation_rules == 0) {
        result->activation_reason = "활성화 규칙 없음";
        return LEF_SUCCESS;
    }

    // 모든 활성화 규칙 평가
    float total_score = 0.0f;
    float max_score = 0.0f;
    uint16_t best_rule_id = 0;
    size_t matched_rules = 0;

    for (size_t i = 0; i < extension->num_activation_rules; i++) {
        const LEFXActivationRule* rule = &extension->activation_rules[i];
        float match_score = 0.0f;

        if (lefx_match_activation_rule(rule, context, &match_score)) {
            total_score += match_score;
            matched_rules++;

            if (match_score > max_score) {
                max_score = match_score;
                best_rule_id = rule->rule_id;
            }
        }
    }

    // 활성화 결정
    if (matched_rules > 0) {
        // 평균 점수 계산
        float avg_score = total_score / (float)matched_rules;

        // 성능 예산 고려
        float performance_factor = 1.0f;
        if (context->performance_budget < 1.0f) {
            performance_factor = context->performance_budget;
        }

        // 품질 선호도 고려
        float quality_factor = 1.0f;
        if (context->quality_preference < 0.5f) {
            // 속도 우선 모드에서는 가중치 감소
            quality_factor = 0.5f + context->quality_preference;
        }

        result->should_activate = true;
        result->activation_weight = avg_score * performance_factor * quality_factor;
        result->blend_weight = result->activation_weight;
        result->matched_rule_id = best_rule_id;
        result->confidence_score = max_score;
        result->activation_reason = "조건 매칭됨";

        // 가중치 범위 제한
        if (result->activation_weight > 1.0f) result->activation_weight = 1.0f;
        if (result->blend_weight > 1.0f) result->blend_weight = 1.0f;
    } else {
        result->activation_reason = "조건 매칭 실패";
    }

    return LEF_SUCCESS;
}

/**
 * 모든 등록된 확장 모델의 조건부 활성화 평가
 * @param manager 활성화 매니저
 * @param context 활성화 컨텍스트
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_evaluate_all_extensions(LEFXActivationManager* manager,
                                const LEFXActivationContext* context) {
    if (!manager || !context) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    uint64_t start_time = get_current_time_ms();

    // 모든 확장에 대해 평가 수행
    for (size_t i = 0; i < manager->num_extensions; i++) {
        LEFXModel* extension = manager->extensions[i];
        LEFXActivationResult* result = &manager->activation_results[i];

        int eval_result = lefx_evaluate_single_extension(extension, context, result);
        if (eval_result != LEF_SUCCESS) {
            return eval_result;
        }

        // 활성화된 경우 통계 업데이트
        if (result->should_activate) {
            manager->total_activations++;
        }
    }

    // 성능 예산에 따른 최적화
    if (context->performance_budget < 1.0f) {
        lefx_optimize_activations(manager, context->performance_budget);
    }

    // 평균 활성화 시간 업데이트
    uint64_t end_time = get_current_time_ms();
    float activation_time = MILLISECONDS_TO_SECONDS(end_time - start_time);

    if (manager->total_activations > 0) {
        manager->avg_activation_time = (manager->avg_activation_time * (manager->total_activations - 1) + activation_time) / manager->total_activations;
    } else {
        manager->avg_activation_time = activation_time;
    }

    // 캐시 업데이트
    memcpy(manager->cached_context, context, sizeof(LEFXActivationContext));
    manager->cache_timestamp = get_current_time_ms();
    manager->cache_valid = true;

    return LEF_SUCCESS;
}

// ============================================================================
// 블렌딩 및 보간 함수들
// ============================================================================

/**
 * 레이어 블렌딩 수행
 * @param base_data 기본 레이어 데이터
 * @param extension_data 확장 레이어 데이터
 * @param output_data 출력 데이터
 * @param data_size 데이터 크기
 * @param blend_mode 블렌딩 모드
 * @param blend_weight 블렌딩 가중치
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_blend_layer_data(const void* base_data,
                         const void* extension_data,
                         void* output_data,
                         size_t data_size,
                         LEFXBlendMode blend_mode,
                         float blend_weight) {
    if (!base_data || !extension_data || !output_data || data_size == 0) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 가중치 범위 제한
    if (blend_weight < 0.0f) blend_weight = 0.0f;
    if (blend_weight > 1.0f) blend_weight = 1.0f;

    const float* base = (const float*)base_data;
    const float* extension = (const float*)extension_data;
    float* output = (float*)output_data;
    size_t num_elements = data_size / sizeof(float);

    switch (blend_mode) {
        case LEFX_BLEND_REPLACE: // 교체
            for (size_t i = 0; i < num_elements; i++) {
                output[i] = base[i] * (1.0f - blend_weight) + extension[i] * blend_weight;
            }
            break;

        case LEFX_BLEND_ADD: // 덧셈
            for (size_t i = 0; i < num_elements; i++) {
                output[i] = base[i] + extension[i] * blend_weight;
            }
            break;

        case LEFX_BLEND_MULTIPLY: // 곱셈
            for (size_t i = 0; i < num_elements; i++) {
                output[i] = base[i] * (1.0f + extension[i] * blend_weight);
            }
            break;

        case LEFX_BLEND_INTERPOLATE: // 보간
            for (size_t i = 0; i < num_elements; i++) {
                output[i] = base[i] * (1.0f - blend_weight) + extension[i] * blend_weight;
            }
            break;

        case LEFX_BLEND_WEIGHTED_SUM: // 가중합
        {
            float base_weight = 1.0f - blend_weight;
            for (size_t i = 0; i < num_elements; i++) {
                output[i] = base[i] * base_weight + extension[i] * blend_weight;
            }
            break;
        }

        default:
            // 기본적으로 교체 모드 사용
            for (size_t i = 0; i < num_elements; i++) {
                output[i] = base[i] * (1.0f - blend_weight) + extension[i] * blend_weight;
            }
            break;
    }

    return LEF_SUCCESS;
}

// ============================================================================
// 실시간 전환 시스템 함수들
// ============================================================================

/**
 * 전환 곡선 계산
 * @param progress 진행률 (0.0 ~ 1.0)
 * @param curve_type 곡선 타입
 * @return 조정된 진행률 (0.0 ~ 1.0)
 */
float lefx_calculate_transition_curve(float progress, uint8_t curve_type) {
    // 진행률 범위 제한
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    switch (curve_type) {
        case 0: // 선형
            return progress;

        case 1: // ease-in (가속)
            return progress * progress;

        case 2: // ease-out (감속)
            return 1.0f - (1.0f - progress) * (1.0f - progress);

        case 3: // ease-in-out (가속 후 감속)
            if (progress < 0.5f) {
                return 2.0f * progress * progress;
            } else {
                return 1.0f - 2.0f * (1.0f - progress) * (1.0f - progress);
            }

        default:
            return progress; // 기본적으로 선형
    }
}

/**
 * 실시간 전환 시작
 * @param manager 활성화 매니저
 * @param extension_index 확장 인덱스
 * @param target_weight 목표 가중치
 * @param transition_duration 전환 지속 시간 (초)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_start_transition(LEFXActivationManager* manager,
                         size_t extension_index,
                         float target_weight,
                         float transition_duration) {
    if (!manager || extension_index >= manager->num_extensions) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    if (transition_duration <= 0.0f) {
        transition_duration = manager->default_transition_duration;
    }

    LEFXTransitionState* state = &manager->transition_states[extension_index];
    LEFXActivationResult* result = &manager->activation_results[extension_index];

    // 전환 상태 설정
    state->is_transitioning = true;
    state->transition_progress = 0.0f;
    state->transition_duration = transition_duration;
    state->transition_start_time = get_current_time_ms();

    // 현재 상태를 이전 상태로 저장
    state->prev_weight = result->blend_weight;
    state->target_weight = target_weight;

    // 블렌딩 모드는 현재 확장의 설정을 따름
    if (manager->extensions[extension_index]->num_layers > 0) {
        state->prev_blend_mode = (LEFXBlendMode)manager->extensions[extension_index]->layer_headers[0].blend_mode;
        state->target_blend_mode = state->prev_blend_mode;
    }

    manager->total_transitions++;

    return LEF_SUCCESS;
}

/**
 * 실시간 전환 업데이트
 * @param manager 활성화 매니저
 * @param current_time 현재 시간 (밀리초)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_update_transitions(LEFXActivationManager* manager, uint64_t current_time) {
    if (!manager) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < manager->num_extensions; i++) {
        LEFXTransitionState* state = &manager->transition_states[i];
        LEFXActivationResult* result = &manager->activation_results[i];

        if (!state->is_transitioning) {
            continue;
        }

        // 전환 진행률 계산
        uint64_t elapsed_time = current_time - state->transition_start_time;
        float elapsed_seconds = MILLISECONDS_TO_SECONDS(elapsed_time);
        float raw_progress = elapsed_seconds / state->transition_duration;

        if (raw_progress >= 1.0f) {
            // 전환 완료
            state->is_transitioning = false;
            state->transition_progress = 1.0f;
            result->blend_weight = state->target_weight;
        } else {
            // 전환 중
            state->transition_progress = raw_progress;

            // 전환 곡선 적용
            float curved_progress = lefx_calculate_transition_curve(raw_progress, state->transition_curve);

            // 스무딩 적용
            if (state->smoothing_factor > 0.0f) {
                float smoothed_progress = state->smoothing_factor * curved_progress +
                                        (1.0f - state->smoothing_factor) * state->transition_progress;
                curved_progress = smoothed_progress;
            }

            // 가중치 보간
            result->blend_weight = state->prev_weight * (1.0f - curved_progress) +
                                 state->target_weight * curved_progress;
        }
    }

    return LEF_SUCCESS;
}

// ============================================================================
// 최적화 및 유틸리티 함수들
// ============================================================================

/**
 * 활성화 결과 최적화 (성능 예산 고려)
 * @param manager 활성화 매니저
 * @param performance_budget 성능 예산 (0.0 ~ 1.0)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_optimize_activations(LEFXActivationManager* manager, float performance_budget) {
    if (!manager || performance_budget < 0.0f || performance_budget > 1.0f) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    // 활성화된 확장들을 성능 영향도 순으로 정렬
    typedef struct {
        size_t index;
        float performance_impact;
        float activation_weight;
    } ExtensionInfo;

    ExtensionInfo* infos = (ExtensionInfo*)malloc(manager->num_extensions * sizeof(ExtensionInfo));
    if (!infos) {
        return LEF_ERROR_OUT_OF_MEMORY;
    }

    size_t active_count = 0;
    for (size_t i = 0; i < manager->num_extensions; i++) {
        if (manager->activation_results[i].should_activate) {
            infos[active_count].index = i;
            infos[active_count].performance_impact = manager->extensions[i]->meta.performance_impact;
            infos[active_count].activation_weight = manager->activation_results[i].activation_weight;
            active_count++;
        }
    }

    // 성능 영향도가 낮은 순으로 정렬 (버블 정렬 - 간단한 구현)
    for (size_t i = 0; i < active_count - 1; i++) {
        for (size_t j = 0; j < active_count - i - 1; j++) {
            if (infos[j].performance_impact > infos[j + 1].performance_impact) {
                ExtensionInfo temp = infos[j];
                infos[j] = infos[j + 1];
                infos[j + 1] = temp;
            }
        }
    }

    // 성능 예산에 맞춰 확장 선택
    float used_budget = 0.0f;
    for (size_t i = 0; i < active_count; i++) {
        size_t ext_index = infos[i].index;
        float impact = infos[i].performance_impact;

        if (used_budget + impact <= performance_budget) {
            // 예산 내에서 활성화 유지
            used_budget += impact;
        } else {
            // 예산 초과 시 가중치 감소 또는 비활성화
            float remaining_budget = performance_budget - used_budget;
            if (remaining_budget > 0.0f) {
                // 부분적 활성화
                float reduction_factor = remaining_budget / impact;
                manager->activation_results[ext_index].activation_weight *= reduction_factor;
                manager->activation_results[ext_index].blend_weight *= reduction_factor;
                used_budget = performance_budget;
            } else {
                // 완전 비활성화
                manager->activation_results[ext_index].should_activate = false;
                manager->activation_results[ext_index].activation_weight = 0.0f;
                manager->activation_results[ext_index].blend_weight = 0.0f;
            }
        }
    }

    free(infos);
    return LEF_SUCCESS;
}

/**
 * 활성화 캐시 무효화
 * @param manager 활성화 매니저
 */
void lefx_invalidate_cache(LEFXActivationManager* manager) {
    if (!manager) return;

    manager->cache_valid = false;
    manager->cache_timestamp = 0;
}

/**
 * 활성화 통계 정보 가져오기
 * @param manager 활성화 매니저
 * @param active_extensions 활성화된 확장 수 (출력)
 * @param total_weight 총 활성화 가중치 (출력)
 * @param performance_impact 성능 영향도 (출력)
 * @return 성공 시 LEF_SUCCESS, 실패 시 에러 코드
 */
int lefx_get_activation_stats(const LEFXActivationManager* manager,
                             size_t* active_extensions,
                             float* total_weight,
                             float* performance_impact) {
    if (!manager) {
        return LEF_ERROR_INVALID_ARGUMENT;
    }

    size_t active_count = 0;
    float weight_sum = 0.0f;
    float impact_sum = 0.0f;

    for (size_t i = 0; i < manager->num_extensions; i++) {
        if (manager->activation_results[i].should_activate) {
            active_count++;
            weight_sum += manager->activation_results[i].activation_weight;
            impact_sum += manager->extensions[i]->meta.performance_impact;
        }
    }

    if (active_extensions) *active_extensions = active_count;
    if (total_weight) *total_weight = weight_sum;
    if (performance_impact) *performance_impact = impact_sum;

    return LEF_SUCCESS;
}

/**
 * 활성화 정보 출력 (디버깅용)
 * @param manager 활성화 매니저
 */
void lefx_print_activation_info(const LEFXActivationManager* manager) {
    if (!manager) {
        printf("활성화 매니저가 NULL입니다.\n");
        return;
    }

    printf("=== 확장 활성화 정보 ===\n");
    printf("등록된 확장 수: %zu\n", manager->num_extensions);
    printf("총 활성화 횟수: %zu\n", manager->total_activations);
    printf("총 전환 횟수: %zu\n", manager->total_transitions);
    printf("평균 활성화 시간: %.3f ms\n", manager->avg_activation_time * 1000.0f);

    size_t active_count = 0;
    float total_weight = 0.0f;
    float total_impact = 0.0f;

    lefx_get_activation_stats(manager, &active_count, &total_weight, &total_impact);

    printf("\n=== 현재 상태 ===\n");
    printf("활성화된 확장: %zu / %zu\n", active_count, manager->num_extensions);
    printf("총 활성화 가중치: %.3f\n", total_weight);
    printf("총 성능 영향도: %.3f\n", total_impact);

    printf("\n=== 확장별 상세 정보 ===\n");
    for (size_t i = 0; i < manager->num_extensions && i < 10; i++) { // 최대 10개만 출력
        const LEFXModel* ext = manager->extensions[i];
        const LEFXActivationResult* result = &manager->activation_results[i];
        const LEFXTransitionState* state = &manager->transition_states[i];

        printf("확장 %zu (%s):\n", i, ext->header.extension_name);
        printf("  활성화: %s\n", result->should_activate ? "예" : "아니오");
        printf("  가중치: %.3f\n", result->activation_weight);
        printf("  블렌딩 가중치: %.3f\n", result->blend_weight);
        printf("  신뢰도: %.3f\n", result->confidence_score);
        printf("  전환 중: %s\n", state->is_transitioning ? "예" : "아니오");
        if (state->is_transitioning) {
            printf("  전환 진행률: %.1f%%\n", state->transition_progress * 100.0f);
        }
        printf("  이유: %s\n", result->activation_reason);
        printf("\n");
    }

    if (manager->num_extensions > 10) {
        printf("... (총 %zu개 확장 중 10개만 표시)\n", manager->num_extensions);
    }
}