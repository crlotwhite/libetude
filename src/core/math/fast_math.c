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

// =============================================================================
// 음성 특화 수학 함수들 구현 (Voice-Specific Math Functions Implementation)
// =============================================================================

// Hz를 Mel 스케일로 변환
float et_hz_to_mel(float hz) {
    if (hz <= 0.0f) return 0.0f;

    // mel = 2595 * log10(1 + hz / 700)
    return 2595.0f * et_fast_log10(1.0f + hz / 700.0f);
}

// Mel 스케일을 Hz로 변환
float et_mel_to_hz(float mel) {
    if (mel <= 0.0f) return 0.0f;

    // hz = 700 * (10^(mel / 2595) - 1)
    return 700.0f * (et_fast_pow(10.0f, mel / 2595.0f) - 1.0f);
}

// Mel 필터뱅크 생성
int et_create_mel_filterbank(int n_fft, int n_mels, float sample_rate,
                            float fmin, float fmax, float* mel_filters) {
    if (!mel_filters || n_fft <= 0 || n_mels <= 0 || sample_rate <= 0.0f) {
        return -1; // 잘못된 파라미터
    }

    int n_freqs = n_fft / 2 + 1;

    // 모든 필터를 0으로 초기화
    memset(mel_filters, 0, n_mels * n_freqs * sizeof(float));

    // Mel 스케일에서의 최소/최대 주파수
    float mel_fmin = et_hz_to_mel(fmin);
    float mel_fmax = et_hz_to_mel(fmax);

    // Mel 스케일에서 균등하게 분포된 점들 생성
    float* mel_points = (float*)malloc((n_mels + 2) * sizeof(float));
    if (!mel_points) return -1;

    for (int i = 0; i < n_mels + 2; i++) {
        mel_points[i] = mel_fmin + (mel_fmax - mel_fmin) * i / (n_mels + 1);
    }

    // Mel 점들을 Hz로 변환
    float* hz_points = (float*)malloc((n_mels + 2) * sizeof(float));
    if (!hz_points) {
        free(mel_points);
        return -1;
    }

    for (int i = 0; i < n_mels + 2; i++) {
        hz_points[i] = et_mel_to_hz(mel_points[i]);
    }

    // Hz를 FFT 빈 인덱스로 변환
    int* bin_points = (int*)malloc((n_mels + 2) * sizeof(int));
    if (!bin_points) {
        free(mel_points);
        free(hz_points);
        return -1;
    }

    for (int i = 0; i < n_mels + 2; i++) {
        bin_points[i] = (int)(hz_points[i] * n_fft / sample_rate + 0.5f);
        if (bin_points[i] >= n_freqs) bin_points[i] = n_freqs - 1;
    }

    // 삼각형 필터 생성
    for (int m = 0; m < n_mels; m++) {
        int left = bin_points[m];
        int center = bin_points[m + 1];
        int right = bin_points[m + 2];

        // 왼쪽 기울기 (상승)
        for (int k = left; k < center; k++) {
            if (center > left) {
                mel_filters[m * n_freqs + k] = (float)(k - left) / (center - left);
            }
        }

        // 오른쪽 기울기 (하강)
        for (int k = center; k < right; k++) {
            if (right > center) {
                mel_filters[m * n_freqs + k] = (float)(right - k) / (right - center);
            }
        }
    }

    free(mel_points);
    free(hz_points);
    free(bin_points);

    return 0;
}

// 스펙트로그램을 Mel 스펙트로그램으로 변환
void et_spectrogram_to_mel(const float* spectrogram, const float* mel_filters,
                          float* mel_spectrogram, int n_frames, int n_freqs, int n_mels) {
    // 각 시간 프레임에 대해
    for (int t = 0; t < n_frames; t++) {
        // 각 Mel 필터에 대해
        for (int m = 0; m < n_mels; m++) {
            float mel_value = 0.0f;

            // 필터와 스펙트로그램의 내적 계산
            for (int f = 0; f < n_freqs; f++) {
                mel_value += spectrogram[t * n_freqs + f] * mel_filters[m * n_freqs + f];
            }

            mel_spectrogram[t * n_mels + m] = mel_value;
        }
    }
}

// 피치 시프팅을 위한 주파수 스케일링
float et_pitch_shift_frequency(float frequency, float pitch_shift) {
    if (frequency <= 0.0f || pitch_shift <= 0.0f) return frequency;
    return frequency * pitch_shift;
}

