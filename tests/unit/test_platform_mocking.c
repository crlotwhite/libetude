/**
 * @file test_platform_mocking.c
 * @brief 플랫폼 추상화 레이어 모킹 프레임워크 구현
 */

#include "test_platform_mocking.h"

// 전역 모킹 컨텍스트
MockContext g_mock_context = {0};

/**
 * @brief 모킹 프레임워크 초기화
 */
void mock_init(void) {
    memset(&g_mock_context, 0, sizeof(MockContext));
    g_mock_context.strict_mode = false;
    g_mock_context.recording_mode = false;
}

/**
 * @brief 모킹 프레임워크 정리
 */
void mock_cleanup(void) {
    mock_reset();
}

/**
 * @brief 모킹 상태 초기화
 */
void mock_reset(void) {
    // 호출 기록 정리
    MockCall* call = g_mock_context.call_history;
    while (call) {
        MockCall* next = call->next;
        if (call->parameters) {
            free(call->parameters);
        }
        free(call);
        call = next;
    }

    // 기대값 정리
    MockExpectation* expectation = g_mock_context.expectations;
    while (expectation) {
        MockExpectation* next = expectation->next;
        if (expectation->expected_parameters) {
            free(expectation->expected_parameters);
        }
        free(expectation);
        expectation = next;
    }

    g_mock_context.call_history = NULL;
    g_mock_context.expectations = NULL;
    g_mock_context.total_calls = 0;
    g_mock_context.failed_expectations = 0;
}

/**
 * @brief 엄격 모드 설정
 * @param strict 엄격 모드 활성화 여부
 */
void mock_set_strict_mode(bool strict) {
    g_mock_context.strict_mode = strict;
}

/**
 * @brief 기록 모드 설정
 * @param recording 기록 모드 활성화 여부
 */
void mock_set_recording_mode(bool recording) {
    g_mock_context.recording_mode = recording;
}

/**
 * @brief 함수 호출 기대값 설정
 * @param function_name 함수명
 * @param parameters 매개변수
 * @param param_size 매개변수 크기
 * @param return_value 반환값
 * @param max_calls 최대 호출 횟수
 */
void mock_expect_call(const char* function_name, const void* parameters, size_t param_size,
                     ETResult return_value, int max_calls) {
    mock_expect_call_with_matcher(function_name, parameters, param_size, return_value, max_calls, mock_match_exact);
}

/**
 * @brief 매처를 사용한 함수 호출 기대값 설정
 * @param function_name 함수명
 * @param parameters 매개변수
 * @param param_size 매개변수 크기
 * @param return_value 반환값
 * @param max_calls 최대 호출 횟수
 * @param matcher 매개변수 매처 함수
 */
void mock_expect_call_with_matcher(const char* function_name, const void* parameters, size_t param_size,
                                  ETResult return_value, int max_calls,
                                  bool (*matcher)(const void*, const void*, size_t)) {
    MockExpectation* expectation = malloc(sizeof(MockExpectation));
    if (!expectation) return;

    expectation->function_name = function_name;
    expectation->return_value = return_value;
    expectation->call_count = 0;
    expectation->max_calls = max_calls;
    expectation->parameter_matcher = matcher;
    expectation->next = g_mock_context.expectations;

    if (parameters && param_size > 0) {
        expectation->expected_parameters = malloc(param_size);
        if (expectation->expected_parameters) {
            memcpy(expectation->expected_parameters, parameters, param_size);
            expectation->parameter_size = param_size;
        } else {
            expectation->parameter_size = 0;
        }
    } else {
        expectation->expected_parameters = NULL;
        expectation->parameter_size = 0;
    }

    g_mock_context.expectations = expectation;
}

/**
 * @brief 함수 호출 기록
 * @param function_name 함수명
 * @param parameters 매개변수
 * @param param_size 매개변수 크기
 * @param return_value 반환값
 */
