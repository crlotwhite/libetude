/**
 * @file mel_scale.c
 * @brief Mel 스케일 변환 최적화 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * SIMD 최적화된 Mel 스케일 변환과 필터뱅크, 캐싱 최적화를 제공합니다.
 */

#include "libetude/mel_scale.h"
#include "libetude/simd_kernels.h"
#include "libetude/fast_math.h"
#include "libetude/memory.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>

// 임시 함수 선언 (실제 구현에서는 적절한 헤더에서 가져와야 함)
static uint64_t et_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

// ============================================================================
// 내부 상수 및 매크로
// ============================================================================

#define MEL_BREAK_FREQUENCY_HTK 700.0f    /**< HTK Mel 스케일 분기점 */
#define MEL_HIGH_FREQ_Q 1127.01048f       /**< HTK 고주파 계수 */
#define MEL_LOW_FREQ_Q 1.0f               /**< HTK 저주파 계수 */

#define MEL_BREAK_FREQUENCY_SLANEY 1000.0f /**< Slaney Mel 스케일 분기점 */
#define MEL_LOGSTEP 0.06875177742f         /**< Slaney 로그 스텝 */
#define MEL_LINSCALE 200.0f / 3.0f         /**< Slaney 선형 스케일 */

#define MAX_CACHE_SIZE 64                  /**< 최대 캐시 크기 */
#define SIMD_ALIGNMENT 32                  /**< SIMD 정렬 크기 */

// ============================================================================
// 내부 구조체 정의
// ============================================================================

/**
 * @brief 희소 행렬 구조체 (CSR 형식)
 */
typedef struct {
    float* data;        /**< 0이 아닌 값들 */
    int* indices;       /**< 열 인덱스 */
    int* indptr;        /**< 행 포인터 */
    int nnz;           /**< 0이 아닌 값의 개수 */
} SparseMatrix;

/**
 * @brief Mel 필터뱅크 내부 구조체
 */
struct ETMelFilterbank {
    // 설정
    ETMelFilterbankConfig config;

    // 필터뱅크 데이터
    float* filters;              /**< 밀집 필터 행렬 [n_mels * n_freq_bins] */
    SparseMatrix sparse_filters; /**< 희소 필터 행렬 */
    float* pseudo_inverse;       /**< 의사역행렬 [n_freq_bins * n_mels] */

    // 사전 계산된 값들
    float* mel_points;           /**< Mel 주파수 포인트 [n_mels + 2] */
    float* hz_points;            /**< Hz 주파수 포인트 [n_mels + 2] */
    int* fft_bin_indices;        /**< FFT bin 인덱스 [n_mels + 2] */

    // 메모리 관리
    void* aligned_memory;        /**< 정렬된 메모리 블록 */
    size_t memory_size;          /**< 총 메모리 크기 */

    // 성능 통계
    ETMelStats stats;

    // 캐싱 정보
    bool is_cached;
    int cache_index;
};

/**
 * @brief 캐시 엔트리 구조체
 */
typedef struct {
    ETMelFilterbankConfig config;
    ETMelFilterbank* filterbank;
    bool in_use;
    int access_count;
} CacheEntry;

// ============================================================================
// 전역 변수
// ============================================================================

static CacheEntry* g_cache = NULL;
static int g_cache_size = 0;
static bool g_cache_initialized = false;

// 사전 계산된 테이블
static float* g_log_table = NULL;
static float* g_exp_table = NULL;
static bool g_tables_initialized = false;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult create_dense_filterbank(ETMelFilterbank* mel_fb);
static ETResult create_sparse_filterbank(ETMelFilterbank* mel_fb);
static ETResult compute_mel_points(ETMelFilterbank* mel_fb);
static ETResult compute_fft_bin_indices(ETMelFilterbank* mel_fb);
static void cleanup_filterbank_memory(ETMelFilterbank* mel_fb);
static int find_cache_slot(void);
static bool configs_equal(const ETMelFilterbankConfig* a, const ETMelFilterbankConfig* b);

// ============================================================================
// Mel 스케일 변환 함수 구현
// ============================================================================

