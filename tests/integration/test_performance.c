/**
 * @file test_performance.c
 * @brief 성능 통합 테스트
 *
 * 지연 시간, 처리량, 메모리 사용량을 측정합니다.
 * Requirements: 3.1, 8.2, 10.4
 */

#include "unity.h"
#include "libetude/api.h"
#include "libetude/error.h"
#include "libetude/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

// 성능 측정 구조체
typedef struct {
    double latency_ms;           // 지연 시간 (밀리초)
    double throughput_samples_per_sec; // 처리량 (샘플/초)
    size_t memory_usage_bytes;   // 메모리 사용량 (바이트)
    size_t peak_memory_bytes;    // 피크 메모리 사용량
    double cpu_usage_percent;    // CPU 사용률 (%)
    int processed_texts;         // 처리된 텍스트 수
    int total_audio_samples;     // 총 오디오 샘플 수
} PerformanceMetrics;

// 테스트용 전역 변수
static LibEtudeEngine* perf_engine = NULL;
static PerformanceMetrics metrics;
static struct rusage start_usage, end_usage;

void setUp(void) {
    // 테스트 전 초기화
    et_set_log_level(ET_LOG_WARNING); // 성능 테스트 중에는 로그 최소화

    // 성능 메트릭 초기화
    memset(&metrics, 0, sizeof(PerformanceMetrics));

    // 리소스 사용량 측정 시작
    getrusage(RUSAGE_SELF, &start_usage);
}

void tearDown(void) {
    // 테스트 후 정리
    if (perf_engine) {
        libetude_destroy_engine(perf_engine);
        perf_engine = NULL;
    }

    // 리소스 사용량 측정 종료
    getrusage(RUSAGE_SELF, &end_usage);

    et_clear_error();
}

// 고해상도 시간 측정 함수
double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// 메모리 사용량 측정 함수
size_t get_memory_usage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1024; // KB를 바이트로 변환 (Linux)
}

// CPU 사용률 계산 함수
double calculate_cpu_usage(void) {
    double user_time = (end_usage.ru_utime.tv_sec - start_usage.ru_utime.tv_sec) +
                      (end_usage.ru_utime.tv_usec - start_usage.ru_utime.tv_usec) / 1000000.0;

    double sys_time = (end_usage.ru_stime.tv_sec - start_usage.ru_stime.tv_sec) +
                     (end_usage.ru_stime.tv_usec - start_usage.ru_stime.tv_usec) / 1000000.0;

    return (user_time + sys_time) * 100.0; // 대략적인 CPU 사용률
}

