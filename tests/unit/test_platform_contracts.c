/**
 * @file test_platform_contracts.c
 * @brief 나머지 플랫폼 인터페이스 계약 검증 테스트들
 */

#include "test_platform_abstraction.h"

/**
 * @brief 메모리 인터페이스 계약 검증 테스트
 * @return 테스트 결과
 */
ETResult test_memory_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->memory);

    ETMemoryInterface* memory = platform->memory;

    // 필수 함수 포인터 검증
    TEST_ASSERT_NOT_NULL(memory->malloc);
    TEST_ASSERT_NOT_NULL(memory->calloc);
    TEST_ASSERT_NOT_NULL(memory->realloc);
    TEST_ASSERT_NOT_NULL(memory->free);
    TEST_ASSERT_NOT_NULL(memory->aligned_malloc);
    TEST_ASSERT_NOT_NULL(memory->aligned_free);
    TEST_ASSERT_NOT_NULL(memory->lock_pages);
    TEST_ASSERT_NOT_NULL(memory->unlock_pages);

    // 기본 메모리 할당 테스트
    void* ptr = memory->malloc(1024);
    TEST_ASSERT_NOT_NULL(ptr);

    // 메모리에 쓰기 테스트
    memset(ptr, 0xAA, 1024);

    memory->free(ptr);

    // calloc 테스트
    ptr = memory->calloc(256, 4);
    TEST_ASSERT_NOT_NULL(ptr);

    // 0으로 초기화되었는지 확인
    uint8_t* bytes = (uint8_t*)ptr;
    for (int i = 0; i < 1024; i++) {
        TEST_ASSERT_EQUAL(0, bytes[i]);
    }

    memory->free(ptr);

    // realloc 테스트
    ptr = memory->malloc(512);
    TEST_ASSERT_NOT_NULL(ptr);

    ptr = memory->realloc(ptr, 1024);
    TEST_ASSERT_NOT_NULL(ptr);

    memory->free(ptr);

    // 정렬된 메모리 할당 테스트
    void* aligned_ptr = memory->aligned_malloc(1024, 64);
    TEST_ASSERT_NOT_NULL(aligned_ptr);

    // 정렬 확인
    uintptr_t addr = (uintptr_t)aligned_ptr;
    TEST_ASSERT_EQUAL(0, addr % 64);

    memory->aligned_free(aligned_ptr);

    return ET_SUCCESS;
}

/**
 * @brief 파일시스템 인터페이스 계약 검증 테스트
 * @return 테스트 결과
 */
ETResult test_filesystem_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->filesystem);

    ETFilesystemInterface* fs = platform->filesystem;

    // 필수 함수 포인터 검증
    TEST_ASSERT_NOT_NULL(fs->normalize_path);
    TEST_ASSERT_NOT_NULL(fs->join_path);
    TEST_ASSERT_NOT_NULL(fs->get_absolute_path);
    TEST_ASSERT_NOT_NULL(fs->open_file);
    TEST_ASSERT_NOT_NULL(fs->read_file);
    TEST_ASSERT_NOT_NULL(fs->write_file);
    TEST_ASSERT_NOT_NULL(fs->seek_file);
    TEST_ASSERT_NOT_NULL(fs->tell_file);
    TEST_ASSERT_NOT_NULL(fs->close_file);
    TEST_ASSERT_NOT_NULL(fs->create_directory);
    TEST_ASSERT_NOT_NULL(fs->remove_directory);
    TEST_ASSERT_NOT_NULL(fs->list_directory);
    TEST_ASSERT_NOT_NULL(fs->get_file_info);
    TEST_ASSERT_NOT_NULL(fs->set_file_permissions);
    TEST_ASSERT_NOT_NULL(fs->file_exists);
    TEST_ASSERT_NOT_NULL(fs->is_directory);

    // 경로 정규화 테스트
    char normalized[256];
    ETResult result = fs->normalize_path("./test/../test.txt", normalized, sizeof(normalized));
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT(strlen(normalized) > 0);

    // 경로 결합 테스트
    char joined[256];
    result = fs->join_path("/tmp", "test.txt", joined, sizeof(joined));
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT(strstr(joined, "test.txt") != NULL);

    // 절대 경로 테스트
    char absolute[256];
    result = fs->get_absolute_path(".", absolute, sizeof(absolute));
    TEST_ASSERT_EQUAL(ET_SUCCESS, result);
    TEST_ASSERT(strlen(absolute) > 0);

    // 파일 존재 확인 테스트 (존재하지 않는 파일)
    bool exists = fs->file_exists("/nonexistent/file.txt");
    TEST_ASSERT(!exists);

    return ET_SUCCESS;
}

