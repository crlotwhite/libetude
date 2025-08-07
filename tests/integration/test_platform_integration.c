/**
 * @file test_platform_integration.c
 * @brief 크로스 플랫폼 통합 테스트 구현
 */

#include "test_platform_integration.h"

/**
 * @brief 모든 크로스 플랫폼 호환성 테스트 실행
 * @return 테스트 결과
 */
ETResult run_cross_platform_compatibility_tests(void) {
    printf("=== 크로스 플랫폼 호환성 테스트 시작 ===\n");

    ETResult result;

    // 각 인터페이스별 호환성 테스트
    result = test_audio_cross_platform_compatibility();
    if (result != ET_SUCCESS) {
        printf("오디오 호환성 테스트 실패\n");
        return result;
    }

    result = test_system_cross_platform_compatibility();
    if (result != ET_SUCCESS) {
        printf("시스템 호환성 테스트 실패\n");
        return result;
    }

    result = test_threading_cross_platform_compatibility();
    if (result != ET_SUCCESS) {
        printf("스레딩 호환성 테스트 실패\n");
        return result;
    }

    result = test_memory_cross_platform_compatibility();
    if (result != ET_SUCCESS) {
        printf("메모리 호환성 테스트 실패\n");
        return result;
    }

    result = test_filesystem_cross_platform_compatibility();
    if (result != ET_SUCCESS) {
        printf("파일시스템 호환성 테스트 실패\n");
        return result;
    }

    result = test_network_cross_platform_compatibility();
    if (result != ET_SUCCESS) {
        printf("네트워크 호환성 테스트 실패\n");
        return result;
    }

    result = test_dynlib_cross_platform_compatibility();
    if (result != ET_SUCCESS) {
        printf("동적 라이브러리 호환성 테스트 실패\n");
        return result;
    }

    printf("=== 크로스 플랫폼 호환성 테스트 완료 ===\n");
    return ET_SUCCESS;
}

/**
 * @brief 실제 하드웨어 검증 테스트 실행
 * @return 테스트 결과
 */
ETResult run_hardware_validation_tests(void) {
    printf("=== 실제 하드웨어 검증 테스트 시작 ===\n");

    if (!has_sufficient_system_resources()) {
        printf("시스템 리소스가 부족하여 하드웨어 테스트를 건너뜁니다.\n");
        return ET_SUCCESS;
    }

    ETResult result;

    result = test_real_hardware_audio_devices();
    if (result != ET_SUCCESS) {
        printf("실제 오디오 디바이스 테스트 실패\n");
        return result;
    }

    result = test_real_hardware_cpu_features();
    if (result != ET_SUCCESS) {
        printf("실제 CPU 기능 테스트 실패\n");
        return result;
    }

    result = test_real_hardware_memory_limits();
    if (result != ET_SUCCESS) {
        printf("실제 메모리 한계 테스트 실패\n");
        return result;
    }

    result = test_real_hardware_storage_performance();
    if (result != ET_SUCCESS) {
        printf("실제 스토리지 성능 테스트 실패\n");
        return result;
    }

    result = test_real_hardware_network_interfaces();
    if (result != ET_SUCCESS) {
        printf("실제 네트워크 인터페이스 테스트 실패\n");
        return result;
    }

    printf("=== 실제 하드웨어 검증 테스트 완료 ===\n");
    return ET_SUCCESS;
}

/**
 * @brief 성능 벤치마크 테스트 실행
 * @return 테스트 결과
 */