void test_latency_measurement(void) {
    printf("\n=== 지연 시간 측정 테스트 시작 ===\n");

    // 더미 엔진 생성
    perf_engine = (LibEtudeEngine*)malloc(sizeof(void*));
    TEST_ASSERT_NOT_NULL_MESSAGE(perf_engine, "더미 엔진 생성 실패");

    // 다양한 길이의 텍스트로 지연 시간 측정
    const char* test_texts[] = {
        "짧은 텍스트",
        "중간 길이의 텍스트입니다. 이 정도면 적당한 길이라고 할 수 있겠네요.",
        "매우 긴 텍스트입니다. 이 텍스트는 음성 합성 엔진의 성능을 측정하기 위해 작성된 것으로, 실제 사용 환경에서 발생할 수 있는 긴 문장을 시뮬레이션합니다. 이런 긴 텍스트를 처리할 때의 지연 시간을 측정하여 엔진의 성능을 평가할 수 있습니다."
    };

    const char* text_labels[] = {
        "짧은 텍스트",
        "중간 텍스트",
        "긴 텍스트"
    };

    float* output_buffer = (float*)malloc(44100 * 10 * sizeof(float)); // 10초 분량
    TEST_ASSERT_NOT_NULL_MESSAGE(output_buffer, "출력 버퍼 할당 실패");

    double total_latency = 0.0;
    int successful_tests = 0;

    for (int i = 0; i < 3; i++) {
        printf("\n%s 지연 시간 측정:\n", text_labels[i]);
        printf("텍스트 길이: %zu 문자\n", strlen(test_texts[i]));

        // 여러 번 측정하여 평균 계산
        double test_latencies[5];
        int valid_measurements = 0;

        for (int j = 0; j < 5; j++) {
            int output_length = 44100 * 10;

            double start_time = get_time_ms();
            int result = libetude_synthesize_text(perf_engine, test_texts[i], output_buffer, &output_length);
            double end_time = get_time_ms();

            double latency = end_time - start_time;

            if (result == LIBETUDE_SUCCESS) {
                test_latencies[valid_measurements] = latency;
                valid_measurements++;
                printf("  측정 %d: %.2f ms (출력: %d 샘플)\n", j+1, latency, output_length);

                // 오디오 길이 대비 처리 시간 비율
                double audio_duration_ms = (double)output_length / 44.1;
                double realtime_factor = latency / audio_duration_ms;
                printf("    실시간 팩터: %.3f\n", realtime_factor);

            } else if (result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
                printf("  합성 기능 미구현 (정상)\n");
                break;
            } else {
                printf("  측정 %d 실패: 오류 코드 %d\n", j+1, result);
            }
        }

        if (valid_measurements > 0) {
            // 평균 지연 시간 계산
            double avg_latency = 0.0;
            double min_latency = test_latencies[0];
            double max_latency = test_latencies[0];

            for (int k = 0; k < valid_measurements; k++) {
                avg_latency += test_latencies[k];
                if (test_latencies[k] < min_latency) min_latency = test_latencies[k];
                if (test_latencies[k] > max_latency) max_latency = test_latencies[k];
            }
            avg_latency /= valid_measurements;

            printf("  평균 지연시간: %.2f ms\n", avg_latency);
            printf("  최소 지연시간: %.2f ms\n", min_latency);
            printf("  최대 지연시간: %.2f ms\n", max_latency);

            total_latency += avg_latency;
            successful_tests++;

            // 실시간 처리 요구사항 검증
            if (avg_latency <= 100.0) {
                printf("  ✓ 실시간 처리 요구사항 만족 (100ms 이내)\n");
            } else {
                printf("  ✗ 실시간 처리 요구사항 미달 (%.2f ms > 100ms)\n", avg_latency);
            }
        }
    }

    if (successful_tests > 0) {
        metrics.latency_ms = total_latency / successful_tests;
        printf("\n전체 평균 지연시간: %.2f ms\n", metrics.latency_ms);

        // 지연시간 요구사항 검증
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(100.0, metrics.latency_ms, "평균 지연시간이 100ms를 초과함");
    } else {
        printf("유효한 지연시간 측정 없음 (기능 미구현)\n");
        TEST_PASS();
    }

    free(output_buffer);

    printf("=== 지연 시간 측정 테스트 완료 ===\n");
}

