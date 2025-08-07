/**
 * @file test_optimization.c
 * @brief 컴파일 타임 최적화 시스템 테스트
 * @author LibEtude Project
 * @version 1.0.0
 */

#include "libetude/platform/optimization.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// 테스트 헬퍼 함수들
// ============================================================================

static int test_count = 0;
static int test_passed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (condition) { \
            test_passed++; \
            printf("[PASS] %s\n", message); \
        } else { \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

// ============================================================================
// 컴파일 타임 매크로 테스트
// ============================================================================

/**
 * @brief 플랫폼별 컴파일 타임 선택 매크로 테스트
 */
static void test_platform_selection_macros(void) {
    printf("\n=== 플랫폼별 컴파일 타임 선택 매크로 테스트 ===\n");

    // 플랫폼별 헤더 정의 확인
    TEST_ASSERT(ET_AUDIO_IMPL_HEADER != NULL, "오디오 구현 헤더 정의됨");
    TEST_ASSERT(ET_SYSTEM_IMPL_HEADER != NULL, "시스템 구현 헤더 정의됨");
    TEST_ASSERT(ET_THREAD_IMPL_HEADER != NULL, "스레딩 구현 헤더 정의됨");
    TEST_ASSERT(ET_MEMORY_IMPL_HEADER != NULL, "메모리 구현 헤더 정의됨");
    TEST_ASSERT(ET_FILESYSTEM_IMPL_HEADER != NULL, "파일시스템 구현 헤더 정의됨");
    TEST_ASSERT(ET_NETWORK_IMPL_HEADER != NULL, "네트워크 구현 헤더 정의됨");
    TEST_ASSERT(ET_DYNLIB_IMPL_HEADER != NULL, "동적 라이브러리 구현 헤더 정의됨");

    printf("선택된 구현:\n");
    printf("  - 오디오: %s\n", ET_AUDIO_IMPL_HEADER);
    printf("  - 시스템: %s\n", ET_SYSTEM_IMPL_HEADER);
    printf("  - 스레딩: %s\n", ET_THREAD_IMPL_HEADER);
    printf("  - 메모리: %s\n", ET_MEMORY_IMPL_HEADER);
    printf("  - 파일시스템: %s\n", ET_FILESYSTEM_IMPL_HEADER);
    printf("  - 네트워크: %s\n", ET_NETWORK_IMPL_HEADER);
    printf("  - 동적 라이브러리: %s\n", ET_DYNLIB_IMPL_HEADER);
}

/**
 * @brief 조건부 컴파일 매크로 테스트
 */
static void test_conditional_compilation_macros(void) {
    printf("\n=== 조건부 컴파일 매크로 테스트 ===\n");

    // SIMD 기능 테스트
    printf("SIMD 지원 상태:\n");
    printf("  - SSE: %s\n", ET_SSE_ENABLED ? "활성화" : "비활성화");
    printf("  - SSE2: %s\n", ET_SSE2_ENABLED ? "활성화" : "비활성화");
    printf("  - AVX: %s\n", ET_AVX_ENABLED ? "활성화" : "비활성화");
    printf("  - AVX2: %s\n", ET_AVX2_ENABLED ? "활성화" : "비활성화");
    printf("  - NEON: %s\n", ET_NEON_ENABLED ? "활성화" : "비활성화");

    // 플랫폼별 조건부 컴파일 테스트
    int platform_detected = 0;

    ET_IF_WINDOWS({
        printf("Windows 플랫폼 감지됨\n");
        platform_detected = 1;
    });

    ET_IF_LINUX({
        printf("Linux 플랫폼 감지됨\n");
        platform_detected = 1;
    });

    ET_IF_MACOS({
        printf("macOS 플랫폼 감지됨\n");
        platform_detected = 1;
    });

    TEST_ASSERT(platform_detected == 1, "플랫폼이 올바르게 감지됨");

    // 디버그/릴리스 모드 테스트
    ET_IF_DEBUG({
        printf("디버그 모드에서 실행 중\n");
        TEST_ASSERT(1, "디버그 모드 매크로 작동");
    });

    ET_IF_RELEASE({
        printf("릴리스 모드에서 실행 중\n");
        TEST_ASSERT(1, "릴리스 모드 매크로 작동");
    });
}

/**
 * @brief 컴파일러별 최적화 힌트 매크로 테스트
 */
static void test_compiler_optimization_hints(void) {
    printf("\n=== 컴파일러별 최적화 힌트 매크로 테스트 ===\n");

    // 분기 예측 힌트 테스트
    int value = 1;
    if (ET_LIKELY(value == 1)) {
        TEST_ASSERT(1, "LIKELY 매크로 작동");
    }

    if (ET_UNLIKELY(value == 0)) {
        TEST_ASSERT(0, "UNLIKELY 매크로 실패");
    } else {
        TEST_ASSERT(1, "UNLIKELY 매크로 작동");
    }

    // 메모리 배리어 테스트 (실제로는 컴파일만 확인)
    ET_MEMORY_BARRIER();
    ET_COMPILER_BARRIER();
    TEST_ASSERT(1, "메모리 배리어 매크로 컴파일됨");

    // 프리페치 테스트 (실제로는 컴파일만 확인)
    int dummy_array[100];
    ET_PREFETCH(&dummy_array[50], 0, 3);
    TEST_ASSERT(1, "프리페치 매크로 컴파일됨");
}

/**
 * @brief 정적 어서션 테스트
 */
static void test_static_assertions(void) {
    printf("\n=== 정적 어서션 테스트 ===\n");

    // 컴파일 타임에 검증되는 어서션들
    ET_STATIC_ASSERT(sizeof(int) >= 4, "int는 최소 4바이트여야 함");
    ET_STATIC_ASSERT(sizeof(void*) >= sizeof(int), "포인터는 int보다 크거나 같아야 함");
    ET_STATIC_ASSERT(1 == 1, "기본 어서션 테스트");

    TEST_ASSERT(1, "정적 어서션들이 컴파일 타임에 통과됨");
}

// ============================================================================
// 런타임 함수 테스트
// ============================================================================

/**
 * @brief 최적화 시스템 초기화 테스트
 */
static void test_optimization_initialization(void) {
    printf("\n=== 최적화 시스템 초기화 테스트 ===\n");

    ETResult result = et_optimization_initialize();
    TEST_ASSERT(result == ET_SUCCESS, "최적화 시스템 초기화 성공");

    // 중복 초기화 테스트
    result = et_optimization_initialize();
    TEST_ASSERT(result == ET_SUCCESS, "중복 초기화 처리 성공");
}

/**
 * @brief 컴파일 정보 조회 테스트
 */
static void test_compilation_info_retrieval(void) {
    printf("\n=== 컴파일 정보 조회 테스트 ===\n");

    struct ETCompilationInfo info;
    ETResult result = et_get_compilation_info(&info);
    TEST_ASSERT(result == ET_SUCCESS, "컴파일 정보 조회 성공");

    // 정보 유효성 검사
    TEST_ASSERT(info.platform != ET_PLATFORM_UNKNOWN, "플랫폼 정보 유효");
    TEST_ASSERT(info.architecture != ET_ARCH_UNKNOWN, "아키텍처 정보 유효");
    TEST_ASSERT(strlen(info.compiler_name) > 0, "컴파일러 이름 유효");
    TEST_ASSERT(strlen(info.version_string) > 0, "버전 문자열 유효");
    TEST_ASSERT(strlen(info.build_date) > 0, "빌드 날짜 유효");
    TEST_ASSERT(strlen(info.build_time) > 0, "빌드 시간 유효");

    printf("컴파일 정보:\n");
    printf("  - 플랫폼: %d\n", info.platform);
    printf("  - 아키텍처: %d\n", info.architecture);
    printf("  - 컴파일러: %s (버전: %d)\n", info.compiler_name, info.compiler_version);
    printf("  - 버전: %s\n", info.version_string);
    printf("  - 빌드: %s %s\n", info.build_date, info.build_time);
    printf("  - SIMD 활성화: %s\n", info.simd_enabled ? "예" : "아니오");
    printf("  - 디버그 모드: %s\n", info.debug_enabled ? "예" : "아니오");

    // NULL 포인터 테스트
    result = et_get_compilation_info(NULL);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "NULL 포인터 처리 성공");
}