/**
 * @brief 네트워크 인터페이스 계약 검증 테스트
 * @return 테스트 결과
 */
ETResult test_network_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->network);

    ETNetworkInterface* network = platform->network;

    // 필수 함수 포인터 검증
    TEST_ASSERT_NOT_NULL(network->create_socket);
    TEST_ASSERT_NOT_NULL(network->bind_socket);
    TEST_ASSERT_NOT_NULL(network->listen_socket);
    TEST_ASSERT_NOT_NULL(network->accept_socket);
    TEST_ASSERT_NOT_NULL(network->connect_socket);
    TEST_ASSERT_NOT_NULL(network->close_socket);
    TEST_ASSERT_NOT_NULL(network->send_data);
    TEST_ASSERT_NOT_NULL(network->receive_data);
    TEST_ASSERT_NOT_NULL(network->set_socket_option);
    TEST_ASSERT_NOT_NULL(network->get_socket_option);

    // 소켓 생성 테스트
    ETSocket* socket = NULL;
    ETResult result = network->create_socket(ET_SOCKET_TYPE_TCP, &socket);

    if (result == ET_SUCCESS && socket) {
        // 소켓이 성공적으로 생성된 경우
        TEST_ASSERT_NOT_NULL(socket);

        // 소켓 닫기
        network->close_socket(socket);
    } else {
        // 네트워크가 지원되지 않는 환경일 수 있음
        TEST_ASSERT(result == ET_ERROR_NOT_SUPPORTED || result == ET_ERROR_NETWORK_UNAVAILABLE);
    }

    return ET_SUCCESS;
}

/**
 * @brief 동적 라이브러리 인터페이스 계약 검증 테스트
 * @return 테스트 결과
 */
ETResult test_dynlib_interface_contract(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);
    TEST_ASSERT_NOT_NULL(platform->dynlib);

    ETDynlibInterface* dynlib = platform->dynlib;

    // 필수 함수 포인터 검증
    TEST_ASSERT_NOT_NULL(dynlib->load_library);
    TEST_ASSERT_NOT_NULL(dynlib->get_symbol);
    TEST_ASSERT_NOT_NULL(dynlib->unload_library);
    TEST_ASSERT_NOT_NULL(dynlib->get_last_error);

    // 존재하지 않는 라이브러리 로드 테스트
    ETDynamicLibrary* lib = NULL;
    ETResult result = dynlib->load_library("/nonexistent/library.so", &lib);
    TEST_ASSERT(result != ET_SUCCESS);
    TEST_ASSERT_NULL(lib);

    // 오류 메시지 확인
    const char* error = dynlib->get_last_error();
    TEST_ASSERT_NOT_NULL(error);
    TEST_ASSERT(strlen(error) > 0);

    return ET_SUCCESS;
}

/**
 * @brief 오류 조건 테스트
 * @return 테스트 결과
 */
ETResult test_error_conditions(void) {
    // 각 인터페이스의 오류 조건들을 테스트

    // 메모리 인터페이스 오류 조건
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);

    if (platform->memory) {
        ETMemoryInterface* memory = platform->memory;

        // 0 크기 할당
        void* ptr = memory->malloc(0);
        if (ptr) {
            memory->free(ptr);
        }

        // NULL 포인터 해제 (안전해야 함)
        memory->free(NULL);
        memory->aligned_free(NULL);

        // 잘못된 정렬값
        ptr = memory->aligned_malloc(1024, 0);
        TEST_ASSERT_NULL(ptr);

        ptr = memory->aligned_malloc(1024, 3); // 2의 거듭제곱이 아님
        TEST_ASSERT_NULL(ptr);
    }

    return ET_SUCCESS;
}

