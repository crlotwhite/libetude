#include "libetude/benchmark.h"
#include "libetude/api.h"
#include "libetude/tensor.h"
#include "libetude/fast_math.h"
#include "libetude/simd_kernels.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// 벤치마크 함수들
void benchmark_tensor_add(void* user_data) {
    // 텐서 덧셈 벤치마크
    int size = 1024 * 1024;
    float* a = malloc(size * sizeof(float));
    float* b = malloc(size * sizeof(float));
    float* c = malloc(size * sizeof(float));

    // 데이터 초기화
    for (int i = 0; i < size; i++) {
        a[i] = (float)i;
        b[i] = (float)(i + 1);
    }

    // 벤치마크 실행
    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }

    free(a);
    free(b);
    free(c);
}

void benchmark_tensor_mul(void* user_data) {
    // 텐서 곱셈 벤치마크
    int size = 1024 * 1024;
    float* a = malloc(size * sizeof(float));
    float* b = malloc(size * sizeof(float));
    float* c = malloc(size * sizeof(float));

    // 데이터 초기화
    for (int i = 0; i < size; i++) {
        a[i] = (float)i * 0.001f;
        b[i] = (float)(i + 1) * 0.001f;
    }

    // 벤치마크 실행
    for (int i = 0; i < size; i++) {
        c[i] = a[i] * b[i];
    }

    free(a);
    free(b);
    free(c);
}

void benchmark_fast_math_exp(void* user_data) {
    // 고속 지수 함수 벤치마크
    int size = 100000;
    float* input = malloc(size * sizeof(float));
    float* output = malloc(size * sizeof(float));

    // 데이터 초기화
    for (int i = 0; i < size; i++) {
        input[i] = (float)i * 0.0001f - 5.0f;
    }

    // 벤치마크 실행
    for (int i = 0; i < size; i++) {
        output[i] = et_fast_exp(input[i]);
    }

    free(input);
    free(output);
}

void benchmark_fast_math_tanh(void* user_data) {
    // 고속 tanh 함수 벤치마크
    int size = 100000;
    float* input = malloc(size * sizeof(float));
    float* output = malloc(size * sizeof(float));

    // 데이터 초기화
    for (int i = 0; i < size; i++) {
        input[i] = (float)i * 0.0001f - 5.0f;
    }

    // 벤치마크 실행
    for (int i = 0; i < size; i++) {
        output[i] = et_fast_tanh(input[i]);
    }

    free(input);
    free(output);
}

void benchmark_simd_vector_add(void* user_data) {
    // SIMD 벡터 덧셈 벤치마크
    int size = 1024 * 1024;
    float* a = malloc(size * sizeof(float));
    float* b = malloc(size * sizeof(float));
    float* c = malloc(size * sizeof(float));

    // 데이터 초기화
    for (int i = 0; i < size; i++) {
        a[i] = (float)i;
        b[i] = (float)(i + 1);
    }

    // SIMD 벡터 덧셈 실행
    et_simd_vector_add(a, b, c, size);

    free(a);
    free(b);
    free(c);
}

void benchmark_matrix_multiply(void* user_data) {
    // 행렬 곱셈 벤치마크
    int size = 512;
    float* a = malloc(size * size * sizeof(float));
    float* b = malloc(size * size * sizeof(float));
    float* c = malloc(size * size * sizeof(float));

    // 데이터 초기화
    for (int i = 0; i < size * size; i++) {
        a[i] = (float)i * 0.001f;
        b[i] = (float)(i + 1) * 0.001f;
    }

    // 행렬 곱셈 실행 (간단한 구현)
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            float sum = 0.0f;
            for (int k = 0; k < size; k++) {
                sum += a[i * size + k] * b[k * size + j];
            }
            c[i * size + j] = sum;
        }
    }

    free(a);
    free(b);
    free(c);
}

// 도움말 출력
void print_usage(const char* program_name) {
    printf("사용법: %s [옵션]\n", program_name);
    printf("옵션:\n");
    printf("  -h, --help              이 도움말 출력\n");
    printf("  -o, --output FILE       결과를 파일로 저장\n");
    printf("  -f, --format FORMAT     출력 형식 (text, json, csv)\n");
    printf("  -i, --iterations N      측정 반복 횟수 (기본값: 10)\n");
    printf("  -w, --warmup N          워밍업 반복 횟수 (기본값: 3)\n");
    printf("  -t, --timeout SECONDS   타임아웃 (기본값: 30)\n");
    printf("  -q, --quick             빠른 벤치마크 모드\n");
    printf("  -v, --verbose           상세 출력\n");
    printf("  --memory                메모리 사용량 측정\n");
    printf("  --cpu                   CPU 사용률 측정\n");
    printf("  --system-info           시스템 정보 출력\n");
}