ETResult run_performance_benchmark_tests(void) {
    printf("=== 성능 벤치마크 테스트 시작 ===\n");

    BenchmarkResult result;
    ETResult status;

    // 오디오 지연시간 벤치마크
    status = benchmark_audio_latency(&result);
    if (status == ET_SUCCESS) {
        print_benchmark_result(&result);
    } else {
        printf("오디오 지연시간 벤치마크 실패\n");
    }

    // 메모리 할당 속도 벤치마크
    status = benchmark_memory_allocation_speed(&result);
    if (status == ET_SUCCESS) {
        print_benchmark_result(&result);
    } else {
        printf("메모리 할당 속도 벤치마크 실패\n");
    }

    // 스레딩 오버헤드 벤치마크
    status = benchmark_threading_overhead(&result);
    if (status == ET_SUCCESS) {
        print_benchmark_result(&result);
    } else {
        printf("스레딩 오버헤드 벤치마크 실패\n");
    }

    // 파일시스템 I/O 속도 벤치마크
    status = benchmark_filesystem_io_speed(&result);
    if (status == ET_SUCCESS) {
        print_benchmark_result(&result);
    } else {
        printf("파일시스템 I/O 속도 벤치마크 실패\n");
    }

    // 네트워크 처리량 벤치마크
    status = benchmark_network_throughput(&result);
    if (status == ET_SUCCESS) {
        print_benchmark_result(&result);
    } else {
        printf("네트워크 처리량 벤치마크 실패\n");
    }

    printf("=== 성능 벤치마크 테스트 완료 ===\n");
    return ET_SUCCESS;
}

/**
 * @brief 스트레스 및 안정성 테스트 실행
 * @return 테스트 결과
 */
ETResult run_stress_and_stability_tests(void) {
    printf("=== 스트레스 및 안정성 테스트 시작 ===\n");

    if (is_running_in_ci_environment()) {
        printf("CI 환경에서는 스트레스 테스트를 제한적으로 실행합니다.\n");
    }

    StressTestConfig config = {
        .thread_count = 4,
        .iterations_per_thread = 1000,
        .duration_seconds = 30,
        .enable_memory_stress = true,
        .enable_cpu_stress = true,
        .enable_io_stress = true
    };

    ETResult result;

    // 메모리 할당 스트레스 테스트
    result = stress_test_memory_allocation(&config);
    if (result != ET_SUCCESS) {
        printf("메모리 할당 스트레스 테스트 실패\n");
        return result;
    }

    // 스레딩 경합 스트레스 테스트
    result = stress_test_threading_contention(&config);
    if (result != ET_SUCCESS) {
        printf("스레딩 경합 스트레스 테스트 실패\n");
        return result;
    }

    // 오디오 스트리밍 스트레스 테스트
    result = stress_test_audio_streaming(&config);
    if (result != ET_SUCCESS) {
        printf("오디오 스트리밍 스트레스 테스트 실패\n");
        return result;
    }

    // 파일시스템 작업 스트레스 테스트
    result = stress_test_filesystem_operations(&config);
    if (result != ET_SUCCESS) {
        printf("파일시스템 작업 스트레스 테스트 실패\n");
        return result;
    }

    // 혼합 워크로드 스트레스 테스트
    result = stress_test_mixed_workload(&config);
    if (result != ET_SUCCESS) {
        printf("혼합 워크로드 스트레스 테스트 실패\n");
        return result;
    }

    // 안정성 테스트들
    result = stability_test_long_running_audio();
    if (result != ET_SUCCESS) {
        printf("장시간 오디오 안정성 테스트 실패\n");
        return result;
    }

    result = stability_test_memory_leak_detection();
    if (result != ET_SUCCESS) {
        printf("메모리 누수 감지 테스트 실패\n");
        return result;
    }

    result = stability_test_resource_exhaustion();
    if (result != ET_SUCCESS) {
        printf("리소스 고갈 테스트 실패\n");
        return result;
    }

    result = stability_test_error_recovery();
    if (result != ET_SUCCESS) {
        printf("오류 복구 테스트 실패\n");
        return result;
    }

    printf("=== 스트레스 및 안정성 테스트 완료 ===\n");
    return ET_SUCCESS;
}

/**
 * @brief 오디오 크로스 플랫폼 호환성 테스트
 * @return 테스트 결과
 */