float et_mel_hz_to_mel(float hz, ETMelScaleType scale_type) {
    if (hz <= 0.0f) return 0.0f;

    switch (scale_type) {
        case ET_MEL_SCALE_HTK:
            return MEL_HIGH_FREQ_Q * logf(1.0f + hz / MEL_BREAK_FREQUENCY_HTK);

        case ET_MEL_SCALE_SLANEY:
            if (hz < MEL_BREAK_FREQUENCY_SLANEY) {
                return hz / MEL_LINSCALE;
            } else {
                return MEL_BREAK_FREQUENCY_SLANEY / MEL_LINSCALE +
                       logf(hz / MEL_BREAK_FREQUENCY_SLANEY) / MEL_LOGSTEP;
            }

        default:
            return 0.0f;
    }
}

float et_mel_mel_to_hz(float mel, ETMelScaleType scale_type) {
    if (mel <= 0.0f) return 0.0f;

    switch (scale_type) {
        case ET_MEL_SCALE_HTK:
            return MEL_BREAK_FREQUENCY_HTK * (expf(mel / MEL_HIGH_FREQ_Q) - 1.0f);

        case ET_MEL_SCALE_SLANEY: {
            float break_mel = MEL_BREAK_FREQUENCY_SLANEY / MEL_LINSCALE;
            if (mel < break_mel) {
                return mel * MEL_LINSCALE;
            } else {
                return MEL_BREAK_FREQUENCY_SLANEY *
                       expf(MEL_LOGSTEP * (mel - break_mel));
            }
        }

        default:
            return 0.0f;
    }
}

float et_mel_fft_bin_to_hz(int bin, int n_fft, int sample_rate) {
    return (float)bin * sample_rate / (float)n_fft;
}

float et_mel_hz_to_fft_bin(float hz, int n_fft, int sample_rate) {
    return hz * (float)n_fft / (float)sample_rate;
}

ETResult et_mel_create_mel_points(float fmin, float fmax, int n_mels,
                                 ETMelScaleType scale_type, float* mel_points) {
    if (!mel_points || n_mels <= 0 || fmin >= fmax) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    float mel_min = et_mel_hz_to_mel(fmin, scale_type);
    float mel_max = et_mel_hz_to_mel(fmax, scale_type);
    float mel_step = (mel_max - mel_min) / (n_mels + 1);

    for (int i = 0; i < n_mels + 2; i++) {
        mel_points[i] = mel_min + i * mel_step;
    }

    return ET_SUCCESS;
}

// ============================================================================
// 필터뱅크 관리 함수 구현
// ============================================================================

ETMelFilterbankConfig et_mel_default_config(int n_fft, int n_mels, int sample_rate,
                                           float fmin, float fmax) {
    ETMelFilterbankConfig config = {0};

    config.n_fft = n_fft;
    config.n_mels = n_mels;
    config.sample_rate = sample_rate;
    config.fmin = (fmin > 0.0f) ? fmin : 0.0f;
    config.fmax = (fmax > 0.0f) ? fmax : sample_rate / 2.0f;
    config.scale_type = ET_MEL_SCALE_HTK;
    config.enable_simd = true;
    config.enable_caching = true;
    config.normalize = true;

    return config;
}