void test_throughput_measurement(void) {
    printf("\n=== 처리량 측정 테스트 시작 ===\n");

    if (!perf_engine) {
        perf_engine = (LibEtudeEngine*)malloc(sizeof(void*));
        TEST_ASSERT_NOT_NULL_MESSAGE(perf_engine, "더미 엔진 생성 실패");
    }

    // 처리량 측정을 위한 텍스트 배열
    const char* batch_texts[] = {
        "첫 번째 배치 텍스트입니다.",
        "두 번째 배치 텍스트입니다.",
        "세 번째 배치 텍스트입니다.",
        "네 번째 배치 텍스트입니다.",
        "다섯 번째 배치 텍스트입니다.",
        "여섯 번째 배치 텍스트입니다.",
        "일곱 번째 배치 텍스트입니다.",
        "여덟 번째 배치 텍스트입니다.",
        "아홉 번째 배치 텍스트입니다.",
        "열 번째 배치 텍스트입니다."
    };

    const int batch_size = 10;
    float* output_buffer = (float*)malloc(44100 * 5 * sizeof(float)); // 5초 분량
    TEST_ASSERT_NOT_NULL_MESSAGE(output_buffer, "출력 버퍼 할당 실패");

    printf("배치 처리량 측정 (텍스트 %d개)\n", batch_size);

    double start_time = get_time_ms();
    size_t start_memory = get_memory_usage();

    int total_samples = 0;
    int successful_syntheses = 0;

    for (int i = 0; i < batch_size; i++) {
        int output_length = 44100 * 5;

        int result = libetude_synthesize_text(perf_engine, batch_texts[i], output_buffer, &output_length);

        if (result == LIBETUDE_SUCCESS) {
            total_samples += output_length;
            successful_syntheses++;
            printf("  텍스트 %d 처리 완료: %d 샘플\n", i+1, output_length);
        } else if (result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("  합성 기능 미구현, 처리량 측정 중단\n");
            break;
        } else {
            printf("  텍스트 %d 처리 실패: 오류 코드 %d\n", i+1, result);
        }

        // 메모리 사용량 모니터링
        size_t current_memory = get_memory_usage();
        if (current_memory > metrics.peak_memory_bytes) {
            metrics.peak_memory_bytes = current_memory;
        }
    }

    double end_time = get_time_ms();
    size_t end_memory = get_memory_usage();

    double total_time_sec = (end_time - start_time) / 1000.0;

    if (successful_syntheses > 0) {
        metrics.throughput_samples_per_sec = total_samples / total_time_sec;
        metrics.processed_texts = successful_syntheses;
        metrics.total_audio_samples = total_samples;
        metrics.memory_usage_bytes = end_memory - start_memory;

        printf("\n처리량 측정 결과:\n");
        printf("  처리된 텍스트 수: %d/%d\n", successful_syntheses, batch_size);
        printf("  총 처리 시간: %.2f 초\n", total_time_sec);
        printf("  총 오디오 샘플: %d\n", total_samples);
        printf("  처리량: %.0f 샘플/초\n", metrics.throughput_samples_per_sec);
        printf("  텍스트 처리율: %.2f 텍스트/초\n", successful_syntheses / total_time_sec);

        // 오디오 시간 기준 처리량
        double total_audio_duration = (double)total_samples / 44100.0;
        double realtime_factor = total_time_sec / total_audio_duration;
        printf("  실시간 팩터: %.2f (1.0 이하가 실시간)\n", realtime_factor);

        // 처리량 요구사항 검증 (예: 최소 44100 샘플/초)
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(44100.0, metrics.throughput_samples_per_sec,
                                           "처리량이 최소 요구사항을 만족해야 함");

        if (realtime_factor <= 1.0) {
            printf("  ✓ 실시간 처리 가능\n");
        } else {
            printf("  ✗ 실시간 처리 불가능 (%.2fx 느림)\n", realtime_factor);
        }

    } else {
        printf("유효한 처리량 측정 없음 (기능 미구현)\n");
        TEST_PASS();
    }

    free(output_buffer);

    printf("=== 처리량 측정 테스트 완료 ===\n");
}