/**
 * @brief 경계값 테스트
 * @return 테스트 결과
 */
ETResult test_boundary_values(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);

    // 메모리 할당 경계값 테스트
    if (platform->memory) {
        ETMemoryInterface* memory = platform->memory;

        // 매우 작은 할당
        void* ptr = memory->malloc(1);
        if (ptr) {
            memory->free(ptr);
        }

        // 큰 정렬값
        ptr = memory->aligned_malloc(1024, 4096);
        if (ptr) {
            memory->aligned_free(ptr);
        }
    }

    // 파일시스템 경계값 테스트
    if (platform->filesystem) {
        ETFilesystemInterface* fs = platform->filesystem;

        // 빈 경로
        char result[256];
        ETResult res = fs->normalize_path("", result, sizeof(result));
        TEST_ASSERT(res == ET_SUCCESS || res == ET_ERROR_INVALID_PARAMETER);

        // 매우 긴 경로
        char long_path[2048];
        memset(long_path, 'a', sizeof(long_path) - 1);
        long_path[sizeof(long_path) - 1] = '\0';

        res = fs->normalize_path(long_path, result, sizeof(result));
        TEST_ASSERT(res == ET_ERROR_BUFFER_TOO_SMALL || res == ET_ERROR_INVALID_PARAMETER);
    }

    return ET_SUCCESS;
}

/**
 * @brief 리소스 정리 테스트
 * @return 테스트 결과
 */
ETResult test_resource_cleanup(void) {
    ETPlatformInterface* platform = et_platform_get_interface();
    TEST_ASSERT_NOT_NULL(platform);

    // 메모리 리소스 정리 테스트
    if (platform->memory) {
        ETMemoryInterface* memory = platform->memory;

        // 여러 할당 후 모두 해제
        void* ptrs[10];
        for (int i = 0; i < 10; i++) {
            ptrs[i] = memory->malloc(1024 * (i + 1));
            TEST_ASSERT_NOT_NULL(ptrs[i]);
        }

        for (int i = 0; i < 10; i++) {
            memory->free(ptrs[i]);
        }
    }

    // 스레딩 리소스 정리 테스트
    if (platform->threading) {
        ETThreadInterface* threading = platform->threading;

        // 여러 뮤텍스 생성 후 파괴
        ETMutex* mutexes[5];
        for (int i = 0; i < 5; i++) {
            ETResult result = threading->create_mutex(&mutexes[i]);
            TEST_ASSERT_EQUAL(ET_SUCCESS, result);
            TEST_ASSERT_NOT_NULL(mutexes[i]);
        }

        for (int i = 0; i < 5; i++) {
            threading->destroy_mutex(mutexes[i]);
        }
    }

    return ET_SUCCESS;
}

// 플랫폼별 구현 테스트 (조건부 컴파일)
#ifdef LIBETUDE_PLATFORM_WINDOWS
ETResult test_windows_implementations(void) {
    // Windows 특화 테스트
    printf("Testing Windows-specific implementations...\n");

    // Windows 오디오 시스템 테스트
    // Windows 메모리 관리 테스트
    // Windows 스레딩 테스트

    return ET_SUCCESS;
}
#endif

#ifdef LIBETUDE_PLATFORM_LINUX
ETResult test_linux_implementations(void) {
    // Linux 특화 테스트
    printf("Testing Linux-specific implementations...\n");

    // ALSA 오디오 시스템 테스트
    // POSIX 스레딩 테스트
    // Linux 시스템 정보 테스트

    return ET_SUCCESS;
}
#endif

#ifdef LIBETUDE_PLATFORM_MACOS
ETResult test_macos_implementations(void) {
    // macOS 특화 테스트
    printf("Testing macOS-specific implementations...\n");

    // CoreAudio 시스템 테스트
    // macOS 시스템 정보 테스트
    // macOS 메모리 관리 테스트

    return ET_SUCCESS;
}
#endif