void mock_record_call(const char* function_name, const void* parameters, size_t param_size,
                     ETResult return_value) {
    MockCall* call = malloc(sizeof(MockCall));
    if (!call) return;

    call->function_name = function_name;
    call->return_value = return_value;
    call->timestamp = (uint64_t)time(NULL);
    call->next = g_mock_context.call_history;

    if (parameters && param_size > 0) {
        call->parameters = malloc(param_size);
        if (call->parameters) {
            memcpy(call->parameters, parameters, param_size);
            call->parameter_size = param_size;
        } else {
            call->parameter_size = 0;
        }
    } else {
        call->parameters = NULL;
        call->parameter_size = 0;
    }

    g_mock_context.call_history = call;
    g_mock_context.total_calls++;
}/**

 * @brief 모든 기대값 검증
 * @return 검증 성공 여부
 */
bool mock_verify_all_expectations(void) {
    bool all_verified = true;
    MockExpectation* expectation = g_mock_context.expectations;

    while (expectation) {
        if (expectation->call_count < expectation->max_calls && expectation->max_calls > 0) {
            printf("기대값 실패: %s - 예상 호출 수: %d, 실제 호출 수: %d\n",
                   expectation->function_name, expectation->max_calls, expectation->call_count);
            all_verified = false;
        }
        expectation = expectation->next;
    }

    return all_verified;
}

/**
 * @brief 특정 함수의 호출 횟수 검증
 * @param function_name 함수명
 * @param expected_count 예상 호출 횟수
 * @return 검증 성공 여부
 */
bool mock_verify_call_count(const char* function_name, int expected_count) {
    int actual_count = mock_get_call_count(function_name);
    return actual_count == expected_count;
}

/**
 * @brief 함수 호출 순서 검증
 * @param function_names 함수명 배열
 * @param count 함수 개수
 * @return 검증 성공 여부
 */
bool mock_verify_call_order(const char** function_names, int count) {
    MockCall* call = g_mock_context.call_history;
    int index = count - 1; // 역순으로 저장되므로

    while (call && index >= 0) {
        if (strcmp(call->function_name, function_names[index]) != 0) {
            return false;
        }
        call = call->next;
        index--;
    }

    return index < 0;
}

/**
 * @brief 예상하지 않은 호출이 없는지 검증
 * @return 검증 성공 여부
 */
bool mock_verify_no_unexpected_calls(void) {
    if (!g_mock_context.strict_mode) {
        return true; // 엄격 모드가 아니면 항상 성공
    }

    return g_mock_context.failed_expectations == 0;
}

/**
 * @brief 특정 함수의 호출 횟수 조회
 * @param function_name 함수명
 * @return 호출 횟수
 */
int mock_get_call_count(const char* function_name) {
    int count = 0;
    MockCall* call = g_mock_context.call_history;

    while (call) {
        if (strcmp(call->function_name, function_name) == 0) {
            count++;
        }
        call = call->next;
    }

    return count;
}

/**
 * @brief 특정 함수의 호출 기록 조회
 * @param function_name 함수명
 * @return 호출 기록 (첫 번째)
 */
MockCall* mock_get_call_history(const char* function_name) {
    MockCall* call = g_mock_context.call_history;

    while (call) {
        if (strcmp(call->function_name, function_name) == 0) {
            return call;
        }
        call = call->next;
    }

    return NULL;
}

/**
 * @brief 호출 기록 출력
 */
void mock_print_call_history(void) {
    printf("=== 모킹 호출 기록 ===\n");
    printf("총 호출 수: %d\n", g_mock_context.total_calls);

    MockCall* call = g_mock_context.call_history;
    int index = 0;

    while (call) {
        printf("%d. %s (반환값: %d, 시간: %llu)\n",
               ++index, call->function_name, call->return_value,
               (unsigned long long)call->timestamp);
        call = call->next;
    }
}

// 매개변수 매처 함수들
bool mock_match_exact(const void* expected, const void* actual, size_t size) {
    if (!expected && !actual) return true;
    if (!expected || !actual) return false;
    return memcmp(expected, actual, size) == 0;
}

bool mock_match_ignore(const void* expected, const void* actual, size_t size) {
    return true; // 항상 매치
}

bool mock_match_string(const void* expected, const void* actual, size_t size) {
    const char* exp_str = (const char*)expected;
    const char* act_str = (const char*)actual;

    if (!exp_str && !act_str) return true;
    if (!exp_str || !act_str) return false;

    return strcmp(exp_str, act_str) == 0;
}