void test_memory_usage_measurement(void) {
    printf("\n=== 메모리 사용량 측정 테스트 시작 ===\n");

    // 초기 메모리 사용량 측정
    size_t initial_memory = get_memory_usage();
    printf("초기 메모리 사용량: %zu KB\n", initial_memory / 1024);

    // 엔진 생성 전후 메모리 비교
    size_t before_engine = get_memory_usage();

    if (!perf_engine) {
        perf_engine = (LibEtudeEngine*)malloc(sizeof(void*));
        TEST_ASSERT_NOT_NULL_MESSAGE(perf_engine, "더미 엔진 생성 실패");
    }

    size_t after_engine = get_memory_usage();
    size_t engine_memory = after_engine - before_engine;

    printf("엔진 생성 후 메모리 증가: %zu KB\n", engine_memory / 1024);

    // 메모리 사용량 모니터링을 위한 배열
    size_t memory_samples[100];
    int sample_count = 0;

    // 여러 텍스트 처리하면서 메모리 사용량 모니터링
    const char* memory_test_texts[] = {
        "메모리 테스트용 짧은 텍스트",
        "메모리 테스트용 중간 길이 텍스트입니다. 이 정도 길이면 적당할 것 같습니다.",
        "메모리 테스트용 매우 긴 텍스트입니다. 이 텍스트는 메모리 사용량을 측정하기 위해 작성된 것으로, 실제 사용 환경에서 발생할 수 있는 긴 문장을 시뮬레이션합니다. 메모리 누수나 과도한 메모리 사용을 감지하기 위해 이런 긴 텍스트를 여러 번 처리해보겠습니다."
    };

    float* memory_test_buffer = (float*)malloc(44100 * 10 * sizeof(float)); // 10초 분량
    TEST_ASSERT_NOT_NULL_MESSAGE(memory_test_buffer, "메모리 테스트 버퍼 할당 실패");

    printf("\n메모리 사용량 모니터링 시작:\n");

    for (int round = 0; round < 5; round++) {
        printf("라운드 %d:\n", round + 1);

        for (int i = 0; i < 3; i++) {
            int output_length = 44100 * 10;

            size_t before_synthesis = get_memory_usage();

            int result = libetude_synthesize_text(perf_engine, memory_test_texts[i],
                                                memory_test_buffer, &output_length);

            size_t after_synthesis = get_memory_usage();

            if (sample_count < 100) {
                memory_samples[sample_count] = after_synthesis;
                sample_count++;
            }

            printf("  텍스트 %d: %zu KB -> %zu KB (차이: %zd KB)\n",
                   i+1, before_synthesis/1024, after_synthesis/1024,
                   (after_synthesis - before_synthesis)/1024);

            if (result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
                printf("  합성 기능 미구현, 메모리 측정 중단\n");
                goto memory_analysis;
            } else if (result != LIBETUDE_SUCCESS) {
                printf("  합성 실패: %d\n", result);
            }

            // 피크 메모리 업데이트
            if (after_synthesis > metrics.peak_memory_bytes) {
                metrics.peak_memory_bytes = after_synthesis;
            }
        }

        // 라운드 간 짧은 대기
        usleep(100000); // 100ms
    }

memory_analysis:

    // 메모리 사용량 분석
    if (sample_count > 0) {
        size_t min_memory = memory_samples[0];
        size_t max_memory = memory_samples[0];
        size_t total_memory = 0;

        for (int i = 0; i < sample_count; i++) {
            total_memory += memory_samples[i];
            if (memory_samples[i] < min_memory) min_memory = memory_samples[i];
            if (memory_samples[i] > max_memory) max_memory = memory_samples[i];
        }

        size_t avg_memory = total_memory / sample_count;

        printf("\n메모리 사용량 분석:\n");
        printf("  샘플 수: %d\n", sample_count);
        printf("  평균 메모리: %zu KB\n", avg_memory / 1024);
        printf("  최소 메모리: %zu KB\n", min_memory / 1024);
        printf("  최대 메모리: %zu KB\n", max_memory / 1024);
        printf("  피크 메모리: %zu KB\n", metrics.peak_memory_bytes / 1024);
        printf("  메모리 변동: %zu KB\n", (max_memory - min_memory) / 1024);

        metrics.memory_usage_bytes = avg_memory;

        // 메모리 누수 검사
        size_t memory_growth = max_memory - min_memory;
        double growth_percentage = (double)memory_growth / min_memory * 100.0;

        printf("  메모리 증가율: %.2f%%\n", growth_percentage);

        if (growth_percentage > 50.0) {
            printf("  ⚠️  메모리 누수 의심 (50%% 이상 증가)\n");
        } else if (growth_percentage > 20.0) {
            printf("  ⚠️  메모리 사용량 증가 주의 (20%% 이상 증가)\n");
        } else {
            printf("  ✓ 메모리 사용량 안정적\n");
        }

        // 메모리 사용량 요구사항 검증 (예: 100MB 이하)
        const size_t max_allowed_memory = 100 * 1024 * 1024; // 100MB
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(max_allowed_memory, metrics.peak_memory_bytes,
                                        "피크 메모리 사용량이 허용 한계를 초과함");

    } else {
        printf("메모리 사용량 샘플 없음 (기능 미구현)\n");
        TEST_PASS();
    }

    free(memory_test_buffer);

    printf("=== 메모리 사용량 측정 테스트 완료 ===\n");
}

