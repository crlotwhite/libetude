/**
 * @file windows_simd.c
 * @brief Windows 특화 SIMD 최적화 및 CPU 기능 감지
 *
 * Windows 환경에서 CPU 기능을 감지하고 AVX2/AVX-512 최적화된
 * 행렬 연산 커널을 제공합니다.
 */

#include <windows.h>
#include <intrin.h>
#include <immintrin.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "libetude/types.h"
#include "libetude/platform/windows.h"

// CPU 기능 감지 결과를 저장하는 전역 변수
static ETWindowsCPUFeatures g_cpu_features = {0};
static bool g_cpu_features_initialized = false;

/**
 * @brief CPUID 명령어를 사용하여 CPU 정보를 가져옵니다
 *
 * @param info 4개의 정수 배열 (EAX, EBX, ECX, EDX)
 * @param function_id CPUID 함수 ID
 * @param subfunction_id CPUID 서브함수 ID (선택사항)
 */
static void et_windows_cpuid(int info[4], int function_id, int subfunction_id) {
    __cpuidex(info, function_id, subfunction_id);
}

/**
 * @brief Windows에서 CPU 기능을 감지합니다
 *
 * @return ETWindowsCPUFeatures CPU 기능 정보 구조체
 */
ETWindowsCPUFeatures et_windows_detect_cpu_features(void) {
    if (g_cpu_features_initialized) {
        return g_cpu_features;
    }

    ETWindowsCPUFeatures features = {0};
    int cpu_info[4] = {0};

    // 기본 CPUID 지원 확인
    et_windows_cpuid(cpu_info, 0, 0);
    int max_function_id = cpu_info[0];

    if (max_function_id >= 1) {
        // 기본 기능 정보 (Function 1)
        et_windows_cpuid(cpu_info, 1, 0);

        // ECX 레지스터에서 기능 확인
        int ecx = cpu_info[2];
        int edx = cpu_info[3];

        features.has_sse41 = (ecx & (1 << 19)) != 0;  // SSE4.1
        features.has_avx = (ecx & (1 << 28)) != 0;    // AVX

        // SSE 기본 지원 확인
        features.has_sse = (edx & (1 << 25)) != 0;    // SSE
        features.has_sse2 = (edx & (1 << 26)) != 0;   // SSE2
    }

    if (max_function_id >= 7) {
        // 확장 기능 정보 (Function 7, Sub-function 0)
        et_windows_cpuid(cpu_info, 7, 0);

        // EBX 레지스터에서 AVX2/AVX-512 확인
        int ebx = cpu_info[1];

        features.has_avx2 = (ebx & (1 << 5)) != 0;    // AVX2
        features.has_avx512f = (ebx & (1 << 16)) != 0; // AVX-512 Foundation
        features.has_avx512dq = (ebx & (1 << 17)) != 0; // AVX-512 DQ
        features.has_avx512bw = (ebx & (1 << 30)) != 0; // AVX-512 BW
        features.has_avx512vl = (ebx & (1 << 31)) != 0; // AVX-512 VL
    }

    // OS에서 AVX/AVX-512 지원 확인 (XGETBV 명령어 사용)
    if (features.has_avx) {
        // XCR0 레지스터 확인으로 OS AVX 지원 검증
        uint64_t xcr0 = _xgetbv(0);

        // AVX 상태 저장 지원 확인 (비트 1, 2)
        bool os_avx_support = (xcr0 & 0x6) == 0x6;

        if (!os_avx_support) {
            features.has_avx = false;
            features.has_avx2 = false;
            features.has_avx512f = false;
            features.has_avx512dq = false;
            features.has_avx512bw = false;
            features.has_avx512vl = false;
        } else if (features.has_avx512f) {
            // AVX-512 상태 저장 지원 확인 (비트 5, 6, 7)
            bool os_avx512_support = (xcr0 & 0xE0) == 0xE0;

            if (!os_avx512_support) {
                features.has_avx512f = false;
                features.has_avx512dq = false;
                features.has_avx512bw = false;
                features.has_avx512vl = false;
            }
        }
    }

    g_cpu_features = features;
    g_cpu_features_initialized = true;

    return features;
}