bool mock_match_pointer_not_null(const void* expected, const void* actual, size_t size) {
    return actual != NULL;
}// 모킹된 오
디오 인터페이스 구현

static ETResult mock_audio_open_output_device(const char* device_name, const ETAudioFormat* format, ETAudioDevice** device) {
    MockAudioInterface* mock = (MockAudioInterface*)((char*)device - offsetof(MockAudioInterface, base));
    mock->open_output_device_calls++;

    mock_record_call("open_output_device", format, sizeof(ETAudioFormat), mock->open_output_device_result);

    if (mock->open_output_device_result == ET_SUCCESS && device) {
        *device = (ETAudioDevice*)0x12345678; // 더미 포인터
    }

    return mock->open_output_device_result;
}

static ETResult mock_audio_open_input_device(const char* device_name, const ETAudioFormat* format, ETAudioDevice** device) {
    MockAudioInterface* mock = (MockAudioInterface*)((char*)device - offsetof(MockAudioInterface, base));
    mock->open_input_device_calls++;

    mock_record_call("open_input_device", format, sizeof(ETAudioFormat), mock->open_input_device_result);

    if (mock->open_input_device_result == ET_SUCCESS && device) {
        *device = (ETAudioDevice*)0x12345679; // 더미 포인터
    }

    return mock->open_input_device_result;
}

static void mock_audio_close_device(ETAudioDevice* device) {
    // 실제 구현에서는 device로부터 mock을 찾아야 하지만, 여기서는 전역 접근
    mock_record_call("close_device", &device, sizeof(device), ET_SUCCESS);
}

static ETResult mock_audio_start_stream(ETAudioDevice* device) {
    mock_record_call("start_stream", &device, sizeof(device), ET_SUCCESS);
    return ET_SUCCESS;
}

static ETResult mock_audio_stop_stream(ETAudioDevice* device) {
    mock_record_call("stop_stream", &device, sizeof(device), ET_SUCCESS);
    return ET_SUCCESS;
}

static ETResult mock_audio_pause_stream(ETAudioDevice* device) {
    mock_record_call("pause_stream", &device, sizeof(device), ET_SUCCESS);
    return ET_SUCCESS;
}

static ETResult mock_audio_set_callback(ETAudioDevice* device, ETAudioCallback callback, void* user_data) {
    mock_record_call("set_callback", &device, sizeof(device), ET_SUCCESS);
    return ET_SUCCESS;
}

static ETResult mock_audio_enumerate_devices(ETAudioDeviceType type, ETAudioDeviceInfo* devices, int* count) {
    mock_record_call("enumerate_devices", &type, sizeof(type), ET_SUCCESS);

    if (devices && count && *count > 0) {
        int copy_count = (*count < 2) ? *count : 2; // 최대 2개 반환
        for (int i = 0; i < copy_count; i++) {
            generate_test_audio_device_info(&devices[i], i);
        }
        *count = copy_count;
    }

    return ET_SUCCESS;
}

static uint32_t mock_audio_get_latency(const ETAudioDevice* device) {
    mock_record_call("get_latency", &device, sizeof(device), ET_SUCCESS);
    return 64; // 64ms 지연시간
}

static ETAudioState mock_audio_get_state(const ETAudioDevice* device) {
    mock_record_call("get_state", &device, sizeof(device), ET_SUCCESS);
    return ET_AUDIO_STATE_READY;
}

/**
 * @brief 모킹된 오디오 인터페이스 생성
 * @return 모킹된 오디오 인터페이스
 */