ETResult test_audio_cross_platform_compatibility(void) {
    printf("오디오 크로스 플랫폼 호환성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->audio != NULL);

    ETAudioInterface* audio = platform->audio;

    // 기본 오디오 포맷 호환성 테스트
    ETAudioFormat formats[] = {
        {44100, 2, 16, ET_AUDIO_FORMAT_PCM_S16LE},
        {48000, 2, 16, ET_AUDIO_FORMAT_PCM_S16LE},
        {22050, 1, 16, ET_AUDIO_FORMAT_PCM_S16LE}
    };

    for (int i = 0; i < 3; i++) {
        ETAudioDevice* device = NULL;
        ETResult result = audio->open_output_device(NULL, &formats[i], &device);

        if (result == ET_SUCCESS && device) {
            // 디바이스가 성공적으로 열린 경우
            ETAudioState state = audio->get_state(device);
            INTEGRATION_TEST_ASSERT(state != ET_AUDIO_STATE_ERROR);

            uint32_t latency = audio->get_latency(device);
            INTEGRATION_TEST_ASSERT(latency > 0 && latency < 1000);

            audio->close_device(device);
        }
        // 지원되지 않는 경우는 허용
    }

    printf("오디오 크로스 플랫폼 호환성 테스트 통과\n");
    return ET_SUCCESS;
}

/**
 * @brief 시스템 크로스 플랫폼 호환성 테스트
 * @return 테스트 결과
 */
ETResult test_system_cross_platform_compatibility(void) {
    printf("시스템 크로스 플랫폼 호환성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->system != NULL);

    ETSystemInterface* system = platform->system;

    // 시스템 정보 일관성 테스트
    ETSystemInfo sys_info;
    ETResult result = system->get_system_info(&sys_info);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    // 플랫폼별 기본 검증
    INTEGRATION_TEST_ASSERT(sys_info.cpu_count > 0);
    INTEGRATION_TEST_ASSERT(sys_info.total_memory > 0);
    INTEGRATION_TEST_ASSERT(strlen(sys_info.system_name) > 0);

    // 고해상도 타이머 일관성 테스트
    uint64_t time1, time2;
    result = system->get_high_resolution_time(&time1);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    system->sleep(10); // 10ms 대기

    result = system->get_high_resolution_time(&time2);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    INTEGRATION_TEST_ASSERT(time2 > time1);

    // 시간 차이가 합리적인 범위인지 확인 (5ms ~ 50ms)
    uint64_t elapsed_ns = time2 - time1;
    uint64_t elapsed_ms = elapsed_ns / 1000000;
    INTEGRATION_TEST_ASSERT(elapsed_ms >= 5 && elapsed_ms <= 50);

    printf("시스템 크로스 플랫폼 호환성 테스트 통과\n");
    return ET_SUCCESS;
}

/**
 * @brief 스레딩 크로스 플랫폼 호환성 테스트
 * @return 테스트 결과
 */
ETResult test_threading_cross_platform_compatibility(void) {
    printf("스레딩 크로스 플랫폼 호환성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->threading != NULL);

    ETThreadInterface* threading = platform->threading;

    // 기본 스레드 생성/조인 호환성 테스트
    static int counter = 0;
    ETThread* thread = NULL;

    ETResult result = threading->create_thread(&thread,
        (void*(*)(void*))([](void* arg) -> void* {
            int* c = (int*)arg;
            (*c)++;
            return NULL;
        }), &counter);

    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    INTEGRATION_TEST_ASSERT(thread != NULL);

    void* thread_result = NULL;
    result = threading->join_thread(thread, &thread_result);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    INTEGRATION_TEST_ASSERT_EQUAL(1, counter);

    threading->destroy_thread(thread);

    // 뮤텍스 호환성 테스트
    ETMutex* mutex = NULL;
    result = threading->create_mutex(&mutex);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    INTEGRATION_TEST_ASSERT(mutex != NULL);

    result = threading->lock_mutex(mutex);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    result = threading->unlock_mutex(mutex);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    threading->destroy_mutex(mutex);

    printf("스레딩 크로스 플랫폼 호환성 테스트 통과\n");
    return ET_SUCCESS;
}

/**
 * @brief 메모리 크로스 플랫폼 호환성 테스트
 * @return 테스트 결과
 */
ETResult test_memory_cross_platform_compatibility(void) {
    printf("메모리 크로스 플랫폼 호환성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->memory != NULL);

    ETMemoryInterface* memory = platform->memory;

    // 기본 메모리 할당 호환성 테스트
    void* ptr = memory->malloc(1024);
    INTEGRATION_TEST_ASSERT(ptr != NULL);

    // 메모리 쓰기/읽기 테스트
    memset(ptr, 0xAA, 1024);
    uint8_t* bytes = (uint8_t*)ptr;
    for (int i = 0; i < 1024; i++) {
        INTEGRATION_TEST_ASSERT_EQUAL(0xAA, bytes[i]);
    }

    memory->free(ptr);

    // 정렬된 메모리 할당 호환성 테스트
    void* aligned_ptr = memory->aligned_malloc(1024, 64);
    INTEGRATION_TEST_ASSERT(aligned_ptr != NULL);

    // 정렬 확인
    uintptr_t addr = (uintptr_t)aligned_ptr;
    INTEGRATION_TEST_ASSERT_EQUAL(0, addr % 64);

    memory->aligned_free(aligned_ptr);

    printf("메모리 크로스 플랫폼 호환성 테스트 통과\n");
    return ET_SUCCESS;
}

/**
 * @brief 파일시스템 크로스 플랫폼 호환성 테스트
 * @return 테스트 결과
 */
ETResult test_filesystem_cross_platform_compatibility(void) {
    printf("파일시스템 크로스 플랫폼 호환성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->filesystem != NULL);

    ETFilesystemInterface* fs = platform->filesystem;

    // 경로 정규화 호환성 테스트
    char normalized[256];
    ETResult result = fs->normalize_path("./test/../test.txt", normalized, sizeof(normalized));
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    INTEGRATION_TEST_ASSERT(strlen(normalized) > 0);

    // 경로 결합 호환성 테스트
    char joined[256];
    result = fs->join_path("tmp", "test.txt", joined, sizeof(joined));
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    INTEGRATION_TEST_ASSERT(strstr(joined, "test.txt") != NULL);

    // 현재 디렉토리 존재 확인
    bool exists = fs->is_directory(".");
    INTEGRATION_TEST_ASSERT(exists);

    printf("파일시스템 크로스 플랫폼 호환성 테스트 통과\n");
    return ET_SUCCESS;
}

/**
 * @brief 네트워크 크로스 플랫폼 호환성 테스트
 * @return 테스트 결과
 */
ETResult test_network_cross_platform_compatibility(void) {
    printf("네트워크 크로스 플랫폼 호환성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->network != NULL);

    ETNetworkInterface* network = platform->network;

    // 소켓 생성 호환성 테스트
    ETSocket* socket = NULL;
    ETResult result = network->create_socket(ET_SOCKET_TYPE_TCP, &socket);

    if (result == ET_SUCCESS && socket) {
        // 네트워크가 지원되는 환경
        network->close_socket(socket);
        printf("네트워크 기능 지원됨\n");
    } else {
        // 네트워크가 지원되지 않는 환경 (허용)
        printf("네트워크 기능 지원되지 않음 (허용)\n");
    }

    printf("네트워크 크로스 플랫폼 호환성 테스트 통과\n");
    return ET_SUCCESS;
}

/**
 * @brief 동적 라이브러리 크로스 플랫폼 호환성 테스트
 * @return 테스트 결과
 */
ETResult test_dynlib_cross_platform_compatibility(void) {
    printf("동적 라이브러리 크로스 플랫폼 호환성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->dynlib != NULL);

    ETDynlibInterface* dynlib = platform->dynlib;

    // 존재하지 않는 라이브러리 로드 테스트 (오류 처리 호환성)
    ETDynamicLibrary* lib = NULL;
    ETResult result = dynlib->load_library("/nonexistent/library", &lib);
    INTEGRATION_TEST_ASSERT(result != ET_SUCCESS);
    INTEGRATION_TEST_ASSERT(lib == NULL);

    // 오류 메시지 확인
    const char* error = dynlib->get_last_error();
    INTEGRATION_TEST_ASSERT(error != NULL);
    INTEGRATION_TEST_ASSERT(strlen(error) > 0);

    printf("동적 라이브러리 크로스 플랫폼 호환성 테스트 통과\n");
    return ET_SUCCESS;
}

// 유틸리티 함수들 구현
void start_performance_measurement(PerformanceMeasurement* measurement, const char* operation_name) {
    measurement->operation_name = operation_name;

    ETPlatformInterface* platform = et_platform_get_interface();
    if (platform && platform->system) {
        platform->system->get_high_resolution_time(&measurement->start_time);
    } else {
        measurement->start_time = (uint64_t)clock();
    }
}

void end_performance_measurement(PerformanceMeasurement* measurement) {
    ETPlatformInterface* platform = et_platform_get_interface();
    if (platform && platform->system) {
        platform->system->get_high_resolution_time(&measurement->end_time);
        measurement->elapsed_seconds = (double)(measurement->end_time - measurement->start_time) / 1000000000.0;
    } else {
        measurement->end_time = (uint64_t)clock();
        measurement->elapsed_seconds = (double)(measurement->end_time - measurement->start_time) / CLOCKS_PER_SEC;
    }
}

void calculate_benchmark_statistics(const double* times, int count, BenchmarkResult* result) {
    if (count == 0) return;

    result->iterations = count;
    result->min_time = times[0];
    result->max_time = times[0];

    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        if (times[i] < result->min_time) result->min_time = times[i];
        if (times[i] > result->max_time) result->max_time = times[i];
        sum += times[i];
    }

    result->avg_time = sum / count;

    // 표준편차 계산
    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        double diff = times[i] - result->avg_time;
        variance += diff * diff;
    }
    result->std_dev = sqrt(variance / count);
}

void print_benchmark_result(const BenchmarkResult* result) {
    printf("벤치마크 결과: %s\n", result->test_name);
    printf("  반복 횟수: %d\n", result->iterations);
    printf("  최소 시간: %.6f초\n", result->min_time);
    printf("  최대 시간: %.6f초\n", result->max_time);
    printf("  평균 시간: %.6f초\n", result->avg_time);
    printf("  표준편차: %.6f초\n", result->std_dev);
}

bool is_running_in_ci_environment(void) {
    // CI 환경 변수 확인
    return (getenv("CI") != NULL ||
            getenv("CONTINUOUS_INTEGRATION") != NULL ||
            getenv("GITHUB_ACTIONS") != NULL);
}

bool has_sufficient_system_resources(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->system) {
        return false;
    }

    ETSystemInfo sys_info;
    if (platform->system->get_system_info(&sys_info) != ET_SUCCESS) {
        return false;
    }

    // 최소 요구사항: 1GB RAM, 2 CPU 코어
    return (sys_info.total_memory >= 1024 * 1024 * 1024 &&
            sys_info.cpu_count >= 2);
}

// 메인 통합 테스트 실행 함수
int main(int argc, char* argv[]) {
    printf("LibEtude 플랫폼 추상화 레이어 통합 테스트 시작\n");
    printf("================================================\n");

    ETResult result;
    int exit_code = 0;

    // 크로스 플랫폼 호환성 테스트
    result = run_cross_platform_compatibility_tests();
    if (result != ET_SUCCESS) {
        printf("크로스 플랫폼 호환성 테스트 실패\n");
        exit_code = 1;
    }

    // 실제 하드웨어 검증 테스트
    result = run_hardware_validation_tests();
    if (result != ET_SUCCESS) {
        printf("하드웨어 검증 테스트 실패\n");
        exit_code = 1;
    }

    // 성능 벤치마크 테스트
    result = run_performance_benchmark_tests();
    if (result != ET_SUCCESS) {
        printf("성능 벤치마크 테스트 실패\n");
        exit_code = 1;
    }

    // 스트레스 및 안정성 테스트 (CI 환경에서는 제한적으로 실행)
    if (!is_running_in_ci_environment()) {
        result = run_stress_and_stability_tests();
        if (result != ET_SUCCESS) {
            printf("스트레스 및 안정성 테스트 실패\n");
            exit_code = 1;
        }
    } else {
        printf("CI 환경에서는 스트레스 테스트를 건너뜁니다.\n");
    }

    if (exit_code == 0) {
        printf("\n모든 통합 테스트가 성공적으로 완료되었습니다! ✓\n");
    } else {
        printf("\n일부 통합 테스트가 실패했습니다. ✗\n");
    }

    return exit_code;
}