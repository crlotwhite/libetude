/**
 * @file test_hardware_validation.c
 * @brief 실제 하드웨어 검증 테스트 구현
 */

#include "test_platform_integration.h"

/**
 * @brief 실제 오디오 디바이스 테스트
 * @return 테스트 결과
 */
ETResult test_real_hardware_audio_devices(void) {
    printf("실제 오디오 디바이스 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->audio != NULL);

    ETAudioInterface* audio = platform->audio;

    // 출력 디바이스 열거
    ETAudioDeviceInfo output_devices[16];
    int output_count = 16;
    ETResult result = audio->enumerate_devices(ET_AUDIO_DEVICE_OUTPUT, output_devices, &output_count);

    if (result == ET_SUCCESS && output_count > 0) {
        printf("발견된 출력 디바이스: %d개\n", output_count);

        for (int i = 0; i < output_count; i++) {
            printf("  디바이스 %d: %s\n", i, output_devices[i].name);
            printf("    최대 채널: %d\n", output_devices[i].max_channels);
            printf("    지원 샘플 레이트: ");
            for (int j = 0; j < output_devices[i].rate_count; j++) {
                printf("%d ", output_devices[i].supported_rates[j]);
            }
            printf("\n");

            // 기본 디바이스인지 확인
            if (output_devices[i].is_default) {
                printf("    (기본 디바이스)\n");

                // 기본 디바이스로 실제 테스트
                ETAudioFormat format = {
                    .sample_rate = 44100,
                    .channels = 2,
                    .bits_per_sample = 16,
                    .format = ET_AUDIO_FORMAT_PCM_S16LE
                };

                ETAudioDevice* device = NULL;
                result = audio->open_output_device(output_devices[i].name, &format, &device);

                if (result == ET_SUCCESS && device) {
                    printf("    기본 디바이스 열기 성공\n");

                    // 지연시간 측정
                    uint32_t latency = audio->get_latency(device);
                    printf("    지연시간: %dms\n", latency);
                    INTEGRATION_TEST_ASSERT(latency > 0 && latency < 1000);

                    // 디바이스 상태 확인
                    ETAudioState state = audio->get_state(device);
                    INTEGRATION_TEST_ASSERT(state != ET_AUDIO_STATE_ERROR);

                    audio->close_device(device);
                } else {
                    printf("    기본 디바이스 열기 실패: %d\n", result);
                }
            }
        }
    } else {
        printf("오디오 출력 디바이스를 찾을 수 없습니다 (허용)\n");
    }

    // 입력 디바이스 열거
    ETAudioDeviceInfo input_devices[16];
    int input_count = 16;
    result = audio->enumerate_devices(ET_AUDIO_DEVICE_INPUT, input_devices, &input_count);

    if (result == ET_SUCCESS && input_count > 0) {
        printf("발견된 입력 디바이스: %d개\n", input_count);

        for (int i = 0; i < input_count; i++) {
            printf("  디바이스 %d: %s\n", i, input_devices[i].name);
        }
    } else {
        printf("오디오 입력 디바이스를 찾을 수 없습니다 (허용)\n");
    }

    printf("실제 오디오 디바이스 테스트 완료\n");
    return ET_SUCCESS;
}

/**
 * @brief 실제 CPU 기능 테스트
 * @return 테스트 결과
 */
ETResult test_real_hardware_cpu_features(void) {
    printf("실제 CPU 기능 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->system != NULL);

    ETSystemInterface* system = platform->system;

    // CPU 정보 조회
    ETCPUInfo cpu_info;
    ETResult result = system->get_cpu_info(&cpu_info);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    printf("CPU 정보:\n");
    printf("  제조사: %s\n", cpu_info.vendor);
    printf("  브랜드: %s\n", cpu_info.brand);
    printf("  코어 수: %d\n", cpu_info.core_count);
    printf("  스레드 수: %d\n", cpu_info.thread_count);
    printf("  기본 주파수: %d MHz\n", cpu_info.base_frequency);

    // 기본 검증
    INTEGRATION_TEST_ASSERT(cpu_info.core_count > 0);
    INTEGRATION_TEST_ASSERT(cpu_info.thread_count >= cpu_info.core_count);
    INTEGRATION_TEST_ASSERT(strlen(cpu_info.vendor) > 0);
    INTEGRATION_TEST_ASSERT(strlen(cpu_info.brand) > 0);

    // SIMD 기능 테스트
    uint32_t simd_features = system->get_simd_features();
    printf("SIMD 기능:\n");

    if (system->has_feature(ET_HARDWARE_FEATURE_SSE)) {
        printf("  SSE: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_SSE2)) {
        printf("  SSE2: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_SSE3)) {
        printf("  SSE3: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_SSSE3)) {
        printf("  SSSE3: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_SSE4_1)) {
        printf("  SSE4.1: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_SSE4_2)) {
        printf("  SSE4.2: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_AVX)) {
        printf("  AVX: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_AVX2)) {
        printf("  AVX2: 지원됨\n");
    }
    if (system->has_feature(ET_HARDWARE_FEATURE_NEON)) {
        printf("  NEON: 지원됨\n");
    }

    // 플랫폼별 기본 기능 확인
#ifdef __x86_64__
    // x86_64에서는 최소한 SSE2는 지원해야 함
    INTEGRATION_TEST_ASSERT(system->has_feature(ET_HARDWARE_FEATURE_SSE2));
#endif

#ifdef __aarch64__
    // ARM64에서는 NEON을 지원해야 함
    INTEGRATION_TEST_ASSERT(system->has_feature(ET_HARDWARE_FEATURE_NEON));
#endif

    printf("실제 CPU 기능 테스트 완료\n");
    return ET_SUCCESS;
}

/**
 * @brief 실제 메모리 한계 테스트
 * @return 테스트 결과
 */
ETResult test_real_hardware_memory_limits(void) {
    printf("실제 메모리 한계 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->system != NULL);
    INTEGRATION_TEST_ASSERT(platform->memory != NULL);

    ETSystemInterface* system = platform->system;
    ETMemoryInterface* memory = platform->memory;

    // 메모리 정보 조회
    ETMemoryInfo mem_info;
    ETResult result = system->get_memory_info(&mem_info);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    printf("메모리 정보:\n");
    printf("  총 물리 메모리: %llu MB\n", mem_info.total_physical / (1024 * 1024));
    printf("  사용 가능한 물리 메모리: %llu MB\n", mem_info.available_physical / (1024 * 1024));
    printf("  총 가상 메모리: %llu MB\n", mem_info.total_virtual / (1024 * 1024));
    printf("  사용 가능한 가상 메모리: %llu MB\n", mem_info.available_virtual / (1024 * 1024));

    // 기본 검증
    INTEGRATION_TEST_ASSERT(mem_info.total_physical > 0);
    INTEGRATION_TEST_ASSERT(mem_info.available_physical <= mem_info.total_physical);
    INTEGRATION_TEST_ASSERT(mem_info.total_virtual >= mem_info.total_physical);

    // 메모리 사용률 테스트
    ETMemoryUsage mem_usage;
    result = system->get_memory_usage(&mem_usage);
    INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);

    printf("메모리 사용률:\n");
    printf("  물리 메모리 사용률: %.1f%%\n", mem_usage.physical_usage_percent);
    printf("  가상 메모리 사용률: %.1f%%\n", mem_usage.virtual_usage_percent);

    INTEGRATION_TEST_ASSERT(mem_usage.physical_usage_percent >= 0.0f &&
                           mem_usage.physical_usage_percent <= 100.0f);

    // 대용량 메모리 할당 테스트 (사용 가능한 메모리의 10% 또는 최대 100MB)
    uint64_t test_size = mem_info.available_physical / 10;
    if (test_size > 100 * 1024 * 1024) {
        test_size = 100 * 1024 * 1024; // 최대 100MB
    }
    if (test_size < 1024 * 1024) {
        test_size = 1024 * 1024; // 최소 1MB
    }

    printf("대용량 메모리 할당 테스트: %llu MB\n", test_size / (1024 * 1024));

    void* large_ptr = memory->malloc(test_size);
    if (large_ptr) {
        printf("  대용량 메모리 할당 성공\n");

        // 메모리 쓰기 테스트 (일부만)
        memset(large_ptr, 0x55, 4096); // 첫 4KB만 테스트

        uint8_t* bytes = (uint8_t*)large_ptr;
        for (int i = 0; i < 4096; i++) {
            INTEGRATION_TEST_ASSERT_EQUAL(0x55, bytes[i]);
        }

        memory->free(large_ptr);
        printf("  대용량 메모리 해제 완료\n");
    } else {
        printf("  대용량 메모리 할당 실패 (메모리 부족)\n");
    }

    // 정렬된 메모리 할당 테스트
    void* aligned_ptrs[8];
    size_t alignments[] = {16, 32, 64, 128, 256, 512, 1024, 4096};

    printf("정렬된 메모리 할당 테스트:\n");
    for (int i = 0; i < 8; i++) {
        aligned_ptrs[i] = memory->aligned_malloc(1024, alignments[i]);
        if (aligned_ptrs[i]) {
            uintptr_t addr = (uintptr_t)aligned_ptrs[i];
            INTEGRATION_TEST_ASSERT_EQUAL(0, addr % alignments[i]);
            printf("  %zu바이트 정렬: 성공\n", alignments[i]);
        } else {
            printf("  %zu바이트 정렬: 실패\n", alignments[i]);
        }
    }

    // 정렬된 메모리 해제
    for (int i = 0; i < 8; i++) {
        if (aligned_ptrs[i]) {
            memory->aligned_free(aligned_ptrs[i]);
        }
    }

    printf("실제 메모리 한계 테스트 완료\n");
    return ET_SUCCESS;
}

/**
 * @brief 실제 스토리지 성능 테스트
 * @return 테스트 결과
 */
ETResult test_real_hardware_storage_performance(void) {
    printf("실제 스토리지 성능 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->filesystem != NULL);

    ETFilesystemInterface* fs = platform->filesystem;

    // 임시 파일 생성 및 쓰기 성능 테스트
    const char* test_filename = "libetude_storage_test.tmp";
    const size_t test_data_size = 1024 * 1024; // 1MB

    ETFile* file = NULL;
    ETResult result = fs->open_file(test_filename, ET_FILE_MODE_WRITE_CREATE, &file);

    if (result == ET_SUCCESS && file) {
        printf("임시 파일 생성 성공\n");

        // 테스트 데이터 생성
        uint8_t* test_data = malloc(test_data_size);
        INTEGRATION_TEST_ASSERT(test_data != NULL);

        for (size_t i = 0; i < test_data_size; i++) {
            test_data[i] = (uint8_t)(i % 256);
        }

        // 쓰기 성능 측정
        PerformanceMeasurement write_perf;
        start_performance_measurement(&write_perf, "파일 쓰기");

        size_t bytes_written;
        result = fs->write_file(file, test_data, test_data_size, &bytes_written);

        end_performance_measurement(&write_perf);

        INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
        INTEGRATION_TEST_ASSERT_EQUAL(test_data_size, bytes_written);

        double write_speed = (double)test_data_size / (1024 * 1024) / write_perf.elapsed_seconds;
        printf("  쓰기 속도: %.2f MB/s\n", write_speed);

        fs->close_file(file);

        // 읽기 성능 테스트
        result = fs->open_file(test_filename, ET_FILE_MODE_READ, &file);
        INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
        INTEGRATION_TEST_ASSERT(file != NULL);

        uint8_t* read_data = malloc(test_data_size);
        INTEGRATION_TEST_ASSERT(read_data != NULL);

        PerformanceMeasurement read_perf;
        start_performance_measurement(&read_perf, "파일 읽기");

        size_t bytes_read;
        result = fs->read_file(file, read_data, test_data_size, &bytes_read);

        end_performance_measurement(&read_perf);

        INTEGRATION_TEST_ASSERT_EQUAL(ET_SUCCESS, result);
        INTEGRATION_TEST_ASSERT_EQUAL(test_data_size, bytes_read);

        double read_speed = (double)test_data_size / (1024 * 1024) / read_perf.elapsed_seconds;
        printf("  읽기 속도: %.2f MB/s\n", read_speed);

        // 데이터 무결성 확인
        INTEGRATION_TEST_ASSERT(memcmp(test_data, read_data, test_data_size) == 0);
        printf("  데이터 무결성: 확인됨\n");

        fs->close_file(file);

        // 정리
        free(test_data);
        free(read_data);

        // 임시 파일 삭제 (파일 존재 확인 후)
        if (fs->file_exists(test_filename)) {
            // 파일 삭제는 구현되지 않았을 수 있으므로 경고만 출력
            printf("  임시 파일 삭제 필요: %s\n", test_filename);
        }
    } else {
        printf("임시 파일 생성 실패 (읽기 전용 파일시스템일 수 있음)\n");
    }

    printf("실제 스토리지 성능 테스트 완료\n");
    return ET_SUCCESS;
}

/**
 * @brief 실제 네트워크 인터페이스 테스트
 * @return 테스트 결과
 */
ETResult test_real_hardware_network_interfaces(void) {
    printf("실제 네트워크 인터페이스 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    INTEGRATION_TEST_ASSERT(platform != NULL);
    INTEGRATION_TEST_ASSERT(platform->network != NULL);

    ETNetworkInterface* network = platform->network;

    // TCP 소켓 생성 테스트
    ETSocket* tcp_socket = NULL;
    ETResult result = network->create_socket(ET_SOCKET_TYPE_TCP, &tcp_socket);

    if (result == ET_SUCCESS && tcp_socket) {
        printf("TCP 소켓 생성 성공\n");

        // 소켓 옵션 테스트
        int reuse_addr = 1;
        result = network->set_socket_option(tcp_socket, ET_SOCKET_OPTION_REUSE_ADDR,
                                          &reuse_addr, sizeof(reuse_addr));
        if (result == ET_SUCCESS) {
            printf("  SO_REUSEADDR 설정 성공\n");
        }

        network->close_socket(tcp_socket);
    } else {
        printf("TCP 소켓 생성 실패 또는 네트워크 지원 안됨\n");
    }

    // UDP 소켓 생성 테스트
    ETSocket* udp_socket = NULL;
    result = network->create_socket(ET_SOCKET_TYPE_UDP, &udp_socket);

    if (result == ET_SUCCESS && udp_socket) {
        printf("UDP 소켓 생성 성공\n");
        network->close_socket(udp_socket);
    } else {
        printf("UDP 소켓 생성 실패 또는 네트워크 지원 안됨\n");
    }

    // 로컬 루프백 연결 테스트 (간단한 테스트)
    ETSocket* server_socket = NULL;
    result = network->create_socket(ET_SOCKET_TYPE_TCP, &server_socket);

    if (result == ET_SUCCESS && server_socket) {
        // 로컬 주소에 바인드 시도
        ETSocketAddress local_addr = {0};
        // 주소 설정은 플랫폼별로 다를 수 있으므로 간단히 테스트

        result = network->bind_socket(server_socket, &local_addr);
        if (result == ET_SUCCESS) {
            printf("로컬 바인드 성공\n");
        } else {
            printf("로컬 바인드 실패 (허용)\n");
        }

        network->close_socket(server_socket);
    }

    printf("실제 네트워크 인터페이스 테스트 완료\n");
    return ET_SUCCESS;
}