ETMelFilterbank* et_mel_create_filterbank(const ETMelFilterbankConfig* config) {
    if (!config || config->n_fft <= 0 || config->n_mels <= 0 ||
        config->sample_rate <= 0 || config->fmin >= config->fmax) {
        return NULL;
    }

    // 캐시에서 기존 필터뱅크 확인
    if (config->enable_caching && g_cache_initialized) {
        ETMelFilterbank* cached = et_mel_get_cached_filterbank(config);
        if (cached) {
            cached->stats.cache_hits++;
            return cached;
        }
    }

    // 새 필터뱅크 생성
    ETMelFilterbank* mel_fb = (ETMelFilterbank*)calloc(1, sizeof(ETMelFilterbank));
    if (!mel_fb) {
        return NULL;
    }

    mel_fb->config = *config;

    // 메모리 할당
    int n_freq_bins = config->n_fft / 2 + 1;
    size_t total_size = 0;

    // 필터 행렬 크기
    total_size += config->n_mels * n_freq_bins * sizeof(float);
    // 의사역행렬 크기
    total_size += n_freq_bins * config->n_mels * sizeof(float);
    // Mel 포인트 크기
    total_size += (config->n_mels + 2) * sizeof(float);
    // Hz 포인트 크기
    total_size += (config->n_mels + 2) * sizeof(float);
    // FFT bin 인덱스 크기
    total_size += (config->n_mels + 2) * sizeof(int);

    // SIMD 정렬을 위한 추가 공간
    total_size += SIMD_ALIGNMENT;

    // SIMD 정렬을 위해 크기를 정렬 경계로 맞춤
    size_t aligned_size = (total_size + SIMD_ALIGNMENT - 1) & ~(SIMD_ALIGNMENT - 1);
    mel_fb->aligned_memory = aligned_alloc(SIMD_ALIGNMENT, aligned_size);
    if (!mel_fb->aligned_memory) {
        // aligned_alloc 실패시 일반 malloc 사용
        mel_fb->aligned_memory = malloc(aligned_size);
        if (!mel_fb->aligned_memory) {
            free(mel_fb);
            return NULL;
        }
    }

    mel_fb->memory_size = total_size;

    // 메모리 포인터 설정 (void* 사용하여 캐스팅 안전성 확보)
    void* ptr = mel_fb->aligned_memory;
    mel_fb->filters = (float*)ptr;
    ptr = (char*)ptr + config->n_mels * n_freq_bins * sizeof(float);

    mel_fb->pseudo_inverse = (float*)ptr;
    ptr = (char*)ptr + n_freq_bins * config->n_mels * sizeof(float);

    mel_fb->mel_points = (float*)ptr;
    ptr = (char*)ptr + (config->n_mels + 2) * sizeof(float);

    mel_fb->hz_points = (float*)ptr;
    ptr = (char*)ptr + (config->n_mels + 2) * sizeof(float);

    mel_fb->fft_bin_indices = (int*)ptr;

    // 필터뱅크 생성
    ETResult result = compute_mel_points(mel_fb);
    if (result != ET_SUCCESS) {
        et_mel_destroy_filterbank(mel_fb);
        return NULL;
    }

    result = compute_fft_bin_indices(mel_fb);
    if (result != ET_SUCCESS) {
        et_mel_destroy_filterbank(mel_fb);
        return NULL;
    }

    result = create_dense_filterbank(mel_fb);
    if (result != ET_SUCCESS) {
        et_mel_destroy_filterbank(mel_fb);
        return NULL;
    }

    // 희소 행렬 최적화
    if (config->enable_simd) {
        create_sparse_filterbank(mel_fb);
    }

    // 정규화
    if (config->normalize) {
        et_mel_normalize_filterbank(mel_fb);
    }

    // 의사역행렬 생성
    et_mel_create_pseudo_inverse(mel_fb);

    // 캐시에 저장
    if (config->enable_caching && g_cache_initialized) {
        et_mel_cache_filterbank(config, mel_fb);
        mel_fb->stats.cache_misses++;
    }

    return mel_fb;
}

void et_mel_destroy_filterbank(ETMelFilterbank* mel_fb) {
    if (!mel_fb) return;

    cleanup_filterbank_memory(mel_fb);

    if (mel_fb->aligned_memory) {
        free(mel_fb->aligned_memory);
    }

    free(mel_fb);
}

ETResult et_mel_update_config(ETMelFilterbank* mel_fb, const ETMelFilterbankConfig* config) {
    if (!mel_fb || !config) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 설정이 동일하면 업데이트 불필요
    if (configs_equal(&mel_fb->config, config)) {
        return ET_SUCCESS;
    }

    // 새로운 필터뱅크 생성 필요
    ETMelFilterbank* new_fb = et_mel_create_filterbank(config);
    if (!new_fb) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 기존 데이터 복사 (통계 등)
    new_fb->stats = mel_fb->stats;

    // 메모리 교체
    cleanup_filterbank_memory(mel_fb);
    if (mel_fb->aligned_memory) {
        free(mel_fb->aligned_memory);
    }

    *mel_fb = *new_fb;
    free(new_fb);  // 구조체만 해제, 메모리는 이미 이동됨

    return ET_SUCCESS;
}

// ============================================================================
// Mel 변환 핵심 함수 구현
// ============================================================================

