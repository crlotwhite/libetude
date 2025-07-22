/**
 * @file test_memory_allocator_simple.c
 * @brief 메모리 할당자 간단한 테스트
 */

#include "include/libetude/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

// 간단한 테스트 매크로
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            printf("  Condition: %s\n", #condition); \
            printf("  File: %s:%d\n", __FILE__, __LINE__); \
            return 1; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        printf("PASS: %s\n", message); \
    } while(0)

// 테스트 상수
#define TEST_POOL_SIZE (1024 * 1024)  // 1MB
#define TEST_ALIGNMENT 32

int test_allocator_creation(void) {
    printf("Testing allocator creation...\n");

    // 정상적인 할당자 생성
    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 할당자 유효성 검사
    TEST_ASSERT(rt_validate_allocator(allocator), "Allocator validation failed");

    // 초기 상태 확인
    TEST_ASSERT(rt_get_total_size(allocator) == TEST_POOL_SIZE, "Incorrect total size");
    TEST_ASSERT(rt_get_used_size(allocator) == 0, "Initial used size should be 0");
    TEST_ASSERT(rt_get_free_size(allocator) == TEST_POOL_SIZE, "Incorrect free size");
    TEST_ASSERT(rt_get_peak_usage(allocator) == 0, "Initial peak usage should be 0");

    rt_destroy_allocator(allocator);

    // 잘못된 파라미터로 생성 시도
    RTAllocator* invalid_allocator = rt_create_allocator(0, TEST_ALIGNMENT);
    TEST_ASSERT(invalid_allocator == NULL, "Should fail with zero size");

    TEST_PASS("Allocator creation test");
    return 0;
}

int test_basic_allocation(void) {
    printf("Testing basic allocation...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 기본 할당 테스트
    void* ptr1 = rt_alloc(allocator, 128);
    TEST_ASSERT(ptr1 != NULL, "Failed to allocate memory");
    TEST_ASSERT(rt_get_used_size(allocator) > 0, "Used size should increase");

    void* ptr2 = rt_alloc(allocator, 256);
    TEST_ASSERT(ptr2 != NULL, "Failed to allocate second block");
    TEST_ASSERT(ptr1 != ptr2, "Pointers should be different");

    // 메모리 사용 테스트
    memset(ptr1, 0xAA, 128);
    memset(ptr2, 0xBB, 256);

    // calloc 테스트
    void* ptr3 = rt_calloc(allocator, 10, sizeof(int));
    TEST_ASSERT(ptr3 != NULL, "Failed to calloc memory");

    int* int_ptr = (int*)ptr3;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(int_ptr[i] == 0, "calloc should initialize to zero");
    }

    // 메모리 해제
    rt_free(allocator, ptr1);
    rt_free(allocator, ptr2);
    rt_free(allocator, ptr3);

    rt_destroy_allocator(allocator);

    TEST_PASS("Basic allocation test");
    return 0;
}

