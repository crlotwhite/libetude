/**
 * @file fast_math.h
 * @brief FastApprox 기반 고속 수학 함수 라이브러리
 *
 * 음성 합성에 최적화된 고속 근사 수학 함수들을 제공합니다.
 * FastApprox 알고리즘을 기반으로 하여 정확도와 성능의 균형을 맞춥니다.
 */

#ifndef LIBETUDE_FAST_MATH_H
#define LIBETUDE_FAST_MATH_H

#include "libetude/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 고속 지수 함수 (FastApprox 기반)
 *
 * 표준 exp() 함수보다 빠른 근사 구현입니다.
 * 음성 합성에서 활성화 함수 계산에 사용됩니다.
 *
 * @param x 입력값
 * @return e^x의 근사값
 */
float et_fast_exp(float x);

/**
 * @brief 고속 자연로그 함수 (FastApprox 기반)
 *
 * 표준 log() 함수보다 빠른 근사 구현입니다.
 *
 * @param x 입력값 (x > 0)
 * @return ln(x)의 근사값
 */
float et_fast_log(float x);

/**
 * @brief 고속 밑이 2인 로그 함수
 *
 * @param x 입력값 (x > 0)
 * @return log2(x)의 근사값
 */
float et_fast_log2(float x);

/**
 * @brief 고속 밑이 10인 로그 함수
 *
 * @param x 입력값 (x > 0)
 * @return log10(x)의 근사값
 */
float et_fast_log10(float x);

/**
 * @brief 고속 거듭제곱 함수
 *
 * @param base 밑
 * @param exponent 지수
 * @return base^exponent의 근사값
 */
float et_fast_pow(float base, float exponent);

/**
 * @brief 고속 사인 함수 (룩업 테이블 기반)
 *
 * 룩업 테이블과 선형 보간을 사용한 고속 구현입니다.
 *
 * @param x 입력값 (라디안)
 * @return sin(x)의 근사값
 */
float et_fast_sin(float x);

/**
 * @brief 고속 코사인 함수 (룩업 테이블 기반)
 *
 * @param x 입력값 (라디안)
 * @return cos(x)의 근사값
 */
float et_fast_cos(float x);

/**
 * @brief 고속 탄젠트 함수
 *
 * @param x 입력값 (라디안)
 * @return tan(x)의 근사값
 */
float et_fast_tan(float x);

/**
 * @brief 고속 아크탄젠트 함수
 *
 * @param x 입력값
 * @return atan(x)의 근사값
 */
float et_fast_atan(float x);

/**
 * @brief 고속 아크탄젠트2 함수
 *
 * @param y y 좌표
 * @param x x 좌표
 * @return atan2(y, x)의 근사값
 */
float et_fast_atan2(float y, float x);

/**
 * @brief 고속 하이퍼볼릭 탄젠트 함수 (활성화 함수)
 *
 * 신경망의 활성화 함수로 자주 사용되는 tanh 함수의 고속 구현입니다.
 *
 * @param x 입력값
 * @return tanh(x)의 근사값
 */
float et_fast_tanh(float x);

/**
 * @brief 고속 시그모이드 함수 (활성화 함수)
 *
 * 신경망의 활성화 함수로 사용되는 sigmoid 함수의 고속 구현입니다.
 *
 * @param x 입력값
 * @return 1/(1+e^(-x))의 근사값
 */
float et_fast_sigmoid(float x);

/**
 * @brief 고속 GELU 활성화 함수
 *
 * Gaussian Error Linear Unit 활성화 함수의 고속 구현입니다.
 *
 * @param x 입력값
 * @return GELU(x)의 근사값
 */
float et_fast_gelu(float x);

/**
 * @brief 고속 역제곱근 함수
 *
 * 1/sqrt(x)의 고속 구현입니다. 정규화 연산에 사용됩니다.
 *
 * @param x 입력값 (x > 0)
 * @return 1/sqrt(x)의 근사값
 */
float et_fast_inv_sqrt(float x);

/**
 * @brief 고속 제곱근 함수
 *
 * @param x 입력값 (x >= 0)
 * @return sqrt(x)의 근사값
 */
float et_fast_sqrt(float x);

/**
 * @brief 벡터화된 지수 함수
 *
 * 배열의 모든 요소에 대해 지수 함수를 적용합니다.
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_exp_vec(const float* input, float* output, size_t size);

/**
 * @brief 벡터화된 로그 함수
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_log_vec(const float* input, float* output, size_t size);

/**
 * @brief 벡터화된 tanh 함수
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_tanh_vec(const float* input, float* output, size_t size);

/**
 * @brief 벡터화된 sigmoid 함수
 *
 * @param input 입력 배열
 * @param output 출력 배열
 * @param size 배열 크기
 */
void et_fast_sigmoid_vec(const float* input, float* output, size_t size);

/**
 * @brief 고속 수학 함수 라이브러리 초기화
 *
 * 룩업 테이블을 생성하고 초기화합니다.
 *
 * @return 성공 시 0, 실패 시 음수
 */
int et_fast_math_init(void);

/**
 * @brief 고속 수학 함수 라이브러리 정리
 *
 * 할당된 메모리를 해제합니다.
 */
void et_fast_math_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_FAST_MATH_H