MockAudioInterface* mock_audio_interface_create(void) {
    MockAudioInterface* mock = malloc(sizeof(MockAudioInterface));
    if (!mock) return NULL;

    memset(mock, 0, sizeof(MockAudioInterface));

    // 인터페이스 함수 설정
    mock->base.open_output_device = mock_audio_open_output_device;
    mock->base.open_input_device = mock_audio_open_input_device;
    mock->base.close_device = mock_audio_close_device;
    mock->base.start_stream = mock_audio_start_stream;
    mock->base.stop_stream = mock_audio_stop_stream;
    mock->base.pause_stream = mock_audio_pause_stream;
    mock->base.set_callback = mock_audio_set_callback;
    mock->base.enumerate_devices = mock_audio_enumerate_devices;
    mock->base.get_latency = mock_audio_get_latency;
    mock->base.get_state = mock_audio_get_state;

    // 기본 반환값 설정
    mock->open_output_device_result = ET_SUCCESS;
    mock->open_input_device_result = ET_SUCCESS;
    mock->start_stream_result = ET_SUCCESS;
    mock->stop_stream_result = ET_SUCCESS;
    mock->pause_stream_result = ET_SUCCESS;
    mock->set_callback_result = ET_SUCCESS;
    mock->enumerate_devices_result = ET_SUCCESS;

    mock->mock_latency = 64;
    mock->mock_state = ET_AUDIO_STATE_READY;
    mock->context = &g_mock_context;

    return mock;
}

/**
 * @brief 모킹된 오디오 인터페이스 파괴
 * @param mock 모킹된 오디오 인터페이스
 */
void mock_audio_interface_destroy(MockAudioInterface* mock) {
    if (mock) {
        if (mock->mock_devices) {
            free(mock->mock_devices);
        }
        free(mock);
    }
}

/**
 * @brief 모킹된 오디오 인터페이스 초기화
 * @param mock 모킹된 오디오 인터페이스
 */
void mock_audio_interface_reset(MockAudioInterface* mock) {
    if (!mock) return;

    // 호출 카운터 초기화
    mock->open_output_device_calls = 0;
    mock->open_input_device_calls = 0;
    mock->close_device_calls = 0;
    mock->start_stream_calls = 0;
    mock->stop_stream_calls = 0;
    mock->pause_stream_calls = 0;
    mock->set_callback_calls = 0;
    mock->enumerate_devices_calls = 0;
    mock->get_latency_calls = 0;
    mock->get_state_calls = 0;
}// 테스트 데이
터 생성 유틸리티

/**
 * @brief 테스트용 오디오 디바이스 정보 생성
 * @param info 오디오 디바이스 정보 구조체
 * @param index 디바이스 인덱스
 */
void generate_test_audio_device_info(ETAudioDeviceInfo* info, int index) {
    if (!info) return;

    snprintf(info->name, sizeof(info->name), "Test Audio Device %d", index);
    snprintf(info->id, sizeof(info->id), "test_device_%d", index);
    info->type = (index % 2 == 0) ? ET_AUDIO_DEVICE_OUTPUT : ET_AUDIO_DEVICE_INPUT;
    info->max_channels = (index % 2 == 0) ? 2 : 1;
    info->is_default = (index == 0);

    // 지원 샘플 레이트 설정
    static uint32_t sample_rates[] = {22050, 44100, 48000};
    info->supported_rates = sample_rates;
    info->rate_count = 3;
}

/**
 * @brief 테스트용 시스템 정보 생성
 * @param info 시스템 정보 구조체
 */
void generate_test_system_info(ETSystemInfo* info) {
    if (!info) return;

    info->total_memory = 8ULL * 1024 * 1024 * 1024; // 8GB
    info->available_memory = 4ULL * 1024 * 1024 * 1024; // 4GB
    info->cpu_count = 4;
    info->cpu_frequency = 2400; // 2.4GHz
    strcpy(info->cpu_name, "Test CPU");
    strcpy(info->system_name, "Test System");
}

/**
 * @brief 테스트용 메모리 정보 생성
 * @param info 메모리 정보 구조체
 */
void generate_test_memory_info(ETMemoryInfo* info) {
    if (!info) return;

    info->total_physical = 8ULL * 1024 * 1024 * 1024; // 8GB
    info->available_physical = 4ULL * 1024 * 1024 * 1024; // 4GB
    info->total_virtual = 16ULL * 1024 * 1024 * 1024; // 16GB
    info->available_virtual = 12ULL * 1024 * 1024 * 1024; // 12GB
}

/**
 * @brief 테스트용 CPU 정보 생성
 * @param info CPU 정보 구조체
 */
void generate_test_cpu_info(ETCPUInfo* info) {
    if (!info) return;

    info->core_count = 4;
    info->thread_count = 8;
    info->base_frequency = 2400;
    info->max_frequency = 3200;
    strcpy(info->vendor, "TestVendor");
    strcpy(info->brand, "Test CPU Brand");
    info->cache_line_size = 64;
    info->l1_cache_size = 32 * 1024; // 32KB
    info->l2_cache_size = 256 * 1024; // 256KB
    info->l3_cache_size = 8 * 1024 * 1024; // 8MB
}

