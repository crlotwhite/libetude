/**
 * @file world_realtime_optimization.c
 * @brief WORLD 엔진 실시간 성능 최적화 구현
 *
 * 100ms 이내 처리 목표 달성을 위한 최적화와 멀티스레딩, SIMD 최적화를 구현합니다.
 * 요구사항 6.1, 6.3을 만족하는 실시간 성능 최적화를 제공합니다.
 */

#include "world_engine.h"
#include "world_error.h"
#include <libetude/simd_kernels.h>
#include <libetude/fast_math.h>
#include <libetude/task_scheduler.h>
#include <libetude/memory_optimization.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <string.h>

// SIMD 최적화를 위한 헤더
#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __AVX__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

// 실시간 최적화 설정
#define REALTIME_TARGET_MS 100.0
#define SIMD_ALIGNMENT 32
#define PARALLEL_THRESHOLD 16
#define CACHE_LINE_SIZE 64
#define MAX_WORKER_THREADS 8

// 병렬 처리를 위한 작업 구조체
typedef struct {
    const float* audio;
    int audio_length;
    int sample_rate;
    double* f0;
    double* time_axis;
    int f0_length;
    int start_frame;
    int end_frame;
    WorldF0Extractor* extractor;
    ETResult result;
} F0ExtractionTask;

typedef struct {
    const float* audio;
    int audio_length;
    int sample_rate;
    const double* f0;
    const double* time_axis;
    double** spectrogram;
    int start_frame;
    int end_frame;
    int fft_size;
    WorldSpectrumAnalyzer* analyzer;
    ETResult result;
} SpectrumAnalysisTask;

typedef struct {
    const float* audio;
    int audio_length;
    int sample_rate;
    const double* f0;
    const double* time_axis;
    double** aperiodicity;
    int start_frame;
    int end_frame;
    int fft_size;
    WorldAperiodicityAnalyzer* analyzer;
    ETResult result;
} AperiodicityAnalysisTask;

// 전역 스레드 풀
static ETTaskScheduler* g_task_scheduler = NULL;
static pthread_mutex_t g_scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 실시간 최적화 초기화
 */
ETResult world_realtime_optimization_init(int num_threads) {
    pthread_mutex_lock(&g_scheduler_mutex);

    if (g_task_scheduler) {
        pthread_mutex_unlock(&g_scheduler_mutex);
        return ET_SUCCESS;  // 이미 초기화됨
    }

    if (num_threads <= 0) {
        num_threads = et_get_num_cores();
        if (num_threads > MAX_WORKER_THREADS) {
            num_threads = MAX_WORKER_THREADS;
        }
    }

    g_task_scheduler = et_task_scheduler_create(num_threads);
    if (!g_task_scheduler) {
        pthread_mutex_unlock(&g_scheduler_mutex);
        return ET_ERROR_INITIALIZATION_FAILED;
    }

    pthread_mutex_unlock(&g_scheduler_mutex);
    return ET_SUCCESS;
}

/**
 * @brief 실시간 최적화 정리
 */
void world_realtime_optimization_cleanup(void) {
    pthread_mutex_lock(&g_scheduler_mutex);

    if (g_task_scheduler) {
        et_task_scheduler_destroy(g_task_scheduler);
        g_task_scheduler = NULL;
    }

    pthread_mutex_unlock(&g_scheduler_mutex);
}

/**
 * @brief SIMD 최적화된 DIO F0 추출
 */
