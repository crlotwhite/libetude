/**
 * @file test_platform_abstraction.h
 * @brief 플랫폼 추상화 레이어 단위 테스트 헤더
 *
 * 플랫폼 추상화 레이어의 모든 인터페이스에 대한 단위 테스트를 위한
 * 공통 헤더 파일입니다.
 */

#ifndef TEST_PLATFORM_ABSTRACTION_H
#define TEST_PLATFORM_ABSTRACTION_H

#include <libetude/platform/common.h>
#include <libetude/platform/factory.h>
#include <libetude/platform/audio.h>
#include <libetude/platform/system.h>
#include <libetude/platform/threading.h>
#include <libetude/platform/memory.h>
#include <libetude/platform/filesystem.h>
#include <libetude/platform/network.h>
#include <libetude/platform/dynlib.h>
#include <libetude/error.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// 테스트 매크로 정의
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "ASSERTION FAILED: Expected %d, got %d at %s:%d\n", \
                    (int)(expected), (int)(actual), __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "ASSERTION FAILED: Pointer is NULL at %s:%d\n", __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "ASSERTION FAILED: Pointer is not NULL at %s:%d\n", __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

#define TEST_ASSERT_STRING_EQUAL(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            fprintf(stderr, "ASSERTION FAILED: Expected '%s', got '%s' at %s:%d\n", \
                    (expected), (actual), __FILE__, __LINE__); \
            return ET_ERROR_TEST_FAILED; \
        } \
    } while(0)

// 테스트 결과 타입
typedef enum {
    TEST_RESULT_PASS = 0,
    TEST_RESULT_FAIL = 1,
    TEST_RESULT_SKIP = 2
} TestResult;

// 테스트 케이스 구조체
typedef struct {
    const char* name;
    ETResult (*test_func)(void);
    const char* description;
} TestCase;

// 테스트 스위트 구조체
typedef struct {
    const char* name;
    TestCase* tests;
    int test_count;
    ETResult (*setup)(void);
    ETResult (*teardown)(void);
} TestSuite;

// 테스트 통계
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    double total_time;
} TestStats;

// 메모리 누수 감지를 위한 구조체
typedef struct {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    struct MemoryAllocation* next;
} MemoryAllocation;

// 테스트 컨텍스트
typedef struct {
    TestStats stats;
    MemoryAllocation* allocations;
    int memory_leak_count;
    bool verbose;
} TestContext;

// 전역 테스트 컨텍스트
extern TestContext g_test_context;

// 테스트 실행 함수들
ETResult run_test_suite(TestSuite* suite);
ETResult run_all_platform_tests(void);
void print_test_results(const TestStats* stats);

// 메모리 누수 감지 함수들
void* test_malloc(size_t size, const char* file, int line);
void test_free(void* ptr, const char* file, int line);
void check_memory_leaks(void);
void reset_memory_tracking(void);

// 메모리 추적 매크로
#define TEST_MALLOC(size) test_malloc(size, __FILE__, __LINE__)
#define TEST_FREE(ptr) test_free(ptr, __FILE__, __LINE__)

// 플랫폼별 테스트 함수 선언
ETResult test_audio_interface_contract(void);
ETResult test_system_interface_contract(void);
ETResult test_threading_interface_contract(void);
ETResult test_memory_interface_contract(void);
ETResult test_filesystem_interface_contract(void);
ETResult test_network_interface_contract(void);
ETResult test_dynlib_interface_contract(void);

// 오류 조건 테스트 함수들
ETResult test_error_conditions(void);
ETResult test_boundary_values(void);
ETResult test_resource_cleanup(void);

// 플랫폼별 구현 테스트 함수들
#ifdef LIBETUDE_PLATFORM_WINDOWS
ETResult test_windows_implementations(void);
#endif

#ifdef LIBETUDE_PLATFORM_LINUX
ETResult test_linux_implementations(void);
#endif

#ifdef LIBETUDE_PLATFORM_MACOS
ETResult test_macos_implementations(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // TEST_PLATFORM_ABSTRACTION_H