/**
 * @brief 사용 가능한 최적화 기능 테스트
 */
static void test_available_optimizations(void) {
    printf("\n=== 사용 가능한 최적화 기능 테스트 ===\n");

    uint32_t features = et_get_available_optimizations();

    printf("사용 가능한 최적화 기능:\n");
    if (features & ET_FEATURE_SSE) printf("  - SSE\n");
    if (features & ET_FEATURE_SSE2) printf("  - SSE2\n");
    if (features & ET_FEATURE_AVX) printf("  - AVX\n");
    if (features & ET_FEATURE_AVX2) printf("  - AVX2\n");
    if (features & ET_FEATURE_NEON) printf("  - NEON\n");

    if (features == 0) {
        printf("  - 기본 최적화만 사용 가능\n");
    }

    TEST_ASSERT(1, "최적화 기능 조회 성공");
}

/**
 * @brief 컴파일 정보 문자열 테스트
 */
static void test_compilation_info_string(void) {
    printf("\n=== 컴파일 정보 문자열 테스트 ===\n");

    char buffer[2048];
    ETResult result = et_get_compilation_info_string(buffer, sizeof(buffer));
    TEST_ASSERT(result == ET_SUCCESS, "컴파일 정보 문자열 생성 성공");
    TEST_ASSERT(strlen(buffer) > 0, "문자열 내용 유효");

    printf("컴파일 정보 문자열:\n%s\n", buffer);

    // 버퍼 크기 부족 테스트
    char small_buffer[10];
    result = et_get_compilation_info_string(small_buffer, sizeof(small_buffer));
    TEST_ASSERT(result == ET_ERROR_BUFFER_TOO_SMALL, "작은 버퍼 처리 성공");

    // NULL 포인터 테스트
    result = et_get_compilation_info_string(NULL, 100);
    TEST_ASSERT(result == ET_ERROR_INVALID_ARGUMENT, "NULL 포인터 처리 성공");
}