int main(int argc, char* argv[]) {
    // 기본 설정
    ETBenchmarkConfig config = ET_BENCHMARK_CONFIG_DEFAULT;
    const char* output_file = NULL;
    const char* output_format = "text";
    bool verbose = false;
    bool show_system_info = false;

    // 명령행 옵션 파싱
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"output", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'f'},
        {"iterations", required_argument, 0, 'i'},
        {"warmup", required_argument, 0, 'w'},
        {"timeout", required_argument, 0, 't'},
        {"quick", no_argument, 0, 'q'},
        {"verbose", no_argument, 0, 'v'},
        {"memory", no_argument, 0, 0},
        {"cpu", no_argument, 0, 0},
        {"system-info", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "ho:f:i:w:t:qv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'o':
                output_file = optarg;
                break;
            case 'f':
                output_format = optarg;
                break;
            case 'i':
                config.measurement_iterations = atoi(optarg);
                break;
            case 'w':
                config.warmup_iterations = atoi(optarg);
                break;
            case 't':
                config.timeout_seconds = atof(optarg);
                break;
            case 'q':
                config = ET_BENCHMARK_CONFIG_QUICK;
                break;
            case 'v':
                verbose = true;
                break;
            case 0:
                if (strcmp(long_options[option_index].name, "memory") == 0) {
                    config.measure_memory = true;
                } else if (strcmp(long_options[option_index].name, "cpu") == 0) {
                    config.measure_cpu = true;
                } else if (strcmp(long_options[option_index].name, "system-info") == 0) {
                    show_system_info = true;
                }
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // 벤치마크 프레임워크 초기화
    if (et_benchmark_init() != ET_SUCCESS) {
        fprintf(stderr, "벤치마크 프레임워크 초기화 실패\n");
        return 1;
    }

    // 시스템 정보 출력
    if (show_system_info || verbose) {
        ETSystemInfo sys_info;
        if (et_get_system_info(&sys_info) == ET_SUCCESS) {
            printf("시스템 정보:\n");
            printf("  OS: %s\n", sys_info.os_name);
            printf("  CPU: %s (%d 코어, %d 스레드)\n",
                   sys_info.cpu_name, sys_info.cpu_cores, sys_info.cpu_threads);
            printf("  메모리: %lu MB (사용 가능: %lu MB)\n",
                   sys_info.memory_total_mb, sys_info.memory_available_mb);
            printf("  컴파일러: %s\n", sys_info.compiler_version);
            printf("\n");
        }
    }

    // 벤치마크 스위트 생성
    ETBenchmarkSuite* suite = et_create_benchmark_suite("LibEtude 성능 벤치마크", &config);
    if (!suite) {
        fprintf(stderr, "벤치마크 스위트 생성 실패\n");
        et_benchmark_cleanup();
        return 1;
    }

    // 벤치마크 추가
    et_add_benchmark(suite, "텐서 덧셈", benchmark_tensor_add, NULL);
    et_add_benchmark(suite, "텐서 곱셈", benchmark_tensor_mul, NULL);
    et_add_benchmark(suite, "고속 지수함수", benchmark_fast_math_exp, NULL);
    et_add_benchmark(suite, "고속 tanh", benchmark_fast_math_tanh, NULL);
    et_add_benchmark(suite, "SIMD 벡터 덧셈", benchmark_simd_vector_add, NULL);
    et_add_benchmark(suite, "행렬 곱셈", benchmark_matrix_multiply, NULL);

    // 벤치마크 실행
    printf("LibEtude 벤치마크 도구\n");
    printf("설정: 워밍업 %d회, 측정 %d회, 타임아웃 %.1f초\n\n",
           config.warmup_iterations, config.measurement_iterations, config.timeout_seconds);

    int result = et_run_benchmark_suite(suite);

    // 결과 저장
    if (output_file && result == ET_SUCCESS) {
        if (et_save_benchmark_results(suite->results, suite->num_benchmarks,
                                      output_file, output_format) == ET_SUCCESS) {
            printf("결과가 %s 파일로 저장되었습니다.\n", output_file);
        } else {
            fprintf(stderr, "결과 저장 실패: %s\n", output_file);
        }
    }

    // 정리
    et_destroy_benchmark_suite(suite);
    et_benchmark_cleanup();

    return result == ET_SUCCESS ? 0 : 1;
}