// 세미톤을 주파수 비율로 변환
float et_semitones_to_ratio(float semitones) {
    // 1 세미톤 = 2^(1/12) 비율
    return et_fast_pow(2.0f, semitones / 12.0f);
}

// 주파수 비율을 세미톤으로 변환
float et_ratio_to_semitones(float ratio) {
    if (ratio <= 0.0f) return 0.0f;
    // semitones = 12 * log2(ratio)
    return 12.0f * et_fast_log2(ratio);
}

// 해밍 윈도우 생성
void et_hamming_window(float* window, int size) {
    if (!window || size <= 0) return;

    for (int i = 0; i < size; i++) {
        // Hamming: w(n) = 0.54 - 0.46 * cos(2π * n / (N-1))
        float angle = ET_2_PI * i / (size - 1);
        window[i] = 0.54f - 0.46f * et_fast_cos(angle);
    }
}

// 한 윈도우 생성
void et_hann_window(float* window, int size) {
    if (!window || size <= 0) return;

    for (int i = 0; i < size; i++) {
        // Hann: w(n) = 0.5 * (1 - cos(2π * n / (N-1)))
        float angle = ET_2_PI * i / (size - 1);
        window[i] = 0.5f * (1.0f - et_fast_cos(angle));
    }
}

// 블랙만 윈도우 생성
void et_blackman_window(float* window, int size) {
    if (!window || size <= 0) return;

    for (int i = 0; i < size; i++) {
        // Blackman: w(n) = 0.42 - 0.5*cos(2π*n/(N-1)) + 0.08*cos(4π*n/(N-1))
        float angle1 = ET_2_PI * i / (size - 1);
        float angle2 = 2.0f * angle1;
        window[i] = 0.42f - 0.5f * et_fast_cos(angle1) + 0.08f * et_fast_cos(angle2);
    }
}

// 오디오 신호의 RMS 계산
float et_audio_rms(const float* signal, int size) {
    if (!signal || size <= 0) return 0.0f;

    float sum_squares = 0.0f;
    for (int i = 0; i < size; i++) {
        sum_squares += signal[i] * signal[i];
    }

    return et_fast_sqrt(sum_squares / size);
}

// 오디오 신호 정규화
void et_normalize_audio(float* signal, int size, float target_max) {
    if (!signal || size <= 0 || target_max <= 0.0f) return;

    float peak = et_find_peak(signal, size);
    if (peak <= 0.0f) return;

    float scale = target_max / peak;
    for (int i = 0; i < size; i++) {
        signal[i] *= scale;
    }
}

// 오디오 신호의 피크 값 찾기
float et_find_peak(const float* signal, int size) {
    if (!signal || size <= 0) return 0.0f;

    float peak = 0.0f;
    for (int i = 0; i < size; i++) {
        float abs_val = fabsf(signal[i]);
        if (abs_val > peak) {
            peak = abs_val;
        }
    }

    return peak;
}

// 선형 보간
float et_lerp(float a, float b, float t) {
    // 범위 제한
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    return a + t * (b - a);
}

// 코사인 보간
float et_cosine_interp(float a, float b, float t) {
    // 범위 제한
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;

    // 코사인 곡선을 사용한 부드러운 보간
    float cos_t = (1.0f - et_fast_cos(t * ET_PI)) * 0.5f;
    return et_lerp(a, b, cos_t);
}

// 3차 스플라인 보간
float et_cubic_interp(float p0, float p1, float p2, float p3, float t) {
    // 범위 제한
    if (t <= 0.0f) return p1;
    if (t >= 1.0f) return p2;

    // 3차 스플라인 계수 계산
    float a0 = p3 - p2 - p0 + p1;
    float a1 = p0 - p1 - a0;
    float a2 = p2 - p0;
    float a3 = p1;

    float t2 = t * t;
    float t3 = t2 * t;

    return a0 * t3 + a1 * t2 + a2 * t + a3;
}

// dB를 선형 스케일로 변환
float et_db_to_linear(float db) {
    // linear = 10^(db/20)
    return et_fast_pow(10.0f, db / 20.0f);
}

// 선형 스케일을 dB로 변환
float et_linear_to_db(float linear) {
    if (linear <= 0.0f) return -INFINITY;

    // db = 20 * log10(linear)
    return 20.0f * et_fast_log10(linear);
}