/**
 * @file platform_test.c
 * @brief 플랫폼 추상화 레이어 기본 테스트
 * @author LibEtude Team
 */

#include "libetude/platform.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* 기본 플랫폼 감지 테스트 */
static void test_platform_detection(void) {
    printf("Testing platform detection...\n");

    ETPlatformType platform = et_get_current_platform();
    assert(platform != ET_PLATFORM_UNKNOWN);

    ETArchitecture arch = et_get_current_architecture();
    assert(arch != ET_ARCH_UNKNOWN);

    printf("  Platform: %d, Architecture: %d - OK\n", platform, arch);
}

/* 플랫폼 정보 조회 테스트 */
static void test_platform_info(void) {
    printf("Testing platform info retrieval...\n");

    ETPlatformInfo info;
    ETResult result = et_get_platform_info(&info);
    assert(result == ET_SUCCESS);

    assert(info.type != ET_PLATFORM_UNKNOWN);
    assert(info.arch != ET_ARCH_UNKNOWN);
    assert(info.cpu_count > 0);
    assert(info.total_memory > 0);
    assert(strlen(info.name) > 0);

    printf("  Platform info retrieved successfully - OK\n");
}

/* 하드웨어 기능 감지 테스트 */
static void test_hardware_features(void) {
    printf("Testing hardware feature detection...\n");

    uint32_t features = et_detect_hardware_features();
    printf("  Detected features: 0x%08X\n", features);

    /* 아키텍처별 기본 기능 확인 */
    ETArchitecture arch = et_get_current_architecture();
    if (arch == ET_ARCH_X86 || arch == ET_ARCH_X64) {
        /* x86/x64에서는 최소한 SSE2는 지원해야 함 (현대적인 시스템) */
        if (features & ET_FEATURE_SSE2) {
            printf("  SSE2 support detected - OK\n");
        }
    } else if (arch == ET_ARCH_ARM64) {
        /* ARM64에서는 NEON이 기본 */
        if (features & ET_FEATURE_NEON) {
            printf("  NEON support detected - OK\n");
        }
    }

    printf("  Hardware feature detection completed - OK\n");
}

/* 플랫폼 초기화 테스트 */
static void test_platform_initialization(void) {
    printf("Testing platform initialization...\n");

    ETResult result = et_platform_setup();
    if (result != ET_SUCCESS) {
        printf("  Platform setup failed with error: %s\n", et_result_to_string(result));
        const ETDetailedError* error = et_get_last_error();
        if (error) {
            printf("  Detailed error: %s\n", error->message);
        }
    }
    assert(result == ET_SUCCESS);

    /* 초기화 후 인터페이스 가용성 확인 */
    bool any_available = false;
    for (int i = 0; i < ET_INTERFACE_COUNT; i++) {
        if (et_is_interface_available((ETInterfaceType)i)) {
            any_available = true;
            break;
        }
    }

    /* 최소한 하나의 인터페이스는 사용 가능해야 함 */
    assert(any_available);

    printf("  Platform initialization successful - OK\n");

    /* 정리 */
    et_platform_shutdown();
    printf("  Platform shutdown successful - OK\n");
}

/* 오류 처리 테스트 */
static void test_error_handling(void) {
    printf("Testing error handling...\n");

    /* 잘못된 매개변수로 오류 유발 */
    ETResult result = et_get_platform_info(NULL);
    assert(result == ET_ERROR_INVALID_PARAMETER);

    /* 오류 문자열 변환 테스트 */
    const char* error_str = et_result_to_string(ET_ERROR_INVALID_PARAMETER);
    assert(error_str != NULL);
    assert(strlen(error_str) > 0);

    printf("  Error handling working correctly - OK\n");
}

/* 메인 테스트 함수 */
int main(void) {
    printf("=== LibEtude Platform Abstraction Layer Test ===\n\n");

    /* 기본 테스트들 실행 */
    test_platform_detection();
    test_platform_info();
    test_hardware_features();
    test_platform_initialization();
    test_error_handling();

    printf("\n=== All Tests Passed! ===\n");

    /* 플랫폼 정보 출력 */
    printf("\n");
    et_print_platform_info();

    return 0;
}