// 테스트 검증 유틸리티

/**
 * @brief 오디오 포맷 동등성 검증
 * @param expected 예상 포맷
 * @param actual 실제 포맷
 * @return 동등성 여부
 */
bool verify_audio_format_equal(const ETAudioFormat* expected, const ETAudioFormat* actual) {
    if (!expected && !actual) return true;
    if (!expected || !actual) return false;

    return (expected->sample_rate == actual->sample_rate &&
            expected->channels == actual->channels &&
            expected->bits_per_sample == actual->bits_per_sample &&
            expected->format == actual->format);
}

/**
 * @brief 시스템 정보 유효성 검증
 * @param info 시스템 정보
 * @return 유효성 여부
 */
bool verify_system_info_valid(const ETSystemInfo* info) {
    if (!info) return false;

    return (info->total_memory > 0 &&
            info->available_memory <= info->total_memory &&
            info->cpu_count > 0 &&
            strlen(info->cpu_name) > 0 &&
            strlen(info->system_name) > 0);
}

/**
 * @brief 메모리 정보 유효성 검증
 * @param info 메모리 정보
 * @return 유효성 여부
 */
bool verify_memory_info_valid(const ETMemoryInfo* info) {
    if (!info) return false;

    return (info->total_physical > 0 &&
            info->available_physical <= info->total_physical &&
            info->total_virtual >= info->total_physical);
}

/**
 * @brief CPU 정보 유효성 검증
 * @param info CPU 정보
 * @return 유효성 여부
 */
bool verify_cpu_info_valid(const ETCPUInfo* info) {
    if (!info) return false;

    return (info->core_count > 0 &&
            info->thread_count >= info->core_count &&
            info->base_frequency > 0 &&
            strlen(info->vendor) > 0 &&
            strlen(info->brand) > 0);
}//
자동화된 테스트 실행 및 보고

/**
 * @brief 자동화된 테스트 스위트 실행
 * @param tests 테스트 배열
 * @param test_count 테스트 개수
 * @param report 테스트 보고서
 * @return 실행 결과
 */
ETResult run_automated_test_suite(AutomatedTest* tests, int test_count, TestReport* report) {
    if (!tests || !report) {
        return ET_ERROR_INVALID_PARAMETER;
    }

    memset(report, 0, sizeof(TestReport));
    report->total_tests = test_count;

    clock_t start_time = clock();

    printf("=== 자동화된 테스트 스위트 실행 ===\n");
    printf("총 테스트 수: %d\n", test_count);

    for (int i = 0; i < test_count; i++) {
        AutomatedTest* test = &tests[i];

        if (!test->enabled) {
            printf("SKIP: %s - %s\n", test->test_name, test->description);
            report->skipped_tests++;
            continue;
        }

        printf("RUN:  %s - %s\n", test->test_name, test->description);

        // 모킹 상태 초기화
        mock_reset();

        clock_t test_start = clock();
        ETResult result = test->test_function();
        clock_t test_end = clock();

        double test_time = ((double)(test_end - test_start)) / CLOCKS_PER_SEC;

        if (result == ET_SUCCESS) {
            printf("PASS: %s (%.3fs)\n", test->test_name, test_time);
            report->passed_tests++;
        } else {
            printf("FAIL: %s (%.3fs) - Error: %d\n", test->test_name, test_time, result);
            report->failed_tests++;
        }
    }

    clock_t end_time = clock();
    report->total_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    printf("\n=== 테스트 결과 요약 ===\n");
    printf("총 테스트: %d\n", report->total_tests);
    printf("통과: %d\n", report->passed_tests);
    printf("실패: %d\n", report->failed_tests);
    printf("건너뜀: %d\n", report->skipped_tests);
    printf("총 시간: %.3fs\n", report->total_time);

    return (report->failed_tests == 0) ? ET_SUCCESS : ET_ERROR_TEST_FAILED;
}