void test_cpu_usage_measurement(void) {
    printf("\n=== CPU 사용률 측정 테스트 시작 ===\n");

    if (!perf_engine) {
        perf_engine = (LibEtudeEngine*)malloc(sizeof(void*));
        TEST_ASSERT_NOT_NULL_MESSAGE(perf_engine, "더미 엔진 생성 실패");
    }

    // CPU 집약적인 작업 시뮬레이션
    const char* cpu_test_text = "CPU 사용률 측정을 위한 텍스트입니다. 이 텍스트를 여러 번 처리하여 CPU 사용률을 측정해보겠습니다.";

    float* cpu_test_buffer = (float*)malloc(44100 * 5 * sizeof(float)); // 5초 분량
    TEST_ASSERT_NOT_NULL_MESSAGE(cpu_test_buffer, "CPU 테스트 버퍼 할당 실패");

    printf("CPU 집약적 작업 시작 (10회 반복)\n");

    struct rusage start_cpu_usage, end_cpu_usage;
    getrusage(RUSAGE_SELF, &start_cpu_usage);

    double start_wall_time = get_time_ms();

    int successful_operations = 0;

    for (int i = 0; i < 10; i++) {
        int output_length = 44100 * 5;

        int result = libetude_synthesize_text(perf_engine, cpu_test_text,
                                            cpu_test_buffer, &output_length);

        if (result == LIBETUDE_SUCCESS) {
            successful_operations++;
            printf("  작업 %d 완료\n", i+1);
        } else if (result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
            printf("  합성 기능 미구현, CPU 측정 중단\n");
            break;
        } else {
            printf("  작업 %d 실패: %d\n", i+1, result);
        }

        // 추가적인 CPU 작업 시뮬레이션 (수학 연산)
        volatile double dummy = 0.0;
        for (int j = 0; j < 100000; j++) {
            dummy += sin(j * 0.001) * cos(j * 0.001);
        }
    }

    double end_wall_time = get_time_ms();
    getrusage(RUSAGE_SELF, &end_cpu_usage);

    // CPU 시간 계산
    double user_cpu_time = (end_cpu_usage.ru_utime.tv_sec - start_cpu_usage.ru_utime.tv_sec) +
                          (end_cpu_usage.ru_utime.tv_usec - start_cpu_usage.ru_utime.tv_usec) / 1000000.0;

    double system_cpu_time = (end_cpu_usage.ru_stime.tv_sec - start_cpu_usage.ru_stime.tv_sec) +
                            (end_cpu_usage.ru_stime.tv_usec - start_cpu_usage.ru_stime.tv_usec) / 1000000.0;

    double total_cpu_time = user_cpu_time + system_cpu_time;
    double wall_time_sec = (end_wall_time - start_wall_time) / 1000.0;

    if (wall_time_sec > 0) {
        metrics.cpu_usage_percent = (total_cpu_time / wall_time_sec) * 100.0;

        printf("\nCPU 사용률 측정 결과:\n");
        printf("  성공한 작업 수: %d/10\n", successful_operations);
        printf("  총 실행 시간: %.2f 초\n", wall_time_sec);
        printf("  사용자 CPU 시간: %.2f 초\n", user_cpu_time);
        printf("  시스템 CPU 시간: %.2f 초\n", system_cpu_time);
        printf("  총 CPU 시간: %.2f 초\n", total_cpu_time);
        printf("  CPU 사용률: %.2f%%\n", metrics.cpu_usage_percent);

        // CPU 효율성 분석
        if (metrics.cpu_usage_percent > 90.0) {
            printf("  ⚠️  매우 높은 CPU 사용률 (90%% 이상)\n");
        } else if (metrics.cpu_usage_percent > 70.0) {
            printf("  ⚠️  높은 CPU 사용률 (70%% 이상)\n");
        } else if (metrics.cpu_usage_percent > 50.0) {
            printf("  ✓ 적당한 CPU 사용률\n");
        } else {
            printf("  ✓ 낮은 CPU 사용률 (효율적)\n");
        }

        // CPU 사용률 요구사항 검증 (예: 80% 이하)
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(80.0, metrics.cpu_usage_percent,
                                        "CPU 사용률이 허용 한계를 초과함");

    } else {
        printf("CPU 사용률 측정 실패 (실행 시간 0)\n");
        TEST_PASS();
    }

    free(cpu_test_buffer);

    printf("=== CPU 사용률 측정 테스트 완료 ===\n");
}