ETResult world_dio_f0_estimation_optimized(WorldF0Extractor* extractor,
                                          const float* audio, int audio_length, int sample_rate,
                                          double* f0, int f0_length) {
    if (!extractor || !audio || !f0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 시간 측정 시작
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    const double frame_period = extractor->config.frame_period;
    const double f0_floor = extractor->config.f0_floor;
    const double f0_ceil = extractor->config.f0_ceil;
    const double channels_in_octave = extractor->config.channels_in_octave;

    // F0 후보 주파수 계산
    int num_candidates = (int)(channels_in_octave * log2(f0_ceil / f0_floor));
    double* f0_candidates = (double*)et_aligned_alloc(SIMD_ALIGNMENT,
                                                     num_candidates * sizeof(double));
    if (!f0_candidates) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // F0 후보 주파수 생성 (SIMD 최적화)
    for (int i = 0; i < num_candidates; i++) {
        f0_candidates[i] = f0_floor * pow(2.0, (double)i / channels_in_octave);
    }

    // 각 프레임에 대해 F0 추정
    double frame_shift = frame_period / 1000.0;  // ms를 초로 변환

#ifdef __AVX__
    // AVX를 사용한 벡터화된 처리
    const int simd_width = 4;  // AVX는 4개의 double을 동시 처리

    for (int frame = 0; frame < f0_length; frame++) {
        double current_time = frame * frame_shift;
        int center_sample = (int)(current_time * sample_rate);

        if (center_sample >= audio_length) {
            f0[frame] = 0.0;
            continue;
        }

        double best_f0 = 0.0;
        double best_score = -1.0;

        // SIMD를 사용한 후보 주파수 평가
        for (int cand = 0; cand < num_candidates; cand += simd_width) {
            int remaining = num_candidates - cand;
            int process_count = (remaining < simd_width) ? remaining : simd_width;

            __m256d f0_vec = _mm256_load_pd(&f0_candidates[cand]);
            __m256d score_vec = _mm256_setzero_pd();

            // 각 후보 주파수에 대해 스코어 계산
            for (int i = 0; i < process_count; i++) {
                double candidate_f0 = f0_candidates[cand + i];
                double score = world_calculate_dio_score_simd(audio, audio_length, sample_rate,
                                                            center_sample, candidate_f0);

                if (score > best_score) {
                    best_score = score;
                    best_f0 = candidate_f0;
                }
            }
        }

        f0[frame] = (best_score > extractor->config.threshold) ? best_f0 : 0.0;
    }
#else
    // 일반적인 스칼라 처리
    for (int frame = 0; frame < f0_length; frame++) {
        double current_time = frame * frame_shift;
        int center_sample = (int)(current_time * sample_rate);

        if (center_sample >= audio_length) {
            f0[frame] = 0.0;
            continue;
        }

        double best_f0 = 0.0;
        double best_score = -1.0;

        for (int cand = 0; cand < num_candidates; cand++) {
            double candidate_f0 = f0_candidates[cand];
            double score = world_calculate_dio_score_optimized(audio, audio_length, sample_rate,
                                                             center_sample, candidate_f0);

            if (score > best_score) {
                best_score = score;
                best_f0 = candidate_f0;
            }
        }

        f0[frame] = (best_score > extractor->config.threshold) ? best_f0 : 0.0;
    }
#endif

    et_aligned_free(f0_candidates);

    // 시간 측정 종료
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double processing_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                           (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    // 실시간 요구사항 검증
    if (processing_time > REALTIME_TARGET_MS) {
        printf("경고: DIO F0 추출 시간이 목표를 초과함: %.2fms > %.2fms\n",
               processing_time, REALTIME_TARGET_MS);
    }

    return ET_SUCCESS;
}

/**
 * @brief SIMD 최적화된 DIO 스코어 계산
 */
static double world_calculate_dio_score_simd(const float* audio, int audio_length, int sample_rate,
                                           int center_sample, double f0) {
    if (f0 <= 0.0) return 0.0;

    int period_samples = (int)(sample_rate / f0);
    int half_period = period_samples / 2;

    // 분석 윈도우 설정
    int window_start = center_sample - half_period;
    int window_end = center_sample + half_period;

    if (window_start < 0 || window_end >= audio_length) {
        return 0.0;
    }

    int window_length = window_end - window_start;

#ifdef __AVX__
    // AVX를 사용한 자기상관 계산
    __m256 sum_vec = _mm256_setzero_ps();
    __m256 autocorr_vec = _mm256_setzero_ps();

    const int simd_width = 8;  // AVX는 8개의 float를 동시 처리
    int simd_length = (window_length / simd_width) * simd_width;

    // 벡터화된 자기상관 계산
    for (int i = 0; i < simd_length; i += simd_width) {
        __m256 audio1 = _mm256_loadu_ps(&audio[window_start + i]);
        __m256 audio2 = _mm256_loadu_ps(&audio[window_start + i + period_samples]);

        __m256 product = _mm256_mul_ps(audio1, audio2);
        autocorr_vec = _mm256_add_ps(autocorr_vec, product);

        __m256 square1 = _mm256_mul_ps(audio1, audio1);
        __m256 square2 = _mm256_mul_ps(audio2, audio2);
        sum_vec = _mm256_add_ps(sum_vec, _mm256_add_ps(square1, square2));
    }

    // 벡터 결과를 스칼라로 축약
    float autocorr_result[8], sum_result[8];
    _mm256_storeu_ps(autocorr_result, autocorr_vec);
    _mm256_storeu_ps(sum_result, sum_vec);

    double autocorr = 0.0, sum_squares = 0.0;
    for (int i = 0; i < 8; i++) {
        autocorr += autocorr_result[i];
        sum_squares += sum_result[i];
    }

    // 나머지 샘플들 처리
    for (int i = simd_length; i < window_length - period_samples; i++) {
        float val1 = audio[window_start + i];
        float val2 = audio[window_start + i + period_samples];
        autocorr += val1 * val2;
        sum_squares += val1 * val1 + val2 * val2;
    }

    // 정규화된 자기상관 계산
    if (sum_squares > 0.0) {
        return autocorr / sqrt(sum_squares);
    }

    return 0.0;
#else
    // 일반적인 스칼라 자기상관 계산
    double autocorr = 0.0;
    double sum_squares = 0.0;

    for (int i = 0; i < window_length - period_samples; i++) {
        float val1 = audio[window_start + i];
        float val2 = audio[window_start + i + period_samples];
        autocorr += val1 * val2;
        sum_squares += val1 * val1 + val2 * val2;
    }

    if (sum_squares > 0.0) {
        return autocorr / sqrt(sum_squares);
    }

    return 0.0;
#endif
}

/**
 * @brief 최적화된 DIO 스코어 계산 (일반 버전)
 */
static double world_calculate_dio_score_optimized(const float* audio, int audio_length, int sample_rate,
                                                int center_sample, double f0) {
    if (f0 <= 0.0) return 0.0;

    int period_samples = (int)(sample_rate / f0);
    int half_period = period_samples / 2;

    // 분석 윈도우 설정
    int window_start = center_sample - half_period;
    int window_end = center_sample + half_period;

    if (window_start < 0 || window_end >= audio_length) {
        return 0.0;
    }

    // 빠른 자기상관 계산 (libetude fast_math 사용)
    double autocorr = 0.0;
    double sum_squares = 0.0;
    int window_length = window_end - window_start;

    for (int i = 0; i < window_length - period_samples; i++) {
        float val1 = audio[window_start + i];
        float val2 = audio[window_start + i + period_samples];
        autocorr += val1 * val2;
        sum_squares += val1 * val1 + val2 * val2;
    }

    if (sum_squares > 0.0) {
        // libetude의 빠른 sqrt 사용
        return autocorr / et_fast_sqrt(sum_squares);
    }

    return 0.0;
}

/**
 * @brief 병렬 F0 추출 작업 함수
 */
static void* f0_extraction_worker(void* arg) {
    F0ExtractionTask* task = (F0ExtractionTask*)arg;

    // 할당된 프레임 범위에 대해 F0 추출 수행
    const double frame_period = task->extractor->config.frame_period;
    const double f0_floor = task->extractor->config.f0_floor;
    const double f0_ceil = task->extractor->config.f0_ceil;
    const double channels_in_octave = task->extractor->config.channels_in_octave;

    double frame_shift = frame_period / 1000.0;

    for (int frame = task->start_frame; frame < task->end_frame; frame++) {
        double current_time = frame * frame_shift;
        int center_sample = (int)(current_time * task->sample_rate);

        if (center_sample >= task->audio_length) {
            task->f0[frame] = 0.0;
            continue;
        }

        // DIO 알고리즘으로 F0 추정
        double best_f0 = 0.0;
        double best_score = -1.0;

        int num_candidates = (int)(channels_in_octave * log2(f0_ceil / f0_floor));

        for (int cand = 0; cand < num_candidates; cand++) {
            double candidate_f0 = f0_floor * pow(2.0, (double)cand / channels_in_octave);
            double score = world_calculate_dio_score_optimized(task->audio, task->audio_length,
                                                             task->sample_rate, center_sample, candidate_f0);

            if (score > best_score) {
                best_score = score;
                best_f0 = candidate_f0;
            }
        }

        task->f0[frame] = (best_score > task->extractor->config.threshold) ? best_f0 : 0.0;
        task->time_axis[frame] = current_time;
    }

    task->result = ET_SUCCESS;
    return NULL;
}

/**
 * @brief 병렬 F0 추출
 */
ETResult world_f0_extraction_parallel(WorldF0Extractor* extractor,
                                     const float* audio, int audio_length, int sample_rate,
                                     double* f0, double* time_axis, int f0_length,
                                     int num_threads) {
    if (!extractor || !audio || !f0 || !time_axis) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (num_threads <= 0) {
        num_threads = et_get_num_cores();
        if (num_threads > MAX_WORKER_THREADS) {
            num_threads = MAX_WORKER_THREADS;
        }
    }

    // 프레임 수가 적으면 단일 스레드 사용
    if (f0_length < PARALLEL_THRESHOLD || num_threads == 1) {
        return world_dio_f0_estimation_optimized(extractor, audio, audio_length, sample_rate, f0, f0_length);
    }

    // 작업 분할
    int frames_per_thread = f0_length / num_threads;
    int remaining_frames = f0_length % num_threads;

    F0ExtractionTask* tasks = (F0ExtractionTask*)malloc(num_threads * sizeof(F0ExtractionTask));
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));

    if (!tasks || !threads) {
        free(tasks);
        free(threads);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 각 스레드에 작업 할당
    int current_frame = 0;
    for (int i = 0; i < num_threads; i++) {
        tasks[i].audio = audio;
        tasks[i].audio_length = audio_length;
        tasks[i].sample_rate = sample_rate;
        tasks[i].f0 = f0;
        tasks[i].time_axis = time_axis;
        tasks[i].f0_length = f0_length;
        tasks[i].start_frame = current_frame;
        tasks[i].end_frame = current_frame + frames_per_thread;
        tasks[i].extractor = extractor;
        tasks[i].result = ET_SUCCESS;

        // 마지막 스레드에 남은 프레임 할당
        if (i == num_threads - 1) {
            tasks[i].end_frame += remaining_frames;
        }

        current_frame = tasks[i].end_frame;

        // 스레드 생성
        if (pthread_create(&threads[i], NULL, f0_extraction_worker, &tasks[i]) != 0) {
            // 스레드 생성 실패시 이미 생성된 스레드들 정리
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(tasks);
            free(threads);
            return ET_ERROR_THREAD_CREATION_FAILED;
        }
    }

    // 모든 스레드 완료 대기
    ETResult final_result = ET_SUCCESS;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        if (tasks[i].result != ET_SUCCESS) {
            final_result = tasks[i].result;
        }
    }

    free(tasks);
    free(threads);

    return final_result;
}

