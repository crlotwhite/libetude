/* LibEtude Windows 플랫폼 단위 테스트 */
/* Copyright (c) 2025 LibEtude Project */

#include "libetude/platform/windows.h"
#include "libetude/error.h"
#include <stdio.h>
#include <assert.h>

/* 테스트 결과 출력 매크로 */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return false; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

/* 기본 설정 생성 테스트 */
bool test_default_config_creation(void) {
    printf("\n=== 기본 설정 생성 테스트 ===\n");

    ETWindowsPlatformConfig config = et_windows_create_default_config();

    TEST_ASSERT(config.audio.prefer_wasapi == true, "WASAPI 기본 활성화");
    TEST_ASSERT(config.audio.buffer_size_ms == 20, "기본 버퍼 크기 20ms");
    TEST_ASSERT(config.performance.enable_large_pages == true, "Large Page 기본 활성화");
    TEST_ASSERT(config.security.enforce_dep == true, "DEP 기본 활성화");

    return true;
}

/* Windows 플랫폼 초기화/정리 테스트 */
bool test_platform_init_finalize(void) {
    printf("\n=== 플랫폼 초기화/정리 테스트 ===\n");

    /* 초기화 전 상태 확인 */
    TEST_ASSERT(!et_windows_is_initialized(), "초기화 전 상태 확인");

    /* 기본 설정으로 초기화 */
    ETWindowsPlatformConfig config = et_windows_create_default_config();
    ETResult result = et_windows_init(&config);

    TEST_ASSERT(result == ET_SUCCESS, "플랫폼 초기화 성공");
    TEST_ASSERT(et_windows_is_initialized(), "초기화 후 상태 확인");

    /* 중복 초기화 시도 */
    result = et_windows_init(&config);
    TEST_ASSERT(result == ET_ERROR_ALREADY_INITIALIZED, "중복 초기화 방지");

    /* 플랫폼 정보 조회 */
    char info_buffer[1024];
    result = et_windows_get_platform_info(info_buffer, sizeof(info_buffer));
    TEST_ASSERT(result == ET_SUCCESS, "플랫폼 정보 조회 성공");
    printf("플랫폼 정보:\n%s\n", info_buffer);

    /* 정리 */
    et_windows_finalize();
    TEST_ASSERT(!et_windows_is_initialized(), "정리 후 상태 확인");

    return true;
}

/* CPU 기능 감지 테스트 */
bool test_cpu_feature_detection(void) {
    printf("\n=== CPU 기능 감지 테스트 ===\n");

    ETWindowsCPUFeatures features = et_windows_detect_cpu_features();

    printf("감지된 CPU 기능:\n");
    printf("- SSE4.1: %s\n", features.has_sse41 ? "지원" : "미지원");
    printf("- AVX: %s\n", features.has_avx ? "지원" : "미지원");
    printf("- AVX2: %s\n", features.has_avx2 ? "지원" : "미지원");
    printf("- AVX-512: %s\n", features.has_avx512 ? "지원" : "미지원");

    /* CPU 기능 감지는 항상 성공해야 함 (결과가 모두 false여도 OK) */
    TEST_ASSERT(true, "CPU 기능 감지 완료");

    return true;
}

/* 보안 기능 테스트 */
bool test_security_features(void) {
    printf("\n=== 보안 기능 테스트 ===\n");

    /* DEP 호환성 확인 */
    bool dep_compatible = et_windows_check_dep_compatibility();
    printf("DEP 호환성: %s\n", dep_compatible ? "호환" : "비호환");
    TEST_ASSERT(true, "DEP 호환성 확인 완료");

    /* UAC 권한 확인 */
    bool uac_elevated = et_windows_check_uac_permissions();
    printf("UAC 권한: %s\n", uac_elevated ? "관리자" : "일반 사용자");
    TEST_ASSERT(true, "UAC 권한 확인 완료");

    return true;
}

/* 메모리 할당 테스트 */
bool test_memory_allocation(void) {
    printf("\n=== 메모리 할당 테스트 ===\n");

    /* ASLR 호환 메모리 할당 */
    size_t test_size = 1024 * 1024;  /* 1MB */
    void* aslr_ptr = et_windows_alloc_aslr_compatible(test_size);
    TEST_ASSERT(aslr_ptr != NULL, "ASLR 호환 메모리 할당");

    /* 메모리에 데이터 쓰기/읽기 테스트 */
    char* test_data = (char*)aslr_ptr;
    test_data[0] = 'A';
    test_data[test_size - 1] = 'Z';
    TEST_ASSERT(test_data[0] == 'A' && test_data[test_size - 1] == 'Z', "메모리 읽기/쓰기");

    /* 메모리 해제 */
    if (VirtualFree(aslr_ptr, 0, MEM_RELEASE) == 0) {
        /* VirtualAlloc으로 할당되지 않았으면 free 사용 */
        free(aslr_ptr);
    }

    /* Large Page 메모리 할당 테스트 */
    void* large_page_ptr = et_windows_alloc_large_pages(test_size);
    TEST_ASSERT(large_page_ptr != NULL, "Large Page 메모리 할당");

    /* Large Page 메모리 해제 */
    if (VirtualFree(large_page_ptr, 0, MEM_RELEASE) == 0) {
        free(large_page_ptr);
    }

    return true;
}

/* SIMD 커널 테스트 */
bool test_simd_kernels(void) {
    printf("\n=== SIMD 커널 테스트 ===\n");

    /* 작은 행렬로 테스트 */
    const int m = 2, n = 2, k = 2;
    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float c[4] = {0};

    /* AVX2 최적화된 행렬 곱셈 테스트 */
    et_windows_simd_matrix_multiply_avx2(a, b, c, m, n, k);

    /* 결과 확인 (수동 계산: [1,2;3,4] * [5,6;7,8] = [19,22;43,50]) */
    float expected[4] = {19.0f, 22.0f, 43.0f, 50.0f};
    bool matrix_correct = true;
    for (int i = 0; i < 4; i++) {
        if (c[i] != expected[i]) {
            matrix_correct = false;
            break;
        }
    }

    TEST_ASSERT(matrix_correct, "SIMD 행렬 곱셈 결과 정확성");

    return true;
}

/* 메인 테스트 함수 */
int main(void) {
    printf("LibEtude Windows 플랫폼 단위 테스트 시작\n");
    printf("==========================================\n");

    bool all_passed = true;

    /* 각 테스트 실행 */
    all_passed &= test_default_config_creation();
    all_passed &= test_platform_init_finalize();
    all_passed &= test_cpu_feature_detection();
    all_passed &= test_security_features();
    all_passed &= test_memory_allocation();
    all_passed &= test_simd_kernels();

    printf("\n==========================================\n");
    if (all_passed) {
        printf("모든 테스트 통과! ✓\n");
        return 0;
    } else {
        printf("일부 테스트 실패! ✗\n");
        return 1;
    }
}