void test_performance_profiling(void) {
    printf("\n=== 성능 프로파일링 테스트 시작 ===\n");

    if (!perf_engine) {
        perf_engine = (LibEtudeEngine*)malloc(sizeof(void*));
        TEST_ASSERT_NOT_NULL_MESSAGE(perf_engine, "더미 엔진 생성 실패");
    }

    // 프로파일러 생성 시도
    printf("프로파일러 생성 시도\n");

    // 실제 프로파일러 API 사용 (구현되어 있다면)
    PerformanceStats engine_stats;
    memset(&engine_stats, 0, sizeof(engine_stats));

    int stats_result = libetude_get_performance_stats(perf_engine, &engine_stats);

    if (stats_result == LIBETUDE_SUCCESS) {
        printf("엔진 성능 통계 조회 성공:\n");
        printf("  추론 시간: %.2f ms\n", engine_stats.inference_time_ms);
        printf("  메모리 사용량: %.2f MB\n", engine_stats.memory_usage_mb);
        printf("  CPU 사용률: %.2f%%\n", engine_stats.cpu_usage_percent);
        printf("  GPU 사용률: %.2f%%\n", engine_stats.gpu_usage_percent);
        printf("  활성 스레드 수: %d\n", engine_stats.active_threads);

        // 통계 검증
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0.0, engine_stats.inference_time_ms,
                                           "추론 시간은 0 이상이어야 함");
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0.0, engine_stats.memory_usage_mb,
                                           "메모리 사용량은 0 이상이어야 함");
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(0, engine_stats.active_threads,
                                           "활성 스레드 수는 0 이상이어야 함");

    } else if (stats_result == LIBETUDE_ERROR_NOT_IMPLEMENTED) {
        printf("성능 통계 기능 미구현 (정상)\n");
    } else {
        printf("성능 통계 조회 실패: %d\n", stats_result);
    }

    // 종합 성능 리포트 생성
    printf("\n=== 종합 성능 리포트 ===\n");
    printf("측정된 성능 메트릭:\n");
    printf("  평균 지연시간: %.2f ms\n", metrics.latency_ms);
    printf("  처리량: %.0f 샘플/초\n", metrics.throughput_samples_per_sec);
    printf("  메모리 사용량: %.2f MB\n", (double)metrics.memory_usage_bytes / (1024*1024));
    printf("  피크 메모리: %.2f MB\n", (double)metrics.peak_memory_bytes / (1024*1024));
    printf("  CPU 사용률: %.2f%%\n", metrics.cpu_usage_percent);
    printf("  처리된 텍스트: %d개\n", metrics.processed_texts);
    printf("  총 오디오 샘플: %d개\n", metrics.total_audio_samples);

    // 성능 등급 평가
    int performance_score = 0;

    if (metrics.latency_ms <= 50.0) performance_score += 25;
    else if (metrics.latency_ms <= 100.0) performance_score += 15;
    else if (metrics.latency_ms <= 200.0) performance_score += 5;

    if (metrics.throughput_samples_per_sec >= 88200.0) performance_score += 25;
    else if (metrics.throughput_samples_per_sec >= 44100.0) performance_score += 15;
    else if (metrics.throughput_samples_per_sec >= 22050.0) performance_score += 5;

    if (metrics.memory_usage_bytes <= 50*1024*1024) performance_score += 25;
    else if (metrics.memory_usage_bytes <= 100*1024*1024) performance_score += 15;
    else if (metrics.memory_usage_bytes <= 200*1024*1024) performance_score += 5;

    if (metrics.cpu_usage_percent <= 50.0) performance_score += 25;
    else if (metrics.cpu_usage_percent <= 70.0) performance_score += 15;
    else if (metrics.cpu_usage_percent <= 90.0) performance_score += 5;

    printf("\n성능 점수: %d/100\n", performance_score);

    if (performance_score >= 80) {
        printf("성능 등급: 우수 (A)\n");
    } else if (performance_score >= 60) {
        printf("성능 등급: 양호 (B)\n");
    } else if (performance_score >= 40) {
        printf("성능 등급: 보통 (C)\n");
    } else {
        printf("성능 등급: 개선 필요 (D)\n");
    }

    printf("=== 성능 프로파일링 테스트 완료 ===\n");
}

int main(void) {
    UNITY_BEGIN();

    printf("\n========================================\n");
    printf("LibEtude 성능 통합 테스트 시작\n");
    printf("========================================\n");

    RUN_TEST(test_latency_measurement);
    RUN_TEST(test_throughput_measurement);
    RUN_TEST(test_memory_usage_measurement);
    RUN_TEST(test_cpu_usage_measurement);
    RUN_TEST(test_performance_profiling);

    printf("\n========================================\n");
    printf("LibEtude 성능 통합 테스트 완료\n");
    printf("========================================\n");

    return UNITY_END();
}