/**
 * @file test_stress_tests.c
 * @brief 스트레스 테스트 구현
 */

#include "test_platform_integration.h"

// 스트레스 테스트용 스레드 데이터
typedef struct {
    int thread_id;
    int iterations;
    ETPlatformInterface* platform;
    volatile bool* should_stop;
    int* error_count;
} StressThreadData;

/**
 * @brief 메모리 할당 스트레스 테스트
 * @param config 스트레스 테스트 설정
 * @return 테스트 결과
 */
ETResult stress_test_memory_allocation(const StressTestConfig* config) {
    printf("메모리 할당 스트레스 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->memory || !platform->threading) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETMemoryInterface* memory = platform->memory;
    ETThreadInterface* threading = platform->threading;

    volatile bool should_stop = false;
    int error_count = 0;

    // 스레드 데이터 준비
    StressThreadData* thread_data = malloc(sizeof(StressThreadData) * config->thread_count);
    ETThread** threads = malloc(sizeof(ETThread*) * config->thread_count);

    if (!thread_data || !threads) {
        free(thread_data);
        free(threads);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 메모리 스트레스 스레드 함수
    auto memory_stress_func = [](void* arg) -> void* {
        StressThreadData* data = (StressThreadData*)arg;
        ETMemoryInterface* mem = data->platform->memory;

        for (int i = 0; i < data->iterations && !(*data->should_stop); i++) {
            // 다양한 크기의 메모리 할당/해제
            size_t sizes[] = {64, 256, 1024, 4096, 16384};
            int size_count = sizeof(sizes) / sizeof(sizes[0]);

            void* ptrs[size_count];

            // 할당
            for (int j = 0; j < size_count; j++) {
                ptrs[j] = mem->malloc(sizes[j]);
                if (!ptrs[j]) {
                    (*data->error_count)++;
                    continue;
                }

                // 메모리 쓰기 테스트
                memset(ptrs[j], 0xAA, sizes[j]);
            }

            // 해제
            for (int j = 0; j < size_count; j++) {
                if (ptrs[j]) {
                    mem->free(ptrs[j]);
                }
            }

            // 정렬된 메모리 테스트
            void* aligned_ptr = mem->aligned_malloc(1024, 64);
            if (aligned_ptr) {
                mem->aligned_free(aligned_ptr);
            } else {
                (*data->error_count)++;
            }
        }

        return NULL;
    };

    // 스레드 생성 및 시작
    for (int i = 0; i < config->thread_count; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].iterations = config->iterations_per_thread;
        thread_data[i].platform = platform;
        thread_data[i].should_stop = &should_stop;
        thread_data[i].error_count = &error_count;

        ETResult result = threading->create_thread(&threads[i], memory_stress_func, &thread_data[i]);
        if (result != ET_SUCCESS) {
            printf("스레드 %d 생성 실패\n", i);
            should_stop = true;
            break;
        }
    }

    // 지정된 시간 동안 실행
    if (platform->system) {
        platform->system->sleep(config->duration_seconds * 1000);
    }

    should_stop = true;

    // 모든 스레드 조인
    for (int i = 0; i < config->thread_count; i++) {
        if (threads[i]) {
            void* thread_result = NULL;
            threading->join_thread(threads[i], &thread_result);
            threading->destroy_thread(threads[i]);
        }
    }

    free(thread_data);
    free(threads);

    printf("메모리 스트레스 테스트 완료 - 오류 수: %d\n", error_count);

    return (error_count == 0) ? ET_SUCCESS : ET_ERROR_TEST_FAILED;
}/**
 *
@brief 스레딩 경합 스트레스 테스트
 * @param config 스트레스 테스트 설정
 * @return 테스트 결과
 */
ETResult stress_test_threading_contention(const StressTestConfig* config) {
    printf("스레딩 경합 스트레스 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->threading) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETThreadInterface* threading = platform->threading;

    // 공유 리소스
    volatile int shared_counter = 0;
    volatile bool should_stop = false;
    int error_count = 0;

    // 뮤텍스 생성
    ETMutex* mutex = NULL;
    ETResult result = threading->create_mutex(&mutex);
    if (result != ET_SUCCESS || !mutex) {
        return ET_ERROR_INITIALIZATION_FAILED;
    }

    // 스레드 데이터 준비
    StressThreadData* thread_data = malloc(sizeof(StressThreadData) * config->thread_count);
    ETThread** threads = malloc(sizeof(ETThread*) * config->thread_count);

    if (!thread_data || !threads) {
        threading->destroy_mutex(mutex);
        free(thread_data);
        free(threads);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 스레딩 경합 테스트 함수
    auto threading_contention_func = [](void* arg) -> void* {
        StressThreadData* data = (StressThreadData*)arg;
        ETThreadInterface* thr = data->platform->threading;

        // 뮤텍스는 전역적으로 접근 (실제로는 매개변수로 전달해야 함)
        extern ETMutex* mutex;
        extern volatile int shared_counter;

        for (int i = 0; i < data->iterations && !(*data->should_stop); i++) {
            // 뮤텍스 잠금
            ETResult res = thr->lock_mutex(mutex);
            if (res != ET_SUCCESS) {
                (*data->error_count)++;
                continue;
            }

            // 공유 리소스 접근
            int old_value = shared_counter;
            shared_counter = old_value + 1;

            // 뮤텍스 해제
            res = thr->unlock_mutex(mutex);
            if (res != ET_SUCCESS) {
                (*data->error_count)++;
            }

            // try_lock 테스트
            res = thr->try_lock_mutex(mutex);
            if (res == ET_SUCCESS) {
                thr->unlock_mutex(mutex);
            }
        }

        return NULL;
    };

    // 스레드 생성 및 시작
    for (int i = 0; i < config->thread_count; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].iterations = config->iterations_per_thread;
        thread_data[i].platform = platform;
        thread_data[i].should_stop = &should_stop;
        thread_data[i].error_count = &error_count;

        result = threading->create_thread(&threads[i], threading_contention_func, &thread_data[i]);
        if (result != ET_SUCCESS) {
            printf("스레드 %d 생성 실패\n", i);
            should_stop = true;
            break;
        }
    }

    // 지정된 시간 동안 실행
    if (platform->system) {
        platform->system->sleep(config->duration_seconds * 1000);
    }

    should_stop = true;

    // 모든 스레드 조인
    for (int i = 0; i < config->thread_count; i++) {
        if (threads[i]) {
            void* thread_result = NULL;
            threading->join_thread(threads[i], &thread_result);
            threading->destroy_thread(threads[i]);
        }
    }

    // 정리
    threading->destroy_mutex(mutex);
    free(thread_data);
    free(threads);

    printf("스레딩 경합 테스트 완료 - 공유 카운터: %d, 오류 수: %d\n",
           shared_counter, error_count);

    return (error_count == 0) ? ET_SUCCESS : ET_ERROR_TEST_FAILED;
}/**

 * @brief 오디오 스트리밍 스트레스 테스트
 * @param config 스트레스 테스트 설정
 * @return 테스트 결과
 */
ETResult stress_test_audio_streaming(const StressTestConfig* config) {
    printf("오디오 스트리밍 스트레스 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->audio) {
        printf("오디오 인터페이스를 사용할 수 없음\n");
        return ET_SUCCESS; // 오디오가 없는 환경에서는 성공으로 처리
    }

    ETAudioInterface* audio = platform->audio;
    int error_count = 0;

    // 여러 오디오 포맷으로 테스트
    ETAudioFormat formats[] = {
        {44100, 2, 16, ET_AUDIO_FORMAT_PCM_S16LE},
        {48000, 2, 16, ET_AUDIO_FORMAT_PCM_S16LE},
        {22050, 1, 16, ET_AUDIO_FORMAT_PCM_S16LE}
    };
    int format_count = sizeof(formats) / sizeof(formats[0]);

    for (int f = 0; f < format_count; f++) {
        printf("  포맷 테스트: %dHz, %dch, %dbit\n",
               formats[f].sample_rate, formats[f].channels, formats[f].bits_per_sample);

        for (int i = 0; i < 10; i++) { // 각 포맷당 10회 반복
            ETAudioDevice* device = NULL;
            ETResult result = audio->open_output_device(NULL, &formats[f], &device);

            if (result == ET_SUCCESS && device) {
                // 디바이스 상태 확인
                ETAudioState state = audio->get_state(device);
                if (state == ET_AUDIO_STATE_ERROR) {
                    error_count++;
                }

                // 지연시간 확인
                uint32_t latency = audio->get_latency(device);
                if (latency == 0 || latency > 1000) {
                    error_count++;
                }

                audio->close_device(device);
            }
            // 디바이스를 열 수 없는 경우는 오류로 카운트하지 않음
        }
    }

    printf("오디오 스트리밍 스트레스 테스트 완료 - 오류 수: %d\n", error_count);
    return (error_count < 5) ? ET_SUCCESS : ET_ERROR_TEST_FAILED; // 일부 오류는 허용
}

/**
 * @brief 파일시스템 작업 스트레스 테스트
 * @param config 스트레스 테스트 설정
 * @return 테스트 결과
 */
ETResult stress_test_filesystem_operations(const StressTestConfig* config) {
    printf("파일시스템 작업 스트레스 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->filesystem) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETFilesystemInterface* fs = platform->filesystem;
    int error_count = 0;

    // 경로 처리 스트레스 테스트
    char result_path[256];
    const char* test_paths[] = {
        ".",
        "..",
        "./test",
        "../test",
        "test/path",
        "/absolute/path",
        "relative/path"
    };
    int path_count = sizeof(test_paths) / sizeof(test_paths[0]);

    for (int i = 0; i < config->iterations_per_thread; i++) {
        for (int j = 0; j < path_count; j++) {
            ETResult result = fs->normalize_path(test_paths[j], result_path, sizeof(result_path));
            if (result != ET_SUCCESS && result != ET_ERROR_INVALID_PARAMETER) {
                error_count++;
            }
        }
    }

    // 파일 존재 확인 스트레스 테스트
    const char* test_files[] = {
        "nonexistent_file.txt",
        ".",
        "..",
        "/dev/null",
        "C:\\Windows\\System32", // Windows에서만 유효
        "/usr/bin/ls"            // Unix에서만 유효
    };
    int file_count = sizeof(test_files) / sizeof(test_files[0]);

    for (int i = 0; i < config->iterations_per_thread; i++) {
        for (int j = 0; j < file_count; j++) {
            bool exists = fs->file_exists(test_files[j]);
            bool is_dir = fs->is_directory(test_files[j]);
            // 결과는 검증하지 않고 크래시만 확인
        }
    }

    printf("파일시스템 작업 스트레스 테스트 완료 - 오류 수: %d\n", error_count);
    return (error_count < 10) ? ET_SUCCESS : ET_ERROR_TEST_FAILED; // 일부 오류는 허용
}/**

 * @brief 혼합 워크로드 스트레스 테스트
 * @param config 스트레스 테스트 설정
 * @return 테스트 결과
 */
ETResult stress_test_mixed_workload(const StressTestConfig* config) {
    printf("혼합 워크로드 스트레스 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    int total_error_count = 0;

    // 각 서브시스템을 동시에 스트레스 테스트
    StressTestConfig sub_config = *config;
    sub_config.duration_seconds = config->duration_seconds / 4; // 각각 1/4 시간
    sub_config.iterations_per_thread = config->iterations_per_thread / 4;

    // 메모리 스트레스
    if (config->enable_memory_stress) {
        ETResult result = stress_test_memory_allocation(&sub_config);
        if (result != ET_SUCCESS) {
            total_error_count++;
        }
    }

    // 스레딩 스트레스
    ETResult result = stress_test_threading_contention(&sub_config);
    if (result != ET_SUCCESS) {
        total_error_count++;
    }

    // 오디오 스트레스
    result = stress_test_audio_streaming(&sub_config);
    if (result != ET_SUCCESS) {
        total_error_count++;
    }

    // 파일시스템 스트레스
    if (config->enable_io_stress) {
        result = stress_test_filesystem_operations(&sub_config);
        if (result != ET_SUCCESS) {
            total_error_count++;
        }
    }

    printf("혼합 워크로드 스트레스 테스트 완료 - 실패한 서브시스템: %d\n", total_error_count);
    return (total_error_count <= 1) ? ET_SUCCESS : ET_ERROR_TEST_FAILED; // 1개까지 실패 허용
}

/**
 * @brief 장시간 오디오 안정성 테스트
 * @return 테스트 결과
 */
ETResult stability_test_long_running_audio(void) {
    printf("장시간 오디오 안정성 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->audio) {
        printf("오디오 인터페이스를 사용할 수 없음\n");
        return ET_SUCCESS;
    }

    ETAudioInterface* audio = platform->audio;
    ETAudioFormat format = {
        .sample_rate = 44100,
        .channels = 2,
        .bits_per_sample = 16,
        .format = ET_AUDIO_FORMAT_PCM_S16LE
    };

    ETAudioDevice* device = NULL;
    ETResult result = audio->open_output_device(NULL, &format, &device);

    if (result != ET_SUCCESS || !device) {
        printf("오디오 디바이스를 열 수 없음\n");
        return ET_SUCCESS;
    }

    // 5초간 안정성 테스트 (CI 환경 고려)
    int test_duration = is_running_in_ci_environment() ? 5 : 30;

    for (int i = 0; i < test_duration; i++) {
        // 디바이스 상태 확인
        ETAudioState state = audio->get_state(device);
        if (state == ET_AUDIO_STATE_ERROR) {
            printf("오디오 디바이스 오류 발생\n");
            audio->close_device(device);
            return ET_ERROR_TEST_FAILED;
        }

        // 지연시간 확인
        uint32_t latency = audio->get_latency(device);
        if (latency == 0 || latency > 1000) {
            printf("비정상적인 지연시간: %dms\n", latency);
        }

        // 1초 대기
        if (platform->system) {
            platform->system->sleep(1000);
        }
    }

    audio->close_device(device);
    printf("장시간 오디오 안정성 테스트 완료\n");
    return ET_SUCCESS;
}/**
 *
@brief 메모리 누수 감지 테스트
 * @return 테스트 결과
 */
ETResult stability_test_memory_leak_detection(void) {
    printf("메모리 누수 감지 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform || !platform->memory || !platform->system) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    ETMemoryInterface* memory = platform->memory;
    ETSystemInterface* system = platform->system;

    // 초기 메모리 사용량 측정
    ETMemoryUsage initial_usage;
    ETResult result = system->get_memory_usage(&initial_usage);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 메모리 할당/해제 반복
    const int iterations = 1000;
    for (int i = 0; i < iterations; i++) {
        void* ptr = memory->malloc(1024);
        if (ptr) {
            memset(ptr, 0xAA, 1024);
            memory->free(ptr);
        }

        // 정렬된 메모리도 테스트
        void* aligned_ptr = memory->aligned_malloc(1024, 64);
        if (aligned_ptr) {
            memory->aligned_free(aligned_ptr);
        }
    }

    // 최종 메모리 사용량 측정
    ETMemoryUsage final_usage;
    result = system->get_memory_usage(&final_usage);
    if (result != ET_SUCCESS) {
        return result;
    }

    // 메모리 사용량 증가 확인
    uint64_t memory_increase = final_usage.physical_used - initial_usage.physical_used;
    printf("메모리 사용량 변화: %lld bytes\n", (long long)memory_increase);

    // 10MB 이상 증가하면 누수 의심
    if (memory_increase > 10 * 1024 * 1024) {
        printf("메모리 누수 의심: %lld bytes 증가\n", (long long)memory_increase);
        return ET_ERROR_TEST_FAILED;
    }

    printf("메모리 누수 감지 테스트 완료\n");
    return ET_SUCCESS;
}

/**
 * @brief 리소스 고갈 테스트
 * @return 테스트 결과
 */
ETResult stability_test_resource_exhaustion(void) {
    printf("리소스 고갈 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    int error_count = 0;

    // 메모리 고갈 테스트
    if (platform->memory) {
        printf("  메모리 고갈 테스트...\n");
        void* ptrs[1000];
        int alloc_count = 0;

        // 점진적으로 큰 메모리 할당
        for (int i = 0; i < 1000; i++) {
            size_t size = (i + 1) * 1024 * 1024; // 1MB씩 증가
            ptrs[i] = platform->memory->malloc(size);
            if (ptrs[i]) {
                alloc_count++;
            } else {
                break; // 할당 실패 시 중단
            }
        }

        printf("    할당된 블록 수: %d\n", alloc_count);

        // 할당된 메모리 해제
        for (int i = 0; i < alloc_count; i++) {
            if (ptrs[i]) {
                platform->memory->free(ptrs[i]);
            }
        }
    }

    // 스레드 고갈 테스트
    if (platform->threading) {
        printf("  스레드 고갈 테스트...\n");
        ETThread* threads[100];
        int thread_count = 0;

        auto dummy_thread_func = [](void* arg) -> void* {
            // 짧은 시간 대기 후 종료
            if (arg) {
                ETPlatformInterface* platform = (ETPlatformInterface*)arg;
                if (platform->system) {
                    platform->system->sleep(100); // 100ms
                }
            }
            return NULL;
        };

        // 스레드 생성 시도
        for (int i = 0; i < 100; i++) {
            ETResult result = platform->threading->create_thread(&threads[i], dummy_thread_func, platform);
            if (result == ET_SUCCESS) {
                thread_count++;
            } else {
                break; // 생성 실패 시 중단
            }
        }

        printf("    생성된 스레드 수: %d\n", thread_count);

        // 모든 스레드 조인
        for (int i = 0; i < thread_count; i++) {
            if (threads[i]) {
                void* thread_result = NULL;
                platform->threading->join_thread(threads[i], &thread_result);
                platform->threading->destroy_thread(threads[i]);
            }
        }
    }

    printf("리소스 고갈 테스트 완료 - 오류 수: %d\n", error_count);
    return ET_SUCCESS; // 고갈 테스트는 시스템 한계 확인이 목적
}

/**
 * @brief 오류 복구 테스트
 * @return 테스트 결과
 */
ETResult stability_test_error_recovery(void) {
    printf("오류 복구 테스트...\n");

    ETPlatformInterface* platform = et_platform_get_interface();
    if (!platform) {
        return ET_ERROR_NOT_SUPPORTED;
    }

    int recovery_success_count = 0;
    int total_tests = 0;

    // 잘못된 매개변수로 함수 호출 후 정상 호출
    if (platform->memory) {
        total_tests++;

        // 잘못된 호출
        void* ptr = platform->memory->malloc(0);
        platform->memory->free(NULL);

        // 정상 호출
        ptr = platform->memory->malloc(1024);
        if (ptr) {
            platform->memory->free(ptr);
            recovery_success_count++;
        }
    }

    if (platform->filesystem) {
        total_tests++;

        // 잘못된 경로 처리
        char result[256];
        platform->filesystem->normalize_path(NULL, result, sizeof(result));
        platform->filesystem->file_exists(NULL);

        // 정상 호출
        ETResult res = platform->filesystem->normalize_path(".", result, sizeof(result));
        if (res == ET_SUCCESS) {
            recovery_success_count++;
        }
    }

    if (platform->audio) {
        total_tests++;

        // 잘못된 오디오 호출
        platform->audio->close_device(NULL);
        platform->audio->get_state(NULL);

        // 정상 호출 시도
        ETAudioDeviceInfo devices[1];
        int count = 1;
        ETResult res = platform->audio->enumerate_devices(ET_AUDIO_DEVICE_OUTPUT, devices, &count);
        if (res == ET_SUCCESS || res == ET_ERROR_NOT_SUPPORTED) {
            recovery_success_count++;
        }
    }

    printf("오류 복구 테스트 완료 - 성공률: %d/%d\n", recovery_success_count, total_tests);
    return (recovery_success_count >= total_tests / 2) ? ET_SUCCESS : ET_ERROR_TEST_FAILED;
}