/**
 * @brief 테스트 보고서 생성
 * @param report 테스트 보고서
 * @param format 보고서 형식 ("xml", "json", "text")
 */
void generate_test_report(const TestReport* report, const char* format) {
    if (!report || !format) return;

    if (strcmp(format, "xml") == 0) {
        printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        printf("<testsuites>\n");
        printf("  <testsuite name=\"Platform Abstraction Tests\" tests=\"%d\" failures=\"%d\" skipped=\"%d\" time=\"%.3f\">\n",
               report->total_tests, report->failed_tests, report->skipped_tests, report->total_time);
        printf("  </testsuite>\n");
        printf("</testsuites>\n");
    } else if (strcmp(format, "json") == 0) {
        printf("{\n");
        printf("  \"total_tests\": %d,\n", report->total_tests);
        printf("  \"passed_tests\": %d,\n", report->passed_tests);
        printf("  \"failed_tests\": %d,\n", report->failed_tests);
        printf("  \"skipped_tests\": %d,\n", report->skipped_tests);
        printf("  \"total_time\": %.3f\n", report->total_time);
        printf("}\n");
    } else {
        printf("테스트 보고서\n");
        printf("=============\n");
        printf("총 테스트: %d\n", report->total_tests);
        printf("통과: %d\n", report->passed_tests);
        printf("실패: %d\n", report->failed_tests);
        printf("건너뜀: %d\n", report->skipped_tests);
        printf("총 시간: %.3fs\n", report->total_time);
    }
}

/**
 * @brief 테스트 보고서 파일 저장
 * @param report 테스트 보고서
 * @param filename 파일명
 */
void save_test_report(const TestReport* report, const char* filename) {
    if (!report || !filename) return;

    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("테스트 보고서 파일을 열 수 없습니다: %s\n", filename);
        return;
    }

    fprintf(file, "테스트 보고서\n");
    fprintf(file, "=============\n");
    fprintf(file, "총 테스트: %d\n", report->total_tests);
    fprintf(file, "통과: %d\n", report->passed_tests);
    fprintf(file, "실패: %d\n", report->failed_tests);
    fprintf(file, "건너뜀: %d\n", report->skipped_tests);
    fprintf(file, "총 시간: %.3fs\n", report->total_time);

    fclose(file);
    printf("테스트 보고서가 저장되었습니다: %s\n", filename);
}

// CI/CD 통합 유틸리티

/**
 * @brief CI 환경 여부 확인
 * @return CI 환경 여부
 */
bool is_ci_environment(void) {
    return (getenv("CI") != NULL ||
            getenv("CONTINUOUS_INTEGRATION") != NULL ||
            getenv("GITHUB_ACTIONS") != NULL ||
            getenv("JENKINS_URL") != NULL ||
            getenv("TRAVIS") != NULL);
}

/**
 * @brief CI 환경 변수 설정
 */
void set_ci_environment_variables(void) {
    if (is_ci_environment()) {
        // CI 환경에서 테스트 동작 조정
        mock_set_strict_mode(true);
        printf("CI 환경 감지됨 - 엄격 모드 활성화\n");
    }
}

/**
 * @brief CI용 테스트 결과 내보내기
 * @param report 테스트 보고서
 */
void export_test_results_for_ci(const TestReport* report) {
    if (!report) return;

    // GitHub Actions용 출력
    if (getenv("GITHUB_ACTIONS")) {
        printf("::set-output name=total_tests::%d\n", report->total_tests);
        printf("::set-output name=passed_tests::%d\n", report->passed_tests);
        printf("::set-output name=failed_tests::%d\n", report->failed_tests);
        printf("::set-output name=test_time::%.3f\n", report->total_time);

        if (report->failed_tests > 0) {
            printf("::error::테스트 실패: %d개 테스트가 실패했습니다\n", report->failed_tests);
        }
    }

    // Jenkins용 출력
    if (getenv("JENKINS_URL")) {
        printf("JENKINS_TEST_RESULTS=total:%d,passed:%d,failed:%d,time:%.3f\n",
               report->total_tests, report->passed_tests, report->failed_tests, report->total_time);
    }

    // 일반적인 CI 환경용 종료 코드 설정
    if (report->failed_tests > 0) {
        exit(1);
    }
}