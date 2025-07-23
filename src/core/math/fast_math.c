/**
 * @file fast_math.c
 * @brief FastApprox 기반 고속 수학 함수 구현
 *
 * 음성 합성에 최적화된 고속 근사 수학 함수들을 구현합니다.
 * FastApprox 알고리즘과 룩업 테이블을 사용하여 성능을 최적화합니다.
 */

#include "libetude/fast_math.h"
#include "libetude/types.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// 상수 정의
#define ET_PI 3.14159265358979323846f
#define ET_PI_2 1.57079632679489661923f
#define ET_PI_4 0.78539816339744830962f
#define ET_2_PI 6.28318530717958647692f
#define ET_INV_PI 0.31830988618379067154f
#define ET_E 2.71828182845904523536f
#define ET_LOG2_E 1.44269504088896340736f
#define ET_LOG10_E 0.43429448190325182765f

// 룩업 테이블 크기
#define SIN_TABLE_SIZE 1024
#define SIN_TABLE_MASK (SIN_TABLE_SIZE - 1)

// 전역 룩업 테이블
static float* sin_table = NULL;
static int fast_math_initialized = 0;

// 유틸리티 함수들
static inline uint32_t float_to_uint32(float f) {
    union { float f; uint32_t i; } u;
    u.f = f;
    return u.i;
}

static inline float uint32_to_float(uint32_t i) {
    union { float f; uint32_t i; } u;
    u.i = i;
    return u.f;
}

// FastApprox 기반 지수 함수
float et_fast_exp(float x) {
    // 범위 제한 (-88 ~ 88)
    if (x > 88.0f) return INFINITY;
    if (x < -88.0f) return 0.0f;

    // FastApprox exp 구현
    // exp(x) ≈ 2^(x * log2(e))
    float y = x * ET_LOG2_E;

    // 정수 부분과 소수 부분 분리
    int32_t i = (int32_t)y;
    float f = y - i;

    // 2^f 근사 (f는 [0, 1) 범위)
    // 2^f ≈ 1 + f * (0.693147 + f * (0.240226 + f * 0.0520143))
    float f2 = f * f;
    float f3 = f2 * f;
    float approx_2_f = 1.0f + f * (0.693147f + f * (0.240226f + f * 0.0520143f));

    // 2^i * 2^f
    // 비트 조작을 통한 2^i 계산
    uint32_t exp_bits = (uint32_t)(i + 127) << 23;
    float pow2_i = uint32_to_float(exp_bits);

    return pow2_i * approx_2_f;
}

// FastApprox 기반 자연로그 함수
float et_fast_log(float x) {
    if (x < 0.0f) return NAN;
    if (x == 0.0f) return -INFINITY;
    if (x == 1.0f) return 0.0f;

    // 비트 조작을 통한 빠른 log 근사
    uint32_t bits = float_to_uint32(x);

    // 지수 부분 추출
    int32_t exp = ((int32_t)(bits >> 23) & 0xFF) - 127;

    // 가수 부분을 [1, 2) 범위로 정규화
    uint32_t mantissa_bits = (bits & 0x007FFFFF) | 0x3F800000;
    float mantissa = uint32_to_float(mantissa_bits);

    // log(mantissa) 근사 (mantissa는 [1, 2) 범위)
    float m = mantissa - 1.0f;
    float m2 = m * m;
    float m3 = m2 * m;
    float log_mantissa = m - 0.5f * m2 + 0.333333f * m3 - 0.25f * m2 * m2;

    // log(x) = log(2^exp * mantissa) = exp * log(2) + log(mantissa)
    return exp * 0.693147f + log_mantissa;
}

// 밑이 2인 로그
float et_fast_log2(float x) {
    return et_fast_log(x) * ET_LOG2_E;
}

// 밑이 10인 로그
float et_fast_log10(float x) {
    return et_fast_log(x) * ET_LOG10_E;
}

// 고속 거듭제곱
float et_fast_pow(float base, float exponent) {
    if (base <= 0.0f) {
        if (base == 0.0f && exponent > 0.0f) return 0.0f;
        return NAN; // 음수의 실수 거듭제곱은 복소수
    }

    // base^exponent = exp(exponent * log(base))
    return et_fast_exp(exponent * et_fast_log(base));
}

// 룩업 테이블 기반 사인 함수
float et_fast_sin(float x) {
    if (!fast_math_initialized) {
        et_fast_math_init();
    }

    // 입력을 [0, 2π) 범위로 정규화
    x = fmodf(x, ET_2_PI);
    if (x < 0.0f) x += ET_2_PI;

    // 테이블 인덱스 계산
    float table_pos = x * (SIN_TABLE_SIZE / ET_2_PI);
    int index = (int)table_pos;
    float frac = table_pos - index;

    // 선형 보간
    int next_index = (index + 1) & SIN_TABLE_MASK;
    return sin_table[index] * (1.0f - frac) + sin_table[next_index] * frac;
}

// 룩업 테이블 기반 코사인 함수
float et_fast_cos(float x) {
    // cos(x) = sin(x + π/2)
    return et_fast_sin(x + ET_PI_2);
}