int test_aligned_allocation(void) {
    printf("Testing aligned allocation...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, 16);
    TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 다양한 정렬 요구사항 테스트
    size_t alignments[] = {16, 32, 64, 128, 256};
    void* ptrs[5];

    for (int i = 0; i < 5; i++) {
        ptrs[i] = rt_alloc_aligned(allocator, 100, alignments[i]);
        TEST_ASSERT(ptrs[i] != NULL, "Failed to allocate aligned memory");

        // 정렬 확인
        uintptr_t addr = (uintptr_t)ptrs[i];
        TEST_ASSERT((addr & (alignments[i] - 1)) == 0, "Memory not properly aligned");

        // 메모리 사용 테스트
        memset(ptrs[i], i + 1, 100);
    }

    // 메모리 해제
    for (int i = 0; i < 5; i++) {
        rt_free(allocator, ptrs[i]);
    }

    rt_destroy_allocator(allocator);

    TEST_PASS("Aligned allocation test");
    return 0;
}

int test_memory_tracking(void) {
    printf("Testing memory tracking...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    size_t initial_used = rt_get_used_size(allocator);
    size_t initial_peak = rt_get_peak_usage(allocator);

    // 메모리 할당 및 추적
    void* ptr1 = rt_alloc(allocator, 512);
    TEST_ASSERT(ptr1 != NULL, "Failed to allocate memory");

    size_t used_after_alloc = rt_get_used_size(allocator);
    size_t peak_after_alloc = rt_get_peak_usage(allocator);

    TEST_ASSERT(used_after_alloc > initial_used, "Used size should increase");
    TEST_ASSERT(peak_after_alloc >= used_after_alloc, "Peak should be at least used size");

    // 더 많은 메모리 할당
    void* ptr2 = rt_alloc(allocator, 1024);
    TEST_ASSERT(ptr2 != NULL, "Failed to allocate more memory");

    size_t used_after_second = rt_get_used_size(allocator);
    size_t peak_after_second = rt_get_peak_usage(allocator);

    TEST_ASSERT(used_after_second > used_after_alloc, "Used size should increase more");
    TEST_ASSERT(peak_after_second >= used_after_second, "Peak should track maximum");

    // 메모리 해제 후 추적
    rt_free(allocator, ptr1);
    size_t used_after_free = rt_get_used_size(allocator);
    size_t peak_after_free = rt_get_peak_usage(allocator);

    TEST_ASSERT(used_after_free < used_after_second, "Used size should decrease");
    TEST_ASSERT(peak_after_free == peak_after_second, "Peak should remain same");

    rt_free(allocator, ptr2);
    rt_destroy_allocator(allocator);

    TEST_PASS("Memory tracking test");
    return 0;
}

int test_leak_detection(void) {
    printf("Testing leak detection...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 누수 감지 활성화
    rt_enable_leak_detection(allocator, true);

    // 메모리 할당 (의도적으로 해제하지 않음)
    void* leaked_ptr1 = rt_alloc(allocator, 256);
    void* leaked_ptr2 = rt_alloc(allocator, 512);
    TEST_ASSERT(leaked_ptr1 != NULL && leaked_ptr2 != NULL, "Failed to allocate memory");

    // 정상적으로 해제되는 메모리
    void* normal_ptr = rt_alloc(allocator, 128);
    TEST_ASSERT(normal_ptr != NULL, "Failed to allocate normal memory");
    rt_free(allocator, normal_ptr);

    // 잠시 대기 (타임스탬프 차이를 위해)
    usleep(100000); // 100ms

    // 누수 검사 (50ms 임계값)
    size_t leak_count = rt_check_memory_leaks(allocator, 50);
    TEST_ASSERT(leak_count == 2, "Should detect 2 leaked blocks");

    // 누수 정보 조회
    ETMemoryLeakInfo leak_infos[10];
    size_t actual_leaks = rt_get_memory_leaks(allocator, leak_infos, 10);
    TEST_ASSERT(actual_leaks == 2, "Should return 2 leak infos");

    // 누수 리포트 출력 (파일로)
    rt_print_memory_leak_report(allocator, "test_leak_report.txt");

    // 통계에서 누수 정보 확인
    ETMemoryPoolStats stats;
    rt_get_allocator_stats(allocator, &stats);
    TEST_ASSERT(stats.num_active_blocks >= 2, "Should have active blocks");

    // 누수된 메모리 해제
    rt_free(allocator, leaked_ptr1);
    rt_free(allocator, leaked_ptr2);

    // 누수 재검사
    leak_count = rt_check_memory_leaks(allocator, 50);
    TEST_ASSERT(leak_count == 0, "Should detect no leaks after cleanup");

    rt_destroy_allocator(allocator);

    TEST_PASS("Leak detection test");
    return 0;
}

int test_allocator_stats(void) {
    printf("Testing allocator statistics...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 초기 통계 확인
    ETMemoryPoolStats stats;
    rt_get_allocator_stats(allocator, &stats);

    TEST_ASSERT(stats.total_size == TEST_POOL_SIZE, "Incorrect total size in stats");
    TEST_ASSERT(stats.used_size == 0, "Initial used size should be 0");
    TEST_ASSERT(stats.num_allocations == 0, "Initial allocation count should be 0");
    TEST_ASSERT(stats.num_frees == 0, "Initial free count should be 0");

    // 메모리 할당 후 통계
    void* ptr1 = rt_alloc(allocator, 512);
    void* ptr2 = rt_alloc(allocator, 256);

    rt_get_allocator_stats(allocator, &stats);
    TEST_ASSERT(stats.used_size > 0, "Used size should increase");
    TEST_ASSERT(stats.num_allocations == 2, "Should have 2 allocations");
    TEST_ASSERT(stats.free_size < TEST_POOL_SIZE, "Free size should decrease");

    // 메모리 해제 후 통계
    rt_free(allocator, ptr1);
    rt_get_allocator_stats(allocator, &stats);
    TEST_ASSERT(stats.num_frees == 1, "Should have 1 free");

    rt_free(allocator, ptr2);
    rt_get_allocator_stats(allocator, &stats);
    TEST_ASSERT(stats.num_frees == 2, "Should have 2 frees");

    // 할당자 정보 출력 테스트
    printf("=== Allocator Info ===\n");
    rt_print_allocator_info(allocator);

    rt_destroy_allocator(allocator);

    TEST_PASS("Allocator statistics test");
    return 0;
}

int test_allocator_reset(void) {
    printf("Testing allocator reset...\n");

    RTAllocator* allocator = rt_create_allocator(TEST_POOL_SIZE, TEST_ALIGNMENT);
    TEST_ASSERT(allocator != NULL, "Failed to create allocator");

    // 메모리 할당
    void* ptr1 = rt_alloc(allocator, 512);
    void* ptr2 = rt_alloc(allocator, 256);
    void* ptr3 = rt_alloc(allocator, 128);

    TEST_ASSERT(ptr1 && ptr2 && ptr3, "Failed to allocate memory");
    TEST_ASSERT(rt_get_used_size(allocator) > 0, "Used size should be greater than 0");

    // 할당자 리셋
    rt_reset_allocator(allocator);

    // 리셋 후 상태 확인
    TEST_ASSERT(rt_get_used_size(allocator) == 0, "Used size should be 0 after reset");
    TEST_ASSERT(rt_get_free_size(allocator) == TEST_POOL_SIZE, "Free size should be total size");

    // 리셋 후 새로운 할당 가능한지 확인
    void* new_ptr = rt_alloc(allocator, 1024);
    TEST_ASSERT(new_ptr != NULL, "Should be able to allocate after reset");

    rt_free(allocator, new_ptr);
    rt_destroy_allocator(allocator);

    TEST_PASS("Allocator reset test");
    return 0;
}

int main(void) {
    printf("=== Memory Allocator Simple Tests ===\n\n");

    int failed_tests = 0;

    failed_tests += test_allocator_creation();
    failed_tests += test_basic_allocation();
    failed_tests += test_aligned_allocation();
    failed_tests += test_memory_tracking();
    failed_tests += test_leak_detection();
    failed_tests += test_allocator_stats();
    failed_tests += test_allocator_reset();

    printf("\n=== Test Results ===\n");
    if (failed_tests == 0) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ %d test(s) failed!\n", failed_tests);
        return 1;
    }
}