/**
 * @brief CPU 기능 정보를 문자열로 반환합니다
 *
 * @param features CPU 기능 구조체
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 */
void et_windows_cpu_features_to_string(const ETWindowsCPUFeatures* features,
                                     char* buffer, size_t buffer_size) {
    if (!features || !buffer || buffer_size == 0) {
        return;
    }

    snprintf(buffer, buffer_size,
        "CPU Features: SSE=%d, SSE2=%d, SSE4.1=%d, AVX=%d, AVX2=%d, "
        "AVX-512F=%d, AVX-512DQ=%d, AVX-512BW=%d, AVX-512VL=%d",
        features->has_sse, features->has_sse2, features->has_sse41,
        features->has_avx, features->has_avx2, features->has_avx512f,
        features->has_avx512dq, features->has_avx512bw, features->has_avx512vl);
}
/**

 * @brief AVX2를 사용한 단정밀도 행렬 곱셈 (C = A * B)
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_avx2(const float* a, const float* b,
                                        float* c, int m, int n, int k) {
    if (!a || !b || !c || m <= 0 || n <= 0 || k <= 0) {
        return;
    }

    // CPU 기능 확인
    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
    if (!features.has_avx2) {
        // AVX2가 지원되지 않으면 기본 구현으로 폴백
        et_windows_simd_matrix_multiply_fallback(a, b, c, m, n, k);
        return;
    }

    const int avx2_width = 8; // AVX2는 8개의 float를 동시 처리

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j += avx2_width) {
            // 남은 열 수 계산
            int remaining_cols = n - j;
            int cols_to_process = (remaining_cols >= avx2_width) ? avx2_width : remaining_cols;

            __m256 sum = _mm256_setzero_ps();

            // 내적 계산 (k 차원에 대해)
            for (int l = 0; l < k; l++) {
                __m256 a_vec = _mm256_broadcast_ss(&a[i * k + l]);

                if (cols_to_process == avx2_width) {
                    __m256 b_vec = _mm256_loadu_ps(&b[l * n + j]);
                    sum = _mm256_fmadd_ps(a_vec, b_vec, sum);
                } else {
                    // 부분적으로만 로드 (경계 처리)
                    float b_partial[8] = {0};
                    for (int idx = 0; idx < cols_to_process; idx++) {
                        b_partial[idx] = b[l * n + j + idx];
                    }
                    __m256 b_vec = _mm256_loadu_ps(b_partial);
                    sum = _mm256_fmadd_ps(a_vec, b_vec, sum);
                }
            }

            // 결과 저장
            if (cols_to_process == avx2_width) {
                _mm256_storeu_ps(&c[i * n + j], sum);
            } else {
                // 부분적으로만 저장 (경계 처리)
                float result[8];
                _mm256_storeu_ps(result, sum);
                for (int idx = 0; idx < cols_to_process; idx++) {
                    c[i * n + j + idx] = result[idx];
                }
            }
        }
    }
}

/**
 * @brief AVX-512를 사용한 단정밀도 행렬 곱셈 (C = A * B)
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_avx512(const float* a, const float* b,
                                          float* c, int m, int n, int k) {
    if (!a || !b || !c || m <= 0 || n <= 0 || k <= 0) {
        return;
    }

    // CPU 기능 확인
    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
    if (!features.has_avx512f) {
        // AVX-512가 지원되지 않으면 AVX2로 폴백
        et_windows_simd_matrix_multiply_avx2(a, b, c, m, n, k);
        return;
    }

    const int avx512_width = 16; // AVX-512는 16개의 float를 동시 처리

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j += avx512_width) {
            // 남은 열 수 계산
            int remaining_cols = n - j;
            int cols_to_process = (remaining_cols >= avx512_width) ? avx512_width : remaining_cols;

            __m512 sum = _mm512_setzero_ps();

            // 내적 계산 (k 차원에 대해)
            for (int l = 0; l < k; l++) {
                __m512 a_vec = _mm512_set1_ps(a[i * k + l]);

                if (cols_to_process == avx512_width) {
                    __m512 b_vec = _mm512_loadu_ps(&b[l * n + j]);
                    sum = _mm512_fmadd_ps(a_vec, b_vec, sum);
                } else {
                    // 마스크를 사용한 부분 로드
                    __mmask16 mask = (1 << cols_to_process) - 1;
                    __m512 b_vec = _mm512_maskz_loadu_ps(mask, &b[l * n + j]);
                    sum = _mm512_fmadd_ps(a_vec, b_vec, sum);
                }
            }

            // 결과 저장
            if (cols_to_process == avx512_width) {
                _mm512_storeu_ps(&c[i * n + j], sum);
            } else {
                // 마스크를 사용한 부분 저장
                __mmask16 mask = (1 << cols_to_process) - 1;
                _mm512_mask_storeu_ps(&c[i * n + j], mask, sum);
            }
        }
    }
}

/**
 * @brief 기본 행렬 곱셈 구현 (SIMD 미지원 시 폴백)
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_fallback(const float* a, const float* b,
                                            float* c, int m, int n, int k) {
    if (!a || !b || !c || m <= 0 || n <= 0 || k <= 0) {
        return;
    }

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            c[i * n + j] = sum;
        }
    }
}

/**
 * @brief 최적의 SIMD 구현을 자동 선택하여 행렬 곱셈 수행
 *
 * @param a 입력 행렬 A (m x k)
 * @param b 입력 행렬 B (k x n)
 * @param c 출력 행렬 C (m x n)
 * @param m 행렬 A의 행 수
 * @param n 행렬 B의 열 수
 * @param k 행렬 A의 열 수 (= 행렬 B의 행 수)
 */