// 고속 탄젠트 함수
float et_fast_tan(float x) {
    float sin_x = et_fast_sin(x);
    float cos_x = et_fast_cos(x);

    if (fabsf(cos_x) < 1e-7f) {
        return (sin_x > 0) ? INFINITY : -INFINITY;
    }

    return sin_x / cos_x;
}

// 고속 아크탄젠트 함수
float et_fast_atan(float x) {
    // 범위 축소를 위한 절댓값 사용
    float abs_x = fabsf(x);
    int sign = (x < 0) ? -1 : 1;

    float result;

    if (abs_x > 1.0f) {
        // atan(x) = π/2 - atan(1/x) for |x| > 1
        float inv_x = 1.0f / abs_x;
        float inv_x2 = inv_x * inv_x;
        result = ET_PI_2 - inv_x * (1.0f - inv_x2 * (0.333333f - inv_x2 * 0.2f));
    } else {
        // 테일러 급수 근사
        float x2 = abs_x * abs_x;
        result = abs_x * (1.0f - x2 * (0.333333f - x2 * (0.2f - x2 * 0.142857f)));
    }

    return sign * result;
}

// 고속 아크탄젠트2 함수
float et_fast_atan2(float y, float x) {
    if (x == 0.0f) {
        if (y > 0.0f) return ET_PI_2;
        if (y < 0.0f) return -ET_PI_2;
        return 0.0f; // 정의되지 않음, 하지만 0 반환
    }

    float atan_val = et_fast_atan(y / x);

    if (x > 0.0f) {
        return atan_val;
    } else if (y >= 0.0f) {
        return atan_val + ET_PI;
    } else {
        return atan_val - ET_PI;
    }
}

// 고속 하이퍼볼릭 탄젠트 (활성화 함수)
float et_fast_tanh(float x) {
    // 범위 제한
    if (x > 5.0f) return 1.0f;
    if (x < -5.0f) return -1.0f;

    // tanh(x) = (e^(2x) - 1) / (e^(2x) + 1)
    // 또는 tanh(x) = (1 - e^(-2x)) / (1 + e^(-2x))
    float exp_2x = et_fast_exp(2.0f * x);
    return (exp_2x - 1.0f) / (exp_2x + 1.0f);
}

// 고속 시그모이드 함수 (활성화 함수)
float et_fast_sigmoid(float x) {
    // 범위 제한
    if (x > 10.0f) return 1.0f;
    if (x < -10.0f) return 0.0f;

    // sigmoid(x) = 1 / (1 + e^(-x))
    return 1.0f / (1.0f + et_fast_exp(-x));
}

// 고속 GELU 활성화 함수
float et_fast_gelu(float x) {
    // GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x^3)))
    float x3 = x * x * x;
    float inner = 0.797885f * (x + 0.044715f * x3); // sqrt(2/π) ≈ 0.797885
    return 0.5f * x * (1.0f + et_fast_tanh(inner));
}

// 고속 역제곱근 (Quake III 알고리즘 개선)
float et_fast_inv_sqrt(float x) {
    if (x <= 0.0f) return INFINITY;

    // Quake III 역제곱근 알고리즘
    float x_half = 0.5f * x;
    uint32_t i = float_to_uint32(x);
    i = 0x5f3759df - (i >> 1); // 매직 넘버
    float y = uint32_to_float(i);

    // 뉴턴-랩슨 반복 (1회)
    y = y * (1.5f - x_half * y * y);

    return y;
}

// 고속 제곱근
float et_fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    return x * et_fast_inv_sqrt(x);
}

// 벡터화된 지수 함수
void et_fast_exp_vec(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = et_fast_exp(input[i]);
    }
}

// 벡터화된 로그 함수
void et_fast_log_vec(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = et_fast_log(input[i]);
    }
}

// 벡터화된 tanh 함수
void et_fast_tanh_vec(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = et_fast_tanh(input[i]);
    }
}

// 벡터화된 sigmoid 함수
void et_fast_sigmoid_vec(const float* input, float* output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = et_fast_sigmoid(input[i]);
    }
}

// 고속 수학 함수 라이브러리 초기화
int et_fast_math_init(void) {
    if (fast_math_initialized) {
        return 0; // 이미 초기화됨
    }

    // 사인 룩업 테이블 할당
    sin_table = (float*)malloc(SIN_TABLE_SIZE * sizeof(float));
    if (!sin_table) {
        return -1; // 메모리 할당 실패
    }

    // 사인 테이블 생성
    for (int i = 0; i < SIN_TABLE_SIZE; i++) {
        float angle = (float)i * ET_2_PI / SIN_TABLE_SIZE;
        sin_table[i] = sinf(angle);
    }

    fast_math_initialized = 1;
    return 0;
}

// 고속 수학 함수 라이브러리 정리
void et_fast_math_cleanup(void) {
    if (sin_table) {
        free(sin_table);
        sin_table = NULL;
    }
    fast_math_initialized = 0;
}