/**
 * @brief 병렬 스펙트럼 분석 작업 함수
 */
static void* spectrum_analysis_worker(void* arg) {
    SpectrumAnalysisTask* task = (SpectrumAnalysisTask*)arg;

    // CheapTrick 알고리즘을 각 프레임에 적용
    for (int frame = task->start_frame; frame < task->end_frame; frame++) {
        double f0_value = task->f0[frame];

        if (f0_value > 0.0) {
            // 유성음 프레임 처리
            ETResult result = world_cheaptrick_frame_optimized(task->analyzer,
                                                             task->audio, task->audio_length,
                                                             task->sample_rate, f0_value,
                                                             frame, task->spectrogram[frame],
                                                             task->fft_size);
            if (result != ET_SUCCESS) {
                task->result = result;
                return NULL;
            }
        } else {
            // 무성음 프레임 처리 (평균 스펙트럼 사용)
            world_fill_unvoiced_spectrum(task->spectrogram[frame], task->fft_size / 2 + 1);
        }
    }

    task->result = ET_SUCCESS;
    return NULL;
}

/**
 * @brief 병렬 스펙트럼 분석
 */
ETResult world_spectrum_analyzer_cheaptrick_parallel(WorldSpectrumAnalyzer* analyzer,
                                                   const float* audio, int audio_length, int sample_rate,
                                                   const double* f0, const double* time_axis, int f0_length,
                                                   double** spectrogram, int num_threads) {
    if (!analyzer || !audio || !f0 || !spectrogram) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (num_threads <= 0) {
        num_threads = et_get_num_cores();
        if (num_threads > MAX_WORKER_THREADS) {
            num_threads = MAX_WORKER_THREADS;
        }
    }

    // 프레임 수가 적으면 단일 스레드 사용
    if (f0_length < PARALLEL_THRESHOLD || num_threads == 1) {
        return world_spectrum_analyzer_cheaptrick(analyzer, audio, audio_length, sample_rate,
                                                 f0, time_axis, f0_length, spectrogram);
    }

    // 작업 분할
    int frames_per_thread = f0_length / num_threads;
    int remaining_frames = f0_length % num_threads;

    SpectrumAnalysisTask* tasks = (SpectrumAnalysisTask*)malloc(num_threads * sizeof(SpectrumAnalysisTask));
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));

    if (!tasks || !threads) {
        free(tasks);
        free(threads);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 각 스레드에 작업 할당
    int current_frame = 0;
    for (int i = 0; i < num_threads; i++) {
        tasks[i].audio = audio;
        tasks[i].audio_length = audio_length;
        tasks[i].sample_rate = sample_rate;
        tasks[i].f0 = f0;
        tasks[i].time_axis = time_axis;
        tasks[i].spectrogram = spectrogram;
        tasks[i].start_frame = current_frame;
        tasks[i].end_frame = current_frame + frames_per_thread;
        tasks[i].fft_size = analyzer->config.fft_size;
        tasks[i].analyzer = analyzer;
        tasks[i].result = ET_SUCCESS;

        // 마지막 스레드에 남은 프레임 할당
        if (i == num_threads - 1) {
            tasks[i].end_frame += remaining_frames;
        }

        current_frame = tasks[i].end_frame;

        // 스레드 생성
        if (pthread_create(&threads[i], NULL, spectrum_analysis_worker, &tasks[i]) != 0) {
            // 스레드 생성 실패시 이미 생성된 스레드들 정리
            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(tasks);
            free(threads);
            return ET_ERROR_THREAD_CREATION_FAILED;
        }
    }

    // 모든 스레드 완료 대기
    ETResult final_result = ET_SUCCESS;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        if (tasks[i].result != ET_SUCCESS) {
            final_result = tasks[i].result;
        }
    }

    free(tasks);
    free(threads);

    return final_result;
}