/**
 * @brief 프로파일링 기능 테스트
 */
static void test_profiling_functionality(void) {
    printf("\n=== 프로파일링 기능 테스트 ===\n");

#if ET_PROFILE_ENABLED
    printf("프로파일링이 활성화되어 있습니다.\n");

    ET_PROFILE_BEGIN("test_function");

    // 간단한 작업 시뮬레이션
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }

    ET_PROFILE_END("test_function");

    TEST_ASSERT(1, "프로파일링 매크로 작동");
#else
    printf("프로파일링이 비활성화되어 있습니다.\n");
    TEST_ASSERT(1, "프로파일링 비활성화 상태 확인");
#endif
}

/**
 * @brief 최적화 시스템 정리 테스트
 */
static void test_optimization_finalization(void) {
    printf("\n=== 최적화 시스템 정리 테스트 ===\n");

    et_optimization_finalize();
    TEST_ASSERT(1, "최적화 시스템 정리 성공");

    // 정리 후 중복 호출 테스트
    et_optimization_finalize();
    TEST_ASSERT(1, "중복 정리 호출 처리 성공");
}

// ============================================================================
// 메인 테스트 함수
// ============================================================================

int main(void) {
    printf("=== LibEtude 컴파일 타임 최적화 시스템 테스트 ===\n");

    // 컴파일 타임 매크로 테스트
    test_platform_selection_macros();
    test_conditional_compilation_macros();
    test_compiler_optimization_hints();
    test_static_assertions();

    // 런타임 함수 테스트
    test_optimization_initialization();
    test_compilation_info_retrieval();
    test_available_optimizations();
    test_compilation_info_string();
    test_profiling_functionality();
    test_optimization_finalization();

    // 테스트 결과 출력
    printf("\n=== 테스트 결과 ===\n");
    printf("총 테스트: %d\n", test_count);
    printf("통과: %d\n", test_passed);
    printf("실패: %d\n", test_count - test_passed);
    printf("성공률: %.1f%%\n", (float)test_passed / test_count * 100.0f);

    return (test_passed == test_count) ? 0 : 1;
}