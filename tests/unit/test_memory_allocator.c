/**
 * @file test_memory_allocator.c
 * @brief 메모리 할당자 단위 테스트
 *
 * 메모리 할당자의 기본 기능과 누수 감지 기능을 테스트합니다.
 */

#include "libetude/memory.h"
#include "tests/framework/test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 테스트 상수
#define TEST_POOL_SIZE (1024 * 1024)  // 1MB
#define TEST_ALIGNMENT 32
#define TEST_BLOCK_SIZE 256

// 테스트 함수 선언
static void test_allocator_creation(void);
static void test_basic_allocation(void);
static void test_aligned_allocation(void);
static void test_memory_tracking(void);
static void test_leak_detection(void);
static void test_memory_corruption_detection(void);
static void test_allocator_stats(void);
static void test_allocator_reset(void);

// 테스트 스위트 정의
ETTestCase allocator_test_cases[] = {
    {"Allocator Creation", NULL, NULL, test_allocator_creation, false, ""},
    {"Basic Allocation", NULL, NULL, test_basic_allocation, false, ""},
    {"Aligned Allocation", NULL, NULL, test_aligned_allocation, false, ""},
    {"Memory Tracking", NULL, NULL, test_memory_tracking, false, ""},
    {"Leak Detection", NULL, NULL, test_leak_detection, false, ""},
    {"Memory Corruption Detection", NULL, NULL, test_memory_corruption_detection, false, ""},
    {"Allocator Stats", NULL, NULL, test_allocator_stats, false, ""},
    {"Allocator Reset", NULL, NULL, test_allocator_reset, false, ""}
};

ETTestSuite allocator_test_suite = {
    "Memory Allocator Tests",
    allocator_test_cases,
    sizeof(allocator_test_cases) / sizeof(ETTestCase),
    0, 0
};

// =============================================================================
// 테스트 함수 구현
// =============================================================================

static void test_allocator_creation(void) {
    printf("Testing allocator creation...\n");

    // 정상적인 할당자 생성
    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 할당자 유효성 검사
    ET_TEST_ASSERT(rt_validate_allocator(allocator), "Allocator validation failed");

    // 초기 상태 확인
    ET_TEST_ASSERT(rt_get_total_size(allocator) == TEST_POOL_SIZE, "Incorrect total size");
    ET_TEST_ASSERT(rt_get_used_size(allocator) == 0, "Initial used size should be 0");
    ET_TEST_ASSERT(rt_get_free_size(allocator) == TEST_POOL_SIZE, "Incorrect free size");
    ET_TEST_ASSERT(rt_get_peak_usage(allocator) == 0, "Initial peak usage should be 0");

    rt_destroy_allocator(allocator);

    // 잘못된 파라미터로 생성 시도
    RTAllocator* invalid_allocator = rt_create_allocator(0, TEST_ALIGNMENT);
    ET_TEST_ASSERT(invalid_allocator == NULL, "Should fail with zero size");

    printf("✓ Allocator creation test passed\n");
}