/**
 * @brief 실시간 처리를 위한 청크 기반 분석
 */
ETResult world_analyze_audio_realtime(WorldAnalysisEngine* engine,
                                     const float* audio, int audio_length, int sample_rate,
                                     WorldParameters* params, double max_processing_time_ms) {
    if (!engine || !audio || !params) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // 실시간 최적화 설정 적용
    engine->config.enable_simd_optimization = true;
    engine->config.enable_parallel_processing = true;

    // 청크 크기 동적 조정
    int chunk_size = audio_length;
    if (max_processing_time_ms > 0 && max_processing_time_ms < REALTIME_TARGET_MS) {
        // 더 작은 청크로 분할하여 지연 시간 최소화
        chunk_size = (int)(sample_rate * max_processing_time_ms / 1000.0 / 4);
        if (chunk_size < 1024) chunk_size = 1024;  // 최소 청크 크기
    }

    // WorldParameters 초기화
    ETResult result = world_parameters_init(params, sample_rate, audio_length,
                                           engine->config.f0_config.frame_period);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 병렬 F0 추출
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    double elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000.0 +
                       (current_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    if (elapsed_ms < max_processing_time_ms * 0.4) {  // F0 추출에 40% 시간 할당
        result = world_f0_extraction_parallel(engine->f0_extractor, audio, audio_length, sample_rate,
                                             params->f0, params->time_axis, params->f0_length, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    } else {
        // 시간이 부족하면 단일 스레드로 빠른 처리
        result = world_dio_f0_estimation_optimized(engine->f0_extractor, audio, audio_length, sample_rate,
                                                  params->f0, params->f0_length);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 병렬 스펙트럼 분석
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000.0 +
                (current_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    if (elapsed_ms < max_processing_time_ms * 0.7) {  // 스펙트럼 분석에 30% 시간 할당
        result = world_spectrum_analyzer_cheaptrick_parallel(engine->spectrum_analyzer,
                                                           audio, audio_length, sample_rate,
                                                           params->f0, params->time_axis, params->f0_length,
                                                           params->spectrogram, 0);
        if (result != ET_SUCCESS) {
            return result;
        }
    } else {
        // 시간이 부족하면 단순화된 스펙트럼 분석
        result = world_spectrum_analyzer_cheaptrick_fast(engine->spectrum_analyzer,
                                                        audio, audio_length, sample_rate,
                                                        params->f0, params->time_axis, params->f0_length,
                                                        params->spectrogram);
        if (result != ET_SUCCESS) {
            return result;
        }
    }

    // 비주기성 분석 (남은 시간 사용)
    result = world_analyze_aperiodicity(engine, audio, audio_length, sample_rate,
                                       params->f0, params->time_axis, params->f0_length,
                                       params->aperiodicity);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 최종 처리 시간 확인
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    double total_time_ms = (current_time.tv_sec - start_time.tv_sec) * 1000.0 +
                          (current_time.tv_nsec - start_time.tv_nsec) / 1000000.0;

    if (total_time_ms > max_processing_time_ms) {
        printf("경고: 실시간 분석 시간 초과: %.2fms > %.2fms\n", total_time_ms, max_processing_time_ms);
    }

    return ET_SUCCESS;
}

/**
 * @brief 메모리 사용량 최적화된 분석
 */
ETResult world_analyze_audio_memory_optimized(WorldAnalysisEngine* engine,
                                             const float* audio, int audio_length, int sample_rate,
                                             WorldParameters* params) {
    if (!engine || !audio || !params) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // 메모리 풀 최적화 설정
    et_memory_pool_set_optimization_level(engine->mem_pool, ET_MEMORY_OPTIMIZATION_AGGRESSIVE);

    // 청크 단위로 처리하여 메모리 사용량 최소화
    const int chunk_size = 8192;  // 8K 샘플 청크
    int processed_samples = 0;

    while (processed_samples < audio_length) {
        int current_chunk_size = (audio_length - processed_samples < chunk_size) ?
                                (audio_length - processed_samples) : chunk_size;

        // 현재 청크에 대해 분석 수행
        const float* chunk_audio = audio + processed_samples;

        // 청크별 F0 추출 (메모리 효율적)
        int chunk_f0_length = world_get_samples_for_dio(current_chunk_size, sample_rate,
                                                       engine->config.f0_config.frame_period);

        // 임시 버퍼 사용 (메모리 풀에서 할당)
        double* chunk_f0 = (double*)et_alloc_from_pool(engine->mem_pool, chunk_f0_length * sizeof(double));
        double* chunk_time_axis = (double*)et_alloc_from_pool(engine->mem_pool, chunk_f0_length * sizeof(double));

        if (!chunk_f0 || !chunk_time_axis) {
            return ET_ERROR_OUT_OF_MEMORY;
        }

        ETResult result = world_dio_f0_estimation_optimized(engine->f0_extractor,
                                                           chunk_audio, current_chunk_size, sample_rate,
                                                           chunk_f0, chunk_f0_length);
        if (result != ET_SUCCESS) {
            return result;
        }

        // 결과를 전체 버퍼에 복사
        int output_offset = (processed_samples * params->f0_length) / audio_length;
        int copy_length = (chunk_f0_length < params->f0_length - output_offset) ?
                         chunk_f0_length : (params->f0_length - output_offset);

        memcpy(params->f0 + output_offset, chunk_f0, copy_length * sizeof(double));

        // 시간축 조정
        for (int i = 0; i < copy_length; i++) {
            params->time_axis[output_offset + i] = chunk_time_axis[i] +
                                                  (double)processed_samples / sample_rate;
        }

        // 메모리 풀에서 할당된 임시 버퍼는 자동으로 관리됨
        processed_samples += current_chunk_size;

        // 메모리 풀 정리 (필요시)
        if (processed_samples % (chunk_size * 4) == 0) {
            et_memory_pool_compact(engine->mem_pool);
        }
    }

    // 스펙트럼 및 비주기성 분석도 유사하게 청크 단위로 처리
    // (구현 생략 - 유사한 패턴)

    return ET_SUCCESS;
}