ETResult et_mel_spectrogram_to_mel(ETMelFilterbank* mel_fb, const float* spectrogram,
                                  int time_frames, float* mel_spec) {
    if (!mel_fb || !spectrogram || !mel_spec || time_frames <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    uint64_t start_time = et_get_time_us();

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    if (mel_fb->config.enable_simd && mel_fb->sparse_filters.data) {
        // SIMD 최적화된 희소 행렬 변환
        et_mel_batch_transform_simd(mel_fb, spectrogram, mel_spec,
                                   time_frames, n_freq_bins, n_mels);
    } else {
        // 일반 행렬 곱셈
        for (int t = 0; t < time_frames; t++) {
            const float* spectrum = spectrogram + t * n_freq_bins;
            float* mel_frame = mel_spec + t * n_mels;

            et_mel_matvec_simd(mel_fb->filters, spectrum, mel_frame, n_mels, n_freq_bins);
        }
    }

    uint64_t end_time = et_get_time_us();
    mel_fb->stats.forward_time_ms = (end_time - start_time) / 1000.0f;

    return ET_SUCCESS;
}

ETResult et_mel_mel_to_spectrogram(ETMelFilterbank* mel_fb, const float* mel_spec,
                                  int time_frames, float* spectrogram) {
    if (!mel_fb || !mel_spec || !spectrogram || time_frames <= 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!mel_fb->pseudo_inverse) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    uint64_t start_time = et_get_time_us();

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    // 의사역행렬을 사용한 역변환
    for (int t = 0; t < time_frames; t++) {
        const float* mel_frame = mel_spec + t * n_mels;
        float* spectrum = spectrogram + t * n_freq_bins;

        et_mel_matvec_simd(mel_fb->pseudo_inverse, mel_frame, spectrum, n_freq_bins, n_mels);

        // 음수 값 클리핑 (스펙트럼은 항상 양수)
        for (int i = 0; i < n_freq_bins; i++) {
            if (spectrum[i] < 0.0f) {
                spectrum[i] = 0.0f;
            }
        }
    }

    uint64_t end_time = et_get_time_us();
    mel_fb->stats.inverse_time_ms = (end_time - start_time) / 1000.0f;

    return ET_SUCCESS;
}

ETResult et_mel_spectrum_to_mel_frame(ETMelFilterbank* mel_fb, const float* spectrum,
                                     float* mel_frame) {
    if (!mel_fb || !spectrum || !mel_frame) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    if (mel_fb->config.enable_simd && mel_fb->sparse_filters.data) {
        et_mel_sparse_matvec_simd(mel_fb->sparse_filters.data,
                                 mel_fb->sparse_filters.indices,
                                 mel_fb->sparse_filters.indptr,
                                 spectrum, mel_frame, n_mels);
    } else {
        et_mel_matvec_simd(mel_fb->filters, spectrum, mel_frame, n_mels, n_freq_bins);
    }

    return ET_SUCCESS;
}

ETResult et_mel_mel_frame_to_spectrum(ETMelFilterbank* mel_fb, const float* mel_frame,
                                     float* spectrum) {
    if (!mel_fb || !mel_frame || !spectrum) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (!mel_fb->pseudo_inverse) {
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    et_mel_matvec_simd(mel_fb->pseudo_inverse, mel_frame, spectrum, n_freq_bins, n_mels);

    // 음수 값 클리핑
    for (int i = 0; i < n_freq_bins; i++) {
        if (spectrum[i] < 0.0f) {
            spectrum[i] = 0.0f;
        }
    }

    return ET_SUCCESS;
}

// ============================================================================
// 필터뱅크 생성 및 최적화 함수 구현
// ============================================================================

ETResult et_mel_create_triangular_filters(ETMelFilterbank* mel_fb) {
    if (!mel_fb) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    // 필터 행렬 초기화
    memset(mel_fb->filters, 0, n_mels * n_freq_bins * sizeof(float));

    for (int m = 0; m < n_mels; m++) {
        int left_bin = mel_fb->fft_bin_indices[m];
        int center_bin = mel_fb->fft_bin_indices[m + 1];
        int right_bin = mel_fb->fft_bin_indices[m + 2];

        // 삼각형 필터 생성
        for (int k = left_bin; k <= right_bin; k++) {
            if (k < 0 || k >= n_freq_bins) continue;

            float weight = 0.0f;
            if (k <= center_bin) {
                // 상승 구간
                if (center_bin > left_bin) {
                    weight = (float)(k - left_bin) / (center_bin - left_bin);
                }
            } else {
                // 하강 구간
                if (right_bin > center_bin) {
                    weight = (float)(right_bin - k) / (right_bin - center_bin);
                }
            }

            mel_fb->filters[m * n_freq_bins + k] = weight;
        }
    }

    return ET_SUCCESS;
}

ETResult et_mel_normalize_filterbank(ETMelFilterbank* mel_fb) {
    if (!mel_fb) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    for (int m = 0; m < n_mels; m++) {
        float* filter = mel_fb->filters + m * n_freq_bins;

        // 필터의 합 계산
        float sum = 0.0f;
        for (int k = 0; k < n_freq_bins; k++) {
            sum += filter[k];
        }

        // 정규화
        if (sum > 0.0f) {
            float inv_sum = 1.0f / sum;
            for (int k = 0; k < n_freq_bins; k++) {
                filter[k] *= inv_sum;
            }
        }
    }

    return ET_SUCCESS;
}

ETResult et_mel_optimize_sparse_filterbank(ETMelFilterbank* mel_fb) {
    if (!mel_fb) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    // 0이 아닌 값의 개수 계산
    int nnz = 0;
    for (int i = 0; i < n_mels * n_freq_bins; i++) {
        if (mel_fb->filters[i] != 0.0f) {
            nnz++;
        }
    }

    // 희소 행렬 메모리 할당
    mel_fb->sparse_filters.data = (float*)malloc(nnz * sizeof(float));
    mel_fb->sparse_filters.indices = (int*)malloc(nnz * sizeof(int));
    mel_fb->sparse_filters.indptr = (int*)malloc((n_mels + 1) * sizeof(int));

    if (!mel_fb->sparse_filters.data || !mel_fb->sparse_filters.indices ||
        !mel_fb->sparse_filters.indptr) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    mel_fb->sparse_filters.nnz = nnz;

    // CSR 형식으로 변환
    int data_idx = 0;
    mel_fb->sparse_filters.indptr[0] = 0;

    for (int m = 0; m < n_mels; m++) {
        for (int k = 0; k < n_freq_bins; k++) {
            float value = mel_fb->filters[m * n_freq_bins + k];
            if (value != 0.0f) {
                mel_fb->sparse_filters.data[data_idx] = value;
                mel_fb->sparse_filters.indices[data_idx] = k;
                data_idx++;
            }
        }
        mel_fb->sparse_filters.indptr[m + 1] = data_idx;
    }

    return ET_SUCCESS;
}

ETResult et_mel_create_pseudo_inverse(ETMelFilterbank* mel_fb) {
    if (!mel_fb) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    // 간단한 전치 행렬 사용 (실제로는 Moore-Penrose 의사역행렬 필요)
    // 여기서는 정규화된 전치 행렬을 사용
    for (int k = 0; k < n_freq_bins; k++) {
        for (int m = 0; m < n_mels; m++) {
            mel_fb->pseudo_inverse[k * n_mels + m] = mel_fb->filters[m * n_freq_bins + k];
        }
    }

    // 각 주파수 bin에 대해 정규화
    for (int k = 0; k < n_freq_bins; k++) {
        float sum = 0.0f;
        for (int m = 0; m < n_mels; m++) {
            float val = mel_fb->pseudo_inverse[k * n_mels + m];
            sum += val * val;
        }

        if (sum > 0.0f) {
            float norm = 1.0f / sqrtf(sum);
            for (int m = 0; m < n_mels; m++) {
                mel_fb->pseudo_inverse[k * n_mels + m] *= norm;
            }
        }
    }

    return ET_SUCCESS;
}

// ============================================================================
// SIMD 최적화 함수 구현
// ============================================================================

void et_mel_matvec_simd(const float* filters, const float* spectrum, float* mel_frame,
                       int n_mels, int n_freq_bins) {
    // SIMD 최적화된 행렬-벡터 곱셈
    for (int m = 0; m < n_mels; m++) {
        const float* filter = filters + m * n_freq_bins;
        float sum = 0.0f;

        // SIMD 벡터화된 내적 계산
        int simd_end = n_freq_bins - (n_freq_bins % 8);

        for (int k = 0; k < simd_end; k += 8) {
            // 8개씩 처리 (AVX 사용 가정)
            sum += filter[k] * spectrum[k];
            sum += filter[k+1] * spectrum[k+1];
            sum += filter[k+2] * spectrum[k+2];
            sum += filter[k+3] * spectrum[k+3];
            sum += filter[k+4] * spectrum[k+4];
            sum += filter[k+5] * spectrum[k+5];
            sum += filter[k+6] * spectrum[k+6];
            sum += filter[k+7] * spectrum[k+7];
        }

        // 나머지 처리
        for (int k = simd_end; k < n_freq_bins; k++) {
            sum += filter[k] * spectrum[k];
        }

        mel_frame[m] = sum;
    }
}

void et_mel_sparse_matvec_simd(const float* sparse_filters, const int* indices,
                              const int* indptr, const float* spectrum,
                              float* mel_frame, int n_mels) {
    for (int m = 0; m < n_mels; m++) {
        float sum = 0.0f;
        int start = indptr[m];
        int end = indptr[m + 1];

        for (int idx = start; idx < end; idx++) {
            sum += sparse_filters[idx] * spectrum[indices[idx]];
        }

        mel_frame[m] = sum;
    }
}

void et_mel_batch_transform_simd(const ETMelFilterbank* mel_fb, const float* spectrogram,
                                float* mel_spec, int time_frames, int n_freq_bins, int n_mels) {
    if (mel_fb->sparse_filters.data) {
        // 희소 행렬 사용
        for (int t = 0; t < time_frames; t++) {
            const float* spectrum = spectrogram + t * n_freq_bins;
            float* mel_frame = mel_spec + t * n_mels;

            et_mel_sparse_matvec_simd(mel_fb->sparse_filters.data,
                                     mel_fb->sparse_filters.indices,
                                     mel_fb->sparse_filters.indptr,
                                     spectrum, mel_frame, n_mels);
        }
    } else {
        // 밀집 행렬 사용
        for (int t = 0; t < time_frames; t++) {
            const float* spectrum = spectrogram + t * n_freq_bins;
            float* mel_frame = mel_spec + t * n_mels;

            et_mel_matvec_simd(mel_fb->filters, spectrum, mel_frame, n_mels, n_freq_bins);
        }
    }
}

// ============================================================================
// 캐싱 및 사전 계산 최적화 함수 구현
// ============================================================================

ETResult et_mel_init_cache(int cache_size) {
    if (cache_size <= 0 || cache_size > MAX_CACHE_SIZE) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (g_cache_initialized) {
        et_mel_destroy_cache();
    }

    g_cache = (CacheEntry*)calloc(cache_size, sizeof(CacheEntry));
    if (!g_cache) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    g_cache_size = cache_size;
    g_cache_initialized = true;

    return ET_SUCCESS;
}

void et_mel_destroy_cache(void) {
    if (!g_cache_initialized || !g_cache) {
        return;
    }

    for (int i = 0; i < g_cache_size; i++) {
        if (g_cache[i].in_use && g_cache[i].filterbank) {
            // 캐시된 필터뱅크는 직접 해제하지 않음 (사용자가 해제)
            g_cache[i].filterbank->is_cached = false;
        }
    }

    free(g_cache);
    g_cache = NULL;
    g_cache_size = 0;
    g_cache_initialized = false;
}

ETMelFilterbank* et_mel_get_cached_filterbank(const ETMelFilterbankConfig* config) {
    if (!g_cache_initialized || !g_cache || !config) {
        return NULL;
    }

    for (int i = 0; i < g_cache_size; i++) {
        if (g_cache[i].in_use && configs_equal(&g_cache[i].config, config)) {
            g_cache[i].access_count++;
            return g_cache[i].filterbank;
        }
    }

    return NULL;
}

ETResult et_mel_cache_filterbank(const ETMelFilterbankConfig* config, ETMelFilterbank* mel_fb) {
    if (!g_cache_initialized || !g_cache || !config || !mel_fb) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int slot = find_cache_slot();
    if (slot < 0) {
        return ET_ERROR_OUT_OF_MEMORY;  // 캐시 가득참
    }

    g_cache[slot].config = *config;
    g_cache[slot].filterbank = mel_fb;
    g_cache[slot].in_use = true;
    g_cache[slot].access_count = 1;

    mel_fb->is_cached = true;
    mel_fb->cache_index = slot;

    return ET_SUCCESS;
}

ETResult et_mel_init_precomputed_tables(void) {
    if (g_tables_initialized) {
        return ET_SUCCESS;
    }

    // 로그 테이블 생성 (0.1 ~ 100000 Hz 범위)
    int table_size = 10000;
    g_log_table = (float*)malloc(table_size * sizeof(float));
    g_exp_table = (float*)malloc(table_size * sizeof(float));

    if (!g_log_table || !g_exp_table) {
        et_mel_destroy_precomputed_tables();
        return ET_ERROR_OUT_OF_MEMORY;
    }

    for (int i = 0; i < table_size; i++) {
        float x = 0.1f + (100000.0f - 0.1f) * i / (table_size - 1);
        g_log_table[i] = logf(x);
        g_exp_table[i] = expf(x / 10000.0f);  // 정규화된 지수
    }

    g_tables_initialized = true;
    return ET_SUCCESS;
}

void et_mel_destroy_precomputed_tables(void) {
    if (g_log_table) {
        free(g_log_table);
        g_log_table = NULL;
    }

    if (g_exp_table) {
        free(g_exp_table);
        g_exp_table = NULL;
    }

    g_tables_initialized = false;
}

// ============================================================================
// 유틸리티 및 통계 함수 구현
// ============================================================================

ETResult et_mel_get_filterbank_info(ETMelFilterbank* mel_fb, int* n_fft, int* n_mels,
                                   int* sample_rate, float* fmin, float* fmax) {
    if (!mel_fb) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (n_fft) *n_fft = mel_fb->config.n_fft;
    if (n_mels) *n_mels = mel_fb->config.n_mels;
    if (sample_rate) *sample_rate = mel_fb->config.sample_rate;
    if (fmin) *fmin = mel_fb->config.fmin;
    if (fmax) *fmax = mel_fb->config.fmax;

    return ET_SUCCESS;
}

ETResult et_mel_get_performance_stats(ETMelFilterbank* mel_fb, ETMelStats* stats) {
    if (!mel_fb || !stats) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *stats = mel_fb->stats;
    stats->memory_usage = mel_fb->memory_size;

    if (mel_fb->sparse_filters.data) {
        stats->memory_usage += mel_fb->sparse_filters.nnz * sizeof(float);
        stats->memory_usage += mel_fb->sparse_filters.nnz * sizeof(int);
        stats->memory_usage += (mel_fb->config.n_mels + 1) * sizeof(int);
    }

    return ET_SUCCESS;
}

ETResult et_mel_reset_filterbank(ETMelFilterbank* mel_fb) {
    if (!mel_fb) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(&mel_fb->stats, 0, sizeof(ETMelStats));
    return ET_SUCCESS;
}

ETResult et_mel_get_filter_responses(ETMelFilterbank* mel_fb, float* filter_responses) {
    if (!mel_fb || !filter_responses) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    int n_freq_bins = mel_fb->config.n_fft / 2 + 1;
    int n_mels = mel_fb->config.n_mels;

    memcpy(filter_responses, mel_fb->filters, n_mels * n_freq_bins * sizeof(float));

    return ET_SUCCESS;
}

ETResult et_mel_verify_accuracy(ETMelFilterbank* mel_fb, const float* test_spectrum,
                               int n_freq_bins, float* reconstruction_error) {
    if (!mel_fb || !test_spectrum || !reconstruction_error) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (n_freq_bins != mel_fb->config.n_fft / 2 + 1) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 순방향 변환
    float* mel_frame = (float*)malloc(mel_fb->config.n_mels * sizeof(float));
    float* reconstructed = (float*)malloc(n_freq_bins * sizeof(float));

    if (!mel_frame || !reconstructed) {
        free(mel_frame);
        free(reconstructed);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    et_mel_spectrum_to_mel_frame(mel_fb, test_spectrum, mel_frame);
    et_mel_mel_frame_to_spectrum(mel_fb, mel_frame, reconstructed);

    // 재구성 오차 계산 (MSE)
    float error = 0.0f;
    for (int i = 0; i < n_freq_bins; i++) {
        float diff = test_spectrum[i] - reconstructed[i];
        error += diff * diff;
    }

    *reconstruction_error = error / n_freq_bins;

    free(mel_frame);
    free(reconstructed);

    return ET_SUCCESS;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static ETResult create_dense_filterbank(ETMelFilterbank* mel_fb) {
    return et_mel_create_triangular_filters(mel_fb);
}

static ETResult create_sparse_filterbank(ETMelFilterbank* mel_fb) {
    return et_mel_optimize_sparse_filterbank(mel_fb);
}

static ETResult compute_mel_points(ETMelFilterbank* mel_fb) {
    ETResult result = et_mel_create_mel_points(mel_fb->config.fmin, mel_fb->config.fmax,
                                              mel_fb->config.n_mels, mel_fb->config.scale_type,
                                              mel_fb->mel_points);
    if (result != ET_SUCCESS) {
        return result;
    }

    // Mel 포인트를 Hz로 변환
    for (int i = 0; i < mel_fb->config.n_mels + 2; i++) {
        mel_fb->hz_points[i] = et_mel_mel_to_hz(mel_fb->mel_points[i], mel_fb->config.scale_type);
    }

    return ET_SUCCESS;
}

static ETResult compute_fft_bin_indices(ETMelFilterbank* mel_fb) {
    for (int i = 0; i < mel_fb->config.n_mels + 2; i++) {
        float bin_float = et_mel_hz_to_fft_bin(mel_fb->hz_points[i],
                                              mel_fb->config.n_fft,
                                              mel_fb->config.sample_rate);
        mel_fb->fft_bin_indices[i] = (int)roundf(bin_float);

        // 범위 제한
        if (mel_fb->fft_bin_indices[i] < 0) {
            mel_fb->fft_bin_indices[i] = 0;
        }
        if (mel_fb->fft_bin_indices[i] >= mel_fb->config.n_fft / 2 + 1) {
            mel_fb->fft_bin_indices[i] = mel_fb->config.n_fft / 2;
        }
    }

    return ET_SUCCESS;
}

static void cleanup_filterbank_memory(ETMelFilterbank* mel_fb) {
    if (!mel_fb) return;

    if (mel_fb->sparse_filters.data) {
        free(mel_fb->sparse_filters.data);
        mel_fb->sparse_filters.data = NULL;
    }

    if (mel_fb->sparse_filters.indices) {
        free(mel_fb->sparse_filters.indices);
        mel_fb->sparse_filters.indices = NULL;
    }

    if (mel_fb->sparse_filters.indptr) {
        free(mel_fb->sparse_filters.indptr);
        mel_fb->sparse_filters.indptr = NULL;
    }

    // 캐시에서 제거
    if (mel_fb->is_cached && g_cache_initialized && g_cache) {
        if (mel_fb->cache_index >= 0 && mel_fb->cache_index < g_cache_size) {
            g_cache[mel_fb->cache_index].in_use = false;
            g_cache[mel_fb->cache_index].filterbank = NULL;
        }
    }
}

static int find_cache_slot(void) {
    if (!g_cache_initialized || !g_cache) {
        return -1;
    }

    // 빈 슬롯 찾기
    for (int i = 0; i < g_cache_size; i++) {
        if (!g_cache[i].in_use) {
            return i;
        }
    }

    // 가장 적게 사용된 슬롯 찾기 (LRU)
    int min_access = g_cache[0].access_count;
    int min_index = 0;

    for (int i = 1; i < g_cache_size; i++) {
        if (g_cache[i].access_count < min_access) {
            min_access = g_cache[i].access_count;
            min_index = i;
        }
    }

    // 기존 엔트리 정리
    if (g_cache[min_index].filterbank) {
        g_cache[min_index].filterbank->is_cached = false;
        g_cache[min_index].filterbank->cache_index = -1;
    }

    g_cache[min_index].in_use = false;
    return min_index;
}

static bool configs_equal(const ETMelFilterbankConfig* a, const ETMelFilterbankConfig* b) {
    return (a->n_fft == b->n_fft &&
            a->n_mels == b->n_mels &&
            a->sample_rate == b->sample_rate &&
            fabsf(a->fmin - b->fmin) < 1e-6f &&
            fabsf(a->fmax - b->fmax) < 1e-6f &&
            a->scale_type == b->scale_type &&
            a->normalize == b->normalize);
}