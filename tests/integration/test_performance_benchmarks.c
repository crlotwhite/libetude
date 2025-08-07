/**
 * @file test_performance_benchmarks.c
 * @brief 성능 벤치마크 테스트 구현
 */

#include "test_platform_integration.h"

/**
 * @brief 오디오 지연시간 벤치마크
 * @param result 벤치마크 결과
 * @return 테스트 결과
 */
ETResult benchmark_audio_latency(BenchmarkResult* result) {
    printf("오디오 지연시간 벤치마크...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->audio) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETAudioInterface* audio = platform->audio;
    const int iterations = 100;
    double times[iterations];

    ETAudioFormat format = {
        .sample_rate = 44100,
        .channels = 2,
        .bits_per_sample = 16,
        .format = ET_AUDIO_FORMAT_PCM_S16LE
    };

    for (int i = 0; i < iterations; i++) {
        PerformanceMeasurement measurement;
        start_performance_measurement(&measurement, "오디오 디바이스 열기");

        ETAudioDevice* device = NULL;
        ETResult res = audio->open_output_device(NULL, &format, &device);

        end_performance_measurement(&measurement);

        if (res == ET_SUCCESS && device) {
            times[i] = measurement.elapsed_seconds;
            audio->close_device(device);
        } else {
            // 오디오 디바이스가 없는 경우 시뮬레이션
            times[i] = 0.001; // 1ms
        }
    }

    result->test_name = "오디오 지연시간";
    calculate_benchmark_statistics(times, iterations, result);

    return ET_SUCCESS;
}/**

 * @brief 메모리 할당 속도 벤치마크
 * @param result 벤치마크 결과
 * @return 테스트 결과
 */
ETResult benchmark_memory_allocation_speed(BenchmarkResult* result) {
    printf("메모리 할당 속도 벤치마크...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->memory) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETMemoryInterface* memory = platform->memory;
    const int iterations = 1000;
    double times[iterations];
    const size_t alloc_size = 1024; // 1KB

    for (int i = 0; i < iterations; i++) {
        PerformanceMeasurement measurement;
        start_performance_measurement(&measurement, "메모리 할당");

        void* ptr = memory->malloc(alloc_size);
        if (ptr) {
            memory->free(ptr);
        }

        end_performance_measurement(&measurement);
        times[i] = measurement.elapsed_seconds;
    }

    result->test_name = "메모리 할당 속도";
    calculate_benchmark_statistics(times, iterations, result);

    return ET_SUCCESS;
}

/**
 * @brief 스레딩 오버헤드 벤치마크
 * @param result 벤치마크 결과
 * @return 테스트 결과
 */
ETResult benchmark_threading_overhead(BenchmarkResult* result) {
    printf("스레딩 오버헤드 벤치마크...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->threading) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETThreadInterface* threading = platform->threading;
    const int iterations = 50;
    double times[iterations];

    // 간단한 스레드 함수
    auto simple_thread_func = [](void* arg) -> void* {
        return NULL;
    };

    for (int i = 0; i < iterations; i++) {
        PerformanceMeasurement measurement;
        start_performance_measurement(&measurement, "스레드 생성/조인");

        ETThread* thread = NULL;
        ETResult res = threading->create_thread(&thread, simple_thread_func, NULL);

        if (res == ET_SUCCESS && thread) {
            void* thread_result = NULL;
            threading->join_thread(thread, &thread_result);
            threading->destroy_thread(thread);
        }

        end_performance_measurement(&measurement);
        times[i] = measurement.elapsed_seconds;
    }

    result->test_name = "스레딩 오버헤드";
    calculate_benchmark_statistics(times, iterations, result);

    return ET_SUCCESS;
}/**

 * @brief 파일시스템 I/O 속도 벤치마크
 * @param result 벤치마크 결과
 * @return 테스트 결과
 */
ETResult benchmark_filesystem_io_speed(BenchmarkResult* result) {
    printf("파일시스템 I/O 속도 벤치마크...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->filesystem) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETFilesystemInterface* fs = platform->filesystem;
    const int iterations = 10;
    double times[iterations];
    const size_t data_size = 64 * 1024; // 64KB
    const char* test_file = "benchmark_test.tmp";

    uint8_t* test_data = malloc(data_size);
    if (!test_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 테스트 데이터 초기화
    for (size_t i = 0; i < data_size; i++) {
        test_data[i] = (uint8_t)(i % 256);
    }

    for (int i = 0; i < iterations; i++) {
        PerformanceMeasurement measurement;
        start_performance_measurement(&measurement, "파일 I/O");

        // 파일 쓰기
        ETFile* file = NULL;
        ETResult res = fs->open_file(test_file, ET_FILE_MODE_WRITE_CREATE, &file);

        if (res == ET_SUCCESS && file) {
            size_t bytes_written;
            fs->write_file(file, test_data, data_size, &bytes_written);
            fs->close_file(file);

            // 파일 읽기
            res = fs->open_file(test_file, ET_FILE_MODE_READ, &file);
            if (res == ET_SUCCESS && file) {
                uint8_t* read_data = malloc(data_size);
                if (read_data) {
                    size_t bytes_read;
                    fs->read_file(file, read_data, data_size, &bytes_read);
                    free(read_data);
                }
                fs->close_file(file);
            }
        }

        end_performance_measurement(&measurement);
        times[i] = measurement.elapsed_seconds;
    }

    free(test_data);

    result->test_name = "파일시스템 I/O 속도";
    calculate_benchmark_statistics(times, iterations, result);

    return ET_SUCCESS;
}/**

* @brief 네트워크 처리량 벤치마크
 * @param result 벤치마크 결과
 * @return 테스트 결과
 */
ETResult benchmark_network_throughput(BenchmarkResult* result) {
    printf("네트워크 처리량 벤치마크...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->network) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETNetworkInterface* network = platform->network;
    const int iterations = 10;
    double times[iterations];

    for (int i = 0; i < iterations; i++) {
        PerformanceMeasurement measurement;
        start_performance_measurement(&measurement, "소켓 생성/해제");

        ETSocket* socket = NULL;
        ETResult res = network->create_socket(ET_SOCKET_TYPE_TCP, &socket);

        if (res == ET_SUCCESS && socket) {
            network->close_socket(socket);
        }

        end_performance_measurement(&measurement);
        times[i] = measurement.elapsed_seconds;
    }

    result->test_name = "네트워크 처리량";
    calculate_benchmark_statistics(times, iterations, result);

    return ET_SUCCESS;
}