void et_windows_simd_matrix_multiply_auto(const float* a, const float* b,
                                        float* c, int m, int n, int k) {
    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();

    if (features.has_avx512f) {
        et_windows_simd_matrix_multiply_avx512(a, b, c, m, n, k);
    } else if (features.has_avx2) {
        et_windows_simd_matrix_multiply_avx2(a, b, c, m, n, k);
    } else {
        et_windows_simd_matrix_multiply_fallback(a, b, c, m, n, k);
    }
}/**
 *
@brief AVX2를 사용한 벡터 덧셈 (c = a + b)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param c 출력 벡터 C
 * @param size 벡터 크기
 */
void et_windows_simd_vector_add_avx2(const float* a, const float* b,
                                   float* c, int size) {
    if (!a || !b || !c || size <= 0) {
        return;
    }

    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
    if (!features.has_avx2) {
        et_windows_simd_vector_add_fallback(a, b, c, size);
        return;
    }

    const int avx2_width = 8;
    int i = 0;

    // AVX2로 8개씩 처리
    for (; i <= size - avx2_width; i += avx2_width) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(&c[i], vc);
    }

    // 나머지 요소들 처리
    for (; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

/**
 * @brief AVX-512를 사용한 벡터 덧셈 (c = a + b)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param c 출력 벡터 C
 * @param size 벡터 크기
 */
void et_windows_simd_vector_add_avx512(const float* a, const float* b,
                                     float* c, int size) {
    if (!a || !b || !c || size <= 0) {
        return;
    }

    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
    if (!features.has_avx512f) {
        et_windows_simd_vector_add_avx2(a, b, c, size);
        return;
    }

    const int avx512_width = 16;
    int i = 0;

    // AVX-512로 16개씩 처리
    for (; i <= size - avx512_width; i += avx512_width) {
        __m512 va = _mm512_loadu_ps(&a[i]);
        __m512 vb = _mm512_loadu_ps(&b[i]);
        __m512 vc = _mm512_add_ps(va, vb);
        _mm512_storeu_ps(&c[i], vc);
    }

    // 나머지 요소들 처리
    for (; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

/**
 * @brief 기본 벡터 덧셈 구현 (SIMD 미지원 시 폴백)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param c 출력 벡터 C
 * @param size 벡터 크기
 */
void et_windows_simd_vector_add_fallback(const float* a, const float* b,
                                       float* c, int size) {
    if (!a || !b || !c || size <= 0) {
        return;
    }

    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

/**
 * @brief AVX2를 사용한 벡터 내적 계산
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param size 벡터 크기
 * @return float 내적 결과
 */
float et_windows_simd_vector_dot_avx2(const float* a, const float* b, int size) {
    if (!a || !b || size <= 0) {
        return 0.0f;
    }

    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();
    if (!features.has_avx2) {
        return et_windows_simd_vector_dot_fallback(a, b, size);
    }

    const int avx2_width = 8;
    __m256 sum = _mm256_setzero_ps();
    int i = 0;

    // AVX2로 8개씩 처리
    for (; i <= size - avx2_width; i += avx2_width) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        sum = _mm256_fmadd_ps(va, vb, sum);
    }

    // 수평 덧셈으로 최종 결과 계산
    __m128 sum_high = _mm256_extractf128_ps(sum, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum);
    __m128 sum_128 = _mm_add_ps(sum_high, sum_low);

    sum_128 = _mm_hadd_ps(sum_128, sum_128);
    sum_128 = _mm_hadd_ps(sum_128, sum_128);

    float result = _mm_cvtss_f32(sum_128);

    // 나머지 요소들 처리
    for (; i < size; i++) {
        result += a[i] * b[i];
    }

    return result;
}

/**
 * @brief 기본 벡터 내적 구현 (SIMD 미지원 시 폴백)
 *
 * @param a 입력 벡터 A
 * @param b 입력 벡터 B
 * @param size 벡터 크기
 * @return float 내적 결과
 */
float et_windows_simd_vector_dot_fallback(const float* a, const float* b, int size) {
    if (!a || !b || size <= 0) {
        return 0.0f;
    }

    float result = 0.0f;
    for (int i = 0; i < size; i++) {
        result += a[i] * b[i];
    }
    return result;
}

/**
 * @brief Windows SIMD 모듈 초기화
 *
 * @return ETResult 초기화 결과
 */
ETResult et_windows_simd_init(void) {
    // CPU 기능 감지 수행
    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();

    // 로그 출력 (디버그 빌드에서만)
    #ifdef _DEBUG
    char feature_str[512];
    et_windows_cpu_features_to_string(&features, feature_str, sizeof(feature_str));
    OutputDebugStringA("LibEtude Windows SIMD: ");
    OutputDebugStringA(feature_str);
    OutputDebugStringA("\n");
    #endif

    return ET_RESULT_SUCCESS;
}

/**
 * @brief Windows SIMD 모듈 정리
 */
void et_windows_simd_finalize(void) {
    g_cpu_features_initialized = false;
    memset(&g_cpu_features, 0, sizeof(g_cpu_features));
}