static void test_basic_allocation(void) {
    printf("Testing basic allocation...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 기본 할당 테스트
    void* ptr1 = rt_alloc(allocator, 128);
    ET_TEST_ASSERT(ptr1 != NULL, "Failed to allocate memory");
    ET_TEST_ASSERT(rt_get_used_size(allocator) > 0, "Used size should increase");

    void* ptr2 = rt_alloc(allocator, 256);
    ET_TEST_ASSERT(ptr2 != NULL, "Failed to allocate second block");
    ET_TEST_ASSERT(ptr1 != ptr2, "Pointers should be different");

    // 메모리 사용 테스트
    memset(ptr1, 0xAA, 128);
    memset(ptr2, 0xBB, 256);

    // calloc 테스트
    void* ptr3 = rt_calloc(allocator, 10, sizeof(int));
    ET_TEST_ASSERT(ptr3 != NULL, "Failed to calloc memory");

    int* int_ptr = (int*)ptr3;
    for (int i = 0; i < 10; i++) {
        ET_TEST_ASSERT(int_ptr[i] == 0, "calloc should initialize to zero");
    }

    // 메모리 해제
    rt_free(allocator, ptr1);
    rt_free(allocator, ptr2);
    rt_free(allocator, ptr3);

    rt_destroy_allocator(allocator);

    printf("✓ Basic allocation test passed\n");
}

static void test_aligned_allocation(void) {
    printf("Testing aligned allocation...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, 16);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 다양한 정렬 요구사항 테스트
    size_t alignments[] = {16, 32, 64, 128, 256};
    void* ptrs[5];

    for (int i = 0; i < 5; i++) {
        ptrs[i] = rt_alloc_aligned(allocator, 100, alignments[i]);
        ET_TEST_ASSERT(ptrs[i] != NULL, "Failed to allocate aligned memory");

        // 정렬 확인
        uintptr_t addr = (uintptr_t)ptrs[i];
        ET_TEST_ASSERT((addr & (alignments[i] - 1)) == 0, "Memory not properly aligned");

        // 메모리 사용 테스트
        memset(ptrs[i], i + 1, 100);
    }

    // 메모리 해제
    for (int i = 0; i < 5; i++) {
        rt_free(allocator, ptrs[i]);
    }

    rt_destroy_allocator(allocator);

    printf("✓ Aligned allocation test passed\n");
}

static void test_memory_tracking(void) {
    printf("Testing memory tracking...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    size_t initial_used = rt_get_used_size(allocator);
    size_t initial_peak = rt_get_peak_usage(allocator);

    // 메모리 할당 및 추적
    void* ptr1 = rt_alloc(allocator, 512);
    ET_TEST_ASSERT(ptr1 != NULL, "Failed to allocate memory");

    size_t used_after_alloc = rt_get_used_size(allocator);
    size_t peak_after_alloc = rt_get_peak_usage(allocator);

    ET_TEST_ASSERT(used_after_alloc > initial_used, "Used size should increase");
    ET_TEST_ASSERT(peak_after_alloc >= used_after_alloc, "Peak should be at least used size");

    // 더 많은 메모리 할당
    void* ptr2 = rt_alloc(allocator, 1024);
    ET_TEST_ASSERT(ptr2 != NULL, "Failed to allocate more memory");

    size_t used_after_second = rt_get_used_size(allocator);
    size_t peak_after_second = rt_get_peak_usage(allocator);

    ET_TEST_ASSERT(used_after_second > used_after_alloc, "Used size should increase more");
    ET_TEST_ASSERT(peak_after_second >= used_after_second, "Peak should track maximum");

    // 메모리 해제 후 추적
    rt_free(allocator, ptr1);
    size_t used_after_free = rt_get_used_size(allocator);
    size_t peak_after_free = rt_get_peak_usage(allocator);

    ET_TEST_ASSERT(used_after_free < used_after_second, "Used size should decrease");
    ET_TEST_ASSERT(peak_after_free == peak_after_second, "Peak should remain same");

    rt_free(allocator, ptr2);
    rt_destroy_allocator(allocator);

    printf("✓ Memory tracking test passed\n");
}

static void test_leak_detection(void) {
    printf("Testing leak detection...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 누수 감지 활성화
    rt_enable_leak_detection(allocator, true);

    // 메모리 할당 (의도적으로 해제하지 않음)
    void* leaked_ptr1 = rt_alloc(allocator, 256);
    void* leaked_ptr2 = rt_alloc(allocator, 512);
    ET_TEST_ASSERT(leaked_ptr1 != NULL && leaked_ptr2 != NULL, "Failed to allocate memory");

    // 정상적으로 해제되는 메모리
    void* normal_ptr = rt_alloc(allocator, 128);
    ET_TEST_ASSERT(normal_ptr != NULL, "Failed to allocate normal memory");
    rt_free(allocator, normal_ptr);

    // 잠시 대기 (타임스탬프 차이를 위해)
    usleep(100000); // 100ms

    // 누수 검사 (100ms 임계값)
    size_t leak_count = rt_check_memory_leaks(allocator, 50);
    ET_TEST_ASSERT(leak_count == 2, "Should detect 2 leaked blocks");

    // 누수 정보 조회
    ETMemoryLeakInfo leak_infos[10];
    size_t actual_leaks = rt_get_memory_leaks(allocator, leak_infos, 10);
    ET_TEST_ASSERT(actual_leaks == 2, "Should return 2 leak infos");

    // 누수 리포트 출력 (파일로)
    rt_print_memory_leak_report(allocator, "test_leak_report.txt");

    // 통계에서 누수 정보 확인
    ETMemoryPoolStats stats;
    rt_get_allocator_stats(allocator, &stats);
    ET_TEST_ASSERT(stats.num_active_blocks >= 2, "Should have active blocks");

    // 누수된 메모리 해제
    rt_free(allocator, leaked_ptr1);
    rt_free(allocator, leaked_ptr2);

    // 누수 재검사
    leak_count = rt_check_memory_leaks(allocator, 50);
    ET_TEST_ASSERT(leak_count == 0, "Should detect no leaks after cleanup");

    rt_destroy_allocator(allocator);

    printf("✓ Leak detection test passed\n");
}

static void test_memory_corruption_detection(void) {
    printf("Testing memory corruption detection...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 누수 감지 활성화 (손상 감지도 포함)
    rt_enable_leak_detection(allocator, true);

    // 메모리 할당
    void* ptr = rt_alloc(allocator, 256);
    ET_TEST_ASSERT(ptr != NULL, "Failed to allocate memory");

    // 초기 손상 검사 (손상이 없어야 함)
    size_t corruption_count = rt_check_memory_corruption(allocator);
    ET_TEST_ASSERT(corruption_count == 0, "Should detect no corruption initially");

    // 정상 사용
    memset(ptr, 0xCC, 256);

    // 다시 손상 검사
    corruption_count = rt_check_memory_corruption(allocator);
    ET_TEST_ASSERT(corruption_count == 0, "Should detect no corruption after normal use");

    rt_free(allocator, ptr);
    rt_destroy_allocator(allocator);

    printf("✓ Memory corruption detection test passed\n");
}

static void test_allocator_stats(void) {
    printf("Testing allocator statistics...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 초기 통계 확인
    ETMemoryPoolStats stats;
    rt_get_allocator_stats(allocator, &stats);

    ET_TEST_ASSERT(stats.total_size == TEST_POOL_SIZE, "Incorrect total size in stats");
    ET_TEST_ASSERT(stats.used_size == 0, "Initial used size should be 0");
    ET_TEST_ASSERT(stats.num_allocations == 0, "Initial allocation count should be 0");
    ET_TEST_ASSERT(stats.num_frees == 0, "Initial free count should be 0");

    // 메모리 할당 후 통계
    void* ptr1 = rt_alloc(allocator, 512);
    void* ptr2 = rt_alloc(allocator, 256);

    rt_get_allocator_stats(allocator, &stats);
    ET_TEST_ASSERT(stats.used_size > 0, "Used size should increase");
    ET_TEST_ASSERT(stats.num_allocations == 2, "Should have 2 allocations");
    ET_TEST_ASSERT(stats.free_size < TEST_POOL_SIZE, "Free size should decrease");

    // 메모리 해제 후 통계
    rt_free(allocator, ptr1);
    rt_get_allocator_stats(allocator, &stats);
    ET_TEST_ASSERT(stats.num_frees == 1, "Should have 1 free");

    rt_free(allocator, ptr2);
    rt_get_allocator_stats(allocator, &stats);
    ET_TEST_ASSERT(stats.num_frees == 2, "Should have 2 frees");

    // 할당자 정보 출력 테스트
    printf("=== Allocator Info ===\n");
    rt_print_allocator_info(allocator);

    rt_destroy_allocator(allocator);

    printf("✓ Allocator statistics test passed\n");
}

static void test_allocator_reset(void) {
    printf("Testing allocator reset...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    ET_TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 메모리 할당
    void* ptr1 = rt_alloc(allocator, 512);
    void* ptr2 = rt_alloc(allocator, 256);
    void* ptr3 = rt_alloc(allocator, 128);

    ET_TEST_ASSERT(ptr1 && ptr2 && ptr3, "Failed to allocate memory");
    ET_TEST_ASSERT(rt_get_used_size(allocator) > 0, "Used size should be greater than 0");

    // 할당자 리셋
    rt_reset_allocator(allocator);

    // 리셋 후 상태 확인
    ET_TEST_ASSERT(rt_get_used_size(allocator) == 0, "Used size should be 0 after reset");
    ET_TEST_ASSERT(rt_get_free_size(allocator) == TEST_POOL_SIZE, "Free size should be total size");

    // 리셋 후 새로운 할당 가능한지 확인
    void* new_ptr = rt_alloc(allocator, 1024);
    ET_TEST_ASSERT(new_ptr != NULL, "Should be able to allocate after reset");

    rt_free(allocator, new_ptr);
    rt_destroy_allocator(allocator);

    printf("✓ Allocator reset test passed\n");
}

// =============================================================================
// 메인 테스트 실행 함수
// =============================================================================

int main(void) {
    printf("=== Memory Allocator Unit Tests ===\n\n");

    et_run_test_suite(&allocator_test_suite);

    printf("\n=== Test Results ===\n");
    printf("Total Tests: %d\n", allocator_test_suite.num_test_cases);
    printf("Passed: %d\n", allocator_test_suite.passed_count);
    printf("Failed: %d\n", allocator_test_suite.failed_count);

    if (allocator_test_suite.failed_count == 0) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed!\n");
        return 1;
    }
}